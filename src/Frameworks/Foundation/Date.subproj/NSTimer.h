/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSTimer_h
#define NSTimer_h

#import <Foundation/NSDate.h>
#include <dispatch/dispatch.h>

@class NSDictionary;

@interface NSTimer : NSObject {
    dispatch_source_t _source;
    id _target;
    SEL _selector;
    id _userInfo;
    NSTimeInterval _interval;
    BOOL _repeats;
    BOOL _valid;
    void (^_block)(NSTimer *timer);
    NSDate *_fireDate;
    NSTimeInterval _tolerance;
}

+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                     target:(id)target
                                   selector:(SEL)selector
                                   userInfo:(id)userInfo
                                    repeats:(BOOL)repeats;

+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)interval
                            target:(id)target
                          selector:(SEL)selector
                          userInfo:(id)userInfo
                           repeats:(BOOL)repeats;

+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                    repeats:(BOOL)repeats
                                      block:(void (^)(NSTimer *timer))block;
+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)interval
                           repeats:(BOOL)repeats
                             block:(void (^)(NSTimer *timer))block;
- (instancetype)initWithFireDate:(NSDate *)date
                        interval:(NSTimeInterval)interval
                         repeats:(BOOL)repeats
                           block:(void (^)(NSTimer *timer))block;
- (instancetype)initWithFireDate:(NSDate *)date
                        interval:(NSTimeInterval)interval
                          target:(id)target
                        selector:(SEL)selector
                        userInfo:(id)userInfo
                         repeats:(BOOL)repeats;
- (NSDate *)fireDate;
- (void)setFireDate:(NSDate *)date;
- (NSTimeInterval)tolerance;
- (void)setTolerance:(NSTimeInterval)tolerance;
- (void)fire;
- (void)invalidate;
- (BOOL)isValid;
- (NSTimeInterval)timeInterval;
- (id)userInfo;

@end

#endif /* NSTimer_h */
