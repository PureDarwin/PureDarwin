/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSIndexSet.h>
#include <stdlib.h>
#include <string.h>

/* Stored as a sorted, non-overlapping, non-adjacent run of ranges. Adjacent
 * runs are always coalesced, so equality is a plain memcmp of the runs. */

#define NSNotFoundIndex ((NSUInteger)NSIntegerMax)

@implementation NSIndexSet

static void ensureCapacity(NSIndexSet *self, NSUInteger needed) {
    if (needed <= self->_rangeCapacity) {
        return;
    }
    NSUInteger capacity = (self->_rangeCapacity == 0) ? 4 : self->_rangeCapacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    self->_ranges = realloc(self->_ranges, capacity * sizeof(NSRange));
    self->_rangeCapacity = capacity;
}

static void insertRangeAt(NSIndexSet *self, NSUInteger at, NSRange range) {
    ensureCapacity(self, self->_rangeCount + 1);
    memmove(&self->_ranges[at + 1], &self->_ranges[at],
            (self->_rangeCount - at) * sizeof(NSRange));
    self->_ranges[at] = range;
    self->_rangeCount++;
}

static void removeRangeAt(NSIndexSet *self, NSUInteger at) {
    memmove(&self->_ranges[at], &self->_ranges[at + 1],
            (self->_rangeCount - at - 1) * sizeof(NSRange));
    self->_rangeCount--;
}

static void addRange(NSIndexSet *self, NSRange range) {
    if (range.length == 0) {
        return;
    }

    NSUInteger i = 0;
    while (i < self->_rangeCount &&
           NSMaxRange(self->_ranges[i]) < range.location) {
        i++;
    }

    /* Merge with every run this range touches or bridges. */
    NSUInteger start = range.location;
    NSUInteger end = NSMaxRange(range);
    while (i < self->_rangeCount && self->_ranges[i].location <= end) {
        NSUInteger existingEnd = NSMaxRange(self->_ranges[i]);
        if (self->_ranges[i].location < start) {
            start = self->_ranges[i].location;
        }
        if (existingEnd > end) {
            end = existingEnd;
        }
        removeRangeAt(self, i);
    }
    insertRangeAt(self, i, NSMakeRange(start, end - start));
}

static void removeRange(NSIndexSet *self, NSRange range) {
    if (range.length == 0) {
        return;
    }
    NSUInteger removeEnd = NSMaxRange(range);

    for (NSUInteger i = 0; i < self->_rangeCount; ) {
        NSRange existing = self->_ranges[i];
        NSUInteger existingEnd = NSMaxRange(existing);

        if (existingEnd <= range.location || existing.location >= removeEnd) {
            i++;
            continue;
        }

        BOOL keepHead = existing.location < range.location;
        BOOL keepTail = existingEnd > removeEnd;

        if (keepHead && keepTail) {
            self->_ranges[i] = NSMakeRange(existing.location,
                                           range.location - existing.location);
            insertRangeAt(self, i + 1,
                          NSMakeRange(removeEnd, existingEnd - removeEnd));
            return;
        } else if (keepHead) {
            self->_ranges[i] = NSMakeRange(existing.location,
                                           range.location - existing.location);
            i++;
        } else if (keepTail) {
            self->_ranges[i] = NSMakeRange(removeEnd, existingEnd - removeEnd);
            i++;
        } else {
            removeRangeAt(self, i);
        }
    }
}

+ (instancetype)indexSet {
    return [[[self alloc] init] autorelease];
}

+ (instancetype)indexSetWithIndex:(NSUInteger)index {
    return [[[self alloc] initWithIndex:index] autorelease];
}

+ (instancetype)indexSetWithIndexesInRange:(NSRange)range {
    return [[[self alloc] initWithIndexesInRange:range] autorelease];
}

- (instancetype)initWithIndex:(NSUInteger)index {
    return [self initWithIndexesInRange:NSMakeRange(index, 1)];
}

- (instancetype)initWithIndexesInRange:(NSRange)range {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    addRange(self, range);
    return self;
}

- (instancetype)initWithIndexSet:(NSIndexSet *)other {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    if (other != nil && other->_rangeCount > 0) {
        ensureCapacity(self, other->_rangeCount);
        memcpy(_ranges, other->_ranges, other->_rangeCount * sizeof(NSRange));
        _rangeCount = other->_rangeCount;
    }
    return self;
}

- (void)dealloc {
    free(_ranges);
    [super dealloc];
}

- (NSUInteger)count {
    NSUInteger total = 0;

    for (NSUInteger i = 0; i < _rangeCount; i++) {
        total += _ranges[i].length;
    }
    return total;
}

- (NSUInteger)firstIndex {
    return (_rangeCount == 0) ? NSNotFoundIndex : _ranges[0].location;
}

- (NSUInteger)lastIndex {
    return (_rangeCount == 0) ? NSNotFoundIndex
                              : NSMaxRange(_ranges[_rangeCount - 1]) - 1;
}

- (BOOL)containsIndex:(NSUInteger)index {
    for (NSUInteger i = 0; i < _rangeCount; i++) {
        if (index < _ranges[i].location) {
            return NO;
        }
        if (index < NSMaxRange(_ranges[i])) {
            return YES;
        }
    }
    return NO;
}

- (BOOL)containsIndexesInRange:(NSRange)range {
    for (NSUInteger i = range.location; i < NSMaxRange(range); i++) {
        if (![self containsIndex:i]) {
            return NO;
        }
    }
    return YES;
}

- (BOOL)containsIndexes:(NSIndexSet *)other {
    if (other == nil) {
        return YES;
    }
    for (NSUInteger i = 0; i < other->_rangeCount; i++) {
        if (![self containsIndexesInRange:other->_ranges[i]]) {
            return NO;
        }
    }
    return YES;
}

- (BOOL)isEqualToIndexSet:(NSIndexSet *)other {
    if (other == nil || other->_rangeCount != _rangeCount) {
        return NO;
    }
    return memcmp(_ranges, other->_ranges, _rangeCount * sizeof(NSRange)) == 0;
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[NSIndexSet class]]) {
        return NO;
    }
    return [self isEqualToIndexSet:other];
}

- (NSUInteger)indexGreaterThanIndex:(NSUInteger)index {
    return [self indexGreaterThanOrEqualToIndex:index + 1];
}

- (NSUInteger)indexGreaterThanOrEqualToIndex:(NSUInteger)index {
    for (NSUInteger i = 0; i < _rangeCount; i++) {
        if (index < _ranges[i].location) {
            return _ranges[i].location;
        }
        if (index < NSMaxRange(_ranges[i])) {
            return index;
        }
    }
    return NSNotFoundIndex;
}

- (NSUInteger)indexLessThanIndex:(NSUInteger)index {
    if (index == 0) {
        return NSNotFoundIndex;
    }
    return [self indexLessThanOrEqualToIndex:index - 1];
}

- (NSUInteger)indexLessThanOrEqualToIndex:(NSUInteger)index {
    for (NSUInteger i = _rangeCount; i > 0; i--) {
        NSRange range = _ranges[i - 1];

        if (index >= NSMaxRange(range)) {
            return NSMaxRange(range) - 1;
        }
        if (index >= range.location) {
            return index;
        }
    }
    return NSNotFoundIndex;
}

- (NSUInteger)getIndexes:(NSUInteger *)buffer
                maxCount:(NSUInteger)maxCount
            inIndexRange:(NSRangePointer)range {
    NSUInteger written = 0;
    NSUInteger from = (range != NULL) ? range->location : 0;
    NSUInteger to = (range != NULL) ? NSMaxRange(*range) : NSNotFoundIndex;

    for (NSUInteger i = 0; i < _rangeCount && written < maxCount; i++) {
        for (NSUInteger v = _ranges[i].location;
             v < NSMaxRange(_ranges[i]) && written < maxCount; v++) {
            if (v < from) {
                continue;
            }
            if (v >= to) {
                break;
            }
            buffer[written++] = v;
        }
    }
    if (range != NULL) {
        NSUInteger next = (written > 0) ? buffer[written - 1] + 1 : to;
        range->location = next;
        range->length = (to > next) ? to - next : 0;
    }
    return written;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[NSIndexSet alloc] initWithIndexSet:self];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [[NSMutableIndexSet alloc] initWithIndexSet:self];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (id)initWithCoder:(NSCoder *)coder {
    return [self init];
}

@end

@implementation NSMutableIndexSet

- (void)addIndex:(NSUInteger)index {
    addRange(self, NSMakeRange(index, 1));
}

- (void)addIndexesInRange:(NSRange)range {
    addRange(self, range);
}

- (void)addIndexes:(NSIndexSet *)other {
    if (other == nil) {
        return;
    }
    NSUInteger index = [other firstIndex];
    while (index != NSNotFoundIndex) {
        [self addIndex:index];
        index = [other indexGreaterThanIndex:index];
    }
}

- (void)removeIndex:(NSUInteger)index {
    removeRange(self, NSMakeRange(index, 1));
}

- (void)removeIndexesInRange:(NSRange)range {
    removeRange(self, range);
}

- (void)removeIndexes:(NSIndexSet *)other {
    if (other == nil) {
        return;
    }
    NSUInteger index = [other firstIndex];
    while (index != NSNotFoundIndex) {
        [self removeIndex:index];
        index = [other indexGreaterThanIndex:index];
    }
}

- (void)removeAllIndexes {
    _rangeCount = 0;
}

@end
