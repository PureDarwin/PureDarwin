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

.text

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

// makecontext leaves s1 = uctx, s2 = munged user func, fp = top of stack and
// sp = the argument block, at least 8 slots so a0-a7 always load from it
.private_extern __ctx_start
.align 2
__ctx_start:
	ld	a0, 0(sp)
	ld	a1, 8(sp)
	ld	a2, 16(sp)
	ld	a3, 24(sp)
	ld	a4, 32(sp)
	ld	a5, 40(sp)
	ld	a6, 48(sp)
	ld	a7, 56(sp)
	addi	sp, sp, 64		// arguments past the eighth stay on the stack

	_OS_PTR_MUNGE_TOKEN(t0, t0)
	_OS_PTR_UNMUNGE(t1, s2, t0)
	jalr	t1

	// user function returned, reset to the top of the stack for _ctx_done
	mv	sp, s0
	mv	a0, s1			// a0 = uctx
	call	__ctx_done

	unimp				// should not get here

#endif
