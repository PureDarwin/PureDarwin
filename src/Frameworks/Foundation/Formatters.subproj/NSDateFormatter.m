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
#import <CoreFoundation/CFDateFormatter.h>
#include <time.h>
#include <string.h>

// formatting goes through CFDateFormatter and ICU, as Apple's does
// strftime is only the fallback when ICU cannot make a formatter

@implementation NSDateFormatter

- (void)dealloc {
    [self _invalidate];
    [_dateFormat release];
    [_locale release];
    [super dealloc];
}

- (void)_invalidate {
    if (_cfFormatter != NULL) {
        CFRelease((CFDateFormatterRef)_cfFormatter);
        _cfFormatter = NULL;
    }
}

// built on first use and dropped whenever the format, styles or locale change
- (CFDateFormatterRef)_formatter {
    CFLocaleRef locale;

    if (_cfFormatter != NULL) {
        return (CFDateFormatterRef)_cfFormatter;
    }
    locale = _locale != nil ? (CFLocaleRef)CFRetain((CFLocaleRef)_locale) : CFLocaleCopyCurrent();
    _cfFormatter = (void *)CFDateFormatterCreate(kCFAllocatorDefault, locale,
                                                 (CFDateFormatterStyle)_dateStyle,
                                                 (CFDateFormatterStyle)_timeStyle);
    if (locale != NULL) {
        CFRelease(locale);
    }
    if (_cfFormatter != NULL && _dateFormat != nil) {
        CFDateFormatterSetFormat((CFDateFormatterRef)_cfFormatter, (CFStringRef)_dateFormat);
    }
    return (CFDateFormatterRef)_cfFormatter;
}

// a style-built formatter reports the pattern ICU picked for the locale
- (NSString *)dateFormat {
    CFDateFormatterRef formatter;

    if (_dateFormat != nil) {
        return _dateFormat;
    }
    formatter = [self _formatter];
    return formatter != NULL ? (NSString *)CFDateFormatterGetFormat(formatter) : nil;
}

- (void)setDateFormat:(NSString *)format {
    format = [format copy];
    [_dateFormat release];
    _dateFormat = format;
    [self _invalidate];
}

- (NSDateFormatterStyle)dateStyle { return _dateStyle; }
- (void)setDateStyle:(NSDateFormatterStyle)style {
    _dateStyle = style;
    [_dateFormat release];
    _dateFormat = nil;
    [self _invalidate];
}
- (NSDateFormatterStyle)timeStyle { return _timeStyle; }
- (void)setTimeStyle:(NSDateFormatterStyle)style {
    _timeStyle = style;
    [_dateFormat release];
    _dateFormat = nil;
    [self _invalidate];
}
- (NSLocale *)locale { return _locale; }

- (void)setLocale:(NSLocale *)locale {
    locale = [locale retain];
    [_locale release];
    _locale = locale;
    [self _invalidate];
}

// the fallback's strftime patterns for the common ICU ones
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
    CFDateFormatterRef formatter;
    CFStringRef text;

    if (date == nil) {
        return nil;
    }
    formatter = [self _formatter];
    if (formatter != NULL) {
        text = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter,
                                                           [date timeIntervalSinceReferenceDate]);
        return [(NSString *)text autorelease];
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
    CFDateFormatterRef formatter;
    CFAbsoluteTime when;

    if (string == nil) {
        return nil;
    }
    formatter = [self _formatter];
    if (formatter != NULL) {
        if (!CFDateFormatterGetAbsoluteTimeFromString(formatter, (CFStringRef)string, NULL, &when)) {
            return nil;
        }
        return [NSDate dateWithTimeIntervalSinceReferenceDate:when];
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
