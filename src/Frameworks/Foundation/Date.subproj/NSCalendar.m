/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSCalendar.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSString.h>
#include <time.h>
#include <string.h>

/* Gregorian only, via the C library's civil-time conversion. Non-Gregorian
 * identifiers are accepted and behave as Gregorian. */

@implementation NSDateComponents

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _era = _year = _month = _day = NSUndefinedDateComponent;
    _hour = _minute = _second = _weekday = NSUndefinedDateComponent;
    return self;
}

- (NSInteger)era { return _era; }
- (NSInteger)year { return _year; }
- (NSInteger)month { return _month; }
- (NSInteger)day { return _day; }
- (NSInteger)hour { return _hour; }
- (NSInteger)minute { return _minute; }
- (NSInteger)second { return _second; }
- (NSInteger)weekday { return _weekday; }

- (void)setEra:(NSInteger)value { _era = value; }
- (void)setYear:(NSInteger)value { _year = value; }
- (void)setMonth:(NSInteger)value { _month = value; }
- (void)setDay:(NSInteger)value { _day = value; }
- (void)setHour:(NSInteger)value { _hour = value; }
- (void)setMinute:(NSInteger)value { _minute = value; }
- (void)setSecond:(NSInteger)value { _second = value; }
- (void)setWeekday:(NSInteger)value { _weekday = value; }

- (id)copyWithZone:(NSZone *)zone {
    NSDateComponents *copy = [[NSDateComponents alloc] init];

    copy->_era = _era;
    copy->_year = _year;
    copy->_month = _month;
    copy->_day = _day;
    copy->_hour = _hour;
    copy->_minute = _minute;
    copy->_second = _second;
    copy->_weekday = _weekday;
    return copy;
}

@end

@implementation NSCalendar

+ (NSCalendar *)currentCalendar {
    static NSCalendar *shared = nil;

    if (shared == nil) {
        shared = [[self alloc] initWithCalendarIdentifier:@"gregorian"];
    }
    return shared;
}

- (instancetype)initWithCalendarIdentifier:(NSString *)identifier {
    return [super init];
}

- (NSDateComponents *)components:(NSCalendarUnit)units fromDate:(NSDate *)date {
    NSDateComponents *components = [[[NSDateComponents alloc] init] autorelease];
    time_t seconds = (time_t)[date timeIntervalSince1970];
    struct tm broken;

    if (localtime_r(&seconds, &broken) == NULL) {
        return components;
    }
    if (units & NSCalendarUnitEra)     { [components setEra:1]; }
    if (units & NSCalendarUnitYear)    { [components setYear:broken.tm_year + 1900]; }
    if (units & NSCalendarUnitMonth)   { [components setMonth:broken.tm_mon + 1]; }
    if (units & NSCalendarUnitDay)     { [components setDay:broken.tm_mday]; }
    if (units & NSCalendarUnitHour)    { [components setHour:broken.tm_hour]; }
    if (units & NSCalendarUnitMinute)  { [components setMinute:broken.tm_min]; }
    if (units & NSCalendarUnitSecond)  { [components setSecond:broken.tm_sec]; }
    if (units & NSCalendarUnitWeekday) { [components setWeekday:broken.tm_wday + 1]; }
    return components;
}

- (NSDate *)dateFromComponents:(NSDateComponents *)components {
    struct tm broken;

    memset(&broken, 0, sizeof(broken));
    broken.tm_year = ([components year] != NSUndefinedDateComponent)
        ? (int)([components year] - 1900) : 70;
    broken.tm_mon = ([components month] != NSUndefinedDateComponent)
        ? (int)([components month] - 1) : 0;
    broken.tm_mday = ([components day] != NSUndefinedDateComponent)
        ? (int)[components day] : 1;
    broken.tm_hour = ([components hour] != NSUndefinedDateComponent)
        ? (int)[components hour] : 0;
    broken.tm_min = ([components minute] != NSUndefinedDateComponent)
        ? (int)[components minute] : 0;
    broken.tm_sec = ([components second] != NSUndefinedDateComponent)
        ? (int)[components second] : 0;
    broken.tm_isdst = -1;

    time_t seconds = mktime(&broken);
    return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)seconds];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
