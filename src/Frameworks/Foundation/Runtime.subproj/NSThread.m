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

#include <CoreFoundation/CFRunLoop.h>
#include <dispatch/dispatch.h>
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
    [_name release];
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


/* The object form of a thread. -start runs the target on a detached pthread and
 * flips the flags a caller polls with -isExecuting/-isFinished; there is no
 * pre-emptive cancel, so -cancel only records the request, as Cocoa's does. */
static void *startThreadObject(void *context) {
    NSThread *thread = (NSThread *)context;
    NSAutoreleasePool *pool = [NSAutoreleasePool new];

    [thread __runThreadBody];
    [pool drain];
    [thread release];
    return NULL;
}

- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(id)argument {
    if ((self = [self init]) != nil) {
        _target = [target retain];
        _argument = [argument retain];
        _selector = selector;
    }
    return self;
}

- (void)__runThreadBody {
    _executing = YES;

    if (_target != nil && _selector != NULL) {
        ((void (*)(id, SEL, id))objc_msgSend)(_target, _selector, _argument);
    }

    _executing = NO;
    _finished = YES;

    [_argument release];
    _argument = nil;
    [_target release];
    _target = nil;
}

- (void)start {
    if (_executing || _finished) {
        return;
    }

    pthread_t thread;

    /* Retained for the thread's lifetime; startThreadObject releases it. */
    [self retain];

    if (pthread_create(&thread, NULL, startThreadObject, self) != 0) {
        [self release];
        _finished = YES;
        return;
    }
    pthread_detach(thread);
}

- (void)cancel {
    _cancelled = YES;
}

- (BOOL)isExecuting {
    return _executing;
}

- (BOOL)isFinished {
    return _finished;
}

- (BOOL)isCancelled {
    return _cancelled;
}

/* Thread names are advisory; Gershwin sets them for its worker threads. */
- (NSString *)name {
    return _name;
}

- (void)setName:(NSString *)name {
    if (_name == name) {
        return;
    }
    [_name release];
    _name = [name copy];
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


@implementation NSObject (NSThreadPerformAdditions)

- (void)performSelectorOnMainThread:(SEL)selector
                         withObject:(id)object
                      waitUntilDone:(BOOL)wait {
    if (selector == NULL) {
        return;
    }

    CFRunLoopRef main = CFRunLoopGetMain();

    /* Already there, or no main run loop to hop to: just call it. Hopping in
     * the first case would deadlock a waitUntilDone:YES against itself. */
    if (main == NULL || [NSThread isMainThread]) {
        ((void (*)(id, SEL, id))objc_msgSend)(self, selector, object);
        return;
    }

    if (!wait) {
        id target = [self retain];
        id argument = [object retain];

        CFRunLoopPerformBlock(main, kCFRunLoopCommonModes, ^{
            ((void (*)(id, SEL, id))objc_msgSend)(target, selector, argument);
            [argument release];
            [target release];
        });
        CFRunLoopWakeUp(main);
        return;
    }

    dispatch_semaphore_t done = dispatch_semaphore_create(0);

    CFRunLoopPerformBlock(main, kCFRunLoopCommonModes, ^{
        ((void (*)(id, SEL, id))objc_msgSend)(self, selector, object);
        dispatch_semaphore_signal(done);
    });
    CFRunLoopWakeUp(main);

    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    dispatch_release(done);
}

@end
