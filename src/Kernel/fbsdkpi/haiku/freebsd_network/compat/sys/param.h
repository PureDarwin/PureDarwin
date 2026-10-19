/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_PARAM_H_
#define _FBSD_COMPAT_SYS_PARAM_H_


#include <posix/sys/param.h>
#include <ByteOrder.h>

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/priority.h>


/* The version this compatibility layer is based on */
#ifdef PD_LINUXKPI
#define __FreeBSD_version 1600000
#else
#define __FreeBSD_version 1200086
#endif

#define MAXBSIZE	0x10000

#define PAGE_SHIFT	12
#define PAGE_MASK	(B_PAGE_SIZE - 1)

#define trunc_page(x)	((x) & ~PAGE_MASK)

#define ptoa(x)			((unsigned long)((x) << PAGE_SHIFT))
#define atop(x)			((unsigned long)((x) >> PAGE_SHIFT))

#ifndef MSIZE
#define MSIZE 256
#endif

#ifndef MCLSHIFT
#define MCLSHIFT 11
#endif

#define MCLBYTES		(1 << MCLSHIFT)

#define	MJUMPAGESIZE	B_PAGE_SIZE
#define	MJUM9BYTES		(9 * 1024)
#define	MJUM16BYTES		(16 * 1024)

#define ALIGN_BYTES		(sizeof(unsigned long) - 1)
#ifndef PD_LINUXKPI
#define ALIGN(x)		((((unsigned long)x) + ALIGN_BYTES) & ~ALIGN_BYTES)
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(__M68K__)
#define	ALIGNED_POINTER(p, t)	1
#elif defined(__powerpc__)
#define	ALIGNED_POINTER(p, t)	((((uintptr_t)(p)) & (sizeof (t) - 1)) == 0)
#elif defined(__arm__)
#define	ALIGNED_POINTER(p, t)	((((unsigned)(p)) & (sizeof(t) - 1)) == 0)
#elif defined(__mips__) || defined(__sparc__) \
	|| (defined(__riscv) && __riscv_xlen == 64) \
	|| defined(__aarch64__) || defined(__arm64__)
#define	ALIGNED_POINTER(p, t)	((((unsigned long)(p)) & (sizeof (t) - 1)) == 0)
#else
#error Need definition of ALIGNED_POINTER for this arch!
#endif

/* defined in arch_cpu.h which we can't include here as it's C++ */
#if defined(__x86_64__) || defined(__i386__) || defined(__arm__) \
	|| defined(__sparc__) || defined(__riscv) \
	|| defined(__aarch64__) || defined(__arm64__)
#define CACHE_LINE_SIZE 64
#elif defined(__powerpc__)
#define CACHE_LINE_SIZE 128
#elif defined(__M68K__)
#define CACHE_LINE_SIZE 16
#else
#error Need definition of CACHE_LINE_SIZE for this arch!
#endif

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#define roundup2(x, y)	(((x) + ((y) - 1)) & (~((y) - 1)))
#define rounddown(x, y)  (((x) / (y)) * (y))
#define rounddown2(x, y) ((x)&(~((y)-1)))          /* if y is power of two */
#define powerof2(x)	((((x)-1)&(x))==0)

#define	PRIMASK	0x0ff
#define	PCATCH	0x100
#define	PDROP	0x200
#define	PBDRY	0x400

#define	NBBY	8		/* number of bits in a byte */

/* Bit map related macros. */
#define	setbit(a,i)	(((unsigned char *)(a))[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	(((unsigned char *)(a))[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)							\
	(((const unsigned char *)(a))[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)							\
	((((const unsigned char *)(a))[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* byteswap macros */
#ifndef htonl
#	define htonl(x) B_HOST_TO_BENDIAN_INT32(x)
#	define ntohl(x) B_BENDIAN_TO_HOST_INT32(x)
#	define htons(x) B_HOST_TO_BENDIAN_INT16(x)
#	define ntohs(x) B_BENDIAN_TO_HOST_INT16(x)
#endif

#endif	/* _FBSD_COMPAT_SYS_PARAM_H_ */
