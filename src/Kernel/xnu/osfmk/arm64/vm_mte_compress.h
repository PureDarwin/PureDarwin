/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#pragma once

#ifndef COMPRESSOR_TESTER
#include <pexpert/arm64/board_config.h>
#include <vm/vm_compressor_xnu.h>
#endif /* COMPRESSOR_TESTER */

#if HAS_MTE

extern uint32_t vm_mte_rle_compress_tags(uint8_t *tags_in_buf, uint32_t in_size, uint8_t *out_buf, uint32_t out_size);
extern bool vm_mte_rle_decompress_tags(uint8_t *in_buf, uint32_t in_size, uint8_t *tags_out_buf, uint32_t out_size);
extern uint32_t vm_mte_compressed_tags_actual_size(uint32_t mte_size);
extern void vm_mte_tags_stats_compressed(uint32_t size_written);

#if DEVELOPMENT || DEBUG

struct comp_histogram {
#define VM_MTE_C_TAG_COUNT (16)
	uint64_t cmd_bins[VM_MTE_C_TAG_COUNT];
	uint64_t cmd_total; // total number of commands

	// comressed size below and including 64, 128, 192, 256, 320, 384, 448, 512
	uint64_t comp_size_bins[C_MTE_SIZE / C_SEG_OFFSET_ALIGNMENT_BOUNDARY];
	uint64_t same_value_count;
};
// generate a histogram of the number of times each command is seen in the input compressed buffer
// hist needs to be initialized with zeros before calling this function
extern bool vm_mte_rle_comp_histogram(uint8_t *in_buf, uint32_t in_size, struct comp_histogram *hist);

struct runs_histogram {
#define VM_MTE_C_MAX_TAG_RUN ((C_MTE_SIZE * 2) + 1)
	uint64_t rh_bins[VM_MTE_C_MAX_TAG_RUN];
};
// generate a histogram of the lengths of same-tag runs in the input uncompressed tags buffer
// hist needs to be zeroed before the call
extern void vm_mte_rle_runs_histogram(uint8_t *in_buf, uint32_t in_size, struct runs_histogram *hist);
#endif /* DEVELOPMENT || DEBUG */

__enum_closed_decl(vm_mte_c_tags_removal_reason_t, uint32_t, {
	VM_MTE_C_TAGS_REMOVAL_DECOMPRESSED,
	VM_MTE_C_TAGS_REMOVAL_FREE,
	VM_MTE_C_TAGS_REMOVAL_CORRUPT
});

extern void vm_mte_tags_stats_removed(uint32_t mte_size, vm_mte_c_tags_removal_reason_t reason);

#endif // HAS_MTE
