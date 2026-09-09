/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSCalendarDate.h>
#import <Foundation/NSString.h>
#include <time.h>
#include <string.h>

/* NSCalendarDate is a deprecated OpenStep class that GNUstep code still uses
 * heavily. It is broken-out local time over an absolute NSDate. */

static NSTimeInterval _referenceOffset(void) {
    /* NSDate's reference date is 2001-01-01 00:00:00 GMT. */
    return 978307200.0;
}

static struct tm _brokenDownForInterval(NSTimeInterval interval) {
    time_t seconds = (time_t)(interval + _referenceOffset());
    struct tm parts;

    memset(&parts, 0, sizeof(parts));
    localtime_r(&seconds, &parts);
    return parts;
}

@implementation NSCalendarDate

+ (instancetype)calendarDate {
    return [[[self alloc] init] autorelease];
}

+ (instancetype)dateWithString:(NSString *)description
                calendarFormat:(NSString *)format {
    if (description == nil || format == nil) {
        return nil;
    }

    struct tm parts;

    memset(&parts, 0, sizeof(parts));
    parts.tm_isdst = -1;

    if (strptime([description UTF8String], [format UTF8String], &parts) == NULL) {
        return nil;
    }

    time_t seconds = mktime(&parts);

    if (seconds == (time_t)-1) {
        return nil;
    }

    NSCalendarDate *date = [[[self alloc] initWithTimeIntervalSinceReferenceDate:
        (NSTimeInterval)seconds - _referenceOffset()] autorelease];

    [date setCalendarFormat:format];
    return date;
}

+ (instancetype)dateWithYear:(NSInteger)year
                       month:(NSUInteger)month
                         day:(NSUInteger)day
                        hour:(NSUInteger)hour
                      minute:(NSUInteger)minute
                      second:(NSUInteger)second
                    timeZone:(id)timeZone {
    struct tm parts;

    memset(&parts, 0, sizeof(parts));
    parts.tm_year = (int)(year - 1900);
    parts.tm_mon = (int)month - 1;
    parts.tm_mday = (int)day;
    parts.tm_hour = (int)hour;
    parts.tm_min = (int)minute;
    parts.tm_sec = (int)second;
    parts.tm_isdst = -1;

    time_t seconds = mktime(&parts);

    if (seconds == (time_t)-1) {
        return nil;
    }
    return [[[self alloc] initWithTimeIntervalSinceReferenceDate:
        (NSTimeInterval)seconds - _referenceOffset()] autorelease];
}

- (instancetype)init {
    return [self initWithTimeIntervalSinceReferenceDate:
        [NSDate timeIntervalSinceReferenceDate]];
}

- (instancetype)initWithTimeIntervalSinceReferenceDate:(NSTimeInterval)interval {
    self = [super init];
    if (self != nil) {
        _interval = interval;
        _format = [@"%Y-%m-%d %H:%M:%S %z" copy];
    }
    return self;
}

- (void)dealloc {
    [_format release];
    [super dealloc];
}

- (NSTimeInterval)timeIntervalSinceReferenceDate {
    return _interval;
}

- (NSInteger)yearOfCommonEra {
    return _brokenDownForInterval(_interval).tm_year + 1900;
}

- (NSInteger)monthOfYear {
    return _brokenDownForInterval(_interval).tm_mon + 1;
}

- (NSInteger)dayOfMonth {
    return _brokenDownForInterval(_interval).tm_mday;
}

- (NSInteger)dayOfWeek {
    return _brokenDownForInterval(_interval).tm_wday;
}

- (NSInteger)dayOfYear {
    return _brokenDownForInterval(_interval).tm_yday + 1;
}

- (NSInteger)hourOfDay {
    return _brokenDownForInterval(_interval).tm_hour;
}

- (NSInteger)minuteOfHour {
    return _brokenDownForInterval(_interval).tm_min;
}

- (NSInteger)secondOfMinute {
    return _brokenDownForInterval(_interval).tm_sec;
}

- (NSString *)calendarFormat {
    return _format;
}

- (void)setCalendarFormat:(NSString *)format {
    [format retain];
    [_format release];
    _format = format;
}

- (NSString *)descriptionWithCalendarFormat:(NSString *)format {
    if (format == nil) {
        format = _format;
    }

    struct tm parts = _brokenDownForInterval(_interval);
    char text[256];
    size_t used = strftime(text, sizeof(text), [format UTF8String], &parts);

    if (used == 0) {
        return @"";
    }
    return [NSString stringWithUTF8String:text];
}

- (NSString *)description {
    return [self descriptionWithCalendarFormat:_format];
}

- (NSCalendarDate *)dateByAddingYears:(NSInteger)years
                               months:(NSInteger)months
                                 days:(NSInteger)days
                                hours:(NSInteger)hours
                              minutes:(NSInteger)minutes
                              seconds:(NSInteger)seconds {
    struct tm parts = _brokenDownForInterval(_interval);

    parts.tm_year += (int)years;
    parts.tm_mon += (int)months;
    parts.tm_mday += (int)days;
    parts.tm_hour += (int)hours;
    parts.tm_min += (int)minutes;
    parts.tm_sec += (int)seconds;
    parts.tm_isdst = -1;

    /* mktime normalises the out-of-range fields, which is how the month and
     * day rollover in this API is meant to work. */
    time_t result = mktime(&parts);

    if (result == (time_t)-1) {
        return nil;
    }

    NSCalendarDate *date = [[[NSCalendarDate alloc]
        initWithTimeIntervalSinceReferenceDate:
            (NSTimeInterval)result - _referenceOffset()] autorelease];

    [date setCalendarFormat:_format];
    return date;
}

@end
