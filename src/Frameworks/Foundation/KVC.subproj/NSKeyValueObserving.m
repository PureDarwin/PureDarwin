/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSLock.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSObjCRuntime.h>
#include <objc/runtime.h>
#include <objc/message.h>

NSKeyValueChangeKey const NSKeyValueChangeKindKey = @"kind";
NSKeyValueChangeKey const NSKeyValueChangeNewKey = @"new";
NSKeyValueChangeKey const NSKeyValueChangeOldKey = @"old";
NSKeyValueChangeKey const NSKeyValueChangeIndexesKey = @"indexes";
NSKeyValueChangeKey const NSKeyValueChangeNotificationIsPriorKey = @"notificationIsPrior";

/* Explicit KVO: -willChangeValueForKey:/-didChangeValueForKey: notify, and so
 * does -setValue:forKey:. There is no isa-swizzling, so a plain setter called
 * directly does NOT notify - a class that wants automatic notification has to
 * bracket its setter with will/did, which is what Cocotron's AppKit does. */

@interface NSKVOObservation : NSObject {
@public
    __unsafe_unretained NSObject *_observer;
    NSString *_keyPath;
    NSKeyValueObservingOptions _options;
    void *_context;
    id _priorValue;
}
@end

@implementation NSKVOObservation

- (void)dealloc {
    [_keyPath release];
    [_priorValue release];
    [super dealloc];
}

@end

/* Observations live in a side table keyed by observed object, so NSObject
 * gains no ivars. */
static NSMutableDictionary *observationsByObject(void) {
    static NSMutableDictionary *shared = nil;

    if (shared == nil) {
        shared = [[NSMutableDictionary alloc] init];
    }
    return shared;
}

static NSLock *observationLock(void) {
    static NSLock *shared = nil;

    if (shared == nil) {
        shared = [[NSLock alloc] init];
    }
    return shared;
}

static NSString *observationKeyForObject(id object) {
    return [NSString stringWithFormat:@"%p", object];
}

static NSMutableArray *observationsForObject(id object, BOOL create) {
    NSString *key = observationKeyForObject(object);
    NSMutableArray *list = [observationsByObject() objectForKey:key];

    if (list == nil && create) {
        list = [NSMutableArray array];
        [observationsByObject() setObject:list forKey:key];
    }
    return list;
}

@implementation NSObject (NSKeyValueObserving)

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    return YES;
}

// dependent keys registered with +setKeys:triggerChangeNotificationsForDependentKey:, per class name
static NSMutableDictionary *dependentKeysByClass(void) {
    static NSMutableDictionary *shared = nil;

    if (shared == nil)
        shared = [[NSMutableDictionary alloc] init];
    return shared;
}

+ (void)setKeys:(NSArray *)keys triggerChangeNotificationsForDependentKey:(NSString *)dependentKey {
    [observationLock() lock];
    NSString *name = NSStringFromClass(self);
    NSMutableDictionary *table = [dependentKeysByClass() objectForKey:name];

    if (table == nil) {
        table = [NSMutableDictionary dictionary];
        [dependentKeysByClass() setObject:table forKey:name];
    }
    NSMutableSet *affecting = [table objectForKey:dependentKey];

    if (affecting == nil) {
        affecting = [NSMutableSet set];
        [table setObject:affecting forKey:dependentKey];
    }
    [affecting addObjectsFromArray:keys];
    [observationLock() unlock];
}

// the keys registered for this class and its superclasses, and +keyPathsForValuesAffecting<Key>
+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key {
    NSMutableSet *result = nil;

    [observationLock() lock];
    for (Class c = self; c != Nil; c = class_getSuperclass(c)) {
        NSSet *keys = [[dependentKeysByClass() objectForKey:NSStringFromClass(c)] objectForKey:key];

        if (keys == nil)
            continue;
        if (result == nil)
            result = [NSMutableSet set];
        [result unionSet:keys];
    }
    [observationLock() unlock];

    if ([key length] > 0) {
        NSString *name = [NSString stringWithFormat:@"keyPathsForValuesAffecting%@%@",
                                   [[key substringToIndex:1] uppercaseString], [key substringFromIndex:1]];
        SEL selector = NSSelectorFromString(name);

        if ([self respondsToSelector:selector]) {
            NSSet *keys = ((NSSet *(*)(id, SEL))objc_msgSend)(self, selector);

            if (keys != nil) {
                if (result == nil)
                    result = [NSMutableSet set];
                [result unionSet:keys];
            }
        }
    }
    return result;
}

// an observation of KEYPATH also hears about the keys that affect it
static BOOL observationMatches(id object, NSKVOObservation *entry, NSString *key) {
    if ([entry->_keyPath isEqualToString:key])
        return YES;
    return [[[object class] keyPathsForValuesAffectingValueForKey:entry->_keyPath] containsObject:key];
}

- (void)addObserver:(NSObject *)observer
         forKeyPath:(NSString *)keyPath
            options:(NSKeyValueObservingOptions)options
            context:(void *)context {
    NSKVOObservation *entry = [[NSKVOObservation alloc] init];

    entry->_observer = observer;
    entry->_keyPath = [keyPath copy];
    entry->_options = options;
    entry->_context = context;

    [observationLock() lock];
    [observationsForObject(self, YES) addObject:entry];
    [observationLock() unlock];
    [entry release];

    if (options & NSKeyValueObservingOptionInitial) {
        NSMutableDictionary *change = [NSMutableDictionary dictionary];

        [change setObject:[NSNumber numberWithUnsignedInteger:NSKeyValueChangeSetting]
                   forKey:NSKeyValueChangeKindKey];
        if (options & NSKeyValueObservingOptionNew) {
            id value = [self valueForKeyPath:keyPath];
            if (value != nil) {
                [change setObject:value forKey:NSKeyValueChangeNewKey];
            }
        }
        [observer observeValueForKeyPath:keyPath ofObject:self change:change context:context];
    }
}

- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath {
    [self removeObserver:observer forKeyPath:keyPath context:NULL];
}

- (void)removeObserver:(NSObject *)observer
            forKeyPath:(NSString *)keyPath
               context:(void *)context {
    [observationLock() lock];

    NSMutableArray *list = observationsForObject(self, NO);
    NSUInteger i = [list count];

    while (i-- > 0) {
        NSKVOObservation *entry = [list objectAtIndex:i];

        if (entry->_observer == observer && [entry->_keyPath isEqualToString:keyPath]) {
            [list removeObjectAtIndex:i];
        }
    }
    if ([list count] == 0) {
        [observationsByObject() removeObjectForKey:observationKeyForObject(self)];
    }
    [observationLock() unlock];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
}

- (void)willChangeValueForKey:(NSString *)key {
    [observationLock() lock];
    NSArray *list = [observationsForObject(self, NO) copy];
    [observationLock() unlock];

    NSUInteger count = [list count];
    for (NSUInteger i = 0; i < count; i++) {
        NSKVOObservation *entry = [list objectAtIndex:i];

        if (!observationMatches(self, entry, key)) {
            continue;
        }
        if (entry->_options & NSKeyValueObservingOptionOld) {
            [entry->_priorValue release];
            entry->_priorValue = [[self valueForKeyPath:entry->_keyPath] retain];
        }
        if (entry->_options & NSKeyValueObservingOptionPrior) {
            NSMutableDictionary *change = [NSMutableDictionary dictionary];

            [change setObject:[NSNumber numberWithUnsignedInteger:NSKeyValueChangeSetting]
                       forKey:NSKeyValueChangeKindKey];
            [change setObject:[NSNumber numberWithBool:YES]
                       forKey:NSKeyValueChangeNotificationIsPriorKey];
            [entry->_observer observeValueForKeyPath:entry->_keyPath
                                            ofObject:self
                                              change:change
                                             context:entry->_context];
        }
    }
    [list release];
}

- (void)didChangeValueForKey:(NSString *)key {
    [observationLock() lock];
    NSArray *list = [observationsForObject(self, NO) copy];
    [observationLock() unlock];

    NSUInteger count = [list count];
    for (NSUInteger i = 0; i < count; i++) {
        NSKVOObservation *entry = [list objectAtIndex:i];

        if (!observationMatches(self, entry, key)) {
            continue;
        }

        NSMutableDictionary *change = [NSMutableDictionary dictionary];
        [change setObject:[NSNumber numberWithUnsignedInteger:NSKeyValueChangeSetting]
                   forKey:NSKeyValueChangeKindKey];

        if (entry->_options & NSKeyValueObservingOptionOld) {
            if (entry->_priorValue != nil) {
                [change setObject:entry->_priorValue forKey:NSKeyValueChangeOldKey];
            }
            [entry->_priorValue release];
            entry->_priorValue = nil;
        }
        if (entry->_options & NSKeyValueObservingOptionNew) {
            id value = [self valueForKeyPath:entry->_keyPath];
            if (value != nil) {
                [change setObject:value forKey:NSKeyValueChangeNewKey];
            }
        }
        [entry->_observer observeValueForKeyPath:entry->_keyPath
                                        ofObject:self
                                          change:change
                                         context:entry->_context];
    }
    [list release];
}

@end
