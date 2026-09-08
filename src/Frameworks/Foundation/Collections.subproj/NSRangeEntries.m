/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSRangeEntries.h>
#import <Foundation/NSString.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    NSRange range;
    void *value;
} NSRangeEntry;

struct NSRangeEntries {
    NSRangeEntry *entries;
    NSUInteger count;
    NSUInteger capacity;
    BOOL ownsObjects;
};

static void retainValue(NSRangeEntries *self, void *value) {
    if (self->ownsObjects) {
        [(id)value retain];
    }
}

static void releaseValue(NSRangeEntries *self, void *value) {
    if (self->ownsObjects) {
        [(id)value release];
    } else {
        free(value);
    }
}

static NSRangeEntries *createEntries(NSUInteger capacity, BOOL ownsObjects) {
    NSRangeEntries *self = calloc(1, sizeof(NSRangeEntries));

    self->capacity = (capacity < 4) ? 4 : capacity;
    self->entries = calloc(self->capacity, sizeof(NSRangeEntry));
    self->ownsObjects = ownsObjects;
    return self;
}

NSRangeEntries *NSCreateRangeToCopiedObjectEntries(NSUInteger capacity) {
    return createEntries(capacity, YES);
}

NSRangeEntries *NSCreateRangeToOwnedPointerEntries(NSUInteger capacity) {
    return createEntries(capacity, NO);
}

void NSResetRangeEntries(NSRangeEntries *self) {
    for (NSUInteger i = 0; i < self->count; i++) {
        releaseValue(self, self->entries[i].value);
    }
    self->count = 0;
}

void NSFreeRangeEntries(NSRangeEntries *self) {
    NSResetRangeEntries(self);
    free(self->entries);
    free(self);
}

NSUInteger NSCountRangeEntries(NSRangeEntries *self) {
    return self->count;
}

static void makeRoom(NSRangeEntries *self, NSUInteger at) {
    if (self->count + 1 > self->capacity) {
        self->capacity *= 2;
        self->entries = realloc(self->entries, self->capacity * sizeof(NSRangeEntry));
    }
    memmove(&self->entries[at + 1], &self->entries[at],
            (self->count - at) * sizeof(NSRangeEntry));
    self->count++;
}

void NSRangeEntryDelete(NSRangeEntries *self, NSUInteger index) {
    for (NSUInteger i = 0; i < self->count; i++) {
        if (NSLocationInRange(index, self->entries[i].range)) {
            releaseValue(self, self->entries[i].value);
            memmove(&self->entries[i], &self->entries[i + 1],
                    (self->count - i - 1) * sizeof(NSRangeEntry));
            self->count--;
            return;
        }
    }
}

/* The entry covering range.location; range.length only matters to callers
 * that then check the effective range against it. */
void *NSRangeEntryAtRange(NSRangeEntries *self, NSRange range) {
    return NSRangeEntryAtIndex(self, range.location, NULL);
}

void NSRangeEntriesRemoveEntryAtIndex(NSRangeEntries *self, NSUInteger index) {
    NSRangeEntryDelete(self, index);
}

NSRangeEnumerator NSRangeEntryEnumerator(NSRangeEntries *self) {
    return NSExplodeRangeEntries(self);
}

void NSRangeEntryInsert(NSRangeEntries *self, NSRange range, void *value) {
    if (range.length == 0) {
        return;
    }
    NSRangeEntriesDivideAndConquer(self, range);

    NSUInteger at = 0;
    while (at < self->count && self->entries[at].range.location < range.location) {
        at++;
    }
    makeRoom(self, at);
    self->entries[at].range = range;
    self->entries[at].value = value;
    retainValue(self, value);
}

void *NSRangeEntryAtIndex(NSRangeEntries *self, NSUInteger index, NSRange *effectiveRange) {
    for (NSUInteger i = 0; i < self->count; i++) {
        if (NSLocationInRange(index, self->entries[i].range)) {
            if (effectiveRange != NULL) {
                *effectiveRange = self->entries[i].range;
            }
            return self->entries[i].value;
        }
    }
    if (effectiveRange != NULL) {
        *effectiveRange = NSMakeRange(NSNotFound, 0);
    }
    return NULL;
}

void NSRangeEntriesDivideAndConquer(NSRangeEntries *self, NSRange range) {
    NSUInteger end = NSMaxRange(range);

    for (NSUInteger i = 0; i < self->count; ) {
        NSRange existing = self->entries[i].range;
        NSUInteger existingEnd = NSMaxRange(existing);

        if (existingEnd <= range.location || existing.location >= end) {
            i++;
            continue;
        }

        BOOL keepHead = existing.location < range.location;
        BOOL keepTail = existingEnd > end;

        if (keepHead && keepTail) {
            void *value = self->entries[i].value;

            self->entries[i].range = NSMakeRange(existing.location,
                                                 range.location - existing.location);
            makeRoom(self, i + 1);
            self->entries[i + 1].range = NSMakeRange(end, existingEnd - end);
            self->entries[i + 1].value = value;
            retainValue(self, value);
            return;
        } else if (keepHead) {
            self->entries[i].range = NSMakeRange(existing.location,
                                                 range.location - existing.location);
            i++;
        } else if (keepTail) {
            self->entries[i].range = NSMakeRange(end, existingEnd - end);
            i++;
        } else {
            releaseValue(self, self->entries[i].value);
            memmove(&self->entries[i], &self->entries[i + 1],
                    (self->count - i - 1) * sizeof(NSRangeEntry));
            self->count--;
        }
    }
}

void NSRangeEntriesExpandAndWipe(NSRangeEntries *self, NSRange range, NSInteger delta) {
    NSRangeEntriesDivideAndConquer(self, range);

    NSUInteger from = NSMaxRange(range);
    for (NSUInteger i = 0; i < self->count; i++) {
        if (self->entries[i].range.location >= from) {
            self->entries[i].range.location =
                (NSUInteger)((NSInteger)self->entries[i].range.location + delta);
        }
    }
}

/* Only meaningful on a DEBUG build; the callers leave it in as a tripwire. */
void NSRangeEntriesVerify(NSRangeEntries *self, NSUInteger length) {
}

void NSRangeEntriesDump(NSRangeEntries *self) {
    for (NSUInteger i = 0; i < self->count; i++) {
        NSLog(@"  {%lu, %lu} -> %p",
              (unsigned long)self->entries[i].range.location,
              (unsigned long)self->entries[i].range.length,
              self->entries[i].value);
    }
}

NSRangeEnumerator NSExplodeRangeEntries(NSRangeEntries *self) {
    NSRangeEnumerator enumerator = { self, 0 };

    return enumerator;
}

BOOL NSNextRangeEnumeratorEntry(NSRangeEnumerator *enumerator, NSRange *range, void **value) {
    NSRangeEntries *self = enumerator->entries;

    if (enumerator->index >= self->count) {
        return NO;
    }
    if (range != NULL) {
        *range = self->entries[enumerator->index].range;
    }
    if (value != NULL) {
        *value = self->entries[enumerator->index].value;
    }
    enumerator->index++;
    return YES;
}
