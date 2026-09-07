/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSTimer.h>

#include <dispatch/dispatch.h>
#include <objc/message.h>

@interface NSTimer (Private)
- (instancetype)initWithTimeInterval:(NSTimeInterval)interval
                               target:(id)target
                             selector:(SEL)selector
                             userInfo:(id)userInfo
                              repeats:(BOOL)repeats;
- (void)_schedule;
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

- (void)dealloc {
    [self invalidate];
    [_target release];
    [_userInfo release];
    [super dealloc];
}

- (void)_schedule {
    if (!_valid || _source != NULL)
        return;

    uint64_t interval = (uint64_t)(_interval * (NSTimeInterval)NSEC_PER_SEC);
    _source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    if (_source == NULL) {
        _valid = NO;
        return;
    }

    dispatch_source_set_timer(_source, dispatch_time(DISPATCH_TIME_NOW, interval),
        _repeats ? interval : DISPATCH_TIME_FOREVER, interval / 20);
    dispatch_source_set_event_handler(_source, ^{
        [self fire];
    });
    dispatch_resume(_source);
}

- (void)fire {
    if (!_valid || _target == nil || _selector == NULL)
        return;

    ((void (*)(id, SEL, id))objc_msgSend)(_target, _selector, self);
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
