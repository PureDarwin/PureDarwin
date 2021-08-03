/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
 *
 *	File: sys/arm/setjmp.s
 *
 *	Implements sigsetjmp(), setjmp(), _setjmp()
 *
 */

#include <architecture/arm/asm_help.h>
#include "_setjmp.h"

/*
 * setjmp  routines
 */

/*	int sigsetjmp(sigjmp_buf env, int savemask); */

ENTRY_POINT(_sigsetjmp)
	str	r1, [ r0, #JMP_SIGFLAG ]	// save sigflag
	cmp	r1, #0				// test if r1 is 0
	beq	L__exit				// if r1 == 0 do _setjmp()
	// else *** fall through ***  to setjmp()

/*	int setjmp(jmp_buf env); */

ENTRY_POINT(_setjmp)
	str	lr, [ r0, #JMP_lr ]
	str	r8, [ r0, #JMP_r8 ]
	mov	r8, r0				// r8 = jmp_buf

	// Get previous sigmask
	mov	r0, #1				// r0 = SIG_BLOCK
	mov	r1, #0				// r1 = NULL
	add	r2, r8, #JMP_sigmask		// r2 = address to put the sigmask in
	CALL_EXTERNAL(_sigprocmask)		// sigprocmask(SIGBLOCK, NULL, &old_mask);

	// Get altstack status
	sub	sp, sp, #32 // Put a stack_t on the stack
	mov	r0, #0			// r0 = ss = NULL
	mov	r1, sp			// r1 = oss = the place on the stack where stack_t is located
	CALL_EXTERNAL(___sigaltstack) // sigaltstack(NULL, oss)
	ldr r0, [sp, STACK_SSFLAGS] // r0 = ss flags from stack_t
	str	r0, [r8, JMP_sigonstack] // *(r8 + JMP_sigonstack) = r0
	add	sp, sp, #32	// reset sp

	// Do the remaining register stuff
	mov	r0, r8				// restore jmp_buf ptr
	ldr	r8, [ r0,  #JMP_r8 ]
	ldr	lr, [ r0,  #JMP_lr ]
L__exit:
	BRANCH_EXTERNAL(__setjmp)
