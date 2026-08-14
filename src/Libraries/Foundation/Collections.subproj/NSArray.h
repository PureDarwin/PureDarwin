/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSArray_h
#define NSArray_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@interface NSArray<__covariant ObjectType> : NSObject

+ (instancetype)array;
+ (instancetype)arrayWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)count;
+ (instancetype)arrayWithArray:(NSArray<ObjectType> *)array;

- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
- (id)objectAtIndexedSubscript:(NSUInteger)index;
- (BOOL)containsObject:(id)object;

@end

@interface NSMutableArray<ObjectType> : NSArray<ObjectType>

+ (instancetype)arrayWithCapacity:(NSUInteger)capacity;

- (void)addObject:(ObjectType)object;
- (void)addObjectsFromArray:(NSArray<ObjectType> *)array;
- (void)removeObjectAtIndex:(NSUInteger)index;
- (void)removeAllObjects;

@end

#endif /* NSArray_h */
