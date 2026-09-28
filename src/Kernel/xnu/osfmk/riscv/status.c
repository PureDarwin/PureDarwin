/*
 * Copyright (c) 2007-2024 Apple Inc. All rights reserved.
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
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/thread_status.h>
#include <kern/thread.h>
#include <kern/kalloc.h>
#include <riscv/vmparam.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/proc_reg.h>
#include <riscv/fp_internal.h>

// Forward definitions
void thread_set_child(thread_t child, int pid);
void thread_set_parent(thread_t parent, int pid);

// Maps state flavor to number of words in the state
/* __private_extern__ */
unsigned int _MachineStateCount[THREAD_STATE_FLAVORS] = {
	[RISCV_THREAD_STATE64] = RISCV_THREAD_STATE64_COUNT,
	[RISCV_FLOAT_STATE64] = RISCV_FLOAT_STATE64_COUNT,
	[RISCV_EXCEPTION_STATE64] = RISCV_EXCEPTION_STATE64_COUNT,
	[RISCV_PAGEIN_STATE] = RISCV_PAGEIN_STATE_COUNT,
};

// what a user thread starts with, user mode, interrupts on at sret and no fp state yet
#define RISCV_USER_SSTATUS_DEFAULT (SSTATUS_SPIE | SSTATUS_UXL_64 | SSTATUS_FS_OFF)

// Copy values from saved_state to ts64.
void
saved_state_to_thread_state64(const riscv_saved_state_t * saved_state,
    riscv_thread_state64_t *    ts64)
{
	uint32_t i;

	ts64->x[0] = 0;
	for (i = 1; i < 32; i++) {
		ts64->x[i] = get_saved_state_reg(saved_state, i);
	}
	ts64->pc = get_saved_state_pc(saved_state);
}

// Copy values from ts64 to saved_state. sstatus is never part of the thread state,
// the saved copy keeps the user mode bits the thread already had.
void
thread_state64_to_saved_state(const riscv_thread_state64_t * ts64,
    riscv_saved_state_t *          saved_state)
{
	uint32_t i;

	for (i = 1; i < 32; i++) {
		set_saved_state_reg(saved_state, i, ts64->x[i]);
	}
	set_saved_state_pc(saved_state, ts64->pc);
}

static kern_return_t
handle_get_riscv_thread_state(thread_state_t            tstate,
    mach_msg_type_number_t *  count,
    const riscv_saved_state_t *saved_state)
{
	if (*count < RISCV_THREAD_STATE64_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}

	saved_state_to_thread_state64(saved_state, (riscv_thread_state64_t *)tstate);
	*count = RISCV_THREAD_STATE64_COUNT;
	return KERN_SUCCESS;
}

static kern_return_t
handle_set_riscv_thread_state(const thread_state_t   tstate,
    mach_msg_type_number_t count,
    riscv_saved_state_t *  saved_state)
{
	if (count < RISCV_THREAD_STATE64_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_state64_to_saved_state((const riscv_thread_state64_t *)tstate, saved_state);
	return KERN_SUCCESS;
}

// Translate thread state arguments to userspace representation

kern_return_t
machine_thread_state_convert_to_user(
	__unused thread_t thread,
	__unused thread_flavor_t flavor,
	__unused thread_state_t tstate,
	__unused mach_msg_type_number_t *count,
	__unused thread_set_status_flags_t tssf_flags)
{
	// no conversion to userspace representation on this platform
	return KERN_SUCCESS;
}

// Translate thread state arguments from userspace representation

kern_return_t
machine_thread_state_convert_from_user(
	__unused thread_t thread,
	__unused thread_flavor_t flavor,
	__unused thread_state_t tstate,
	__unused mach_msg_type_number_t count,
	__unused thread_state_t old_tstate,
	__unused mach_msg_type_number_t old_count,
	__unused thread_set_status_flags_t tssf_flags)
{
	// no conversion from userspace representation on this platform
	return KERN_SUCCESS;
}

// Translate signal context data pointer to userspace representation

kern_return_t
machine_thread_siguctx_pointer_convert_to_user(
	__unused thread_t thread,
	__unused user_addr_t *uctxp)
{
	// no conversion to userspace representation on this platform
	return KERN_SUCCESS;
}

// Translate array of function pointer syscall arguments from userspace representation

kern_return_t
machine_thread_function_pointers_convert_from_user(
	__unused thread_t thread,
	__unused user_addr_t *fptrs,
	__unused uint32_t count)
{
	// no conversion from userspace representation on this platform
	return KERN_SUCCESS;
}

kern_return_t
machine_thread_get_state(thread_t                 thread,
    thread_flavor_t          flavor,
    thread_state_t           tstate,
    mach_msg_type_number_t * count)
{
	switch (flavor) {
	case THREAD_STATE_FLAVOR_LIST:
	case THREAD_STATE_FLAVOR_LIST_NEW:
		if (*count < 3) {
			return KERN_INVALID_ARGUMENT;
		}

		tstate[0] = RISCV_THREAD_STATE64;
		tstate[1] = RISCV_FLOAT_STATE64;
		tstate[2] = RISCV_EXCEPTION_STATE64;
		*count = 3;
		break;

	case THREAD_STATE_FLAVOR_LIST_10_15:
		if (*count < 4) {
			return KERN_INVALID_ARGUMENT;
		}

		tstate[0] = RISCV_THREAD_STATE64;
		tstate[1] = RISCV_FLOAT_STATE64;
		tstate[2] = RISCV_EXCEPTION_STATE64;
		tstate[3] = RISCV_PAGEIN_STATE;
		*count = 4;
		break;

	case RISCV_THREAD_STATE64:
	{
		if (thread->machine.upcb == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		kern_return_t rn = handle_get_riscv_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;
	}

	case RISCV_EXCEPTION_STATE64:{
		riscv_exception_state64_t *state;
		const riscv_saved_state_t *saved_state;

		if (*count < RISCV_EXCEPTION_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread->machine.upcb == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (riscv_exception_state64_t *) tstate;
		saved_state = thread->machine.upcb;

		state->scause = get_saved_state_scause(saved_state);
		state->stval = get_saved_state_stval(saved_state);
		state->sepc = get_saved_state_pc(saved_state);

		*count = RISCV_EXCEPTION_STATE64_COUNT;
		break;
	}

	case RISCV_FLOAT_STATE64:{
		riscv_float_state64_t *state;

		if (*count < RISCV_FLOAT_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread->machine.ufpcb == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		// registers the current thread changed since they were loaded are only live
		if (thread == current_thread()) {
			fp_state_flush_current(thread);
		}

		state = (riscv_float_state64_t *)tstate;
		static_assert(sizeof(*state) == sizeof(*thread->machine.ufpcb));
		bcopy(thread->machine.ufpcb, state, sizeof(*state));

		*count = RISCV_FLOAT_STATE64_COUNT;
		break;
	}

	case RISCV_PAGEIN_STATE: {
		riscv_pagein_state_t *state;

		if (*count < RISCV_PAGEIN_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (riscv_pagein_state_t *)tstate;
		state->pagein_error = thread->t_pagein_error;

		*count = RISCV_PAGEIN_STATE_COUNT;
		break;
	}

	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}


kern_return_t
machine_thread_get_kern_state(thread_t                 thread,
    thread_flavor_t          flavor,
    thread_state_t           tstate,
    mach_msg_type_number_t * count)
{
	// This works only for an interrupted kernel thread
	if (thread != current_thread() || getCpuDatap()->cpu_int_state == NULL) {
		return KERN_FAILURE;
	}

	switch (flavor) {
	case RISCV_THREAD_STATE64:
	{
		kern_return_t rn = handle_get_riscv_thread_state(tstate, count, getCpuDatap()->cpu_int_state);
		if (rn) {
			return rn;
		}
		break;
	}
	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}

void
machine_thread_switch_addrmode(thread_t thread)
{
	// every riscv64 task is 64 bit, only the fp state starts over
	if (thread->machine.ufpcb != NULL) {
		if (thread == current_thread()) {
			fp_state_discard_current();
		}
		fp_state_initialize(thread->machine.ufpcb);
	}
}

kern_return_t
machine_thread_set_state(thread_t               thread,
    thread_flavor_t        flavor,
    thread_state_t         tstate,
    mach_msg_type_number_t count)
{
	kern_return_t rn;

	switch (flavor) {
	case RISCV_THREAD_STATE64:
		if (thread->machine.upcb == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		rn = handle_set_riscv_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;

	case RISCV_FLOAT_STATE64:{
		const riscv_float_state64_t *state;

		if (count < RISCV_FLOAT_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread->machine.ufpcb == NULL) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (const riscv_float_state64_t *)tstate;

		// the live registers would hide the new state, the next fp use loads it
		if (thread == current_thread()) {
			fp_state_discard_current();
		}

		bcopy(state, thread->machine.ufpcb, sizeof(*state));
		thread->machine.ufpcb->pad = 0;
		break;
	}

	case RISCV_EXCEPTION_STATE64:
		// the trap csrs are only ever reported
		return KERN_INVALID_ARGUMENT;

	case RISCV_PAGEIN_STATE:
		return KERN_INVALID_ARGUMENT;

	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}

mach_vm_address_t
machine_thread_pc(thread_t thread)
{
	riscv_saved_state_t *ss = get_user_regs(thread);
	return (mach_vm_address_t)get_saved_state_pc(ss);
}

void
machine_thread_reset_pc(thread_t thread, mach_vm_address_t pc)
{
	set_saved_state_pc(get_user_regs(thread), (uint64_t)pc);
}

void
machine_thread_state_initialize(thread_t thread)
{
	riscv_context_t *context = thread->machine.contextData;

	// Should always be set up later. User threads get it in setup_wqthread, bsdthread_create,
	// load_main() or load_unixthread(), kernel threads don't care.

	if (context != NULL) {
		if (thread == current_thread()) {
			fp_state_discard_current();
		}

		bzero(&context->ss, sizeof(context->ss));
		fp_state_initialize(&context->fs);

		context->ss.sstatus = RISCV_USER_SSTATUS_DEFAULT;
	}
}

kern_return_t
machine_thread_dup(thread_t self,
    thread_t target,
    __unused boolean_t is_corpse)
{
	riscv_saved_state_t *self_saved_state;
	riscv_saved_state_t *target_saved_state;

	target->machine.cthread_self = self->machine.cthread_self;

	self_saved_state = self->machine.upcb;
	target_saved_state = target->machine.upcb;
	bcopy(self_saved_state, target_saved_state, sizeof(riscv_saved_state_t));

	// the parent's newest fp registers may still be live
	if (self == current_thread()) {
		fp_state_flush_current(self);
	}

	riscv_float_saved_state_t *self_fp_state = self->machine.ufpcb;
	riscv_float_saved_state_t *target_fp_state = target->machine.ufpcb;
	bcopy(self_fp_state, target_fp_state, sizeof(*target_fp_state));

	return KERN_SUCCESS;
}

riscv_saved_state_t *
get_user_regs(thread_t thread)
{
	return thread->machine.upcb;
}

riscv_saved_state_t *
find_user_regs(thread_t thread)
{
	return thread->machine.upcb;
}

riscv_float_saved_state_t *
find_user_fp(thread_t thread)
{
	return thread->machine.ufpcb;
}

riscv_saved_state_t *
find_kern_regs(thread_t thread)
{
	// This works only for an interrupted kernel thread
	if (thread != current_thread() || getCpuDatap()->cpu_int_state == NULL) {
		return (riscv_saved_state_t *) NULL;
	} else {
		return getCpuDatap()->cpu_int_state;
	}
}

kern_return_t
thread_userstack(__unused thread_t  thread,
    int                flavor,
    thread_state_t     tstate,
    unsigned int       count,
    mach_vm_offset_t * user_stack,
    int *              customstack,
    boolean_t          is_64bit_data
    )
{
	register_t sp;

	switch (flavor) {
	case RISCV_THREAD_STATE64:
		if (count != RISCV_THREAD_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!is_64bit_data) {
			return KERN_INVALID_ARGUMENT;
		}

		sp = ((riscv_thread_state64_t *)tstate)->x[RISCV_REG_SP];
		break;
	default:
		return KERN_INVALID_ARGUMENT;
	}

	if (sp) {
		*user_stack = CAST_USER_ADDR_T(sp);
		if (customstack) {
			*customstack = 1;
		}
	} else {
		*user_stack = CAST_USER_ADDR_T(USRSTACK64);
		if (customstack) {
			*customstack = 0;
		}
	}

	return KERN_SUCCESS;
}

// Return the default stack location for the thread, if otherwise unknown.
kern_return_t
thread_userstackdefault(mach_vm_offset_t * default_user_stack,
    boolean_t          is64bit)
{
	if (is64bit) {
		*default_user_stack = USRSTACK64;
	} else {
		*default_user_stack = USRSTACK;
	}

	return KERN_SUCCESS;
}

void
thread_setuserstack(thread_t          thread,
    mach_vm_address_t user_stack)
{
	riscv_saved_state_t *sv;

	sv = get_user_regs(thread);

	set_saved_state_sp(sv, user_stack);

	return;
}

user_addr_t
thread_adjuserstack(thread_t thread,
    int      adjust)
{
	riscv_saved_state_t *sv;
	uint64_t sp;

	sv = get_user_regs(thread);

	sp = get_saved_state_sp(sv);
	sp += adjust;
	set_saved_state_sp(sv, sp);

	return sp;
}


void
thread_setentrypoint(thread_t         thread,
    mach_vm_offset_t entry)
{
	riscv_saved_state_t *sv;

	sv = get_user_regs(thread);

	set_saved_state_pc(sv, entry);

	return;
}

kern_return_t
thread_entrypoint(__unused thread_t  thread,
    int                flavor,
    thread_state_t     tstate,
    unsigned int       count,
    mach_vm_offset_t * entry_point
    )
{
	switch (flavor) {
	case RISCV_THREAD_STATE64:
	{
		riscv_thread_state64_t *state;

		if (count != RISCV_THREAD_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (riscv_thread_state64_t *) tstate;

		// If a valid entry point is specified, use it.
		if (state->pc) {
			*entry_point = CAST_USER_ADDR_T(state->pc);
		} else {
			*entry_point = CAST_USER_ADDR_T(VM_MIN_ADDRESS);
		}

		break;
	}
	default:
		return KERN_INVALID_ARGUMENT;
	}

	return KERN_SUCCESS;
}


void
thread_set_child(thread_t child,
    int      pid)
{
	riscv_saved_state_t *child_state;

	child_state = get_user_regs(child);

	set_saved_state_reg(child_state, RISCV_REG_A0, pid);
	set_saved_state_reg(child_state, RISCV_REG_A0 + 1, 1ULL);
	set_saved_state_reg(child_state, RISCV_REG_T0, 0);
}

void
thread_set_parent(thread_t parent,
    int      pid)
{
	riscv_saved_state_t *parent_state;

	parent_state = get_user_regs(parent);

	set_saved_state_reg(parent_state, RISCV_REG_A0, pid);
	set_saved_state_reg(parent_state, RISCV_REG_A0 + 1, 0);
	set_saved_state_reg(parent_state, RISCV_REG_T0, 0);
}


struct riscv_act_context {
	riscv_thread_state64_t ss;
	riscv_float_state64_t  fs;
};

void *
act_thread_csave(void)
{
	struct riscv_act_context *ic;
	kern_return_t   kret;
	unsigned int    val;
	thread_t thread = current_thread();

	ic = kalloc_type(struct riscv_act_context, Z_WAITOK);
	if (ic == (struct riscv_act_context *) NULL) {
		return (void *) 0;
	}

	val = RISCV_THREAD_STATE64_COUNT;
	kret = machine_thread_get_state(thread, RISCV_THREAD_STATE64, (thread_state_t)&ic->ss, &val);
	if (kret != KERN_SUCCESS) {
		kfree_type(struct riscv_act_context, ic);
		return (void *) 0;
	}

	val = RISCV_FLOAT_STATE64_COUNT;
	kret = machine_thread_get_state(thread, RISCV_FLOAT_STATE64, (thread_state_t)&ic->fs, &val);
	if (kret != KERN_SUCCESS) {
		kfree_type(struct riscv_act_context, ic);
		return (void *) 0;
	}
	return ic;
}

void
act_thread_catt(void * ctx)
{
	struct riscv_act_context *ic;
	kern_return_t   kret;
	thread_t thread = current_thread();

	ic = (struct riscv_act_context *) ctx;
	if (ic == (struct riscv_act_context *) NULL) {
		return;
	}

	kret = machine_thread_set_state(thread, RISCV_THREAD_STATE64, (thread_state_t)&ic->ss, RISCV_THREAD_STATE64_COUNT);
	if (kret != KERN_SUCCESS) {
		goto out;
	}

	kret = machine_thread_set_state(thread, RISCV_FLOAT_STATE64, (thread_state_t)&ic->fs, RISCV_FLOAT_STATE64_COUNT);
	if (kret != KERN_SUCCESS) {
		goto out;
	}
out:
	kfree_type(struct riscv_act_context, ic);
}

void
act_thread_cfree(void *ctx)
{
	kfree_type(struct riscv_act_context, ctx);
}

kern_return_t
thread_set_wq_state32(__unused thread_t       thread,
    __unused thread_state_t tstate)
{
	// every riscv64 thread is 64 bit, pthread never asks for this
	panic("32 bit workqueue thread state not implemented on riscv64");
}

kern_return_t
thread_set_wq_state64(thread_t       thread,
    thread_state_t tstate)
{
	riscv_thread_state64_t *state;
	riscv_saved_state_t *saved_state;
	thread_t curth = current_thread();
	spl_t s = 0;

	assert(thread_is_64bit_data(thread));

	saved_state = thread->machine.upcb;
	state = (riscv_thread_state64_t *)tstate;

	if (curth != thread) {
		s = splsched();
		thread_lock(thread);
	}

	// do not zero saved_state, it's concurrently accessed and zero is invalid for some registers (sp).
	// pthread set the tsd base in tp before this, the start state doesn't carry it
	uint64_t tsd_base = get_saved_state_reg(saved_state, RISCV_REG_TP);
	thread_state64_to_saved_state(state, saved_state);
	set_saved_state_reg(saved_state, RISCV_REG_TP, tsd_base);
	saved_state->sstatus = RISCV_USER_SSTATUS_DEFAULT;

	if (curth != thread) {
		thread_unlock(thread);
		splx(s);
	}

	return KERN_SUCCESS;
}
