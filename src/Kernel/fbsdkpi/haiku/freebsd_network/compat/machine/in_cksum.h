/*
 * Copyright 2007, Axel Dörfler, axeld@pinc-software.de. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_MACHINE_IN_CKSUM_H_
#define _FBSD_COMPAT_MACHINE_IN_CKSUM_H_


#include <stdint.h>


#define in_cksum(m, len)	in_cksum_skip(m, len, 0)


static inline u_short
in_pseudo(u_int sum, u_int b, u_int c)
{
	// should never be called
	panic("in_pseudo() called");
	return 0;
}


static inline u_short
in_cksum_skip(struct mbuf* m, int len, int skip)
{
	// should never be called
	panic("in_cksum_skip() called");
	return 0;
}

#endif	/* _FBSD_COMPAT_MACHINE_IN_CKSUM_H_ */
