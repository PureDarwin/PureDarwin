/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRangeEntries_h
#define NSRangeEntries_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

/* A run-indexed store: a sorted list of (range, value) entries covering a
 * string. The text system keeps attribute dictionaries in one of these. */

typedef struct NSRangeEntries NSRangeEntries;

typedef struct {
    NSRangeEntries *entries;
    NSUInteger index;
} NSRangeEnumerator;

FOUNDATION_EXPORT NSRangeEntries *NSCreateRangeToCopiedObjectEntries(NSUInteger capacity);
FOUNDATION_EXPORT NSRangeEntries *NSCreateRangeToOwnedPointerEntries(NSUInteger capacity);
FOUNDATION_EXPORT void NSFreeRangeEntries(NSRangeEntries *entries);
FOUNDATION_EXPORT void NSResetRangeEntries(NSRangeEntries *entries);
FOUNDATION_EXPORT NSUInteger NSCountRangeEntries(NSRangeEntries *entries);

FOUNDATION_EXPORT void NSRangeEntryInsert(NSRangeEntries *entries, NSRange range, void *value);
FOUNDATION_EXPORT void *NSRangeEntryAtIndex(NSRangeEntries *entries, NSUInteger index,
                                            NSRange *effectiveRange);
FOUNDATION_EXPORT void NSRangeEntryDelete(NSRangeEntries *entries, NSUInteger index);
FOUNDATION_EXPORT void NSRangeEntriesRemoveEntryAtIndex(NSRangeEntries *entries, NSUInteger index);
FOUNDATION_EXPORT void *NSRangeEntryAtRange(NSRangeEntries *entries, NSRange range);
FOUNDATION_EXPORT NSRangeEnumerator NSRangeEntryEnumerator(NSRangeEntries *entries);

/* Shifts every entry at or after range by delta and drops what the edit
 * covered, which is what an edit to the backing string requires. */
FOUNDATION_EXPORT void NSRangeEntriesExpandAndWipe(NSRangeEntries *entries,
                                                   NSRange range, NSInteger delta);
/* Splits whatever entries overlap range so range itself can be replaced. */
FOUNDATION_EXPORT void NSRangeEntriesDivideAndConquer(NSRangeEntries *entries, NSRange range);
FOUNDATION_EXPORT void NSRangeEntriesVerify(NSRangeEntries *entries, NSUInteger length);
FOUNDATION_EXPORT void NSRangeEntriesDump(NSRangeEntries *entries);

FOUNDATION_EXPORT NSRangeEnumerator NSExplodeRangeEntries(NSRangeEntries *entries);
FOUNDATION_EXPORT BOOL NSNextRangeEnumeratorEntry(NSRangeEnumerator *enumerator,
                                                  NSRange *range, void **value);

#endif /* NSRangeEntries_h */
