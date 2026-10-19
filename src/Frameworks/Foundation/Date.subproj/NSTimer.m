/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSTimer.h>
#import <Foundation/NSDate.h>

#include <dispatch/dispatch.h>
#include <pthread.h>
#include <objc/message.h>

@interface NSTimer (Private)
- (instancetype)initWithTimeInterval:(NSTimeInterval)interval
                               target:(id)target
                             selector:(SEL)selector
                             userInfo:(id)userInfo
                              repeats:(BOOL)repeats;
- (void)_schedule;
- (void)_scheduleOnQueue:(dispatch_queue_t)queue;
@end

@implementation NSTimer

+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                     target:(id)target
                                   selector:(SEL)selector
                                   userInfo:(id)userInfo
                                    repeats:(BOOL)repeats {
    NSTimer *timer = [self timerWithTimeInterval:interval
                                          target:target
                                        selector:selector
                                        userInfo:userInfo
                                         repeats:repeats];
    [timer _schedule];
    return timer;
}

+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)interval
                            target:(id)target
                          selector:(SEL)selector
                          userInfo:(id)userInfo
                           repeats:(BOOL)repeats {
    return [[[self alloc] initWithTimeInterval:interval
                                        target:target
                                      selector:selector
                                      userInfo:userInfo
                                       repeats:repeats] autorelease];
}

- (instancetype)initWithTimeInterval:(NSTimeInterval)interval
                               target:(id)target
                             selector:(SEL)selector
                             userInfo:(id)userInfo
                              repeats:(BOOL)repeats {
    self = [super init];
    if (self == nil)
        return nil;

    _interval = interval > 0.0 ? interval : 0.000001;
    _target = [target retain];
    _selector = selector;
    _userInfo = [userInfo retain];
    _repeats = repeats;
    _valid = YES;
    return self;
}

+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                    repeats:(BOOL)repeats
                                      block:(void (^)(NSTimer *timer))block {
    NSTimer *timer = [self timerWithTimeInterval:interval repeats:repeats block:block];
    [timer _schedule];
    return timer;
}

+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)interval
                           repeats:(BOOL)repeats
                             block:(void (^)(NSTimer *timer))block {
    return [[[self alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:interval]
                                  interval:interval repeats:repeats block:block] autorelease];
}

- (instancetype)initWithFireDate:(NSDate *)date
                        interval:(NSTimeInterval)interval
                         repeats:(BOOL)repeats
                           block:(void (^)(NSTimer *timer))block {
    self = [self initWithTimeInterval:interval target:nil selector:NULL userInfo:nil repeats:repeats];
    if (self == nil)
        return nil;
    _block = [block copy];
    _fireDate = [date retain];
    return self;
}

- (instancetype)initWithFireDate:(NSDate *)date
                        interval:(NSTimeInterval)interval
                          target:(id)target
                        selector:(SEL)selector
                        userInfo:(id)userInfo
                         repeats:(BOOL)repeats {
    self = [self initWithTimeInterval:interval target:target selector:selector userInfo:userInfo repeats:repeats];
    if (self == nil)
        return nil;
    _fireDate = [date retain];
    return self;
}

- (NSDate *)fireDate {
    return _fireDate ?: [NSDate dateWithTimeIntervalSinceNow:_interval];
}

// a scheduled timer restarts from the new date
- (void)setFireDate:(NSDate *)date {
    BOOL scheduled = (_source != NULL);
    dispatch_queue_t queue = pthread_main_np() ? dispatch_get_main_queue()
                                               : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    date = [date retain];
    [_fireDate release];
    _fireDate = date;
    if (scheduled && _valid) {
        dispatch_source_cancel(_source);
        dispatch_release(_source);
        _source = NULL;
        [self _scheduleOnQueue:queue];
    }
}

- (NSTimeInterval)tolerance {
    return _tolerance;
}

- (void)setTolerance:(NSTimeInterval)tolerance {
    _tolerance = tolerance > 0 ? tolerance : 0;
}

- (void)dealloc {
    [self invalidate];
    [_target release];
    [_userInfo release];
    [_block release];
    [_fireDate release];
    [super dealloc];
}

// a timer fires on the thread whose run loop has it: the main queue for the main thread, which the
// application's event loop drains, other threads run no queue of their own, so those use a global one
- (void)_schedule {
    [self _scheduleOnQueue:pthread_main_np() ? dispatch_get_main_queue()
                                             : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
}

- (void)_scheduleOnQueue:(dispatch_queue_t)queue {
    if (!_valid || _source != NULL)
        return;

    uint64_t interval = (uint64_t)(_interval * (NSTimeInterval)NSEC_PER_SEC);
    _source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (_source == NULL) {
        _valid = NO;
        return;
    }

    NSTimeInterval delay = _fireDate ? [_fireDate timeIntervalSinceNow] : _interval;
    uint64_t leeway = _tolerance > 0 ? (uint64_t)(_tolerance * (NSTimeInterval)NSEC_PER_SEC) : interval / 20;

    if (delay < 0)
        delay = 0;
    dispatch_source_set_timer(_source, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * (NSTimeInterval)NSEC_PER_SEC)),
        _repeats ? interval : DISPATCH_TIME_FOREVER, leeway);
    dispatch_source_set_event_handler(_source, ^{
        [self fire];
    });
    dispatch_resume(_source);
}

- (void)fire {
    if (!_valid)
        return;
    if (_block != nil)
        _block(self);
    else if (_target != nil && _selector != NULL)
        ((void (*)(id, SEL, id))objc_msgSend)(_target, _selector, self);
    else
        return;
    if (_repeats) {
        [_fireDate release];
        _fireDate = [[NSDate dateWithTimeIntervalSinceNow:_interval] retain];
    }
    if (!_repeats)
        [self invalidate];
}

- (void)invalidate {
    if (!_valid && _source == NULL)
        return;

    _valid = NO;
    if (_source != NULL) {
        dispatch_source_cancel(_source);
        dispatch_release(_source);
        _source = NULL;
    }
}

- (BOOL)isValid {
    return _valid;
}

- (NSTimeInterval)timeInterval {
    return _interval;
}

- (id)userInfo {
    return _userInfo;
}

@end
