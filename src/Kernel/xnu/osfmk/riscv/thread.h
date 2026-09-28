/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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
#ifndef _RISCV_THREAD_H_
#define _RISCV_THREAD_H_

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <mach/riscv/vm_types.h>
#include <mach/thread_status.h>

#ifdef MACH_KERNEL_PRIVATE
#include <riscv/cpu_data.h>
#include <os/base.h>
#if SCHED_HYGIENE_DEBUG
#include <kern/timeout_decl.h>
#endif
#endif /* MACH_KERNEL_PRIVATE */

struct perfcontrol_state {
	uint64_t opaque[8] __attribute__((aligned(8)));
};

extern unsigned int _MachineStateCount[];

#ifdef MACH_KERNEL_PRIVATE
typedef riscv_kernel_context_t machine_thread_kernel_state;
#include <kern/thread_kernel_state.h>

struct machine_thread {
	uint64_t                  trap_scratch;            /* t0 while the vector finds the pcb, keep first */
	uint32_t                  riscv_machine_flags;     /* thread flags */
	riscv_context_t *         contextData;             /* allocated user context */
	riscv_saved_state_t *     upcb;                    /* user gpr state */
	riscv_float_saved_state_t *ufpcb;                  /* user fp state */
	riscv_saved_state_t *     kpcb;                    /* kernel gpr state of an interrupted kernel thread */

	uint64_t                  recover_stval;           /* fault address for copyio recovery */
	uint64_t                  recover_scause;
	vm_address_t              cthread_self;            /* user tp */
	uint32_t                  icache_hart_plus1;       /* 1 + cpu that last ran this thread's user code, 0 before */

	void *                    kstackptr;               /* top of kernel stack */
	struct perfcontrol_state  perfctrl_state;

#if SCHED_HYGIENE_DEBUG
	kern_timeout_t            int_timeout;
	unsigned int              int_type;
	uintptr_t                 int_handler_addr;
	uintptr_t                 int_vector;
	uint64_t                  int_time_mt;
#endif /* SCHED_HYGIENE_DEBUG */

	union {
		long              pcpu_data_base_and_cpu_number;
		const uint16_t    cpu_number;
	};
	struct cpu_data *         CpuDatap;               /* current per cpu data */
	unsigned int              preemption_count;       /* preemption count */
	uint16_t                  exception_trace_code;
	bool                      user_synchronous_trap;  /* inside a user mode synchronous trap handler */
};
#endif

static inline long
ml_make_pcpu_base_and_cpu_number(long base, uint16_t cpu)
{
	return (base << 16) | cpu;
}

extern struct riscv_saved_state *       get_user_regs(thread_t);
extern struct riscv_saved_state *       find_user_regs(thread_t);
extern struct riscv_saved_state *       find_kern_regs(thread_t);
extern struct riscv_float_saved_state * find_user_fp(thread_t);

#define FIND_PERFCONTROL_STATE(th) (&th->machine.perfctrl_state)

#ifdef MACH_KERNEL_PRIVATE
extern void fp_state_initialize(struct riscv_float_saved_state *fp_state);
extern void fp_save(struct riscv_float_saved_state *fp_ss);
extern void fp_load(struct riscv_float_saved_state *fp_ss);
#endif /* MACH_KERNEL_PRIVATE */

extern void *act_thread_csave(void);
extern void act_thread_catt(void *ctx);
extern void act_thread_cfree(void *ctx);

#define GET_RETURN_PC(addr) (__builtin_return_address(0))

#endif /* _RISCV_THREAD_H_ */
