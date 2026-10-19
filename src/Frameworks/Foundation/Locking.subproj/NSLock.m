/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSLock.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDate.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

// an NSDate as the absolute CLOCK_REALTIME time pthread_cond_timedwait takes, nil means now
static void absoluteTimeout(NSDate *limit, struct timespec *ts) {
    NSTimeInterval t = limit ? [limit timeIntervalSince1970] : [[NSDate date] timeIntervalSince1970];

    if (t < 0) t = 0;
    ts->tv_sec = (time_t)t;
    ts->tv_nsec = (long)((t - (double)ts->tv_sec) * 1e9);
}

@implementation NSLock

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    pthread_mutex_init(&_mutex, NULL);
    return self;
}

- (void)dealloc {
    pthread_mutex_destroy(&_mutex);
    [_name release];
    [super dealloc];
}

- (void)lock {
    pthread_mutex_lock(&_mutex);
}

- (void)unlock {
    pthread_mutex_unlock(&_mutex);
}

- (BOOL)tryLock {
    return pthread_mutex_trylock(&_mutex) == 0;
}

/* Polls rather than using pthread_mutex_timedlock, which is not available
 * here; the granularity is fine for the callers that use this. */
- (BOOL)lockBeforeDate:(NSDate *)date {
    while (![self tryLock]) {
        if ([date timeIntervalSinceNow] <= 0.0) {
            return NO;
        }
        usleep(1000);
    }
    return YES;
}

- (NSString *)name {
    return _name;
}

- (void)setName:(NSString *)name {
    name = [name copy];
    [_name release];
    _name = name;
}

@end

@implementation NSRecursiveLock

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    return self;
}

- (void)dealloc {
    pthread_mutex_destroy(&_mutex);
    [_name release];
    [super dealloc];
}

- (void)lock {
    pthread_mutex_lock(&_mutex);
}

- (void)unlock {
    pthread_mutex_unlock(&_mutex);
}

- (BOOL)tryLock {
    return pthread_mutex_trylock(&_mutex) == 0;
}

- (NSString *)name {
    return _name;
}

- (void)setName:(NSString *)name {
    name = [name copy];
    [_name release];
    _name = name;
}

@end

@implementation NSCondition

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_condition, NULL);
    return self;
}

- (void)dealloc {
    pthread_cond_destroy(&_condition);
    pthread_mutex_destroy(&_mutex);
    [_name release];
    [super dealloc];
}

- (void)lock {
    pthread_mutex_lock(&_mutex);
}

- (void)unlock {
    pthread_mutex_unlock(&_mutex);
}

- (void)wait {
    pthread_cond_wait(&_condition, &_mutex);
}

- (void)signal {
    pthread_cond_signal(&_condition);
}

- (void)broadcast {
    pthread_cond_broadcast(&_condition);
}

- (BOOL)waitUntilDate:(NSDate *)limit {
    struct timespec ts;

    absoluteTimeout(limit, &ts);
    return pthread_cond_timedwait(&_condition, &_mutex, &ts) == 0;
}

- (NSString *)name {
    return _name;
}

- (void)setName:(NSString *)name {
    name = [name copy];
    [_name release];
    _name = name;
}

@end

@implementation NSConditionLock

- (instancetype)init {
    return [self initWithCondition:0];
}

- (instancetype)initWithCondition:(NSInteger)condition {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_changed, NULL);
    _condition = condition;
    return self;
}

- (void)dealloc {
    pthread_cond_destroy(&_changed);
    pthread_mutex_destroy(&_mutex);
    [_name release];
    [super dealloc];
}

- (NSInteger)condition {
    return _condition;
}

// waits until the lock is free and, if asked, holds the condition, a NULL timeout waits forever
- (BOOL)acquireWhen:(BOOL)matchCondition condition:(NSInteger)condition timeout:(const struct timespec *)ts {
    BOOL got = YES;

    pthread_mutex_lock(&_mutex);
    while (_locked || (matchCondition && _condition != condition)) {
        if (ts == NULL) {
            pthread_cond_wait(&_changed, &_mutex);
        } else if (pthread_cond_timedwait(&_changed, &_mutex, ts) != 0) {
            got = NO;
            break;
        }
    }
    if (got) {
        _locked = YES;
        _owner = pthread_self();
    }
    pthread_mutex_unlock(&_mutex);
    return got;
}

- (void)lock {
    [self acquireWhen:NO condition:0 timeout:NULL];
}

- (void)lockWhenCondition:(NSInteger)condition {
    [self acquireWhen:YES condition:condition timeout:NULL];
}

- (BOOL)tryLock {
    BOOL got = NO;

    pthread_mutex_lock(&_mutex);
    if (!_locked) {
        _locked = YES;
        _owner = pthread_self();
        got = YES;
    }
    pthread_mutex_unlock(&_mutex);
    return got;
}

- (BOOL)tryLockWhenCondition:(NSInteger)condition {
    BOOL got = NO;

    pthread_mutex_lock(&_mutex);
    if (!_locked && _condition == condition) {
        _locked = YES;
        _owner = pthread_self();
        got = YES;
    }
    pthread_mutex_unlock(&_mutex);
    return got;
}

- (BOOL)lockBeforeDate:(NSDate *)limit {
    struct timespec ts;

    absoluteTimeout(limit, &ts);
    return [self acquireWhen:NO condition:0 timeout:&ts];
}

- (BOOL)lockWhenCondition:(NSInteger)condition beforeDate:(NSDate *)limit {
    struct timespec ts;

    absoluteTimeout(limit, &ts);
    return [self acquireWhen:YES condition:condition timeout:&ts];
}

- (void)unlock {
    pthread_mutex_lock(&_mutex);
    _locked = NO;
    pthread_cond_broadcast(&_changed);
    pthread_mutex_unlock(&_mutex);
}

- (void)unlockWithCondition:(NSInteger)condition {
    pthread_mutex_lock(&_mutex);
    _condition = condition;
    _locked = NO;
    pthread_cond_broadcast(&_changed);
    pthread_mutex_unlock(&_mutex);
}

- (NSString *)name {
    return _name;
}

- (void)setName:(NSString *)name {
    name = [name copy];
    [_name release];
    _name = name;
}

@end
