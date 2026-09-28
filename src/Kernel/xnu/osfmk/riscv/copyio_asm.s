/*
 * Copyright (c) 2007-2015 Apple Inc. All rights reserved.
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

#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include <sys/errno.h>
#include "assym.s"

// every routine here runs with sstatus.SUM set, a fault inside a recovery range lands on
// its recovery label with a0 = EFAULT, the table is the one arm64 uses

.macro COPYIO_RECOVER_TABLE_SYM sym_name
	.pushsection __TEXT, __copyio_vectors, regular
	.p2align 3
	.private_extern EXT(\sym_name)
	.globl	EXT(\sym_name)
LEXT(\sym_name)
	.popsection
.endm

// struct copyio_recovery_entry { cre_start, cre_end, cre_recovery } as offsets from the table
.macro COPYIO_RECOVER_RANGE end_addr, recovery_addr
	.pushsection __TEXT, __copyio_vectors, regular
	.p2align 3
	.quad	Lcre_start_\@ - EXT(copyio_recover_table)
	.quad	\end_addr - EXT(copyio_recover_table)
	.quad	\recovery_addr - EXT(copyio_recover_table)
	.popsection
Lcre_start_\@:
.endm

.macro COPYIO_ENTER recovery
	li	t5, SSTATUS_SUM
	csrs	sstatus, t5
	COPYIO_RECOVER_RANGE \recovery, \recovery
.endm

.macro COPYIO_EXIT
	li	t5, SSTATUS_SUM
	csrc	sstatus, t5
.endm

	COPYIO_RECOVER_TABLE_SYM copyio_recover_table

// copy len bytes from src to dst, doublewords when both share an alignment
.macro COPYIO_BCOPY src, dst, len
	xor	t0, \src, \dst
	andi	t0, t0, 7
	bnez	t0, 4f
1:
	andi	t0, \src, 7
	beqz	t0, 2f
	beqz	\len, 5f
	lbu	t1, 0(\src)
	sb	t1, 0(\dst)
	addi	\src, \src, 1
	addi	\dst, \dst, 1
	addi	\len, \len, -1
	j	1b
2:
	li	t2, 32
3:
	bltu	\len, t2, 6f
	ld	t0, 0(\src)
	ld	t1, 8(\src)
	ld	t3, 16(\src)
	ld	t4, 24(\src)
	sd	t0, 0(\dst)
	sd	t1, 8(\dst)
	sd	t3, 16(\dst)
	sd	t4, 24(\dst)
	addi	\src, \src, 32
	addi	\dst, \dst, 32
	addi	\len, \len, -32
	j	3b
6:
	li	t2, 8
7:
	bltu	\len, t2, 4f
	ld	t0, 0(\src)
	sd	t0, 0(\dst)
	addi	\src, \src, 8
	addi	\dst, \dst, 8
	addi	\len, \len, -8
	j	7b
4:
	beqz	\len, 5f
	lbu	t1, 0(\src)
	sb	t1, 0(\dst)
	addi	\src, \src, 1
	addi	\dst, \dst, 1
	addi	\len, \len, -1
	j	4b
5:
.endm

// the loaded values are user controlled, clear them before returning
.macro COPYIO_CLEAR_TEMPS
	mv	t0, zero
	mv	t1, zero
	mv	t3, zero
	mv	t4, zero
.endm

	.text
	.align 2

	// int _bcopyin(const user_addr_t src, char *dst, vm_size_t len)
	.globl	EXT(_bcopyin)
LEXT(_bcopyin)
	COPYIO_ENTER Lbcopyin_exit
	COPYIO_BCOPY a0, a1, a2
	mv	a0, zero
Lbcopyin_exit:
	COPYIO_EXIT
	COPYIO_CLEAR_TEMPS
	ret

	// int _bcopyout(const char *src, user_addr_t dst, vm_size_t len)
	.globl	EXT(_bcopyout)
LEXT(_bcopyout)
	COPYIO_ENTER Lbcopyout_exit
	COPYIO_BCOPY a0, a1, a2
	mv	a0, zero
Lbcopyout_exit:
	COPYIO_EXIT
	COPYIO_CLEAR_TEMPS
	ret

	// int _bcopyinstr(const user_addr_t src, char *dst, vm_size_t max, vm_size_t *actual)
	.globl	EXT(_bcopyinstr)
LEXT(_bcopyinstr)
	COPYIO_ENTER Lbcopyinstr_exit
	mv	t0, zero
1:
	lbu	t1, 0(a0)
	sb	t1, 0(a1)
	addi	a0, a0, 1
	addi	a1, a1, 1
	addi	t0, t0, 1
	beqz	t1, 2f
	bne	t0, a2, 1b
	li	a0, ENAMETOOLONG
	j	3f
2:
	mv	a0, zero
3:
	sd	t0, 0(a3)
Lbcopyinstr_exit:
	COPYIO_EXIT
	mv	t0, zero
	mv	t1, zero
	mv	a2, zero
	ret

	// int _copyin_atomic32(const user_addr_t src, uint32_t *dst)
	.globl	EXT(_copyin_atomic32)
LEXT(_copyin_atomic32)
	COPYIO_ENTER Lcopyin_atomic32_exit
	lw	t0, 0(a0)
	sw	t0, 0(a1)
	mv	a0, zero
Lcopyin_atomic32_exit:
	COPYIO_EXIT
	ret

	// int _copyin_atomic32_wait_if_equals(const user_addr_t src, uint32_t value)
	// harts have no wait for a store, so a match only pauses briefly
	.globl	EXT(_copyin_atomic32_wait_if_equals)
LEXT(_copyin_atomic32_wait_if_equals)
	COPYIO_ENTER Lcopyin_atomic32_wait_exit
	lw	t0, 0(a0)
	sext.w	a1, a1
	li	a0, ESTALE
	bne	t0, a1, Lcopyin_atomic32_wait_exit
	mv	a0, zero
	// pause, a fence hint on harts without zihintpause
	.4byte	0x0100000f
Lcopyin_atomic32_wait_exit:
	COPYIO_EXIT
	ret

	// int _copyin_atomic64(const user_addr_t src, uint64_t *dst)
	.globl	EXT(_copyin_atomic64)
LEXT(_copyin_atomic64)
	COPYIO_ENTER Lcopyin_atomic64_exit
	ld	t0, 0(a0)
	sd	t0, 0(a1)
	mv	a0, zero
Lcopyin_atomic64_exit:
	COPYIO_EXIT
	ret

	// int _copyout_atomic32(uint32_t u32, user_addr_t dst)
	.globl	EXT(_copyout_atomic32)
LEXT(_copyout_atomic32)
	COPYIO_ENTER Lcopyout_atomic32_exit
	sw	a0, 0(a1)
	mv	a0, zero
Lcopyout_atomic32_exit:
	COPYIO_EXIT
	ret

	// int _copyout_atomic64(uint64_t u64, user_addr_t dst)
	.globl	EXT(_copyout_atomic64)
LEXT(_copyout_atomic64)
	COPYIO_ENTER Lcopyout_atomic64_exit
	sd	a0, 0(a1)
	mv	a0, zero
Lcopyout_atomic64_exit:
	COPYIO_EXIT
	ret

	// int copyinframe(const vm_address_t frame_addr, char *kernel_addr, bool is64bit)
	// the 16 byte frame record from user or kernel memory, there are no 32 bit frames
	.globl	EXT(copyinframe)
LEXT(copyinframe)
	li	t0, EFAULT
	beqz	a2, 1f
	COPYIO_ENTER Lcopyinframe_exit
	ld	t0, 0(a0)
	ld	t1, 8(a0)
	sd	t0, 0(a1)
	sd	t1, 8(a1)
	mv	a0, zero
Lcopyinframe_exit:
	COPYIO_EXIT
	mv	t0, zero
	mv	t1, zero
	ret
1:
	mv	a0, t0
	ret

	COPYIO_RECOVER_TABLE_SYM copyio_recover_table_end
