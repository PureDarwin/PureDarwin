/* Detect stack overflow (when getrlimit and sigaction or sigvec are available)
   Copyright (C) 1993, 1994, 2006 Free Software Foundation, Inc.
   Jim Avera <jima@netcom.com>, October 1993.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
 */

/* Compiled only when USE_STACKOVF is defined, which itself requires
   getrlimit with the RLIMIT_STACK option, and support for alternate
   signal stacks using either SVR4 or BSD interfaces.

   This should compile on ANY system which supports either sigaltstack()
   or sigstack(), with or without <siginfo.h> or another way to determine
   the fault address.

   There is no completely portable way to determine if a SIGSEGV signal
   indicates a stack overflow.  The fault address can be used to infer
   this.  However, the fault address is passed to the signal handler in
   different ways on various systems.  One of three methods are used to
   determine the fault address:

      1. The siginfo parameter (with siginfo.h, i.e., SVR4).

      2. 4th "addr" parameter (assumed if struct sigcontext is defined,
	 i.e., SunOS 4.x/BSD).

      3. None (if no method is available).  This case just prints a
      message before aborting with a core dump.  That way the user at
      least knows that it *might* be a recursion problem.

   Jim Avera <jima@netcom.com> writes, on Tue, 5 Oct 93 19:27 PDT:

      "I got interested finding out how a program could catch and
      diagnose its own stack overflow, and ended up modifying m4 to do
      this.  Now it prints a nice error message and exits.

      How it works: SIGSEGV is caught using a separate signal stack.  The
      signal handler declares a stack overflow if the fault address is
      near the end of the stack region, or if the maximum VM address
      space limit has been reached.  Otherwise, it returns to re-execute
      the instruction with SIG_DFL set, so that any real bugs cause a
      core dump as usual."

   Jim Avera <jima@netcom.com> writes, on Fri, 24 Jun 94 12:14 PDT:

      "The stack-overflow detection code would still be needed to avoid a
      SIGSEGV abort if swap space was exhausted at the moment the stack
      tried to grow.  This is probably unlikely to occur with the
      explicit nesting limit option of GNU m4."

   Jim Avera <jima@netcom.com> writes, on Wed, 6 Jul 1994 14:41 PDT:

      "When a stack overflow occurs, a SIGSEGV signal is sent, which by
      default aborts the process with a core dump.

      The code in stackovf.c catches SIGSEGV using a separate signal
      stack.  The signal handler determines whether or not the SIGSEGV
      arose from a stack overflow.  If it is a stack overflow, an
      external function is called (which, in m4, prints a message an
      exits).  Otherwise the SIGSEGV represents an m4 bug, and the signal
      is re-raised with SIG_DFL set, which results in an abort and core
      dump in the usual way.  It seems important (to me) that internal m4
      bugs not be reported as user recursion errors, or vice-versa."  */

#include "m4.h"			/* stdlib.h, xmalloc() */

#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#if HAVE_SIGINFO_H
# include <siginfo.h>
#endif

#ifndef SA_RESETHAND
# define SA_RESETHAND 0
#endif
#ifndef SA_SIGINFO
# define SA_SIGINFO 0
#endif

#ifndef SIGSTKSZ
# define SIGSTKSZ 8192
#endif

/* If the trap address is within STACKOVF_DETECT bytes of the calculated
   stack limit, we diagnose a stack overflow.  This must be large enough
   to cover errors in our estimatation of the limit address, and to
   account for the maximum size of local variables (the amount the
   trapping reference might exceed the stack limit).  Also, some machines
   may report an arbitrary address within the same page frame.
   If the value is too large, we might call some other SIGSEGV a stack
   overflow, masking a bug.  */

#ifndef STACKOVF_DETECT
# define STACKOVF_DETECT 16384
#endif

typedef void (*handler_t) (void);

static const char *stackbot;
static const char *stackend;
static const char *arg0;
static handler_t stackovf_handler;

/* The following OS-independent procedure is called from the SIGSEGV
   signal handler.  The signal handler obtains information about the trap
   in an OS-dependent manner, and passes a parameter with the meanings as
   explained below.

   If the OS explicitly identifies a stack overflow trap, either pass
   PARAM_STACKOVF if a stack overflow, or pass PARAM_NOSTACKOVF if not
   (id est, it is a random bounds violation).  Otherwise, if the fault
   address is available, pass the fault address.  Otherwise (if no
   information is available), pass NULL.

   Not given an explicit indication, we compare the fault address with
   the estimated stack limit, and test to see if overall VM space is
   exhausted.

   If a stack overflow is identified, then the external *stackovf_handler
   function is called, which should print an error message and exit.  If
   it is NOT a stack overflow, then we silently abort with a core dump by
   returning to re-raise the SIGSEGV with SIG_DFL set.  If indeterminate,
   then we do not call *stackovf_handler, but instead print an ambiguous
   message and abort with a core dump.  This only occurs on systems which
   provide no information, but is better than nothing.  */

#define PARAM_STACKOVF ((const char *) (1 + STACKOVF_DETECT))
#define PARAM_NOSTACKOVF ((const char *) (2 + STACKOVF_DETECT))

static void
process_sigsegv (int signo, const char *p)
{
  long diff;
  diff = (p - stackend);

#ifdef DEBUG_STKOVF
  {
    char buf[140];

    sprintf (buf, "process_sigsegv: p=%#lx stackend=%#lx diff=%ld bot=%#lx\n",
	     (long) p, (long) stackend, (long) diff, (long) stackbot);
    write (2, buf, strlen (buf));
  }
#endif

  if (p != PARAM_NOSTACKOVF)
    {
      if ((long) sbrk (8192) == (long) -1)
	{

	  /* sbrk failed.  Assume the RLIMIT_VMEM prevents expansion even
	     if the stack limit has not been reached.  */

	  write (2, "VMEM limit exceeded?\n", 21);
	  p = PARAM_STACKOVF;
	}
      if (diff >= -STACKOVF_DETECT && diff <= STACKOVF_DETECT)
	{

	  /* The fault address is "sufficiently close" to the stack lim.  */

	  p = PARAM_STACKOVF;
	}
      if (p == PARAM_STACKOVF)
	{

	  /* We have determined that this is indeed a stack overflow.  */

	  (*stackovf_handler) ();	/* should call exit() */
	}
    }
  if (p == NULL)
    {
      const char *cp;

      cp = "\
Memory bounds violation detected (SIGSEGV).  Either a stack overflow\n\
occurred, or there is a bug in ";
      write (2, cp, strlen (cp));
      write (2, arg0, strlen (arg0));
      cp = ".  Check for possible infinite recursion.\n";
      write (2, cp, strlen (cp));
    }

  /* Return to re-execute the instruction which caused the trap with
     SIGSEGV set to SIG_DFL.  An abort with core dump should occur.  */

  signal (signo, SIG_DFL);
}

#if HAVE_STRUCT_SIGACTION_SA_SIGACTION

/* POSIX.  */

static void
sigsegv_handler (int signo, siginfo_t *ip, void *context)
{
  process_sigsegv
    (signo, (ip != NULL
	     && ip->si_signo == SIGSEGV ? (char *) ip->si_addr : NULL));
}

#elif HAVE_SIGINFO_T

/* SVR4.  */

static void
sigsegv_handler (int signo, siginfo_t *ip)
{
  process_sigsegv
    (signo, (ip != NULL
	     && ip->si_signo == SIGSEGV ? (char *) ip->si_addr : NULL));
}

#elif HAVE_SIGCONTEXT

/* SunOS 4.x (and BSD?).  (not tested) */

static void
sigsegv_handler (int signo, int code, struct sigcontext *scp, char *addr)
{
  process_sigsegv (signo, addr);
}

#else /* not HAVE_SIGCONTEXT */

/* OS provides no information.  */

static void
sigsegv_handler (int signo)
{
  process_sigsegv (signo, NULL);
}

#endif /* not HAVE_SIGCONTEXT */

/* Arrange to trap a stack-overflow and call a specified handler.  The
   call is on a dedicated signal stack.

   argv and envp are as passed to main().

   If a stack overflow is not detected, then the SIGSEGV is re-raised
   with action set to SIG_DFL, causing an abort and coredump in the usual
   way.

   Detection of a stack overflow depends on the trap address being near
   the stack limit address.  The stack limit can not be directly
   determined in a portable way, but we make an estimate based on the
   address of the argv and environment vectors, their contents, and the
   maximum stack size obtained using getrlimit.  */

void
setup_stackovf_trap (char *const *argv, char *const *envp, handler_t handler)
{
  struct rlimit rl;
  rlim_t stack_len;
  int grows_upward;
  register char *const *v;
  register char *p;
#if HAVE_SIGACTION && defined SA_ONSTACK
  struct sigaction act;
#elif HAVE_SIGVEC && defined SV_ONSTACK
  struct sigvec vec;
#else

Error - Do not know how to set up stack-ovf trap handler...

#endif

  arg0 = argv[0];
  stackovf_handler = handler;

  /* Calculate the approximate expected addr for a stack-ovf trap.  */

  if (getrlimit (RLIMIT_STACK, &rl) < 0)
    error (EXIT_FAILURE, errno, "getrlimit");
  stack_len = (rl.rlim_cur < rl.rlim_max ? rl.rlim_cur : rl.rlim_max);
  stackbot = (char *) argv;
  grows_upward = ((char *) &stack_len > stackbot);
  if (grows_upward)
    {

      /* Grows toward increasing addresses.  */

      for (v = argv; (p = (char *) *v) != NULL; v++)
	{
	  if (p < stackbot)
	    stackbot = p;
	}
      if ((char *) envp < stackbot)
	stackbot = (char *) envp;
      for (v = envp; (p = (char *) *v) != NULL; v++)
	{
	  if (p < stackbot)
	    stackbot = p;
	}
      stackend = stackbot + stack_len;
    }
  else
    {

      /* The stack grows "downward" (toward decreasing addresses).  */

      for (v = argv; (p = (char *) *v) != NULL; v++)
	{
	  if (p > stackbot)
	    stackbot = p;
	}
      if ((char *) envp > stackbot)
	stackbot = (char *) envp;
      for (v = envp; (p = (char *) *v) != NULL; v++)
	{
	  if (p > stackbot)
	    stackbot = p;
	}
      stackend = stackbot - stack_len;
    }

  /* Allocate a separate signal-handler stack.  */

#if HAVE_SIGALTSTACK && (HAVE_SIGINFO_T || ! HAVE_SIGSTACK)

  /* Use sigaltstack only if siginfo_t is available, unless there is no
     choice.  */

  {
    stack_t ss;

    ss.ss_size = SIGSTKSZ;
    ss.ss_sp = xmalloc ((unsigned) ss.ss_size);
    ss.ss_flags = 0;
    if (sigaltstack (&ss, NULL) < 0)
      {
	/* Oops - sigaltstack exists but doesn't work.  We can't
	   install the overflow detector, but should gracefully treat
	   it as though sigaltstack doesn't exist.  For example, this
	   happens when compiled with Linux 2.1 headers but run
	   against Linux 2.0 kernel.  */
	free (ss.ss_sp);
	if (errno == ENOSYS)
	  return;
	error (EXIT_FAILURE, errno, "sigaltstack");
      }
  }

#elif HAVE_SIGSTACK

  {
    struct sigstack ss;
    char *stackbuf = xmalloc (2 * SIGSTKSZ);

    ss.ss_sp = stackbuf + SIGSTKSZ;
    ss.ss_onstack = 0;
    if (sigstack (&ss, NULL) < 0)
      {
	/* Oops - sigstack exists but doesn't work.  We can't install
	   the overflow detector, but should gracefully treat it as
	   though sigstack doesn't exist.  For example, this happens
	   when compiled with Linux 2.1 headers but run against Linux
	   2.0 kernel.  */
	free (stackbuf);
	if (errno == ENOSYS)
	  return;
	error (EXIT_FAILURE, errno, "sigstack");
      }
  }

#else /* not HAVE_SIGSTACK */

Error - Do not know how to set up stack-ovf trap handler...

#endif /* not HAVE_SIGSTACK */

  /* Arm the SIGSEGV signal handler.  */

#if HAVE_SIGACTION && defined SA_ONSTACK

  sigaction (SIGSEGV, NULL, &act);
# if HAVE_STRUCT_SIGACTION_SA_SIGACTION
  act.sa_sigaction = sigsegv_handler;
# else /* ! HAVE_STRUCT_SIGACTION_SA_SIGACTION */
  act.sa_handler = (RETSIGTYPE (*) (int)) sigsegv_handler;
# endif /* ! HAVE_STRUCT_SIGACTION_SA_SIGACTION */
  sigemptyset (&act.sa_mask);
  act.sa_flags = (SA_ONSTACK | SA_RESETHAND | SA_SIGINFO);
  if (sigaction (SIGSEGV, &act, NULL) < 0)
    error (EXIT_FAILURE, errno, "sigaction");

#else /* ! HAVE_SIGACTION */

  vec.sv_handler = (RETSIGTYPE (*) (int)) sigsegv_handler;
  vec.sv_mask = 0;
  vec.sv_flags = (SV_ONSTACK | SV_RESETHAND);
  if (sigvec (SIGSEGV, &vec, NULL) < 0)
    error (EXIT_FAILURE, errno, "sigvec");

#endif /* ! HAVE_SIGACTION */

}
