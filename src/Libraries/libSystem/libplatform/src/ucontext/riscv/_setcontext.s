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

// int _setcontext(const mcontext_t mctx), restores what getcontext kept and
// returns through the saved ra

.text

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

.private_extern __setcontext
.align 2
__setcontext:
	// a0 = mcontext
	ld	s1, MCONTEXT_OFFSET_X(9)(a0)
	ld	s2, MCONTEXT_OFFSET_X(18)(a0)
	ld	s3, MCONTEXT_OFFSET_X(19)(a0)
	ld	s4, MCONTEXT_OFFSET_X(20)(a0)
	ld	s5, MCONTEXT_OFFSET_X(21)(a0)
	ld	s6, MCONTEXT_OFFSET_X(22)(a0)
	ld	s7, MCONTEXT_OFFSET_X(23)(a0)
	ld	s8, MCONTEXT_OFFSET_X(24)(a0)
	ld	s9, MCONTEXT_OFFSET_X(25)(a0)
	ld	s10, MCONTEXT_OFFSET_X(26)(a0)
	ld	s11, MCONTEXT_OFFSET_X(27)(a0)

	fld	fs0, MCONTEXT_OFFSET_F(8)(a0)
	fld	fs1, MCONTEXT_OFFSET_F(9)(a0)
	fld	fs2, MCONTEXT_OFFSET_F(18)(a0)
	fld	fs3, MCONTEXT_OFFSET_F(19)(a0)
	fld	fs4, MCONTEXT_OFFSET_F(20)(a0)
	fld	fs5, MCONTEXT_OFFSET_F(21)(a0)
	fld	fs6, MCONTEXT_OFFSET_F(22)(a0)
	fld	fs7, MCONTEXT_OFFSET_F(23)(a0)
	fld	fs8, MCONTEXT_OFFSET_F(24)(a0)
	fld	fs9, MCONTEXT_OFFSET_F(25)(a0)
	fld	fs10, MCONTEXT_OFFSET_F(26)(a0)
	fld	fs11, MCONTEXT_OFFSET_F(27)(a0)
	lw	t0, MCONTEXT_OFFSET_FCSR(a0)
	fscsr	t0

	// sp, fp and ra last, probing the new stack to catch a corrupt one
	ld	t1, MCONTEXT_OFFSET_X(2)(a0)
	ld	s0, MCONTEXT_OFFSET_X(8)(a0)
	ld	ra, MCONTEXT_OFFSET_X(1)(a0)
	lbu	t0, 0(t1)
	mv	sp, t1

	li	a0, 0			// return value
	ret

#endif // TARGET_OS_OSX || TARGET_OS_DRIVERKIT
