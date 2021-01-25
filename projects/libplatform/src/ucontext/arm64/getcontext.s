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
 * int getcontext(ucontext_t *ucp);
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
 * _STRUCT_MCONTEXT64
 * {
 *		_STRUCT_ARM_EXCEPTION_STATE64   __es;
 * 		_STRUCT_ARM_THREAD_STATE64      __ss;
 * 		_STRUCT_ARM_NEON_STATE64        __ns;
 * };
 *
 * From the standard:
 *		The getcontext(3) function shall initialize the structure pointed to by
 *		ucp to the current user context of the calling thread. The ucontext_t
 *		type that ucp points to defines the user context and includes the
 *		contents of the calling thread's machine registers, the signal mask, and
 *		the current execution stack.
 *
 * getcontext populates the following fields (with the help of a helper function):
 *		uc_sigmask
 *		uc_mcontext
 *		uc_mcsize
 *		__mcontext_data
 *		uc_stack
 *
 * The ASM below mainly handles populating the machine context. Per the
 * standard, getcontext should populate the machine context such that if
 * setcontext is called with "ucp argument which was created with getcontext(),
 * program execution continues as if the corresponding call of getcontext() had
 * just returned".
 *
 * As such, the mcontext is saved such that:
 *		- sp and fp are saved to be that of the caller.
 *		- pc is not saved, lr is saved. We'll return from setcontext to the
 *		caller (the current lr) via a ret.
 *		- only callee save registers are saved in the machine context, caller
 *		will restore the caller save registers.
 *		- For neon registers, we save d8-d15. Per the standard:
 *			Registers v8-v15 must be preserved by a callee across subroutine
 *			calls; the remaining registers (v0-v7, v16-v31) do not need to be
 *			preserved (or should be preserved by the caller).  Additionally,
 *			only the bottom 64 bits of each value stored in v8-v15 need to be
 *			preserved; it is the responsibility of the caller to preserve larger
 *			values.
 *		- we don't need to save the arm exception state
 */

.text

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

/* Pointer auths fp, sp and lr and puts them in the final locations specified by
 * input arguments
 *
 * Modifies: lr
 * Uses: x9
 */
.macro PTR_SIGN_FP_SP_LR fp, sp, lr, flags
#if defined(__arm64e__)
	// Sign fp with fp constant discriminator
	mov		\fp, fp
	mov		x9, #17687		// x9 = ptrauth_string_discriminator("fp")
	pacda	\fp, x9

	// Sign sp with sp constant discriminator
	mov		\sp, sp
	mov		x9, #52205		// x9 = ptrauth_string_discriminator("sp")
	pacda	\sp, x9

	// lr is signed with sp and b key, just set a flag marking so and don't
	// change the signature
	mov		\lr, lr
	mov		\flags, LR_SIGNED_WITH_IB
#else
	mov \fp, fp
	mov \sp, sp
	mov \lr, lr
#endif
.endmacro

.align 2
.globl _getcontext
_getcontext:
	ARM64_STACK_PROLOG

	// Note that we're pushing and popping a frame around the subroutine call so
	// that we have the lr, fp, and sp saved
	PUSH_FRAME
	// We don't need to caller save x9 - x15 since we're not going to
	// save them in the mcontext later anyways and since they are caller save
	// registers, the caller of getcontext will restore them if needed.

	// x0 = ucp pointer
	// x1 = sp
	mov		x1, sp
	bl	_populate_signal_stack_context
	POP_FRAME // Restore lr, fp and sp

	// x0 = mcontext pointer

	// Pointer sign fp, sp, lr and mark flags as needed
	PTR_SIGN_FP_SP_LR x10, x11, x12, x13

	// x10 = signed fp
	// x11 = signed sp
	// x12 = signed lr
	// x13 = mcontext flags

	// Save frame pointer and lr
	stp		x10, x12, [x0, MCONTEXT_OFFSET_FP_LR]

	// Save stack pointer
	str		x11, [x0, MCONTEXT_OFFSET_SP]

#if defined(__arm64e__)
	// Save the flags
	str		w13, [x0, MCONTEXT_OFFSET_FLAGS]
#endif

	// Save x19 - x28
	stp		x19, x20, [x0, MCONTEXT_OFFSET_X19_X20]
	stp		x21, x22, [x0, MCONTEXT_OFFSET_X21_X22]
	stp		x23, x24, [x0, MCONTEXT_OFFSET_X23_X24]
	stp		x25, x26, [x0, MCONTEXT_OFFSET_X25_X26]
	stp		x27, x28, [x0, MCONTEXT_OFFSET_X27_X28]

	// Save return value
	str		 xzr, [x0, MCONTEXT_OFFSET_X0]

	// Save NEON registers
	str		d8, [x0, MCONTEXT_OFFSET_D8]
	str		d9, [x0, MCONTEXT_OFFSET_D9]
	str		d10, [x0, MCONTEXT_OFFSET_D10]
	str		d11, [x0, MCONTEXT_OFFSET_D11]
	str		d12, [x0, MCONTEXT_OFFSET_D12]
	str		d13, [x0, MCONTEXT_OFFSET_D13]
	str		d14, [x0, MCONTEXT_OFFSET_D14]
	str		d15, [x0, MCONTEXT_OFFSET_D15]

	mov x0, xzr /* Return value from getcontext */

	ARM64_STACK_EPILOG

#endif
