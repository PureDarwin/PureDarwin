/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCalendarDate_h
#define NSCalendarDate_h

#import <Foundation/NSDate.h>

@class NSString, NSTimeZone;

@interface NSCalendarDate : NSDate {
    NSTimeInterval _interval;
    NSString *_format;
}

+ (instancetype)calendarDate;
+ (instancetype)dateWithString:(NSString *)description
                calendarFormat:(NSString *)format;
+ (instancetype)dateWithYear:(NSInteger)year
                       month:(NSUInteger)month
                         day:(NSUInteger)day
                        hour:(NSUInteger)hour
                      minute:(NSUInteger)minute
                      second:(NSUInteger)second
                    timeZone:(id)timeZone;

- (NSInteger)yearOfCommonEra;
- (NSInteger)monthOfYear;
- (NSInteger)dayOfMonth;
- (NSInteger)dayOfWeek;
- (NSInteger)dayOfYear;
- (NSInteger)hourOfDay;
- (NSInteger)minuteOfHour;
- (NSInteger)secondOfMinute;

- (NSString *)calendarFormat;
- (void)setCalendarFormat:(NSString *)format;
- (NSString *)descriptionWithCalendarFormat:(NSString *)format;

- (NSCalendarDate *)dateByAddingYears:(NSInteger)years
                               months:(NSInteger)months
                                 days:(NSInteger)days
                                hours:(NSInteger)hours
                              minutes:(NSInteger)minutes
                              seconds:(NSInteger)seconds;

@end

#endif /* NSCalendarDate_h */
