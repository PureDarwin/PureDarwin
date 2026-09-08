/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSKeyValueObserving_h
#define NSKeyValueObserving_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSDictionary, NSSet, NSIndexSet;

typedef NS_OPTIONS(NSUInteger, NSKeyValueObservingOptions) {
    NSKeyValueObservingOptionNew    = 0x01,
    NSKeyValueObservingOptionOld    = 0x02,
    NSKeyValueObservingOptionInitial = 0x04,
    NSKeyValueObservingOptionPrior  = 0x08,
};

typedef NS_ENUM(NSUInteger, NSKeyValueChange) {
    NSKeyValueChangeSetting     = 1,
    NSKeyValueChangeInsertion   = 2,
    NSKeyValueChangeRemoval     = 3,
    NSKeyValueChangeReplacement = 4,
};

typedef NSString *NSKeyValueChangeKey;

FOUNDATION_EXPORT NSKeyValueChangeKey const NSKeyValueChangeKindKey;
FOUNDATION_EXPORT NSKeyValueChangeKey const NSKeyValueChangeNewKey;
FOUNDATION_EXPORT NSKeyValueChangeKey const NSKeyValueChangeOldKey;
FOUNDATION_EXPORT NSKeyValueChangeKey const NSKeyValueChangeIndexesKey;
FOUNDATION_EXPORT NSKeyValueChangeKey const NSKeyValueChangeNotificationIsPriorKey;

@interface NSObject (NSKeyValueObserving)

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key;
+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key;

- (void)addObserver:(NSObject *)observer
         forKeyPath:(NSString *)keyPath
            options:(NSKeyValueObservingOptions)options
            context:(void *)context;

- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath;
- (void)removeObserver:(NSObject *)observer
            forKeyPath:(NSString *)keyPath
               context:(void *)context;

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context;

- (void)willChangeValueForKey:(NSString *)key;
- (void)didChangeValueForKey:(NSString *)key;

@end

#endif /* NSKeyValueObserving_h */
