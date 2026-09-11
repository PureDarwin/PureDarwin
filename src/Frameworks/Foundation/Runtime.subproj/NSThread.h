/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSThread_h
#define NSThread_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSMutableDictionary, NSString;

@interface NSThread : NSObject {
    NSMutableDictionary *_dictionary;
    NSMutableDictionary *_sharedDictionary;
    BOOL _main;
    /* Set only for a thread created with -initWithTarget:...; the class
     * methods above run detached and have nothing to report. */
    SEL _selector;
    id _target;
    id _argument;
    BOOL _executing;
    BOOL _finished;
    BOOL _cancelled;
    NSString *_name;
}

+ (NSThread *)currentThread;
+ (NSThread *)mainThread;
+ (BOOL)isMainThread;
+ (void)detachNewThreadSelector:(SEL)selector
                       toTarget:(id)target
                     withObject:(nullable id)object;

- (BOOL)isMainThread;
- (NSMutableDictionary *)threadDictionary;

- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(id)argument;
- (void)start;
- (void)cancel;
- (BOOL)isExecuting;
- (BOOL)isFinished;
- (BOOL)isCancelled;
- (NSString *)name;
- (void)setName:(NSString *)name;

@end

@interface NSObject (NSThreadPerformAdditions)
/* Hops to the main run loop. When already on the main thread the selector is
 * invoked directly, which is what callers expect and avoids deadlocking a
 * waitUntilDone:YES call against itself. */
- (void)performSelectorOnMainThread:(SEL)selector
                         withObject:(id)object
                      waitUntilDone:(BOOL)wait;
@end


FOUNDATION_EXPORT NSThread *NSCurrentThread(void);

/* Per-thread singletons keyed by class name: Cocotron uses these for the
 * objects that are "one per thread" (NSDisplay, the graphics context stack). */
FOUNDATION_EXPORT id NSThreadSharedInstance(NSString *className);
FOUNDATION_EXPORT id NSThreadSharedInstanceDoNotCreate(NSString *className);

#endif /* NSThread_h */
