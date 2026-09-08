/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCalendar_h
#define NSCalendar_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSDate, NSString, NSLocale, NSTimeZone;

typedef NS_OPTIONS(NSUInteger, NSCalendarUnit) {
    NSCalendarUnitEra     = 1 << 1,
    NSCalendarUnitYear    = 1 << 2,
    NSCalendarUnitMonth   = 1 << 3,
    NSCalendarUnitDay     = 1 << 4,
    NSCalendarUnitHour    = 1 << 5,
    NSCalendarUnitMinute  = 1 << 6,
    NSCalendarUnitSecond  = 1 << 7,
    NSCalendarUnitWeekday = 1 << 9,

    /* The pre-10.7 spellings Cocotron uses. */
    NSEraCalendarUnit     = NSCalendarUnitEra,
    NSYearCalendarUnit    = NSCalendarUnitYear,
    NSMonthCalendarUnit   = NSCalendarUnitMonth,
    NSDayCalendarUnit     = NSCalendarUnitDay,
    NSHourCalendarUnit    = NSCalendarUnitHour,
    NSMinuteCalendarUnit  = NSCalendarUnitMinute,
    NSSecondCalendarUnit  = NSCalendarUnitSecond,
    NSWeekdayCalendarUnit = NSCalendarUnitWeekday,
};

#define NSUndefinedDateComponent NSIntegerMax

@interface NSDateComponents : NSObject <NSCopying> {
    NSInteger _era, _year, _month, _day, _hour, _minute, _second, _weekday;
}

- (NSInteger)era;
- (NSInteger)year;
- (NSInteger)month;
- (NSInteger)day;
- (NSInteger)hour;
- (NSInteger)minute;
- (NSInteger)second;
- (NSInteger)weekday;

- (void)setEra:(NSInteger)value;
- (void)setYear:(NSInteger)value;
- (void)setMonth:(NSInteger)value;
- (void)setDay:(NSInteger)value;
- (void)setHour:(NSInteger)value;
- (void)setMinute:(NSInteger)value;
- (void)setSecond:(NSInteger)value;
- (void)setWeekday:(NSInteger)value;

@end

@interface NSCalendar : NSObject <NSCopying>

+ (NSCalendar *)currentCalendar;

- (instancetype)initWithCalendarIdentifier:(NSString *)identifier;

- (NSDateComponents *)components:(NSCalendarUnit)units fromDate:(NSDate *)date;
- (NSDate *)dateFromComponents:(NSDateComponents *)components;

@end

#endif /* NSCalendar_h */
