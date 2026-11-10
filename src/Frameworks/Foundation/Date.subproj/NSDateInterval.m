// NSDateInterval.m - a span of time from a start date, with its duration
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSDateInterval.h>

@implementation NSDateInterval

- (instancetype)init {
    return [self initWithStartDate:[NSDate date] duration:0];
}

- (instancetype)initWithStartDate:(NSDate *)startDate duration:(NSTimeInterval)duration {
    if ((self = [super init])) {
        _startDate = [startDate copy];
        _duration = duration < 0 ? 0 : duration;
    }
    return self;
}

- (instancetype)initWithStartDate:(NSDate *)startDate endDate:(NSDate *)endDate {
    return [self initWithStartDate:startDate duration:[endDate timeIntervalSinceDate:startDate]];
}

- (void)dealloc {
    [_startDate release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSDate *)startDate { return _startDate; }
- (NSDate *)endDate { return [_startDate dateByAddingTimeInterval:_duration]; }
- (NSTimeInterval)duration { return _duration; }

- (BOOL)containsDate:(NSDate *)date {
    NSTimeInterval offset = [date timeIntervalSinceDate:_startDate];

    return offset >= 0 && offset <= _duration;
}

- (BOOL)intersectsDateInterval:(NSDateInterval *)other {
    return [self intersectionWithDateInterval:other] != nil;
}

// nil when the two do not meet, a zero-length interval when they only touch
- (NSDateInterval *)intersectionWithDateInterval:(NSDateInterval *)other {
    NSTimeInterval start = [_startDate timeIntervalSinceReferenceDate];
    NSTimeInterval otherStart = [[other startDate] timeIntervalSinceReferenceDate];
    NSTimeInterval from = start > otherStart ? start : otherStart;
    NSTimeInterval to = start + _duration < otherStart + [other duration] ? start + _duration : otherStart + [other duration];

    if (other == nil || to < from)
        return nil;
    return [[[NSDateInterval alloc] initWithStartDate:[NSDate dateWithTimeIntervalSinceReferenceDate:from]
        duration:to - from] autorelease];
}

- (BOOL)isEqualToDateInterval:(NSDateInterval *)other {
    return other && [_startDate isEqualToDate:[other startDate]] && _duration == [other duration];
}

- (BOOL)isEqual:(id)object {
    return [object isKindOfClass:[NSDateInterval class]] && [self isEqualToDateInterval:object];
}

- (NSUInteger)hash {
    return [_startDate hash] ^ (NSUInteger)_duration;
}

@end
