/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDate.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/ForFoundationOnly.h>

/* NSDate and CFDate share the same epoch - 2001-01-01 00:00:00 GMT - so the
 * reference-date interval passes through untouched. Only the 1970 accessors
 * need the offset. */
@implementation NSDate

/* Immutable and with no mutable counterpart, so a copy is the object itself.
 * Without this -copy raised "unrecognized selector". */
- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

+ (instancetype)date {
    return [self dateWithTimeIntervalSinceReferenceDate:CFAbsoluteTimeGetCurrent()];
}

+ (instancetype)now {
    return [self date];
}

+ (instancetype)dateWithTimeIntervalSinceNow:(NSTimeInterval)seconds {
    return [self dateWithTimeIntervalSinceReferenceDate:CFAbsoluteTimeGetCurrent() + seconds];
}

+ (instancetype)dateWithTimeIntervalSinceReferenceDate:(NSTimeInterval)seconds {
    CFDateRef result = CFDateCreate(kCFAllocatorDefault, (CFAbsoluteTime)seconds);
    return (id)CFAutorelease(result);
}

+ (instancetype)dateWithTimeIntervalSince1970:(NSTimeInterval)seconds {
    return [self dateWithTimeIntervalSinceReferenceDate:
            seconds - kCFAbsoluteTimeIntervalSince1970];
}

/* The sentinels callers pass to mean "no deadline" / "already elapsed". The
 * values match Apple's, which are far outside any real date arithmetic. */
+ (instancetype)distantFuture {
    return [self dateWithTimeIntervalSinceReferenceDate:63113904000.0];
}

+ (NSTimeInterval)timeIntervalSinceReferenceDate {
    return (NSTimeInterval)CFAbsoluteTimeGetCurrent();
}

+ (instancetype)distantPast {
    return [self dateWithTimeIntervalSinceReferenceDate:-63114076800.0];
}

- (NSTimeInterval)timeIntervalSinceReferenceDate {
    return (NSTimeInterval)CFDateGetAbsoluteTime((CFDateRef)self);
}

- (NSTimeInterval)timeIntervalSince1970 {
    return [self timeIntervalSinceReferenceDate] + kCFAbsoluteTimeIntervalSince1970;
}

/* Goes through the accessor rather than casting to CFDateRef: only a real
 * CFDate carries CF storage, and NSCalendarDate is a plain ObjC subclass. */
- (NSTimeInterval)timeIntervalSinceDate:(NSDate *)other {
    return [self timeIntervalSinceReferenceDate] -
           [other timeIntervalSinceReferenceDate];
}

- (NSComparisonResult)compare:(NSDate *)other {
    NSTimeInterval difference = [self timeIntervalSinceDate:other];
    if (difference < 0.0) {
        return NSOrderedAscending;
    }
    if (difference > 0.0) {
        return NSOrderedDescending;
    }
    return NSOrderedSame;
}

- (NSTimeInterval)timeIntervalSinceNow {
    return [self timeIntervalSinceReferenceDate] - CFAbsoluteTimeGetCurrent();
}

- (instancetype)dateByAddingTimeInterval:(NSTimeInterval)seconds {
    return [NSDate dateWithTimeIntervalSinceReferenceDate:
            [self timeIntervalSinceReferenceDate] + seconds];
}

/* Goes through the accessor rather than CFHash/CFEqual: only a real CFDate
 * carries CF storage, and NSCalendarDate is a plain ObjC subclass. */
- (NSUInteger)hash {
    return (NSUInteger)[self timeIntervalSinceReferenceDate];
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSDate class]]) {
        return NO;
    }
    return [self timeIntervalSinceReferenceDate] ==
           [(NSDate *)other timeIntervalSinceReferenceDate];
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFDateBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFDateGetTypeID(), "NSDate");
}
#endif
