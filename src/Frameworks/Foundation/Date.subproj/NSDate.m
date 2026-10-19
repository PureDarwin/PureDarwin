/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDate.h>
#import <Foundation/NSString.h>
#import <Foundation/NSTimeZone.h>
#include <math.h>
#include <time.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/ForFoundationOnly.h>

extern Boolean _CFIsObjC(CFTypeID typeID, void *obj);

/* NSDate and CFDate share the same epoch - 2001-01-01 00:00:00 GMT - so the
 * reference-date interval passes through untouched. Only the 1970 accessors
 * need the offset. */
@implementation NSDate

/* Immutable and with no mutable counterpart, so a copy is the object itself.
 * Without this -copy raised "unrecognized selector". */
- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

// the instance +alloc made is not a CFDate, so these hand back a real one. Subclasses keep theirs
- (instancetype)initWithTimeIntervalSinceReferenceDate:(NSTimeInterval)seconds {
    if (_CFIsObjC(CFDateGetTypeID(), (void *)self))
        return [super init];
    return (id)CFDateCreate(kCFAllocatorDefault, (CFAbsoluteTime)seconds);
}

// subclasses such as NSCalendarDate call this from their own initializers
- (instancetype)init {
    if (_CFIsObjC(CFDateGetTypeID(), (void *)self))
        return [super init];
    return (id)CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
}

- (instancetype)initWithTimeIntervalSinceNow:(NSTimeInterval)seconds {
    return [self initWithTimeIntervalSinceReferenceDate:CFAbsoluteTimeGetCurrent() + seconds];
}

- (instancetype)initWithTimeIntervalSince1970:(NSTimeInterval)seconds {
    return [self initWithTimeIntervalSinceReferenceDate:seconds - kCFAbsoluteTimeIntervalSince1970];
}

- (instancetype)initWithTimeInterval:(NSTimeInterval)seconds sinceDate:(NSDate *)date {
    return [self initWithTimeIntervalSinceReferenceDate:[date timeIntervalSinceReferenceDate] + seconds];
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

- (BOOL)isEqualToDate:(NSDate *)other {
    return other != nil && [self timeIntervalSinceReferenceDate] == [other timeIntervalSinceReferenceDate];
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

// strftime-style format of an NSCalendarDate in ZONE (the local zone when nil), the locale is unused
- (NSString *)descriptionWithCalendarFormat:(NSString *)format timeZone:(NSTimeZone *)zone locale:(id)locale {
    time_t seconds = (time_t)floor([self timeIntervalSince1970]);
    struct tm parts;
    char text[256];

    if (zone != nil) {
        seconds += (time_t)[zone secondsFromGMTForDate:self];
        gmtime_r(&seconds, &parts);
    } else {
        localtime_r(&seconds, &parts);
    }
    if (format == nil)
        format = @"%Y-%m-%d %H:%M:%S %z";
    if (strftime(text, sizeof(text), [format UTF8String], &parts) == 0)
        return @"";
    return [NSString stringWithUTF8String:text];
}


// CF owns the count of objects CF made, NSObject's would let CFRelease free them under a retainer.
// Instances of ObjC subclasses keep NSObject's count
- (id)retain {
    if (_CFIsObjC(CFDateGetTypeID(), (void *)self))
        return [super retain];
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    if (_CFIsObjC(CFDateGetTypeID(), (void *)self)) {
        [super release];
        return;
    }
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    if (_CFIsObjC(CFDateGetTypeID(), (void *)self))
        return [super retainCount];
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}
@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFDateBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFDateGetTypeID(), "NSDate");
}
#endif
