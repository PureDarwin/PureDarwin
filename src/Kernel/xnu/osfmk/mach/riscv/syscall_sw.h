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
#ifndef _MACH_RISCV_SYSCALL_SW_H_
#define _MACH_RISCV_SYSCALL_SW_H_

#if defined(__riscv)

#include <mach/machine/vm_param.h>

// the trap number goes in t0, negative for mach traps, arguments stay in a0-a7
#define kernel_trap(trap_name, trap_number, num_args) \
.globl _##trap_name                                           ; \
.text                                                         ; \
.align  2                                                     ; \
_##trap_name:                                                 ; \
    li t0, (trap_number)                                      ; \
    ecall                                                     ; \
    ret

#else
#error Unsupported architecture
#endif

#endif  /* _MACH_RISCV_SYSCALL_SW_H_ */
