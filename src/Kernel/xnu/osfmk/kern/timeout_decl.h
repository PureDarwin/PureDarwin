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

#ifndef _KERN_TIMEOUT_DECL_H_
#define _KERN_TIMEOUT_DECL_H_

#include <kern/kern_types.h>

/*
 * The interrupt disabled timeouts mechanism requires that we include this
 * header in arm/thread.h, which is why this is here and not in the timeout.h
 * header.
 */

#define TO_BT_FRAMES 3

typedef struct kern_timeout {
	uint64_t        start_mt;
	uint64_t        end_mt;
	uint64_t        int_mt;
	uint64_t        start_cycles;
	uint64_t        int_cycles;
	uint64_t        start_instrs;
	uint64_t        int_instrs;
	uintptr_t       bt[TO_BT_FRAMES];
} kern_timeout_t;

#endif /* _KERN_TIMEOUT_DECL_H_ */
