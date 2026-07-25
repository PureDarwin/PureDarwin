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

#endif /* ! __FOUNDATION_NSOBJCRUNTIME__ */
