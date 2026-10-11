/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNotification.h>
#import <Foundation/NSOperation.h>
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

// the zeroing-weak calls libobjc exports (objc-internal.h)
OBJC_EXPORT id objc_storeWeak(id *location, id obj);
OBJC_EXPORT id objc_storeWeakOrNil(id *location, id obj);
OBJC_EXPORT id objc_loadWeakRetained(id *location);

// one registration, observer and object held weakly. The observer reference zeroes when it goes away (macOS 10.11+)
// and _observer keeps the raw pointer for removeObserver: from the observer's own -dealloc
@interface NSNotificationObservation : NSObject {
@public
    id _observer;
    id _weakObserver;
    BOOL _zeroing;
    SEL _selector;
    NSString *_name;
    id _object;
    // a block observer the center keeps alive until it is removed
    id _owned;
}
@end

@implementation NSNotificationObservation

- (void)dealloc {
    if (_zeroing) {
        objc_storeWeak(&_weakObserver, nil);
    }
    [_owned release];
    [_name release];
    [super dealloc];
}

@end

// the observer addObserverForName:object:queue:usingBlock: returns, runs its block for each notification
@interface NSNotificationBlockObserver : NSObject {
@public
    void (^_block)(NSNotification *);
    NSOperationQueue *_queue;
}
- (void)_fire:(NSNotification *)notification;
@end

@implementation NSNotificationBlockObserver

- (void)dealloc {
    [_block release];
    [_queue release];
    [super dealloc];
}

// another queue runs the block while the poster waits, as on macOS
- (void)_fire:(NSNotification *)notification {
    if (_queue == nil || _queue == [NSOperationQueue currentQueue]) {
        _block(notification);
        return;
    }
    void (^block)(NSNotification *) = _block;
    NSBlockOperation *operation = [NSBlockOperation blockOperationWithBlock:^{ block(notification); }];

    [_queue addOperations:[NSArray arrayWithObject:operation] waitUntilFinished:YES];
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
    // an object that cannot be referenced weakly stays a plain pointer
    entry->_zeroing = observer != nil && objc_storeWeakOrNil(&entry->_weakObserver, observer) != nil;
    entry->_selector = selector;
    entry->_name = [name copy];
    entry->_object = object;
    [_observers addObject:entry];
    [entry release];
}

- (id)addObserverForName:(NSNotificationName)name
                   object:(id)object
                    queue:(NSOperationQueue *)queue
               usingBlock:(void (^)(NSNotification *))block {
    NSNotificationBlockObserver *observer = [[NSNotificationBlockObserver alloc] init];
    NSNotificationObservation *entry = [[NSNotificationObservation alloc] init];

    observer->_block = [block copy];
    observer->_queue = [queue retain];
    entry->_observer = observer;
    entry->_owned = observer;
    entry->_selector = @selector(_fire:);
    entry->_name = [name copy];
    entry->_object = object;
    [_observers addObject:entry];
    [entry release];
    return observer;
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
        id target = entry->_observer;

        if (entry->_zeroing) {
            target = objc_loadWeakRetained(&entry->_weakObserver);
            if (target == nil) {
                [_observers removeObjectIdenticalTo:entry];
                continue;
            }
        }
        ((void (*)(id, SEL, id))objc_msgSend)(target, entry->_selector,
                                              notification);
        if (entry->_zeroing) {
            [target release];
        }
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
