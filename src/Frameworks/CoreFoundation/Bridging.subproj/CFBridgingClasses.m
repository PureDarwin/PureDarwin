#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>

/* CoreFoundation owns the toll-free-bridged NSNull class.  Foundation
 * consumes the public declaration but must not provide a second class. */
@interface NSNull : NSObject
+ (NSNull *)null;
- (NSString *)description;
@end

@implementation NSNull

+ (NSNull *)null {
    static NSNull *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (NSString *)description {
    return (NSString *)CFSTR("<null>");
}

@end

@interface __NSCFBoolean : NSObject
@end

@implementation __NSCFBoolean

/* A bridged kCFBooleanTrue/False reports this class, not NSNumber, so the
 * NSNumber accessors callers use on it have to be answered here. */
- (BOOL)isKindOfClass:(Class)aClass {
    Class number = objc_getClass("NSNumber");

    if (number != Nil && aClass == number) {
        return YES;
    }
    return [super isKindOfClass:aClass];
}

- (BOOL)boolValue {
    return CFBooleanGetValue((CFBooleanRef)self) ? YES : NO;
}

- (char)charValue {
    return (char)[self boolValue];
}

- (int)intValue {
    return (int)[self boolValue];
}

- (NSInteger)integerValue {
    return (NSInteger)[self boolValue];
}

- (unsigned int)unsignedIntValue {
    return (unsigned int)[self boolValue];
}

- (float)floatValue {
    return (float)[self boolValue];
}

- (double)doubleValue {
    return (double)[self boolValue];
}

- (NSString *)description {
    return (NSString *)([self boolValue] ? CFSTR("1") : CFSTR("0"));
}

@end

@interface __NSCFNumber : NSObject
@end

@implementation __NSCFNumber

- (double)doubleValue {
    double value = 0.0;

    CFNumberGetValue((CFNumberRef)self, kCFNumberDoubleType, &value);
    return value;
}

- (float)floatValue {
    float value = 0.0f;

    CFNumberGetValue((CFNumberRef)self, kCFNumberFloatType, &value);
    return value;
}

- (long long)longLongValue {
    long long value = 0;

    CFNumberGetValue((CFNumberRef)self, kCFNumberLongLongType, &value);
    return value;
}

- (unsigned long long)unsignedLongLongValue {
    return (unsigned long long)[self longLongValue];
}

- (NSInteger)integerValue {
    return (NSInteger)[self longLongValue];
}

- (NSUInteger)unsignedIntegerValue {
    return (NSUInteger)[self longLongValue];
}

- (int)intValue {
    return (int)[self longLongValue];
}

- (unsigned int)unsignedIntValue {
    return (unsigned int)[self longLongValue];
}

- (long)longValue {
    return (long)[self longLongValue];
}

- (unsigned long)unsignedLongValue {
    return (unsigned long)[self longLongValue];
}

- (short)shortValue {
    return (short)[self longLongValue];
}

- (unsigned short)unsignedShortValue {
    return (unsigned short)[self longLongValue];
}

- (char)charValue {
    return (char)[self longLongValue];
}

- (unsigned char)unsignedCharValue {
    return (unsigned char)[self longLongValue];
}

- (BOOL)boolValue {
    return [self longLongValue] != 0 ? YES : NO;
}

- (const char *)objCType {
    return CFNumberIsFloatType((CFNumberRef)self) ? "d" : "q";
}

- (NSString *)stringValue {
    return [self description];
}

- (NSString *)description {
    if (CFNumberIsFloatType((CFNumberRef)self)) {
        return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
            CFSTR("%g"), [self doubleValue]);
    }
    return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("%lld"), [self longLongValue]);
}

- (NSComparisonResult)compare:(id)other {
    double mine = [self doubleValue];
    double theirs = [other doubleValue];

    if (mine < theirs) {
        return NSOrderedAscending;
    }
    return (mine > theirs) ? NSOrderedDescending : NSOrderedSame;
}

/* Callers type-check with isKindOfClass:[NSNumber class]; NSNumber lives in
 * Foundation, the layer above, so claim the relationship at runtime. */
- (BOOL)isKindOfClass:(Class)aClass {
    Class number = objc_getClass("NSNumber");

    if (number != Nil && aClass == number) {
        return YES;
    }
    return [super isKindOfClass:aClass];
}

@end
