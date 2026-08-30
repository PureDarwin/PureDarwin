/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <mach/vm_param.h>
#include <stdint.h>

#define _STR(X) #X
#define STR(X) _STR(X)

void start() asm ("start");

/*
 * A library-free routine to return the number of bytes in a pointer. This
 * allows us to test the kernel loader policy without having an external
 * dependency on dyld and/or the presence of a slice in a library dependency
 * (which it often does not, even if the kernel supports the policy).
 *
 * No C code. The stack is not guaranteed to be aligned yet.
 */
__attribute__((naked, noreturn))
void
start()
{
	/* exit(__WORDSIZE/8) */
	asm volatile (
            "mov x0, " STR(__WORDSIZE/8) "\n"
            "mov x16, #1\n"
            "svc #(" STR(SWI_SYSCALL) ")\n");
}
