/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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
#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include "assym.s"

	.section __BOOTDATA, __data
	.align 12

	// the boot hart's stacks, each between guard pages
	.globl EXT(intstack_low_guard)
LEXT(intstack_low_guard)
	.space	PGBYTES
	.globl	EXT(intstack)
LEXT(intstack)
	.space	INTSTACK_SIZE
	.globl	EXT(intstack_top)
LEXT(intstack_top)
	.globl	EXT(intstack_high_guard)
LEXT(intstack_high_guard)
	.space	PGBYTES
	.globl	EXT(excepstack)
LEXT(excepstack)
	.space	EXCEPSTACK_SIZE
	.globl	EXT(excepstack_top)
LEXT(excepstack_top)
	.globl	EXT(excepstack_high_guard)
LEXT(excepstack_high_guard)
	.space	PGBYTES

	// space for kdebug's early event buffer
	.globl	EXT(kd_early_buffer)
	.align	12
LEXT(kd_early_buffer)
	.space	16*1024, 0

	// sv39 root table start.s maps the kernel with before the pmap exists
	.globl	EXT(bootstrap_pagetable)
	.align	12
LEXT(bootstrap_pagetable)
	.space	PGBYTES, 0
