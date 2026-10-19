/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_TYPES_H_
#define _FBSD_COMPAT_SYS_TYPES_H_


#include <posix/stdint.h>
#include <posix/sys/types.h>

#include <sys/cdefs.h>

#include <machine/endian.h>
#include <sys/_types.h>


typedef int boolean_t;
typedef __const char* c_caddr_t;
typedef uint64_t u_quad_t;


#ifdef __POPCNT__
#define	__bitcount64(x)	__builtin_popcountll((u_int64_t)(x))
#define	__bitcount32(x)	__builtin_popcount((u_int32_t)(x))
#define	__bitcount16(x)	__builtin_popcount((u_int16_t)(x))
#define	__bitcountl(x)	__builtin_popcountl((unsigned long)(x))
#define	__bitcount(x)	__builtin_popcount((unsigned int)(x))
#else
/*
 * Population count algorithm using SWAR approach
 * - "SIMD Within A Register".
 */
static __inline u_int16_t
__bitcount16(u_int16_t _x)
{

	_x = (_x & 0x5555) + ((_x & 0xaaaa) >> 1);
	_x = (_x & 0x3333) + ((_x & 0xcccc) >> 2);
	_x = (_x + (_x >> 4)) & 0x0f0f;
	_x = (_x + (_x >> 8)) & 0x00ff;
	return (_x);
}

static __inline u_int32_t
__bitcount32(u_int32_t _x)
{

	_x = (_x & 0x55555555) + ((_x & 0xaaaaaaaa) >> 1);
	_x = (_x & 0x33333333) + ((_x & 0xcccccccc) >> 2);
	_x = (_x + (_x >> 4)) & 0x0f0f0f0f;
	_x = (_x + (_x >> 8));
	_x = (_x + (_x >> 16)) & 0x000000ff;
	return (_x);
}

#ifdef __LP64__
static __inline u_int64_t
__bitcount64(u_int64_t _x)
{

	_x = (_x & 0x5555555555555555) + ((_x & 0xaaaaaaaaaaaaaaaa) >> 1);
	_x = (_x & 0x3333333333333333) + ((_x & 0xcccccccccccccccc) >> 2);
	_x = (_x + (_x >> 4)) & 0x0f0f0f0f0f0f0f0f;
	_x = (_x + (_x >> 8));
	_x = (_x + (_x >> 16));
	_x = (_x + (_x >> 32)) & 0x000000ff;
	return (_x);
}

#define	__bitcountl(x)	__bitcount64((unsigned long)(x))
#else
static __inline u_int64_t
__bitcount64(u_int64_t _x)
{

	return (__bitcount32(_x >> 32) + __bitcount32(_x));
}

#define	__bitcountl(x)	__bitcount32((unsigned long)(x))
#endif
#define	__bitcount(x)	__bitcount32((unsigned int)(x))
#endif


#endif
