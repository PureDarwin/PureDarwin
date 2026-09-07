/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDate_h
#define NSDate_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

typedef double NSTimeInterval;

@interface NSDate : NSObject

+ (instancetype)date;
+ (instancetype)now;
+ (instancetype)dateWithTimeIntervalSinceNow:(NSTimeInterval)seconds;
+ (instancetype)dateWithTimeIntervalSinceReferenceDate:(NSTimeInterval)seconds;
+ (instancetype)dateWithTimeIntervalSince1970:(NSTimeInterval)seconds;

- (NSTimeInterval)timeIntervalSinceDate:(NSDate *)other;
- (NSTimeInterval)timeIntervalSinceNow;
- (NSTimeInterval)timeIntervalSinceReferenceDate;
- (NSTimeInterval)timeIntervalSince1970;
- (instancetype)dateByAddingTimeInterval:(NSTimeInterval)seconds;

@end

#endif /* NSDate_h */
