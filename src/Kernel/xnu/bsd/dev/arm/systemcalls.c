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
#include <arm/thread.h>
#include <arm64/proc_reg.h>
#include <arm/cpu_capabilities.h>
#include <pexpert/pexpert.h>

#include <sys/csr.h>
#include <sys/kernel.h>
#include <sys/kern_debug.h>
#include <sys/vm.h>
#include <sys/proc_internal.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/kdebug.h>
#include <sys/signalvar.h>
#include <libkern/OSAtomic.h>
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
unix_syscall(struct arm_saved_state * regs, thread_t thread_act, struct proc * proc);

static int      arm_get_syscall_args(uthread_t, struct arm_saved_state *, const struct sysent *);
static int      arm_get_u32_syscall_args(uthread_t, arm_saved_state32_t *, const struct sysent *);
static void     arm_prepare_u32_syscall_return(const struct sysent *, arm_saved_state_t *, uthread_t, int);
static void     arm_prepare_syscall_return(const struct sysent *, struct arm_saved_state *, uthread_t, int);
static unsigned short arm_get_syscall_number(struct arm_saved_state *);
static void     arm_trace_unix_syscall(int, struct arm_saved_state *);
static void     arm_clear_syscall_error(struct arm_saved_state *);
#define save_r0         r[0]
#define save_r1         r[1]
#define save_r2         r[2]
#define save_r3         r[3]
#define save_r4         r[4]
#define save_r5         r[5]
#define save_r6         r[6]
#define save_r7         r[7]
#define save_r8         r[8]
#define save_r9         r[9]
#define save_r10        r[10]
#define save_r11        r[11]
#define save_r12        r[12]
#define save_r13        r[13]

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

// pdflushlog=SECS: SIGTERM logd every SECS so it writes its buffers out (launchd restarts it).
// A VM that is quit rather than shut down otherwise loses the minutes before
extern void IOLog(const char *, ...) __printflike(1, 2);

static int
pd_flush_logd_cb(proc_t p, __unused void *arg)
{
	if (strcmp(proc_best_name(p), "logd") == 0) {
		IOLog("PD-flushlog: SIGTERM logd[%d]\n", proc_pid(p));
		psignal(p, SIGTERM);
	}
	return PROC_RETURNED;
}

static void
pd_flush_logd_check(void)
{
	static int state;	// 0 unparsed, 1 armed, 2 off, 3 flushing
	static uint64_t deadline, period;
	int secs = 0;

	if (state == 2)
		return;
	if (state == 0) {
		state = 2;
		if (PE_parse_boot_argn("pdflushlog", &secs, sizeof(secs)) && secs > 0) {
			nanoseconds_to_absolutetime((uint64_t)secs * NSEC_PER_SEC, &period);
			deadline = mach_absolute_time() + period;
			state = 1;
		}
		return;
	}
	if (state != 1 || mach_absolute_time() < deadline || !OSCompareAndSwap(1, 3, (volatile UInt32 *)&state))
		return;
	proc_iterate(PROC_ALLPROCLIST, pd_flush_logd_cb, NULL, NULL, NULL);
	deadline = mach_absolute_time() + period;
	state = 1;
}

// pdstack=NAME: backtraces of a process's threads, sampled from here (osfmk/arm64/sleh.c)
extern void pd_stack_check(void);

// pdsysctl=NAME logs every sysctl a process named NAME makes and what came back, to diff
// what two machines report: name lookups as name -> oid, reads as oid, length and first bytes
static char pd_sysctl_name[MAXCOMLEN + 1];
static int pd_sysctl_state, pd_sysctl_commpage;

static void
pd_sysctl_log(struct proc *proc, unsigned short code, const uint64_t *args)
{
	int mib[6] = { -1, -1, -1, -1, -1, -1 };
	size_t oldlen = 0, len = 0;
	uint64_t val = 0;
	char name[96] = "";

	if (pd_sysctl_state == 0)
		pd_sysctl_state = PE_parse_boot_argn("pdsysctl", pd_sysctl_name, sizeof(pd_sysctl_name)) ? 2 : 1;
	if (pd_sysctl_state != 2 || (code != SYS_sysctl && code != SYS_sysctlbyname))
		return;
	if (strncmp(proc->p_comm, pd_sysctl_name, strlen(pd_sysctl_name)) != 0)
		return;
	// the commpage once per boot, as the process sees it: cpu counts, clusters and caps live there
	if (!pd_sysctl_commpage) {
		uint64_t words[4];

		pd_sysctl_commpage = 1;
		for (unsigned off = 0; off < 0x200; off += sizeof(words)) {
			if (copyin((user_addr_t)(_COMM_PAGE64_BASE_ADDRESS + off), words, sizeof(words)) != 0)
				break;
			IOLog("PD-sysctl: commpage +%03x %016llx %016llx %016llx %016llx\n", off, words[0], words[1], words[2],
			    words[3]);
		}
	}
	if (code == SYS_sysctlbyname) {
		if (args[3] != 0)
			(void)copyin((user_addr_t)args[3], &oldlen, sizeof(oldlen));
		if (args[2] != 0 && oldlen != 0)
			(void)copyin((user_addr_t)args[2], &val, oldlen < sizeof(val) ? oldlen : sizeof(val));
		(void)copyinstr((user_addr_t)args[0], name, sizeof(name), &len);
		IOLog("PD-sysctl: %s[%d] %s len %lu 0x%llx\n", proc->p_comm, proc_getpid(proc), name, (unsigned long)oldlen,
		    (unsigned long long)val);
		return;
	}
	(void)copyin((user_addr_t)args[0], mib, sizeof(mib));
	if (args[3] != 0)
		(void)copyin((user_addr_t)args[3], &oldlen, sizeof(oldlen));
	if (args[2] != 0 && oldlen != 0)
		(void)copyin((user_addr_t)args[2], &val, oldlen < sizeof(val) ? oldlen : sizeof(val));
	if (mib[0] == 0 && mib[1] == 3) {
		int oid[4] = { -1, -1, -1, -1 };

		(void)copyinstr((user_addr_t)args[4], name, sizeof(name), &len);
		if (args[2] != 0)
			(void)copyin((user_addr_t)args[2], oid, sizeof(oid));
		IOLog("PD-sysctl: %s[%d] %s -> %d.%d.%d.%d\n", proc->p_comm, proc_getpid(proc), name, oid[0], oid[1],
		    oid[2], oid[3]);
		return;
	}
	IOLog("PD-sysctl: %s[%d] %d.%d.%d.%d len %lu 0x%llx\n", proc->p_comm, proc_getpid(proc), mib[0], mib[1],
	    mib[2], mib[3], (unsigned long)oldlen, (unsigned long long)val);
}

// pdsyserr=NAME logs every failing syscall of processes named NAME to the console
static char pd_syserr_name[MAXCOMLEN + 1];
static int pd_syserr_state;

static void
pd_syserr_log(struct proc *proc, unsigned short code, int error, const uint64_t *args)
{
	uint64_t arg0 = args[0];

	if (pd_syserr_state == 0)
		pd_syserr_state = PE_parse_boot_argn("pdsyserr", pd_syserr_name, sizeof(pd_syserr_name)) ? 2 : 1;
	if (pd_syserr_state != 2 || error == EJUSTRETURN || error == ERESTART)
		return;
	// a prefix: p_comm keeps 16 characters (com.apple.fskit.msdos is "com.apple.fskit.")
	if (strncmp(proc->p_comm, pd_syserr_name, strlen(pd_syserr_name)) != 0)
		return;
	// name the path or sysctl mib when the first argument is one
	if (code == SYS_sysctl) {
		int mib[6] = { -1, -1, -1, -1, -1, -1 };
		char name[96] = "";
		size_t len = 0;
		(void)copyin((user_addr_t)arg0, mib, sizeof(mib));
		// 0.3 is name-to-oid, the name being looked up is the new value
		if (mib[0] == 0 && mib[1] == 3)
			(void)copyinstr((user_addr_t)args[4], name, sizeof(name), &len);
		IOLog("PD-syserr: %s[%d] sys %u err %d mib %d.%d.%d.%d %s\n", proc->p_comm, proc_getpid(proc), code, error,
		    mib[0], mib[1], mib[2], mib[3], name);
		return;
	}
	// __mac_syscall: the policy and call number, Sandbox's being what app sandboxing goes through
	if (code == SYS___mac_syscall) {
		char policy[32] = "";
		size_t len = 0;

		(void)copyinstr((user_addr_t)arg0, policy, sizeof(policy), &len);
		IOLog("PD-syserr: %s[%d] sys %u err %d mac_syscall %s call %llu arg 0x%llx\n", proc->p_comm,
		    proc_getpid(proc), code, error, policy, args[1], args[2]);
		return;
	}
	// csrctl: the SIP mask asked about and what the kernel runs with
	if (code == SYS_csrctl) {
		uint32_t mask = 0;
		csr_config_t active = 0;

		(void)copyin((user_addr_t)args[1], &mask, sizeof(mask));
		(void)csr_get_active_config(&active);
		IOLog("PD-syserr: %s[%d] sys %u err %d csr op %llu mask 0x%x active 0x%x\n", proc->p_comm,
		    proc_getpid(proc), code, error, arg0, mask, active);
		return;
	}
	if (code == SYS_open || code == SYS_open_nocancel || code == SYS_stat64 || code == SYS_lstat64 ||
	    code == SYS_statfs64 || code == SYS_getattrlist || code == SYS_unlink || code == SYS_chmod ||
	    code == SYS_chown || code == SYS_access || code == SYS_mkdir || code == SYS_rename) {
		char path[128];
		size_t len = 0;
		if (copyinstr((user_addr_t)arg0, path, sizeof(path), &len) == 0) {
			IOLog("PD-syserr: %s[%d] sys %u err %d path %s\n", proc->p_comm, proc_getpid(proc), code, error, path);
			return;
		}
	}
	IOLog("PD-syserr: %s[%d] sys %u err %d arg0 0x%llx\n", proc->p_comm, proc_getpid(proc), code, error, arg0);
}

/*
 * Function:	unix_syscall
 *
 * Inputs:	regs	- pointer to Process Control Block
 *
 * Outputs:	none
 */
void
unix_syscall(
	struct arm_saved_state * state,
	thread_t thread_act,
	struct proc * proc)
{
	const struct sysent  *callp;
	int             error;
	unsigned short  code, syscode;
	pid_t           pid;
	struct uthread *uthread = get_bsdthread_info(thread_act);

	uthread_reset_proc_refcount(uthread);
	pd_flush_logd_check();
	pd_stack_check();

	code = arm_get_syscall_number(state);

#define unix_syscall_kprintf(x...)      /* kprintf("unix_syscall: " x) */

	if (kdebug_enable && !code_is_kdebug_trace(code)) {
		arm_trace_unix_syscall(code, state);
	}


	syscode = (code < nsysent) ? code : SYS_invalid;
	callp   = &sysent[syscode];

	/*
	 * sy_narg is inaccurate on ARM if a 64 bit parameter is specified. Since user_addr_t
	 * is currently a 32 bit type, this is really a long word count. See rdar://problem/6104668.
	 */
	if (callp->sy_narg != 0) {
		if (arm_get_syscall_args(uthread, state, callp) != 0) {
			/* Too many arguments, or something failed */
			unix_syscall_kprintf("arm_get_syscall_args failed.\n");
			callp = &sysent[SYS_invalid];
		}
	}

	uthread->uu_flag |= UT_NOTCANCELPT;
	uthread->syscall_code = code;

	uthread->uu_rval[0] = 0;

	/*
	 * r4 is volatile, if we set it to regs->save_r4 here the child
	 * will have parents r4 after execve
	 */
	uthread->uu_rval[1] = 0;

	error = 0;

	/*
	 * ARM runtime will call cerror if the carry bit is set after a
	 * system call, so clear it here for the common case of success.
	 */
	arm_clear_syscall_error(state);

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
	if (__improbable(error != 0))
		pd_syserr_log(proc, code, error, (const uint64_t *)&uthread->uu_arg[0]);
	else if (__improbable(code == SYS_sysctl || code == SYS_sysctlbyname))
		pd_sysctl_log(proc, code, (const uint64_t *)&uthread->uu_arg[0]);

#if CONFIG_MACF
skip_syscall:
#endif /* CONFIG_MACF */

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

	arm_prepare_syscall_return(callp, state, uthread, error);

	uthread->uu_flag &= ~UT_NOTCANCELPT;
	uthread->syscall_code = 0;

	if (uthread->uu_lowpri_window) {
		/*
		 * task is marked as a low priority I/O type
		 * and the I/O we issued while in this system call
		 * collided with normal I/O operations... we'll
		 * delay in order to mitigate the impact of this
		 * task on the normal operation of the system
		 */
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
	struct arm_saved_state *regs;
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

	/*
	 * Get index into sysent table
	 */
	arm_prepare_syscall_return(callp, regs, uthread, error);

	uthread->uu_flag &= ~UT_NOTCANCELPT;
	uthread->syscall_code = 0;

	if (uthread->uu_lowpri_window) {
		/*
		 * task is marked as a low priority I/O type
		 * and the I/O we issued while in this system call
		 * collided with normal I/O operations... we'll
		 * delay in order to mitigate the impact of this
		 * task on the normal operation of the system
		 */
		throttle_lowpri_io(1);
	}
	if (kdebug_enable && !code_is_kdebug_trace(code)) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_END,
		    error, uthread->uu_rval[0], uthread->uu_rval[1], proc_getpid(proc));
	}

	thread_exception_return();
	/* NOTREACHED */
}

static void
arm_prepare_u32_syscall_return(const struct sysent *callp, arm_saved_state_t *regs, uthread_t uthread, int error)
{
	assert(is_saved_state32(regs));

	arm_saved_state32_t *ss32 = saved_state32(regs);

	if (error == ERESTART) {
		ss32->pc -= 4;
	} else if (error != EJUSTRETURN) {
		if (error) {
			ss32->save_r0 = error;
			ss32->save_r1 = 0;
			/* set the carry bit to execute cerror routine */
			ss32->cpsr |= PSR_CF;
			unix_syscall_return_kprintf("error: setting carry to trigger cerror call\n");
		} else {        /* (not error) */
			switch (callp->sy_return_type) {
			case _SYSCALL_RET_INT_T:
			case _SYSCALL_RET_UINT_T:
			case _SYSCALL_RET_OFF_T:
			case _SYSCALL_RET_ADDR_T:
			case _SYSCALL_RET_SIZE_T:
			case _SYSCALL_RET_SSIZE_T:
			case _SYSCALL_RET_UINT64_T:
				ss32->save_r0 = uthread->uu_rval[0];
				ss32->save_r1 = uthread->uu_rval[1];
				break;
			case _SYSCALL_RET_NONE:
				ss32->save_r0 = 0;
				ss32->save_r1 = 0;
				break;
			default:
				panic("unix_syscall: unknown return type");
				break;
			}
		}
	}
	/* else  (error == EJUSTRETURN) { nothing } */
}

static void
arm_trace_u32_unix_syscall(int code, arm_saved_state32_t *regs)
{
	bool indirect = (regs->save_r12 == 0);
	if (indirect) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->save_r1, regs->save_r2, regs->save_r3, regs->save_r4);
	} else {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->save_r0, regs->save_r1, regs->save_r2, regs->save_r3);
	}
}

static void
arm_clear_u32_syscall_error(arm_saved_state32_t *regs)
{
	regs->cpsr &= ~PSR_CF;
}

#if defined(__arm64__)
static void arm_prepare_u64_syscall_return(const struct sysent *, arm_saved_state_t *, uthread_t, int);
static int arm_get_u64_syscall_args(uthread_t, arm_saved_state64_t *, const struct sysent *);

static int
arm_get_syscall_args(uthread_t uthread, struct arm_saved_state *state, const struct sysent *callp)
{
	if (is_saved_state32(state)) {
		return arm_get_u32_syscall_args(uthread, saved_state32(state), callp);
	} else {
		return arm_get_u64_syscall_args(uthread, saved_state64(state), callp);
	}
}

/*
 * 64-bit: all arguments in registers.  We're willing to use x9, a temporary
 * register per the ABI, to pass an argument to the kernel for one case,
 * an indirect syscall with 8 arguments.  No munging required, as all arguments
 * are in 64-bit wide registers already.
 */
static int
arm_get_u64_syscall_args(uthread_t uthread, arm_saved_state64_t *regs, const struct sysent *callp)
{
	int indirect_offset;

#if CONFIG_REQUIRES_U32_MUNGING
	sy_munge_t *mungerp;
#endif

	indirect_offset = (regs->x[ARM64_SYSCALL_CODE_REG_NUM] == 0) ? 1 : 0;

	/*
	 * Everything should fit in registers for now.
	 */
	if (callp->sy_narg > (int)(sizeof(uthread->uu_arg) / sizeof(uthread->uu_arg[0]))) {
		return -1;
	}

	memcpy(&uthread->uu_arg[0], &regs->x[indirect_offset], callp->sy_narg * sizeof(uint64_t));

#if CONFIG_REQUIRES_U32_MUNGING
	/*
	 * The indirect system call interface is vararg based.  For armv7k, arm64_32,
	 * and arm64, this means we simply lay the values down on the stack, padded to
	 * a width multiple (4 bytes for armv7k and arm64_32, 8 bytes for arm64).
	 * The arm64(_32) stub for syscall will load this data into the registers and
	 * then trap.  This gives us register state that corresponds to what we would
	 * expect from a armv7 task, so in this particular case we need to munge the
	 * arguments.
	 *
	 * TODO: Is there a cleaner way to do this check?  What we're actually
	 * interested in is whether the task is arm64_32.  We don't appear to guarantee
	 * that uu_proc is populated here, which is why this currently uses the
	 * thread_t.
	 */
	mungerp = callp->sy_arg_munge32;

	if (indirect_offset && !ml_thread_is64bit(get_machthread(uthread))) {
		(*mungerp)(&uthread->uu_arg[0]);
	}
#endif

	return 0;
}
/*
 * When the kernel is running AArch64, munge arguments from 32-bit
 * userland out to 64-bit.
 *
 * flavor == 1 indicates an indirect syscall.
 */
static int
arm_get_u32_syscall_args(uthread_t uthread, arm_saved_state32_t *regs, const struct sysent *callp)
{
	int regparams;
#if CONFIG_REQUIRES_U32_MUNGING
	sy_munge_t *mungerp;
#else
#error U32 syscalls on ARM64 kernel requires munging
#endif
	int flavor = (regs->save_r12 == 0 ? 1 : 0);

	regparams = (7 - flavor); /* Indirect value consumes a register */

	assert((unsigned) callp->sy_arg_bytes <= sizeof(uthread->uu_arg));

	if (callp->sy_arg_bytes <= (sizeof(uint32_t) * regparams)) {
		/*
		 * Seven arguments or less are passed in registers.
		 */
		memcpy(&uthread->uu_arg[0], &regs->r[flavor], callp->sy_arg_bytes);
	} else if (callp->sy_arg_bytes <= sizeof(uthread->uu_arg)) {
		/*
		 * In this case, we composite - take the first args from registers,
		 * the remainder from the stack (offset by the 7 regs therein).
		 */
		unix_syscall_kprintf("%s: spillover...\n", __FUNCTION__);
		memcpy(&uthread->uu_arg[0], &regs->r[flavor], regparams * sizeof(int));
		if (copyin((user_addr_t)regs->sp + 7 * sizeof(int), (int *)&uthread->uu_arg[0] + regparams,
		    (callp->sy_arg_bytes - (sizeof(uint32_t) * regparams))) != 0) {
			return -1;
		}
	} else {
		return -1;
	}

#if CONFIG_REQUIRES_U32_MUNGING
	/* Munge here */
	mungerp = callp->sy_arg_munge32;
	if (mungerp != NULL) {
		(*mungerp)(&uthread->uu_arg[0]);
	}
#endif

	return 0;
}

static unsigned short
arm_get_syscall_number(struct arm_saved_state *state)
{
	if (is_saved_state32(state)) {
		if (saved_state32(state)->save_r12 != 0) {
			return (unsigned short)saved_state32(state)->save_r12;
		} else {
			return (unsigned short)saved_state32(state)->save_r0;
		}
	} else {
		if (saved_state64(state)->x[ARM64_SYSCALL_CODE_REG_NUM] != 0) {
			return (unsigned short)saved_state64(state)->x[ARM64_SYSCALL_CODE_REG_NUM];
		} else {
			return (unsigned short)saved_state64(state)->x[0];
		}
	}
}

static void
arm_prepare_syscall_return(const struct sysent *callp, struct arm_saved_state *state, uthread_t uthread, int error)
{
	if (is_saved_state32(state)) {
		arm_prepare_u32_syscall_return(callp, state, uthread, error);
	} else {
		arm_prepare_u64_syscall_return(callp, state, uthread, error);
	}
}

static void
arm_prepare_u64_syscall_return(const struct sysent *callp, arm_saved_state_t *regs, uthread_t uthread, int error)
{
	assert(is_saved_state64(regs));

	arm_saved_state64_t *ss64 = saved_state64(regs);

	if (error == ERESTART) {
		add_user_saved_state_pc(regs, -4);
	} else if (error != EJUSTRETURN) {
		if (error) {
			ss64->x[0] = error;
			ss64->x[1] = 0;
			/*
			 * Set the carry bit to execute cerror routine.
			 * ARM64_TODO: should we have a separate definition?
			 * The bits are the same.
			 */
			ss64->cpsr |= PSR64_CF;
			unix_syscall_return_kprintf("error: setting carry to trigger cerror call\n");
		} else {        /* (not error) */
			switch (callp->sy_return_type) {
			case _SYSCALL_RET_INT_T:
				ss64->x[0] = uthread->uu_rval[0];
				ss64->x[1] = uthread->uu_rval[1];
				break;
			case _SYSCALL_RET_UINT_T:
				ss64->x[0] = (u_int)uthread->uu_rval[0];
				ss64->x[1] = (u_int)uthread->uu_rval[1];
				break;
			case _SYSCALL_RET_OFF_T:
			case _SYSCALL_RET_ADDR_T:
			case _SYSCALL_RET_SIZE_T:
			case _SYSCALL_RET_SSIZE_T:
			case _SYSCALL_RET_UINT64_T:
				ss64->x[0] = *((uint64_t *)(&uthread->uu_rval[0]));
				ss64->x[1] = 0;
				break;
			case _SYSCALL_RET_NONE:
				break;
			default:
				panic("unix_syscall: unknown return type");
				break;
			}
		}
	}
	/* else  (error == EJUSTRETURN) { nothing } */
}
static void
arm_trace_u64_unix_syscall(int code, arm_saved_state64_t *regs)
{
	bool indirect = (regs->x[ARM64_SYSCALL_CODE_REG_NUM] == 0);
	if (indirect) {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->x[1], regs->x[2], regs->x[3], regs->x[4]);
	} else {
		KDBG_RELEASE(BSDDBG_CODE(DBG_BSD_EXCP_SC, code) | DBG_FUNC_START,
		    regs->x[0], regs->x[1], regs->x[2], regs->x[3]);
	}
}

static void
arm_trace_unix_syscall(int code, struct arm_saved_state *state)
{
	if (is_saved_state32(state)) {
		arm_trace_u32_unix_syscall(code, saved_state32(state));
	} else {
		arm_trace_u64_unix_syscall(code, saved_state64(state));
	}
}

static void
arm_clear_u64_syscall_error(arm_saved_state64_t *regs)
{
	regs->cpsr &= ~PSR64_CF;
}

static void
arm_clear_syscall_error(struct arm_saved_state * state)
{
	if (is_saved_state32(state)) {
		arm_clear_u32_syscall_error(saved_state32(state));
	} else {
		arm_clear_u64_syscall_error(saved_state64(state));
	}
}

#else
#error Unknown architecture.
#endif
