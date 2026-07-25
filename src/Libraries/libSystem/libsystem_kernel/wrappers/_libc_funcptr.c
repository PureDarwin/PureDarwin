/*
 * Copyright (c) 2010-2014 Apple Inc. All rights reserved.
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

#include "string/strings.h"
#include "_libkernel_init.h"

extern _libkernel_functions_t _libkernel_functions;
extern void mig_os_release(void* ptr);

#ifdef PD_LD64LLD_WEAK_LIBC_FUNCPTR
#define PD_LIBKERNEL_FUNCPTR_ATTR __attribute__((visibility("hidden"), weak))
#define PD_LIBKERNEL_FUNCPTR_EXPORT_ATTR __attribute__((weak))
#else
#define PD_LIBKERNEL_FUNCPTR_ATTR __attribute__((visibility("hidden")))
#define PD_LIBKERNEL_FUNCPTR_EXPORT_ATTR
#endif

/*
 * PureDarwin: these allocator wrappers dispatch through _libkernel_functions,
 * which is NULL until __libkernel_init() runs (during libSystem's initializer).
 * dyld links libsystem_kernel statically but never runs that init, and provides
 * its OWN malloc/free/realloc (a pre-libSystem pool allocator in dyldNew.cpp).
 * Marking these weak lets dyld's strong definitions win the link (otherwise this
 * archive member -- pulled in for the string dispatchers -- shadowed dyld's
 * malloc, so every dyld allocation dereferenced the NULL table: fault_addr=0x10,
 * the malloc slot offset). Normal libSystem clients still get these as the only
 * (weak) definition and route through the initialized table as before.
 */
__attribute__((visibility("hidden"), weak))
void *
malloc(size_t size)
{
	return _libkernel_functions->malloc(size);
}

__attribute__((visibility("hidden"), weak))
void
free(void *ptr)
{
	return _libkernel_functions->free(ptr);
}

__attribute__((visibility("hidden"), weak))
void *
realloc(void *ptr, size_t size)
{
	return _libkernel_functions->realloc(ptr, size);
}

PD_LIBKERNEL_FUNCPTR_ATTR
void
_pthread_exit_if_canceled(int error)
{
	return _libkernel_functions->_pthread_exit_if_canceled(error);
}

PD_LIBKERNEL_FUNCPTR_ATTR
void
_pthread_set_self(void *ptr __attribute__((__unused__)))
{
}

PD_LIBKERNEL_FUNCPTR_ATTR
void
_pthread_clear_qos_tsd(mach_port_t thread_port)
{
	if (_libkernel_functions->version >= 3 &&
	    _libkernel_functions->pthread_clear_qos_tsd) {
		return _libkernel_functions->pthread_clear_qos_tsd(thread_port);
	}
}

PD_LIBKERNEL_FUNCPTR_ATTR
int
pthread_current_stack_contains_np(const void *addr, size_t len)
{
	if (_libkernel_functions->version >= 4 &&
	    _libkernel_functions->pthread_current_stack_contains_np) {
		return _libkernel_functions->pthread_current_stack_contains_np(addr, len);
	}

	return 0;
}

/*
 * Upcalls to optimized libplatform string functions
 */

/*
 * PureDarwin: the upstream _libkernel_generic_string_functions table only
 * populated a subset of the slots -- on a real system the FULL optimized table
 * is installed by __libkernel_platform_init() (during libSystem's initializer)
 * before any string function is ever called, so the gaps never mattered. But
 * dyld links libsystem_kernel statically and NEVER runs platform_init, so it
 * uses this generic table as-is. Any dispatched string function whose slot was
 * left NULL (strncmp/strncpy/strnlen/strstr/memchr/memcmp/memccpy/strlcat) then
 * calls through a NULL pointer -> jump to 0. dyld's __guard_setup() calls
 * strncmp() during bootstrap, which crashed pid 1 with rip=0 before _main().
 * Provide portable generic implementations for every remaining slot so the
 * generic table is self-sufficient, matching what the optimized table supplies.
 */
__attribute__((visibility("hidden")))
static void *
_libkernel_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s;
	while (n--) {
		if (*p == (unsigned char)c)
			return (void *)p;
		p++;
	}
	return 0;
}

__attribute__((visibility("hidden")))
static int
_libkernel_memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *a = (const unsigned char *)s1;
	const unsigned char *b = (const unsigned char *)s2;
	while (n--) {
		if (*a != *b)
			return (int)*a - (int)*b;
		a++; b++;
	}
	return 0;
}

__attribute__((visibility("hidden")))
static void *
_libkernel_memccpy(void *__restrict dst, const void *__restrict src, int c, size_t n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n--) {
		unsigned char ch = *s++;
		*d++ = ch;
		if (ch == (unsigned char)c)
			return d;
	}
	return 0;
}

__attribute__((visibility("hidden")))
static size_t
_libkernel_generic_strnlen(const char *s, size_t maxlen)
{
	size_t i = 0;
	while (i < maxlen && s[i] != '\0')
		i++;
	return i;
}

__attribute__((visibility("hidden")))
static int
_libkernel_strncmp(const char *s1, const char *s2, size_t n)
{
	while (n--) {
		unsigned char c1 = (unsigned char)*s1++;
		unsigned char c2 = (unsigned char)*s2++;
		if (c1 != c2)
			return (int)c1 - (int)c2;
		if (c1 == '\0')
			break;
	}
	return 0;
}

__attribute__((visibility("hidden")))
static char *
_libkernel_strncpy(char *__restrict dst, const char *__restrict src, size_t maxlen)
{
	size_t i = 0;
	for (; i < maxlen && src[i] != '\0'; i++)
		dst[i] = src[i];
	for (; i < maxlen; i++)
		dst[i] = '\0';
	return dst;
}

__attribute__((visibility("hidden")))
static size_t
_libkernel_strlcat(char *__restrict dst, const char *__restrict src, size_t maxlen)
{
	size_t dlen = _libkernel_generic_strnlen(dst, maxlen);
	size_t slen = 0;
	while (src[slen] != '\0')
		slen++;
	if (dlen == maxlen)
		return maxlen + slen;
	size_t i = 0;
	while (src[i] != '\0' && dlen + i + 1 < maxlen) {
		dst[dlen + i] = src[i];
		i++;
	}
	dst[dlen + i] = '\0';
	return dlen + slen;
}

__attribute__((visibility("hidden")))
static char *
_libkernel_strstr(const char *s, const char *find)
{
	if (find[0] == '\0')
		return (char *)s;
	for (; *s != '\0'; s++) {
		const char *a = s;
		const char *b = find;
		while (*a != '\0' && *b != '\0' && *a == *b) {
			a++; b++;
		}
		if (*b == '\0')
			return (char *)s;
	}
	return 0;
}

static const struct _libkernel_string_functions
    _libkernel_generic_string_functions = {
	.version = 1,
	.bzero = _libkernel_bzero,
	.memchr = _libkernel_memchr,
	.memcmp = _libkernel_memcmp,
	.memmove = _libkernel_memmove,
	.memccpy = _libkernel_memccpy,
	.memset = _libkernel_memset,
	.strchr = _libkernel_strchr,
	.strcmp = _libkernel_strcmp,
	.strcpy = _libkernel_strcpy,
	.strlcat = _libkernel_strlcat,
	.strlcpy = _libkernel_strlcpy,
	.strlen = _libkernel_strlen,
	.strncmp = _libkernel_strncmp,
	.strncpy = _libkernel_strncpy,
	.strnlen = _libkernel_generic_strnlen,
	.strstr = _libkernel_strstr,
};
static _libkernel_string_functions_t _libkernel_string_functions =
    &_libkernel_generic_string_functions;

kern_return_t
__libkernel_platform_init(_libkernel_string_functions_t fns)
{
	_libkernel_string_functions = fns;
	return KERN_SUCCESS;
}

PD_LIBKERNEL_FUNCPTR_ATTR
void
bzero(void *s, size_t n)
{
	return _libkernel_string_functions->bzero(s, n);
}

PD_LIBKERNEL_FUNCPTR_ATTR
void
__bzero(void *s, size_t n)
{
	return _libkernel_string_functions->bzero(s, n);
}

/* Default (not hidden) visibility: libSystem.B exports memchr from here.
 * libplatform has no dedicated exported copy that gets pulled otherwise --
 * the -u root resolves to this archive first, so a hidden def would leave
 * the export list unsatisfiable ("cannot export hidden symbol"). */
void *
PD_LIBKERNEL_FUNCPTR_EXPORT_ATTR
memchr(const void *s, int c, size_t n)
{
	return _libkernel_string_functions->memchr(s, c, n);
}

PD_LIBKERNEL_FUNCPTR_ATTR
int
memcmp(const void *s1, const void *s2, size_t n)
{
	return _libkernel_string_functions->memcmp(s1, s2, n);
}

PD_LIBKERNEL_FUNCPTR_ATTR
void *
memmove(void *dst, const void *src, size_t n)
{
	return _libkernel_string_functions->memmove(dst, src, n);
}

PD_LIBKERNEL_FUNCPTR_ATTR
void *
memcpy(void *dst, const void *src, size_t n)
{
	return _libkernel_string_functions->memmove(dst, src, n);
}

/* memccpy: same problem as strlcpy/strnlen/index/strlcat -- libplatform
 * already has a genuinely public memccpy, so just drop this hidden dup
 * rather than add a third definition. */

PD_LIBKERNEL_FUNCPTR_ATTR
void *
memset(void *b, int c, size_t len)
{
	return _libkernel_string_functions->memset(b, c, len);
}

PD_LIBKERNEL_FUNCPTR_ATTR
char *
strchr(const char *s, int c)
{
	return _libkernel_string_functions->strchr(s, c);
}

/* index: see the strlcpy/strnlen comment further down -- same problem
 * (explicit visibility("hidden") here with no public alternative). Real
 * public index() now lives in stub/pd_libSystem_compat.c. */

PD_LIBKERNEL_FUNCPTR_ATTR
int
strcmp(const char *s1, const char *s2)
{
	return _libkernel_string_functions->strcmp(s1, s2);
}

PD_LIBKERNEL_FUNCPTR_EXPORT_ATTR
char *
strcpy(char * restrict dst, const char * restrict src)
{
	return _libkernel_string_functions->strcpy(dst, src);
}

/* strlcat: same problem as strlcpy/strnlen/index -- real public strlcat
 * now lives in libc/string/pd_strnlen.c. */

/*
 * strlcpy: no PD_LIBKERNEL_FUNCPTR_ATTR public wrapper here. That attribute
 * makes the symbol private_extern, and -exported_symbols_list can't undo
 * that at final link -- unlike strlen/strncmp below (which happen to also
 * get a genuinely public definition from libplatform, so whichever the
 * linker keeps is fine), strlcpy had no other public source, so it stayed
 * hidden no matter what the exports list said. The real public strlcpy now
 * lives in libc/string/pd_strnlen.c.
 */

PD_LIBKERNEL_FUNCPTR_ATTR
size_t
strlen(const char *str)
{
	return _libkernel_string_functions->strlen(str);
}

PD_LIBKERNEL_FUNCPTR_ATTR
int
strncmp(const char *s1, const char *s2, size_t n)
{
	return _libkernel_string_functions->strncmp(s1, s2, n);
}

PD_LIBKERNEL_FUNCPTR_EXPORT_ATTR
char *
strncpy(char * restrict dst, const char * restrict src, size_t maxlen)
{
	return _libkernel_string_functions->strncpy(dst, src, maxlen);
}

/* strnlen: see the strlcpy comment above -- same problem, same fix (real
 * public strnlen now lives in libc/string/pd_strnlen.c). */

PD_LIBKERNEL_FUNCPTR_ATTR
char *
strstr(const char *s, const char *find)
{
	return _libkernel_string_functions->strstr(s, find);
}

/*
 * mach/mach.h voucher_mach_msg API
 */

static const struct _libkernel_voucher_functions
    _libkernel_voucher_functions_empty;
static _libkernel_voucher_functions_t _libkernel_voucher_functions =
    &_libkernel_voucher_functions_empty;

kern_return_t
__libkernel_voucher_init(_libkernel_voucher_functions_t fns)
{
	_libkernel_voucher_functions = fns;
	return KERN_SUCCESS;
}

boolean_t
voucher_mach_msg_set(mach_msg_header_t *msg)
{
	if (_libkernel_voucher_functions->voucher_mach_msg_set) {
		return _libkernel_voucher_functions->voucher_mach_msg_set(msg);
	}
	return 0;
}

void
voucher_mach_msg_clear(mach_msg_header_t *msg)
{
	if (_libkernel_voucher_functions->voucher_mach_msg_clear) {
		return _libkernel_voucher_functions->voucher_mach_msg_clear(msg);
	}
}

voucher_mach_msg_state_t
voucher_mach_msg_adopt(mach_msg_header_t *msg)
{
	if (_libkernel_voucher_functions->voucher_mach_msg_adopt) {
		return _libkernel_voucher_functions->voucher_mach_msg_adopt(msg);
	}
	return VOUCHER_MACH_MSG_STATE_UNCHANGED;
}

void
voucher_mach_msg_revert(voucher_mach_msg_state_t state)
{
	if (_libkernel_voucher_functions->voucher_mach_msg_revert) {
		return _libkernel_voucher_functions->voucher_mach_msg_revert(state);
	}
}
