/*
 * Copyright (c) 2012-2023 Apple Inc. All rights reserved.
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

#include <riscv/cpu_data.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/thread.h>
#include <riscv/rtclock.h>
#include <riscv/trap_internal.h>
#include <riscv/proc_reg.h>
#include <riscv/machine_cpu.h>
#include <riscv/machine_routines.h>
#include <riscv/fp_internal.h>
#include <kern/ast.h>
#include <kern/debug.h>
#include <kern/locks.h>
#include <kern/restartable.h>
#include <kern/sched_prim.h>
#include <kern/startup.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/zalloc_internal.h>
#include <kern/recount.h>
#include <kern/policy_internal.h>
#include <kern/trap_telemetry.h>
#include <mach/exception.h>
#include <mach/riscv/traps.h>
#include <mach/vm_types.h>
#include <mach/machine/thread_status.h>
#include <machine/atomic.h>
#include <pexpert/pexpert.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_fault.h>
#include <vm/vm_kern.h>
#include <vm/vm_map_xnu.h>
#include <sys/errno.h>
#include <sys/kdebug.h>
#include <kperf/kperf.h>
#include <prng/entropy.h>

#if DEBUG || DEVELOPMENT
#define HAS_TELEMETRY_KERNEL_BRK 1
#endif

riscv_saved_state_t *sleh_kernel(riscv_saved_state_t *ss);
void sleh_user(riscv_saved_state_t *ss) __dead2;

void panic_with_thread_kernel_state(const char *msg, riscv_saved_state_t *ss) __abortlike;
void mach_syscall_trace_exit(unsigned int retval, unsigned int call_number);

// locore helpers, the first never returns and resumes the kernel from the frame's copy
extern void riscv_handle_on_stack(riscv_saved_state_t *frame, vm_offset_t stack_top,
    void (*fn)(riscv_saved_state_t *)) __dead2;
extern void riscv_call_on_stack(void *arg, void (*fn)(void *), vm_offset_t stack_top);

extern void unix_syscall(riscv_saved_state_t *regs, thread_t thread_act, struct proc *proc);
extern void mach_syscall(riscv_saved_state_t *);
extern void current_cached_proc_cred_update(void);

#if CONFIG_DTRACE
extern void dtrace_thread_bootstrap(void);
#endif

static void sleh_kernel_synchronous(riscv_saved_state_t *ss, bool on_thread_stack);
static void sleh_kernel_synchronous_on_thread_stack(riscv_saved_state_t *ss);
static void sleh_kernel_interrupt(riscv_saved_state_t *ss);
static void sleh_kernel_preempt(riscv_saved_state_t *ss);
static void sleh_user_interrupt(void *ss);
static void sleh_user_synchronous(riscv_saved_state_t *ss);
static void sleh_interrupt(riscv_saved_state_t *ss, bool is_user);

static void sleh_interrupt_handler_prologue(riscv_saved_state_t *, unsigned int type);
static void sleh_interrupt_handler_epilogue(void);

static void handle_ecall(riscv_saved_state_t *);
static void handle_mach_absolute_time_trap(riscv_saved_state_t *);
static void handle_mach_continuous_time_trap(riscv_saved_state_t *);

static void handle_user_page_fault(riscv_saved_state_t *, uint64_t) ;
static void handle_kernel_page_fault(riscv_saved_state_t *, uint64_t, bool);
static void handle_user_illegal(riscv_saved_state_t *);
static void handle_user_breakpoint(riscv_saved_state_t *) __dead2;
static void handle_kernel_breakpoint(riscv_saved_state_t *);
static void handle_user_bad_access(riscv_saved_state_t *, mach_exception_data_type_t) __dead2;

// the per cpu data comes from gp, which is valid even before the first thread exists
static inline cpu_data_t *
sleh_cpu_datap(void)
{
	unsigned long result;
	__asm__ volatile ("mv %0, gp" : "=r" (result));
	return (cpu_data_t *)result;
}

static inline bool
saved_state_is_user(const riscv_saved_state_t *ss)
{
	return (ss->sstatus & SSTATUS_SPP) == 0;
}

static inline bool
saved_state_interrupts_enabled(const riscv_saved_state_t *ss)
{
	return (ss->sstatus & SSTATUS_SPIE) != 0;
}

static inline vm_prot_t
sleh_fault_type(uint64_t code)
{
	switch (code) {
	case T_INSN_PAGE_FAULT:
		return VM_PROT_READ | VM_PROT_EXECUTE;
	case T_STORE_PAGE_FAULT:
		return VM_PROT_READ | VM_PROT_WRITE;
	default:
		return VM_PROT_READ;
	}
}

// the copyio routines' fault recovery ranges, offsets from copyio_recover_table as on arm64
struct copyio_recovery_entry {
	ptrdiff_t cre_start;
	ptrdiff_t cre_end;
	ptrdiff_t cre_recovery;
};

extern struct copyio_recovery_entry copyio_recover_table[];
extern struct copyio_recovery_entry copyio_recover_table_end[];

// the recovery pc for a faulting kernel pc, 0 when the pc isn't in a copyio routine
static vm_offset_t
copyio_recovery_pc(vm_offset_t pc)
{
	ptrdiff_t offset = (ptrdiff_t)(pc - (uintptr_t)copyio_recover_table);

	for (struct copyio_recovery_entry *e = copyio_recover_table; e < copyio_recover_table_end; e++) {
		if (offset >= e->cre_start && offset < e->cre_end) {
			return (uintptr_t)copyio_recover_table + (uintptr_t)e->cre_recovery;
		}
	}
	return 0;
}

void
panic_with_thread_kernel_state(const char *msg, riscv_saved_state_t *ss)
{
	panic_plain("%s at pc 0x%016llx, ra 0x%016llx (saved state: %p)\n"
	    "\t  ra:  0x%016llx sp:  0x%016llx  gp:  0x%016llx  tp:  0x%016llx\n"
	    "\t  t0:  0x%016llx t1:  0x%016llx  t2:  0x%016llx  s0:  0x%016llx\n"
	    "\t  s1:  0x%016llx a0:  0x%016llx  a1:  0x%016llx  a2:  0x%016llx\n"
	    "\t  a3:  0x%016llx a4:  0x%016llx  a5:  0x%016llx  a6:  0x%016llx\n"
	    "\t  a7:  0x%016llx s2:  0x%016llx  s3:  0x%016llx  s4:  0x%016llx\n"
	    "\t  s5:  0x%016llx s6:  0x%016llx  s7:  0x%016llx  s8:  0x%016llx\n"
	    "\t  s9:  0x%016llx s10: 0x%016llx  s11: 0x%016llx  t3:  0x%016llx\n"
	    "\t  t4:  0x%016llx t5:  0x%016llx  t6:  0x%016llx\n"
	    "\t  pc:  0x%016llx sstatus: 0x%016llx scause: 0x%016llx stval: 0x%016llx\n",
	    msg, ss->pc, ss->x[1], ss,
	    ss->x[1], ss->x[2], ss->x[3], ss->x[4],
	    ss->x[5], ss->x[6], ss->x[7], ss->x[8],
	    ss->x[9], ss->x[10], ss->x[11], ss->x[12],
	    ss->x[13], ss->x[14], ss->x[15], ss->x[16],
	    ss->x[17], ss->x[18], ss->x[19], ss->x[20],
	    ss->x[21], ss->x[22], ss->x[23], ss->x[24],
	    ss->x[25], ss->x[26], ss->x[27], ss->x[28],
	    ss->x[29], ss->x[30], ss->x[31],
	    ss->pc, ss->sstatus, ss->scause, ss->stval);
}

// check whether task vtimers are running and set thread and CPU BSD AST. Needs interrupts
// masked for atomic updates, and inline to avoid an FBT probe on the exception path.
__attribute__((__always_inline__))
static inline void
task_vtimer_check(thread_t thread)
{
	task_t task = get_threadtask_early(thread);

	if (__improbable(task != NULL && task->vtimers)) {
		thread_ast_set(thread, AST_BSD);
		thread->machine.CpuDatap->cpu_pending_ast |= AST_BSD;
	}
}

#if MACH_ASSERT
// get_preemption_level() that works in early boot, before the initial thread exists,
// where the regular one would recurse infinitely in the SLEH.
static inline int
sleh_get_preemption_level(void)
{
	if (__improbable(current_thread() == NULL)) {
		return 0;
	}
	return get_preemption_level();
}
#endif // MACH_ASSERT

// the interrupted sp is on the thread's own kernel stack, a page clear of its bottom
static bool
sleh_on_thread_stack(thread_t thread, vm_offset_t sp)
{
	if (thread == THREAD_NULL || thread->kernel_stack == 0) {
		return false;
	}
	return sp >= thread->kernel_stack + PAGE_SIZE &&
	       sp <= thread->kernel_stack + kernel_stack_size;
}

// the interrupted sp is in the guard page or the last page of the thread's stack
static bool
sleh_stack_overflowed(thread_t thread, vm_offset_t sp)
{
	if (thread == THREAD_NULL || thread->kernel_stack == 0) {
		return false;
	}
	return sp >= thread->kernel_stack - PAGE_SIZE &&
	       sp < thread->kernel_stack + PAGE_SIZE;
}

// Kernel traps land on the per cpu exception stack. Blocking work moves to the thread's
// stack, interrupts to the interrupt stack, the rest resumes from the returned frame.
riscv_saved_state_t *
sleh_kernel(riscv_saved_state_t *ss)
{
	cpu_data_t *cdp = sleh_cpu_datap();
	thread_t thread = current_thread();
	vm_offset_t sp = get_saved_state_sp(ss);

	// copyio's user access window stays in the frame and closes for the handler
	csr_clear(sstatus, SSTATUS_SUM);

	if (ss->scause & SCAUSE_INTERRUPT) {
		vm_offset_t top = cdp->intstack_top;

		// a trap taken on the interrupt stack nests below the frame it interrupted
		if (sp > top - INTSTACK_SIZE && sp <= top) {
			top = sp;
		}
		riscv_handle_on_stack(ss, top & ~0xfUL, sleh_kernel_interrupt);
	}

	if (__improbable(sleh_stack_overflowed(thread, sp))) {
		panic_with_thread_kernel_state("Invalid kernel stack pointer (probable overflow).", ss);
	}

	if (sleh_on_thread_stack(thread, sp)) {
		riscv_handle_on_stack(ss, sp & ~0xfUL, sleh_kernel_synchronous_on_thread_stack);
	}

	// early boot, the interrupt stack or the exception stack itself, nothing here may block
	sleh_kernel_synchronous(ss, false);
	return ss;
}

static void
sleh_kernel_synchronous_on_thread_stack(riscv_saved_state_t *ss)
{
	sleh_kernel_synchronous(ss, true);

	// the interrupted code could have been preempted, so it can be now
	sleh_kernel_preempt(ss);
}

static void
sleh_kernel_synchronous(riscv_saved_state_t *ss, bool on_thread_stack)
{
	uint64_t code = SCAUSE_CODE(ss->scause);
	thread_t thread = current_thread();
#if MACH_ASSERT
	int preemption_level = sleh_get_preemption_level();
#endif

	if (thread != THREAD_NULL) {
		task_vtimer_check(thread);
	}

	// breakpoints are handled with interrupts still masked, a panic keeps the state intact
	if (code == T_BREAKPOINT) {
		handle_kernel_breakpoint(ss);
		goto out;
	}

	/* Inherit the interrupt masks from previous context */
	if (on_thread_stack && saved_state_interrupts_enabled(ss)) {
		ml_set_interrupts_enabled(TRUE);
	}

	switch (code) {
	case T_LOAD_PAGE_FAULT:
	case T_STORE_PAGE_FAULT:
		handle_kernel_page_fault(ss, code, on_thread_stack);
		break;

	case T_INSN_PAGE_FAULT:
		panic_with_thread_kernel_state("Kernel instruction fetch abort", ss);

	case T_INSN_ACCESS:
		panic_with_thread_kernel_state("Kernel instruction access fault", ss);

	case T_LOAD_MISALIGNED:
	case T_STORE_MISALIGNED:
		if (thread != THREAD_NULL && copyio_recovery_pc(get_saved_state_pc(ss))) {
			goto recover;
		}
		panic_with_thread_kernel_state("Unaligned kernel data abort.", ss);

	case T_LOAD_ACCESS:
	case T_STORE_ACCESS:
		if (thread != THREAD_NULL && copyio_recovery_pc(get_saved_state_pc(ss))) {
			goto recover;
		}
		panic_with_thread_kernel_state("Kernel data access fault", ss);

	case T_INSN_MISALIGNED:
		panic_with_thread_kernel_state("Unaligned kernel instruction fetch", ss);

	case T_ILLEGAL:
		panic_with_thread_kernel_state("Kernel illegal instruction", ss);

	default:
		panic_with_thread_kernel_state("Unexpected kernel exception", ss);
	}
	goto out;

recover:
	thread->machine.recover_scause = ss->scause;
	thread->machine.recover_stval = ss->stval;
	set_saved_state_reg(ss, RISCV_REG_A0, EFAULT);
	set_saved_state_pc(ss, copyio_recovery_pc(get_saved_state_pc(ss)));

out:
	ml_set_interrupts_enabled(FALSE);

#if MACH_ASSERT
	if (preemption_level != sleh_get_preemption_level()) {
		panic("synchronous exception changed preemption level from %d to %d", preemption_level, sleh_get_preemption_level());
	}
#endif
}

static void
sleh_kernel_interrupt(riscv_saved_state_t *ss)
{
	thread_t thread = current_thread();

	sleh_interrupt(ss, false);

	// an urgent ast preempts on the interrupted thread's own stack
	if (sleh_on_thread_stack(thread, get_saved_state_sp(ss)) &&
	    saved_state_interrupts_enabled(ss) &&
	    get_preemption_level() == 0 &&
	    (sleh_cpu_datap()->cpu_pending_ast & AST_URGENT)) {
		riscv_handle_on_stack(ss, get_saved_state_sp(ss) & ~0xfUL, sleh_kernel_preempt);
	}
}

// what locore's kernel return does on other ports, with interrupts masked
static void
sleh_kernel_preempt(riscv_saved_state_t *ss)
{
	if (saved_state_interrupts_enabled(ss) &&
	    get_preemption_level() == 0 &&
	    (sleh_cpu_datap()->cpu_pending_ast & AST_URGENT)) {
		ast_taken_kernel();
	}
}

// User traps arrive on the thread's kernel stack with interrupts masked and state in the pcb,
// every path leaves through thread_exception_return.
void
sleh_user(riscv_saved_state_t *ss)
{
	thread_t thread = current_thread();

	csr_clear(sstatus, SSTATUS_SUM);

	if (ss->scause & SCAUSE_INTERRUPT) {
		riscv_call_on_stack(ss, sleh_user_interrupt, sleh_cpu_datap()->intstack_top);
	} else {
		thread->machine.user_synchronous_trap = true;
		sleh_user_synchronous(ss);
	}

	thread_exception_return();
}

static void
sleh_user_interrupt(void *ss)
{
	sleh_interrupt((riscv_saved_state_t *)ss, true);
}

static void
sleh_user_synchronous(riscv_saved_state_t *ss)
{
	uint64_t code = SCAUSE_CODE(ss->scause);
	thread_t thread = current_thread();
	mach_exception_data_type_t codes[2];
#if MACH_ASSERT
	int preemption_level = sleh_get_preemption_level();
#endif

	task_vtimer_check(thread);

	if (code == T_LOAD_PAGE_FAULT || code == T_STORE_PAGE_FAULT) {
		thread_reset_pcs_will_fault(thread);
	}

	/* user code always runs with interrupts on */
	ml_set_interrupts_enabled(TRUE);

	switch (code) {
	case T_ECALL_U:
		// the syscall returns past the ecall, and restarts by stepping back onto it
		add_saved_state_pc(ss, 4);
		handle_ecall(ss);
		break;

	case T_INSN_PAGE_FAULT:
	case T_LOAD_PAGE_FAULT:
	case T_STORE_PAGE_FAULT:
		handle_user_page_fault(ss, code);
		break;

	case T_ILLEGAL:
		handle_user_illegal(ss);
		break;

	case T_BREAKPOINT:
		handle_user_breakpoint(ss);

	case T_INSN_MISALIGNED:
	case T_LOAD_MISALIGNED:
	case T_STORE_MISALIGNED:
		handle_user_bad_access(ss, EXC_RISCV_DA_ALIGN);

	case T_INSN_ACCESS:
	case T_LOAD_ACCESS:
	case T_STORE_ACCESS:
		handle_user_bad_access(ss, KERN_INVALID_ADDRESS);

	default:
		codes[0] = EXC_RISCV_UNDEFINED;
		codes[1] = ss->scause;
		exception_triage(EXC_BAD_INSTRUCTION, codes, 2);
		__builtin_unreachable();
	}

#if MACH_ASSERT
	if (preemption_level != sleh_get_preemption_level()) {
		panic("synchronous exception changed preemption level from %d to %d", preemption_level, sleh_get_preemption_level());
	}
#endif
}

static void
handle_ecall(riscv_saved_state_t *state)
{
	int64_t  trap_no = (int64_t)get_saved_state_reg(state, RISCV_REG_T0);
	thread_t thread  = current_thread();
	struct   proc *p;

#define handle_ecall_kprintf(x...) /* kprintf("handle_ecall: " x) */

	thread->iotier_override = THROTTLE_LEVEL_NONE; /* Reset IO tier override before handling the syscall */

	if (trap_no == MACH_RISCV_TRAP_PLATFORM) {
		platform_syscall(state);
		panic("Returned from platform_syscall()?");
	}

	current_cached_proc_cred_update();

	if (trap_no < 0) {
		switch (trap_no) {
		case MACH_RISCV_TRAP_ABSTIME:
			handle_mach_absolute_time_trap(state);
			return;
		case MACH_RISCV_TRAP_CONTTIME:
			handle_mach_continuous_time_trap(state);
			return;
		default:
			break;
		}

		/* Counting perhaps better in the handler, but this is how it's been done */
		thread->syscalls_mach++;
		mach_syscall(state);
	} else {
		/* Counting perhaps better in the handler, but this is how it's been done */
		thread->syscalls_unix++;
		p = get_bsdthreadtask_info(thread);

		assert(p);

		unix_syscall(state, thread, p);
	}
}

static void
handle_mach_absolute_time_trap(riscv_saved_state_t *state)
{
	uint64_t now = mach_absolute_time();
	set_saved_state_reg(state, RISCV_REG_A0, now);
}

static void
handle_mach_continuous_time_trap(riscv_saved_state_t *state)
{
	uint64_t now = mach_continuous_time();
	set_saved_state_reg(state, RISCV_REG_A0, now);
}

static void
handle_user_page_fault(riscv_saved_state_t *state, uint64_t code)
{
	thread_t      thread     = current_thread();
	vm_map_t      map        = thread->map;
	vm_offset_t   fault_addr = get_saved_state_stval(state);
	vm_prot_t     fault_type = sleh_fault_type(code);
	kern_return_t result;

	thread->iotier_override = THROTTLE_LEVEL_NONE; /* Reset IO tier override before handling abort from userspace */

	assert(map != kernel_map);

	// harts without hardware a/d updates fault for those, the pmap settles them
	result = riscv_fast_fault(map->pmap, fault_addr, fault_type, true, true);

	if (result != KERN_SUCCESS) {
		/* We have to fault the page in */
		result = vm_fault(map, fault_addr, fault_type,
		    /* change_wiring */ FALSE, VM_KERN_MEMORY_NONE, THREAD_ABORTSAFE,
		    /* caller_pmap */ NULL, /* caller_pmap_addr */ 0);
	}

	if (thread->t_rr_state.trr_fault_state != TRR_FAULT_NONE) {
		thread_reset_pcs_done_faulting(thread);
	}

	if (result == KERN_SUCCESS || result == KERN_ABORTED) {
		return;
	}

	// vm_fault() should never return KERN_FAILURE for user page faults, if it does we're
	// leaking preemption disables somewhere in the kernel.
	if (__improbable(result == KERN_FAILURE)) {
		panic("vm_fault() KERN_FAILURE from user fault on thread %p", thread);
	}

	handle_user_bad_access(state, result);
}

static void
handle_user_bad_access(riscv_saved_state_t *state, mach_exception_data_type_t code)
{
	mach_exception_data_type_t codes[2] = {code, get_saved_state_stval(state)};

	exception_triage(EXC_BAD_ACCESS, codes, 2);
	__builtin_unreachable();
}

static void
handle_kernel_page_fault(riscv_saved_state_t *state, uint64_t code, bool on_thread_stack)
{
	thread_t      thread     = current_thread();
	vm_offset_t   fault_addr = get_saved_state_stval(state);
	vm_prot_t     fault_type = sleh_fault_type(code);
	vm_offset_t   recover    = copyio_recovery_pc(get_saved_state_pc(state));
	kern_return_t result     = KERN_FAILURE;
	vm_map_t      map;
	int           interruptible;

	if (VM_KERNEL_ADDRESS(fault_addr) || thread == THREAD_NULL || recover == 0) {
		// Without a recovery handler always fault against the kernel map, so an unprotected
		// userspace VA access fails in vm_fault() and panics here.
		map = kernel_map;
		interruptible = THREAD_UNINT;
	} else {
		map = thread->map;

		// With a recovery handler set (copyio, dtrace) vm_fault() must not abort early,
		// those paths can't restart it.
		interruptible = (recover) ? THREAD_UNINT : THREAD_ABORTSAFE;
	}

	// before the vm is up there is nothing to fault in, report it without touching kernel_map
	if (__improbable(map == VM_MAP_NULL)) {
		panic_with_thread_kernel_state("Kernel page fault before the VM exists", state);
	}

	/* check to see if it is just a pmap ref/modify fault */
	result = riscv_fast_fault(map->pmap, fault_addr, fault_type, true, false);
	if (result == KERN_SUCCESS) {
		return;
	}

	// vm_fault() may block, so it needs the thread's stack and the interrupts the faulting code had.
	// kernel paging from non-interruptible code (bar early boot) ends in a data abort panic anyway.
	if (__probable((on_thread_stack && saved_state_interrupts_enabled(state)) ||
	    startup_phase < STARTUP_SUB_EARLY_BOOT)) {
		if (result != KERN_PROTECTION_FAILURE) {
			// We have to "fault" the page in. copyio faults are throttled like user faults,
			// the generic flag says we're in one.
			bool const was_recover = (thread != THREAD_NULL) && thread->recover;
			if (thread != THREAD_NULL) {
				thread->recover = was_recover || recover;
			}
			result = vm_fault(map, fault_addr, fault_type,
			    /* change_wiring */ FALSE, VM_KERN_MEMORY_NONE, interruptible,
			    /* caller_pmap */ NULL, /* caller_pmap_addr */ 0);
			if (thread != THREAD_NULL) {
				thread->recover = was_recover;
			}
		}

		if (result == KERN_SUCCESS) {
			return;
		}
	}

	if (recover) {
		// If we have a recover handler, invoke it now.
		thread->machine.recover_scause = state->scause;
		thread->machine.recover_stval = fault_addr;
		set_saved_state_reg(state, RISCV_REG_A0, EFAULT);
		set_saved_state_pc(state, recover);
		return;
	}

	panic_fault_address = fault_addr;
	panic_with_thread_kernel_state("Kernel data abort.", state);
}

// f and d instructions, which trap as illegal while sstatus.FS is off
static bool
riscv_insn_is_fp(uint32_t insn)
{
	uint32_t funct3;

	if ((insn & 3) != 3) {
		// c.fld and c.fsd in quadrant 0, c.fldsp and c.fsdsp in quadrant 2
		funct3 = (insn >> 13) & 7;
		return ((insn & 3) == 0 || (insn & 3) == 2) && (funct3 == 1 || funct3 == 5);
	}

	switch (insn & 0x7f) {
	case 0x07:      /* LOAD-FP */
	case 0x27:      /* STORE-FP */
	case 0x43:      /* FMADD */
	case 0x47:      /* FMSUB */
	case 0x4b:      /* FNMSUB */
	case 0x4f:      /* FNMADD */
	case 0x53:      /* OP-FP */
		return true;
	case 0x73:      /* SYSTEM, a csr access to fflags, frm or fcsr */
		funct3 = (insn >> 12) & 7;
		return funct3 != 0 && funct3 != 4 && (insn >> 20) >= 0x001 && (insn >> 20) <= 0x003;
	default:
		return false;
	}
}

// stval carries the instruction on most harts, otherwise read it at pc
static uint32_t
sleh_user_insn(riscv_saved_state_t *state)
{
	uint16_t half[2] = {0, 0};

	if (get_saved_state_stval(state) != 0) {
		return (uint32_t)get_saved_state_stval(state);
	}
	if (copyin(get_saved_state_pc(state), &half[0], sizeof(half[0])) != 0) {
		return 0;
	}
	if ((half[0] & 3) == 3 &&
	    copyin(get_saved_state_pc(state) + 2, &half[1], sizeof(half[1])) != 0) {
		return 0;
	}
	return (uint32_t)half[0] | ((uint32_t)half[1] << 16);
}

static void
handle_user_illegal(riscv_saved_state_t *state)
{
	thread_t                   thread = current_thread();
	mach_exception_data_type_t codes[2];
	uint32_t                   insn   = sleh_user_insn(state);

	// lazy fp, load the thread's registers on its first fp instruction since it was switched in
	if (thread->machine.ufpcb != NULL && riscv_insn_is_fp(insn)) {
		boolean_t istate = ml_set_interrupts_enabled(FALSE);

		if (fp_state_live() == SSTATUS_FS_OFF) {
			fp_load(thread->machine.ufpcb);
			fp_state_set_live(SSTATUS_FS_CLEAN);
			ml_set_interrupts_enabled(istate);
			return;
		}
		ml_set_interrupts_enabled(istate);
	}

	codes[0] = EXC_RISCV_UNDEFINED;
	codes[1] = insn;
	exception_triage(EXC_BAD_INSTRUCTION, codes, 2);
	__builtin_unreachable();
}

static void
handle_user_breakpoint(riscv_saved_state_t *state)
{
	mach_exception_data_type_t codes[2] = {EXC_RISCV_BREAKPOINT, get_saved_state_pc(state)};

	exception_triage(EXC_BREAKPOINT, codes, 2);
	__builtin_unreachable();
}

#if HAS_TELEMETRY_KERNEL_BRK
static uint32_t bound_chk_violations_event;

static const char *
xnu_soft_trap_handle_breakpoint(
	__unused void     *tstate,
	uint16_t          comment)
{
	if (comment == CLANG_SOFT_TRAP_BOUND_CHK) {
		os_atomic_inc(&bound_chk_violations_event, relaxed);
	}
	return NULL;
}
#endif /* HAS_TELEMETRY_KERNEL_BRK */

static const char *
xnu_hard_trap_handle_breakpoint(void *tstate, uint16_t comment)
{
	kernel_panic_reason_t pr = PERCPU_GET(panic_reason);
	riscv_saved_state_t *state = (riscv_saved_state_t *)tstate;

	switch (comment) {
	case XNU_HARD_TRAP_SAFE_UNLINK:
		snprintf(pr->buf, sizeof(pr->buf),
		    "panic: corrupt list around element %p",
		    (void *)state->x[28]);
		return pr->buf;

	case XNU_HARD_TRAP_STRING_CHK:
		return "panic: string operation caused an overflow";

	case XNU_HARD_TRAP_ASSERT_FAILURE:
		// Implicit assert arguments, ML_TRAP_REGISTER_1..3 are t3, t4, t5.
		panic_assert_format(pr->buf, sizeof(pr->buf),
		    (struct mach_assert_hdr *)state->x[28],
		    state->x[29], state->x[30]);
		return pr->buf;

	default:
		return NULL;
	}
}

KERNEL_BRK_DESCRIPTOR_DEFINE(clang_desc,
    .type                = TRAP_TELEMETRY_TYPE_KERNEL_BRK_CLANG,
    .base                = CLANG_ARM_TRAP_START,
    .max                 = CLANG_ARM_TRAP_END,
    .options             = BRK_TELEMETRY_OPTIONS_FATAL_DEFAULT,
    .handle_breakpoint   = NULL);

KERNEL_BRK_DESCRIPTOR_DEFINE(libcxx_desc,
    .type                = TRAP_TELEMETRY_TYPE_KERNEL_BRK_LIBCXX,
    .base                = LIBCXX_TRAP_START,
    .max                 = LIBCXX_TRAP_END,
    .options             = BRK_TELEMETRY_OPTIONS_FATAL_DEFAULT,
    .handle_breakpoint   = NULL);

#if HAS_TELEMETRY_KERNEL_BRK
KERNEL_BRK_DESCRIPTOR_DEFINE(xnu_soft_traps_desc,
    .type                = TRAP_TELEMETRY_TYPE_KERNEL_BRK_TELEMETRY,
    .base                = XNU_SOFT_TRAP_START,
    .max                 = XNU_SOFT_TRAP_END,
    .options             = BRK_TELEMETRY_OPTIONS_RECOVERABLE_DEFAULT(
	    /* enable_telemetry */ true),
    .handle_breakpoint   = xnu_soft_trap_handle_breakpoint);
#endif /* HAS_TELEMETRY_KERNEL_BRK */

KERNEL_BRK_DESCRIPTOR_DEFINE(xnu_hard_traps_desc,
    .type                = TRAP_TELEMETRY_TYPE_KERNEL_BRK_XNU,
    .base                = XNU_HARD_TRAP_START,
    .max                 = XNU_HARD_TRAP_END,
    .options             = BRK_TELEMETRY_OPTIONS_FATAL_DEFAULT,
    .handle_breakpoint   = xnu_hard_trap_handle_breakpoint);

// the code of a kernel ebreak, from the lui x0 hint that follows a full size ebreak
static uint16_t
sleh_kernel_trap_code(const riscv_saved_state_t *state)
{
	const uint16_t *pc = (const uint16_t *)get_saved_state_pc(state);
	uint32_t insn, hint;

	// kernel text may only be 2 byte aligned, read it in halves
	insn = (uint32_t)pc[0] | ((uint32_t)pc[1] << 16);
	if (insn != RISCV_EBREAK) {
		return 0;
	}
	hint = (uint32_t)pc[2] | ((uint32_t)pc[3] << 16);
	if (!RISCV_IS_TRAP_CODE_HINT(hint)) {
		return 0;
	}
	return (uint16_t)RISCV_TRAP_CODE(hint);
}

// an ebreak with no trap code hint after it is TRAP_DEBUGGER or a kdp breakpoint
static bool
sleh_is_debugger_trap(const riscv_saved_state_t *state)
{
	const uint16_t *pc = (const uint16_t *)get_saved_state_pc(state);

	if (pc[0] == RISCV_C_EBREAK) {
		return true;
	}
	if (((uint32_t)pc[0] | ((uint32_t)pc[1] << 16)) != RISCV_EBREAK) {
		return false;
	}
	return !RISCV_IS_TRAP_CODE_HINT((uint32_t)pc[2] | ((uint32_t)pc[3] << 16));
}

static void
handle_kernel_breakpoint(riscv_saved_state_t *state)
{
	uint16_t comment = sleh_kernel_trap_code(state);

	// hop into the debugger like arm64 does for its gdb trap, kdp_trap steps over the ebreak
	if (sleh_is_debugger_trap(state)) {
		thread_t thread = current_thread();
		boolean_t interrupt_state = ml_set_interrupts_enabled(FALSE);

		if (thread != THREAD_NULL) {
			thread->machine.kpcb = state;
		}
		DebuggerCall(EXC_BREAKPOINT, state);
		if (thread != THREAD_NULL) {
			thread->machine.kpcb = NULL;
		}
		(void) ml_set_interrupts_enabled(interrupt_state);
		return;
	}
	const struct kernel_brk_descriptor *desc;
	const char *msg = NULL;

	desc = find_kernel_brk_descriptor_by_comment(comment);

	if (!desc) {
		goto brk_out;
	}

#if HAS_TELEMETRY_KERNEL_BRK
	if (desc->options.enable_trap_telemetry) {
		trap_telemetry_report_exception(
			/* trap_type   */ desc->type,
			/* trap_code   */ comment,
			/* options     */ desc->options.telemetry_options,
			/* saved_state */ (void *)state);
	}
#endif

	if (desc->handle_breakpoint) {
		msg = desc->handle_breakpoint(state, comment);
	}

#if HAS_TELEMETRY_KERNEL_BRK
	/* Still alive? Check if we should recover. */
	if (desc->options.recoverable) {
		// the lui x0 hint after the ebreak runs as a nop
		add_saved_state_pc(state, 4);
		return;
	}
#endif

brk_out:
	if (msg == NULL) {
		kernel_panic_reason_t pr = PERCPU_GET(panic_reason);

		if (comment == CLANG_ARM_TRAP_BOUND_CHK) {
			msg = tsnprintf(pr->buf, sizeof(pr->buf),
			    "Bounds safety trap");
		} else {
			msg = tsnprintf(pr->buf, sizeof(pr->buf),
			    "Break 0x%04X instruction exception from kernel. "
			    "Panic (by design)",
			    comment);
		}
	}

	panic_with_thread_kernel_state(msg, state);
	__builtin_unreachable();
}

static void
sleh_interrupt(riscv_saved_state_t *state, bool is_user)
{
	cpu_data_t *cdp = sleh_cpu_datap();
	uint64_t    code = SCAUSE_CODE(state->scause);
#if MACH_ASSERT
	int preemption_level = sleh_get_preemption_level();
#endif

	cdp->cpu_int_state = state;

	switch (code) {
	case T_IRQ_TIMER:
		sleh_interrupt_handler_prologue(state, DBG_INTR_TYPE_TIMER);
		cdp->cpu_decrementer = -1; /* Large */
		ml_interrupt_masked_debug_start(rtclock_intr, DBG_INTR_TYPE_TIMER);
		rtclock_intr(is_user);
		ml_interrupt_masked_debug_end();
		sleh_interrupt_handler_epilogue();
		break;

	case T_IRQ_SOFT:
		sleh_interrupt_handler_prologue(state, DBG_INTR_TYPE_IPI);
		// acknowledge first, an ipi sent while the handler runs is taken again
		csr_clear(sip, SIE_SSIE);
		cpu_signal_handler();
		sleh_interrupt_handler_epilogue();
		break;

	case T_IRQ_EXTERNAL:
		sleh_interrupt_handler_prologue(state, DBG_INTR_TYPE_OTHER);
		if (__improbable(cdp->interrupt_handler == NULL)) {
			// a source firmware left enabled, masked until a driver installs a handler on this hart
			csr_clear(sie, SIE_SEIE);
			sleh_interrupt_handler_epilogue();
			break;
		}
		/* Run the registered interrupt handler. */
		cdp->interrupt_handler(cdp->interrupt_target,
		    cdp->interrupt_refCon,
		    cdp->interrupt_nub,
		    cdp->interrupt_source);
		entropy_collect();
		sleh_interrupt_handler_epilogue();
		break;

	default:
		panic_with_thread_kernel_state("Unexpected interrupt", state);
	}

	cdp->cpu_int_state = NULL;

#if MACH_ASSERT
	if (preemption_level != sleh_get_preemption_level()) {
		panic("interrupt %llu changed preemption level from %d to %d", code, preemption_level, sleh_get_preemption_level());
	}
#endif
}

static void
sleh_interrupt_handler_prologue(riscv_saved_state_t *state, unsigned int type)
{
	const bool is_user = saved_state_is_user(state);

	if (is_user == true) {
		/* Sanitize stval (only if the interrupt occurred while the CPU was in usermode) */
		state->stval = 0;
	}

	recount_enter_interrupt();

	task_vtimer_check(current_thread());

	uint64_t pc = is_user ? get_saved_state_pc(state) :
	    VM_KERNEL_UNSLIDE(get_saved_state_pc(state));

	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0) | DBG_FUNC_START,
	    0, pc, is_user, type);
}

static void
sleh_interrupt_handler_epilogue(void)
{
#if KPERF
	kperf_interrupt();
#endif /* KPERF */
	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0) | DBG_FUNC_END);
	recount_leave_interrupt();
}

void
mach_syscall_trace_exit(unsigned int retval,
    unsigned int call_number)
{
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    MACHDBG_CODE(DBG_MACH_EXCP_SC, (call_number)) |
	    DBG_FUNC_END, retval, 0, 0, 0, 0);
}

__attribute__((noreturn))
void
thread_syscall_return(kern_return_t error)
{
	thread_t thread;
	riscv_saved_state_t *state;

	thread = current_thread();
	state = get_user_regs(thread);

	set_saved_state_reg(state, RISCV_REG_A0, (uint64_t)(int64_t)error);

#if MACH_ASSERT
	kern_allocation_name_t
	prior __assert_only = thread_get_kernel_state(thread)->allocation_name;
	assertf(prior == NULL, "thread_set_allocation_name(\"%s\") not cleared", kern_allocation_get_name(prior));
#endif /* MACH_ASSERT */

	if (kdebug_enable) {
		/* Invert syscall number (negative for a mach syscall) */
		mach_syscall_trace_exit(error, (unsigned int)(-(int64_t)get_saved_state_reg(state, RISCV_REG_T0)));
	}

	thread_exception_return();
}

// a new thread's first way out to user space
void
thread_bootstrap_return(void)
{
#if CONFIG_DTRACE
	dtrace_thread_bootstrap();
#endif
	thread_exception_return();
}
