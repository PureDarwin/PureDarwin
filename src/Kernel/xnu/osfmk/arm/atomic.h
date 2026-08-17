/*
 * Copyright (c) 2015-2018 Apple Inc. All rights reserved.
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

#ifndef _MACHINE_ATOMIC_H
#error "Do not include <arm/atomic.h> directly, use <machine/atomic.h>"
#endif

#ifndef _ARM_ATOMIC_H_
#define _ARM_ATOMIC_H_

#include <mach/boolean.h>

// Parameter for __builtin_arm_dmb
#define DMB_OSHLD       0x1
#define DMB_OSHST       0x2
#define DMB_OSH         0x3
#define DMB_NSHLD       0x5
#define DMB_NSHST       0x6
#define DMB_NSH         0x7
#define DMB_ISHLD       0x9
#define DMB_ISHST       0xa
#define DMB_ISH         0xb
#define DMB_LD          0xd
#define DMB_ST          0xe
#define DMB_SY          0xf

// Parameter for __builtin_arm_dsb
#define DSB_OSHLD       0x1
#define DSB_OSHST       0x2
#define DSB_OSH         0x3
#define DSB_NSHLD       0x5
#define DSB_NSHST       0x6
#define DSB_NSH         0x7
#define DSB_ISHLD       0x9
#define DSB_ISHST       0xa
#define DSB_ISH         0xb
#define DSB_LD          0xd
#define DSB_ST          0xe
#define DSB_SY          0xf

// Parameter for __builtin_arm_isb
#define ISB_SY          0xf

#if defined (__arm__) && (__ARM_ARCH < 7)
/*
 * ARMv6 has no DMB/DSB/ISB instructions - the barriers are CP15 operations,
 * and LLVM has no way to select the llvm.arm.{dmb,dsb,isb} intrinsics for a
 * pre-v7 target ("Cannot select: intrinsic %llvm.arm.dmb"). Supply the CP15
 * forms under the builtin names so the ~80 call sites across the tree need no
 * changes.
 *
 * The shareability/ordering parameter is discarded deliberately: ARMv6 has no
 * such encoding, and the CP15 operation is a full system barrier - stronger
 * than any variant that could have been asked for, never weaker.
 */
#define __builtin_arm_dmb(_domain) \
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")
#define __builtin_arm_dsb(_domain) \
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
#define __builtin_arm_isb(_domain) \
	__asm__ volatile ("mcr p15, 0, %0, c7, c5, 4" : : "r" (0) : "memory")
#endif /* __arm__ && __ARM_ARCH < 7 */

/*
 * Hand-written barriers in the shared arm/ sources spell out the ARMv7+
 * mnemonics, which do not assemble for ARMv6. Route them through the builtins
 * instead, which are real instructions on v7+ and CP15 operations above.
 */
#if defined (__arm__) && (__ARM_ARCH < 7)
#define ARM_DMB_ISH()   __builtin_arm_dmb(DMB_ISH)
#define ARM_DSB_LD()    __builtin_arm_dsb(DSB_LD)
#else
#define ARM_DMB_ISH()   __asm__ volatile ("dmb ish" : : : "memory")
#define ARM_DSB_LD()    __asm__ volatile ("dsb ld"  : : : "memory")
#endif

#endif // _ARM_ATOMIC_H_
