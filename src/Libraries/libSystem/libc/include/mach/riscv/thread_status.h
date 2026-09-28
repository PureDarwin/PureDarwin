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
// FILE_ID: thread_status.h


#ifndef _RISCV_THREAD_STATUS_H_
#define _RISCV_THREAD_STATUS_H_

#if defined (__riscv)

#include <mach/machine/_structs.h>
#include <mach/machine/thread_state.h>
#include <mach/message.h>
#include <mach/vm_types.h>

// flavors, 1 is also what ld64 writes into LC_UNIXTHREAD
#define RISCV_THREAD_STATE64     1
#define RISCV_FLOAT_STATE64      2
#define RISCV_EXCEPTION_STATE64  3
#define RISCV_PAGEIN_STATE       4
#define THREAD_STATE_NONE        5


#define THREAD_STATE_FLAVORS     22     /* one more than the highest flavor */

#define FLAVOR_MODIFIES_CORE_CPU_REGISTERS(x) \
	((x) == RISCV_THREAD_STATE64)

#define VALID_THREAD_STATE_FLAVOR(x) \
	(((x) == RISCV_THREAD_STATE64) ||      \
	 ((x) == RISCV_FLOAT_STATE64) ||       \
	 ((x) == RISCV_EXCEPTION_STATE64) ||   \
	 ((x) == RISCV_PAGEIN_STATE) ||        \
	 ((x) == THREAD_STATE_NONE))

typedef _STRUCT_RISCV_THREAD_STATE64    riscv_thread_state64_t;
typedef _STRUCT_RISCV_FLOAT_STATE64     riscv_float_state64_t;
typedef _STRUCT_RISCV_EXCEPTION_STATE64 riscv_exception_state64_t;
typedef _STRUCT_RISCV_PAGEIN_STATE      riscv_pagein_state_t;

#define RISCV_THREAD_STATE64_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_thread_state64_t)/sizeof(uint32_t)))
#define RISCV_FLOAT_STATE64_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_float_state64_t)/sizeof(uint32_t)))
#define RISCV_EXCEPTION_STATE64_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_exception_state64_t)/sizeof(uint32_t)))
#define RISCV_PAGEIN_STATE_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_pagein_state_t)/sizeof(uint32_t)))

#define MACHINE_THREAD_STATE       RISCV_THREAD_STATE64
#define MACHINE_THREAD_STATE_COUNT RISCV_THREAD_STATE64_COUNT

#define THREAD_MACHINE_STATE_MAX THREAD_STATE_MAX


#endif /* defined (__riscv) */

#endif /* _RISCV_THREAD_STATUS_H_ */
