/*
 * Copyright (c) 2007, 2009 Apple Inc. All rights reserved.
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

/*
 * Copyright (c) 2001 Daniel M. Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _XOPEN_SOURCE 600L
#define _DARWIN_C_SOURCE
#include <ucontext.h>
#include <errno.h>

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/param.h>

#include <TargetConditionals.h>

/* This is a macro to capture all the code added in here that is purely to make
 * conformance tests pass and seems to have no functional reason nor is it
 * required by the standard */
#define CONFORMANCE_SPECIFIC_HACK 1

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

#if defined(__x86_64__) || defined(__i386__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <stddef.h>

/* Prototypes */
extern void _ctx_start(ucontext_t *, int argc, ...);

__attribute__((visibility("hidden")))
void
_ctx_done (ucontext_t *ucp)
{
	if (ucp->uc_link == NULL)
		_exit(0);
	else {
		/*
		 * Since this context has finished, don't allow it
		 * to be restarted without being reinitialized (via
		 * setcontext or swapcontext).
		 */
		ucp->uc_mcsize = 0;

		/* Set context to next one in link */
		/* XXX - what to do for error, abort? */
		setcontext((const ucontext_t *)ucp->uc_link);
		__builtin_trap();	/* should never get here */
	}
}

void
makecontext(ucontext_t *ucp, void (*start)(), int argc, ...)
{
	va_list		ap;
	char		*stack_top;
	intptr_t	*argp;
	int		i;

	if (ucp == NULL)
		return;
	else if (ucp->uc_stack.ss_sp == NULL) {
		/*
		 * This should really return -1 with errno set to ENOMEM
		 * or something, but the spec says that makecontext is
		 * a void function.   At least make sure that the context
		 * isn't valid so it can't be used without an error.
		 */
		ucp->uc_mcsize = 0;
	}
	/* XXX - Do we want to sanity check argc? */
	else if ((argc < 0) || (argc > NCARGS)) {
		ucp->uc_mcsize = 0;
	}
	/* Make sure the context is valid. */
	else {
		/*
		 * Arrange the stack as follows:
		 *
		 * Bottom of the stack
		 *
		 *	_ctx_start()	- context start wrapper
		 *	start()		- user start routine
		 * 	arg1            - first argument, aligned(16)
		 *	...
		 *	argn
		 *	ucp		- this context, %rbp/%ebp points here
		 *
		 *	stack top
		 *
		 * When the context is started, control will return to
		 * the context start wrapper which will pop the user
		 * start routine from the top of the stack.  After that,
		 * the top of the stack will be setup with all arguments
		 * necessary for calling the start routine.  When the
		 * start routine returns, the context wrapper then sets
		 * the stack pointer to %rbp/%ebp which was setup to point to
		 * the base of the stack (and where ucp is stored).  It
		 * will then call _ctx_done() to swap in the next context
		 * (uc_link != 0) or exit the program (uc_link == 0).
		 */
		stack_top = (char *)(ucp->uc_stack.ss_sp +
		    ucp->uc_stack.ss_size - sizeof(intptr_t));

		int minargc = argc;
#if defined(__x86_64__)
		/* Give 6 stack slots to _ctx_start */
		if (minargc < 6)
			minargc = 6;
#endif
		/*
		 * Adjust top of stack to allow for 3 pointers (return
		 * address, _ctx_start, and ucp) and argc arguments.
		 * We allow the arguments to be pointers also.  The first
		 * argument to the user function must be properly aligned.
		 */

		stack_top = stack_top - (sizeof(intptr_t) * (1 + minargc));
		stack_top = (char *)((intptr_t)stack_top & ~15);
		stack_top = stack_top - (2 * sizeof(intptr_t));
		argp = (intptr_t *)stack_top;

		/*
		 * Setup the top of the stack with the user start routine
		 * followed by all of its aguments and the pointer to the
		 * ucontext.  We need to leave a spare spot at the top of
		 * the stack because setcontext will move rip/eip to the top
		 * of the stack before returning.
		 */
		*argp = (intptr_t)_ctx_start;  /* overwritten with same value */
		argp++;
		*argp = (intptr_t)start;
		argp++;

		/* Add all the arguments: */
		va_start(ap, argc);
		for (i = 0; i < argc; i++) {
			*argp = va_arg(ap, intptr_t);
			argp++;
		}
		va_end(ap);

#if defined(__x86_64__)
		/* Always provide space for ctx_start to pop the parameter registers */
		for (;argc < minargc; argc++) {
			*argp++ = 0;
		}

		/* Keep stack aligned */
		if (argc & 1) {
			*argp++ = 0;
		}
#endif

		/* The ucontext is placed at the bottom of the stack. */
		*argp = (intptr_t)ucp;

#if CONFORMANCE_SPECIFIC_HACK
		// There is a conformance test which initialized a ucontext A by memcpy-ing
		// a ucontext B that was previously initialized with getcontext.
		// getcontext(B) modified B such that B.uc_mcontext = &B.__mcontext_data;
		// But by doing the memcpy of B to A, A.uc_mcontext = &B.__mcontext_data
		// when that's not necessarily what we want. We therefore have to
		// unfortunately reassign A.uc_mccontext = &A.__mcontext_data even though we
		// don't know if A.__mcontext_data was properly initialized before we use
		// it. This is really because the conformance test doesn't initialize
		// properly with multiple getcontexts and instead copies contexts around.
		ucp->uc_mcontext = (mcontext_t) &ucp->__mcontext_data;
#endif

		/*
		 * Set the machine context to point to the top of the
		 * stack and the program counter to the context start
		 * wrapper.  Note that setcontext() pushes the return
		 * address onto the top of the stack, so allow for this
		 * by adjusting the stack downward 1 slot.  Also set
		 * %r12/%esi to point to the base of the stack where ucp
		 * is stored.
		 */
		mcontext_t mc = ucp->uc_mcontext;
#if defined(__x86_64__)
		/* Use callee-save and match _ctx_start implementation */
		mc->__ss.__r12 = (intptr_t)argp;
		mc->__ss.__rbp = 0;
		mc->__ss.__rsp = (intptr_t)stack_top + sizeof(caddr_t);
		mc->__ss.__rip = (intptr_t)_ctx_start;
#else
		mc->__ss.__esi = (int)argp;
		mc->__ss.__ebp = 0;
		mc->__ss.__esp = (int)stack_top + sizeof(caddr_t);
		mc->__ss.__eip = (int)_ctx_start;
#endif
	}
}

#elif defined(__arm64__)

/*
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
 * From the standard:
 *		The makecontext() function shall modify the context specified by uctx, which
 * 		has been initialized using getcontext(). When this context is resumed using
 * 		swapcontext() or setcontext(), program execution shall continue by calling
 * 		func, passing it the arguments that follow argc in the makecontext() call.
 *
 * 		Before a call is made to makecontext(), the application shall ensure that the
 * 		context being modified has a stack allocated for it. The application shall
 * 		ensure that the value of argc matches the number of arguments of type int
 * 		passed to func; otherwise, the behavior is undefined.
 *
 * makecontext will set up the uc_stack such that when setcontext or swapcontext
 * is called on the ucontext, it will first execute a helper function _ctx_start()
 * which will call the client specified function and then call a second
 * helper _ctx_done() (which will either follow the ctxt specified by uc_link or
 * exit.)
 *
 * void _ctx_start((void *func)(int arg1, ...), ...)
 * void _ctx_done(ucontext_t *uctx);
 *
 * makecontext modifies the uc_stack as specified:
 *
 *  High addresses
 *  __________________ <---- fp in context
 * | arg n-1, arg n  |
 * |    ...          |
 * |   arg1, arg2    |
 * | _______________ | <----- sp in mcontext
 * |                 |
 * |                 |
 * |                 |
 * |                 |
 * |                 |
 * |                 |
 *  Low addresses
 *
 * The mcontext is also modified such that:
 *		- sp points to the end of the arguments on the stack
 *		- fp points to the stack top
 *		- lr points to _ctx_start.
 *		- x19 = uctx
 *		- x20 = user func
 *		Note: It is fine to modify register state since we'll never go back to
 *		the state we getcontext-ed from. We modify callee save registers so that
 *		they are a) actually set by setcontext b) still present when we return
 *		from user_func in _ctx_start
 *
 * The first thing which _ctx_start will do is pop the first 8 arguments off the
 * stack and then branch to user_func. This works because it leaves the
 * remaining arguments after the first 8 from the stack. Once the client
 * function returns in _ctx_start, we'll be back to the current state as
 * specified above in the diagram.
 *
 * We can then set up the stack for calling _ctx_done
 * a) Set sp = fp.
 * b) Move x19 (which is callee save and therefore restored if used by user_func), to x0
 * c) Call _ctx_done()
 */

#include <ptrauth.h>
#include <os/tsd.h>
#include <platform/compat.h>
#include <platform/string.h>
#include <mach/arm/thread_status.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern void _ctx_start(void (*user_func)());

void
_ctx_done(ucontext_t *uctx)
{
	if (uctx->uc_link == NULL) {
		_exit(0);
	} else {
		uctx->uc_mcsize = 0; /* To make sure that this is not called again without reinitializing */
		setcontext((ucontext_t *) uctx->uc_link);
		__builtin_trap();	/* should never get here */
	}
}

#define ALIGN_TO_16_BYTES(addr) (addr & ~0xf)
#define ARM64_REGISTER_ARGS 8

void
makecontext(ucontext_t *uctx, void (*func)(void), int argc, ...)
{
	if (uctx == NULL) {
		return;
	}

	if (uctx->uc_stack.ss_sp == NULL) {
		goto error;
	}

	if (argc < 0 || argc > NCARGS) {
		goto error;
	}

#if CONFORMANCE_SPECIFIC_HACK
	// There is a conformance test which initialized a ucontext A by memcpy-ing
	// a ucontext B that was previously initialized with getcontext.
	// getcontext(B) modified B such that B.uc_mcontext = &B.__mcontext_data;
	// But by doing the memcpy of B to A, A.uc_mcontext = &B.__mcontext_data
	// when that's not necessarily what we want. We therefore have to
	// unfortunately reassign A.uc_mccontext = &A.__mcontext_data even though we
	// don't know if A.__mcontext_data was properly initialized before we use
	// it. This is really because the conformance test doesn't initialize
	// properly with multiple getcontexts and instead copies contexts around.
	uctx->uc_mcontext = (mcontext_t) &uctx->__mcontext_data;
#endif

	bzero(uctx->uc_stack.ss_sp, uctx->uc_stack.ss_size);

	uintptr_t fp = (char *) uctx->uc_stack.ss_sp + uctx->uc_stack.ss_size;
	fp = ALIGN_TO_16_BYTES(fp);

	// All args are set up on the stack. We also make sure that we also have at
	// least 8 args on the stack (and populate with 0 if the input argc < 8).
	// This way _ctx_start will always have 8 args to pop out from the stack
	// before it calls the client function.
	int padded_argc = (argc < ARM64_REGISTER_ARGS) ? ARM64_REGISTER_ARGS : argc;

	uintptr_t sp = fp - (sizeof(int) * padded_argc);
	sp = ALIGN_TO_16_BYTES(sp);

	// Populate the stack with all the args. Per arm64 calling convention ABI, we
	// do not need to pad and make sure that the arguments are aligned in any
	// manner.
	int *current_arg_addr = (int *) sp;

	va_list argv;
	va_start(argv, argc);
	for (int i = 0; i < argc; i++) {
		*current_arg_addr = va_arg(argv, int);
		current_arg_addr++;
	}
	va_end(argv);

	mcontext_t mctx = uctx->uc_mcontext;

#if defined(__arm64e__)
	// The set macros below read from the opaque_flags to decide how to set the
	// fields (including whether to sign them) and so we need to make sure that
	// we require signing always.
	mctx->__ss.__opaque_flags &= ~__DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
#endif

	arm_thread_state64_set_fp(mctx->__ss, fp);
	arm_thread_state64_set_sp(mctx->__ss, sp);
	arm_thread_state64_set_lr_fptr(mctx->__ss, (void *) _ctx_start);

	mctx->__ss.__x[19] = uctx;
	mctx->__ss.__x[20] = _OS_PTR_MUNGE(func);
	return;
error:
	uctx->uc_mcsize = 0;
	return;
}

#endif /* arm64 || x86_64 || i386 */

#else /* TARGET_OS_OSX || TARGET_OS_DRIVERKIT */

void
makecontext(ucontext_t *u, void (*f)(void), int argc, ...)
{
}

#endif /* TARGET_OS_OSX || TARGET_OS_DRIVERKIT */
