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
#include <TargetConditionals.h>

// int getcontext(ucontext_t *ucp), keeps the caller's sp, fp, ra and the psabi
// callee saved registers so setcontext returns as if getcontext just had

.text

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

.align 2
.globl _getcontext
_getcontext:
	// frame around the helper call so ra, fp and sp come back unchanged
	addi	sp, sp, -16
	sd	ra, 8(sp)
	sd	s0, 0(sp)
	addi	s0, sp, 16

	// a0 = ucp, a1 = the caller's sp
	addi	a1, sp, 16
	call	_populate_signal_stack_context

	ld	ra, 8(sp)
	ld	s0, 0(sp)
	addi	sp, sp, 16

	// a0 = mcontext pointer
	sd	ra, MCONTEXT_OFFSET_X(1)(a0)
	sd	sp, MCONTEXT_OFFSET_X(2)(a0)
	sd	s0, MCONTEXT_OFFSET_X(8)(a0)
	sd	s1, MCONTEXT_OFFSET_X(9)(a0)
	sd	s2, MCONTEXT_OFFSET_X(18)(a0)
	sd	s3, MCONTEXT_OFFSET_X(19)(a0)
	sd	s4, MCONTEXT_OFFSET_X(20)(a0)
	sd	s5, MCONTEXT_OFFSET_X(21)(a0)
	sd	s6, MCONTEXT_OFFSET_X(22)(a0)
	sd	s7, MCONTEXT_OFFSET_X(23)(a0)
	sd	s8, MCONTEXT_OFFSET_X(24)(a0)
	sd	s9, MCONTEXT_OFFSET_X(25)(a0)
	sd	s10, MCONTEXT_OFFSET_X(26)(a0)
	sd	s11, MCONTEXT_OFFSET_X(27)(a0)

	// return value seen when the context is resumed
	sd	zero, MCONTEXT_OFFSET_X(10)(a0)

	fsd	fs0, MCONTEXT_OFFSET_F(8)(a0)
	fsd	fs1, MCONTEXT_OFFSET_F(9)(a0)
	fsd	fs2, MCONTEXT_OFFSET_F(18)(a0)
	fsd	fs3, MCONTEXT_OFFSET_F(19)(a0)
	fsd	fs4, MCONTEXT_OFFSET_F(20)(a0)
	fsd	fs5, MCONTEXT_OFFSET_F(21)(a0)
	fsd	fs6, MCONTEXT_OFFSET_F(22)(a0)
	fsd	fs7, MCONTEXT_OFFSET_F(23)(a0)
	fsd	fs8, MCONTEXT_OFFSET_F(24)(a0)
	fsd	fs9, MCONTEXT_OFFSET_F(25)(a0)
	fsd	fs10, MCONTEXT_OFFSET_F(26)(a0)
	fsd	fs11, MCONTEXT_OFFSET_F(27)(a0)
	frcsr	t0
	sw	t0, MCONTEXT_OFFSET_FCSR(a0)

	li	a0, 0			// return value from getcontext
	ret

#endif
