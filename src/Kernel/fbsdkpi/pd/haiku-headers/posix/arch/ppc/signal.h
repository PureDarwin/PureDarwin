/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_PPC_SIGNAL_H_
#define _ARCH_PPC_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if __POWERPC__
struct vregs
{
	ulong pc,                                         /* program counter */
	      r0,                                         /* scratch */
	      r1,                                         /* stack ptr */
	      r2,                                         /* TOC */
	      r3,r4,r5,r6,r7,r8,r9,r10,                   /* volatile regs */
	      r11,r12;                                    /* scratch regs */

   double f0,                                         /* fp scratch */
	      f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13; /* fp volatile regs */

	ulong filler1,                                    /* place holder */
	      fpscr,                                      /* fp condition codes */
	      ctr, xer, cr, msr, lr;                      /* misc. status */
};
#endif /* __POWERPC__ */


#endif /* _ARCH_PPC_SIGNAL_H_ */
