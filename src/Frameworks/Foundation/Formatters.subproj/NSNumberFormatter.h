/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSNumberFormatter_h
#define NSNumberFormatter_h

#import <Foundation/NSFormatter.h>

@class NSNumber, NSLocale;

typedef NS_ENUM(NSUInteger, NSNumberFormatterStyle) {
    NSNumberFormatterNoStyle        = 0,
    NSNumberFormatterDecimalStyle   = 1,
    NSNumberFormatterCurrencyStyle  = 2,
    NSNumberFormatterPercentStyle   = 3,
    NSNumberFormatterScientificStyle = 4,
    NSNumberFormatterSpellOutStyle  = 5,
};

typedef NS_ENUM(NSUInteger, NSNumberFormatterBehavior) {
    NSNumberFormatterBehaviorDefault = 0,
    NSNumberFormatterBehavior10_0 = 1000,
    NSNumberFormatterBehavior10_4 = 1040,
};

@interface NSNumberFormatter : NSFormatter {
    NSNumberFormatterStyle _numberStyle;
    NSLocale *_locale;
    NSNumber *_minimum;
    NSNumber *_maximum;
    NSUInteger _minimumFractionDigits;
    NSUInteger _maximumFractionDigits;
    BOOL _generatesDecimalNumbers;
    BOOL _allowsFloats;
}

+ (NSNumberFormatterBehavior)defaultFormatterBehavior;
+ (void)setDefaultFormatterBehavior:(NSNumberFormatterBehavior)behavior;

- (NSNumberFormatterStyle)numberStyle;
- (void)setNumberStyle:(NSNumberFormatterStyle)style;

- (NSLocale *)locale;
- (void)setLocale:(NSLocale *)locale;

- (NSNumber *)minimum;
- (void)setMinimum:(NSNumber *)minimum;
- (NSNumber *)maximum;
- (void)setMaximum:(NSNumber *)maximum;

- (NSUInteger)minimumFractionDigits;
- (void)setMinimumFractionDigits:(NSUInteger)digits;
- (NSUInteger)maximumFractionDigits;
- (void)setMaximumFractionDigits:(NSUInteger)digits;

- (BOOL)allowsFloats;
- (void)setAllowsFloats:(BOOL)allowsFloats;

- (BOOL)generatesDecimalNumbers;
- (void)setGeneratesDecimalNumbers:(BOOL)generates;

- (NSString *)stringFromNumber:(NSNumber *)number;
- (NSNumber *)numberFromString:(NSString *)string;

@end

#endif /* NSNumberFormatter_h */
