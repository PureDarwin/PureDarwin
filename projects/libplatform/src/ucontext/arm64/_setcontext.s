/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "asm_help.h"
#include <os/tsd.h>
#include <TargetConditionals.h>
/*
 * void setcontext(ucontext_t *ucp);
 *
 * _STRUCT_UCONTEXT {
 *		int                     uc_onstack;
 *		__darwin_sigset_t       uc_sigmask;     // signal mask used by this context
 *		_STRUCT_SIGALTSTACK     uc_stack;       // stack used by this context
 *		_STRUCT_UCONTEXT        *uc_link;       // pointer to resuming context
 *		__darwin_size_t         uc_mcsize;      // size of the machine context passed in
 *		_STRUCT_MCONTEXT        *uc_mcontext;   // pointer to machine specific context
 * #ifdef _XOPEN_SOURCE
 *		_STRUCT_MCONTEXT        __mcontext_data;
 * #endif
 * };
 *
 * From the standard:
 * 		The setcontext() function shall restore the user context pointed to by
 * 		ucp. A successful call to setcontext() shall not return; program execution
 * 		resumes at the point specified by the ucp argument passed to setcontext().
 * 		The ucp argument should be created either by a prior call to getcontext()
 * 		or makecontext(), or by being passed as an argument to a signal handler.
 * 		If the ucp argument was created with getcontext(), program execution continues
 * 		as if the corresponding call of getcontext() had just returned.
 *
 * setcontext restores the following fields (with the help of a helper function):
 *		uc_sigmask
 *		machine data pointed by uc_mcontext
 *
 * The ASM below mainly handles restoring the machine context data - note that
 * in coordination with getcontext, only the arm64 callee save registers are
 * being restored.
 */

.text

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT
/* Helper macro for authenticating fp, sp and lr and moves the auth-ed values to
 * the right registers
 *
 * Uses x9
 * Modifies input registers, fp, sp and lr
 */
.macro PTR_AUTH_FP_SP_LR fp, sp, lr, flags
#if defined(__arm64e__)
	// Auth sp with constant discriminator
	mov		x9, #52205				// x9 = ptrauth_string_discriminator("sp")
	autda	\sp, x9
	ldr		xzr, [\sp]				// Probe the new stack pointer to catch a corrupt stack
	mov		sp, \sp

	// Auth fp with constant discriminator
	mov		x9, #17687				// x9 = ptrauth_string_discriminator("fp")
	autda	\fp, x9
	mov		fp, \fp

	// Check to see how the lr is signed. If it is signed with B key, nothing to
	// do
	mov		lr, \lr
	tbnz	\flags, LR_SIGNED_WITH_IB_BIT, 2f

	// Auth the input LR per the scheme in the thread state
	mov		x16, \lr
	mov		x17, x16				// x16 = x17 = lr

	mov		x9, #30675				// x9 = ptrauth_string_discriminator("lr")
	autia	x16, x9
	xpaci	x17
	cmp		x16, x17
	b.eq	1f
	brk		#666

1:
	// Auth succeeded - resign the lr with the sp, auth will happen again on
	// return
	mov		lr, x16
	pacibsp
2:
#else
	mov		sp, \sp
	mov		fp, \fp
	mov		lr, \lr
#endif
.endmacro

.private_extern __setcontext
.align 2
__setcontext:
	// x0 = mcontext

	// Restore x19-x28
	ldp		x19, x20, [x0, MCONTEXT_OFFSET_X19_X20]
	ldp		x21, x22, [x0, MCONTEXT_OFFSET_X21_X22]
	ldp		x23, x24, [x0, MCONTEXT_OFFSET_X23_X24]
	ldp		x25, x26, [x0, MCONTEXT_OFFSET_X25_X26]
	ldp		x27, x28, [x0, MCONTEXT_OFFSET_X27_X28]

	// Restore NEON registers
	ldr		d8, [x0, MCONTEXT_OFFSET_D8]
	ldr		d9, [x0, MCONTEXT_OFFSET_D9]
	ldr		d10, [x0, MCONTEXT_OFFSET_D10]
	ldr		d11, [x0, MCONTEXT_OFFSET_D11]
	ldr		d12, [x0, MCONTEXT_OFFSET_D12]
	ldr		d13, [x0, MCONTEXT_OFFSET_D13]
	ldr		d14, [x0, MCONTEXT_OFFSET_D14]
	ldr		d15, [x0, MCONTEXT_OFFSET_D15]

	// Restore sp, fp, lr.
	ldp		x10, x12, [x0, MCONTEXT_OFFSET_FP_LR]
	ldr		x11, [x0, MCONTEXT_OFFSET_SP]
	ldr		w13, [x0, MCONTEXT_OFFSET_FLAGS]

	// x10 = signed fp
	// x11 = signed sp
	// x12 = signed lr
	// x13 = flags

	// Auth the ptrs and move them to the right registers
	PTR_AUTH_FP_SP_LR x10, x11, x12, w13

	// Restore return value
	mov x0, xzr

	ARM64_STACK_EPILOG

#endif /* TARGET_OS_OSX || TARGET_OS_DRIVERKIT */
