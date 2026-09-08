/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCountedSet_h
#define NSCountedSet_h

#import <Foundation/NSSet.h>

@class NSArray, NSMutableDictionary, NSEnumerator;

/* A set that remembers how many times each object was added. Bindings use it
 * to track how many observers hold a key. */
@interface NSCountedSet : NSSet {
    NSMutableDictionary *_counts;
}

- (instancetype)initWithArray:(NSArray *)array;
- (instancetype)initWithCapacity:(NSUInteger)capacity;

- (NSUInteger)count;
- (NSUInteger)countForObject:(id)object;
- (void)addObject:(id)object;
- (void)removeObject:(id)object;
- (NSEnumerator *)objectEnumerator;
- (BOOL)containsObject:(id)object;
- (NSArray *)allObjects;

@end

#endif /* NSCountedSet_h */
