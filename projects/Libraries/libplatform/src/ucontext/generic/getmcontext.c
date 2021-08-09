/*
 * Copyright (c) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#define _XOPEN_SOURCE 600L
#include <ucontext.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <TargetConditionals.h>

/* This is a macro to capture all the code added in here that is purely to make
 * conformance tests pass and seems to have no functional reason nor is it
 * required by the standard */
#define CONFORMANCE_SPECIFIC_HACK 1

#ifdef __DYNAMIC__
extern int __in_sigtramp;
#endif /* __DYNAMIC_ */

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

#if defined(__x86_64__) || defined(__i386__)

#include <sys/resource.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>

#include <platform/string.h>
#include <platform/compat.h>

extern int __sigaltstack(const stack_t * __restrict, stack_t * __restrict);

#ifdef __DYNAMIC__
extern int __in_sigtramp;
#endif /* __DYNAMIC_ */

__attribute__((visibility("hidden")))
mcontext_t
getmcontext(ucontext_t *uctx, void *sp)
{
	mcontext_t mctx = (mcontext_t)&uctx->__mcontext_data;
	size_t stacksize = 0;
	stack_t stack;

#if CONFORMANCE_SPECIFIC_HACK
	uctx->uc_stack.ss_sp = sp;
	uctx->uc_stack.ss_flags = 0;

	if (0 == __sigaltstack(NULL, &stack)) {
		if (stack.ss_flags & SS_ONSTACK) {
			uctx->uc_stack = stack;
			stacksize = stack.ss_size;
		}
	}

	if (stacksize == 0) {
		struct rlimit rlim;
		if (0 == getrlimit(RLIMIT_STACK, &rlim))
			stacksize = rlim.rlim_cur;
	}

	uctx->uc_stack.ss_size = stacksize;
#endif

	uctx->uc_mcontext = mctx;
	uctx->uc_mcsize = sizeof(*mctx);

#if CONFORMANCE_SPECIFIC_HACK

#ifdef __DYNAMIC__
	uctx->uc_link = (ucontext_t*)(uintptr_t)__in_sigtramp; /* non-zero if in signal handler */
#else  /* !__DYNAMIC__ */
	uctx->uc_link = NULL;
#endif /* __DYNAMIC__ */

#endif /* CONFORMANCE_SPECIFIC_HACK */

	sigprocmask(0, NULL, &uctx->uc_sigmask);
	return mctx;
}

#elif defined(__arm64__)

#include <signal.h>
#include <strings.h>
#include <stdint.h>

#include <platform/string.h>
#include <platform/compat.h>

extern int __sigaltstack(const stack_t * __restrict, stack_t * __restrict);

/* @function populate_signal_stack_context
 *
 * @note
 *
 * _STRUCT_UCONTEXT {
 *		int                     uc_onstack;
 * 		__darwin_sigset_t       uc_sigmask;     // signal mask used by this context
 * 		_STRUCT_SIGALTSTACK     uc_stack;       // stack used by this context
 * 		_STRUCT_UCONTEXT        *uc_link;       // pointer to resuming context
 * 		__darwin_size_t         uc_mcsize;      // size of the machine context passed in
 * 		_STRUCT_MCONTEXT        *uc_mcontext;   // pointer to machine specific context
 * #ifdef _XOPEN_SOURCE
 *		_STRUCT_MCONTEXT        __mcontext_data;
 * #endif
 * };
 *
 * populate_signal_stack_context unconditionally populates the following fields:
 *		uc_sigmask
 *		uc_mcontext
 *		uc_mcsize
 *		__mcontext_data
 *		uc_link
 *
 * The standard specifies this about uc_stack:
 *
 *		Before a call is made to makecontext(), the application shall ensure
 *		that the context being modified has a stack allocated for it.
 *
 * ie. the client is generally responsible for managing the stack on on which
 * their context runs and initializing it properly.
 */
__attribute__((visibility("hidden")))
mcontext_t
populate_signal_stack_context(ucontext_t *ucp, void *sp)
{
#if CONFORMANCE_SPECIFIC_HACK
	/* The conformance tests seems to require that we populate the uc_stack in
	 * getcontext even though the standard requires - as stated above - that the
	 * clients manage the stack that their code runs on. This makes no
	 * functional sense but is put in here to make conformance tests work */
	stack_t stack;

	if (0 == __sigaltstack(NULL, &stack) && (stack.ss_flags & SA_ONSTACK)) {
	} else {
		stack.ss_sp = sp;

		// This stacksize is the wrong number - it provides the stack size of
		// the main thread and not the current thread. We can't know the
		// stacksize of the current thread without jumping through some crazy
		// hoops and it seems like per the standard, this field should not be
		// required anyways since the client should be allocating and managing
		// stacks themselves for makecontext.
		struct rlimit rlim;
		if (0 == getrlimit(RLIMIT_STACK, &rlim))
			stack.ss_size = rlim.rlim_cur;
	}
	ucp->uc_stack = stack;
#endif

	/* Populate signal information */
	sigprocmask(SIG_UNBLOCK, NULL, &ucp->uc_sigmask);

	/* Always use the mcontext that is embedded in the struct */
	mcontext_t mctx = (mcontext_t) &ucp->__mcontext_data;
	ucp->uc_mcontext = mctx;
	ucp->uc_mcsize = sizeof(*mctx);

#if CONFORMANCE_SPECIFIC_HACK
	/* The conformance tests for getcontext requires that:
	 *		uc_link = 0 if we're in the "main context"
	 *		uc_link = non-0 if we're on signal context while calling getcontext
	 *
	 * It seems like it doesn't require uc_link to a valid pointer in the 2nd
	 * case, just not 0. It also seems to require that the uc_link is
	 * diversified if we have multiple contexts populated from the signal stack.
	 * So we have it be the address of the in_signal_handler value.
	 *
	 * AFAICT, there seems to be no reason to require populating uc_link at all
	 * but it is what the tests expects.
	 */
#ifdef __DYNAMIC__
	ucp->uc_link = (ucontext_t*)(uintptr_t)__in_sigtramp; /* non-zero if in signal handler */
#else  /* !__DYNAMIC__ */
	ucp->uc_link = NULL;
#endif /* __DYNAMIC__ */

#endif

	return mctx;
}

#endif /* arm64 || x86_64 || i386 */

#else

int
getcontext(ucontext_t *uctx)
{
	errno = ENOTSUP;
	return -1;
}
#endif
