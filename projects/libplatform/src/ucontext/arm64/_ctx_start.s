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

/* Helper macro for unmunging pointers in place */
.macro PTR_UNMUNGE addr
#if defined(__LP64__)
	_OS_PTR_MUNGE_TOKEN(x16, x16)
#else
	_OS_PTR_MUNGE_TOKEN(x16, w16)
#endif
	_OS_PTR_UNMUNGE(\addr, \addr, x16)
.endmacro

.macro CALL_USER_FUNC func
	// Populate the first 8 arguments in registers from the stack. Coordinated
	// with makecontext which populates the arguments on the stack
	ldp w0, w1, [sp], #32
	ldp w2, w3, [sp, #-24]
	ldp w4, w5, [sp, #-16]
	ldp w6, w7, [sp, #-8]

	PTR_UNMUNGE \func

#if defined(__arm64e__)
	blraaz	\func
#else
	blr	\func
#endif
.endmacro

.private_extern __ctx_start
.align 2
__ctx_start:
	/* x20 = munged signed user func,
	 * x19 = uctx,
	 * fp = top of stack,
	 * sp = where args end */
	CALL_USER_FUNC x20

	/* user function returned, set up stack for _ctx_done */

	/* Reset to top of stack */
	mov		sp, fp

	mov		x0, x19 /* x0 = uctx */
	bl		__ctx_done

	brk		#666	/* Should not get here */

#endif
