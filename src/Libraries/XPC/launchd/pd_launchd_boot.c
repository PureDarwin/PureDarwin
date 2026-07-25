#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdio.h>

#include "launch.h"
#include "launch_priv.h"
#include "core.h"

static void
pd_launchd_boot_mkdir_p(const char *path, mode_t mode)
{
	if (mkdir(path, mode) < 0 && errno != EEXIST) {
		perror(path);
	}
}

static void
pd_launchd_boot_try_mount(const char *src, const char *target, int flags, void *data)
{
	pd_launchd_boot_mkdir_p(target, 0755);
	if (mount(src, target, flags, data) < 0 && errno != EBUSY) {
		fprintf(stderr, "mount %s on %s failed: ", src, target);
		perror("");
	}
}

void
pd_launchd_boot(void)
{
	pd_launchd_boot_mkdir_p("/dev", 0755);
	pd_launchd_boot_try_mount("devfs", "/dev", 0, NULL);
}
