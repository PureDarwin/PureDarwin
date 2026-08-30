/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)endian.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_

#include <sys/cdefs.h>
#include <sys/_types.h>
#include <sys/_endian.h>

#ifndef DRIVERKIT
#include <libkern/_OSByteOrder.h>
#define __bswap16(x)    __DARWIN_OSSwapInt16(x)
#define __bswap32(x)    __DARWIN_OSSwapInt32(x)
#define __bswap64(x)    __DARWIN_OSSwapInt64(x)
#else /* DRIVERKIT */
#define __bswap16(x)    __builtin_bswap16(x)
#define __bswap32(x)    __builtin_bswap32(x)
#define __bswap64(x)    __builtin_bswap64(x)
#endif /* DRIVERKIT */

/*
 * General byte order swapping functions.
 */
#define bswap16(x)      __bswap16(x)
#define bswap32(x)      __bswap32(x)
#define bswap64(x)      __bswap64(x)

/*
 * Macros to convert to a specific endianness.
 */
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
#define htobe16(x)      __bswap16((x))
#define htobe32(x)      __bswap32((x))
#define htobe64(x)      __bswap64((x))
#define htole16(x)      ((__uint16_t)(x))
#define htole32(x)      ((__uint32_t)(x))
#define htole64(x)      ((__uint64_t)(x))

#define be16toh(x)      __bswap16((x))
#define be32toh(x)      __bswap32((x))
#define be64toh(x)      __bswap64((x))
#define le16toh(x)      ((__uint16_t)(x))
#define le32toh(x)      ((__uint32_t)(x))
#define le64toh(x)      ((__uint64_t)(x))
#else /* __DARWIN_BYTE_ORDER != __DARWIN_LITTLE_ENDIAN */
#define htobe16(x)      ((__uint16_t)(x))
#define htobe32(x)      ((__uint32_t)(x))
#define htobe64(x)      ((__uint64_t)(x))
#define htole16(x)      __bswap16((x))
#define htole32(x)      __bswap32((x))
#define htole64(x)      __bswap64((x))

#define be16toh(x)      ((__uint16_t)(x))
#define be32toh(x)      ((__uint32_t)(x))
#define be64toh(x)      ((__uint64_t)(x))
#define le16toh(x)      __bswap16((x))
#define le32toh(x)      __bswap32((x))
#define le64toh(x)      __bswap64((x))
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */

/*
 * Routines to encode/decode big- and little-endian multi-octet values
 * to/from an octet stream.
 */
static __inline __uint16_t
be16dec(const void *__sized_by(sizeof(__uint16_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return (__uint16_t)(p[0] << 8) | p[1];
}

static __inline __uint32_t
be32dec(const void *__sized_by(sizeof(__uint32_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return ((__uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static __inline __uint64_t
be64dec(const void *__sized_by(sizeof(__uint64_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return ((__uint64_t)be32dec(p) << 32) | be32dec(p + 4);
}

static __inline __uint16_t
le16dec(const void *__sized_by(sizeof(__uint16_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return (__uint16_t)(p[1] << 8) | p[0];
}

static __inline __uint32_t
le32dec(const void *__sized_by(sizeof(__uint32_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return ((__uint32_t)p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

static __inline __uint64_t
le64dec(const void *__sized_by(sizeof(__uint64_t)) pp)
{
	const __uint8_t *p = (const __uint8_t *)pp;

	return ((__uint64_t)le32dec(p + 4) << 32) | le32dec(p);
}

static __inline void
be16enc(void *__sized_by(sizeof(__uint16_t)) pp, __uint16_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	p[0] = (u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static __inline void
be32enc(void *__sized_by(sizeof(__uint32_t)) pp, __uint32_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static __inline void
be64enc(void *__sized_by(sizeof(__uint64_t)) pp, __uint64_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	be32enc(p, (__uint32_t)(u >> 32));
	be32enc(p + 4, (__uint32_t)(u & 0xffffffffU));
}

static __inline void
le16enc(void *__sized_by(sizeof(__uint16_t)) pp, __uint16_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static __inline void
le32enc(void *__sized_by(sizeof(__uint32_t)) pp, __uint32_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

static __inline void
le64enc(void *__sized_by(sizeof(__uint64_t)) pp, __uint64_t u)
{
	__uint8_t *p = (__uint8_t *)pp;

	le32enc(p, (__uint32_t)(u & 0xffffffffU));
	le32enc(p + 4, (__uint32_t)(u >> 32));
}

#endif  /* _SYS_ENDIAN_H_ */
