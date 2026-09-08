/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDateFormatter.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSString.h>
#import <Foundation/NSLocale.h>
#include <time.h>
#include <string.h>

/* strftime over the local timezone. The date/time *styles* are stored and
 * mapped to a few fixed patterns; full locale-driven formatting would go
 * through CFDateFormatter, which CoreFoundation does build. */

@implementation NSDateFormatter

- (void)dealloc {
    [_dateFormat release];
    [_locale release];
    [super dealloc];
}

- (NSString *)dateFormat { return _dateFormat; }

- (void)setDateFormat:(NSString *)format {
    format = [format copy];
    [_dateFormat release];
    _dateFormat = format;
}

- (NSDateFormatterStyle)dateStyle { return _dateStyle; }
- (void)setDateStyle:(NSDateFormatterStyle)style { _dateStyle = style; }
- (NSDateFormatterStyle)timeStyle { return _timeStyle; }
- (void)setTimeStyle:(NSDateFormatterStyle)style { _timeStyle = style; }
- (NSLocale *)locale { return _locale; }

- (void)setLocale:(NSLocale *)locale {
    locale = [locale retain];
    [_locale release];
    _locale = locale;
}

/* Translates the ICU-style pattern subset callers actually use into strftime. */
- (const char *)_strftimeFormat {
    if (_dateFormat != nil) {
        const char *pattern = [_dateFormat UTF8String];

        if (strcmp(pattern, "yyyy-MM-dd") == 0)          { return "%Y-%m-%d"; }
        if (strcmp(pattern, "yyyy-MM-dd HH:mm:ss") == 0) { return "%Y-%m-%d %H:%M:%S"; }
        if (strcmp(pattern, "HH:mm:ss") == 0)            { return "%H:%M:%S"; }
    }
    if (_dateStyle != NSDateFormatterNoStyle && _timeStyle != NSDateFormatterNoStyle) {
        return "%Y-%m-%d %H:%M:%S";
    }
    if (_timeStyle != NSDateFormatterNoStyle) {
        return "%H:%M:%S";
    }
    return "%Y-%m-%d";
}

- (NSString *)stringFromDate:(NSDate *)date {
    if (date == nil) {
        return nil;
    }

    time_t seconds = (time_t)[date timeIntervalSince1970];
    struct tm broken;
    char buffer[128];

    if (localtime_r(&seconds, &broken) == NULL) {
        return nil;
    }
    if (strftime(buffer, sizeof(buffer), [self _strftimeFormat], &broken) == 0) {
        return nil;
    }
    return [NSString stringWithUTF8String:buffer];
}

- (NSDate *)dateFromString:(NSString *)string {
    if (string == nil) {
        return nil;
    }

    struct tm broken;
    memset(&broken, 0, sizeof(broken));
    broken.tm_isdst = -1;

    if (strptime([string UTF8String], [self _strftimeFormat], &broken) == NULL) {
        return nil;
    }
    return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)mktime(&broken)];
}

- (NSString *)stringForObjectValue:(id)object {
    if (![object isKindOfClass:[NSDate class]]) {
        return nil;
    }
    return [self stringFromDate:object];
}

@end
