/*
 * Copyright 2002-2011, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SIGNAL_H_
#define _SIGNAL_H_


#include <sys/types.h>


typedef int	sig_atomic_t;
typedef __haiku_uint64 sigset_t;


/* macros defining the standard signal handling behavior */
#define SIG_DFL		((__sighandler_t)0)		/* "default" signal behaviour */
#define SIG_IGN		((__sighandler_t)1)		/* ignore signal */
#define SIG_ERR		((__sighandler_t)-1)	/* an error occurred during signal
											   processing */
#define SIG_HOLD	((__sighandler_t)3)		/* the signal was hold */

/* macros specifying the event notification type (sigevent::sigev_notify) */
#define SIGEV_NONE		0	/* no notification */
#define SIGEV_SIGNAL	1	/* notify via queued signal */
#define SIGEV_THREAD	2	/* notify via function called in new thread */


union sigval {
	int		sival_int;
	void*	sival_ptr;
};

struct sigevent {
	int				sigev_notify;	/* notification type */
	int				sigev_signo;	/* signal number */
	union sigval	sigev_value;	/* user-defined signal value */
	void			(*sigev_notify_function)(union sigval);
									/* notification function in case of
									   SIGEV_THREAD */
	pthread_attr_t*	sigev_notify_attributes;
									/* pthread creation attributes in case of
									   SIGEV_THREAD */
};

typedef struct __siginfo_t {
	int				si_signo;	/* signal number */
	int				si_code;	/* signal code */
	int				si_errno;	/* if non zero, an error number associated with
								   this signal */
	pid_t			si_pid;		/* sending process ID */
	uid_t			si_uid;		/* real user ID of sending process */
	void*			si_addr;	/* address of faulting instruction */
	int				si_status;	/* exit value or signal */
	long			si_band;	/* band event for SIGPOLL */
	union sigval	si_value;	/* signal value */
} siginfo_t;


/* signal handler function types */
typedef void (*__sighandler_t)(int);
typedef void  (*__siginfo_handler_t)(int, siginfo_t*, void*);

#if defined(_DEFAULT_SOURCE)
typedef __sighandler_t	sighandler_t;
	/* GNU-like signal handler typedef */
#endif


/* structure used by sigaction() */
struct sigaction {
	union {
		__sighandler_t		sa_handler;
		__siginfo_handler_t	sa_sigaction;
	};
	sigset_t				sa_mask;
	int						sa_flags;
	void*					sa_userdata;	/* will be passed to the signal
											   handler, BeOS extension */
};

/* values for sa_flags */
#define SA_NOCLDSTOP	0x01
#define SA_NOCLDWAIT	0x02
#define SA_RESETHAND	0x04
#define SA_NODEFER		0x08
#define SA_RESTART		0x10
#define SA_ONSTACK		0x20
#define SA_SIGINFO		0x40
#define SA_NOMASK		SA_NODEFER
#define SA_STACK		SA_ONSTACK
#define SA_ONESHOT		SA_RESETHAND

/* values for ss_flags */
#define SS_ONSTACK		0x1
#define SS_DISABLE		0x2

#define MINSIGSTKSZ		8192
#define SIGSTKSZ		16384

/* for signals using an alternate stack */
typedef struct stack_t {
	void*	ss_sp;
	size_t	ss_size;
	int		ss_flags;
} stack_t;

/* for the 'how' arg of sigprocmask() */
#define SIG_BLOCK		1
#define SIG_UNBLOCK		2
#define SIG_SETMASK		3

/*
 * The list of all defined signals:
 *
 * The numbering of signals for Haiku attempts to maintain
 * some consistency with UN*X conventions so that things
 * like "kill -9" do what you expect.
 */
#define	SIGHUP			1	/* hangup -- tty is gone! */
#define SIGINT			2	/* interrupt */
#define SIGQUIT			3	/* `quit' special character typed in tty  */
#define SIGILL			4	/* illegal instruction */
#define SIGCHLD			5	/* child process exited */
#define SIGABRT			6	/* abort() called, dont' catch */
#define SIGPIPE			7	/* write to a pipe w/no readers */
#define SIGFPE			8	/* floating point exception */
#define SIGKILL			9	/* kill a team (not catchable) */
#define SIGSTOP			10	/* suspend a thread (not catchable) */
#define SIGSEGV			11	/* segmentation violation (read: invalid pointer) */
#define SIGCONT			12	/* continue execution if suspended */
#define SIGTSTP			13	/* `stop' special character typed in tty */
#define SIGALRM			14	/* an alarm has gone off (see alarm()) */
#define SIGTERM			15	/* termination requested */
#define SIGTTIN			16	/* read of tty from bg process */
#define SIGTTOU			17	/* write to tty from bg process */
#define SIGUSR1			18	/* app defined signal 1 */
#define SIGUSR2			19	/* app defined signal 2 */
#define SIGWINCH		20	/* tty window size changed */
#define SIGKILLTHR		21	/* be specific: kill just the thread, not team */
#define SIGTRAP			22	/* Trace/breakpoint trap */
#define SIGPOLL			23	/* Pollable event */
#define SIGPROF			24	/* Profiling timer expired */
#define SIGSYS			25	/* Bad system call */
#define SIGURG			26	/* High bandwidth data is available at socket */
#define SIGVTALRM		27	/* Virtual timer expired */
#define SIGXCPU			28	/* CPU time limit exceeded */
#define SIGXFSZ			29	/* File size limit exceeded */
#define SIGBUS			30	/* access to undefined portion of a memory object */
#define SIGRESERVED1	31	/* reserved for future use */
#define SIGRESERVED2	32	/* reserved for future use */

#define SIGRTMIN		(__signal_get_sigrtmin())
							/* lowest realtime signal number */
#define SIGRTMAX		(__signal_get_sigrtmax())
							/* greatest realtime signal number */

#define __MAX_SIGNO		64	/* greatest possible signal number, can be used (+1)
							   as size of static arrays */
#define NSIG			(__MAX_SIGNO + 1)
							/* BSD extension, size of the sys_siglist table,
							   obsolete */


/* Signal code values appropriate for siginfo_t::si_code: */
/* any signal */
#define SI_USER			0	/* signal sent by user */
#define SI_QUEUE		1	/* signal sent by sigqueue() */
#define SI_TIMER		2	/* signal sent on timer_settime() timeout */
#define SI_ASYNCIO		3	/* signal sent on asynchronous I/O completion */
#define SI_MESGQ		4	/* signal sent on arrival of message on empty
							   message queue */
/* SIGILL */
#define ILL_ILLOPC		10	/* illegal opcode */
#define ILL_ILLOPN		11	/* illegal operand */
#define ILL_ILLADR		12	/* illegal addressing mode */
#define ILL_ILLTRP		13	/* illegal trap */
#define ILL_PRVOPC		14	/* privileged opcode */
#define ILL_PRVREG		15	/* privileged register */
#define ILL_COPROC		16	/* coprocessor error */
#define ILL_BADSTK		17	/* internal stack error */
/* SIGFPE */
#define FPE_INTDIV		20	/* integer division by zero */
#define FPE_INTOVF		21	/* integer overflow */
#define FPE_FLTDIV		22	/* floating-point division by zero */
#define FPE_FLTOVF		23	/* floating-point overflow */
#define FPE_FLTUND		24	/* floating-point underflow */
#define FPE_FLTRES		25	/* floating-point inexact result */
#define FPE_FLTINV		26	/* invalid floating-point operation */
#define FPE_FLTSUB		27	/* subscript out of range */
/* SIGSEGV */
#define SEGV_MAPERR		30	/* address not mapped to object */
#define SEGV_ACCERR		31	/* invalid permissions for mapped object */
/* SIGBUS */
#define BUS_ADRALN		40	/* invalid address alignment */
#define BUS_ADRERR		41	/* nonexistent physical address */
#define BUS_OBJERR		42	/* object-specific hardware error */
/* SIGTRAP */
#define TRAP_BRKPT		50	/* process breakpoint */
#define TRAP_TRACE		51	/* process trace trap. */
/* SIGCHLD */
#define CLD_EXITED		60	/* child exited */
#define CLD_KILLED		61	/* child terminated abnormally without core dump */
#define CLD_DUMPED		62	/* child terminated abnormally with core dump */
#define CLD_TRAPPED		63	/* traced child trapped */
#define CLD_STOPPED		64	/* child stopped */
#define CLD_CONTINUED	65	/* stopped child continued */
/* SIGPOLL */
#define POLL_IN			70	/* input available */
#define POLL_OUT		71	/* output available */
#define POLL_MSG		72	/* input message available */
#define POLL_ERR		73	/* I/O error */
#define POLL_PRI		74	/* high priority input available */
#define POLL_HUP		75	/* device disconnected */


/* the global table of text strings containing descriptions for each signal */
extern const char* const sys_siglist[NSIG];
	/* BSD extension, obsolete, use strsignal() instead */


#ifdef __cplusplus
extern "C" {
#endif


/* signal management (actions and block masks) */
__sighandler_t signal(int _signal, __sighandler_t signalHandler);
int     sigaction(int _signal, const struct sigaction* action,
			struct sigaction* oldAction);
__sighandler_t sigset(int _signal, __sighandler_t signalHandler);
int		sigignore(int _signal);
int		siginterrupt(int _signal, int flag);

int     sigprocmask(int how, const sigset_t* set, sigset_t* oldSet);
int		pthread_sigmask(int how, const sigset_t* set, sigset_t* oldSet);
int		sighold(int _signal);
int		sigrelse(int _signal);

/* sending signals */
int     raise(int _signal);
int     kill(pid_t pid, int _signal);
int		killpg(pid_t processGroupID, int _signal);
int		sigqueue(pid_t pid, int _signal, const union sigval userValue);
int		pthread_kill(pthread_t thread, int _signal);

/* querying and waiting for signals */
int     sigpending(sigset_t* set);
int     sigsuspend(const sigset_t* mask);
int		sigpause(int _signal);
int 	sigwait(const sigset_t* set, int* _signal);
int		sigwaitinfo(const sigset_t* set, siginfo_t* info);
int		sigtimedwait(const sigset_t* set, siginfo_t* info,
           const struct timespec* timeout);

/* setting the per-thread signal stack */
int		sigaltstack(const stack_t* stack, stack_t* oldStack);

/* signal set (sigset_t) manipulation */
int     sigemptyset(sigset_t* set);
int     sigfillset(sigset_t* set);
int 	sigaddset(sigset_t* set, int _signal);
int 	sigdelset(sigset_t* set, int _signal);
int 	sigismember(const sigset_t* set, int _signal);

/* printing signal names */
void	psiginfo(const siginfo_t* info, const char* message);
void	psignal(int _signal, const char* message);

/* implementation private */
int		__signal_get_sigrtmin();
int		__signal_get_sigrtmax();


#ifdef __cplusplus
}
#endif


/* TODO: move this into the documentation!
 * ==================================================
 * !!! SPECIAL NOTES CONCERNING NON-POSIX EXTENSIONS:
 * ==================================================
 *
 * The standard Posix interface for signal handlers is not as useful
 * as it could be. The handler can define only one single argument
 * (the signal number). For example:
 *    void
 *    my_signal_handler(int sig)
 *    {
 *    . . .
 *    }
 *
 *    // install the handler
 *    signal(SIGINT, &my_signal_handler);
 *
 * The sigaction() function allows finer grained control of the signal
 * handling. It also allows an opportunity, via the 'sigaction' struct, to
 * enable additional data to be passed to the handler. For example:
 *    void
 *    my_signal_handler(int sig, char* userData, vregs* regs)
 *    {
 *    . . .
 *    }
 *
 *    struct sigaction sa;
 *    char data_buffer[32];
 *
 *    sa.sa_handler = (__sighandler_t)my_signal_handler;
 *    sigemptyset(&sa.sa_mask);
 *    sa.sa_userdata = userData;
 *
 *    // install the handler
 *    sigaction(SIGINT, &sa, NULL);
 *
 * The two additional arguments available to the signal handler are extensions
 * to the Posix standard. This feature was introduced by the BeOS and retained
 * by Haiku. However, to remain compatible with Posix and ANSI C, the type
 * of the sa_handler field is defined as '__sighandler_t'. This requires the
 * handler to be cast when assigned to the sa_handler field, as in the example
 * above.
 *
 * The 3 arguments that Haiku provides to signal handlers are as follows:
 * 1) The first argument is the (usual) signal number.
 *
 * 2) The second argument is whatever value is put in the sa_userdata field
 *    of the sigaction struct.
 *
 * 3) The third argument is a pointer to a vregs struct (defined below).
 *    The vregs struct contains the contents of the volatile registers at
 *    the time the signal was delivered to your thread. You can change the
 *    fields of the structure. After your signal handler completes, the OS uses
 *    this struct to reload the registers for your thread (privileged registers
 *    are not loaded of course). The vregs struct is of course terribly machine
 *    dependent.
 *    Note that in BeOS the vregs argument was passed by value, not by pointer.
 *    While Haiku retains binary compability with code compiled for BeOS, code
 *    built under Haiku must use the pointer argument.
 */

/*
 * the vregs struct:
 *
 * signal handlers get this as the last argument
 */
typedef struct vregs vregs;
	/* BeOS extension */


/* include architecture specific definitions */
#include __HAIKU_ARCH_HEADER(signal.h)


typedef struct vregs mcontext_t;

typedef struct __ucontext_t {
	struct __ucontext_t*	uc_link;
	sigset_t				uc_sigmask;
	stack_t					uc_stack;
	mcontext_t				uc_mcontext;
} ucontext_t;


#endif /* _SIGNAL_H_ */
