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
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSRange.h>

@class NSString;

@interface NSArray<__covariant ObjectType> : NSObject <NSFastEnumeration>

- (NSArray<ObjectType> *)arrayByAddingObjectsFromArray:(NSArray<ObjectType> *)other;
- (NSArray<ObjectType> *)subarrayWithRange:(NSRange)range;
- (BOOL)isEqualToArray:(NSArray<ObjectType> *)other;
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (id)valueForKey:(NSString *)key;
- (instancetype)initWithObjects:(id)firstObject, ...;
- (instancetype)initWithArray:(NSArray<ObjectType> *)array;

+ (instancetype)array;
+ (instancetype)arrayWithObject:(ObjectType)object;
+ (instancetype)arrayWithObjects:(ObjectType)firstObject, ...;
+ (instancetype)arrayWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)count;
+ (instancetype)arrayWithArray:(NSArray<ObjectType> *)array;

- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
- (id)objectAtIndexedSubscript:(NSUInteger)index;
- (nullable ObjectType)firstObject;
- (nullable ObjectType)lastObject;
- (NSEnumerator *)objectEnumerator;
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained [])buffer
                                    count:(NSUInteger)length;
- (BOOL)containsObject:(id)object;
- (NSUInteger)indexOfObject:(id)object;
- (NSUInteger)indexOfObjectIdenticalTo:(id)object;
- (NSArray<ObjectType> *)arrayByAddingObject:(ObjectType)object;
- (NSString *)componentsJoinedByString:(NSString *)separator;
- (NSArray<ObjectType> *)sortedArrayUsingSelector:(SEL)selector;
- (void)makeObjectsPerformSelector:(SEL)selector;
- (void)makeObjectsPerformSelector:(SEL)selector withObject:(id)object;

@end

@interface NSMutableArray<ObjectType> : NSArray<ObjectType>

+ (instancetype)new;
+ (instancetype)arrayWithCapacity:(NSUInteger)capacity;

- (instancetype)initWithCapacity:(NSUInteger)capacity;
- (void)addObject:(ObjectType)object;
- (void)insertObject:(ObjectType)object atIndex:(NSUInteger)index;
- (void)addObjectsFromArray:(NSArray<ObjectType> *)array;
- (void)removeObjectAtIndex:(NSUInteger)index;
- (void)removeObjectIdenticalTo:(ObjectType)object;
- (void)removeObject:(ObjectType)object;
- (void)sortUsingFunction:(NSInteger (*)(id, id, void *))comparator context:(void *)context;
- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(ObjectType)object;
- (void)removeAllObjects;
- (void)sortUsingSelector:(SEL)selector;

@end

#endif /* NSArray_h */
