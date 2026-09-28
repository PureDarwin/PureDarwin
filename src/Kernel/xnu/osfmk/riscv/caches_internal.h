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
#ifndef _RISCV_CACHES_INTERNAL
#define _RISCV_CACHES_INTERNAL    1

#include <riscv/proc_reg.h>

#include <kern/kern_types.h>

// harts on the virt board are coherent, the c906 needs these for non-coherent dma
extern void flush_dcache_syscall( vm_offset_t addr, unsigned length);

#ifdef MACH_KERNEL_PRIVATE
extern void flush_dcache(vm_offset_t addr, unsigned count, int phys);
extern void flush_dcache64(addr64_t addr, unsigned count, int phys);
extern void invalidate_icache(vm_offset_t addr, unsigned cnt, int phys);
extern void invalidate_icache64(addr64_t addr, unsigned cnt, int phys);
#endif
extern void clean_dcache(vm_offset_t addr, unsigned count, int phys);
extern void clean_dcache64(addr64_t addr, unsigned count, int phys);

extern void cache_sync_page(ppnum_t pp);

extern void platform_cache_init(void);
extern void platform_cache_idle_enter(void);
extern void platform_cache_flush(void);
extern boolean_t platform_cache_batch_wimg(unsigned int new_wimg, unsigned int size);
extern void platform_cache_flush_wimg(unsigned int new_wimg);
extern void platform_cache_clean(void);
extern void platform_cache_shutdown(void);
extern void platform_cache_disable(void);

#endif /* #ifndef _RISCV_CACHES_INTERNAL */
