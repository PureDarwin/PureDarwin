/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSLock_h
#define NSLock_h

#import <Foundation/NSObject.h>
#include <pthread.h>

@class NSString, NSDate;

@protocol NSLocking
- (void)lock;
- (void)unlock;
@end

@interface NSLock : NSObject <NSLocking> {
    pthread_mutex_t _mutex;
    NSString *_name;
}

- (BOOL)tryLock;
- (BOOL)lockBeforeDate:(NSDate *)date;
- (NSString *)name;
- (void)setName:(NSString *)name;

@end

@interface NSRecursiveLock : NSObject <NSLocking> {
    pthread_mutex_t _mutex;
    NSString *_name;
}

- (BOOL)tryLock;
- (NSString *)name;
- (void)setName:(NSString *)name;

@end

@interface NSCondition : NSObject <NSLocking> {
    pthread_mutex_t _mutex;
    pthread_cond_t _condition;
    NSString *_name;
}

- (void)wait;
- (void)signal;
- (void)broadcast;

@end

#endif /* NSLock_h */
