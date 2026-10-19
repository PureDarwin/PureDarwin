/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_SPARC64_SIGNAL_H_
#define _ARCH_SPARC64_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if __sparc64__

struct vregs
{
	// ulong g0; // always 0, so no need to save
	ulong g1;
	ulong g2;
	ulong g3;
	ulong g4;
	ulong g5;
	ulong g6;
	ulong g7;
	ulong o0;
	ulong o1;
	ulong o2;
	ulong o3;
	ulong o4;
	ulong o5;
	ulong sp;
	ulong o7;
	ulong l0;
	ulong l1;
	ulong l2;
	ulong l3;
	ulong l4;
	ulong l5;
	ulong l6;
	ulong l7;
	ulong i0;
	ulong i1;
	ulong i2;
	ulong i3;
	ulong i4;
	ulong i5;
	ulong fp;
	ulong i7;
	// TODO: sparc: Fix floats in vregs
};


#endif /* __sparc64__ */

#endif /* _ARCH_SPARC64_SIGNAL_H_ */
