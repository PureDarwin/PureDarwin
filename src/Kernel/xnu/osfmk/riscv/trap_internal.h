/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#ifndef _RISCV_TRAP_INTERNAL_H_
#define _RISCV_TRAP_INTERNAL_H_

#include <riscv/trap.h>
#include <riscv/thread.h>

// the lui x0 hint that carries the code of a kernel ebreak
#define RISCV_TRAP_CODE_HINT_MASK       0x00000fff
#define RISCV_TRAP_CODE_HINT_OP         0x00000037
#define RISCV_IS_TRAP_CODE_HINT(insn)   (((insn) & RISCV_TRAP_CODE_HINT_MASK) == RISCV_TRAP_CODE_HINT_OP)
#define RISCV_TRAP_CODE(insn)           ((uint32_t)(insn) >> 12)

#define RISCV_EBREAK                    0x00100073
#define RISCV_C_EBREAK                  0x9002

extern kern_return_t riscv_fast_fault(pmap_t, vm_map_address_t, vm_prot_t, bool, bool);

typedef kern_return_t (*perfCallback)(
	int                       trapno,
	struct riscv_saved_state  *ss,
	int,
	int);

#endif  /* _RISCV_TRAP_INTERNAL_H_ */
