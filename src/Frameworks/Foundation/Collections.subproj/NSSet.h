/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSet_h
#define NSSet_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSArray.h>

@interface NSSet<__covariant ObjectType> : NSObject

+ (instancetype)set;
+ (instancetype)setWithObject:(ObjectType)object;
+ (instancetype)setWithArray:(NSArray<ObjectType> *)array;
+ (instancetype)setWithObjects:(const ObjectType _Nonnull [_Nullable])objects
                          count:(NSUInteger)count;

- (NSUInteger)count;
- (nullable ObjectType)member:(ObjectType)object;
- (BOOL)containsObject:(ObjectType)object;
- (NSArray<ObjectType> *)allObjects;

@end

@interface NSMutableSet<ObjectType> : NSSet<ObjectType>

+ (instancetype)setWithCapacity:(NSUInteger)capacity;

- (void)addObject:(ObjectType)object;
- (void)removeObject:(ObjectType)object;
- (void)removeAllObjects;
- (void)addObjectsFromArray:(NSArray<ObjectType> *)array;
- (void)unionSet:(NSSet<ObjectType> *)other;
- (void)minusSet:(NSSet<ObjectType> *)other;
- (void)intersectSet:(NSSet<ObjectType> *)other;

@end

#endif /* NSSet_h */
