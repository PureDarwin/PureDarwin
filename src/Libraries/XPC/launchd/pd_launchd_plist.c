#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "launch.h"
#include "launch_priv.h"
#include "core.h"

static launch_data_t pd_cf_to_launch_data(CFTypeRef value);

struct pd_cf_dict_ctx {
	launch_data_t ldict;
};

static void
pd_cf_dict_apply(const void *key, const void *value, void *ctxp)
{
	struct pd_cf_dict_ctx *ctx = ctxp;
	char keybuf[256];

	if (CFGetTypeID(key) != CFStringGetTypeID()) {
		return;
	}
	if (!CFStringGetCString((CFStringRef)key, keybuf, sizeof(keybuf),
			kCFStringEncodingUTF8)) {
		return;
	}

	launch_data_t lval = pd_cf_to_launch_data((CFTypeRef)value);
	if (lval == NULL) {
		return;
	}
	launch_data_dict_insert(ctx->ldict, lval, keybuf);
}

static launch_data_t
pd_cf_to_launch_data(CFTypeRef value)
{
	CFTypeID type = CFGetTypeID(value);

	if (type == CFDictionaryGetTypeID()) {
		launch_data_t ldict = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
		struct pd_cf_dict_ctx ctx = { ldict };
		CFDictionaryApplyFunction((CFDictionaryRef)value, pd_cf_dict_apply, &ctx);
		return ldict;
	}

	if (type == CFArrayGetTypeID()) {
		CFArrayRef arr = (CFArrayRef)value;
		CFIndex count = CFArrayGetCount(arr);
		launch_data_t larray = launch_data_alloc(LAUNCH_DATA_ARRAY);
		for (CFIndex i = 0; i < count; i++) {
			launch_data_t lval = pd_cf_to_launch_data(CFArrayGetValueAtIndex(arr, i));
			if (lval != NULL) {
				launch_data_array_set_index(larray, lval, (size_t)i);
			}
		}
		return larray;
	}

	if (type == CFStringGetTypeID()) {
		char buf[1024];
		if (!CFStringGetCString((CFStringRef)value, buf, sizeof(buf),
				kCFStringEncodingUTF8)) {
			buf[0] = '\0';
		}
		return launch_data_new_string(buf);
	}

	if (type == CFBooleanGetTypeID()) {
		return launch_data_new_bool(CFBooleanGetValue((CFBooleanRef)value));
	}

	if (type == CFNumberGetTypeID()) {
		long long n = 0;
		CFNumberGetValue((CFNumberRef)value, kCFNumberLongLongType, &n);
		return launch_data_new_integer(n);
	}

	/* Unsupported plist value type (CFData/CFDate/...): no launch_data_t
	 * equivalent in this subset, drop the key rather than fabricate one. */
	return NULL;
}

static bool
pd_launchd_import_plist(const char *path)
{
	int fd;
	struct stat st;
	void *map = MAP_FAILED;
	bool ok = false;

	fd = open(path, O_RDONLY | O_NOCTTY);
	if (fd < 0) {
		return false;
	}
	if (fstat(fd, &st) < 0 || st.st_size == 0) {
		close(fd);
		return false;
	}

	map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED) {
		fprintf(stderr, "pd_launchd_boot: mmap(%s) failed\n", path);
		return false;
	}

	CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)map,
			st.st_size, kCFAllocatorNull);
	if (data != NULL) {
		CFPropertyListRef plist = CFPropertyListCreateFromXMLData(NULL, data,
				kCFPropertyListImmutable, NULL);
		if (plist != NULL) {
			if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
				launch_data_t job = pd_cf_to_launch_data(plist);
				if (job != NULL) {
					if (job_import(job) == NULL) {
						fprintf(stderr,
							"pd_launchd_boot: job_import(%s) failed\n", path);
					} else {
						ok = true;
					}
					launch_data_free(job);
				}
			} else {
				fprintf(stderr,
					"pd_launchd_boot: %s is not a dictionary plist\n", path);
			}
			CFRelease(plist);
		} else {
			fprintf(stderr, "pd_launchd_boot: failed to parse %s\n", path);
		}
		CFRelease(data);
	}

	munmap(map, (size_t)st.st_size);
	return ok;
}

void
pd_launchd_load_daemons_dir(const char *dir)
{
	DIR *dp = opendir(dir);
	if (dp == NULL) {
		return;
	}

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		size_t namelen = strlen(de->d_name);
		if (namelen < 7 || strcmp(de->d_name + namelen - 6, ".plist") != 0) {
			continue;
		}

		char path[1024];
		if (snprintf(path, sizeof(path), "%s/%s", dir, de->d_name) >= (int)sizeof(path)) {
			continue;
		}

		pd_launchd_import_plist(path);
	}

	closedir(dp);
}
