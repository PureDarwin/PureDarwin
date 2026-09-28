/* PureDarwin compat redirect (see System/i386/cpu_capabilities.h). */
#if defined(__i386__) || defined(__x86_64__)
#include <System/i386/cpu_capabilities.h>
#elif defined(__arm__) || defined(__arm64__)
#include <System/arm/cpu_capabilities.h>
#elif defined(__riscv)
#include <System/riscv/cpu_capabilities.h>
#else
#error architecture not supported
#endif
