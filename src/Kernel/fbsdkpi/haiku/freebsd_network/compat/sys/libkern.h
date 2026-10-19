/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_LIBKERN_H_
#define _FBSD_COMPAT_SYS_LIBKERN_H_


#include <strings.h>
#include <sys/cdefs.h>
#include <sys/types.h>


__BEGIN_DECLS


extern int random(void);
u_int read_random(void *, u_int);
void arc4rand(void *ptr, u_int len, int reseed);
uint32_t arc4random(void);
void arc4random_buf(void *ptr, size_t len);

int	 timingsafe_bcmp(const void *, const void *, size_t);

static __inline int imax(int a, int b) { return (a > b ? a : b); }
static __inline int imin(int a, int b) { return (a < b ? a : b); }
static __inline long lmax(long a, long b) { return (a > b ? a : b); }
static __inline long lmin(long a, long b) { return (a < b ? a : b); }

extern int abs(int a);


static __inline __pure2 int
fls(int mask)
{
	return (mask == 0 ? 0 :
		8 * sizeof(mask) - __builtin_clz((u_int)mask));
}

static __inline __pure2 int
flsl(long mask)
{
	return (mask == 0 ? 0 :
		8 * sizeof(mask) - __builtin_clzl((u_long)mask));
}

static __inline __pure2 int
flsll(long long mask)
{
	return (mask == 0 ? 0 :
		8 * sizeof(mask) - __builtin_clzll((unsigned long long)mask));
}

__END_DECLS

#endif /* _FBSD_COMPAT_SYS_LIBKERN_H_ */
