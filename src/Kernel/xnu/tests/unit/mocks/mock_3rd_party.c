/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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

#include <stdint.h>
#include <stdbool.h>

#include <firehose/firehose_types_private.h>
#include <firehose/tracepoint_private.h>

// This usually comes from tightbeam library. This is not needed by the tester so it is mocked here are a no-op
void
tb_transport_startup(void)
{
}

#if !__BUILDING_FOR_COVERAGE__
// These __llvm_* functions usually come from cc_kext library and are used for profiling.
// This functionality is not needed by the tester so they are mocked here as a no-op to resolve their linking.
// linking to cc_kext is not possible because it is compiled with -mkernel
// When building unit-tests for coverage, these function come from the user-mode coverage lib, so they shouldn't
// be redefined here.
uint64_t
__llvm_profile_get_size_for_buffer_internal(const char *DataBegin,
    const char *DataEnd,
    const char *CountersBegin,
    const char *CountersEnd,
    const char *NamesBegin,
    const char *NamesEnd)
{
	return 0;
}

int
__llvm_profile_write_buffer_internal(char *Buffer,
    const char *DataBegin,
    const char *DataEnd,
    const char *CountersBegin,
    const char *CountersEnd,
    const char *NamesBegin,
    const char *NamesEnd)
{
	return 0;
}
#endif // !__BUILDING_FOR_COVERAGE__

// These __firehose_* functions usually come from libfirehose_kernel are used for logging
// This functionality is not needed and is mocked by the tester, so these functions are mocked as no-op
// to resolve their linking.
// Linking to libfirehose_kernel is not possible because it is compiled with -mkernel

firehose_buffer_t
__firehose_buffer_create(size_t *size)
{
	return NULL;
}

void
__firehose_buffer_tracepoint_flush(firehose_tracepoint_t vat,
    firehose_tracepoint_id_u vatid)
{
}

firehose_tracepoint_t
__firehose_buffer_tracepoint_reserve(uint64_t stamp, firehose_stream_t stream,
    uint16_t pubsize, uint16_t privsize, uint8_t **privptr)
{
	return NULL;
}

int
__firehose_kernel_configuration_valid(uint8_t chunk_count, uint8_t io_pages)
{
	return 0;
}

bool
__firehose_merge_updates(firehose_push_reply_t update)
{
	return false;
}


