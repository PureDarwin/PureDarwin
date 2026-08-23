/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if !defined(__FOUNDATION_NSOBJCRUNTIME__)
#define __FOUNDATION_NSOBJCRUNTIME__ 1

#include <objc/NSObjCRuntime.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

#ifndef NS_INLINE
    #define NS_INLINE static __inline__ __attribute__((always_inline))
#endif

#ifndef NS_RETURNS_INNER_POINTER
    #define NS_RETURNS_INNER_POINTER __attribute__((objc_returns_inner_pointer))
#endif

#ifndef NS_REQUIRES_SUPER
    #define NS_REQUIRES_SUPER __attribute__((objc_requires_super))
#endif

#ifndef NS_DESIGNATED_INITIALIZER
    #define NS_DESIGNATED_INITIALIZER __attribute__((objc_designated_initializer))
#endif

/* Ownership annotations: a +1 return from a method whose name does not begin
 * with alloc/new/copy, and the reverse. */
#ifndef NS_RETURNS_RETAINED
    #define NS_RETURNS_RETAINED __attribute__((ns_returns_retained))
#endif
#ifndef NS_RETURNS_NOT_RETAINED
    #define NS_RETURNS_NOT_RETAINED __attribute__((ns_returns_not_retained))
#endif
#ifndef NS_CONSUMED
    #define NS_CONSUMED __attribute__((ns_consumed))
#endif
#ifndef NS_CONSUMES_SELF
    #define NS_CONSUMES_SELF __attribute__((ns_consumes_self))
#endif

typedef struct _NSZone NSZone;

#ifndef __has_attribute
    #define __has_attribute(x) 0
#endif

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

#ifndef __has_feature
    #define __has_feature(x) 0
#endif

/*
 * XX_ENUM & XX_OPTIONS macros, courtesy of CoreFoundation
 */
#include <CoreFoundation/CFAvailability.h>

#define _NS_TYPED_ENUM              _CF_TYPED_ENUM
#define _NS_TYPED_EXTENSIBLE_ENUM   _CF_TYPED_EXTENSIBLE_ENUM

#define NS_ENUM(...)                CF_ENUM(__VA_ARGS__)
#define NS_OPTIONS(type, name)      CF_OPTIONS(type, name)
#define NS_CLOSED_ENUM(type, name)  CF_CLOSED_ENUM(type, name)
#define NS_TYPED_ENUM               _NS_TYPED_ENUM
#define NS_TYPED_EXTENSIBLE_ENUM    _NS_TYPED_EXTENSIBLE_ENUM
#define NS_STRING_ENUM              _NS_TYPED_ENUM
#define NS_EXTENSIBLE_STRING_ENUM   _NS_TYPED_EXTENSIBLE_ENUM


/*
 * While we're here...
 *
 * We really don't care about availability. If it's there it's there. If it isn't; well damn.
 */
#define NS_AVAILABLE(...)
#define NS_AVAILABLE_MAC(...)
#define NS_AVAILABLE_IOS(...)

#define NS_DEPRECATED(...)
#define NS_DEPRECATED_MAC(...)
#define NS_DEPRECATED_IOS(...)

#define NS_ENUM_AVAILABLE(...)
#define NS_ENUM_AVAILABLE_MAC(...)
#define NS_ENUM_AVAILABLE_IOS(...)

#define NS_ENUM_DEPRECATED(...)
#define NS_ENUM_DEPRECATED_MAC(...)
#define NS_ENUM_DEPRECATED_IOS(...)

#ifndef FOUNDATION_EXPORT
    #ifdef __cplusplus
        #define FOUNDATION_EXPORT extern "C"
    #else
        #define FOUNDATION_EXPORT extern
    #endif
#endif

#ifndef FOUNDATION_EXTERN
    #define FOUNDATION_EXTERN FOUNDATION_EXPORT
#endif

typedef NS_CLOSED_ENUM(NSInteger, NSComparisonResult) {
    NSOrderedAscending = -1,
    NSOrderedSame = 0,
    NSOrderedDescending = 1,
};

enum { NSNotFound = NSIntegerMax };

/* Apple's NSObjCRuntime.h defines these, and ported ObjC code relies on
 * getting them from Foundation rather than from a system header. */
#if !defined(MIN)
    #define MIN(A, B) __NSMinOrMax(A, B, <)
#endif
#if !defined(MAX)
    #define MAX(A, B) __NSMinOrMax(A, B, >)
#endif
#if !defined(ABS)
    #define ABS(A) ({ __typeof__(A) __a = (A); __a < 0 ? -__a : __a; })
#endif

#define __NSMinOrMax(A, B, OP) ({ \
    __typeof__(A) __a = (A); \
    __typeof__(B) __b = (B); \
    __a OP __b ? __a : __b; \
})

@class NSString;

void NSLog(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));
void NSLogv(NSString *format, va_list args) __attribute__((format(__NSString__, 1, 0)));

FOUNDATION_EXPORT NSString *NSStringFromSelector(SEL selector);
FOUNDATION_EXPORT SEL NSSelectorFromString(NSString *name);
FOUNDATION_EXPORT NSString *NSStringFromClass(Class aClass);
FOUNDATION_EXPORT Class NSClassFromString(NSString *name);
FOUNDATION_EXPORT NSString *NSStringFromProtocol(Protocol *protocol);
FOUNDATION_EXPORT Protocol *NSProtocolFromString(NSString *name);

FOUNDATION_EXPORT const char *NSGetSizeAndAlignment(const char *typePtr,
                                                    NSUInteger *sizep,
                                                    NSUInteger *alignp);

#endif /* ! __FOUNDATION_NSOBJCRUNTIME__ */
