/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef _ARCH_ARM64_SIGNAL_H_
#define _ARCH_ARM64_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if defined(__aarch64__)
struct vregs
{
	ulong		x[30];
	ulong		lr;
	ulong		sp;
	ulong		elr;
	ulong		spsr;
	__uint128_t	fp_q[32];
	u_int		fpsr;
	u_int		fpcr;
};
#endif /* defined(__aarch64__) */


#endif /* _ARCH_ARM64_SIGNAL_H_ */
