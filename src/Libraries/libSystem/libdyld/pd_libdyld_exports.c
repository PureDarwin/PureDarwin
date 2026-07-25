extern void start(void);
void *pd_libdyld_getStartGlueToCallExit(void) { return (void *)&start; }

extern void tlv_initializer(void);
void pd_libdyld_tlv_initializer(void) { tlv_initializer(); }

#include <stdint.h>
#include <stdbool.h>
bool
__isPlatformVersionAtLeast(uint32_t platform, uint32_t major, uint32_t minor,
		uint32_t subminor)
{
	(void)platform;
	(void)major;
	(void)minor;
	(void)subminor;
	return true;
}
