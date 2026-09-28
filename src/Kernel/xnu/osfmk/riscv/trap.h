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
#ifndef _RISCV_TRAP_H_
#define _RISCV_TRAP_H_

// scause exception codes, interrupts have the top bit set
#define T_INSN_MISALIGNED       0
#define T_INSN_ACCESS           1
#define T_ILLEGAL               2
#define T_BREAKPOINT            3
#define T_LOAD_MISALIGNED       4
#define T_LOAD_ACCESS           5
#define T_STORE_MISALIGNED      6
#define T_STORE_ACCESS          7
#define T_ECALL_U               8
#define T_ECALL_S               9
#define T_INSN_PAGE_FAULT       12
#define T_LOAD_PAGE_FAULT       13
#define T_STORE_PAGE_FAULT      15

#define T_IRQ_SOFT              1
#define T_IRQ_TIMER             5
#define T_IRQ_EXTERNAL          9

#define T_PF_PROT               0x1             /* protection violation */
#define T_PF_WRITE              0x2             /* write access */
#define T_PF_USER               0x4             /* from user state */

#if !defined(ASSEMBLER)

#define ML_TRAP_REGISTER_1      "t3"
#define ML_TRAP_REGISTER_2      "t4"
#define ML_TRAP_REGISTER_3      "t5"

// ebreak has no immediate, the code rides in the "lui x0, code" hint after it
// which runs as a nop when the handler resumes past the ebreak
#define ml_recoverable_trap(code) \
	__asm__ volatile (".option push\n.option norvc\nebreak\nlui x0, %0\n.option pop" : : "i"(code))

#define ml_fatal_trap(code)  ({ \
	ml_recoverable_trap(code); \
	__builtin_unreachable(); \
})

#if defined(XNU_KERNEL_PRIVATE)
#define TRAP_CASE(code) \
	case code: \
	    ml_fatal_trap(0x5500 + code);

#define TRAP_5CASES(code) \
	TRAP_CASE(code) \
	TRAP_CASE(code + 1) \
	TRAP_CASE(code + 2) \
	TRAP_CASE(code + 3) \
	TRAP_CASE(code + 4)

/* For use by clang option -ftrap-function only */
__attribute__((cold, always_inline))
static inline void
ml_bound_chk_soft_trap(unsigned char code)
{
	switch (code) {
		/* 0 ~ 24 */
		TRAP_5CASES(0)
		TRAP_5CASES(5)
		TRAP_5CASES(10)
		TRAP_5CASES(15)
		TRAP_5CASES(20)
	case 25:         /* Bound check */
#if BOUND_CHECKS_DEBUG
		ml_recoverable_trap(0x5500 + 25);
#else /* BOUND_CHECKS_DEBUG */
		ml_recoverable_trap(0xFF00 + 25);
#endif /* BOUND_CHECKS_DEBUG */
		break;
	default:
		ml_fatal_trap(0x0);
	}
}
#endif /* XNU_KERNEL_PRIVATE */
#endif /* !ASSEMBLER */

#endif  /* _RISCV_TRAP_H_ */
