/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSMapTable_h
#define NSMapTable_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSDictionary;

/* The legacy C map table: keys and values are void *, and the callback structs
 * decide whether they are retained, compared as objects, or treated as plain
 * integers. NSMapTable is a real class as on Darwin, so `@class NSMapTable;` in
 * ported code stays valid. */
@interface NSMapTable : NSObject
@end

typedef struct {
    NSUInteger (*hash)(NSMapTable *table, const void *key);
    BOOL (*isEqual)(NSMapTable *table, const void *key1, const void *key2);
    void (*retain)(NSMapTable *table, const void *key);
    void (*release)(NSMapTable *table, void *key);
    NSString *(*describe)(NSMapTable *table, const void *key);
    const void *notAKeyMarker;
} NSMapTableKeyCallBacks;

typedef struct {
    void (*retain)(NSMapTable *table, const void *value);
    void (*release)(NSMapTable *table, void *value);
    NSString *(*describe)(NSMapTable *table, const void *value);
} NSMapTableValueCallBacks;

typedef struct {
    NSMapTable *table;
    NSUInteger index;
} NSMapEnumerator;

#define NSNotAnIntMapKey     ((const void *)0x80000000)
#define NSNotAPointerMapKey  ((const void *)0xffffffff)

FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSIntegerMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonOwnedPointerMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonOwnedPointerOrNullMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonRetainedObjectMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSObjectMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSOwnedPointerMapKeyCallBacks;

FOUNDATION_EXPORT const NSMapTableValueCallBacks NSIntegerMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSNonOwnedPointerMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSNonRetainedObjectMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSObjectMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSOwnedPointerMapValueCallBacks;

FOUNDATION_EXPORT NSMapTable *NSCreateMapTable(NSMapTableKeyCallBacks keyCallBacks,
                                               NSMapTableValueCallBacks valueCallBacks,
                                               NSUInteger capacity);
FOUNDATION_EXPORT NSMapTable *NSCreateMapTableWithZone(NSMapTableKeyCallBacks keyCallBacks,
                                                       NSMapTableValueCallBacks valueCallBacks,
                                                       NSUInteger capacity,
                                                       NSZone *zone);
FOUNDATION_EXPORT NSMapTable *NSCopyMapTableWithZone(NSMapTable *table, NSZone *zone);
FOUNDATION_EXPORT void NSFreeMapTable(NSMapTable *table);
FOUNDATION_EXPORT void NSResetMapTable(NSMapTable *table);
FOUNDATION_EXPORT BOOL NSCompareMapTables(NSMapTable *table1, NSMapTable *table2);
FOUNDATION_EXPORT NSUInteger NSCountMapTable(NSMapTable *table);

FOUNDATION_EXPORT BOOL NSMapMember(NSMapTable *table, const void *key,
                                   void **originalKey, void **value);
FOUNDATION_EXPORT void *NSMapGet(NSMapTable *table, const void *key);
FOUNDATION_EXPORT void NSMapInsert(NSMapTable *table, const void *key, const void *value);
FOUNDATION_EXPORT void *NSMapInsertIfAbsent(NSMapTable *table, const void *key, const void *value);
FOUNDATION_EXPORT void NSMapInsertKnownAbsent(NSMapTable *table, const void *key, const void *value);
FOUNDATION_EXPORT void NSMapRemove(NSMapTable *table, const void *key);

FOUNDATION_EXPORT NSMapEnumerator NSEnumerateMapTable(NSMapTable *table);
FOUNDATION_EXPORT BOOL NSNextMapEnumeratorPair(NSMapEnumerator *enumerator,
                                               void **key, void **value);
FOUNDATION_EXPORT void NSEndMapTableEnumeration(NSMapEnumerator *enumerator);

FOUNDATION_EXPORT NSArray *NSAllMapTableKeys(NSMapTable *table);
FOUNDATION_EXPORT NSArray *NSAllMapTableValues(NSMapTable *table);
FOUNDATION_EXPORT NSString *NSStringFromMapTable(NSMapTable *table);

#endif /* NSMapTable_h */
