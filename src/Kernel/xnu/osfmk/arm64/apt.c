/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#include <arm/cpu_data_internal.h>
#include <mach/mach_types.h>
#include <mach/mach_traps.h>
#include <mach/mach_vm.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_pageout_internal.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_object_internal.h>

// fixme: rdar://114299113 tracks resolving the supportlib issue with hwtrace features


bool
apt_allocate_va_buffer(__unused size_t allocation_size, vm_map_offset_t *__unused ret_mapped_addr, upl_t *__unused ret_upl);
bool
apt_allocate_va_buffer(__unused size_t allocation_size, vm_map_offset_t *__unused ret_mapped_addr, upl_t *__unused ret_upl)
{
	return false;
}

void
apt_free_va_buffer(__unused size_t allocation_size, __unused vm_map_offset_t mapped_addr, __unused upl_t upl);
void
apt_free_va_buffer(__unused size_t allocation_size, __unused vm_map_offset_t mapped_addr, __unused upl_t upl)
{
}
