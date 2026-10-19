#ifndef _FENV_H
#define _FENV_H

#if defined(__i386__)
#  include <arch/x86/fenv.h>
#elif defined(__x86_64__)
#  include <arch/x86_64/fenv.h>
#elif defined(__arm__)
#  include <arch/arm/fenv.h>
#elif defined(__aarch64__)
#  include <arch/arm64/fenv.h>
#elif defined(__POWERPC__)
#  include <arch/ppc/fenv.h>
#elif (defined(__riscv) && __riscv_xlen == 64)
#  include <arch/riscv64/fenv.h>
#elif defined(__sparc__)
#  include <arch/sparc64/fenv.h>
#elif defined(__M68K__)
#  include <arch/m68k/fenv.h>
#else
#  error There is no fenv.h for this architecture!
#endif

#endif /* _FENV_H */

