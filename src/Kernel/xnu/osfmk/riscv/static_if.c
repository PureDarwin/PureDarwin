/* * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include <mach/vm_types.h>
#include <machine/static_if.h>
#include <riscv/proc_reg.h>
#include <riscv/sbi.h>
#include <kern/startup.h>

extern char __text_exec_start[] __SEGMENT_START_SYM("__TEXT_EXEC");
extern char __text_exec_end[]   __SEGMENT_END_SYM("__TEXT_EXEC");

__attribute__((always_inline))
static uint32_t
riscv_insn_nop(void)
{
	// addi x0, x0, 0
	return 0x00000013;
}

__attribute__((always_inline))
static uint32_t
riscv_insn_j(int32_t delta)
{
	uint32_t imm = (uint32_t)delta;

	// jal x0, delta with the j-type immediate scattered as imm[20|10:1|11|19:12]
	return 0x0000006fu |
	       (((imm >> 20) & 0x1) << 31) |
	       (((imm >> 1) & 0x3ff) << 21) |
	       (((imm >> 11) & 0x1) << 20) |
	       (((imm >> 12) & 0xff) << 12);
}

MARK_AS_FIXUP_TEXT void
ml_static_if_entry_patch(static_if_entry_t sie, int branch)
{
	vm_offset_t patch_point = __static_if_entry_patch_point(sie);
	uint32_t insn;

	if (branch) {
		insn = riscv_insn_j(sie->sie_target);
	} else {
		insn = riscv_insn_nop();
	}

	if ((vm_offset_t)__text_exec_start <= patch_point &&
	    patch_point < (vm_offset_t)__text_exec_end) {
		// both forms are 4 bytes, rvc code may leave them only 2 byte aligned
		if (patch_point & 3) {
			((volatile uint16_t *)patch_point)[0] = (uint16_t)insn;
			((volatile uint16_t *)patch_point)[1] = (uint16_t)(insn >> 16);
		} else {
			*(volatile uint32_t *)patch_point = insn;
		}
		__asm__ volatile ("fence.i" ::: "memory");
	}
}

MARK_AS_FIXUP_TEXT void
ml_static_if_flush_icache(void)
{
	__asm__ volatile ("fence rw, rw" ::: "memory");
	__asm__ volatile ("fence.i" ::: "memory");
	// other harts may hold the old instructions too
	(void)sbi_remote_fence_i(0, SBI_HART_MASK_BASE_ALL);
}
