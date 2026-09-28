/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */
#ifndef _BSD_RISCV__TYPES_H_
#define _BSD_RISCV__TYPES_H_

#if defined (__riscv)

#if defined(KERNEL)
#ifdef XNU_KERNEL_PRIVATE
// Xcode doesn't set up kext search paths correctly, so the clang headers
// are not seen in the correct order to use their types.
#endif
#define USE_CLANG_TYPES 0
#else
#if defined(__has_feature) && __has_feature(modules)
#define USE_CLANG_TYPES 1
#else
#define USE_CLANG_TYPES 0
#endif
#endif

#if USE_CLANG_TYPES
#include <sys/_types/_ptrdiff_t.h>
#include <sys/_types/_size_t.h>
#include <sys/_types/_va_list.h>
#include <sys/_types/_wchar_t.h>
#endif

// This header file contains integer types. It may later also hold
// floating point and other arithmetic types.

#ifdef __GNUC__
typedef __signed char           __int8_t;
#else   /* !__GNUC__ */
typedef char                    __int8_t;
#endif  /* !__GNUC__ */
typedef unsigned char           __uint8_t;
typedef short                   __int16_t;
typedef unsigned short          __uint16_t;
typedef int                     __int32_t;
typedef unsigned int            __uint32_t;
typedef long long               __int64_t;
typedef unsigned long long      __uint64_t;

typedef long                    __darwin_intptr_t;
typedef unsigned int            __darwin_natural_t;

// The rune type is int, not unsigned long, so EOF (-1) fits and is*()/to*() can take it.
// NOTE: rune_t and wchar_t must be the same type. wint_t is at least 16 bits and also holds WEOF.

typedef int                     __darwin_ct_rune_t;     /* ct_rune_t */

// mbstate_t is an opaque object to keep conversion state during multibyte
// stream conversions. User programs must not reference its content.
typedef union {
	char            __mbstate8[128];
	long long       _mbstateL;                      /* for alignment */
} __mbstate_t;

typedef __mbstate_t             __darwin_mbstate_t;     /* mbstate_t */

#if USE_CLANG_TYPES
typedef ptrdiff_t               __darwin_ptrdiff_t;     /* ptr1 - ptr2 */
#elif defined(__PTRDIFF_TYPE__)
typedef __PTRDIFF_TYPE__        __darwin_ptrdiff_t;     /* ptr1 - ptr2 */
#elif defined(__LP64__)
typedef long                    __darwin_ptrdiff_t;     /* ptr1 - ptr2 */
#else
typedef int                     __darwin_ptrdiff_t;     /* ptr1 - ptr2 */
#endif /* __GNUC__ */

#if USE_CLANG_TYPES
typedef size_t                  __darwin_size_t;        /* sizeof() */
#elif defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__           __darwin_size_t;        /* sizeof() */
#else
typedef unsigned long           __darwin_size_t;        /* sizeof() */
#endif

#if USE_CLANG_TYPES
typedef va_list                 __darwin_va_list;       /* va_list */
#elif (__GNUC__ > 2)
typedef __builtin_va_list       __darwin_va_list;       /* va_list */
#else
typedef void *                  __darwin_va_list;       /* va_list */
#endif

#if USE_CLANG_TYPES
typedef wchar_t                 __darwin_wchar_t;       /* wchar_t */
#elif defined(__WCHAR_TYPE__)
typedef __WCHAR_TYPE__          __darwin_wchar_t;       /* wchar_t */
#else
typedef __darwin_ct_rune_t      __darwin_wchar_t;       /* wchar_t */
#endif

typedef __darwin_wchar_t        __darwin_rune_t;        /* rune_t */

#if defined(__WINT_TYPE__)
typedef __WINT_TYPE__           __darwin_wint_t;        /* wint_t */
#else
typedef __darwin_ct_rune_t      __darwin_wint_t;        /* wint_t */
#endif

typedef unsigned long           __darwin_clock_t;       /* clock() */
typedef __uint32_t              __darwin_socklen_t;     /* socklen_t (duh) */
typedef long                    __darwin_ssize_t;       /* byte count or error */
typedef long                    __darwin_time_t;        /* time() */

#undef USE_CLANG_TYPES

#endif /* defined (__riscv) */

#endif  /* _BSD_RISCV__TYPES_H_ */
