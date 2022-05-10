/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file defines the standard set of inlines and translators to be made
 * available for all D programs to use to examine process model state.
 */

#pragma D depends_on module procfs

inline int errno = curthread->t_lwp ? curthread->t_lwp->lwp_errno : 0;
#pragma D binding "1.0" errno

/*
 * The following miscellaneous constants are used by the proc(4) translators
 * defined below.  These are assigned the latest values from the system .h's.
 */
inline char SSLEEP = 1;
#pragma D binding "1.0" SSLEEP
inline char SRUN = 2;
#pragma D binding "1.0" SRUN
inline char SZOMB = 3;
#pragma D binding "1.0" SZOMB
inline char SSTOP = 4;
#pragma D binding "1.0" SSTOP
inline char SIDL = 5;
#pragma D binding "1.0" SIDL
inline char SONPROC = 6;
#pragma D binding "1.0" SONPROC

inline int PR_STOPPED = 0x00000001;
#pragma D binding "1.0" PR_STOPPED
inline int PR_ISTOP = 0x00000002;
#pragma D binding "1.0" PR_ISTOP
inline int PR_DSTOP = 0x00000004;
#pragma D binding "1.0" PR_DSTOP
inline int PR_STEP = 0x00000008;
#pragma D binding "1.0" PR_STEP
inline int PR_ASLEEP = 0x00000010;
#pragma D binding "1.0" PR_ASLEEP
inline int PR_PCINVAL = 0x00000020;
#pragma D binding "1.0" PR_PCINVAL
inline int PR_ASLWP = 0x00000040;
#pragma D binding "1.0" PR_ASLWP
inline int PR_AGENT = 0x00000080;
#pragma D binding "1.0" PR_AGENT
inline int PR_DETACH = 0x00000100;
#pragma D binding "1.0" PR_DETACH
inline int PR_DAEMON = 0x00000200;
#pragma D binding "1.0" PR_DAEMON
inline int PR_ISSYS = 0x00001000;
#pragma D binding "1.0" PR_ISSYS
inline int PR_VFORKP = 0x00002000;
#pragma D binding "1.0" PR_VFORKP
inline int PR_ORPHAN = 0x00004000;
#pragma D binding "1.0" PR_ORPHAN
inline int PR_FORK = 0x00100000;
#pragma D binding "1.0" PR_FORK
inline int PR_RLC = 0x00200000;
#pragma D binding "1.0" PR_RLC
inline int PR_KLC = 0x00400000;
#pragma D binding "1.0" PR_KLC
inline int PR_ASYNC = 0x00800000;
#pragma D binding "1.0" PR_ASYNC
inline int PR_MSACCT = 0x01000000;
#pragma D binding "1.0" PR_MSACCT
inline int PR_BPTADJ = 0x02000000;
#pragma D binding "1.0" PR_BPTADJ
inline int PR_PTRACE = 0x04000000;
#pragma D binding "1.0" PR_PTRACE
inline int PR_MSFORK = 0x08000000;
#pragma D binding "1.0" PR_MSFORK
inline int PR_IDLE = 0x10000000;
#pragma D binding "1.0" PR_IDLE

inline char PR_MODEL_ILP32 = 1;
#pragma D binding "1.0" PR_MODEL_ILP32
inline char PR_MODEL_LP64 = 2;
#pragma D binding "1.0" PR_MODEL_LP64

inline char SOBJ_NONE = 0;
#pragma D binding "1.0" SOBJ_NONE
inline char SOBJ_MUTEX = 1;
#pragma D binding "1.0" SOBJ_MUTEX
inline char SOBJ_RWLOCK = 2;
#pragma D binding "1.0" SOBJ_RWLOCK
inline char SOBJ_CV = 3;
#pragma D binding "1.0" SOBJ_CV
inline char SOBJ_SEMA = 4;
#pragma D binding "1.0" SOBJ_SEMA
inline char SOBJ_USER = 5;
#pragma D binding "1.0" SOBJ_USER
inline char SOBJ_USER_PI = 6;
#pragma D binding "1.0" SOBJ_USER_PI
inline char SOBJ_SHUTTLE = 7;
#pragma D binding "1.0" SOBJ_SHUTTLE

inline int SI_USER = 0;
#pragma D binding "1.0" SI_USER
inline int SI_LWP = (-1);
#pragma D binding "1.0" SI_LWP
inline int SI_QUEUE = (-2);
#pragma D binding "1.0" SI_QUEUE
inline int SI_TIMER = (-3);
#pragma D binding "1.0" SI_TIMER
inline int SI_ASYNCIO = (-4);
#pragma D binding "1.0" SI_ASYNCIO
inline int SI_MESGQ = (-5);
#pragma D binding "1.0" SI_MESGQ
inline int SI_RCTL = 2049;
#pragma D binding "1.0" SI_RCTL
inline int ILL_ILLOPC = 1;
#pragma D binding "1.0" ILL_ILLOPC
inline int ILL_ILLOPN = 2;
#pragma D binding "1.0" ILL_ILLOPN
inline int ILL_ILLADR = 3;
#pragma D binding "1.0" ILL_ILLADR
inline int ILL_ILLTRP = 4;
#pragma D binding "1.0" ILL_ILLTRP
inline int ILL_PRVOPC = 5;
#pragma D binding "1.0" ILL_PRVOPC
inline int ILL_PRVREG = 6;
#pragma D binding "1.0" ILL_PRVREG
inline int ILL_COPROC = 7;
#pragma D binding "1.0" ILL_COPROC
inline int ILL_BADSTK = 8;
#pragma D binding "1.0" ILL_BADSTK
inline int FPE_INTDIV = 1;
#pragma D binding "1.0" FPE_INTDIV
inline int FPE_INTOVF = 2;
#pragma D binding "1.0" FPE_INTOVF
inline int FPE_FLTDIV = 3;
#pragma D binding "1.0" FPE_FLTDIV
inline int FPE_FLTOVF = 4;
#pragma D binding "1.0" FPE_FLTOVF
inline int FPE_FLTUND = 5;
#pragma D binding "1.0" FPE_FLTUND
inline int FPE_FLTRES = 6;
#pragma D binding "1.0" FPE_FLTRES
inline int FPE_FLTINV = 7;
#pragma D binding "1.0" FPE_FLTINV
inline int FPE_FLTSUB = 8;
#pragma D binding "1.0" FPE_FLTSUB
inline int SEGV_MAPERR = 1;
#pragma D binding "1.0" SEGV_MAPERR
inline int SEGV_ACCERR = 2;
#pragma D binding "1.0" SEGV_ACCERR
inline int BUS_ADRALN = 1;
#pragma D binding "1.0" BUS_ADRALN
inline int BUS_ADRERR = 2;
#pragma D binding "1.0" BUS_ADRERR
inline int BUS_OBJERR = 3;
#pragma D binding "1.0" BUS_OBJERR
inline int TRAP_BRKPT = 1;
#pragma D binding "1.0" TRAP_BRKPT
inline int TRAP_TRACE = 2;
#pragma D binding "1.0" TRAP_TRACE
inline int CLD_EXITED = 1;
#pragma D binding "1.0" CLD_EXITED
inline int CLD_KILLED = 2;
#pragma D binding "1.0" CLD_KILLED
inline int CLD_DUMPED = 3;
#pragma D binding "1.0" CLD_DUMPED
inline int CLD_TRAPPED = 4;
#pragma D binding "1.0" CLD_TRAPPED
inline int CLD_STOPPED = 5;
#pragma D binding "1.0" CLD_STOPPED
inline int CLD_CONTINUED = 6;
#pragma D binding "1.0" CLD_CONTINUED
inline int POLL_IN = 1;
#pragma D binding "1.0" POLL_IN
inline int POLL_OUT = 2;
#pragma D binding "1.0" POLL_OUT
inline int POLL_MSG = 3;
#pragma D binding "1.0" POLL_MSG
inline int POLL_ERR = 4;
#pragma D binding "1.0" POLL_ERR
inline int POLL_PRI = 5;
#pragma D binding "1.0" POLL_PRI
inline int POLL_HUP = 6;
#pragma D binding "1.0" POLL_HUP

/*
 * Translate from the kernel's proc_t structure to a proc(4) psinfo_t struct.
 * We do not provide support for pr_size, pr_rssize, pr_pctcpu, and pr_pctmem.
 * We also do not fill in pr_lwp (the lwpsinfo_t for the representative LWP)
 * because we do not have the ability to select and stop any representative.
 * Also, for the moment, pr_wstat, pr_time, and pr_ctime are not supported,
 * but these could be supported by DTrace in the future using subroutines.
 * Note that any member added to this translator should also be added to the
 * kthread_t-to-psinfo_t translator, below.
 */
#pragma D binding "1.0" translator
translator psinfo_t < proc_t *T > {
	pr_nlwp = T->p_lwpcnt;
	pr_pid = T->p_pidp->pid_id;
	pr_ppid = T->p_ppid;
	pr_pgid = T->p_pgidp->pid_id;
	pr_sid = T->p_sessp->s_sidp->pid_id;
	pr_uid = T->p_cred->cr_ruid;
	pr_euid = T->p_cred->cr_uid;
	pr_gid = T->p_cred->cr_rgid;
	pr_egid = T->p_cred->cr_gid;
	pr_addr = (uintptr_t)T;

	pr_ttydev = (T->p_sessp->s_vp == NULL) ? (dev_t)-1 :
	    (T->p_sessp->s_dev == `rwsconsdev) ? `uconsdev :
	    (T->p_sessp->s_dev == `rconsdev) ? `uconsdev : T->p_sessp->s_dev;

	pr_start = T->p_user.u_start;
	pr_fname = T->p_user.u_comm;
	pr_psargs = T->p_user.u_psargs;
	pr_argc = T->p_user.u_argc;
	pr_argv = T->p_user.u_argv;
	pr_envp = T->p_user.u_envp;

	pr_dmodel = (T->p_model == 0x00100000) ?
	    PR_MODEL_ILP32 : PR_MODEL_LP64;

	pr_taskid = T->p_task->tk_tkid;
	pr_projid = T->p_task->tk_proj->kpj_id;
	pr_poolid = T->p_pool->pool_id;
	pr_zoneid = T->p_zone->zone_id;
};

/*
 * Translate from the kernel's kthread_t structure to a proc(4) psinfo_t
 * struct.  Lacking a facility to define one translator only in terms of
 * another, we explicitly define each member by using the proc_t-to-psinfo_t
 * translator, above; any members added to that translator should also be
 * added here.  (The only exception to this is pr_start, which -- due to it
 * being a structure -- cannot be defined in terms of a translator at all.)
 */
#pragma D binding "1.0" translator
translator psinfo_t < kthread_t *T > {
	pr_nlwp = xlate <psinfo_t> (T->t_procp).pr_nlwp;
	pr_pid = xlate <psinfo_t> (T->t_procp).pr_pid;
	pr_ppid = xlate <psinfo_t> (T->t_procp).pr_ppid;
	pr_pgid = xlate <psinfo_t> (T->t_procp).pr_pgid;
	pr_sid = xlate <psinfo_t> (T->t_procp).pr_sid;
	pr_uid = xlate <psinfo_t> (T->t_procp).pr_uid;
	pr_euid = xlate <psinfo_t> (T->t_procp).pr_euid;
	pr_gid = xlate <psinfo_t> (T->t_procp).pr_gid;
	pr_egid = xlate <psinfo_t> (T->t_procp).pr_egid;
	pr_addr = xlate <psinfo_t> (T->t_procp).pr_addr;
	pr_ttydev = xlate <psinfo_t> (T->t_procp).pr_ttydev;
	pr_start = (timestruc_t)xlate <psinfo_t> (T->t_procp).pr_start;
	pr_fname = xlate <psinfo_t> (T->t_procp).pr_fname;
	pr_psargs = xlate <psinfo_t> (T->t_procp).pr_psargs;
	pr_argc = xlate <psinfo_t> (T->t_procp).pr_argc;
	pr_argv = xlate <psinfo_t> (T->t_procp).pr_argv;
	pr_envp = xlate <psinfo_t> (T->t_procp).pr_envp;
	pr_dmodel = xlate <psinfo_t> (T->t_procp).pr_dmodel;
	pr_taskid = xlate <psinfo_t> (T->t_procp).pr_taskid;
	pr_projid = xlate <psinfo_t> (T->t_procp).pr_projid;
	pr_poolid = xlate <psinfo_t> (T->t_procp).pr_poolid;
	pr_zoneid = xlate <psinfo_t> (T->t_procp).pr_zoneid;
};

/*
 * Translate from the kernel's kthread_t structure to a proc(4) lwpsinfo_t.
 * We do not provide support for pr_nice, pr_oldpri, pr_cpu, or pr_pctcpu.
 * Also, for the moment, pr_start and pr_time are not supported, but these
 * could be supported by DTrace in the future using subroutines.
 */
#pragma D binding "1.0" translator
translator lwpsinfo_t < kthread_t *T > {
	pr_flag = ((T->t_state == 0x10) ? (PR_STOPPED |
	    ((!(T->t_schedflag & 0x0800)) ? PR_ISTOP : 0)) :
	    ((T->t_proc_flag & 0x0080) ? PR_STOPPED | PR_ISTOP : 0)) |
	    ((T == T->t_procp->p_agenttp) ? PR_AGENT : 0) |
	    ((!(T->t_proc_flag & 0x0004)) ? PR_DETACH : 0) |
	    ((T->t_proc_flag & 0x0001) ? PR_DAEMON : 0) |
	    ((T->t_procp->p_proc_flag & 0x0004) ? PR_FORK : 0) |
	    ((T->t_procp->p_proc_flag & 0x0080) ? PR_RLC : 0) |
	    ((T->t_procp->p_proc_flag & 0x0100) ? PR_KLC : 0) |
	    ((T->t_procp->p_proc_flag & 0x0010) ? PR_ASYNC : 0) |
	    ((T->t_procp->p_proc_flag & 0x0040) ? PR_BPTADJ : 0) |
	    ((T->t_procp->p_proc_flag & 0x0002) ? PR_PTRACE : 0) |
	    ((T->t_procp->p_flag & 0x02000000) ? PR_MSACCT : 0) |
	    ((T->t_procp->p_flag & 0x40000000) ? PR_MSFORK : 0) |
	    ((T->t_procp->p_flag & 0x00080000) ? PR_VFORKP : 0) |
	    (((T->t_procp->p_flag & 0x00000001) ||
	    (T->t_procp->p_as == &`kas)) ? PR_ISSYS : 0) |
	    ((T == T->t_cpu->cpu_idle_thread) ? PR_IDLE : 0);

	pr_lwpid = T->t_tid;
	pr_addr = (uintptr_t)T;
	pr_wchan = (uintptr_t)T->t_lwpchan.lc_wchan;
	pr_stype = T->t_sobj_ops ? T->t_sobj_ops->sobj_type : 0;

	pr_state = (T->t_proc_flag & 0x0080) ? SSTOP :
	    (T->t_state == 0x01) ? SSLEEP :
	    (T->t_state == 0x02) ? SRUN :
	    (T->t_state == 0x04) ? SONPROC :
	    (T->t_state == 0x08) ? SZOMB :
	    (T->t_state == 0x10) ? SSTOP : 0;

	pr_sname = (T->t_proc_flag & 0x0080) ? 'T' :
	    (T->t_state == 0x01) ? 'S' :
	    (T->t_state == 0x02) ? 'R' :
	    (T->t_state == 0x04) ? 'O' :
	    (T->t_state == 0x08) ? 'Z' :
	    (T->t_state == 0x10) ? 'T' : '?';

	pr_syscall = T->t_sysnum;
	pr_pri = T->t_pri;
	pr_clname = `sclass[T->t_cid].cl_name;
	pr_onpro = T->t_cpu->cpu_id;
	pr_bindpro = T->t_bind_cpu;
	pr_bindpset = T->t_bind_pset;
};

inline psinfo_t *curpsinfo = xlate <psinfo_t *> (curthread->t_procp);
#pragma D attributes Stable/Stable/Common curpsinfo
#pragma D binding "1.0" curpsinfo

inline lwpsinfo_t *curlwpsinfo = xlate <lwpsinfo_t *> (curthread);
#pragma D attributes Stable/Stable/Common curlwpsinfo
#pragma D binding "1.0" curlwpsinfo

inline string cwd = curthread->t_procp->p_user.u_cdir->v_path == NULL ?
    "<unknown>" : stringof(curthread->t_procp->p_user.u_cdir->v_path);
#pragma D attributes Stable/Stable/Common cwd
#pragma D binding "1.0" cwd

inline string root = curthread->t_procp->p_user.u_rdir == NULL ? "/" :
    curthread->t_procp->p_user.u_rdir->v_path == NULL ? "<unknown>" :
    stringof(curthread->t_procp->p_user.u_rdir->v_path);
#pragma D attributes Stable/Stable/Common root
#pragma D binding "1.0" root

/*
 * ppid, uid and gid are used frequently enough to merit their own inlines...
 */
inline uid_t ppid = curpsinfo->pr_ppid;
#pragma D attributes Stable/Stable/Common ppid
#pragma D binding "1.0" ppid

inline uid_t uid = curpsinfo->pr_uid;
#pragma D attributes Stable/Stable/Common uid
#pragma D binding "1.0" uid

inline gid_t gid = curpsinfo->pr_gid;
#pragma D attributes Stable/Stable/Common gid
#pragma D binding "1.0" gid
