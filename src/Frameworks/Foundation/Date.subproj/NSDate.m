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

- (NSTimeInterval)timeIntervalSinceReferenceDate {
    return (NSTimeInterval)CFDateGetAbsoluteTime((CFDateRef)self);
}

- (NSTimeInterval)timeIntervalSince1970 {
    return [self timeIntervalSinceReferenceDate] + kCFAbsoluteTimeIntervalSince1970;
}

- (NSTimeInterval)timeIntervalSinceDate:(NSDate *)other {
    return (NSTimeInterval)CFDateGetTimeIntervalSinceDate((CFDateRef)self,
                                                          (CFDateRef)other);
}

- (NSTimeInterval)timeIntervalSinceNow {
    return [self timeIntervalSinceReferenceDate] - CFAbsoluteTimeGetCurrent();
}

- (instancetype)dateByAddingTimeInterval:(NSTimeInterval)seconds {
    return [NSDate dateWithTimeIntervalSinceReferenceDate:
            [self timeIntervalSinceReferenceDate] + seconds];
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFDateBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFDateGetTypeID(), "NSDate");
}
#endif
