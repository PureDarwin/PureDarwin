/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
#ifndef _MACH_RISCV__STRUCTS_H_
#define _MACH_RISCV__STRUCTS_H_

#if defined (__riscv)

#include <sys/cdefs.h> /* __DARWIN_UNIX03 */
#include <machine/types.h> /* __uint32_t */

// flavor 1, x0 is always zero and only keeps register numbers lined up with the array
#if __DARWIN_UNIX03
#define _STRUCT_RISCV_THREAD_STATE64 struct __darwin_riscv_thread_state64
_STRUCT_RISCV_THREAD_STATE64
{
	__uint64_t    __x[32];  /* x0-x31, x1 is ra, x2 is sp, x8 is fp */
	__uint64_t    __pc;     /* program counter */
};
#else /* !__DARWIN_UNIX03 */
#define _STRUCT_RISCV_THREAD_STATE64 struct riscv_thread_state64
_STRUCT_RISCV_THREAD_STATE64
{
	__uint64_t    x[32];    /* x0-x31, x1 is ra, x2 is sp, x8 is fp */
	__uint64_t    pc;       /* program counter */
};
#endif /* __DARWIN_UNIX03 */

#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#if __DARWIN_UNIX03
#define __darwin_riscv_thread_state64_get_pc(ts)  ((ts).__pc)
#define __darwin_riscv_thread_state64_get_lr(ts)  ((ts).__x[1])
#define __darwin_riscv_thread_state64_get_sp(ts)  ((ts).__x[2])
#define __darwin_riscv_thread_state64_get_fp(ts)  ((ts).__x[8])
#define __darwin_riscv_thread_state64_set_pc(ts, v) ((void)((ts).__pc = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_lr(ts, v) ((void)((ts).__x[1] = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_sp(ts, v) ((void)((ts).__x[2] = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_fp(ts, v) ((void)((ts).__x[8] = (__uint64_t)(v)))
#else /* !__DARWIN_UNIX03 */
#define __darwin_riscv_thread_state64_get_pc(ts)  ((ts).pc)
#define __darwin_riscv_thread_state64_get_lr(ts)  ((ts).x[1])
#define __darwin_riscv_thread_state64_get_sp(ts)  ((ts).x[2])
#define __darwin_riscv_thread_state64_get_fp(ts)  ((ts).x[8])
#define __darwin_riscv_thread_state64_set_pc(ts, v) ((void)((ts).pc = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_lr(ts, v) ((void)((ts).x[1] = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_sp(ts, v) ((void)((ts).x[2] = (__uint64_t)(v)))
#define __darwin_riscv_thread_state64_set_fp(ts, v) ((void)((ts).x[8] = (__uint64_t)(v)))
#endif /* __DARWIN_UNIX03 */
#endif /* __DARWIN_C_LEVEL >= __DARWIN_C_FULL */

// flavor 2, the d extension registers
#if __DARWIN_UNIX03
#define _STRUCT_RISCV_FLOAT_STATE64 struct __darwin_riscv_float_state64
_STRUCT_RISCV_FLOAT_STATE64
{
	__uint64_t    __f[32];
	__uint32_t    __fcsr;
	__uint32_t    __pad;
};
#else /* !__DARWIN_UNIX03 */
#define _STRUCT_RISCV_FLOAT_STATE64 struct riscv_float_state64
_STRUCT_RISCV_FLOAT_STATE64
{
	__uint64_t    f[32];
	__uint32_t    fcsr;
	__uint32_t    pad;
};
#endif /* __DARWIN_UNIX03 */

// flavor 3, the supervisor trap csrs of the last exception
#if __DARWIN_UNIX03
#define _STRUCT_RISCV_EXCEPTION_STATE64 struct __darwin_riscv_exception_state64
_STRUCT_RISCV_EXCEPTION_STATE64
{
	__uint64_t    __scause;
	__uint64_t    __stval;
	__uint64_t    __sepc;
};
#else /* !__DARWIN_UNIX03 */
#define _STRUCT_RISCV_EXCEPTION_STATE64 struct riscv_exception_state64
_STRUCT_RISCV_EXCEPTION_STATE64
{
	__uint64_t    scause;
	__uint64_t    stval;
	__uint64_t    sepc;
};
#endif /* __DARWIN_UNIX03 */

#if __DARWIN_UNIX03
#define _STRUCT_RISCV_PAGEIN_STATE struct __riscv_pagein_state
_STRUCT_RISCV_PAGEIN_STATE
{
	int __pagein_error;
};
#else /* !__DARWIN_UNIX03 */
#define _STRUCT_RISCV_PAGEIN_STATE struct riscv_pagein_state
_STRUCT_RISCV_PAGEIN_STATE
{
	int pagein_error;
};
#endif /* __DARWIN_UNIX03 */

#endif /* defined (__riscv) */

#endif /* _MACH_RISCV__STRUCTS_H_ */
