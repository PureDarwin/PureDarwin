/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* Wraps CFTimeZone, which already carries the zoneinfo handling. */

#import <Foundation/NSTimeZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFTimeZone.h>

@implementation NSTimeZone

static NSTimeZone *_wrap(CFTimeZoneRef zone) {
    if (zone == NULL) {
        return nil;
    }

    NSTimeZone *result = [[[NSTimeZone alloc] init] autorelease];

    result->_cfTimeZone = (void *)zone;
    return result;
}

- (void)dealloc {
    if (_cfTimeZone != NULL) {
        CFRelease((CFTimeZoneRef)_cfTimeZone);
    }
    [super dealloc];
}

+ (NSTimeZone *)systemTimeZone {
    return _wrap(CFTimeZoneCopySystem());
}

/* Tracks the system zone rather than snapshotting it, which is the documented
 * difference from +systemTimeZone. */
+ (NSTimeZone *)localTimeZone {
    return [self systemTimeZone];
}

+ (NSTimeZone *)defaultTimeZone {
    return _wrap(CFTimeZoneCopyDefault());
}

+ (void)setDefaultTimeZone:(NSTimeZone *)timeZone {
    if (timeZone != nil) {
        CFTimeZoneSetDefault((CFTimeZoneRef)timeZone->_cfTimeZone);
    }
}

+ (void)resetSystemTimeZone {
    CFTimeZoneResetSystem();
}

+ (NSTimeZone *)timeZoneWithName:(NSString *)name {
    return _wrap(CFTimeZoneCreateWithName(kCFAllocatorDefault, (CFStringRef)name, true));
}

+ (NSTimeZone *)timeZoneForSecondsFromGMT:(NSInteger)seconds {
    return _wrap(CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault,
                                                         (CFTimeInterval)seconds));
}

+ (NSTimeZone *)timeZoneWithAbbreviation:(NSString *)abbreviation {
    CFDictionaryRef abbreviations = CFTimeZoneCopyAbbreviationDictionary();

    if (abbreviations == NULL) {
        return nil;
    }

    CFStringRef name = CFDictionaryGetValue(abbreviations, (CFStringRef)abbreviation);
    NSTimeZone *result = (name != NULL)
        ? [self timeZoneWithName:(NSString *)name] : nil;

    CFRelease(abbreviations);
    return result;
}

+ (NSArray *)knownTimeZoneNames {
    CFArrayRef names = CFTimeZoneCopyKnownNames();

    return (names != NULL) ? (id)CFAutorelease(names) : [NSArray array];
}

- (NSString *)name {
    return (NSString *)CFTimeZoneGetName((CFTimeZoneRef)_cfTimeZone);
}

- (NSString *)abbreviation {
    return [self abbreviationForDate:[NSDate date]];
}

- (NSString *)abbreviationForDate:(NSDate *)date {
    CFStringRef abbreviation = CFTimeZoneCopyAbbreviation(
        (CFTimeZoneRef)_cfTimeZone, (CFAbsoluteTime)[date timeIntervalSinceReferenceDate]);

    return (abbreviation != NULL) ? (id)CFAutorelease(abbreviation) : nil;
}

- (NSInteger)secondsFromGMT {
    return [self secondsFromGMTForDate:[NSDate date]];
}

- (NSInteger)secondsFromGMTForDate:(NSDate *)date {
    return (NSInteger)CFTimeZoneGetSecondsFromGMT(
        (CFTimeZoneRef)_cfTimeZone, (CFAbsoluteTime)[date timeIntervalSinceReferenceDate]);
}

- (BOOL)isDaylightSavingTime {
    return [self isDaylightSavingTimeForDate:[NSDate date]];
}

- (BOOL)isDaylightSavingTimeForDate:(NSDate *)date {
    return CFTimeZoneIsDaylightSavingTime(
        (CFTimeZoneRef)_cfTimeZone,
        (CFAbsoluteTime)[date timeIntervalSinceReferenceDate]) ? YES : NO;
}

- (NSTimeInterval)daylightSavingTimeOffsetForDate:(NSDate *)date {
    return (NSTimeInterval)CFTimeZoneGetDaylightSavingTimeOffset(
        (CFTimeZoneRef)_cfTimeZone, (CFAbsoluteTime)[date timeIntervalSinceReferenceDate]);
}

- (BOOL)isEqualToTimeZone:(NSTimeZone *)timeZone {
    if (timeZone == self) {
        return YES;
    }
    if (timeZone == nil) {
        return NO;
    }
    return [[self name] isEqualToString:[timeZone name]];
}

- (NSString *)description {
    return [self name];
}

@end
