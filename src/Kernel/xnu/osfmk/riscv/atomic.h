/*
 * Copyright (c) 2015-2018 Apple Inc. All rights reserved.
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
#ifndef _MACHINE_ATOMIC_H
#error "Do not include <riscv/atomic.h> directly, use <machine/atomic.h>"
#endif

#ifndef _RISCV_ATOMIC_H_
#define _RISCV_ATOMIC_H_

#include <mach/boolean.h>

// fence operands, predecessor and successor sets of i/o/r/w
#define RISCV_FENCE(pred, succ) __asm__ volatile ("fence " #pred ", " #succ ::: "memory")

#define riscv_fence_rw_rw()     RISCV_FENCE(rw, rw)
#define riscv_fence_r_rw()      RISCV_FENCE(r, rw)
#define riscv_fence_rw_w()      RISCV_FENCE(rw, w)
#define riscv_fence_iorw()      RISCV_FENCE(iorw, iorw)
#define riscv_fence_i()         __asm__ volatile ("fence.i" ::: "memory")

#endif // _RISCV_ATOMIC_H_
