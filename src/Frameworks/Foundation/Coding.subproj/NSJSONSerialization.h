/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSJSONSerialization_h
#define NSJSONSerialization_h

#import <Foundation/NSObject.h>

@class NSData, NSError, NSString;

typedef NS_OPTIONS(NSUInteger, NSJSONReadingOptions) {
    NSJSONReadingMutableContainers = 1 << 0,
    NSJSONReadingMutableLeaves = 1 << 1,
    NSJSONReadingFragmentsAllowed = 1 << 2,
    NSJSONReadingAllowFragments = NSJSONReadingFragmentsAllowed
};

typedef NS_OPTIONS(NSUInteger, NSJSONWritingOptions) {
    NSJSONWritingPrettyPrinted = 1 << 0,
    NSJSONWritingSortedKeys = 1 << 1,
    NSJSONWritingFragmentsAllowed = 1 << 2
};

@interface NSJSONSerialization : NSObject

+ (BOOL)isValidJSONObject:(id)object;

+ (NSData *)dataWithJSONObject:(id)object
                       options:(NSJSONWritingOptions)options
                         error:(NSError **)error;

+ (id)JSONObjectWithData:(NSData *)data
                 options:(NSJSONReadingOptions)options
                   error:(NSError **)error;

@end

#endif /* NSJSONSerialization_h */
