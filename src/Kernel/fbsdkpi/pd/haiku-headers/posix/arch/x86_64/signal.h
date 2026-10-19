/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_X86_64_SIGNAL_H_
#define _ARCH_X86_64_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */


struct x86_64_fp_register {
	unsigned char value[10];
	unsigned char reserved[6];
};

struct x86_64_xmm_register {
	unsigned char value[16];
};


// The layout of this struct matches the one used by the FXSAVE instruction
struct fpu_state {
	unsigned short		control;
	unsigned short		status;
	unsigned short		tag;
	unsigned short		opcode;
	unsigned long		rip;
	unsigned long		rdp;
	unsigned int		mxcsr;
	unsigned int		mxcsr_mask;

	union {
		struct x86_64_fp_register fp[8];
		struct x86_64_fp_register mmx[8];
	};

	struct x86_64_xmm_register		xmm[16];
	unsigned char		_reserved_416_463[48];

	// This area is explicitly not read and written by the XSAVE and FXSAVE instructions
	// according to Intel documentation. Which is good news, because we have a few more things
	// of our own to store...

	unsigned long		fault_address;
	unsigned long		error_code;
	unsigned short		cs;
	unsigned short		ss;
	unsigned char		trap_number;

	unsigned char		_available_485_511[27];
};


struct xstate_hdr {
	unsigned long		bv;
	unsigned long		xcomp_bv;
	unsigned char		_reserved[48];
};


// The layout of this struct matches the one used by the XSAVE instruction on
// an AVX CPU
struct savefpu {
	struct fpu_state			fp_fxsave;
	struct xstate_hdr			fp_xstate;
	struct x86_64_xmm_register	fp_ymm[16];
		// The high half of the YMM registers, to combine with the low half
		// found in fp_fxsave.xmm
};


#ifdef __x86_64__


struct vregs {
	unsigned long		rax;
	unsigned long		rbx;
	unsigned long		rcx;
	unsigned long		rdx;
	unsigned long		rdi;
	unsigned long		rsi;
	unsigned long		rbp;
	unsigned long		r8;
	unsigned long		r9;
	unsigned long		r10;
	unsigned long		r11;
	unsigned long		r12;
	unsigned long		r13;
	unsigned long		r14;
	unsigned long		r15;

	unsigned long		rsp;
	unsigned long		rip;
	unsigned long		rflags;

	struct savefpu		fpu;
};


#endif


#endif /* _ARCH_X86_64_SIGNAL_H_ */
