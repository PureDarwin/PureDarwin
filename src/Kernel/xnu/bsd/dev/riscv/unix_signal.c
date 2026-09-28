/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */

#include <mach/mach_types.h>
#include <mach/exception_types.h>

#include <sys/param.h>
#include <sys/proc_internal.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/ucontext.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/ux_exception.h>

#include <riscv/signal.h>
#include <sys/signalvar.h>
#include <sys/kdebug.h>
#include <sys/sdt.h>
#include <sys/wait.h>
#include <kern/thread.h>
#include <mach/riscv/thread_status.h>
#include <sys/reason.h>

#include <kern/assert.h>
#include <kern/ast.h>
#include <pexpert/pexpert.h>
#include <sys/random.h>

extern kern_return_t thread_getstatus(thread_t act, int flavor,
    thread_state_t tstate, mach_msg_type_number_t *count);
extern kern_return_t thread_getstatus_to_user(thread_t act, int flavor,
    thread_state_t tstate, mach_msg_type_number_t *count, thread_set_status_flags_t);
extern kern_return_t machine_thread_state_convert_to_user(thread_t act, int flavor,
    thread_state_t tstate, mach_msg_type_number_t *count, thread_set_status_flags_t);
extern kern_return_t thread_setstatus(thread_t thread, int flavor,
    thread_state_t tstate, mach_msg_type_number_t count);
extern kern_return_t thread_setstatus_from_user(thread_t thread, int flavor,
    thread_state_t tstate, mach_msg_type_number_t count,
    thread_state_t old_tstate, mach_msg_type_number_t old_count,
    thread_set_status_flags_t flags, audit_token_t *audit);
/* XXX Put these someplace smarter... */
typedef struct mcontext64 mcontext64_t;

/* Signal handler flavors supported */
/* These defns should match the libplatform implmn */
#define UC_TRAD                 1
#define UC_FLAVOR               30
#define UC_SET_ALT_STACK        0x40000000
#define UC_RESET_ALT_STACK      0x80000000

/* The following are valid mcontext sizes */
#define UC_FLAVOR_SIZE64 ((RISCV_THREAD_STATE64_COUNT + RISCV_EXCEPTION_STATE64_COUNT + RISCV_FLOAT_STATE64_COUNT) * sizeof(int))

#define TRUNC_TO_16_BYTES(addr) (addr & ~0xf)

// the psabi has no red zone, the frame goes right below the interrupted sp
struct user_sigframe64 {
	/* We can pass the last two args in registers */
	user64_siginfo_t        sinfo;
	struct user_ucontext64  uctx;
	mcontext64_t            mctx;
};

static int
sendsig_get_state64(thread_t th_act, riscv_thread_state64_t *ts, mcontext64_t *mcp)
{
	void *tstate;
	mach_msg_type_number_t state_count;

	assert(proc_is64bit_data(current_proc()));

	tstate = (void *) ts;
	state_count = RISCV_THREAD_STATE64_COUNT;
	if (thread_getstatus(th_act, RISCV_THREAD_STATE64, (thread_state_t) tstate, &state_count) != KERN_SUCCESS) {
		return EINVAL;
	}

	mcp->ss = *ts;
	tstate = (void *) &mcp->ss;
	state_count = RISCV_THREAD_STATE64_COUNT;
	if (machine_thread_state_convert_to_user(th_act, RISCV_THREAD_STATE64, (thread_state_t) tstate,
	    &state_count, TSSF_FLAGS_NONE) != KERN_SUCCESS) {
		return EINVAL;
	}

	tstate = (void *) &mcp->es;
	state_count = RISCV_EXCEPTION_STATE64_COUNT;
	if (thread_getstatus(th_act, RISCV_EXCEPTION_STATE64, (thread_state_t) tstate, &state_count) != KERN_SUCCESS) {
		return EINVAL;
	}

	tstate = (void *) &mcp->fs;
	state_count = RISCV_FLOAT_STATE64_COUNT;
	if (thread_getstatus_to_user(th_act, RISCV_FLOAT_STATE64, (thread_state_t) tstate, &state_count, TSSF_FLAGS_NONE) != KERN_SUCCESS) {
		return EINVAL;
	}

	return 0;
}

static void
sendsig_fill_uctx64(user_ucontext64_t *uctx, int oonstack, int mask, user64_addr_t sp, user64_size_t stack_size, user64_addr_t p_mctx)
{
	bzero(uctx, sizeof(*uctx));
	uctx->uc_onstack = oonstack;
	uctx->uc_sigmask = mask;
	uctx->uc_stack.ss_sp = sp;
	uctx->uc_stack.ss_size = stack_size;
	if (oonstack) {
		uctx->uc_stack.ss_flags |= SS_ONSTACK;
	}
	uctx->uc_link = (user64_addr_t)0;
	uctx->uc_mcsize = (user64_size_t) UC_FLAVOR_SIZE64;
	uctx->uc_mcontext64 = (user64_addr_t) p_mctx;
}

// same entry as arm64, the trampoline gets (catcher, infostyle, sig, sinfo, uctx, token)
// in a0-a5 and calls the handler itself before __sigreturn
static kern_return_t
sendsig_set_thread_state64(riscv_thread_state64_t *regs,
    user64_addr_t catcher, int infostyle, int sig, user64_addr_t p_sinfo,
    user64_addr_t p_uctx, user64_addr_t token, user64_addr_t trampact, user64_addr_t sp, thread_t th_act)
{
	assert(proc_is64bit_data(current_proc()));

	regs->x[RISCV_REG_A0 + 0] = catcher;
	regs->x[RISCV_REG_A0 + 1] = infostyle;
	regs->x[RISCV_REG_A0 + 2] = sig;
	regs->x[RISCV_REG_A0 + 3] = p_sinfo;
	regs->x[RISCV_REG_A0 + 4] = p_uctx;
	regs->x[RISCV_REG_A0 + 5] = token;
	regs->pc = trampact;
	regs->x[RISCV_REG_SP] = sp;

	return thread_setstatus(th_act, RISCV_THREAD_STATE64, (void *)regs, RISCV_THREAD_STATE64_COUNT);
}

#if CONFIG_DTRACE
static void
sendsig_do_dtrace(uthread_t ut, user_siginfo_t *sinfo, int sig, user_addr_t catcher)
{
	bzero((caddr_t)&(ut->t_dtrace_siginfo), sizeof(ut->t_dtrace_siginfo));

	ut->t_dtrace_siginfo.si_signo = sinfo->si_signo;
	ut->t_dtrace_siginfo.si_code = sinfo->si_code;
	ut->t_dtrace_siginfo.si_pid = sinfo->si_pid;
	ut->t_dtrace_siginfo.si_uid = sinfo->si_uid;
	ut->t_dtrace_siginfo.si_status = sinfo->si_status;
	/* XXX truncates faulting address to void *  */
	ut->t_dtrace_siginfo.si_addr = CAST_DOWN_EXPLICIT(void *, sinfo->si_addr);

	/* Fire DTrace proc:::fault probe when signal is generated by hardware. */
	switch (sig) {
	case SIGILL: case SIGBUS: case SIGSEGV: case SIGFPE: case SIGTRAP:
		DTRACE_PROC2(fault, int, (int)(ut->uu_code), siginfo_t *, &(ut->t_dtrace_siginfo));
		break;
	default:
		break;
	}

	/* XXX truncates faulting address to uintptr_t  */
	DTRACE_PROC3(signal__handle, int, sig, siginfo_t *, &(ut->t_dtrace_siginfo),
	    void (*)(void), CAST_DOWN(uintptr_t, catcher));
}
#endif

// Send an interrupt to process.
void
sendsig(
	struct proc * p,
	user_addr_t catcher,
	int sig,
	int mask,
	__unused uint32_t code,
	sigset_t siginfo
	)
{
	riscv_thread_state64_t ts;
	struct user_sigframe64 user_frame;

	user_siginfo_t sinfo;
	user_addr_t     sp = 0, trampact;
	struct sigacts *ps = &p->p_sigacts;
	int             oonstack, infostyle;
	thread_t        th_act;
	struct uthread *ut;
	user_size_t     stack_size = 0;
	user_addr_t     p_uctx, token_uctx;
	user64_addr_t   token;
	kern_return_t   kr;
	int             ret = 0;

	th_act = current_thread();
	ut = get_bsdthread_info(th_act);
	assert(p == current_proc());

	bzero(&ts, sizeof(ts));
	bzero(&user_frame, sizeof(user_frame));

	if (siginfo & sigmask(sig)) {
		infostyle = UC_FLAVOR;
	} else {
		infostyle = UC_TRAD;
	}

	trampact = SIGTRAMP(p, sig);
	oonstack = ut->uu_sigstk.ss_flags & SA_ONSTACK;

	if (ut->uu_pending_sigreturn == 0) {
		/* Generate random token value used to validate sigreturn arguments */
		read_random(&ut->uu_sigreturn_token, sizeof(ut->uu_sigreturn_token));
	}
	ut->uu_pending_sigreturn++;

	// every riscv process is rv64
	if (!proc_is64bit_data(p)) {
		goto bad2;
	}

	// Get sundry thread state.
	if ((ret = sendsig_get_state64(th_act, &ts, &user_frame.mctx)) != 0) {
#if DEVELOPMENT || DEBUG
		printf("process [%s][%d] sendsig_get_state64 failed with ret %d, expected 0", p->p_comm, proc_getpid(p), ret);
#endif
		goto bad2;
	}

	// Figure out where our new stack lives.
	if ((ut->uu_flag & UT_ALTSTACK) && !oonstack &&
	    (ps->ps_sigonstack & sigmask(sig))) {
		sp = ut->uu_sigstk.ss_sp;
		stack_size = ut->uu_sigstk.ss_size;

		sp += stack_size;
		ut->uu_sigstk.ss_flags |= SA_ONSTACK;
	} else {
		// Get the stack pointer and allocate space for the signal handler data.
		sp = CAST_USER_ADDR_T(ts.x[RISCV_REG_SP]);
	}

	/* Make sure to move stack pointer down for room for metadata */
	sp = (sp - sizeof(user_frame));
	sp = TRUNC_TO_16_BYTES(sp);

	proc_unlock(p);

	// Fill in ucontext (points to mcontext, i.e. thread states).
	sendsig_fill_uctx64(&user_frame.uctx, oonstack, mask, sp, (user64_size_t)stack_size,
	    (user64_addr_t)&((struct user_sigframe64*)sp)->mctx);

	// Setup siginfo.
	bzero((caddr_t) &sinfo, sizeof(sinfo));
	sinfo.si_signo = sig;

	sinfo.si_addr = ts.pc;
	sinfo.pad[0] = ts.x[RISCV_REG_SP];

	switch (sig) {
	case SIGILL:
		sinfo.si_code = ILL_ILLTRP;
		break;

	case SIGFPE:
		switch (ut->uu_code) {
		case EXC_RISCV_FP_UF:
			sinfo.si_code = FPE_FLTUND;
			break;
		case EXC_RISCV_FP_OF:
			sinfo.si_code = FPE_FLTOVF;
			break;
		case EXC_RISCV_FP_NV:
			sinfo.si_code = FPE_FLTINV;
			break;
		case EXC_RISCV_FP_DZ:
			sinfo.si_code = FPE_FLTDIV;
			break;
		case EXC_RISCV_FP_NX:
			sinfo.si_code = FPE_FLTRES;
			break;
		default:
			sinfo.si_code = FPE_NOOP;
			break;
		}

		break;

	case SIGBUS:
		sinfo.si_addr = user_frame.mctx.es.stval;
		sinfo.si_code = BUS_ADRALN;
		break;

	case SIGSEGV:
		sinfo.si_addr = user_frame.mctx.es.stval;
		sinfo.si_code = SEGV_ACCERR;
		break;

	default:
	{
		int status_and_exitcode;

		// Other signals fill out a minimal siginfo for SA_SIGINFO handlers. p->si_status holds
		// both status and exit code, saved in its own variable for later breakdown.
		proc_lock(p);
		sinfo.si_pid = p->si_pid;
		p->si_pid = 0;
		status_and_exitcode = p->si_status;
		p->si_status = 0;
		sinfo.si_uid = p->si_uid;
		p->si_uid = 0;
		sinfo.si_code = p->si_code;
		p->si_code = 0;
		proc_unlock(p);
		if (sinfo.si_code == CLD_EXITED) {
			if (WIFEXITED(status_and_exitcode)) {
				sinfo.si_code = CLD_EXITED;
			} else if (WIFSIGNALED(status_and_exitcode)) {
				if (WCOREDUMP(status_and_exitcode)) {
					sinfo.si_code = CLD_DUMPED;
					status_and_exitcode = W_EXITCODE(status_and_exitcode, status_and_exitcode);
				} else {
					sinfo.si_code = CLD_KILLED;
					status_and_exitcode = W_EXITCODE(status_and_exitcode, status_and_exitcode);
				}
			}
		}
		// The recorded status holds exit code and signal info, the handler's siginfo only gets
		// the status, so shift it out.
		sinfo.si_status = (WEXITSTATUS(status_and_exitcode) & 0x00FFFFFF) | (((uint32_t)(p->p_xhighbits) << 24) & 0xFF000000);
		p->p_xhighbits = 0;
		break;
	}
	}

#if CONFIG_DTRACE
	sendsig_do_dtrace(ut, &sinfo, sig, catcher);
#endif /* CONFIG_DTRACE */

	// Copy the signal frame out to user space and set thread state. mctx is filled when we get
	// state, uctx by sendsig_fill_uctx64(), sinfo now.
	siginfo_user_to_user64(&sinfo, &user_frame.sinfo);

	p_uctx = (user_addr_t)&((struct user_sigframe64*)sp)->uctx;
	// Generate the validation token for sigreturn.
	token_uctx = p_uctx;
	kr = machine_thread_siguctx_pointer_convert_to_user(th_act, &token_uctx);
	assert(kr == KERN_SUCCESS);
	token = (user64_addr_t)token_uctx ^ (user64_addr_t)ut->uu_sigreturn_token;

	if ((ret = copyout(&user_frame, sp, sizeof(user_frame))) != 0) {
#if DEVELOPMENT || DEBUG
		printf("process [%s][%d] copyout of user_frame to  (sp, size) = (0x%llx, %zu) failed with ret %d, expected 0\n", p->p_comm, proc_getpid(p), sp, sizeof(user_frame), ret);
#endif
		goto bad;
	}

	if ((kr = sendsig_set_thread_state64(&ts,
	    catcher, infostyle, sig, (user64_addr_t)&((struct user_sigframe64*)sp)->sinfo,
	    (user64_addr_t)p_uctx, token, trampact, sp, th_act)) != KERN_SUCCESS) {
#if DEVELOPMENT || DEBUG
		printf("process [%s][%d] sendsig_set_thread_state64 failed with kr %d, expected 0", p->p_comm, proc_getpid(p), kr);
#endif
		goto bad;
	}

	proc_lock(p);
	return;

bad:
	proc_lock(p);
bad2:
	assert(ut->uu_pending_sigreturn > 0);
	ut->uu_pending_sigreturn--;
	proc_set_sigact(p, SIGILL, SIG_DFL);
	int sigill_mask = sigmask(SIGILL);
	p->p_sigignore &= ~sigill_mask;
	p->p_sigcatch &= ~sigill_mask;
	ut->uu_sigmask &= ~sigill_mask;
	/* sendsig is called with signal lock held */
	proc_unlock(p);

	// Signal delivery failed (e.g. copyout). Attribute the SIGILL crash to this thread where
	// possible, otherwise the report blames the wrong one (rdar://149082274).
	os_reason_t exit_reason = os_reason_create(OS_REASON_SIGNAL, sig);
	psignal_try_thread_with_reason_locked(p, current_thread(), SIGILL, exit_reason /* possibly NULL */);

	proc_lock(p);
}

// sigreturn: restore signal mask and stack state from the context left by sendsig, checking
// the user hasn't modified it to gain privileges.

static int
sigreturn_copyin_ctx64(struct user_ucontext64 *uctx, mcontext64_t *mctx, user_addr_t uctx_addr)
{
	int error;

	assert(proc_is64bit_data(current_proc()));

	error = copyin(uctx_addr, uctx, sizeof(*uctx));
	if (error) {
		return error;
	}

	/* validate the machine context size */
	switch (uctx->uc_mcsize) {
	case UC_FLAVOR_SIZE64:
		break;
	default:
		return EINVAL;
	}

	assert(uctx->uc_mcsize == sizeof(*mctx));
	error = copyin((user_addr_t)uctx->uc_mcontext64, mctx, uctx->uc_mcsize);
	if (error) {
		return error;
	}

	return 0;
}

// the thread state carries no privilege bits, osfmk keeps sstatus of its own
static int
sigreturn_set_state64(thread_t th_act, mcontext64_t *mctx)
{
	assert(proc_is64bit_data(current_proc()));
	audit_token_t *audit = NULL;

	if (thread_setstatus_from_user(th_act, RISCV_THREAD_STATE64, (void *)&mctx->ss,
	    RISCV_THREAD_STATE64_COUNT, NULL, 0, TSSF_FLAGS_NONE, audit) != KERN_SUCCESS) {
		return EINVAL;
	}
	if (thread_setstatus_from_user(th_act, RISCV_FLOAT_STATE64, (void *)&mctx->fs,
	    RISCV_FLOAT_STATE64_COUNT, NULL, 0, TSSF_FLAGS_NONE, audit) != KERN_SUCCESS) {
		return EINVAL;
	}

	return 0;
}

/* ARGSUSED */
int
sigreturn(
	struct proc * p,
	struct sigreturn_args * uap,
	__unused int *retval)
{
	user_ucontext64_t uctx;
	mcontext64_t    mctx;

	struct sigacts *ps = &p->p_sigacts;
	int             error, sigmask = 0, onstack = 0;
	thread_t        th_act;
	struct uthread *ut;
	uint32_t        sigreturn_validation;
	user_addr_t     token_uctx;
	user64_addr_t   token;
	kern_return_t   kr;

	th_act = current_thread();
	ut = (struct uthread *) get_bsdthread_info(th_act);

	/* see osfmk/kern/restartable.c */
	act_set_ast_reset_pcs(TASK_NULL, th_act);

	// Changing the thread's altstack flag only sets/resets it and returns, uap->uctx is unused.
	if ((unsigned int)uap->infostyle == UC_SET_ALT_STACK) {
		ut->uu_sigstk.ss_flags |= SA_ONSTACK;
		return 0;
	} else if ((unsigned int)uap->infostyle == UC_RESET_ALT_STACK) {
		ut->uu_sigstk.ss_flags &= ~SA_ONSTACK;
		return 0;
	}

	if (!proc_is64bit_data(p)) {
		return EINVAL;
	}

	error = sigreturn_copyin_ctx64(&uctx, &mctx, uap->uctx);
	if (error != 0) {
		return error;
	}

	onstack = uctx.uc_onstack;
	sigmask = uctx.uc_sigmask;

	if ((onstack & 01)) {
		ut->uu_sigstk.ss_flags |= SA_ONSTACK;
	} else {
		ut->uu_sigstk.ss_flags &= ~SA_ONSTACK;
	}

	ut->uu_sigmask = sigmask & ~sigcantmask;
	if (ut->uu_siglist & ~ut->uu_sigmask) {
		signal_setast(current_thread());
	}

	sigreturn_validation = atomic_load_explicit(
		&ps->ps_sigreturn_validation, memory_order_relaxed);
	token_uctx = uap->uctx;
	kr = machine_thread_siguctx_pointer_convert_to_user(th_act, &token_uctx);
	assert(kr == KERN_SUCCESS);

	token = (user64_addr_t)token_uctx ^ (user64_addr_t)ut->uu_sigreturn_token;
	if ((user64_addr_t)uap->token != token) {
#if DEVELOPMENT || DEBUG
		printf("process %s[%d] sigreturn token mismatch: received 0x%llx expected 0x%llx\n",
		    p->p_comm, proc_getpid(p), (user64_addr_t)uap->token, token);
#endif /* DEVELOPMENT || DEBUG */
		if (sigreturn_validation != PS_SIGRETURN_VALIDATION_DISABLED) {
			return EINVAL;
		}
	}

	error = sigreturn_set_state64(th_act, &mctx);
	if (error != 0) {
#if DEVELOPMENT || DEBUG
		printf("process %s[%d] sigreturn set_state64 error %d\n",
		    p->p_comm, proc_getpid(p), error);
#endif /* DEVELOPMENT || DEBUG */
		return error;
	}

	/* Decrement the pending sigreturn count */
	if (ut->uu_pending_sigreturn > 0) {
		ut->uu_pending_sigreturn--;
	}

	return EJUSTRETURN;
}

// machine_exception() translates a mach exception to a unix signal.
int
machine_exception(int                           exception,
    __unused mach_exception_code_t         code,
    __unused mach_exception_subcode_t      subcode)
{
	switch (exception) {
	case EXC_BAD_INSTRUCTION:
		return SIGILL;

	case EXC_ARITHMETIC:
		return SIGFPE;
	}

	return 0;
}
