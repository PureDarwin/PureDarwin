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

@class NSMutableDictionary;

@interface NSThread : NSObject {
    NSMutableDictionary *_dictionary;
    NSMutableDictionary *_sharedDictionary;
    BOOL _main;
}

+ (NSThread *)currentThread;
+ (NSThread *)mainThread;
+ (BOOL)isMainThread;

- (BOOL)isMainThread;
- (NSMutableDictionary *)threadDictionary;

@end

#endif /* NSThread_h */
