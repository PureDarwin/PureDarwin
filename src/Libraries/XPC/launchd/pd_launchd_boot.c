#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define SYSTEM_VERSION_PLIST "/System/Library/CoreServices/SystemVersion.plist"

/*
 * Minimal <key>NAME</key> ... <string>VALUE</string> scan. This runs before
 * anything has brought CoreFoundation up, and the file is our own flat plist.
 */
static char *
pd_launchd_plist_string(const char *contents, const char *name)
{
	char key[128];
	const char *p, *start, *end;
	char *value;

	if ((size_t)snprintf(key, sizeof(key), "<key>%s</key>", name) >= sizeof(key)) {
		return NULL;
	}
	if ((p = strstr(contents, key)) == NULL) {
		return NULL;
	}
	if ((start = strstr(p, "<string>")) == NULL) {
		return NULL;
	}
	start += strlen("<string>");
	if ((end = strstr(start, "</string>")) == NULL) {
		return NULL;
	}
	if ((value = malloc((size_t)(end - start) + 1)) == NULL) {
		return NULL;
	}
	memcpy(value, start, (size_t)(end - start));
	value[end - start] = '\0';
	return value;
}

/*
 * kern.osproductversion is write-once and only from pid 1, so if launchd does
 * not publish it nothing can: it reads back empty and every consumer -
 * including libdyld's _availability_version_check - falls back to a guess.
 */
static void
pd_launchd_publish_system_version(void)
{
	FILE *f = fopen(SYSTEM_VERSION_PLIST, "r");
	char *contents = NULL, *version;
	long size;

	if (f == NULL) {
		return;
	}
	if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 &&
	    fseek(f, 0, SEEK_SET) == 0 && (contents = malloc((size_t)size + 1)) != NULL) {
		contents[fread(contents, 1, (size_t)size, f)] = '\0';
	}
	fclose(f);
	if (contents == NULL) {
		return;
	}

	if ((version = pd_launchd_plist_string(contents, "ProductVersion")) != NULL) {
		if (sysctlbyname("kern.osproductversion", NULL, NULL, version,
		    strlen(version) + 1) < 0) {
			perror("sysctl kern.osproductversion");
		}
		free(version);
	}

	if (sysctlbyname("kern.osreleasetype", NULL, NULL, "User", sizeof("User")) < 0) {
		perror("sysctl kern.osreleasetype");
	}

	free(contents);
}

void
pd_launchd_boot(void)
{
	pd_launchd_boot_mkdir_p("/dev", 0755);
	pd_launchd_boot_try_mount("devfs", "/dev", 0, NULL);
	pd_launchd_publish_system_version();
}
