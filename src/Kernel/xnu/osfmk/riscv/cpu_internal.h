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
#ifndef _RISCV_CPU_INTERNAL_H_
#define _RISCV_CPU_INTERNAL_H_

#include <mach/kern_return.h>
#include <riscv/cpu_data_internal.h>

extern void                     cpu_bootstrap(
	void);

extern void                     cpu_init(
	void);

extern void                     cpu_timebase_init(boolean_t from_boot);

extern kern_return_t            cpu_signal(
	cpu_data_t              *target,
	cpu_signal_t            signal,
	void                    *p0,
	void                    *p1);

extern kern_return_t            cpu_signal_deferred(
	cpu_data_t              *target,
	cpu_signal_t            signal);

extern void                     cpu_signal_cancel(
	cpu_data_t              *target,
	cpu_signal_t            signal);

#if DEVELOPMENT || DEBUG
extern void                     cpu_signal_set_test_vector(
	void                    (*func)(void));
#endif /* DEVELOPMENT || DEBUG */

extern bool cpu_has_SIGPdebug_pending(void);

extern unsigned int real_ncpus;

#endif  /* _RISCV_CPU_INTERNAL_H_ */
