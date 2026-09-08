/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSHashTable.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#include <stdlib.h>
#include <string.h>

/* Open-addressed table of pointers. Element 0 marks an empty slot, which is
 * why NSHashGet on a NULL element is meaningless - matching the callers, which
 * always store real objects. */
@interface NSHashTable () {
@public
    NSHashTableCallBacks _callBacks;
    const void **_elements;
    NSUInteger _capacity;
    NSUInteger _count;
}
@end

@implementation NSHashTable

- (void)dealloc {
    free(_elements);
    [super dealloc];
}

@end

static NSUInteger pointerHash(NSHashTable *table, const void *element) {
    return (NSUInteger)((uintptr_t)element >> 4);
}

static BOOL pointerIsEqual(NSHashTable *table, const void *a, const void *b) {
    return a == b;
}

static void nullRetain(NSHashTable *table, const void *element) {
}

static void nullRelease(NSHashTable *table, void *element) {
}

static NSString *nullDescribe(NSHashTable *table, const void *element) {
    return nil;
}

static NSUInteger objectHash(NSHashTable *table, const void *element) {
    return (NSUInteger)[(id)element hash];
}

static BOOL objectIsEqual(NSHashTable *table, const void *a, const void *b) {
    return [(id)a isEqual:(id)b] ? YES : NO;
}

static void objectRetain(NSHashTable *table, const void *element) {
    [(id)element retain];
}

static void objectRelease(NSHashTable *table, void *element) {
    [(id)element release];
}

static void ownedRelease(NSHashTable *table, void *element) {
    free(element);
}

const NSHashTableCallBacks NSIntegerHashCallBacks = {
    pointerHash, pointerIsEqual, nullRetain, nullRelease, nullDescribe, NULL
};
const NSHashTableCallBacks NSNonOwnedPointerHashCallBacks = {
    pointerHash, pointerIsEqual, nullRetain, nullRelease, nullDescribe, NULL
};
const NSHashTableCallBacks NSNonRetainedObjectHashCallBacks = {
    objectHash, objectIsEqual, nullRetain, nullRelease, nullDescribe, NULL
};
const NSHashTableCallBacks NSObjectHashCallBacks = {
    objectHash, objectIsEqual, objectRetain, objectRelease, nullDescribe, NULL
};
const NSHashTableCallBacks NSOwnedPointerHashCallBacks = {
    pointerHash, pointerIsEqual, nullRetain, ownedRelease, nullDescribe, NULL
};

static void resizeIfNeeded(NSHashTable *table);

NSHashTable *NSCreateHashTableWithZone(NSHashTableCallBacks callBacks,
                                       NSUInteger capacity, NSZone *zone) {
    NSHashTable *table = [[NSHashTable alloc] init];

    table->_callBacks = callBacks;
    table->_capacity = (capacity < 8) ? 8 : capacity * 2;
    table->_elements = calloc(table->_capacity, sizeof(const void *));
    table->_count = 0;
    return table;
}

NSHashTable *NSCreateHashTable(NSHashTableCallBacks callBacks, NSUInteger capacity) {
    return NSCreateHashTableWithZone(callBacks, capacity, NULL);
}

void NSFreeHashTable(NSHashTable *table) {
    NSResetHashTable(table);
    [table release];
}

void NSResetHashTable(NSHashTable *table) {
    for (NSUInteger i = 0; i < table->_capacity; i++) {
        if (table->_elements[i] != NULL) {
            table->_callBacks.release(table, (void *)table->_elements[i]);
            table->_elements[i] = NULL;
        }
    }
    table->_count = 0;
}

NSUInteger NSCountHashTable(NSHashTable *table) {
    return table->_count;
}

static NSUInteger slotForElement(NSHashTable *table, const void *element) {
    NSUInteger index = table->_callBacks.hash(table, element) % table->_capacity;

    while (table->_elements[index] != NULL) {
        if (table->_callBacks.isEqual(table, table->_elements[index], element)) {
            break;
        }
        index = (index + 1) % table->_capacity;
    }
    return index;
}

void *NSHashGet(NSHashTable *table, const void *element) {
    if (element == NULL) {
        return NULL;
    }
    return (void *)table->_elements[slotForElement(table, element)];
}

void NSHashInsert(NSHashTable *table, const void *element) {
    if (element == NULL) {
        return;
    }

    NSUInteger index = slotForElement(table, element);
    if (table->_elements[index] != NULL) {
        /* Replacing an equal element: release the old one first. */
        table->_callBacks.release(table, (void *)table->_elements[index]);
        table->_elements[index] = element;
        table->_callBacks.retain(table, element);
        return;
    }

    table->_callBacks.retain(table, element);
    table->_elements[index] = element;
    table->_count++;
    resizeIfNeeded(table);
}

void NSHashInsertIfAbsent(NSHashTable *table, const void *element) {
    if (NSHashGet(table, element) == NULL) {
        NSHashInsert(table, element);
    }
}

void NSHashInsertKnownAbsent(NSHashTable *table, const void *element) {
    NSHashInsert(table, element);
}

void NSHashRemove(NSHashTable *table, const void *element) {
    if (element == NULL) {
        return;
    }

    NSUInteger index = slotForElement(table, element);
    if (table->_elements[index] == NULL) {
        return;
    }
    table->_callBacks.release(table, (void *)table->_elements[index]);
    table->_elements[index] = NULL;
    table->_count--;

    /* Reinsert the rest of the probe run so lookups do not stop at the hole. */
    NSUInteger next = (index + 1) % table->_capacity;
    while (table->_elements[next] != NULL) {
        const void *moved = table->_elements[next];

        table->_elements[next] = NULL;
        table->_count--;
        NSHashInsert(table, moved);
        table->_callBacks.release(table, (void *)moved);
        next = (next + 1) % table->_capacity;
    }
}

static void resizeIfNeeded(NSHashTable *table) {
    if (table->_count * 4 < table->_capacity * 3) {
        return;
    }

    NSUInteger oldCapacity = table->_capacity;
    const void **old = table->_elements;

    table->_capacity = oldCapacity * 2;
    table->_elements = calloc(table->_capacity, sizeof(const void *));
    table->_count = 0;

    for (NSUInteger i = 0; i < oldCapacity; i++) {
        if (old[i] != NULL) {
            NSUInteger index = slotForElement(table, old[i]);
            table->_elements[index] = old[i];
            table->_count++;
        }
    }
    free(old);
}

NSHashEnumerator NSEnumerateHashTable(NSHashTable *table) {
    NSHashEnumerator enumerator = { table, 0 };

    return enumerator;
}

void *NSNextHashEnumeratorItem(NSHashEnumerator *enumerator) {
    NSHashTable *table = enumerator->table;

    while (enumerator->index < table->_capacity) {
        const void *element = table->_elements[enumerator->index++];

        if (element != NULL) {
            return (void *)element;
        }
    }
    return NULL;
}

NSArray *NSAllHashTableObjects(NSHashTable *table) {
    NSMutableArray *result = [NSMutableArray array];
    NSHashEnumerator state = NSEnumerateHashTable(table);
    void *element;

    while ((element = NSNextHashEnumeratorItem(&state)) != NULL) {
        [result addObject:(id)element];
    }
    return result;
}
