// SPDX-License-Identifier: MIT
// /dev/dri for every drm kext, with system-wide minors

#include <mach/mach_types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>
#include <kern/locks.h>
#include <kern/clock.h>
#include <kern/thread_call.h>
#include <libkern/OSMalloc.h>
#include <IOKit/IOLib.h>

#include "pd_dri_cdev.h"

extern "C" int pd_cdev_mmap_register(int major,
	int (*fn)(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages,
		unsigned int *wimg, void (**release)(void *), void **release_arg));

// a clone minor is (open slot << 8) | drm minor, slot 0 marks the node itself
#define PD_DRI_SLOTS		1024
#define PD_DRI_SLOT(dev)	((minor(dev) >> 8) & 0xffff)
#define PD_DRI_DRM_MINOR(dev)	(minor(dev) & 0xff)

// a drm kext and the offset its minors get
#define PD_DRI_PROVIDERS	4

static int sMajor = -1;
static void *sSlots[PD_DRI_SLOTS];
static bool sSlotUsed[PD_DRI_SLOTS];
static lck_grp_t *sLockGroup;
static lck_mtx_t *sLock;
static const struct pd_dri_ops *sProviders[PD_DRI_PROVIDERS];
// for each system-wide minor, the kext that owns it and its own number for it
static struct {
	const struct pd_dri_ops *ops;
	int minor;
} sOwner[256];

static const struct pd_dri_ops *
owner_ops(dev_t dev)
{
	return sOwner[PD_DRI_DRM_MINOR(dev)].ops;
}

static void *
slot_cookie(dev_t dev)
{
	int slot = PD_DRI_SLOT(dev);
	return slot > 0 && slot < PD_DRI_SLOTS ? sSlots[slot] : NULL;
}

static int
dri_clone(dev_t dev, int action)
{
	int slot;

	if (action == DEVFS_CLONE_ALLOC) {
		lck_mtx_lock(sLock);
		for (slot = 1; slot < PD_DRI_SLOTS; slot++) {
			if (!sSlotUsed[slot]) {
				sSlotUsed[slot] = true;
				break;
			}
		}
		lck_mtx_unlock(sLock);
		if (slot == PD_DRI_SLOTS)
			return -1;
		return (slot << 8) | PD_DRI_DRM_MINOR(dev);
	}
	slot = PD_DRI_SLOT(dev);
	if (slot > 0 && slot < PD_DRI_SLOTS) {
		lck_mtx_lock(sLock);
		sSlotUsed[slot] = false;
		sSlots[slot] = NULL;
		lck_mtx_unlock(sLock);
	}
	return 0;
}

static int
dri_open(dev_t dev, int flags, int devtype, proc_t p)
{
	void *cookie = NULL;
	int slot = PD_DRI_SLOT(dev);
	int error;

	if (slot <= 0 || slot >= PD_DRI_SLOTS || owner_ops(dev) == NULL)
		return ENXIO;
	error = owner_ops(dev)->open(sOwner[PD_DRI_DRM_MINOR(dev)].minor, flags, &cookie);
	if (error != 0)
		return error;
	sSlots[slot] = cookie;
	return 0;
}

static int
dri_close(dev_t dev, int flags, int devtype, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie != NULL) {
		owner_ops(dev)->close(cookie);
		sSlots[PD_DRI_SLOT(dev)] = NULL;
	}
	return 0;
}

static int
dri_read(dev_t dev, struct uio *uio, int ioflag)
{
	void *cookie = slot_cookie(dev);
	char buffer[512];
	int error;

	if (cookie == NULL)
		return ENXIO;
	while (uio_resid(uio) > 0) {
		size_t want = uio_resid(uio) < (user_ssize_t)sizeof(buffer) ?
			(size_t)uio_resid(uio) : sizeof(buffer);
		ssize_t got = owner_ops(dev)->read(cookie, buffer, want, (ioflag & IO_NDELAY) != 0);
		if (got < 0)
			return (int)-got;
		if (got == 0)
			break;
		error = uiomove(buffer, (int)got, uio);
		if (error != 0 || (size_t)got < want)
			return error;
	}
	return 0;
}

static int
dri_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return ENXIO;
	return owner_ops(dev)->ioctl(cookie, (unsigned int)cmd, data, IOCPARM_LEN(cmd));
}

static int
dri_select(dev_t dev, int which, void *wql, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return 0;
	return owner_ops(dev)->poll(cookie, which, wql);
}

static int
dri_mmap_pages(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages,
	unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return ENXIO;
	return owner_ops(dev)->mmap_pages(cookie, offset, size, prot, pages, wimg, release, release_arg);
}

static struct cdevsw sDriCdevsw = {
	.d_open = dri_open,
	.d_close = dri_close,
	.d_read = dri_read,
	.d_write = eno_rdwrt,
	.d_ioctl = dri_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = dri_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

extern "C" int
pd_dri_register(const struct pd_dri_ops *ops)
{
	int k;

	if (sMajor < 0) {
		sLockGroup = lck_grp_alloc_init("pd dri", LCK_GRP_ATTR_NULL);
		sLock = lck_mtx_alloc_init(sLockGroup, LCK_ATTR_NULL);
		sMajor = cdevsw_add(-1, &sDriCdevsw);
		if (sMajor < 0)
			return ENXIO;
		pd_cdev_mmap_register(sMajor, dri_mmap_pages);
		IOLog("pd_dri: character major %d\n", sMajor);
	}
	lck_mtx_lock(sLock);
	for (k = 0; k < PD_DRI_PROVIDERS && sProviders[k] != NULL && sProviders[k] != ops; k++)
		;
	if (k < PD_DRI_PROVIDERS)
		sProviders[k] = ops;
	lck_mtx_unlock(sLock);
	return k < PD_DRI_PROVIDERS ? 0 : ENOSPC;
}

static int
provider_index(const struct pd_dri_ops *ops)
{
	for (int k = 0; k < PD_DRI_PROVIDERS; k++) {
		if (sProviders[k] == ops)
			return k;
	}
	return -1;
}

// what libdrm reads to find the pci device behind a minor: kern.pd_dri.major and
// kern.pd_dri.devices, one "minor busid vendor device subvendor subdevice revision" line each
struct pd_dri_info {
	bool used;
	char busid[80];
	char compatible[64];	// platform devices only
	uint16_t vendor, device, subvendor, subdevice;
	uint8_t revision;
};
static struct pd_dri_info sInfo[256];

static int
dri_sysctl_devices(__unused struct sysctl_oid *oidp, __unused void *arg1,
	__unused int arg2, struct sysctl_req *req)
{
	char *text = (char *)IOMallocZero(256 * 64);
	size_t len = 0;
	int error;

	if (text == NULL)
		return ENOMEM;
	for (int i = 0; i < 256; i++) {
		if (!sInfo[i].used)
			continue;
		// a platform line has two fields, so libdrm's pci parser passes it by
		if (strncmp(sInfo[i].busid, "platform:", 9) == 0)
			len += snprintf(text + len, 256 * 64 - len, "%d %s %s\n", i, sInfo[i].busid,
			    sInfo[i].compatible);
		else
			len += snprintf(text + len, 256 * 64 - len, "%d %s 0x%04x 0x%04x 0x%04x 0x%04x 0x%02x\n",
			    i, sInfo[i].busid, sInfo[i].vendor, sInfo[i].device, sInfo[i].subvendor,
			    sInfo[i].subdevice, sInfo[i].revision);
	}
	error = SYSCTL_OUT(req, text, len + 1);
	IOFree(text, 256 * 64);
	return error;
}

SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, pd_dri, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "drm devices");
SYSCTL_INT(_kern_pd_dri, OID_AUTO, major, CTLFLAG_RD | CTLFLAG_LOCKED, &sMajor, 0, "dri character major");
SYSCTL_PROC(_kern_pd_dri, OID_AUTO, devices, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, dri_sysctl_devices, "A", "drm minors and their pci devices");

static bool sSysctls;

static void
dri_register_sysctls(void)
{
	if (sSysctls)
		return;
	sysctl_register_oid(&sysctl__kern_pd_dri);
	sysctl_register_oid(&sysctl__kern_pd_dri_major);
	sysctl_register_oid(&sysctl__kern_pd_dri_devices);
	sSysctls = true;
}

// a node per drm minor: the kext's card0 or renderD128 gets the system-wide
// number, dri/card1 or dri/renderD129 for the second kext
static int dri_make_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_pci *pci, const struct pd_dri_platform *plat);

extern "C" int
pd_dri_make_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_pci *pci)
{
	return dri_make_node(ops, drm_minor, name, pci, NULL);
}

extern "C" int
pd_dri_make_platform_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_platform *plat)
{
	return dri_make_node(ops, drm_minor, name, NULL, plat);
}

// node names not yet in devfs, by system-wide minor
static char sNode[256][32];

extern "C" int devfs_is_ready(void);
static int dri_publish(int global);
static void dri_schedule_publish(void);

static int
dri_make_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_pci *pci, const struct pd_dri_platform *plat)
{
	int k = provider_index(ops), global;
	char node[32];

	if (sMajor < 0 || k < 0 || drm_minor < 0 || drm_minor >= 256)
		return ENXIO;
	global = drm_minor + k;
	if (global >= 256 || sOwner[global].ops != NULL)
		return EEXIST;
	sOwner[global].ops = ops;
	sOwner[global].minor = drm_minor;
	if (drm_minor >= 128)
		snprintf(node, sizeof(node), "renderD%d", global);
	else if (drm_minor < 64)
		snprintf(node, sizeof(node), "card%d", global);
	else
		strlcpy(node, name, sizeof(node));

	if (pci != NULL) {
		struct pd_dri_info *info = &sInfo[global];

		snprintf(info->busid, sizeof(info->busid), "pci:%04x:%02x:%02x.%u", pci->domain,
		    pci->bus, pci->dev, pci->func);
		info->vendor = pci->vendor;
		info->device = pci->device;
		info->subvendor = pci->subvendor;
		info->subdevice = pci->subdevice;
		info->revision = pci->revision;
		info->used = true;
		dri_register_sysctls();
		IOLog("pd_dri: minor %d is %s, %04x:%04x\n", global, info->busid, info->vendor,
		    info->device);
	}
	if (plat != NULL) {
		struct pd_dri_info *info = &sInfo[global];

		snprintf(info->busid, sizeof(info->busid), "platform:%s", plat->fullname);
		strlcpy(info->compatible, plat->compatible[0] ? plat->compatible : "-",
		    sizeof(info->compatible));
		info->used = true;
		dri_register_sysctls();
		IOLog("pd_dri: minor %d is %s (%s)\n", global, info->busid, info->compatible);
	}
	strlcpy(sNode[global], node, sizeof(sNode[global]));
	// a drm kext that attaches before devfs is up gets its node once devfs is
	if (!devfs_is_ready()) {
		dri_schedule_publish();
		return 0;
	}
	return dri_publish(global);
}

static int
dri_publish(int global)
{
	if (devfs_make_node_clone(makedev(sMajor, global & 0xff), DEVFS_CHAR, 0, 0, 0666,
	    dri_clone, "dri/%s", sNode[global]) == NULL)
		return ENOMEM;
	IOLog("pd_dri: /dev/dri/%s\n", sNode[global]);
	sNode[global][0] = 0;
	return 0;
}

static void
dri_publish_pending(thread_call_param_t, thread_call_param_t)
{
	if (!devfs_is_ready()) {
		dri_schedule_publish();
		return;
	}
	for (int i = 0; i < 256; i++)
		if (sNode[i][0] != 0)
			dri_publish(i);
}

static void
dri_schedule_publish(void)
{
	static thread_call_t call;
	uint64_t deadline;

	if (call == NULL)
		call = thread_call_allocate(dri_publish_pending, NULL);
	clock_interval_to_deadline(100, 1000 * 1000, &deadline);
	thread_call_enter_delayed(call, deadline);
}

// a library kext: the drm kexts call in, there is nothing to start
extern "C" kern_return_t pd_dri_start(kmod_info_t *ki, void *data);
extern "C" kern_return_t pd_dri_stop(kmod_info_t *ki, void *data);

extern "C" kern_return_t
pd_dri_start(kmod_info_t *ki, void *data)
{
	return KERN_SUCCESS;
}

extern "C" kern_return_t
pd_dri_stop(kmod_info_t *ki, void *data)
{
	return KERN_FAILURE;
}
