/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_ARM_SIGNAL_H_
#define _ARCH_ARM_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if defined(__arm__)
struct vregs
{
	ulong r0;
	ulong r1;
	ulong r2;
	ulong r3;
	ulong r4;
	ulong r5;
	ulong r6;
	ulong r7;
	ulong r8;
	ulong r9;
	ulong r10;
	ulong r11;
	ulong r12;
	ulong r13;	/* stack pointer */
	ulong r14;	/* link register */
	ulong r15;	/* program counter */
	ulong cpsr;

	double d[32];
	ulong fpscr;
};
#endif /* defined(__arm__) */


#endif /* _ARCH_ARM_SIGNAL_H_ */
