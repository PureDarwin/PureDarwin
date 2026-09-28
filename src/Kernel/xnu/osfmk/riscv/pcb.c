/*
 * Copyright (c) 2007-2023 Apple Inc. All rights reserved.
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

#include <debug.h>

#include <types.h>

#include <mach/mach_types.h>
#include <mach/thread_status.h>
#include <mach/vm_types.h>

#include <kern/kern_types.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/misc_protos.h>
#include <kern/mach_param.h>
#include <kern/spl.h>
#include <kern/machine.h>
#include <kern/kpc.h>
#include <kern/startup.h>

#include <machine/atomic.h>
#include <riscv/proc_reg.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/fp_internal.h>

#include <vm/vm_map_xnu.h>
#include <vm/vm_protos.h>

#include <sys/kdebug.h>

#include <san/kcov_stksz.h>

#include <pexpert/pexpert.h>

extern int debug_task;

/* zone for the user register and fp state of every user thread */
ZONE_DEFINE_TYPE(user_ss_zone, "user save state", riscv_context_t, ZC_NONE);

void
consider_machine_collect(void)
{
	pmap_gc();
}

void
consider_machine_adjust(void)
{
}

void
fp_state_initialize(struct riscv_float_saved_state *fp_state)
{
	bzero(fp_state, sizeof(*fp_state));
}

// keep the old thread's fp registers if it changed them, then leave the fp
// unit off so the new thread's first fp instruction traps and loads its own
static inline void
machine_switch_fp_context(thread_t old)
{
	if (fp_state_live() == SSTATUS_FS_DIRTY && old->machine.ufpcb != NULL) {
		fp_save(old->machine.ufpcb);
	}
	fp_state_discard_current();
}

static inline void
machine_thread_switch_cpu_data(thread_t old, thread_t new)
{
	// Built with -fno-strict-aliasing, so load through temporaries to get a single
	// load/store pair.
	cpu_data_t *datap = old->machine.CpuDatap;
	vm_offset_t base  = old->machine.pcpu_data_base_and_cpu_number;

	/* TODO: Should this be ordered? */

	old->machine.CpuDatap = NULL;
	old->machine.pcpu_data_base_and_cpu_number = 0;

	new->machine.CpuDatap = datap;
	new->machine.pcpu_data_base_and_cpu_number = base;
}

// Shared by machine_switch_context and machine_stack_handoff: switches the extended
// context and the pmap if needed.
static inline void
machine_switch_pmap_and_extended_context(thread_t old, thread_t new)
{
	pmap_t new_pmap;

	machine_switch_fp_context(old);

	new_pmap = new->map->pmap;
	if (old->map->pmap != new_pmap) {
		pmap_set_pmap(new_pmap, new);
	}

	machine_thread_switch_cpu_data(old, new);

	// harts only see their own fence.i, a user thread arriving from another hart syncs here
	if (new->map->pmap != kernel_pmap) {
		uint32_t here = (uint32_t)cpu_number() + 1;
		if (new->machine.icache_hart_plus1 != here) {
			__asm__ volatile ("fence.i" ::: "memory");
			new->machine.icache_hart_plus1 = here;
		}
	}
}

thread_t
machine_switch_context(thread_t old,
    thread_continue_t continuation,
    thread_t new)
{
	thread_t retval;

#define machine_switch_context_kprintf(x...) \
	/* kprintf("machine_switch_context: " x) */

	if (old == new) {
		panic("machine_switch_context");
	}

#if CONFIG_CPU_COUNTERS
	kpc_off_cpu(old);
#endif /* CONFIG_CPU_COUNTERS */

	machine_switch_pmap_and_extended_context(old, new);

	machine_switch_context_kprintf("old= %x contination = %x new = %x\n", old, continuation, new);

	retval = Switch_context(old, continuation, new);
	assert(retval != NULL);

	return retval;
}

boolean_t
machine_thread_on_core(thread_t thread)
{
	return thread->machine.CpuDatap != NULL;
}

boolean_t
machine_thread_on_core_allow_invalid(thread_t thread)
{
	extern int _copyin_atomic64(const char *src, uint64_t *dst);
	uint64_t addr;

	// The thread zone is sequestered, so this kernel-to-kernel copyin can only read a
	// thread, zeroed or freed memory.
	assert(get_preemption_level() > 0);
	if (thread == THREAD_NULL) {
		return false;
	}
	thread_require(thread);
	if (_copyin_atomic64((void *)&thread->machine.CpuDatap, &addr) == 0) {
		return addr != 0;
	}
	return false;
}

void
machine_thread_create(thread_t thread, task_t task, bool first_thread)
{
#define machine_thread_create_kprintf(x...) \
	/* kprintf("machine_thread_create: " x) */

	machine_thread_create_kprintf("thread = %x\n", thread);

	if (!first_thread) {
		thread->machine.CpuDatap = (cpu_data_t *)0;
		// setting this offset will cause trying to use it to panic
		thread->machine.pcpu_data_base_and_cpu_number =
		    ml_make_pcpu_base_and_cpu_number(VM_MIN_KERNEL_ADDRESS, 0);
	}
	thread->machine.riscv_machine_flags = 0;
	thread->machine.preemption_count = 0;
	thread->machine.cthread_self = 0;
	thread->machine.kpcb = NULL;
	thread->machine.exception_trace_code = 0;
	thread->machine.user_synchronous_trap = false;

	if (task != kernel_task) {
		/* If this isn't a kernel thread, we'll have userspace state. */
		riscv_context_t *contextData = zalloc_flags(user_ss_zone,
		    Z_WAITOK | Z_NOFAIL);

		thread->machine.contextData = contextData;
		thread->machine.upcb = &contextData->ss;
		thread->machine.ufpcb = &contextData->fs;
	} else {
		thread->machine.upcb = NULL;
		thread->machine.ufpcb = NULL;
		thread->machine.contextData = NULL;
	}

	bzero(&thread->machine.perfctrl_state, sizeof(thread->machine.perfctrl_state));
	machine_thread_state_initialize(thread);
}

// Code-signature dependent thread state adjustments. Usually called twice for the main
// thread, at creation (signature maybe not attached yet) and after the signature attaches.
kern_return_t
machine_thread_process_signature(thread_t __unused thread, task_t __unused task)
{
	// no thread state depends on the code signature on riscv
	return KERN_SUCCESS;
}

void
machine_thread_destroy(thread_t thread)
{
	riscv_context_t *thread_user_ss;

	if (thread->machine.contextData) {
		/* Disassociate the user save state from the thread before we free it. */
		thread_user_ss = thread->machine.contextData;
		thread->machine.upcb = NULL;
		thread->machine.ufpcb = NULL;
		thread->machine.contextData = NULL;

		zfree(user_ss_zone, thread_user_ss);
	}
}

void
machine_thread_init(void)
{
}

void
machine_thread_template_init(thread_t __unused thr_template)
{
	/* Nothing to do on this platform. */
}

user_addr_t
get_useraddr()
{
	return get_saved_state_pc(current_thread()->machine.upcb);
}

vm_offset_t
machine_stack_detach(thread_t thread)
{
	vm_offset_t stack;

	KERNEL_DEBUG(MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_DETACH),
	    (uintptr_t)thread_tid(thread), thread->priority, thread->sched_pri, 0, 0);

	stack = thread->kernel_stack;
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(thread, stack);
#endif
	thread->kernel_stack = 0;
	thread->machine.kstackptr = NULL;

	return stack;
}


void
machine_stack_attach(thread_t thread,
    vm_offset_t stack)
{
	struct riscv_kernel_context *context;
	struct riscv_kernel_saved_state *savestate;

#define machine_stack_attach_kprintf(x...) \
	/* kprintf("machine_stack_attach: " x) */

	KERNEL_DEBUG(MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_ATTACH),
	    (uintptr_t)thread_tid(thread), thread->priority, thread->sched_pri, 0, 0);

	thread->kernel_stack = stack;
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(thread, 0);
#endif
	void *kstackptr = (void *)(stack + kernel_stack_size - sizeof(struct thread_kernel_state));
	thread->machine.kstackptr = kstackptr;
	thread_initialize_kernel_state(thread);

	machine_stack_attach_kprintf("kstackptr: %lx\n", (vm_address_t)kstackptr);

	// Switch_context loads this and returns into thread_continue on the empty stack
	context = &((thread_kernel_state_t) thread->machine.kstackptr)->machine;
	savestate = &context->ss;
	bzero(savestate->s, sizeof(savestate->s));
	savestate->sp = (uint64_t)kstackptr;
	savestate->ra = (uintptr_t)thread_continue;

	machine_stack_attach_kprintf("thread = %p pc = %llx, sp = %llx\n", thread, savestate->ra, savestate->sp);
}


void
machine_stack_handoff(thread_t old,
    thread_t new)
{
	vm_offset_t  stack;

#if CONFIG_CPU_COUNTERS
	kpc_off_cpu(old);
#endif /* CONFIG_CPU_COUNTERS */

	stack = machine_stack_detach(old);
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(new, 0);
#endif
	new->kernel_stack = stack;
	void *kstackptr = (void *)(stack + kernel_stack_size - sizeof(struct thread_kernel_state));
	new->machine.kstackptr = kstackptr;
	if (stack == old->reserved_stack) {
		assert(new->reserved_stack);
		old->reserved_stack = new->reserved_stack;
		new->reserved_stack = stack;
	}

	machine_switch_pmap_and_extended_context(old, new);

	machine_set_current_thread(new);
	thread_initialize_kernel_state(new);
}


void
call_continuation(thread_continue_t continuation,
    void *parameter,
    wait_result_t wresult,
    boolean_t enable_interrupts)
{
#define call_continuation_kprintf(x...) \
	/* kprintf("call_continuation_kprintf:" x) */

	call_continuation_kprintf("thread = %p continuation = %p, stack = %lx\n",
	    current_thread(), continuation, current_thread()->machine.kstackptr);
	Call_continuation(continuation, parameter, wresult, enable_interrupts);
}

kern_return_t
machine_thread_set_tsd_base(thread_t         thread,
    mach_vm_offset_t tsd_base)
{
	if (get_threadtask(thread) == kernel_task) {
		return KERN_INVALID_ARGUMENT;
	}

	if (thread_is_64bit_addr(thread)) {
		if (tsd_base > vm_map_max(thread->map)) {
			tsd_base = 0ULL;
		}
	} else {
		if (tsd_base > UINT32_MAX) {
			tsd_base = 0ULL;
		}
	}

	thread->machine.cthread_self = tsd_base;

	// user tp lives in the pcb, the return to user loads it
	if (thread->machine.upcb != NULL) {
		set_saved_state_reg(thread->machine.upcb, RISCV_REG_TP, tsd_base);
	}

	return KERN_SUCCESS;
}

void
machine_tecs(__unused thread_t thr)
{
}

int
machine_csv(__unused cpuvn_e cve)
{
	return 0;
}
