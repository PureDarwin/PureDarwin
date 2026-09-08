/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSNotificationQueue_h
#define NSNotificationQueue_h

#import <Foundation/NSNotification.h>

@class NSArray, NSMutableArray, NSNotificationCenter;

typedef NS_ENUM(NSUInteger, NSPostingStyle) {
    NSPostWhenIdle = 1,
    NSPostASAP     = 2,
    NSPostNow      = 3,
};

typedef NS_OPTIONS(NSUInteger, NSNotificationCoalescing) {
    NSNotificationNoCoalescing       = 0,
    NSNotificationCoalescingOnName   = 1,
    NSNotificationCoalescingOnSender = 2,
};

@interface NSNotificationQueue : NSObject {
    NSNotificationCenter *_center;
    NSMutableArray *_asapQueue;
    NSMutableArray *_idleQueue;
}

+ (NSNotificationQueue *)defaultQueue;

- (instancetype)initWithNotificationCenter:(NSNotificationCenter *)center;

- (void)enqueueNotification:(NSNotification *)notification
               postingStyle:(NSPostingStyle)style;
- (void)enqueueNotification:(NSNotification *)notification
               postingStyle:(NSPostingStyle)style
               coalesceMask:(NSNotificationCoalescing)coalesceMask
                   forModes:(NSArray *)modes;

- (void)dequeueNotificationsMatching:(NSNotification *)notification
                        coalesceMask:(NSUInteger)coalesceMask;

@end

#endif /* NSNotificationQueue_h */
