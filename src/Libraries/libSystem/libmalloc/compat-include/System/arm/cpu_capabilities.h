/* PureDarwin ARM capability header path used by System/machine/cpu_capabilities.h. */

/* Most of arm/cpu_capabilities.h - including the _COMM_PAGE_* addresses that
 * userspace reads - sits inside an #ifdef PRIVATE block, exactly as on i386
 * (see the sibling System/i386/cpu_capabilities.h). */
#ifndef PRIVATE
#define PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES 1
#define PRIVATE 1
#endif

#include <arm/cpu_capabilities.h>

#ifdef PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES
#undef PRIVATE
#undef PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES
#endif
