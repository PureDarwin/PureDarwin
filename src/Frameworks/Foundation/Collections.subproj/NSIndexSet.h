/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSIndexSet_h
#define NSIndexSet_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

@interface NSIndexSet : NSObject <NSCopying, NSMutableCopying, NSCoding> {
    NSRange *_ranges;
    NSUInteger _rangeCount;
    NSUInteger _rangeCapacity;
}

+ (instancetype)indexSet;
+ (instancetype)indexSetWithIndex:(NSUInteger)index;
+ (instancetype)indexSetWithIndexesInRange:(NSRange)range;

- (instancetype)initWithIndex:(NSUInteger)index;
- (instancetype)initWithIndexesInRange:(NSRange)range;
- (instancetype)initWithIndexSet:(NSIndexSet *)other;

- (NSUInteger)count;
- (NSUInteger)firstIndex;
- (NSUInteger)lastIndex;
- (BOOL)containsIndex:(NSUInteger)index;
- (BOOL)containsIndexes:(NSIndexSet *)other;
- (BOOL)containsIndexesInRange:(NSRange)range;
- (BOOL)isEqualToIndexSet:(NSIndexSet *)other;
- (NSUInteger)indexGreaterThanIndex:(NSUInteger)index;
- (NSUInteger)indexLessThanIndex:(NSUInteger)index;
- (NSUInteger)indexGreaterThanOrEqualToIndex:(NSUInteger)index;
- (NSUInteger)indexLessThanOrEqualToIndex:(NSUInteger)index;
- (NSUInteger)getIndexes:(NSUInteger *)buffer
                maxCount:(NSUInteger)maxCount
            inIndexRange:(NSRangePointer)range;

@end

@interface NSMutableIndexSet : NSIndexSet

- (void)addIndex:(NSUInteger)index;
- (void)addIndexes:(NSIndexSet *)other;
- (void)addIndexesInRange:(NSRange)range;
- (void)removeIndex:(NSUInteger)index;
- (void)removeIndexes:(NSIndexSet *)other;
- (void)removeIndexesInRange:(NSRange)range;
- (void)removeAllIndexes;

@end

#endif /* NSIndexSet_h */
