/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNumber.h>
#import <Foundation/NSException.h>
#include <CoreFoundation/CFBase.h>
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

/* The -initWith... forms were missing entirely, so [[NSNumber alloc]
 * initWithInt:] raised "unrecognized selector". They must return a retained
 * object, unlike the +numberWith... forms above: the caller owns the result
 * and releases it. The freshly allocated shell is released first, as NSData's
 * initialisers do, so it is not leaked. */
static id __NSNumberInit(CFNumberType type, const void *value) {
    return (id)CFNumberCreate(kCFAllocatorDefault, type, value);
}

- (instancetype)initWithChar:(char)value {
    [self release];
    return __NSNumberInit(kCFNumberCharType, &value);
}

- (instancetype)initWithUnsignedChar:(unsigned char)value {
    short widened = value;
    [self release];
    return __NSNumberInit(kCFNumberShortType, &widened);
}

- (instancetype)initWithShort:(short)value {
    [self release];
    return __NSNumberInit(kCFNumberShortType, &value);
}

- (instancetype)initWithUnsignedShort:(unsigned short)value {
    int widened = value;
    [self release];
    return __NSNumberInit(kCFNumberIntType, &widened);
}

- (instancetype)initWithInt:(int)value {
    [self release];
    return __NSNumberInit(kCFNumberIntType, &value);
}

- (instancetype)initWithUnsignedInt:(unsigned int)value {
    long long widened = value;
    [self release];
    return __NSNumberInit(kCFNumberLongLongType, &widened);
}

- (instancetype)initWithLong:(long)value {
    [self release];
    return __NSNumberInit(kCFNumberLongType, &value);
}

- (instancetype)initWithUnsignedLong:(unsigned long)value {
    long long widened = (long long)value;
    [self release];
    return __NSNumberInit(kCFNumberLongLongType, &widened);
}

- (instancetype)initWithLongLong:(long long)value {
    [self release];
    return __NSNumberInit(kCFNumberLongLongType, &value);
}

- (instancetype)initWithUnsignedLongLong:(unsigned long long)value {
    long long widened = (long long)value;
    [self release];
    return __NSNumberInit(kCFNumberLongLongType, &widened);
}

- (instancetype)initWithFloat:(float)value {
    [self release];
    return __NSNumberInit(kCFNumberFloatType, &value);
}

- (instancetype)initWithDouble:(double)value {
    [self release];
    return __NSNumberInit(kCFNumberDoubleType, &value);
}

/* The booleans are immortal constants, so retaining is a no-op and releasing
 * one is harmless - the caller's release balances correctly either way. */
- (instancetype)initWithBool:(BOOL)value {
    [self release];
    return (id)CFRetain(value ? kCFBooleanTrue : kCFBooleanFalse);
}

- (instancetype)initWithInteger:(NSInteger)value {
    [self release];
    return __NSNumberInit(kCFNumberNSIntegerType, &value);
}

- (instancetype)initWithUnsignedInteger:(NSUInteger)value {
    long long widened = (long long)value;
    [self release];
    return __NSNumberInit(kCFNumberLongLongType, &widened);
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

// the value as text, like Foundation's ("3", "3.5", "1" for YES), since CFCopyDescription gives
// the decorated "<CFNumber 0x... [...]>{value = ...}" form
- (NSString *)stringValue {
    if (CFGetTypeID((CFTypeRef)self) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)self) ? @"1" : @"0";
    }
    if (CFNumberIsFloatType((CFNumberRef)self)) {
        return [NSString stringWithFormat:@"%0.16g", [self doubleValue]];
    }
    return [NSString stringWithFormat:@"%lld", [self longLongValue]];
}

// %@ of a number is its value, as on macOS (NSObject's would print the pointer)
- (NSString *)description {
    return [self stringValue];
}

- (BOOL)boolValue {
    /* -longLongValue goes through CFNumberGetValue, which does not read a
     * CFBoolean, so [@YES boolValue] answered NO. */
    if (CFGetTypeID((CFTypeRef)self) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)self) ? YES : NO;
    }
    return [self longLongValue] != 0;
}

/* These are CF objects, not ObjC allocations: the default NSObject refcounting
 * would free CF-allocated memory, and constant strings, which CF keeps
 * immortal, are not heap objects at all. Forward to CF. */
- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

/* CFNumber is immutable, so a copy is just a retain. */
- (id)copyWithZone:(NSZone *)zone {
    return (id)CFRetain((CFTypeRef)self);
}

/* Bridged to CFNumberRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSNumber class]]) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

/* +numberWithBool: yields kCFBooleanTrue/False, which are CFBooleans rather
 * than CFNumbers, so CFNumberGetValue and CFNumberCompare do not read them. */
static double __NSNumberAsDouble(NSNumber *number) {
    double result = 0.0;

    if (CFGetTypeID((CFTypeRef)number) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)number) ? 1.0 : 0.0;
    }
    CFNumberGetValue((CFNumberRef)number, kCFNumberDoubleType, &result);
    return result;
}

- (NSComparisonResult)compare:(NSNumber *)other {
    if (other == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[NSNumber compare:] nil argument"];
    }
    if (self == other) {
        return NSOrderedSame;
    }

    CFTypeID booleanID = CFBooleanGetTypeID();

    if (CFGetTypeID((CFTypeRef)self) == booleanID ||
        CFGetTypeID((CFTypeRef)other) == booleanID) {
        double left = __NSNumberAsDouble(self);
        double right = __NSNumberAsDouble(other);

        if (left < right) return NSOrderedAscending;
        if (left > right) return NSOrderedDescending;
        return NSOrderedSame;
    }

    return (NSComparisonResult)CFNumberCompare((CFNumberRef)self,
                                               (CFNumberRef)other, NULL);
}

- (BOOL)isEqualToNumber:(NSNumber *)other {
    return [self isEqual:other];
}

/* Reports the encoding matching the CFNumber's stored type, which is what
 * callers switch on to tell integers, floats and booleans apart. */
- (const char *)objCType {
    if (CFGetTypeID((CFTypeRef)self) == CFBooleanGetTypeID()) {
        return "c";
    }

    switch (CFNumberGetType((CFNumberRef)self)) {
        case kCFNumberFloat32Type:
        case kCFNumberFloatType:
            return "f";
        case kCFNumberFloat64Type:
        case kCFNumberDoubleType:
        case kCFNumberCGFloatType:
            return "d";
        case kCFNumberCharType:
            return "c";
        case kCFNumberShortType:
        case kCFNumberSInt16Type:
            return "s";
        case kCFNumberIntType:
        case kCFNumberSInt32Type:
            return "i";
        default:
            return "q";
    }
}

@end

// clang's constant number literals (@1, @1.5f, @1.5): {isa, value} objects in __DATA_CONST that are never written.
// CF sends hash/isEqual:/compare: back here, so those go through a temporary CFNumber
@interface NSConstantNumberBase : NSNumber
- (CFNumberRef)_pdCopyCFNumber;
@end

@implementation NSConstantNumberBase

- (CFNumberRef)_pdCopyCFNumber {
    long long zero = 0;
    return CFNumberCreate(NULL, kCFNumberLongLongType, &zero);
}

- (id)retain {
    return self;
}

- (oneway void)release {
}

- (id)autorelease {
    return self;
}

- (NSUInteger)retainCount {
    return NSUIntegerMax;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (CFTypeID)_cfTypeID {
    return CFNumberGetTypeID();
}

- (CFNumberType)_cfNumberType {
    CFNumberRef n = [self _pdCopyCFNumber];
    CFNumberType type = CFNumberGetType(n);

    CFRelease(n);
    return type;
}

- (Boolean)_getValue:(void *)value forType:(CFNumberType)type {
    CFNumberRef n = [self _pdCopyCFNumber];
    Boolean ok = CFNumberGetValue(n, type, value);

    CFRelease(n);
    return ok;
}

- (NSUInteger)hash {
    CFNumberRef n = [self _pdCopyCFNumber];
    NSUInteger h = (NSUInteger)CFHash(n);

    CFRelease(n);
    return h;
}

- (NSComparisonResult)compare:(NSNumber *)other {
    if (other == nil)
        [NSException raise:NSInvalidArgumentException format:@"-[NSNumber compare:] nil argument"];
    CFNumberRef n = [self _pdCopyCFNumber];
    NSComparisonResult r;

    if (CFGetTypeID((CFTypeRef)other) == CFBooleanGetTypeID()) {
        double left = 0, right = CFBooleanGetValue((CFBooleanRef)other) ? 1.0 : 0.0;
        CFNumberGetValue(n, kCFNumberDoubleType, &left);
        r = left < right ? NSOrderedAscending : left > right ? NSOrderedDescending : NSOrderedSame;
    } else {
        r = (NSComparisonResult)CFNumberCompare(n, (CFNumberRef)other, NULL);
    }
    CFRelease(n);
    return r;
}

- (CFComparisonResult)_reverseCompare:(NSNumber *)other {
    return (CFComparisonResult)-(NSInteger)[self compare:other];
}

- (BOOL)isEqual:(id)other {
    if (self == other)
        return YES;
    if (other == nil || ![other isKindOfClass:[NSNumber class]])
        return NO;
    return [self compare:other] == NSOrderedSame;
}

- (BOOL)boolValue {
    return [self doubleValue] != 0;
}

@end

@interface NSConstantIntegerNumber : NSConstantNumberBase {
    const char *_encoding;
    long long _value;
}
@end

@implementation NSConstantIntegerNumber

- (CFNumberRef)_pdCopyCFNumber {
    // unsigned values above LLONG_MAX only fit a double here
    if (_encoding && (_encoding[0] == 'Q' || _encoding[0] == 'L') && _value < 0) {
        double d = (double)(unsigned long long)_value;
        return CFNumberCreate(NULL, kCFNumberDoubleType, &d);
    }
    return CFNumberCreate(NULL, kCFNumberLongLongType, &_value);
}

- (const char *)objCType {
    return _encoding ? _encoding : "q";
}

- (long long)longLongValue {
    return _value;
}

- (unsigned long long)unsignedLongLongValue {
    return (unsigned long long)_value;
}

- (BOOL)boolValue {
    return _value != 0;
}

- (NSString *)stringValue {
    if (_encoding && (_encoding[0] == 'Q' || _encoding[0] == 'L' || _encoding[0] == 'I' || _encoding[0] == 'S' || _encoding[0] == 'C'))
        return [NSString stringWithFormat:@"%llu", (unsigned long long)_value];
    return [NSString stringWithFormat:@"%lld", _value];
}

@end

@interface NSConstantDoubleNumber : NSConstantNumberBase {
    double _value;
}
@end

@implementation NSConstantDoubleNumber

- (CFNumberRef)_pdCopyCFNumber {
    return CFNumberCreate(NULL, kCFNumberDoubleType, &_value);
}

- (const char *)objCType {
    return "d";
}

- (double)doubleValue {
    return _value;
}

@end

@interface NSConstantFloatNumber : NSConstantNumberBase {
    float _value;
}
@end

@implementation NSConstantFloatNumber

- (CFNumberRef)_pdCopyCFNumber {
    return CFNumberCreate(NULL, kCFNumberFloatType, &_value);
}

- (const char *)objCType {
    return "f";
}

- (float)floatValue {
    return _value;
}

- (double)doubleValue {
    return _value;
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFNumberBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFNumberGetTypeID(), "NSNumber");
}
#endif
