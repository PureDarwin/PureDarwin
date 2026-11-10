// NSDateInterval.h - a span of time from a start date, with its duration
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@interface NSDateInterval : NSObject <NSCopying> {
    NSDate *_startDate;
    NSTimeInterval _duration;
}

- (instancetype)initWithStartDate:(NSDate *)startDate duration:(NSTimeInterval)duration;
- (instancetype)initWithStartDate:(NSDate *)startDate endDate:(NSDate *)endDate;

@property (readonly, copy) NSDate *startDate;
@property (readonly, copy) NSDate *endDate;
@property (readonly) NSTimeInterval duration;

- (BOOL)containsDate:(NSDate *)date;
- (BOOL)intersectsDateInterval:(NSDateInterval *)dateInterval;
- (NSDateInterval *)intersectionWithDateInterval:(NSDateInterval *)dateInterval;
- (BOOL)isEqualToDateInterval:(NSDateInterval *)dateInterval;

@end
