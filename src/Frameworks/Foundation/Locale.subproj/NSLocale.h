/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSLocale_h
#define NSLocale_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSDictionary;

typedef NSString *NSLocaleKey;

FOUNDATION_EXPORT NSLocaleKey const NSLocaleIdentifier;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleLanguageCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCountryCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleDecimalSeparator;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleGroupingSeparator;

@interface NSLocale : NSObject <NSCopying>

+ (NSLocale *)currentLocale;
+ (NSLocale *)systemLocale;
+ (NSArray *)preferredLanguages;

- (instancetype)initWithLocaleIdentifier:(NSString *)identifier;

- (NSString *)localeIdentifier;
- (id)objectForKey:(NSLocaleKey)key;

@end

#endif /* NSLocale_h */
