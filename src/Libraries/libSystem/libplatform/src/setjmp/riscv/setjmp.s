/*
 * Copyright (c) 2011-2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// jmp_buf layout, ra sp and s0 are stored munged
#define	JMP_ra		0x00
#define	JMP_sp		0x08
#define	JMP_s0		0x10
#define	JMP_s1		0x18
#define	JMP_s2		0x20
#define	JMP_s3		0x28
#define	JMP_s4		0x30
#define	JMP_s5		0x38
#define	JMP_s6		0x40
#define	JMP_s7		0x48
#define	JMP_s8		0x50
#define	JMP_s9		0x58
#define	JMP_s10		0x60
#define	JMP_s11		0x68
#define	JMP_fs0		0x70
#define	JMP_fs1		0x78
#define	JMP_fs2		0x80
#define	JMP_fs3		0x88
#define	JMP_fs4		0x90
#define	JMP_fs5		0x98
#define	JMP_fs6		0xa0
#define	JMP_fs7		0xa8
#define	JMP_fs8		0xb0
#define	JMP_fs9		0xb8
#define	JMP_fs10	0xc0
#define	JMP_fs11	0xc8
#define	JMP_sigmask	0xd0
#define	JMP_sigflag	0xd8
#define	JMP_sigonstack	0xdc	// whether the thread is on sigaltstack or not

#define STACK_SSFLAGS	16	// offsetof(stack_t, ss_flags)

#include <architecture/riscv/asm_help.h>
#include <os/tsd.h>


// int _setjmp(jmp_buf env)
ENTRY_POINT(__setjmp)
	_OS_PTR_MUNGE_TOKEN(t0, t0)
	_OS_PTR_MUNGE(t1, ra, t0)
	_OS_PTR_MUNGE(t2, sp, t0)
	_OS_PTR_MUNGE(t3, s0, t0)
	sd	t1, JMP_ra(a0)
	sd	t2, JMP_sp(a0)
	sd	t3, JMP_s0(a0)
	sd	s1, JMP_s1(a0)
	sd	s2, JMP_s2(a0)
	sd	s3, JMP_s3(a0)
	sd	s4, JMP_s4(a0)
	sd	s5, JMP_s5(a0)
	sd	s6, JMP_s6(a0)
	sd	s7, JMP_s7(a0)
	sd	s8, JMP_s8(a0)
	sd	s9, JMP_s9(a0)
	sd	s10, JMP_s10(a0)
	sd	s11, JMP_s11(a0)
	fsd	fs0, JMP_fs0(a0)
	fsd	fs1, JMP_fs1(a0)
	fsd	fs2, JMP_fs2(a0)
	fsd	fs3, JMP_fs3(a0)
	fsd	fs4, JMP_fs4(a0)
	fsd	fs5, JMP_fs5(a0)
	fsd	fs6, JMP_fs6(a0)
	fsd	fs7, JMP_fs7(a0)
	fsd	fs8, JMP_fs8(a0)
	fsd	fs9, JMP_fs9(a0)
	fsd	fs10, JMP_fs10(a0)
	fsd	fs11, JMP_fs11(a0)
	li	a0, 0
	ret

// void _longjmp(jmp_buf env, int val)
ENTRY_POINT(__longjmp)
	ld	t1, JMP_ra(a0)
	ld	t2, JMP_sp(a0)
	ld	t3, JMP_s0(a0)
	ld	s1, JMP_s1(a0)
	ld	s2, JMP_s2(a0)
	ld	s3, JMP_s3(a0)
	ld	s4, JMP_s4(a0)
	ld	s5, JMP_s5(a0)
	ld	s6, JMP_s6(a0)
	ld	s7, JMP_s7(a0)
	ld	s8, JMP_s8(a0)
	ld	s9, JMP_s9(a0)
	ld	s10, JMP_s10(a0)
	ld	s11, JMP_s11(a0)
	fld	fs0, JMP_fs0(a0)
	fld	fs1, JMP_fs1(a0)
	fld	fs2, JMP_fs2(a0)
	fld	fs3, JMP_fs3(a0)
	fld	fs4, JMP_fs4(a0)
	fld	fs5, JMP_fs5(a0)
	fld	fs6, JMP_fs6(a0)
	fld	fs7, JMP_fs7(a0)
	fld	fs8, JMP_fs8(a0)
	fld	fs9, JMP_fs9(a0)
	fld	fs10, JMP_fs10(a0)
	fld	fs11, JMP_fs11(a0)
	_OS_PTR_MUNGE_TOKEN(t0, t0)
	_OS_PTR_UNMUNGE(ra, t1, t0)
	_OS_PTR_UNMUNGE(t2, t2, t0)
	_OS_PTR_UNMUNGE(s0, t3, t0)
	lbu	t0, 0(t2)		// probe to detect absolutely corrupt stack pointers
	mv	sp, t2
	sext.w	a0, a1
	seqz	t0, a0			// longjmp(env, 0) returns 1
	add	a0, a0, t0
	ret

// int sigsetjmp(sigjmp_buf env, int savemask)
ENTRY_POINT(_sigsetjmp)
	sw	a1, JMP_sigflag(a0)
	bnez	a1, 1f
	tail	__setjmp
1:
	// else, fall through

// int setjmp(jmp_buf env)
ENTRY_POINT(_setjmp)
	sd	s1, JMP_s1(a0)		// stash s1 and ra in the jmpbuf for now
	sd	ra, JMP_ra(a0)
	mv	s1, a0			// s1 = env

	// save the sigmask
	li	a0, 1			// how = SIG_BLOCK
	li	a1, 0			// set = 0
	addi	a2, s1, JMP_sigmask	// oset = env + JMP_sigmask
	CALL_EXTERNAL(_sigprocmask)

	// get current sigaltstack status
	addi	sp, sp, -32		// 24 bytes for a stack_t, sp stays 16 byte aligned
	li	a0, 0			// ss = NULL
	mv	a1, sp			// oss = the stack_t on the stack
	CALL_EXTERNAL(___sigaltstack)	// sigaltstack(NULL, oss)
	lw	a0, STACK_SSFLAGS(sp)	// ss flags from the stack_t
	sw	a0, JMP_sigonstack(s1)
	addi	sp, sp, 32		// reset sp

	mv	a0, s1
	ld	ra, JMP_ra(a0)
	ld	s1, JMP_s1(a0)
	tail	__setjmp


// void siglongjmp(sigjmp_buf env, int val)
ENTRY_POINT(_siglongjmp)
	lw	t0, JMP_sigflag(a0)
	bnez	t0, 1f
	tail	__longjmp
1:
	// else, fall through

// void longjmp(jmp_buf env, int val)
ENTRY_POINT(_longjmp)
	addi	sp, sp, -16
	mv	s1, a0			// s1 and s2 are restored by __longjmp
	mv	s2, a1

	// restore the signal mask
	ld	t0, JMP_sigmask(s1)
	sd	t0, 8(sp)
	li	a0, 3			// SIG_SETMASK
	addi	a1, sp, 8		// set
	li	a2, 0			// oset
	CALL_EXTERNAL(_sigprocmask)

	// restore the sigaltstack status
	lw	a0, JMP_sigonstack(s1)	// saved sigonstack info
	CALL_EXTERNAL(__sigunaltstack)

	mv	a0, s1
	mv	a1, s2
	addi	sp, sp, 16
	tail	__longjmp
