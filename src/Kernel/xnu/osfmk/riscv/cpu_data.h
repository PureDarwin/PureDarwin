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
#ifndef RISCV_CPU_DATA
#define RISCV_CPU_DATA

#ifdef  MACH_KERNEL_PRIVATE

#include <mach_assert.h>
#include <kern/assert.h>
#include <kern/kern_types.h>
#include <kern/processor.h>
#include <pexpert/pexpert.h>
#include <riscv/thread.h>
#include <riscv/proc_reg.h>

#include <mach/mach_types.h>
#include <machine/thread.h>

__ASSUME_PTR_ABI_SINGLE_BEGIN

// tp holds the current thread whenever the hart runs kernel code
static inline __attribute__((const)) thread_t
current_thread_fast(void)
{
	unsigned long result;
	__asm__ ("mv %0, tp" : "=r" (result));
	return __unsafe_forge_single(thread_t, result);
}

static inline thread_t
current_thread_volatile(void)
{
	unsigned long result;
	__asm__ volatile ("mv %0, tp" : "=r" (result));
	return __unsafe_forge_single(thread_t, result);
}

#define getCpuDatap()            current_thread()->machine.CpuDatap
#define current_cpu_datap()      getCpuDatap()

extern int                       get_preemption_level(void);
extern unsigned int              get_preemption_level_for_thread(thread_t);

#define mp_disable_preemption()  _disable_preemption()
#define mp_enable_preemption()   _enable_preemption()

__ASSUME_PTR_ABI_SINGLE_END

#endif  /* MACH_KERNEL_PRIVATE */

#endif  /* RISCV_CPU_DATA */
