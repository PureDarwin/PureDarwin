/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSThread-Private.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObjCRuntime.h>

#include <objc/message.h>
#include <pthread.h>
#include <stdlib.h>

static pthread_key_t threadKey;
static pthread_once_t threadKeyOnce = PTHREAD_ONCE_INIT;
static pthread_t mainPthread;
static NSThread *mainThread;

typedef struct {
    SEL selector;
    id target;
    id object;
} NSThreadStartInfo;

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

static void *startDetachedThread(void *context) {
    NSThreadStartInfo *info = context;
    NSAutoreleasePool *pool = [NSAutoreleasePool new];

    [NSThread currentThread];
    ((void (*)(id, SEL, id))objc_msgSend)(info->target, info->selector,
        info->object);

    [info->object release];
    [info->target release];
    free(info);
    [pool drain];
    return NULL;
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

+ (void)detachNewThreadSelector:(SEL)selector
                       toTarget:(id)target
                     withObject:(id)object {
    NSThreadStartInfo *info = calloc(1, sizeof(*info));
    if (info == NULL)
        return;

    info->selector = selector;
    info->target = [target retain];
    info->object = [object retain];

    pthread_t thread;
    if (pthread_create(&thread, NULL, startDetachedThread, info) != 0) {
        [info->object release];
        [info->target release];
        free(info);
        return;
    }
    pthread_detach(thread);
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

id NSThreadSharedInstance(NSString *className) {
    NSThread *thread = [NSThread currentThread];
    NSMutableDictionary *shared = [thread sharedDictionary];
    id instance = [shared objectForKey:className];

    if (instance == nil) {
        Class cls = NSClassFromString(className);

        if (cls == Nil) {
            return nil;
        }
        instance = [[cls alloc] init];
        [shared setObject:instance forKey:className];
    }
    return instance;
}

id NSThreadSharedInstanceDoNotCreate(NSString *className) {
    return [[[NSThread currentThread] sharedDictionary] objectForKey:className];
}
