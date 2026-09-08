/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#include <objc/message.h>

@implementation NSNotification

+ (instancetype)notificationWithName:(NSNotificationName)name object:(id)object {
    return [self notificationWithName:name object:object userInfo:nil];
}

+ (instancetype)notificationWithName:(NSNotificationName)name
                              object:(id)object
                            userInfo:(NSDictionary *)userInfo {
    return [[[self alloc] initWithName:name object:object userInfo:userInfo] autorelease];
}

- (instancetype)initWithName:(NSNotificationName)name
                      object:(id)object
                    userInfo:(NSDictionary *)userInfo {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _name = [name copy];
    _object = [object retain];
    _userInfo = [userInfo retain];
    return self;
}

- (void)dealloc {
    [_name release];
    [_object release];
    [_userInfo release];
    [super dealloc];
}

- (NSString *)name {
    return _name;
}

- (id)object {
    return _object;
}

- (NSDictionary *)userInfo {
    return _userInfo;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end

/* One registration. The observer and object are held weakly: a notification
 * centre that retained them would keep every observer alive forever. */
@interface NSNotificationObservation : NSObject {
@public
    id _observer;
    SEL _selector;
    NSString *_name;
    id _object;
}
@end

@implementation NSNotificationObservation

- (void)dealloc {
    [_name release];
    [super dealloc];
}

@end

@implementation NSNotificationCenter

+ (NSNotificationCenter *)defaultCenter {
    static NSNotificationCenter *shared = nil;

    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _observers = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc {
    [_observers release];
    [super dealloc];
}

- (void)addObserver:(id)observer
           selector:(SEL)selector
               name:(NSNotificationName)name
             object:(id)object {
    NSNotificationObservation *entry = [[NSNotificationObservation alloc] init];

    entry->_observer = observer;
    entry->_selector = selector;
    entry->_name = [name copy];
    entry->_object = object;
    [_observers addObject:entry];
    [entry release];
}

- (void)removeObserver:(id)observer {
    [self removeObserver:observer name:nil object:nil];
}

- (void)removeObserver:(id)observer name:(NSNotificationName)name object:(id)object {
    NSUInteger i = [_observers count];

    while (i-- > 0) {
        NSNotificationObservation *entry = [_observers objectAtIndex:i];

        if (entry->_observer != observer) {
            continue;
        }
        if (name != nil && ![entry->_name isEqualToString:name]) {
            continue;
        }
        if (object != nil && entry->_object != object) {
            continue;
        }
        [_observers removeObjectAtIndex:i];
    }
}

- (void)postNotification:(NSNotification *)notification {
    NSString *name = [notification name];
    id object = [notification object];
    /* Copy first: an observer is allowed to add or remove registrations while
     * it is being notified. */
    NSArray *snapshot = [_observers copy];
    NSUInteger count = [snapshot count];

    for (NSUInteger i = 0; i < count; i++) {
        NSNotificationObservation *entry = [snapshot objectAtIndex:i];

        if (entry->_name != nil && ![entry->_name isEqualToString:name]) {
            continue;
        }
        if (entry->_object != nil && entry->_object != object) {
            continue;
        }
        ((void (*)(id, SEL, id))objc_msgSend)(entry->_observer, entry->_selector,
                                              notification);
    }
    [snapshot release];
}

- (void)postNotificationName:(NSNotificationName)name object:(id)object {
    [self postNotificationName:name object:object userInfo:nil];
}

- (void)postNotificationName:(NSNotificationName)name
                      object:(id)object
                    userInfo:(NSDictionary *)userInfo {
    [self postNotification:[NSNotification notificationWithName:name
                                                         object:object
                                                       userInfo:userInfo]];
}

@end
