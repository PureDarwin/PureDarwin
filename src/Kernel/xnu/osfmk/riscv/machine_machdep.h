/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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
#ifndef _RISCV_MACHINE_MACHDEP_H_
#define _RISCV_MACHINE_MACHDEP_H_

// the user gp carries the cpu data arm64 keeps in TPIDRRO_EL0, the kernel writes it on every
// return to user and user code only reads it, keep in sync with libsyscall's os/tsd.h
#define MACHDEP_GP_CPUNUM_SHIFT         0
#define MACHDEP_GP_CPUNUM_MASK          0x0000000000000fff
#define MACHDEP_GP_CLUSTERID_SHIFT      12
#define MACHDEP_GP_CLUSTERID_MASK       0x00000000000ff000

#endif /* _RISCV_MACHINE_MACHDEP_H_ */
