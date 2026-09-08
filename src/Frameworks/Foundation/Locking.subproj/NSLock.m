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

@end
