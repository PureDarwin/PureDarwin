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

+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key {
    return nil;
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

        if (![entry->_keyPath isEqualToString:key]) {
            continue;
        }
        if (entry->_options & NSKeyValueObservingOptionOld) {
            [entry->_priorValue release];
            entry->_priorValue = [[self valueForKeyPath:key] retain];
        }
        if (entry->_options & NSKeyValueObservingOptionPrior) {
            NSMutableDictionary *change = [NSMutableDictionary dictionary];

            [change setObject:[NSNumber numberWithUnsignedInteger:NSKeyValueChangeSetting]
                       forKey:NSKeyValueChangeKindKey];
            [change setObject:[NSNumber numberWithBool:YES]
                       forKey:NSKeyValueChangeNotificationIsPriorKey];
            [entry->_observer observeValueForKeyPath:key
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

        if (![entry->_keyPath isEqualToString:key]) {
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
            id value = [self valueForKeyPath:key];
            if (value != nil) {
                [change setObject:value forKey:NSKeyValueChangeNewKey];
            }
        }
        [entry->_observer observeValueForKeyPath:key
                                        ofObject:self
                                          change:change
                                         context:entry->_context];
    }
    [list release];
}

@end
