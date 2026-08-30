/*
 * Copyright (c) 2016-2020 Apple Inc. All rights reserved.
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
#ifndef _SAN_MEMINTRINSICS_H_
#define _SAN_MEMINTRINSICS_H_

#include <sys/cdefs.h>

/*
 * Non-sanitized versions of memory intrinsics
 */
static inline void *
__sized_by(sz)
__nosan_memcpy(void *dst __sized_by(sz), const void *src __sized_by(sz), size_t sz)
{
	return memcpy(dst, src, sz);
}
static inline void *
__sized_by(sz)
__nosan_memset(void *dst __sized_by(sz), int c, size_t sz)
{
	return memset(dst, c, sz);
}
static inline void *
__sized_by(sz)
__nosan_memmove(void *dst __sized_by(sz), const void *src __sized_by(sz), size_t sz)
{
	return memmove(dst, src, sz);
}
static inline int
__nosan_bcmp(const void *a __sized_by(sz), const void *b __sized_by(sz), size_t sz)
{
	return bcmp(a, b, sz);
}
static inline void
__nosan_bcopy(const void *src __sized_by(sz), void *dst __sized_by(sz), size_t sz)
{
	bcopy(src, dst, sz);
}
static inline int
__nosan_memcmp(const void *a __sized_by(sz), const void *b __sized_by(sz), size_t sz)
{
	return memcmp(a, b, sz);
}
static inline void
__nosan_bzero(void *dst __sized_by(sz), size_t sz)
{
	bzero(dst, sz);
}

static inline size_t
__nosan_strlcpy(char *__sized_by(sz)dst, const char *__null_terminated src, size_t sz)
{
	return strlcpy(dst, src, sz);
}
static inline size_t
__nosan_strlcat(char *__sized_by(sz)dst, const char *__null_terminated src, size_t sz)
{
	return strlcat(dst, src, sz);
}
static inline size_t
__nosan_strnlen(const char *__counted_by(sz)src, size_t sz)
{
	return strnlen(src, sz);
}
static inline size_t
__nosan_strlen(const char *__null_terminated src)
{
	return strlen(src);
}
static inline int
__nosan_strcmp(const char *__null_terminated s1, const char *__null_terminated s2)
{
	return strcmp(s1, s2);
}
static inline int
__nosan_strncmp(const char *__unsafe_indexable s1, const char *__unsafe_indexable s2, size_t n)
{
	return strbufcmp(__unsafe_forge_bidi_indexable(const char *, s1, n), n, __unsafe_forge_bidi_indexable(const char *, s2, n), n);
}
static inline int
__nosan_strlcmp(const char *__counted_by(n)s1, const char *s2, size_t n)
{
	return strlcmp(s1, s2, n);
}
static inline int
__nosan_strbufcmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len)
{
	return strbufcmp(s1, s1len, s2, s2len);
}
static inline int
__nosan_strcasecmp(const char *__null_terminated s1, const char *__null_terminated s2)
{
	return strcasecmp(s1, s2);
}
static inline int
__nosan_strncasecmp(const char *__unsafe_indexable s1, const char *__unsafe_indexable s2, size_t n)
{
	return strbufcasecmp(__unsafe_forge_bidi_indexable(const char *, s1, n), n, __unsafe_forge_bidi_indexable(const char *, s2, n), n);
}
static inline int
__nosan_strlcasecmp(const char *__counted_by(n)s1, const char *s2, size_t n)
{
	return strlcasecmp(s1, s2, n);
}
static inline int
__nosan_strbufcasecmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len)
{
	return strbufcasecmp(s1, s1len, s2, s2len);
}
#if !__has_ptrcheck && !__has_include(<__xnu_libcxx_sentinel.h>)
static inline char *
__nosan_strncpy(char *dst, const char *src, size_t sz)
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	return strncpy(dst, src, sz);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
static inline char *
__nosan_strncat(char *dst, const char *src, size_t sz)
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	return strncat(dst, src, sz);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
#endif /* !__has_ptrcheck && !__has_include(<__xnu_libcxx_sentinel.h>) */

#if KASAN
void *__sized_by(sz) __asan_memcpy(void *dst __sized_by(sz), const void *src __sized_by(sz), size_t sz);
void *__sized_by(sz) __asan_memset(void * __sized_by(sz), int c, size_t sz);
void *__sized_by(sz) __asan_memmove(void *dst __sized_by(sz), const void *src __sized_by(sz), size_t sz);
void  __asan_bcopy(const void *src __sized_by(sz), void *dst __sized_by(sz), size_t sz);
void  __asan_bzero(void *dst __sized_by(sz), size_t sz);
int   __asan_bcmp(const void *a __sized_by(sz), const void *b __sized_by(sz), size_t sz) __stateful_pure;
int   __asan_memcmp(const void *a __sized_by(sz), const void *b __sized_by(sz), size_t sz) __stateful_pure;

size_t __asan_strlcpy(char *__sized_by(sz) dst, const char *__null_terminated src, size_t sz);
char  *__asan_strncpy(char *dst, const char *src, size_t sz);
char  *__asan_strncat(char *dst, const char *src, size_t sz);
size_t __asan_strlcat(char *__sized_by(sz) dst, const char *__null_terminated src, size_t sz);
size_t __asan_strnlen(const char *__counted_by(n)s, size_t n) __stateful_pure;
size_t __asan_strlen(const char *__null_terminated src) __stateful_pure;
int __asan_strcmp(const char *__null_terminated s1, const char *__null_terminated s2) __stateful_pure;
__ptrcheck_unavailable_r("strlcmp or strbufcmp")
int __asan_strncmp(const char *__null_terminated s1, const char *__null_terminated s2, size_t n) __stateful_pure;
int __asan_strlcmp(const char *__counted_by(n)s1, const char *s2, size_t n) __stateful_pure;
int __asan_strbufcmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len) __stateful_pure;
int __asan_strcasecmp(const char *__null_terminated s1, const char *__null_terminated s2) __stateful_pure;
__ptrcheck_unavailable_r("strlcasecmp or strbufcasecmp")
int __asan_strncasecmp(const char *__unsafe_indexable s1, const char *__unsafe_indexable s2, size_t n) __stateful_pure;
int __asan_strlcasecmp(const char *__counted_by(n)s1, const char *s2, size_t n) __stateful_pure;
int __asan_strbufcasecmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len) __stateful_pure;

#define memcpy      __asan_memcpy
#define memmove     __asan_memmove
#define memset      __asan_memset
#define bcopy       __asan_bcopy
#define bzero       __asan_bzero
#define bcmp        __asan_bcmp
#define memcmp      __asan_memcmp

#define strlcpy     __asan_strlcpy
#define strncpy     __asan_strncpy
#define strlcat     __asan_strlcat
#define strncat     __asan_strncat
#define strnlen     __asan_strnlen
#define strlen      __asan_strlen
#define strcmp      __asan_strcmp
#define strncmp     __asan_strncmp
#define strlcmp     __asan_strlcmp
#define strcasecmp  __asan_strcasecmp
#define strncasecmp __asan_strncasecmp
#define strlcasecmp __asan_strlcasecmp

// Previously defined as macros in string.h
#undef strbuflen_1
#undef strbuflen_2
#undef strbuflen
#undef strbufcmp_2
#undef strbufcmp_4
#undef strbufcmp
#undef strbufcasecmp_2
#undef strbufcasecmp_4
#undef strbufcasecmp

#define strbuflen_1(BUF) ({ \
     __xnu_assert_is_array(BUF, "argument is not an array"); \
     __asan_strnlen((BUF), sizeof(BUF)); \
})
#define strbuflen_2(BUF, LEN) __asan_strnlen(BUF, LEN)
#define strbuflen(...) __xnu_argc_overload(strbuflen, __VA_ARGS__)

#define strbufcmp_2(A, B) ({ \
	__xnu_assert_is_array(A, "first argument is not an array"); \
	__xnu_assert_is_array(B, "second argument is not an array"); \
	(__asan_strbufcmp)((A), sizeof(A), (B), sizeof(B)); \
})
#define strbufcmp_4 (__asan_strbufcmp)
#define strbufcmp(...) __xnu_argc_overload(strbufcmp, __VA_ARGS__)

#define strbufcasecmp_2(A, B) ({ \
	__xnu_assert_is_array(A, "first argument is not an array"); \
	__xnu_assert_is_array(B, "second argument is not an array"); \
	(__asan_strbufcasecmp)((A), sizeof(A), (B), sizeof(B)); \
})
#define strbufcasecmp_4 (__asan_strbufcasecmp)
#define strbufcasecmp(...) __xnu_argc_overload(strbufcasecmp, __VA_ARGS__)

#endif

#endif /* _SAN_MEMINTRINSICS_H_ */
