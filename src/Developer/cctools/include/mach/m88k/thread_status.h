/*
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* Copyright (c) 1991 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	mach/m88k/thread_status.h
 *	Author:	Mike DeMoney, NeXT Computer, Inc.
 *
 *	This include file defines the per-thread state
 *	for NeXT 88K-based products.
 *
 * HISTORY
 * 23-Jan-91  Mike DeMoney (mike@next.com)
 *	Created.
 *
 * FIXME:
 *	All of these types should be pulled from architecture.
 *	Solve possible conflicting types problem for implementations
 *	by making user define an implementation (e.g. #define __M88110__)
 *	to get a implementation specific features.
 *
 *	Put fp envelope stuff in mach/m88k/m88110_fpee.h.
 */

#ifndef	_MACH_M88K_THREAD_STATE_
#define	_MACH_M88K_THREAD_STATE_

#import <architecture/m88k/fp_regs.h>
#import <architecture/m88k/reg_help.h>

/**************************************************************************
 * Data Typedefs used by thread_getstatus() and thread_setstatus()        *
 * NOTE: FP control and status regs described in <mach/m88k/fp_regs.h>    *
 **************************************************************************/

#define	M88K_THREAD_STATE_GRF		(1)	// general registers
#define M88K_THREAD_STATE_XRF		(2)	// extended and fp registers
#define	M88K_THREAD_STATE_USER		(3)	// non-architectural user state
#define M88110_THREAD_STATE_IMPL	(4)	// 88110 impl specific

#define	M88K_THREAD_STATE_MAXFLAVOR	(M88110_THREAD_STATE_IMPL)

/*
 * m88k_thread_state_grf -- basic thread state for NeXT 88K-based products
 */
typedef struct _m88k_thread_state_grf {
	unsigned	r1;		// rpc: return pc, caller-saved
	unsigned	r2;		// a0: argument 0, caller-saved
	unsigned	r3;		// a1
	unsigned	r4;		// a2
	unsigned	r5;		// a3
	unsigned	r6;		// a4
	unsigned	r7;		// a5
	unsigned	r8;		// a6
	unsigned	r9;		// a7
	unsigned	r10;		// t0: temporary, caller-saved
	unsigned	r11;		// t1
	unsigned	r12;		// t2: struct return ptr, 
	unsigned	r13;		// t3
	unsigned	r14;		// s0: saved, callee-saved
	unsigned	r15;		// s1
	unsigned	r16;		// s2
	unsigned	r17;		// s3
	unsigned	r18;		// s4
	unsigned	r19;		// s5
	unsigned	r20;		// s6
	unsigned	r21;		// s7
	unsigned	r22;		// s8
	unsigned	r23;		// s9
	unsigned	r24;		// s10
	unsigned	r25;		// s11
	unsigned	r26;		// t4
	unsigned	r27;		// at: temp, used by asm macros
	unsigned	r28;		// lk0: reserved for link editor
	unsigned	r29;		// lk1
	unsigned	r30;		// fp: frame ptr, callee-saved
	unsigned	r31;		// sp: stack ptr, callee-saved
	unsigned	xip;		// executing instruction pointer
	unsigned	xip_in_bd;	// non-zero => xip in branch delay slot
	/*
	 * nip is only valid if xip_in_bd is TRUE
	 */
	unsigned	nip;		// next instruction pointer
} m88k_thread_state_grf_t;

#define	M88K_THREAD_STATE_GRF_COUNT 	\
	(sizeof(m88k_thread_state_grf_t)/sizeof(int))

/*
 * m88k_thread_state_xrf -- extended register file contents and floating point
 * control registers for NeXT 88K-based products.
 */
typedef struct _m88k_thread_state_xrf {
	m88k_xrf_t	x1;			// caller-saved
	m88k_xrf_t	x2;
	m88k_xrf_t	x3;
	m88k_xrf_t	x4;
	m88k_xrf_t	x5;
	m88k_xrf_t	x6;
	m88k_xrf_t	x7;
	m88k_xrf_t	x8;
	m88k_xrf_t	x9;
	m88k_xrf_t	x10;
	m88k_xrf_t	x11;
	m88k_xrf_t	x12;
	m88k_xrf_t	x13;
	m88k_xrf_t	x14;
	m88k_xrf_t	x15;
	m88k_xrf_t	x16;
	m88k_xrf_t	x17;
	m88k_xrf_t	x18;
	m88k_xrf_t	x19;
	m88k_xrf_t	x20;
	m88k_xrf_t	x21;
	m88k_xrf_t	x22;			// callee-saved
	m88k_xrf_t	x23;
	m88k_xrf_t	x24;
	m88k_xrf_t	x25;
	m88k_xrf_t	x26;
	m88k_xrf_t	x27;
	m88k_xrf_t	x28;
	m88k_xrf_t	x29;
	m88k_xrf_t	x30;			// reserved
	m88k_xrf_t	x31;
	m88k_fpsr_t	fpsr;			// fp status, fcr62
	m88k_fpcr_t	fpcr;			// fp control, fcr63
} m88k_thread_state_xrf_t;

#define	M88K_THREAD_STATE_XRF_COUNT 		\
	(sizeof(m88k_thread_state_xrf_t)/sizeof(int))

typedef struct _m88k_thread_state_user {
	int		user;			// user register (for cthreads)
} m88k_thread_state_user_t;

#define M88K_THREAD_STATE_USER_COUNT 		\
	(sizeof(m88k_thread_state_user_t)/sizeof(int))

/*
 * Motorola 88110 specific state
 * (Can't count on this being in all m88k implementations.)
 */

#define	M88110_N_DATA_BP	2		// 88110 supports 2 data bp's

/*
 * Data Breakpoint Address Match Mask -- actually indicates don't
 * care bits in addr
 */
typedef enum {
	M88110_MATCH_BYTE = 0,
	M88110_MATCH_SHORT = 0x1,
	M88110_MATCH_WORD = 0x3,
	M88110_MATCH_DOUBLE = 0x7,
	M88110_MATCH_QUAD = 0xf,
	M88110_MATCH_32 = 0x1f,
	M88110_MATCH_64 = 0x3f,
	M88110_MATCH_128 = 0x7f,
	M88110_MATCH_256 = 0xff,
	M88110_MATCH_512 = 0x1ff,
	M88110_MATCH_1024 = 0x3ff,
	M88110_MATCH_2048 = 0x7ff,
	M88110_MATCH_4096 = 0xfff
} m88110_match_t;

/*
 * Data Breakpoint Control Word
 */
typedef	struct {
	unsigned	:BITS_WIDTH(31,29);
	unsigned	rw:BIT_WIDTH(28);		// 1 => read access
	unsigned	rwm:BIT_WIDTH(27);		// 0 => rw is don't care
	unsigned	:BITS_WIDTH(26,13);
	m88110_match_t	addr_match:BITS_WIDTH(12,1);	// addr(12,1) don't cares
	unsigned	v:BIT_WIDTH(0);
} m88110_bp_ctrl_t;

/*
 * A complete Data Breakpoint spec
 */
typedef	struct {
	unsigned		addr;		// data address
	m88110_bp_ctrl_t	ctrl;
} m88110_data_bp_t;

/*
 * m88110_psr_t -- 88110 Processor Status Register
 * System prohibits modification of supr, le, se, sgn_imd, sm and mxm_dis
 * bits for user threads.
 */
typedef struct {
	unsigned	supr:BIT_WIDTH(31);
	unsigned	le:BIT_WIDTH(30);	// little endian mode
	unsigned	se:BIT_WIDTH(29);	// serial exec mode
	unsigned	c:BIT_WIDTH(28);	// carry
	unsigned	:BIT_WIDTH(27);
	unsigned	sgn_imd:BIT_WIDTH(26);	// signed immediates
	unsigned	sm:BIT_WIDTH(25);	// serialize mem refs
	unsigned	:BIT_WIDTH(24);
	unsigned	trace:BIT_WIDTH(23);
	unsigned	:BITS_WIDTH(22,5);
	unsigned	sfu2dis:BIT_WIDTH(4);	// gpu (sfu2) disable
	unsigned	sfu1dis:BIT_WIDTH(3);	// fpu (sfu1) disable
	unsigned	mxm_dis:BIT_WIDTH(2);	// misaligned dis
	unsigned	:BITS_WIDTH(1,0);
} m88110_psr_t;

/*
 * Information for IEEE floating point user trap handlers
 */
typedef enum {
	M88110_IRESULT_SIZE_NONE = 0,		// no intermediate result
	M88110_IRESULT_SIZE_SINGLE = 1,		// single precision result
	M88110_IRESULT_SIZE_DOUBLE = 2,		// double precision result
	M88110_IRESULT_SIZE_EXTENDED = 3	// double extended result
} m88110_iresult_size_t;

typedef struct {
	unsigned	:BITS_WIDTH(31,16);		// unused
	m88110_iresult_size_t	iresult_size:BITS_WIDTH(15,14);
							// size of iresult
	unsigned	:BITS_WIDTH(13,9);		// unused
	unsigned	sfu1_disabled:BIT_WIDTH(8);	// sfu disabled
	unsigned	int:BIT_WIDTH(7);		// invalid int conv
	unsigned	unimp:BIT_WIDTH(6);		// unimp ctrl reg
	unsigned	priv:BIT_WIDTH(5);		// priv violation
	unsigned	efinv:BIT_WIDTH(4);		// IEEE EFINV
	unsigned	efdvz:BIT_WIDTH(3);		// IEEE EFDVZ
	unsigned	efunf:BIT_WIDTH(2);		// IEEE EFUNF
	unsigned	efovf:BIT_WIDTH(1);		// IEEE EFOVF
	unsigned	efinx:BIT_WIDTH(0);		// IEEE EFINX
} m88110_fp_trap_status_t;

/*
 * m88110_thread_state_impl -- 88110 implementation-specific
 * control registers for NeXT 88K-based products.
 */
typedef struct _m88110_thread_state_impl {
	m88110_data_bp_t	data_bp[M88110_N_DATA_BP];

	/*
	 * Certain of the 88110 psr bits may be modified
	 */
	m88110_psr_t		psr;		// processor status
	/*
	 * IEEE floating point user trap information.  Read only.
	 * (Only valid immediately after an EXC_ARITHMETIC
	 * exception with code EXC_M88K_SFU1_EXCP.  Trap
	 * handlers must determine operation, source and
	 * destination registers by fetching instruction at
	 * exip.)
	 */
	m88k_xrf_t		intermediate_result;
	m88110_fp_trap_status_t	fp_trap_status;
} m88110_thread_state_impl_t;

#define	M88110_THREAD_STATE_IMPL_COUNT	\
	(sizeof(m88110_thread_state_impl_t)/sizeof(int))

#endif	/* _MACH_M88K_THREAD_STATE_ */
