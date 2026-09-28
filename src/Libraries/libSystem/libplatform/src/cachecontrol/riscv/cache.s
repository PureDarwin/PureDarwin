/*
 * Copyright (c) 2011-2017 Apple Computer, Inc. All rights reserved.
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

// fence.i only reaches this hart, other threads of the task may run the code on
// other harts, so the kernel's platform call 0 fences every hart

// void sys_icache_invalidate(void *start, size_t length)
.globl	_sys_icache_invalidate
.p2align	2
_sys_icache_invalidate:
	beqz	a1, 1f				// length > 0 ?
	fence	rw, rw				// order the new code's stores first
	li	a3, 0				// platform call 0, icache flush on all harts
	li	t0, -0x80000000
	ecall
1:
	ret

// void sys_dcache_flush(void *start, size_t length)
.globl	_sys_dcache_flush
.p2align	2
_sys_dcache_flush:
	fence	rw, rw				// noop, we are fully coherent
	ret
