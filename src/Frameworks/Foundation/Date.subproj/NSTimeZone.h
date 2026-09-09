/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSTimeZone_h
#define NSTimeZone_h

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSString, NSArray, NSData, NSDictionary;

@interface NSTimeZone : NSObject {
    void *_cfTimeZone;
}

+ (NSTimeZone *)systemTimeZone;
+ (NSTimeZone *)localTimeZone;
+ (NSTimeZone *)defaultTimeZone;
+ (void)setDefaultTimeZone:(NSTimeZone *)timeZone;
+ (void)resetSystemTimeZone;

+ (NSTimeZone *)timeZoneWithName:(NSString *)name;
+ (NSTimeZone *)timeZoneForSecondsFromGMT:(NSInteger)seconds;
+ (NSTimeZone *)timeZoneWithAbbreviation:(NSString *)abbreviation;

+ (NSArray *)knownTimeZoneNames;

- (NSString *)name;
- (NSString *)abbreviation;
- (NSString *)abbreviationForDate:(NSDate *)date;
- (NSInteger)secondsFromGMT;
- (NSInteger)secondsFromGMTForDate:(NSDate *)date;
- (BOOL)isDaylightSavingTime;
- (BOOL)isDaylightSavingTimeForDate:(NSDate *)date;
- (NSTimeInterval)daylightSavingTimeOffsetForDate:(NSDate *)date;
- (BOOL)isEqualToTimeZone:(NSTimeZone *)timeZone;

@end

#endif /* NSTimeZone_h */
