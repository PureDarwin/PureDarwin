/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_X86_SIGNAL_H_
#define _ARCH_X86_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if __i386__

typedef struct packed_fp_stack {
	unsigned char	st0[10];
	unsigned char	st1[10];
	unsigned char	st2[10];
	unsigned char	st3[10];
	unsigned char	st4[10];
	unsigned char	st5[10];
	unsigned char	st6[10];
	unsigned char	st7[10];
} packed_fp_stack;

typedef struct packed_mmx_regs {
	unsigned char	mm0[10];
	unsigned char	mm1[10];
	unsigned char	mm2[10];
	unsigned char	mm3[10];
	unsigned char	mm4[10];
	unsigned char	mm5[10];
	unsigned char	mm6[10];
	unsigned char	mm7[10];
} packed_mmx_regs;

// The layout of this structure matches the one used by the FSAVE/FNSAVE instruction, so the
// context can be loaded directly in the FPU if that instruction is available.
typedef struct old_extended_regs {
	unsigned short	fp_control;
	unsigned short	_reserved1;
	unsigned short	fp_status;
	unsigned short	_reserved2;
	unsigned short	fp_tag;
	unsigned short	_reserved3;
	unsigned long	fp_eip;
	unsigned short	fp_cs;
	unsigned short	fp_opcode;
	unsigned long	fp_datap;
	unsigned short	fp_ds;
	unsigned short	_reserved4;
	union {
		packed_fp_stack	fp;
		packed_mmx_regs	mmx;
	} fp_mmx;
} old_extended_regs;

typedef struct fp_stack {
	unsigned char	st0[10];
	unsigned char	_reserved_42_47[6];
	unsigned char	st1[10];
	unsigned char	_reserved_58_63[6];
	unsigned char	st2[10];
	unsigned char	_reserved_74_79[6];
	unsigned char	st3[10];
	unsigned char	_reserved_90_95[6];
	unsigned char	st4[10];
	unsigned char	_reserved_106_111[6];
	unsigned char	st5[10];
	unsigned char	_reserved_122_127[6];
	unsigned char	st6[10];
	unsigned char	_reserved_138_143[6];
	unsigned char	st7[10];
	unsigned char	_reserved_154_159[6];
} fp_stack;

typedef struct mmx_regs {
	unsigned char	mm0[10];
	unsigned char	_reserved_42_47[6];
	unsigned char	mm1[10];
	unsigned char	_reserved_58_63[6];
	unsigned char	mm2[10];
	unsigned char	_reserved_74_79[6];
	unsigned char	mm3[10];
	unsigned char	_reserved_90_95[6];
	unsigned char	mm4[10];
	unsigned char	_reserved_106_111[6];
	unsigned char	mm5[10];
	unsigned char	_reserved_122_127[6];
	unsigned char	mm6[10];
	unsigned char	_reserved_138_143[6];
	unsigned char	mm7[10];
	unsigned char	_reserved_154_159[6];
} mmx_regs;

typedef struct xmmx_regs {
	unsigned char	xmm0[16];
	unsigned char	xmm1[16];
	unsigned char	xmm2[16];
	unsigned char	xmm3[16];
	unsigned char	xmm4[16];
	unsigned char	xmm5[16];
	unsigned char	xmm6[16];
	unsigned char	xmm7[16];
} xmmx_regs;

// The layout matches the one used by the FXSAVE instruction, so that the context can be loaded
// quickly into the FPU using that.
typedef struct new_extended_regs {
	unsigned short	fp_control;
	unsigned short	fp_status;
	unsigned short	fp_tag;
	unsigned short	fp_opcode;
	unsigned long	fp_eip;
	unsigned short	fp_cs;
	unsigned short	res_14_15;
	unsigned long	fp_datap;
	unsigned short	fp_ds;
	unsigned short	_reserved_22_23;
	unsigned long	mxcsr;
	unsigned long	mxcsr_mask;
	union {
		fp_stack fp;
		mmx_regs mmx;
	} fp_mmx;
	xmmx_regs xmmx;
	unsigned char	_reserved_288_463[176];

	// This area is explicitly not read and written by the XSAVE and FXSAVE instructions
	// according to Intel documentation. Which is good news, because we have a few more things
	// of our own to store...

	unsigned long		fault_address;
	unsigned long		error_code;
	unsigned short		cs;
	unsigned short		ds;
	unsigned short		es;
	unsigned short		fs;
	unsigned short		gs;
	unsigned short		ss;
	unsigned char		trap_number;

	unsigned char	_available_485_511[27];
} new_extended_regs;

typedef struct extended_regs {
	union {
		old_extended_regs	old_format;
		new_extended_regs	new_format;
	} state;
	unsigned long	format;
} extended_regs;

struct vregs {
	unsigned long			eip;
	unsigned long			eflags;
	unsigned long			eax;
	unsigned long			ecx;
	unsigned long			edx;
	unsigned long			esp;
	unsigned long			ebp;
	unsigned long			_reserved_1;
	extended_regs	xregs;
	unsigned long			edi;
	unsigned long			esi;
	unsigned long			ebx;
};

#endif /* __i386__ */

#endif /* _ARCH_X86_SIGNAL_H_ */
