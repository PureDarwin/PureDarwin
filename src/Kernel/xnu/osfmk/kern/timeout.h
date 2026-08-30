/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef _KERN_TIMEOUT_H_
#define _KERN_TIMEOUT_H_

#include <kern/kern_types.h>
#include <kern/timeout_decl.h>

__options_closed_decl(timeout_flags_t, uint32_t, {
	TF_NONSPEC_TIMEBASE       = 0x01,
	TF_BACKTRACE              = 0x02,
#if XNU_KERNEL_PRIVATE
	TF_SAMPLE_INTERRUPT_TIME  = 0x04,
	TF_SAMPLE_PMC             = 0x08,
#endif /* XNU_KERNEL_PRIVATE */
});

__enum_decl(kern_timeout_type_t, uint32_t, {
	KERN_TIMEOUT_PREEMPTION   = 1,
	KERN_TIMEOUT_INTERRUPT    = 2,
	KERN_TIMEOUT_MMIO         = 3,
	KERN_TIMEOUT_LOCK         = 4,
});

extern void kern_timeout_start(kern_timeout_t *to, timeout_flags_t flags);
extern void kern_timeout_restart(kern_timeout_t *to, timeout_flags_t flags);
extern void kern_timeout_end(kern_timeout_t *to, timeout_flags_t flags);
extern void kern_timeout_override(kern_timeout_t *to);
extern void kern_timeout_try_panic(kern_timeout_type_t type, uint64_t payload, kern_timeout_t *to,
    const char *prefix, uint64_t threshold);

#if XNU_KERNEL_PRIVATE
extern void kern_timeout_cycles_instrs(kern_timeout_t *to, uint64_t *cycles, uint64_t *instrs);
extern void kern_timeout_cpi(kern_timeout_t *to, uint64_t *cpi_whole, uint64_t *cpi_fractional);
#endif /* XNU_KERNEL_PRIVATE */

static inline void
kern_timeout_stretch(kern_timeout_t *to, uint64_t mt_ticks)
{
	to->start_mt -= mt_ticks;
}

static inline uint64_t
kern_timeout_start_time(kern_timeout_t *to)
{
	return to->start_mt;
}

/*
 * Return the mach time elapsed beteween calls to kern_timeout_start() and kern_timeout_end().
 */
static inline uint64_t
kern_timeout_gross_duration(kern_timeout_t *to)
{
	if (__improbable(to->start_mt == 0 || to->end_mt < to->start_mt)) {
		return 0;
	}
	return to->end_mt - to->start_mt;
}

#if XNU_KERNEL_PRIVATE
/*
 * Return the mach time elapsed beteween calls to kern_timeout_start() and kern_timeout_end()
 * subtracting the mach time that elapsed handling interrupts.
 */
static inline uint64_t
kern_timeout_net_duration(kern_timeout_t *to)
{
	uint64_t gross_duration = kern_timeout_gross_duration(to);
	uint64_t int_duration = to->int_mt;

	if (__improbable(to->start_mt == 0 || gross_duration < int_duration)) {
		return 0;
	}
	return gross_duration - int_duration;
}
#endif /* XNU_KERNEL_PRIVATE */

static inline void
kern_timeout_mach_times(kern_timeout_t *to, uint64_t *start_mt, uint64_t *end_mt)
{
	*start_mt = to->start_mt;
	*end_mt = to->end_mt;
}

#endif /* _KERN_TIMEOUT_H_ */
