/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#include <string.h>
#include <mach/boolean.h>

#include <mach/boolean.h>
#include <machine/limits.h>
#include <kern/debug.h>
#include <san/kcov.h>

#include "kasan_internal.h"
#include "memintrinsics.h"

void
__asan_bcopy(const void *src, void *dst, size_t sz)
{
	kasan_check_range(src, sz, TYPE_MEMR);
	kasan_check_range(dst, sz, TYPE_MEMW);
	__nosan_bcopy(src, dst, sz);
}

void *
__asan_memmove(void *src, const void *dst, size_t sz)
{
	kasan_check_range(src, sz, TYPE_MEMR);
	kasan_check_range(dst, sz, TYPE_MEMW);
	return __nosan_memmove(src, dst, sz);
}

void *
__asan_memcpy(void *dst, const void *src, size_t sz)
{
	kasan_check_range(src, sz, TYPE_MEMR);
	kasan_check_range(dst, sz, TYPE_MEMW);
	return __nosan_memcpy(dst, src, sz);
}

void *
__asan_memset(void *dst, int c, size_t sz)
{
	kasan_check_range(dst, sz, TYPE_MEMW);
	return __nosan_memset(dst, c, sz);
}

void
__asan_bzero(void *dst, size_t sz)
{
	kasan_check_range(dst, sz, TYPE_MEMW);
	__nosan_bzero(dst, sz);
}

int
__asan_bcmp(const void *a, const void *b, size_t len)
{
	kasan_check_range(a, len, TYPE_MEMR);
	kasan_check_range(b, len, TYPE_MEMR);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_MEMCMP, a, len, b, len, false);
#endif
	return __nosan_bcmp(a, b, len);
}

int
__asan_memcmp(const void *a, const void *b, size_t n)
{
	kasan_check_range(a, n, TYPE_MEMR);
	kasan_check_range(b, n, TYPE_MEMR);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_MEMCMP, a, n, b, n, false);
#endif
	return __nosan_memcmp(a, b, n);
}

size_t
__asan_strlcpy(char *dst, const char *src, size_t sz)
{
	kasan_check_range(dst, sz, TYPE_STRW);
	return __nosan_strlcpy(dst, src, sz);
}

size_t
__asan_strlcat(char *dst, const char *src, size_t sz)
{
	kasan_check_range(dst, sz, TYPE_STRW);
	return __nosan_strlcat(dst, src, sz);
}

char *
__asan_strncpy(char *dst, const char *src, size_t sz)
{
	kasan_check_range(dst, sz, TYPE_STRW);
	return __nosan_strncpy(dst, src, sz);
}

char *
__asan_strncat(char *dst, const char *src, size_t sz)
{
	kasan_check_range(dst, strlen(dst) + sz + 1, TYPE_STRW);
	return __nosan_strncat(dst, src, sz);
}

size_t
__asan_strnlen(const char *src, size_t sz)
{
	size_t n = __nosan_strnlen(src, sz);
	if (n < sz) {
		sz = n + 1; // Include NUL
	}
	kasan_check_range(src, sz, TYPE_STRR);
	return n;
}

size_t
__asan_strlen(const char *src)
{
	size_t sz = __nosan_strlen(src);
	kasan_check_range(src, sz + 1, TYPE_STRR);
	return sz;
}

int
__asan_strcmp(const char *__null_terminated s1, const char *__null_terminated s2)
{
	size_t l1 = __asan_strlen(s1);
	size_t l2 = __asan_strlen(s2);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strcmp(s1, s2);
}

__ptrcheck_unavailable_r("strlcmp or strbufcmp")
int
__asan_strncmp(const char *__unsafe_indexable s1, const char *__unsafe_indexable s2, size_t n)
{
	size_t l1 = __asan_strnlen(s1, n);
	size_t l2 = __asan_strnlen(s2, n);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRNCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strncmp(s1, s2, n);
}

int
__asan_strlcmp(const char *__counted_by(n)s1, const char *s2, size_t n)
{
	size_t l1 = __asan_strnlen(s1, n);
	size_t l2 = __asan_strlen(s2);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRNCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strlcmp(s1, s2, n);
}

int
__asan_strbufcmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len)
{
	size_t l1 = __asan_strnlen(s1, s1len);
	size_t l2 = __asan_strnlen(s2, s2len);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRBUFCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strbufcmp(s1, s1len, s2, s2len);
}

int
__asan_strcasecmp(const char *__null_terminated s1, const char *__null_terminated s2)
{
	size_t l1 = __asan_strlen(s1);
	size_t l2 = __asan_strlen(s2);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strcasecmp(s1, s2);
}

__ptrcheck_unavailable_r("strlcasecmp or strbufcasecmp")
int
__asan_strncasecmp(const char *__unsafe_indexable s1, const char *__unsafe_indexable s2, size_t n)
{
	size_t l1 = __asan_strnlen(s1, n);
	size_t l2 = __asan_strnlen(s2, n);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRNCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strncasecmp(s1, s2, n);
}

int
__asan_strlcasecmp(const char *__counted_by(n)s1, const char *s2, size_t n)
{
	size_t l1 = __asan_strnlen(s1, n);
	size_t l2 = __asan_strlen(s2);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRNCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strlcasecmp(s1, s2, n);
}

int
__asan_strbufcasecmp(const char *__counted_by(s1len)s1, size_t s1len, const char *__counted_by(s2len)s2, size_t s2len)
{
	size_t l1 = __asan_strnlen(s1, s1len);
	size_t l2 = __asan_strnlen(s2, s2len);
#if CONFIG_KCOV
	kcov_trace_cmp_func(__builtin_return_address(0), KCOV_CMP_FUNC_STRBUFCMP, s1, l1, s2, l2, false);
#else
	(void)l1;
	(void)l2;
#endif
	return __nosan_strbufcasecmp(s1, s1len, s2, s2len);
}
