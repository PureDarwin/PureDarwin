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

@class NSURL;

@class NSIndexSet;

@interface NSArray<__covariant ObjectType> : NSObject <NSFastEnumeration>

- (NSArray<ObjectType> *)arrayByAddingObjectsFromArray:(NSArray<ObjectType> *)other;
- (NSArray<ObjectType> *)subarrayWithRange:(NSRange)range;
- (BOOL)isEqualToArray:(NSArray<ObjectType> *)other;
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (void)setArray:(NSArray<ObjectType> *)array;
- (void)removeObjectsInArray:(NSArray<ObjectType> *)array;
- (void)exchangeObjectAtIndex:(NSUInteger)index1 withObjectAtIndex:(NSUInteger)index2;
- (id)valueForKey:(NSString *)key;
- (instancetype)initWithObjects:(id)firstObject, ...;
- (instancetype)initWithArray:(NSArray<ObjectType> *)array;

+ (instancetype)array;
+ (instancetype)arrayWithObject:(ObjectType)object;
+ (instancetype)arrayWithObjects:(ObjectType)firstObject, ...;
+ (instancetype)arrayWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)count;
+ (instancetype)arrayWithArray:(NSArray<ObjectType> *)array;

/* Reads what -writeToFile:atomically: produces. Sent to NSMutableArray the
 * result is mutable. */
+ (nullable instancetype)arrayWithContentsOfFile:(NSString *_Nonnull)path;
+ (nullable instancetype)arrayWithContentsOfURL:(NSURL *_Nonnull)url;

- (NSUInteger)count;
- (ObjectType)objectAtIndex:(NSUInteger)index;
- (ObjectType)objectAtIndexedSubscript:(NSUInteger)index;
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
- (NSArray<ObjectType> *)sortedArrayUsingComparator:(NSComparator)comparator;
- (void)enumerateObjectsUsingBlock:(void (^)(ObjectType object, NSUInteger index, BOOL *stop))block;
- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)options
                         usingBlock:(void (^)(ObjectType object, NSUInteger index, BOOL *stop))block;
- (void)getObjects:(ObjectType __unsafe_unretained *)objects;
- (NSArray<ObjectType> *)objectsAtIndexes:(NSIndexSet *)indexes;
- (NSEnumerator *)reverseObjectEnumerator;
- (nullable ObjectType)firstObjectCommonWithArray:(NSArray<ObjectType> *)other;
- (NSUInteger)indexOfObjectPassingTest:(BOOL (^)(ObjectType object, NSUInteger index, BOOL *stop))predicate;
- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)options
                           passingTest:(BOOL (^)(ObjectType object, NSUInteger index, BOOL *stop))predicate;
- (NSArray<ObjectType> *)sortedArrayUsingDescriptors:(NSArray *)descriptors;
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
- (void)removeLastObject;
- (void)removeObjectIdenticalTo:(ObjectType)object;
- (void)removeObject:(ObjectType)object;
- (void)sortUsingFunction:(NSInteger (*)(id, id, void *))comparator context:(void *)context;
- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(ObjectType)object;
- (void)setObject:(ObjectType)object atIndexedSubscript:(NSUInteger)index;
- (void)removeAllObjects;
- (void)sortUsingSelector:(SEL)selector;
- (void)sortUsingComparator:(NSComparator)comparator;
- (void)sortUsingDescriptors:(NSArray *)descriptors;
- (void)sortWithOptions:(NSSortOptions)options usingComparator:(NSComparator)comparator;

@end

#endif /* NSArray_h */
