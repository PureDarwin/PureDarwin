/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


/*!
 @header SOSChangeTracker.h - Manifest caching and change propagation.
 */

#ifndef _SEC_SOSCHANGETRACKER_H_
#define _SEC_SOSCHANGETRACKER_H_

#include "keychain/SecureObjectSync/SOSDataSource.h"

#include "utilities/simulatecrash_assert.h"

__BEGIN_DECLS

enum {
    kSOSErrorNotConcreteError = 1042,
};


//
// Interface to encoding and decoding changes
//

typedef CFTypeRef SOSChangeRef;

static inline void SOSChangesAppendDelete(CFMutableArrayRef changes, CFTypeRef object) {
    const void *values[] = { object };
    SOSChangeRef change = CFArrayCreate(kCFAllocatorDefault, values, array_size(values), &kCFTypeArrayCallBacks);
    CFArrayAppendValue(changes, change);
    CFReleaseSafe(change);
}

static inline void SOSChangesAppendAdd(CFMutableArrayRef changes, CFTypeRef object) {
    CFArrayAppendValue(changes, object);
}

// Return the object and return true if it's an add and false if it's a delete.
static inline bool SOSChangeGetObject(SOSChangeRef change, CFTypeRef *object) {
    if (CFGetTypeID(change) == CFArrayGetTypeID()) {
        assert(CFArrayGetCount(change) == 1);
        *object = CFArrayGetValueAtIndex(change, 0);
        return false;
    } else {
        *object = change;
        return true;
    }
}

CFDataRef SOSChangeCopyDigest(SOSDataSourceRef dataSource, SOSChangeRef change, bool *isDel, SOSObjectRef *object, CFErrorRef *error);

CFStringRef SOSChangeCopyDescription(SOSChangeRef change);

CFStringRef SOSChangesCopyDescription(CFArrayRef changes);

//
// ChangeTracker
//

typedef struct __OpaqueSOSChangeTracker *SOSChangeTrackerRef;

SOSChangeTrackerRef SOSChangeTrackerCreate(CFAllocatorRef allocator, bool isConcrete, CFArrayRef children, CFErrorRef *error);

// Change the concreteness of the current ct (a non concrete ct does not support SOSChangeTrackerCopyManifest().
void SOSChangeTrackerSetConcrete(SOSChangeTrackerRef ct, bool isConcrete);

typedef bool(^SOSChangeTrackerUpdatesChanges)(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, CFArrayRef changes, CFErrorRef *error);

void SOSChangeTrackerRegisterChangeUpdate(SOSChangeTrackerRef ct, SOSChangeTrackerUpdatesChanges child);

typedef bool(^SOSChangeTrackerUpdatesManifests)(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error);

void SOSChangeTrackerRegisterManifestUpdate(SOSChangeTrackerRef ct, SOSChangeTrackerUpdatesManifests child);

// Remove any blocks registered though either SOSChangeTrackerRegisterChangeUpdate or SOSChangeTrackerRegisterManifestUpdate
void SOSChangeTrackerResetRegistration(SOSChangeTrackerRef ct);

// Set the manifest for this changeTracker, causing it to be updated whenever changes pass by.
// Set the manifest to NULL to stop tracking changes (but not forwarding to children).
void SOSChangeTrackerSetManifest(SOSChangeTrackerRef ct, SOSManifestRef manifest);

// Return a snapshot of the current manifest of this ct.
SOSManifestRef SOSChangeTrackerCopyManifest(SOSChangeTrackerRef ct, CFErrorRef *error);

// Apply changes to the (cached) manifest, and notify all children accordingly
bool SOSChangeTrackerTrackChanges(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, CFArrayRef changes, CFErrorRef *error);

__END_DECLS

#endif /* !_SEC_SOSCHANGETRACKER_H_ */
