// riscv capability header behind System/machine/cpu_capabilities.h
// the _COMM_PAGE_* addresses userspace reads sit under PRIVATE like on arm and i386
#ifndef PRIVATE
#define PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES 1
#define PRIVATE 1
#endif

#include <riscv/cpu_capabilities.h>

#ifdef PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES
#undef PRIVATE
#undef PUREDARWIN_DEFINED_PRIVATE_FOR_CPU_CAPABILITIES
#endif
