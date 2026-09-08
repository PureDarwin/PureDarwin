/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNumberFormatter.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSScanner.h>

/* Plain decimal formatting. The style and locale are stored so callers round
 * trip them, but only the fraction-digit settings affect the output; currency
 * and percent styles would need CFNumberFormatter. */

static NSNumberFormatterBehavior defaultBehavior = NSNumberFormatterBehaviorDefault;

@implementation NSNumberFormatter

+ (NSNumberFormatterBehavior)defaultFormatterBehavior {
    return defaultBehavior;
}

+ (void)setDefaultFormatterBehavior:(NSNumberFormatterBehavior)behavior {
    defaultBehavior = behavior;
}

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _allowsFloats = YES;
    _maximumFractionDigits = 3;
    return self;
}

- (void)dealloc {
    [_locale release];
    [_minimum release];
    [_maximum release];
    [super dealloc];
}

- (NSNumberFormatterStyle)numberStyle { return _numberStyle; }
- (void)setNumberStyle:(NSNumberFormatterStyle)style { _numberStyle = style; }

- (NSLocale *)locale { return _locale; }

- (void)setLocale:(NSLocale *)locale {
    locale = [locale retain];
    [_locale release];
    _locale = locale;
}

- (NSNumber *)minimum { return _minimum; }

- (void)setMinimum:(NSNumber *)minimum {
    minimum = [minimum retain];
    [_minimum release];
    _minimum = minimum;
}

- (NSNumber *)maximum { return _maximum; }

- (void)setMaximum:(NSNumber *)maximum {
    maximum = [maximum retain];
    [_maximum release];
    _maximum = maximum;
}

- (NSUInteger)minimumFractionDigits { return _minimumFractionDigits; }
- (void)setMinimumFractionDigits:(NSUInteger)digits { _minimumFractionDigits = digits; }
- (NSUInteger)maximumFractionDigits { return _maximumFractionDigits; }
- (void)setMaximumFractionDigits:(NSUInteger)digits { _maximumFractionDigits = digits; }

- (BOOL)allowsFloats { return _allowsFloats; }
- (void)setAllowsFloats:(BOOL)allowsFloats { _allowsFloats = allowsFloats; }

- (BOOL)generatesDecimalNumbers { return _generatesDecimalNumbers; }
- (void)setGeneratesDecimalNumbers:(BOOL)generates { _generatesDecimalNumbers = generates; }

- (NSString *)stringFromNumber:(NSNumber *)number {
    if (number == nil) {
        return nil;
    }
    if (_numberStyle == NSNumberFormatterNoStyle) {
        return [NSString stringWithFormat:@"%lld", [number longLongValue]];
    }
    return [NSString stringWithFormat:@"%.*f", (int)_maximumFractionDigits,
                                      [number doubleValue]];
}

- (NSNumber *)numberFromString:(NSString *)string {
    NSScanner *scanner = [NSScanner scannerWithString:string];

    if (_allowsFloats) {
        double value = 0.0;

        if ([scanner scanDouble:&value]) {
            return [NSNumber numberWithDouble:value];
        }
        return nil;
    }

    NSInteger value = 0;
    if ([scanner scanInteger:&value]) {
        return [NSNumber numberWithInteger:value];
    }
    return nil;
}

- (NSString *)stringForObjectValue:(id)object {
    if (![object isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    return [self stringFromNumber:object];
}

- (BOOL)getObjectValue:(out id *)object
             forString:(NSString *)string
      errorDescription:(out NSString **)error {
    NSNumber *number = [self numberFromString:string];

    if (number == nil) {
        if (error != NULL) {
            *error = @"not a number";
        }
        return NO;
    }
    if (object != NULL) {
        *object = number;
    }
    return YES;
}

@end
