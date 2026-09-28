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
#ifndef _MACH_RISCV_VM_PARAM_H_
#define _MACH_RISCV_VM_PARAM_H_

#if defined (__riscv)

#if !defined (KERNEL) && !defined (__ASSEMBLER__)
#include <mach/vm_page_size.h>
#endif

#define BYTE_SIZE       8       /* byte size in bits */

#if defined (KERNEL)

#ifndef __ASSEMBLER__

// sv39 with 4K base pages only
#define PAGE_SHIFT_CONST        12
#define PAGE_SHIFT              PAGE_SHIFT_CONST
#define PAGE_SIZE               (1 << PAGE_SHIFT)
#define PAGE_MASK               (PAGE_SIZE-1)

#define VM_PAGE_SIZE            PAGE_SIZE

#define machine_ptob(x)         ((x) << PAGE_SHIFT)

#define TEST_PAGE_SIZE_16K      (0)
#define TEST_PAGE_SIZE_4K       (1)

#endif  /* !__ASSEMBLER__ */

#else

#define PAGE_SHIFT                      vm_page_shift
#define PAGE_SIZE                       vm_page_size
#define PAGE_MASK                       vm_page_mask

#define VM_PAGE_SIZE            vm_page_size

#define machine_ptob(x)         ((x) << PAGE_SHIFT)

#endif

#define PAGE_MAX_SHIFT          12
#define PAGE_MAX_SIZE           (1 << PAGE_MAX_SHIFT)
#define PAGE_MAX_MASK           (PAGE_MAX_SIZE-1)

#define PAGE_MIN_SHIFT          12
#define PAGE_MIN_SIZE           (1 << PAGE_MIN_SHIFT)
#define PAGE_MIN_MASK           (PAGE_MIN_SIZE-1)

#define VM_MAX_PAGE_ADDRESS     MACH_VM_MAX_ADDRESS

#ifndef __ASSEMBLER__

#ifdef  MACH_KERNEL_PRIVATE

#define VM32_SUPPORT            1
#define VM32_MIN_ADDRESS        ((vm32_offset_t) 0)
#define VM32_MAX_ADDRESS        ((vm32_offset_t) (VM_MAX_ADDRESS & 0xFFFFFFFF))

#endif /* MACH_KERNEL_PRIVATE */

#define VM_MIN_ADDRESS          ((vm_address_t) 0x0000000000000000ULL)
#define VM_MAX_ADDRESS          ((vm_address_t) 0x00000000F0000000ULL)

// sv39 gives user space 256GB, the last GB is kept for the commpage
#define MACH_VM_MIN_ADDRESS_RAW 0x0ULL
#define MACH_VM_MAX_ADDRESS_RAW 0x0000003FC0000000ULL

#if XNU_KERNEL_PRIVATE
#define VM_LARGE_FILE_THRESHOLD (1ULL << 30)
#define MACH_VM_JUMBO_ADDRESS   ((mach_vm_offset_t) MACH_VM_MAX_ADDRESS_RAW)
#endif /* XNU_KERNEL_PRIVATE */

#define MACH_VM_MIN_ADDRESS     ((mach_vm_offset_t) MACH_VM_MIN_ADDRESS_RAW)
#define MACH_VM_MAX_ADDRESS     ((mach_vm_offset_t) MACH_VM_MAX_ADDRESS_RAW)

#define VM_MAP_MIN_ADDRESS      VM_MIN_ADDRESS
#define VM_MAP_MAX_ADDRESS      VM_MAX_ADDRESS

#ifdef  KERNEL

#define TiB(x)                  ((0ULL + (x)) << 40)
#define GiB(x)                  ((0ULL + (x)) << 30)
#define KALLOC_MINSIZE          16      /* minimum allocation size */
#define KALLOC_LOG2_MINALIGN    4       /* log2 minimum alignment */

#ifndef __BUILDING_XNU_LIBRARY__
// the kernel uses the top 128GB of the upper sv39 half, the reach of packed kernel pointers
// the kernel collection sits in its top 2GB
#define VM_KERNEL_POINTER_SIGNIFICANT_BITS  39
#define VM_MIN_KERNEL_ADDRESS   ((vm_address_t) 0xffffffe000000000ULL)
#define VM_MAX_KERNEL_ADDRESS   ((vm_address_t) 0xffffffffffffefffULL)
#else /* __BUILDING_XNU_LIBRARY__ */
#define VM_MIN_KERNEL_ADDRESS   ((vm_address_t)(0ULL + PAGE_SIZE))
#define VM_MAX_KERNEL_ADDRESS   ((vm_address_t)(0ULL + GiB(64)))
#define VM_KERNEL_POINTER_SIGNIFICANT_BITS  39
#endif /* __BUILDING_XNU_LIBRARY__ */

#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  VM_MIN_KERNEL_ADDRESS

// no top byte ignore and no pointer authentication
#define VM_USER_STRIP_TBI(_v)           (_v)
#define _VM_KERNEL_STRIP_PTR(_v)        (_v)

#define VM_KERNEL_STRIP_PTR(_va)        (_VM_KERNEL_STRIP_PTR((uintptr_t)(_va)))
#define VM_KERNEL_STRIP_UPTR(_va)       (VM_KERNEL_STRIP_PTR(_va))

#define VM_KERNEL_ADDRESS(_va)  \
	((VM_KERNEL_STRIP_PTR(_va) >= VM_MIN_KERNEL_ADDRESS) && \
	 (VM_KERNEL_STRIP_PTR(_va) <= VM_MAX_KERNEL_ADDRESS))

#define VM_USER_STRIP_PTR(_v)           (VM_USER_STRIP_TBI(_v))

#define ML_ADDRPERM(addr, slide) ((addr) + (slide))

#ifdef  MACH_KERNEL_PRIVATE
// physical aperture bounds, set up by the pmap from boot_args
extern unsigned long            gVirtBase, gPhysBase, gPhysSize;

#define isphysmem(a)            (((vm_address_t)(a) - gPhysBase) < gPhysSize)
#define physmap_enclosed(a)     isphysmem(a)

#include <stdint.h>
extern uint64_t                 gDramBase, gDramSize;
#define is_dram_addr(addr)      (((uint64_t)(addr) - gDramBase) < gDramSize)

#endif /* MACH_KERNEL_PRIVATE */

#ifdef  XNU_KERNEL_PRIVATE

#if KASAN
# define KERNEL_STACK_SIZE      (4*4*4096)
#elif DEBUG
# define KERNEL_STACK_SIZE      (2*4*4096)
#else
#ifndef KERNEL_STACK_MULTIPLIER
#define KERNEL_STACK_MULTIPLIER (1)
#endif /* KERNEL_STACK_MULTIPLIER */
# define KERNEL_STACK_SIZE      (4*4096*KERNEL_STACK_MULTIPLIER)
#endif

#define INTSTACK_SIZE           (4*4096)
#define EXCEPSTACK_SIZE         (4*4096)

#endif /* XNU_KERNEL_PRIVATE */

// VM_KERNEL_LINK_ADDRESS comes from makedefs/MakeInc.def

#endif  /* KERNEL */

#endif  /* !__ASSEMBLER__ */

#endif /* defined (__riscv) */

#endif  /* _MACH_RISCV_VM_PARAM_H_ */
