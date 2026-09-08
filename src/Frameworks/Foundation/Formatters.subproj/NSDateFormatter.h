/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDateFormatter_h
#define NSDateFormatter_h

#import <Foundation/NSFormatter.h>

@class NSDate, NSLocale, NSTimeZone;

typedef NS_ENUM(NSUInteger, NSDateFormatterStyle) {
    NSDateFormatterNoStyle     = 0,
    NSDateFormatterShortStyle  = 1,
    NSDateFormatterMediumStyle = 2,
    NSDateFormatterLongStyle   = 3,
    NSDateFormatterFullStyle   = 4,
};

@interface NSDateFormatter : NSFormatter {
    NSString *_dateFormat;
    NSDateFormatterStyle _dateStyle;
    NSDateFormatterStyle _timeStyle;
    NSLocale *_locale;
}

- (NSString *)dateFormat;
- (void)setDateFormat:(NSString *)format;
- (NSDateFormatterStyle)dateStyle;
- (void)setDateStyle:(NSDateFormatterStyle)style;
- (NSDateFormatterStyle)timeStyle;
- (void)setTimeStyle:(NSDateFormatterStyle)style;
- (NSLocale *)locale;
- (void)setLocale:(NSLocale *)locale;

- (NSString *)stringFromDate:(NSDate *)date;
- (NSDate *)dateFromString:(NSString *)string;

@end

#endif /* NSDateFormatter_h */
