/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/ForFoundationOnly.h>

/* Every constructor here is a class convenience method, so the result must be
 * autoreleased - a boxed @(x) under ARC is released by its caller. */
static id
__NSNumberCreate(CFNumberType type, const void *value)
{
    CFNumberRef result = CFNumberCreate(kCFAllocatorDefault, type, value);
    return (id)CFAutorelease(result);
}

/* CFNumber has no boolean storage of its own; kCFBooleanTrue/False are the
 * canonical bridged objects and are what a CFDictionary round-trips. */
@implementation NSNumber

+ (instancetype)numberWithChar:(char)value {
    return __NSNumberCreate(kCFNumberCharType, &value);
}

+ (instancetype)numberWithUnsignedChar:(unsigned char)value {
    short widened = value;
    return __NSNumberCreate(kCFNumberShortType, &widened);
}

+ (instancetype)numberWithShort:(short)value {
    return __NSNumberCreate(kCFNumberShortType, &value);
}

+ (instancetype)numberWithUnsignedShort:(unsigned short)value {
    int widened = value;
    return __NSNumberCreate(kCFNumberIntType, &widened);
}

+ (instancetype)numberWithInt:(int)value {
    return __NSNumberCreate(kCFNumberIntType, &value);
}

+ (instancetype)numberWithUnsignedInt:(unsigned int)value {
    long long widened = value;
    return __NSNumberCreate(kCFNumberLongLongType, &widened);
}

+ (instancetype)numberWithLong:(long)value {
    return __NSNumberCreate(kCFNumberLongType, &value);
}

/* An unsigned long past LLONG_MAX cannot be represented; CFNumber is signed
 * throughout, so it wraps, exactly as Foundation's own NSNumber does. */
+ (instancetype)numberWithUnsignedLong:(unsigned long)value {
    long long widened = (long long)value;
    return __NSNumberCreate(kCFNumberLongLongType, &widened);
}

+ (instancetype)numberWithLongLong:(long long)value {
    return __NSNumberCreate(kCFNumberLongLongType, &value);
}

+ (instancetype)numberWithUnsignedLongLong:(unsigned long long)value {
    long long widened = (long long)value;
    return __NSNumberCreate(kCFNumberLongLongType, &widened);
}

+ (instancetype)numberWithFloat:(float)value {
    return __NSNumberCreate(kCFNumberFloatType, &value);
}

+ (instancetype)numberWithDouble:(double)value {
    return __NSNumberCreate(kCFNumberDoubleType, &value);
}

+ (instancetype)numberWithBool:(BOOL)value {
    return (id)(value ? kCFBooleanTrue : kCFBooleanFalse);
}

+ (instancetype)numberWithInteger:(NSInteger)value {
    return __NSNumberCreate(kCFNumberNSIntegerType, &value);
}

+ (instancetype)numberWithUnsignedInteger:(NSUInteger)value {
    long long widened = (long long)value;
    return __NSNumberCreate(kCFNumberLongLongType, &widened);
}

/* CFNumberGetValue converts, and reports false when the value did not fit.
 * The result is still the truncated conversion, which is what NSNumber
 * promises for a lossy read, so the return value is deliberately ignored. */
#define __NSNUMBER_GETTER(name, type, cfType)                       \
    - (type)name {                                                  \
        type result = 0;                                            \
        CFNumberGetValue((CFNumberRef)self, cfType, &result);       \
        return result;                                              \
    }

__NSNUMBER_GETTER(charValue, char, kCFNumberCharType)
__NSNUMBER_GETTER(shortValue, short, kCFNumberShortType)
__NSNUMBER_GETTER(intValue, int, kCFNumberIntType)
__NSNUMBER_GETTER(longValue, long, kCFNumberLongType)
__NSNUMBER_GETTER(longLongValue, long long, kCFNumberLongLongType)
__NSNUMBER_GETTER(floatValue, float, kCFNumberFloatType)
__NSNUMBER_GETTER(doubleValue, double, kCFNumberDoubleType)
__NSNUMBER_GETTER(integerValue, NSInteger, kCFNumberNSIntegerType)

- (unsigned char)unsignedCharValue {
    return (unsigned char)[self charValue];
}

- (unsigned short)unsignedShortValue {
    return (unsigned short)[self shortValue];
}

- (unsigned int)unsignedIntValue {
    return (unsigned int)[self intValue];
}

- (unsigned long)unsignedLongValue {
    return (unsigned long)[self longValue];
}

- (unsigned long long)unsignedLongLongValue {
    return (unsigned long long)[self longLongValue];
}

- (NSUInteger)unsignedIntegerValue {
    return (NSUInteger)[self integerValue];
}

/* CFCopyDescription of a CFNumber is the number itself, with no decoration. */
- (NSString *)stringValue {
    CFStringRef result = CFCopyDescription((CFTypeRef)self);
    return (NSString *)CFAutorelease(result);
}

- (BOOL)boolValue {
    return [self longLongValue] != 0;
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFNumberBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFNumberGetTypeID(), "NSNumber");
}
#endif
