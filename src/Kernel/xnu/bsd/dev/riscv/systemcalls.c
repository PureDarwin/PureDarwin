/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
 */

#include <kern/bits.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/assert.h>
#include <kern/clock.h>
#include <kern/locks.h>
#include <kern/sched_prim.h>
#include <mach/machine/thread_status.h>
#include <mach/thread_act.h>
#include <machine/machine_routines.h>
#include <riscv/thread.h>
#include <riscv/proc_reg.h>
#include <pexpert/pexpert.h>

#include <sys/kernel.h>
#include <sys/kern_debug.h>
#include <sys/vm.h>
#include <sys/proc_internal.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/kdebug.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/kauth.h>
#include <sys/bitstring.h>

#include <security/audit/audit.h>

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif

#if CONFIG_DTRACE
extern int32_t dtrace_systrace_syscall(struct proc *, void *, int *);
extern void dtrace_systrace_syscall_return(unsigned short, int, int *);
#endif  /* CONFIG_DTRACE */

extern void
unix_syscall(riscv_saved_state_t * regs, thread_t thread_act, struct proc * proc);

static int      riscv_get_syscall_args(uthread_t, riscv_saved_state_t *, const struct sysent *);
static void     riscv_prepare_syscall_return(const struct sysent *, riscv_saved_state_t *, uthread_t, int);
static unsigned short riscv_get_syscall_number(riscv_saved_state_t *);
static void     riscv_trace_unix_syscall(int, riscv_saved_state_t *);

// sleh has stepped sepc past the ecall already, the syscall number rides in t0
#define RISCV_ECALL_LEN                 4
#define RISCV_SYSCALL_CODE_REG_NUM      RISCV_REG_T0
#define RISCV_SYSCALL_ARG_REG_COUNT     8

// t0 on return, libsyscall calls cerror when it is set
#define RISCV_SYSCALL_OK                0
#define RISCV_SYSCALL_ERR               1

#if COUNT_SYSCALLS
__XNU_PRIVATE_EXTERN    int             do_count_syscalls = 1;
__XNU_PRIVATE_EXTERN    int             syscalls_log[SYS_MAXSYSCALL];
#endif

#define code_is_kdebug_trace(code) (((code) == SYS_kdebug_trace) ||   \
	                            ((code) == SYS_kdebug_trace64) || \
	                            ((code) == SYS_kdebug_trace_string))

#if CONFIG_DEBUG_SYSCALL_REJECTION
extern int mach_trap_count;
#endif

// Function: unix_syscall, regs is a pointer to the Process Control Block.
void
unix_syscall(
	riscv_saved_state_t * state,
	thread_t thread_act,
	struct proc * proc)
{
	const struct sysent  *callp;
	int             error;
	unsigned short  code, syscode;
	pid_t           pid;
	struct uthread *uthread = get_bsdthread_info(thread_act);

	uthread_reset_proc_refcount(uthread);

	code = riscv_get_syscall_number(state);

#define unix_syscall_kprintf(x...)      /* kprintf("unix_syscall: " x) */

	if (kdebug_enable && !code_is_kdebug_trace(code)) {
		riscv_trace_unix_syscall(code, state);
	}

	syscode = (code < nsysent) ? code : SYS_invalid;
	callp   = &sysent[syscode];

	if (callp->sy_narg != 0) {
		if (riscv_get_syscall_args(uthread, state, callp) != 0) {
			/* Too many arguments, or something failed */
			unix_syscall_kprintf("riscv_get_syscall_args failed.\n");
			callp = &sysent[SYS_invalid];
		}
	}

	uthread->uu_flag |= UT_NOTCANCELPT;
	uthread->syscall_code = code;

	uthread->uu_rval[0] = 0;
	uthread->uu_rval[1] = 0;

	error = 0;

#if COUNT_SYSCALLS
	if (do_count_syscalls > 0) {
		syscalls_log[code]++;
	}
#endif
	pid = proc_pid(proc);

#ifdef CONFIG_IOCOUNT_TRACE
	uthread->uu_iocount = 0;
	uthread->uu_vpindex = 0;
#endif
	unix_syscall_kprintf("code %d (pid %d - %s, tid %lld)\n", code,
	    pid, proc->p_comm, thread_tid(current_thread()));

#if CONFIG_MACF
	if (__improbable(proc_syscall_filter_mask(proc) != NULL && !bitstr_test(proc_syscall_filter_mask(proc), syscode))) {
		error = mac_proc_check_syscall_unix(proc, syscode);
		if (error) {
			goto skip_syscall;
		}
	}
#endif /* CONFIG_MACF */

#if CONFIG_DEBUG_SYSCALL_REJECTION
	unsigned int call_number = mach_trap_count + syscode;
	if (__improbable(uthread->syscall_rejection_mask != NULL &&
	    uthread_syscall_rejection_is_enabled(uthread)) &&
	    !bitmap_test(uthread->syscall_rejection_mask, call_number)) {
		if (debug_syscall_rejection_handle(syscode)) {
			goto skip_syscall;
		}
	}
#endif /* CONFIG_DEBUG_SYSCALL_REJECTION */

	AUDIT_SYSCALL_ENTER(code, proc, uthread);
	error = (*(callp->sy_call))(proc, &uthread->uu_arg[0], &(uthread->uu_rval[0]));
	AUDIT_SYSCALL_EXIT(code, proc, uthread, error);

#if CONFIG_MACF || CONFIG_DEBUG_SYSCALL_REJECTION
skip_syscall:
#endif /* CONFIG_MACF || CONFIG_DEBUG_SYSCALL_REJECTION */

	unix_syscall_kprintf("code %d, error %d, results %x, %x (pid %d - %s, tid %lld)\n", code, error,
	    uthread->uu_rval[0], uthread->uu_rval[1],
	    pid, get_bsdtask_info(current_task()) ? proc->p_comm : "unknown", thread_tid(current_thread()));

#ifdef CONFIG_IOCOUNT_TRACE
	if (uthread->uu_iocount) {
		printf("system call(%d) returned with uu_iocount(%d) != 0",
		    code, uthread->uu_iocount);
	}
#endif
#if CONFIG_DTRACE
	uthread->t_dtrace_errno = error;
#endif /* CONFIG_DTRACE */
#if DEBUG || DEVELOPMENT
	kern_allocation_name_t
	prior __assert_only = thread_set_allocation_name(NULL);
	assertf(prior == NULL, "thread_set_allocation_name(\"%s\") not cleared", kern_allocation_get_name(prior));
#endif /* DEBUG || DEVELOPMENT */

	riscv_prepare_syscall_return(callp, state, uthread, error);

	uthread->uu_flag &= ~UT_NOTCANCELPT;
	uthread->syscall_code = 0;

	if (uthread->uu_lowpri_window) {
		// task is marked low priority I/O and its I/O in this syscall collided with normal I/O,
		// so delay to mitigate its impact on the system
		throttle_lowpri_io(1);
	}
	if (kdebug_enable && !code_is_kdebug_trace(code)) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_END,
		    error, uthread->uu_rval[0], uthread->uu_rval[1], pid);
	}

	uthread_assert_zero_proc_refcount(uthread);
}

void
unix_syscall_return(int error)
{
	thread_t        thread_act;
	struct uthread *uthread;
	struct proc    *proc;
	riscv_saved_state_t *regs;
	unsigned short  code;
	const struct sysent  *callp;

#define unix_syscall_return_kprintf(x...)       /* kprintf("unix_syscall_retur
	                                         * n: " x) */

	thread_act = current_thread();
	proc = current_proc();
	uthread = get_bsdthread_info(thread_act);

	regs = find_user_regs(thread_act);
	code = uthread->syscall_code;
	callp = (code >= nsysent) ? &sysent[SYS_invalid] : &sysent[code];

#if CONFIG_DTRACE
	if (callp->sy_call == dtrace_systrace_syscall) {
		dtrace_systrace_syscall_return( code, error, uthread->uu_rval );
	}
#endif /* CONFIG_DTRACE */
#if DEBUG || DEVELOPMENT
	kern_allocation_name_t
	prior __assert_only = thread_set_allocation_name(NULL);
	assertf(prior == NULL, "thread_set_allocation_name(\"%s\") not cleared", kern_allocation_get_name(prior));
#endif /* DEBUG || DEVELOPMENT */

	AUDIT_SYSCALL_EXIT(code, proc, uthread, error);

	// Get index into sysent table
	riscv_prepare_syscall_return(callp, regs, uthread, error);

	uthread->uu_flag &= ~UT_NOTCANCELPT;
	uthread->syscall_code = 0;

	if (uthread->uu_lowpri_window) {
		// task is marked low priority I/O and its I/O in this syscall collided with normal I/O,
		// so delay to mitigate its impact on the system
		throttle_lowpri_io(1);
	}
	if (kdebug_enable && !code_is_kdebug_trace(code)) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_END,
		    error, uthread->uu_rval[0], uthread->uu_rval[1], proc_getpid(proc));
	}

	thread_exception_return();
	/* NOTREACHED */
}

// All processes are rv64, so no munging. Arguments come from a0-a7 (an indirect syscall
// gives up a0 to the syscall number), whatever does not fit sits at the user sp.
static int
riscv_get_syscall_args(uthread_t uthread, riscv_saved_state_t *regs, const struct sysent *callp)
{
	int indirect_offset, nregs, narg;

	indirect_offset = (regs->x[RISCV_SYSCALL_CODE_REG_NUM] == 0) ? 1 : 0;
	nregs = RISCV_SYSCALL_ARG_REG_COUNT - indirect_offset;
	narg = callp->sy_narg;

	if (narg > (int)(sizeof(uthread->uu_arg) / sizeof(uthread->uu_arg[0]))) {
		return -1;
	}

	memcpy(&uthread->uu_arg[0], &regs->x[RISCV_REG_A0 + indirect_offset],
	    MIN(narg, nregs) * sizeof(uint64_t));

	if (narg > nregs) {
		unix_syscall_kprintf("%s: spillover...\n", __FUNCTION__);
		if (copyin((user_addr_t)get_saved_state_sp(regs), &uthread->uu_arg[nregs],
		    (narg - nregs) * sizeof(uint64_t)) != 0) {
			return -1;
		}
	}

	return 0;
}

static unsigned short
riscv_get_syscall_number(riscv_saved_state_t *state)
{
	if (state->x[RISCV_SYSCALL_CODE_REG_NUM] != 0) {
		return (unsigned short)state->x[RISCV_SYSCALL_CODE_REG_NUM];
	} else {
		return (unsigned short)state->x[RISCV_REG_A0];
	}
}

static void
riscv_prepare_syscall_return(const struct sysent *callp, riscv_saved_state_t *regs, uthread_t uthread, int error)
{
	// ERESTART backs up onto the ecall and keeps t0 and the arguments so it runs again as is
	if (error == ERESTART) {
		add_saved_state_pc(regs, -RISCV_ECALL_LEN);
	} else if (error != EJUSTRETURN) {
		if (error) {
			regs->x[RISCV_REG_A0] = error;
			regs->x[RISCV_REG_A0 + 1] = 0;
			/* set t0 to execute cerror routine */
			regs->x[RISCV_SYSCALL_CODE_REG_NUM] = RISCV_SYSCALL_ERR;
			unix_syscall_return_kprintf("error: setting t0 to trigger cerror call\n");
		} else {        /* (not error) */
			switch (callp->sy_return_type) {
			case _SYSCALL_RET_INT_T:
				regs->x[RISCV_REG_A0] = uthread->uu_rval[0];
				regs->x[RISCV_REG_A0 + 1] = uthread->uu_rval[1];
				break;
			case _SYSCALL_RET_UINT_T:
				regs->x[RISCV_REG_A0] = (u_int)uthread->uu_rval[0];
				regs->x[RISCV_REG_A0 + 1] = (u_int)uthread->uu_rval[1];
				break;
			case _SYSCALL_RET_OFF_T:
			case _SYSCALL_RET_ADDR_T:
			case _SYSCALL_RET_SIZE_T:
			case _SYSCALL_RET_SSIZE_T:
			case _SYSCALL_RET_UINT64_T:
				regs->x[RISCV_REG_A0] = *((uint64_t *)(&uthread->uu_rval[0]));
				regs->x[RISCV_REG_A0 + 1] = 0;
				break;
			case _SYSCALL_RET_NONE:
				break;
			default:
				panic("unix_syscall: unknown return type");
				break;
			}
			regs->x[RISCV_SYSCALL_CODE_REG_NUM] = RISCV_SYSCALL_OK;
		}
	}
	/* else  (error == EJUSTRETURN) { nothing } */
}

static void
riscv_trace_unix_syscall(int code, riscv_saved_state_t *regs)
{
	bool indirect = (regs->x[RISCV_SYSCALL_CODE_REG_NUM] == 0);
	if (indirect) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->x[RISCV_REG_A0 + 1], regs->x[RISCV_REG_A0 + 2],
		    regs->x[RISCV_REG_A0 + 3], regs->x[RISCV_REG_A0 + 4]);
	} else {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->x[RISCV_REG_A0], regs->x[RISCV_REG_A0 + 1],
		    regs->x[RISCV_REG_A0 + 2], regs->x[RISCV_REG_A0 + 3]);
	}
}
