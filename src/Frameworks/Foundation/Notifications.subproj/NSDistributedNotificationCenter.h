/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDistributedNotificationCenter_h
#define NSDistributedNotificationCenter_h

#import <Foundation/NSNotification.h>

@class NSString, NSDictionary;

typedef NSString *NSNotificationCenterType;

FOUNDATION_EXPORT NSNotificationCenterType const NSLocalNotificationCenterType;

typedef NS_ENUM(NSUInteger, NSNotificationSuspensionBehavior) {
    NSNotificationSuspensionBehaviorDrop = 1,
    NSNotificationSuspensionBehaviorCoalesce = 2,
    NSNotificationSuspensionBehaviorHold = 3,
    NSNotificationSuspensionBehaviorDeliverImmediately = 4
};

typedef NS_OPTIONS(NSUInteger, NSDistributedNotificationOptions) {
    NSDistributedNotificationDeliverImmediately = 1 << 0,
    NSDistributedNotificationPostToAllSessions = 1 << 1
};

/* Notifications reach other processes; observers are matched locally by the
 * inherited NSNotificationCenter machinery. */
@interface NSDistributedNotificationCenter : NSNotificationCenter {
    int _notifyToken;
    long long _offset;
    BOOL _suspended;
    id _queue;
}

+ (NSDistributedNotificationCenter *)defaultCenter;
+ (NSDistributedNotificationCenter *)notificationCenterForType:(NSNotificationCenterType)type;

- (void)addObserver:(id)observer
           selector:(SEL)selector
               name:(NSNotificationName)name
             object:(NSString *)object
 suspensionBehavior:(NSNotificationSuspensionBehavior)suspensionBehavior;

- (void)postNotificationName:(NSNotificationName)name object:(NSString *)object;
- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo;
- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo
          deliverImmediately:(BOOL)deliverImmediately;
- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo
                     options:(NSDistributedNotificationOptions)options;

- (void)setSuspended:(BOOL)suspended;
- (BOOL)isSuspended;

@end

#endif /* NSDistributedNotificationCenter_h */
