extern void start(void);
void *pd_libdyld_getStartGlueToCallExit(void) { return (void *)&start; }

extern void tlv_initializer(void);
void pd_libdyld_tlv_initializer(void) { tlv_initializer(); }

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <mach-o/loader.h>

typedef uint32_t pd_dyld_platform_t;
typedef struct {
	pd_dyld_platform_t	platform;
	uint32_t		version;
} pd_dyld_build_version_t;

#define PD_PACK_VERSION(maj, min, pat) \
	(((maj) << 16) | (((min) & 0xff) << 8) | ((pat) & 0xff))

/*
 * Falls back to the version sw_vers reports. kern.osproductversion is only
 * populated if launchd set it, so an empty answer is expected rather than an
 * error - and this must not claim to be newer than the system really is.
 */
static uint32_t
pd_os_version(void)
{
	static uint32_t cached;
	char buf[64];
	size_t len = sizeof(buf);
	unsigned int maj = 11, min = 3, pat = 0;

	if (cached != 0) {
		return cached;
	}
	if (sysctlbyname("kern.osproductversion", buf, &len, NULL, 0) == 0 &&
	    len > 1 && buf[0] != '\0') {
		unsigned int a = 0, b = 0, c = 0;
		int n = sscanf(buf, "%u.%u.%u", &a, &b, &c);
		if (n >= 2) {
			maj = a;
			min = b;
			pat = (n >= 3) ? c : 0;
		}
	}
	cached = PD_PACK_VERSION(maj, min, pat);
	return cached;
}

bool
_availability_version_check(uint32_t count, pd_dyld_build_version_t versions[])
{
	uint32_t os = pd_os_version();

	if (versions == NULL) {
		return false;
	}
	for (uint32_t i = 0; i < count; i++) {
		/*
		 * Only the running platform's entry is meaningful. Version sets
		 * (platform 0xffffffff) would need dyld's version-set table to
		 * resolve; nothing in this tree emits them, so they are refused
		 * rather than guessed at.
		 */
		if (versions[i].platform == PLATFORM_MACOS) {
			return versions[i].version <= os;
		}
	}
	return false;
}
