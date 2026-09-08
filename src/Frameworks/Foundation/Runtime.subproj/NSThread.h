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
}

+ (NSThread *)currentThread;
+ (NSThread *)mainThread;
+ (BOOL)isMainThread;
+ (void)detachNewThreadSelector:(SEL)selector
                       toTarget:(id)target
                     withObject:(nullable id)object;

- (BOOL)isMainThread;
- (NSMutableDictionary *)threadDictionary;

@end


FOUNDATION_EXPORT NSThread *NSCurrentThread(void);

/* Per-thread singletons keyed by class name: Cocotron uses these for the
 * objects that are "one per thread" (NSDisplay, the graphics context stack). */
FOUNDATION_EXPORT id NSThreadSharedInstance(NSString *className);
FOUNDATION_EXPORT id NSThreadSharedInstanceDoNotCreate(NSString *className);

#endif /* NSThread_h */
