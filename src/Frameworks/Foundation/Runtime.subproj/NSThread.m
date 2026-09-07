/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSThread-Private.h>
#import <Foundation/NSDictionary.h>

#include <pthread.h>

static pthread_key_t threadKey;
static pthread_once_t threadKeyOnce = PTHREAD_ONCE_INIT;
static pthread_t mainPthread;
static NSThread *mainThread;

static void destroyThread(void *value) {
    [(id)value release];
}

static void createThreadKey(void) {
    pthread_key_create(&threadKey, destroyThread);
    mainPthread = pthread_self();
}

__attribute__((constructor))
static void initializeThreadKey(void) {
    pthread_once(&threadKeyOnce, createThreadKey);
}

@implementation NSThread

- (instancetype)init {
    self = [super init];
    if (self == nil)
        return nil;

    _dictionary = [NSMutableDictionary new];
    _sharedDictionary = [NSMutableDictionary new];
    return self;
}

- (void)dealloc {
    [_dictionary release];
    [_sharedDictionary release];
    [super dealloc];
}

+ (NSThread *)currentThread {
    pthread_once(&threadKeyOnce, createThreadKey);

    NSThread *thread = pthread_getspecific(threadKey);
    if (thread == nil) {
        thread = [NSThread new];
        thread->_main = pthread_equal(pthread_self(), mainPthread);
        pthread_setspecific(threadKey, thread);
        if (thread->_main)
            mainThread = thread;
    }
    return thread;
}

+ (NSThread *)mainThread {
    if (mainThread == nil)
        [self currentThread];
    return mainThread;
}

+ (BOOL)isMainThread {
    return [[self currentThread] isMainThread];
}

- (BOOL)isMainThread {
    return _main;
}

- (NSMutableDictionary *)threadDictionary {
    return _dictionary;
}

- (NSMutableDictionary *)sharedDictionary {
    return _sharedDictionary;
}

@end

NSThread *NSCurrentThread(void) {
    return [NSThread currentThread];
}
