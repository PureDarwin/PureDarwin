/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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
#include <kern/hvg_hypercall.h>
#include <pexpert/pexpert.h>
#include <riscv/machine_routines.h>

// riscv guests have no hypercall interface to a host, every call reports unsupported

bool
hvg_is_hcall_available(__unused hvg_hcall_code_t hcall)
{
	return false;
}

hvg_hcall_return_t
hvg_hcall_get_mabs_offset(__attribute__((unused)) uint64_t *mabs_offset)
{
	return HVG_HCALL_UNSUPPORTED;
}

hvg_hcall_return_t
hvg_hcall_get_bootsessionuuid(__attribute__((unused)) uuid_string_t uuid)
{
	return HVG_HCALL_UNSUPPORTED;
}

hvg_hcall_return_t
hvg_hcall_trigger_dump(__unused hvg_hcall_vmcore_file_t *vmcore,
    __unused const hvg_hcall_dump_option_t dump_option)
{
	return HVG_HCALL_UNSUPPORTED;
}

/* Unsupported. */
void
hvg_hcall_set_coredump_data(void)
{
}
