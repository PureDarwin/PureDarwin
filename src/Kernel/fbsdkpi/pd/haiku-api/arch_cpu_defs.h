// SPDX-License-Identifier: MIT
// haiku's arch cpu definitions, the pause hint only
// expressions so they also work in a for loop's step (cpu_relax)
#ifndef _PD_HAIKU_ARCH_CPU_DEFS_H
#define _PD_HAIKU_ARCH_CPU_DEFS_H
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_PAUSE()	__builtin_ia32_pause()
#elif defined(__aarch64__)
#define SPINLOCK_PAUSE()	({ __asm__ __volatile__("yield"); })
#else
#define SPINLOCK_PAUSE()	((void)0)
#endif
#endif
