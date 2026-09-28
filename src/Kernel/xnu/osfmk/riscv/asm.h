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
#ifndef _RISCV_ASM_H_
#define _RISCV_ASM_H_

#if defined (__riscv)

/* There is another definition of ALIGN for .c sources */
#ifdef ASSEMBLER
#define ALIGN 2
#endif /* ASSEMBLER */

#ifndef FALIGN
#define FALIGN ALIGN
#endif

#define LB(x,n) n
#ifndef __NO_UNDERSCORES__
#define LCL(x)  L ## x
#define EXT(x) _ ## x
#define LEXT(x) _ ## x ## :
#else
#define LCL(x)  .L ## x
#define EXT(x) x
#define LEXT(x) x ## :
#endif
#define LBc(x,n) n ## :
#define LBb(x,n) n ## b
#define LBf(x,n) n ## f

#define String  .asciz
#define Value   .word
#define Times(a,b) (a*b)
#define Divide(a,b) (a/b)

#define Entry(x)        .globl EXT(x); .align FALIGN; LEXT(x)
#define ENTRY(x)        Entry(x) MCOUNT
#define ENTRY2(x,y)     .globl EXT(x); .globl EXT(y); \
	                .align FALIGN; LEXT(x); LEXT(y) \
	                MCOUNT
#define ASENTRY(x)      .globl x; .align FALIGN; x ## : MCOUNT

#define DATA(x)         .globl EXT(x); .align ALIGN; LEXT(x)

#define End(x)
#define END(x)          End(EXT(x))
#define ENDDATA(x)      END(x)
#define Enddata(x)      End(x)

#ifdef ASSEMBLER

#define MCOUNT

#define RET     ret

// register sized loads and stores, riscv64 only
#define SZREG   8
#define REG_S   sd
#define REG_L   ld

#else /* NOT ASSEMBLER */

#ifndef __NO_UNDERSCORES__
#define CC_SYM_PREFIX "_"
#else
#define CC_SYM_PREFIX ""
#endif /* __NO_UNDERSCORES__ */

#endif /* ASSEMBLER */

#endif /* defined (__riscv) */

#endif /* _RISCV_ASM_H_ */
