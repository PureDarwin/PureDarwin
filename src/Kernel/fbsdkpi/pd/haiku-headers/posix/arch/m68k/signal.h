/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_M68K_SIGNAL_H_
#define _ARCH_M68K_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if __M68K__
struct vregs
{
	ulong	pc,                                         /* program counter */
		d0, d1, d2, d3, d4, d5, d6, d7,
		a0, a1, a2, a3, a4, a5, a6, a7;
	unchar	ccr;
/* TODO: #warning M68K: fix floats in vregs, add missing stuff. */
   double f0,                                         /* fp scratch */
	      f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13; /* fp volatile regs */
};
#endif /* __M68K__ */


#endif /* _ARCH_M68K_SIGNAL_H_ */
