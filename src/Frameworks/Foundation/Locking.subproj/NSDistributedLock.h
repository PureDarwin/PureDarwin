/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDistributedLock_h
#define NSDistributedLock_h

#import <Foundation/NSObject.h>

@class NSString, NSDate;

@interface NSDistributedLock : NSObject {
    NSString *_path;
    BOOL _locked;
}

+ (NSDistributedLock *)lockWithPath:(NSString *)path;

- (instancetype)initWithPath:(NSString *)path;

- (BOOL)tryLock;
- (void)unlock;
- (void)breakLock;
- (NSDate *)lockDate;

@end

#endif /* NSDistributedLock_h */
