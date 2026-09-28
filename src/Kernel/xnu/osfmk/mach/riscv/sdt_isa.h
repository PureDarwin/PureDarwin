/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifndef _MACH_RISCV_SDT_ISA_H
#define _MACH_RISCV_SDT_ISA_H

#include <stdint.h>

#if defined(__riscv)

// Only define when testing, this makes the calls into actual calls to test functions.
/* #define DTRACE_CALL_TEST */

#define DTRACE_STRINGIFY(s) #s
#define DTRACE_TOSTRING(s) DTRACE_STRINGIFY(s)


#if defined(KERNEL)

#if (__SIZEOF_POINTER__ == 8)

#define DTRACE_LABEL(p, n)                                                          \
	".pushsection __DATA_CONST, __sdt_cstring, cstring_literals\n\t"                \
	"1: .ascii \"" DTRACE_STRINGIFY(p##___) "\\0\"\n\t"                             \
	"2: .ascii \"" DTRACE_STRINGIFY(n) "\\0\"\n\t"                                  \
	".popsection" "\n\t"                                                            \
	".pushsection __DATA_CONST, __sdt, regular, live_support\n\t"                   \
	".p2align 3\n\t"                                                                \
	"l3_%=:\n\t"                                                                    \
	".quad 4f""\n\t"                                                                \
	".quad 1b""\n\t"                                                                \
	".quad 2b""\n\t"                                                                \
	".popsection" "\n\t"                                                            \
	"4:"

#else

#define DTRACE_LABEL(p, n)

#endif /* __SIZEOF_POINTER__ == 8 */


#else   /* !KERNEL */

#define DTRACE_LABEL(p, n)                                                                      \
	"__dtrace_probe$" DTRACE_TOSTRING(%=__LINE__) DTRACE_STRINGIFY(_##p##___##n) ":"	"\n\t"

#endif  /* !KERNEL */


// Testing mode that builds function call to the probe site instead of nops.
#ifdef DTRACE_CALL_TEST

#define DTRACE_CALL_INSN(p, n)                                           \
	"call _dtracetest" DTRACE_STRINGIFY(_##p##_##n)	"\n\t"

#define DTRACE_CALL(p, n)        \
	DTRACE_LABEL(p,n)            \
	DTRACE_CALL_INSN(p,n)

#else   /* DTRACE_CALL_TEST */

#define DTRACE_NOPS                                                     \
	"nop"                   "\n\t"

#define DTRACE_CALL(p, n)        \
	DTRACE_LABEL(p,n)            \
	DTRACE_NOPS

#endif  /* DTRACE_CALL_TEST */


#define DTRACE_PROBE(provider, name)                                                        \
	do {                                                                                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                :                                                                       \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE1(provider, name, arg0)                                                 \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0)                                                     \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE2(provider, name, arg0, arg1)                                           \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1)                                  \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE3(provider, name, arg0, arg1, arg2)                                     \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2)               \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE4(provider, name, arg0, arg1, arg2, arg3)                               \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        register uintptr_t __dtrace_a3 asm("a3") = (uintptr_t) arg3;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2),              \
	                  "r" (__dtrace_a3)                                                     \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE5(provider, name, arg0, arg1, arg2, arg3, arg4)                         \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        register uintptr_t __dtrace_a3 asm("a3") = (uintptr_t) arg3;                    \
	        register uintptr_t __dtrace_a4 asm("a4") = (uintptr_t) arg4;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2),              \
	                  "r" (__dtrace_a3), "r" (__dtrace_a4)                                  \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE6(provider, name, arg0, arg1, arg2, arg3, arg4, arg5)                   \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        register uintptr_t __dtrace_a3 asm("a3") = (uintptr_t) arg3;                    \
	        register uintptr_t __dtrace_a4 asm("a4") = (uintptr_t) arg4;                    \
	        register uintptr_t __dtrace_a5 asm("a5") = (uintptr_t) arg5;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2),              \
	                  "r" (__dtrace_a3), "r" (__dtrace_a4), "r" (__dtrace_a5)               \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE7(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6)             \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        register uintptr_t __dtrace_a3 asm("a3") = (uintptr_t) arg3;                    \
	        register uintptr_t __dtrace_a4 asm("a4") = (uintptr_t) arg4;                    \
	        register uintptr_t __dtrace_a5 asm("a5") = (uintptr_t) arg5;                    \
	        register uintptr_t __dtrace_a6 asm("a6") = (uintptr_t) arg6;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2),              \
	                  "r" (__dtrace_a3), "r" (__dtrace_a4), "r" (__dtrace_a5),              \
	                  "r" (__dtrace_a6)                                                     \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#define DTRACE_PROBE8(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7)       \
	do {                                                                                    \
	        register uintptr_t __dtrace_a0 asm("a0") = (uintptr_t) arg0;                    \
	        register uintptr_t __dtrace_a1 asm("a1") = (uintptr_t) arg1;                    \
	        register uintptr_t __dtrace_a2 asm("a2") = (uintptr_t) arg2;                    \
	        register uintptr_t __dtrace_a3 asm("a3") = (uintptr_t) arg3;                    \
	        register uintptr_t __dtrace_a4 asm("a4") = (uintptr_t) arg4;                    \
	        register uintptr_t __dtrace_a5 asm("a5") = (uintptr_t) arg5;                    \
	        register uintptr_t __dtrace_a6 asm("a6") = (uintptr_t) arg6;                    \
	        register uintptr_t __dtrace_a7 asm("a7") = (uintptr_t) arg7;                    \
	        asm volatile (                                                                  \
	                DTRACE_CALL(provider, name)                                             \
	                :                                                                       \
	                : "r" (__dtrace_a0), "r" (__dtrace_a1), "r" (__dtrace_a2),              \
	                  "r" (__dtrace_a3), "r" (__dtrace_a4), "r" (__dtrace_a5),              \
	                  "r" (__dtrace_a6), "r" (__dtrace_a7)                                  \
	                : "memory"                                                              \
	        );                                                                              \
	} while(0)

#endif /* defined (__riscv) */

#endif  /* _MACH_RISCV_SDT_ISA_H */
