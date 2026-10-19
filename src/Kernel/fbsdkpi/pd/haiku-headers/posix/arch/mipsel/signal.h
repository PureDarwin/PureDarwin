/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_MIPSEL_SIGNAL_H_
#define _ARCH_MIPSEL_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if __MIPSEL__
struct vregs
{
	ulong r0;
	/* r1, r2, ... */

#warning MIPSEL: fixme
};
#endif /* __MIPSEL__ */


#endif /* _ARCH_MIPSEL_SIGNAL_H_ */
