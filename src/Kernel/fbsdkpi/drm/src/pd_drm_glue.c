// SPDX-License-Identifier: MIT
// drm glue, the few kernel globals and devfs entry points drm-kmod expects
#include <linux/acpi.h>
#include <linux/errno.h>
#include "../../pd_fbsd_os.h"
#include <linux/string.h>
#include <sys/param.h>
#include <sys/bus.h>

int M_TEMP[1];
struct proc *pageproc;
struct pd_acpi_fadt AcpiGbl_FADT;

int
make_dev_alias_p(int flags, struct cdev **cdev, struct cdev *old,
    const char *fmt, ...)
{
	*cdev = NULL;
	return ENODEV;
}

int
device_print_prettyname(struct _device *dev)
{
	printf("%s%d: ", device_get_name(dev), device_get_unit(dev));
	return 0;
}

// no devd on PureDarwin, hotplug and lease events have no listener
void
devctl_notify(const char *system, const char *subsystem, const char *type,
    const char *data)
{
}

char *
strnstr(const char *s, const char *find, size_t slen)
{
	size_t len = strlen(find);

	if (len == 0)
		return (char *)s;
	for (; slen >= len && *s != '\0'; s++, slen--) {
		if (strncmp(s, find, len) == 0)
			return (char *)s;
	}
	return NULL;
}

// amdgpu asks for a power-off on a thermal emergency, which needs launchd
void
shutdown_nice(int howto)
{
	printf("drm: shutdown requested (howto 0x%x), not supported\n", howto);
}

long
pd_drm_tunable_long(const char *path, long def)
{
	char name[64];
	size_t i;

	for (i = 0; path[i] != '\0' && i < sizeof(name) - 1; i++)
		name[i] = path[i] == '.' ? '_' : path[i];
	name[i] = '\0';
	return pdos_bootarg_int(name, (int)def);
}
