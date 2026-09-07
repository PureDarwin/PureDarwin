/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSMapTable.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSZone.h>
#include <CoreFoundation/CFString.h>
#include <stdlib.h>
#include <string.h>

/* Open-addressed table with linear probing. Buckets carry an explicit state
 * rather than a not-a-key marker, so any pointer value is a legal key. */

enum {
    kBucketEmpty = 0,
    kBucketOccupied = 1,
    kBucketDeleted = 2,
};

typedef struct {
    const void *key;
    const void *value;
    unsigned char state;
} NSMapBucket;

@implementation NSMapTable {
@public
    NSMapTableKeyCallBacks keyCallBacks;
    NSMapTableValueCallBacks valueCallBacks;
    NSMapBucket *buckets;
    NSUInteger bucketCount;
    NSUInteger count;
}

- (void)dealloc {
    NSResetMapTable(self);
    free(buckets);
}

@end

static NSUInteger _hashKey(NSMapTable *table, const void *key) {
    if (table->keyCallBacks.hash != NULL) {
        return table->keyCallBacks.hash(table, key);
    }
    return (NSUInteger)(uintptr_t)key;
}

static BOOL _equalKeys(NSMapTable *table, const void *a, const void *b) {
    if (table->keyCallBacks.isEqual != NULL) {
        return table->keyCallBacks.isEqual(table, a, b);
    }
    return a == b;
}

static void _retainKey(NSMapTable *table, const void *key) {
    if (table->keyCallBacks.retain != NULL) {
        table->keyCallBacks.retain(table, key);
    }
}

static void _releaseKey(NSMapTable *table, const void *key) {
    if (table->keyCallBacks.release != NULL) {
        table->keyCallBacks.release(table, (void *)key);
    }
}

static void _retainValue(NSMapTable *table, const void *value) {
    if (table->valueCallBacks.retain != NULL) {
        table->valueCallBacks.retain(table, value);
    }
}

static void _releaseValue(NSMapTable *table, const void *value) {
    if (table->valueCallBacks.release != NULL) {
        table->valueCallBacks.release(table, (void *)value);
    }
}

/* Returns the bucket holding key, or the first bucket it may be inserted into. */
static NSMapBucket *_findBucket(NSMapTable *table, const void *key) {
    NSUInteger mask = table->bucketCount - 1;
    NSUInteger index = _hashKey(table, key) & mask;
    NSMapBucket *firstDeleted = NULL;

    for (NSUInteger probe = 0; probe < table->bucketCount; probe++) {
        NSMapBucket *bucket = &table->buckets[index];

        if (bucket->state == kBucketEmpty) {
            return firstDeleted != NULL ? firstDeleted : bucket;
        }
        if (bucket->state == kBucketDeleted) {
            if (firstDeleted == NULL) {
                firstDeleted = bucket;
            }
        } else if (_equalKeys(table, bucket->key, key)) {
            return bucket;
        }

        index = (index + 1) & mask;
    }

    return firstDeleted;
}

static void _growIfNeeded(NSMapTable *table) {
    if (table->count * 4 < table->bucketCount * 3) {
        return;
    }

    NSMapBucket *oldBuckets = table->buckets;
    NSUInteger oldCount = table->bucketCount;

    table->bucketCount = oldCount * 2;
    table->buckets = calloc(table->bucketCount, sizeof(NSMapBucket));

    for (NSUInteger i = 0; i < oldCount; i++) {
        if (oldBuckets[i].state != kBucketOccupied) {
            continue;
        }
        NSMapBucket *bucket = _findBucket(table, oldBuckets[i].key);
        *bucket = oldBuckets[i];
    }

    free(oldBuckets);
}

NSMapTable *NSCreateMapTable(NSMapTableKeyCallBacks keyCallBacks,
                             NSMapTableValueCallBacks valueCallBacks,
                             NSUInteger capacity) {
    return NSCreateMapTableWithZone(keyCallBacks, valueCallBacks, capacity, NULL);
}

NSMapTable *NSCreateMapTableWithZone(NSMapTableKeyCallBacks keyCallBacks,
                                     NSMapTableValueCallBacks valueCallBacks,
                                     NSUInteger capacity, NSZone *zone) {
    NSMapTable *table = [[NSMapTable alloc] init];

    table->keyCallBacks = keyCallBacks;
    table->valueCallBacks = valueCallBacks;
    table->count = 0;

    table->bucketCount = 8;
    while (table->bucketCount < capacity * 2) {
        table->bucketCount *= 2;
    }
    table->buckets = calloc(table->bucketCount, sizeof(NSMapBucket));

    return table;
}

NSMapTable *NSCopyMapTableWithZone(NSMapTable *table, NSZone *zone) {
    NSMapTable *copy = NSCreateMapTableWithZone(table->keyCallBacks,
                                                table->valueCallBacks,
                                                table->count, zone);

    for (NSUInteger i = 0; i < table->bucketCount; i++) {
        if (table->buckets[i].state == kBucketOccupied) {
            NSMapInsert(copy, table->buckets[i].key, table->buckets[i].value);
        }
    }

    return copy;
}

void NSFreeMapTable(NSMapTable *table) {
    if (table == NULL) {
        return;
    }
    /* -dealloc releases the buckets and their contents. */
    [table release];
}

void NSResetMapTable(NSMapTable *table) {
    for (NSUInteger i = 0; i < table->bucketCount; i++) {
        if (table->buckets[i].state == kBucketOccupied) {
            _releaseKey(table, table->buckets[i].key);
            _releaseValue(table, table->buckets[i].value);
        }
        table->buckets[i].state = kBucketEmpty;
        table->buckets[i].key = NULL;
        table->buckets[i].value = NULL;
    }
    table->count = 0;
}

BOOL NSCompareMapTables(NSMapTable *table1, NSMapTable *table2) {
    if (table1->count != table2->count) {
        return NO;
    }

    for (NSUInteger i = 0; i < table1->bucketCount; i++) {
        if (table1->buckets[i].state != kBucketOccupied) {
            continue;
        }
        void *other = NULL;
        if (!NSMapMember(table2, table1->buckets[i].key, NULL, &other)) {
            return NO;
        }
        if (other != table1->buckets[i].value) {
            return NO;
        }
    }

    return YES;
}

NSUInteger NSCountMapTable(NSMapTable *table) {
    return table->count;
}

BOOL NSMapMember(NSMapTable *table, const void *key, void **originalKey, void **value) {
    NSMapBucket *bucket = _findBucket(table, key);

    if (bucket == NULL || bucket->state != kBucketOccupied) {
        return NO;
    }
    if (originalKey != NULL) {
        *originalKey = (void *)bucket->key;
    }
    if (value != NULL) {
        *value = (void *)bucket->value;
    }
    return YES;
}

void *NSMapGet(NSMapTable *table, const void *key) {
    NSMapBucket *bucket = _findBucket(table, key);

    if (bucket == NULL || bucket->state != kBucketOccupied) {
        return NULL;
    }
    return (void *)bucket->value;
}

void NSMapInsert(NSMapTable *table, const void *key, const void *value) {
    NSMapBucket *bucket = _findBucket(table, key);

    if (bucket != NULL && bucket->state == kBucketOccupied) {
        _retainValue(table, value);
        _releaseValue(table, bucket->value);
        bucket->value = value;
        return;
    }

    _retainKey(table, key);
    _retainValue(table, value);

    bucket->key = key;
    bucket->value = value;
    bucket->state = kBucketOccupied;
    table->count++;

    _growIfNeeded(table);
}

void *NSMapInsertIfAbsent(NSMapTable *table, const void *key, const void *value) {
    NSMapBucket *bucket = _findBucket(table, key);

    if (bucket != NULL && bucket->state == kBucketOccupied) {
        return (void *)bucket->key;
    }

    NSMapInsert(table, key, value);
    return NULL;
}

void NSMapInsertKnownAbsent(NSMapTable *table, const void *key, const void *value) {
    NSMapInsert(table, key, value);
}

void NSMapRemove(NSMapTable *table, const void *key) {
    NSMapBucket *bucket = _findBucket(table, key);

    if (bucket == NULL || bucket->state != kBucketOccupied) {
        return;
    }

    _releaseKey(table, bucket->key);
    _releaseValue(table, bucket->value);

    bucket->key = NULL;
    bucket->value = NULL;
    bucket->state = kBucketDeleted;
    table->count--;
}

NSMapEnumerator NSEnumerateMapTable(NSMapTable *table) {
    NSMapEnumerator enumerator;
    enumerator.table = table;
    enumerator.index = 0;
    return enumerator;
}

BOOL NSNextMapEnumeratorPair(NSMapEnumerator *enumerator, void **key, void **value) {
    NSMapTable *table = enumerator->table;

    while (enumerator->index < table->bucketCount) {
        NSMapBucket *bucket = &table->buckets[enumerator->index++];

        if (bucket->state != kBucketOccupied) {
            continue;
        }
        if (key != NULL) {
            *key = (void *)bucket->key;
        }
        if (value != NULL) {
            *value = (void *)bucket->value;
        }
        return YES;
    }

    return NO;
}

void NSEndMapTableEnumeration(NSMapEnumerator *enumerator) {
    enumerator->index = enumerator->table->bucketCount;
}

NSArray *NSAllMapTableKeys(NSMapTable *table) {
    NSMutableArray *keys = [NSMutableArray arrayWithCapacity:table->count];

    for (NSUInteger i = 0; i < table->bucketCount; i++) {
        if (table->buckets[i].state == kBucketOccupied) {
            [keys addObject:(__bridge id)(void *)table->buckets[i].key];
        }
    }

    return keys;
}

NSArray *NSAllMapTableValues(NSMapTable *table) {
    NSMutableArray *values = [NSMutableArray arrayWithCapacity:table->count];

    for (NSUInteger i = 0; i < table->bucketCount; i++) {
        if (table->buckets[i].state == kBucketOccupied) {
            [values addObject:(__bridge id)(void *)table->buckets[i].value];
        }
    }

    return values;
}

NSString *NSStringFromMapTable(NSMapTable *table) {
    CFMutableStringRef out = CFStringCreateMutable(kCFAllocatorDefault, 0);

    for (NSUInteger i = 0; i < table->bucketCount; i++) {
        if (table->buckets[i].state != kBucketOccupied) {
            continue;
        }
        CFStringAppendFormat(out, NULL, CFSTR("%p = %p\n"),
                             table->buckets[i].key, table->buckets[i].value);
    }

    return (NSString *)out;
}

/* Callback sets. Object variants go through the objc runtime directly rather
 * than through NSObject messaging so a NULL key stays legal. */

static NSUInteger _objectHash(NSMapTable *table, const void *key) {
    return key == NULL ? 0 : (NSUInteger)[(__bridge id)key hash];
}

static BOOL _objectIsEqual(NSMapTable *table, const void *key1, const void *key2) {
    if (key1 == key2) {
        return YES;
    }
    if (key1 == NULL || key2 == NULL) {
        return NO;
    }
    return [(__bridge id)key1 isEqual:(__bridge id)key2] ? YES : NO;
}

static void _objectRetain(NSMapTable *table, const void *item) {
    if (item != NULL) {
        CFRetain(item);
    }
}

static void _objectRelease(NSMapTable *table, void *item) {
    if (item != NULL) {
        CFRelease(item);
    }
}

static NSString *_objectDescribe(NSMapTable *table, const void *item) {
    return item == NULL ? nil : (NSString *)CFCopyDescription(item);
}

static NSUInteger _pointerHash(NSMapTable *table, const void *key) {
    return (NSUInteger)(uintptr_t)key;
}

static BOOL _pointerIsEqual(NSMapTable *table, const void *key1, const void *key2) {
    return key1 == key2;
}

static NSString *_pointerDescribe(NSMapTable *table, const void *item) {
    return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                CFSTR("%p"), item);
}

static void _ownedPointerRelease(NSMapTable *table, void *item) {
    free(item);
}

const NSMapTableKeyCallBacks NSIntegerMapKeyCallBacks = {
    _pointerHash, _pointerIsEqual, NULL, NULL, _pointerDescribe, NSNotAnIntMapKey
};

const NSMapTableKeyCallBacks NSNonOwnedPointerMapKeyCallBacks = {
    _pointerHash, _pointerIsEqual, NULL, NULL, _pointerDescribe, NSNotAPointerMapKey
};

const NSMapTableKeyCallBacks NSNonOwnedPointerOrNullMapKeyCallBacks = {
    _pointerHash, _pointerIsEqual, NULL, NULL, _pointerDescribe, NSNotAPointerMapKey
};

const NSMapTableKeyCallBacks NSNonRetainedObjectMapKeyCallBacks = {
    _objectHash, _objectIsEqual, NULL, NULL, _objectDescribe, NSNotAPointerMapKey
};

const NSMapTableKeyCallBacks NSObjectMapKeyCallBacks = {
    _objectHash, _objectIsEqual, _objectRetain, _objectRelease, _objectDescribe,
    NSNotAPointerMapKey
};

const NSMapTableKeyCallBacks NSOwnedPointerMapKeyCallBacks = {
    _pointerHash, _pointerIsEqual, NULL, _ownedPointerRelease, _pointerDescribe,
    NSNotAPointerMapKey
};

const NSMapTableValueCallBacks NSIntegerMapValueCallBacks = {
    NULL, NULL, _pointerDescribe
};

const NSMapTableValueCallBacks NSNonOwnedPointerMapValueCallBacks = {
    NULL, NULL, _pointerDescribe
};

const NSMapTableValueCallBacks NSNonRetainedObjectMapValueCallBacks = {
    NULL, NULL, _objectDescribe
};

const NSMapTableValueCallBacks NSObjectMapValueCallBacks = {
    _objectRetain, _objectRelease, _objectDescribe
};

const NSMapTableValueCallBacks NSOwnedPointerMapValueCallBacks = {
    NULL, _ownedPointerRelease, _pointerDescribe
};
