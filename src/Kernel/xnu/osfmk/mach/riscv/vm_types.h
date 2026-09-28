/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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
#ifndef _MACH_RISCV_VM_TYPES_H_
#define _MACH_RISCV_VM_TYPES_H_

#if defined (__riscv)

#ifndef ASSEMBLER

#include <riscv/_types.h>
#include <stdint.h>
#include <Availability.h>
#include <sys/cdefs.h>

typedef __darwin_natural_t      natural_t;
typedef int                     integer_t;

// riscv64 only, there is no 32-bit darwin userland
typedef uintptr_t               vm_offset_t __kernel_ptr_semantics;
typedef uintptr_t               vm_size_t;

typedef uint64_t                mach_vm_address_t __kernel_ptr_semantics;
typedef uint64_t                mach_vm_offset_t __kernel_ptr_semantics;
typedef uint64_t                mach_vm_size_t;

typedef uint64_t                vm_map_offset_t __kernel_ptr_semantics;
typedef uint64_t                vm_map_address_t __kernel_ptr_semantics;
typedef uint64_t                vm_map_size_t;

typedef uint32_t                vm32_offset_t;
typedef uint32_t                vm32_address_t;
typedef uint32_t                vm32_size_t;

typedef vm_offset_t             mach_port_context_t;

#ifdef MACH_KERNEL_PRIVATE
typedef vm32_offset_t           mach_port_context32_t;
typedef mach_vm_offset_t        mach_port_context64_t;
#endif

#endif  /* ASSEMBLER */

#define MACH_MSG_TYPE_INTEGER_T MACH_MSG_TYPE_INTEGER_32

#endif /* defined (__riscv) */

#endif  /* _MACH_RISCV_VM_TYPES_H_ */
