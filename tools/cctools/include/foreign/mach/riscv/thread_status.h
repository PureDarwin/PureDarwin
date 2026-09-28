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

#ifdef XNU_KERNEL_PRIVATE
#define RISCV_SAVED_STATE64      20
#define RISCV_FLOAT_SAVED_STATE64 21
#endif /* XNU_KERNEL_PRIVATE */

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

#ifdef XNU_KERNEL_PRIVATE

#include <kern/assert.h>
#include <riscv/proc_reg.h>

// register numbers in the saved state and thread state arrays
#define RISCV_REG_RA    1
#define RISCV_REG_SP    2
#define RISCV_REG_GP    3
#define RISCV_REG_TP    4
#define RISCV_REG_T0    5
#define RISCV_REG_FP    8
#define RISCV_REG_A0    10
#define RISCV_REG_A7    17

// the trap frame, x0 has no register behind it so the slot stays zero
struct riscv_saved_state {
	uint64_t x[32];     /* x0-x31 */
	uint64_t pc;        /* sepc */
	uint64_t sstatus;
	uint64_t scause;
	uint64_t stval;
};
typedef struct riscv_saved_state riscv_saved_state_t;

#define RISCV_SAVED_STATE64_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_saved_state_t)/sizeof(uint32_t)))

struct riscv_float_saved_state {
	uint64_t f[32];
	uint32_t fcsr;
	uint32_t pad;
};
typedef struct riscv_float_saved_state riscv_float_saved_state_t;

#define RISCV_FLOAT_SAVED_STATE64_COUNT ((mach_msg_type_number_t) \
	(sizeof (riscv_float_saved_state_t)/sizeof(uint32_t)))

// the pcb keeps both next to each other, the fp half is only valid when sstatus.fs says so
struct riscv_context {
	riscv_saved_state_t       ss;
	riscv_float_saved_state_t fs;
};
typedef struct riscv_context riscv_context_t;

// callee saved state of a thread switched out in the kernel, the kernel has no fp code
struct riscv_kernel_saved_state {
	uint64_t s[12];     /* s0-s11, s0 is fp */
	uint64_t ra;
	uint64_t sp;
} __attribute__((aligned(16)));
typedef struct riscv_kernel_saved_state riscv_kernel_saved_state_t;

struct riscv_kernel_context {
	struct riscv_kernel_saved_state ss;
};
typedef struct riscv_kernel_context riscv_kernel_context_t;

extern void saved_state_to_thread_state64(const riscv_saved_state_t *, riscv_thread_state64_t *);
extern void thread_state64_to_saved_state(const riscv_thread_state64_t *, riscv_saved_state_t *);

static inline uint64_t
get_saved_state_pc(const riscv_saved_state_t *iss)
{
	return iss->pc;
}

static inline void
set_saved_state_pc(riscv_saved_state_t *iss, uint64_t pc)
{
	iss->pc = pc;
}

static inline void
add_saved_state_pc(riscv_saved_state_t *iss, int diff)
{
	iss->pc += (uint64_t)(int64_t)diff;
}

static inline uint64_t
get_saved_state_sp(const riscv_saved_state_t *iss)
{
	return iss->x[RISCV_REG_SP];
}

static inline void
set_saved_state_sp(riscv_saved_state_t *iss, uint64_t sp)
{
	iss->x[RISCV_REG_SP] = sp;
}

static inline uint64_t
get_saved_state_lr(const riscv_saved_state_t *iss)
{
	return iss->x[RISCV_REG_RA];
}

static inline void
set_saved_state_lr(riscv_saved_state_t *iss, uint64_t lr)
{
	iss->x[RISCV_REG_RA] = lr;
}

static inline uint64_t
get_saved_state_fp(const riscv_saved_state_t *iss)
{
	return iss->x[RISCV_REG_FP];
}

static inline void
set_saved_state_fp(riscv_saved_state_t *iss, uint64_t fp)
{
	iss->x[RISCV_REG_FP] = fp;
}

static inline uint64_t
get_saved_state_reg(const riscv_saved_state_t *iss, unsigned reg)
{
	return reg == 0 ? 0 : iss->x[reg & 31];
}

static inline void
set_saved_state_reg(riscv_saved_state_t *iss, unsigned reg, uint64_t value)
{
	if (reg != 0) {
		iss->x[reg & 31] = value;
	}
}

static inline uint64_t
get_saved_state_scause(const riscv_saved_state_t *iss)
{
	return iss->scause;
}

static inline uint64_t
get_saved_state_stval(const riscv_saved_state_t *iss)
{
	return iss->stval;
}

// the syscall return value, a0 and a1 carry 64 or 128 bit results
static inline uint64_t
get_saved_state_retval(const riscv_saved_state_t *iss)
{
	return iss->x[RISCV_REG_A0];
}

static inline void
set_saved_state_retval(riscv_saved_state_t *iss, uint64_t value)
{
	iss->x[RISCV_REG_A0] = value;
}

#endif /* XNU_KERNEL_PRIVATE */

#endif /* defined (__riscv) */

#endif /* _RISCV_THREAD_STATUS_H_ */
