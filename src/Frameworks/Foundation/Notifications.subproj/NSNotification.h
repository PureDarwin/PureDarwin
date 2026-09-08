/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSNotification_h
#define NSNotification_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSDictionary, NSArray, NSMutableArray;

typedef NSString *NSNotificationName;

@interface NSNotification : NSObject <NSCopying> {
    NSString *_name;
    id _object;
    NSDictionary *_userInfo;
}

+ (instancetype)notificationWithName:(NSNotificationName)name object:(id)object;
+ (instancetype)notificationWithName:(NSNotificationName)name
                              object:(id)object
                            userInfo:(NSDictionary *)userInfo;

- (instancetype)initWithName:(NSNotificationName)name
                      object:(id)object
                    userInfo:(NSDictionary *)userInfo;

- (NSString *)name;
- (id)object;
- (NSDictionary *)userInfo;

@end

@interface NSNotificationCenter : NSObject {
    NSMutableArray *_observers;
}

+ (NSNotificationCenter *)defaultCenter;

- (void)addObserver:(id)observer
           selector:(SEL)selector
               name:(NSNotificationName)name
             object:(id)object;

- (void)removeObserver:(id)observer;
- (void)removeObserver:(id)observer name:(NSNotificationName)name object:(id)object;

- (void)postNotification:(NSNotification *)notification;
- (void)postNotificationName:(NSNotificationName)name object:(id)object;
- (void)postNotificationName:(NSNotificationName)name
                      object:(id)object
                    userInfo:(NSDictionary *)userInfo;

@end

#endif /* NSNotification_h */
