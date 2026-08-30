/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (C) Apple Computer 1998
 * ALL Rights Reserved
 */
/*
 * This file represents the interfaces that used to come
 * from creating the user headers from the mach.defs file.
 * Because mach.defs was decomposed, this file now just
 * wraps up all the new interface headers generated from
 * each of the new .defs resulting from that decomposition.
 */
#ifndef _MACH_INTERFACE_H_
#define _MACH_INTERFACE_H_

/* Keep the generated client interfaces self-contained during recursive
 * inclusion through the libc Mach wrappers. */
#include <mach/mach_types.h>
#ifdef __cplusplus
extern "C" {
#endif
extern kern_return_t vm_allocate(vm_map_t, vm_address_t *, vm_size_t, int);
extern kern_return_t vm_deallocate(vm_map_t, vm_address_t, vm_size_t);
extern kern_return_t vm_protect(vm_map_t, vm_address_t, vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t vm_remap(mach_port_name_t, vm_address_t *, vm_size_t,
    vm_offset_t, int, mach_port_name_t, vm_address_t, boolean_t,
    vm_prot_t *, vm_prot_t *, vm_inherit_t);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);
#ifdef __cplusplus
}
#endif
#include <mach/clock_priv.h>
#include <mach/host_priv.h>
#include <mach/host_security.h>
#include <mach/lock_set.h>
#include <mach/processor.h>
#include <mach/processor_set.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#endif /* _MACH_INTERFACE_H_ */
