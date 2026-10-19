// SPDX-License-Identifier: MIT
// experimental manual reclocking as kern.nouveau.<minor>.pstate, opt-in with nouveau_pstate=1

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <nvif/class.h>
#include <nvif/if0001.h>

#include "nouveau_drv.h"

#include "../../pd_fbsd_os.h"

// the kern.nouveau sysctl tree lives on the xnu side, in PDDRMnouveau's pstate_sysctl.cpp
void *pd_nouveau_sysctl_add(int minor, const char *name,
    int (*read)(void *, char *, size_t), int (*write)(void *, const char *), void *cookie);
void pd_nouveau_sysctl_remove(void *handle);

// a gpu's control object and its sysctl, one per drm device
struct pd_nouveau_pstate {
	struct nouveau_drm *drm;
	struct nvif_object ctrl;
	void *sysctl;
};

#define PD_PSTATE_MAX 8

static struct pd_nouveau_pstate *pd_pstates[PD_PSTATE_MAX];

static int
pd_pstate_append(char *buf, size_t size, size_t *len, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (*len >= size)
		return -ENOSPC;
	va_start(ap, fmt);
	n = vsnprintf(buf + *len, size - *len, fmt, ap);
	va_end(ap);
	*len += n;
	return *len < size ? 0 : -ENOSPC;
}

// the same text linux's debugfs pstate file shows: each state's clocks, then the current ones
static int
pd_pstate_read(void *cookie, char *buf, size_t size)
{
	struct pd_nouveau_pstate *ps = cookie;
	struct nvif_control_pstate_info_v0 info = {};
	size_t len = 0;
	int ret, i;

	ret = nvif_mthd(&ps->ctrl, NVIF_CONTROL_PSTATE_INFO, &info, sizeof(info));
	if (ret)
		return ret;

	for (i = 0; i < info.count + 1; i++) {
		const s32 state = i < info.count ? i : NVIF_CONTROL_PSTATE_ATTR_V0_STATE_CURRENT;
		struct nvif_control_pstate_attr_v0 attr = { .state = state, .index = 0 };

		ret = nvif_mthd(&ps->ctrl, NVIF_CONTROL_PSTATE_ATTR, &attr, sizeof(attr));
		if (ret)
			return ret;
		if (i < info.count)
			pd_pstate_append(buf, size, &len, "%02x:", attr.state);
		else
			pd_pstate_append(buf, size, &len, "%s:", info.pwrsrc == 0 ? "DC" :
			    info.pwrsrc == 1 ? "AC" : "--");

		attr.index = 0;
		do {
			attr.state = state;
			ret = nvif_mthd(&ps->ctrl, NVIF_CONTROL_PSTATE_ATTR, &attr, sizeof(attr));
			if (ret)
				return ret;
			pd_pstate_append(buf, size, &len, " %s %d", attr.name, attr.min);
			if (attr.min != attr.max)
				pd_pstate_append(buf, size, &len, "-%d", attr.max);
			pd_pstate_append(buf, size, &len, " %s", attr.unit);
		} while (attr.index);

		if (state >= 0) {
			if (info.ustate_ac == state)
				pd_pstate_append(buf, size, &len, " AC");
			if (info.ustate_dc == state)
				pd_pstate_append(buf, size, &len, " DC");
			if (info.pstate == state)
				pd_pstate_append(buf, size, &len, " *");
		} else {
			if (info.ustate_ac < -1)
				pd_pstate_append(buf, size, &len, " AC");
			if (info.ustate_dc < -1)
				pd_pstate_append(buf, size, &len, " DC");
		}
		if (pd_pstate_append(buf, size, &len, "\n") != 0)
			break;
	}
	return 0;
}

// a pstate id in hex, "auto" for the gpu's own choice or "off" to leave the clocks alone
static int
pd_pstate_write(void *cookie, const char *text)
{
	struct pd_nouveau_pstate *ps = cookie;
	struct nvif_control_pstate_user_v0 args = { .pwrsrc = -EINVAL };
	char buf[32], *cur = buf, *nl;
	long value;
	int ret;

	strscpy(buf, text, sizeof(buf));
	if ((nl = strchr(buf, '\n')) != NULL)
		*nl = '\0';
	if (!strncasecmp(cur, "dc:", 3)) {
		args.pwrsrc = 0;
		cur += 3;
	} else if (!strncasecmp(cur, "ac:", 3)) {
		args.pwrsrc = 1;
		cur += 3;
	}

	if (!strcasecmp(cur, "off") || !strcasecmp(cur, "none")) {
		args.ustate = NVIF_CONTROL_PSTATE_USER_V0_STATE_UNKNOWN;
	} else if (!strcasecmp(cur, "auto")) {
		args.ustate = NVIF_CONTROL_PSTATE_USER_V0_STATE_PERFMON;
	} else {
		ret = kstrtol(cur, 16, &value);
		if (ret)
			return ret;
		args.ustate = value;
	}
	printk("nouveau: experimental pstate request \"%s\" on minor %d\n", text,
	    ps->drm->dev->primary->index);
	return nvif_mthd(&ps->ctrl, NVIF_CONTROL_PSTATE_USER, &args, sizeof(args));
}

void
pd_nouveau_pstate_init(struct nouveau_drm *drm)
{
	struct pd_nouveau_pstate *ps;
	int minor = drm->dev->primary->index;

	if (!pdos_bootarg_int("nouveau_pstate", 0) || minor < 0 || minor >= PD_PSTATE_MAX)
		return;
	ps = kzalloc(sizeof(*ps), GFP_KERNEL);
	if (ps == NULL)
		return;
	ps->drm = drm;
	if (nvif_object_ctor(&drm->client.device.object, "pdPstate", 0, NVIF_CLASS_CONTROL,
	    NULL, 0, &ps->ctrl) != 0) {
		kfree(ps);
		return;
	}
	ps->sysctl = pd_nouveau_sysctl_add(minor, "pstate", pd_pstate_read, pd_pstate_write, ps);
	if (ps->sysctl == NULL) {
		nvif_object_dtor(&ps->ctrl);
		kfree(ps);
		return;
	}
	pd_pstates[minor] = ps;
	printk("nouveau: kern.nouveau.%d.pstate is up, experimental\n", minor);
}

void
pd_nouveau_pstate_fini(struct nouveau_drm *drm)
{
	int minor = drm->dev->primary->index;
	struct pd_nouveau_pstate *ps;

	if (minor < 0 || minor >= PD_PSTATE_MAX || (ps = pd_pstates[minor]) == NULL)
		return;
	pd_pstates[minor] = NULL;
	pd_nouveau_sysctl_remove(ps->sysctl);
	nvif_object_dtor(&ps->ctrl);
	kfree(ps);
}
