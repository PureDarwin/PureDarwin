/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSHashTable_h
#define NSHashTable_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSZone.h>

@class NSString, NSArray;

/* The set half of NSMapTable's C API, same shape and callback style. */
@interface NSHashTable : NSObject
@end

typedef struct {
    NSUInteger (*hash)(NSHashTable *table, const void *element);
    BOOL (*isEqual)(NSHashTable *table, const void *element1, const void *element2);
    void (*retain)(NSHashTable *table, const void *element);
    void (*release)(NSHashTable *table, void *element);
    NSString *(*describe)(NSHashTable *table, const void *element);
    const void *notAnElement;
} NSHashTableCallBacks;

typedef struct {
    NSHashTable *table;
    NSUInteger index;
} NSHashEnumerator;

FOUNDATION_EXPORT const NSHashTableCallBacks NSIntegerHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSNonOwnedPointerHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSNonRetainedObjectHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSObjectHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSOwnedPointerHashCallBacks;

FOUNDATION_EXPORT NSHashTable *NSCreateHashTable(NSHashTableCallBacks callBacks,
                                                 NSUInteger capacity);
FOUNDATION_EXPORT NSHashTable *NSCreateHashTableWithZone(NSHashTableCallBacks callBacks,
                                                         NSUInteger capacity,
                                                         NSZone *zone);
FOUNDATION_EXPORT void NSFreeHashTable(NSHashTable *table);
FOUNDATION_EXPORT void NSResetHashTable(NSHashTable *table);
FOUNDATION_EXPORT NSUInteger NSCountHashTable(NSHashTable *table);

FOUNDATION_EXPORT void *NSHashGet(NSHashTable *table, const void *element);
FOUNDATION_EXPORT void NSHashInsert(NSHashTable *table, const void *element);
FOUNDATION_EXPORT void NSHashInsertIfAbsent(NSHashTable *table, const void *element);
FOUNDATION_EXPORT void NSHashInsertKnownAbsent(NSHashTable *table, const void *element);
FOUNDATION_EXPORT void NSHashRemove(NSHashTable *table, const void *element);

FOUNDATION_EXPORT NSHashEnumerator NSEnumerateHashTable(NSHashTable *table);
FOUNDATION_EXPORT void *NSNextHashEnumeratorItem(NSHashEnumerator *enumerator);
FOUNDATION_EXPORT NSArray *NSAllHashTableObjects(NSHashTable *table);

#endif /* NSHashTable_h */
