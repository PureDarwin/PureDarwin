/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SOSDataSource.h
 The functions provided in SOSDataSource.h provide the protocol to a
 secure object syncing data source.  This is something than can produce
 manifests and manifest digests and query objects by digest and merge
 objects into the data source.
 */

#ifndef _SEC_SOSDATASOURCE_H_
#define _SEC_SOSDATASOURCE_H_

#include "keychain/SecureObjectSync/SOSManifest.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "utilities/array_size.h"
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFError.h"

__BEGIN_DECLS

/* SOSDataSource protocol (non opaque). */
typedef struct SOSDataSourceFactory *SOSDataSourceFactoryRef;
typedef struct SOSDataSource *SOSDataSourceRef;
typedef struct __OpaqueSOSEngine *SOSEngineRef;
typedef struct __OpaqueSOSObject *SOSObjectRef;
typedef struct __OpaqueSOSTransaction *SOSTransactionRef;

//
// MARK: - SOSDataSourceFactory protocol
//
struct SOSDataSourceFactory {
    CFStringRef      (*copy_name)(SOSDataSourceFactoryRef factory);
    SOSDataSourceRef (*create_datasource)(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error);
    void             (*release)(SOSDataSourceFactoryRef factory);
};

static inline CFStringRef SOSDataSourceFactoryCopyName(SOSDataSourceFactoryRef dsf) {
    return dsf->copy_name(dsf);
}

static inline SOSDataSourceRef SOSDataSourceFactoryCreateDataSource(SOSDataSourceFactoryRef dsf, CFStringRef dataSourceName, CFErrorRef *error) {
    (void)SecRequirementError(dsf != NULL, error, CFSTR("No datasource"));
    return dsf ? dsf->create_datasource(dsf, dataSourceName, error) : NULL;
}

static inline void SOSDataSourceFactoryRelease(SOSDataSourceFactoryRef dsf) {
    dsf->release(dsf);
}

//
// MARK: - SOSDataSource protocol
//

/* Implement this if you want to create a new type of sync client.
   Currently we support keychains, but the engine should scale to
   entire filesystems. */
enum SOSMergeResult {
    kSOSMergeFailure = 0,   // CFErrorRef returned, no error returned in any other case
    kSOSMergeLocalObject,   // We choose the current object in the dataSource the manifest is still valid.
    kSOSMergePeersObject,   // We chose the peers object over our own, manifest is now dirty.
    kSOSMergeCreatedObject, // *createdObject is returned and should be released
};
typedef CFIndex SOSMergeResult;

//
// MARK: SOSDataSourceTransactionType
//
enum SOSDataSourceTransactionType {
    kSOSDataSourceNoneTransactionType = 0,
    kSOSDataSourceImmediateTransactionType,
    kSOSDataSourceExclusiveTransactionType,
    kSOSDataSourceNormalTransactionType,
    kSOSDataSourceExclusiveRemoteTransactionType,
};
typedef CFOptionFlags SOSDataSourceTransactionType;

enum SOSDataSourceTransactionPhase {
    kSOSDataSourceTransactionDidRollback = 0,   // A transaction just got rolled back
    kSOSDataSourceTransactionWillCommit,        // A transaction is about to commit.
    kSOSDataSourceTransactionDidCommit,         // A transaction successfully committed.
};
typedef CFOptionFlags SOSDataSourceTransactionPhase;

// These must match the values in SecDbTransactionSource
enum SOSDataSourceTransactionSource {
    kSOSDataSourceSOSTransaction = 0,        // A remotely initated transaction.
    kSOSDataSourceCKKSTransaction = 3,       // A transaction initiated by CKKS.
    kSOSDataSourceAPITransaction = 1,        // A user initated transaction.
};
typedef CFOptionFlags SOSDataSourceTransactionSource;

typedef void (^SOSDataSourceNotifyBlock)(SOSDataSourceRef ds, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, CFArrayRef changes);

//
// MARK: - SOSDataSource struct
//

struct SOSDataSource {
    // SOSEngine - every datasource has an engine that is notified of changes
    // to the datasource.
    SOSEngineRef engine;

    // General SOSDataSource methods
    CFStringRef (*dsGetName)(SOSDataSourceRef ds);
    void (*dsAddNotifyPhaseBlock)(SOSDataSourceRef ds, SOSDataSourceNotifyBlock notifyBlock);
    SOSManifestRef (*dsCopyManifestWithViewNameSet)(SOSDataSourceRef ds, CFSetRef viewNameSet, CFErrorRef *error);
    bool (*dsForEachObject)(SOSDataSourceRef ds, SOSTransactionRef txn, SOSManifestRef manifest, CFErrorRef *error, void (^handleObject)(CFDataRef key, SOSObjectRef object, bool *stop));
    CFDataRef (*dsCopyStateWithKey)(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error);
    CFDataRef (*dsCopyItemDataWithKeys)(SOSDataSourceRef ds, CFDictionaryRef keys, CFErrorRef *error);
    bool (*dsDeleteStateWithKey)(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error);

    bool (*dsWith)(SOSDataSourceRef ds, CFErrorRef *error, SOSDataSourceTransactionSource source, bool onCommitQueue, void(^transaction)(SOSTransactionRef txn, bool *commit));
    bool (*dsRelease)(SOSDataSourceRef ds, CFErrorRef *error); // Destructor
    bool (*dsReadWith)(SOSDataSourceRef ds, CFErrorRef *error, SOSDataSourceTransactionSource source, void(^perform)(SOSTransactionRef txn));

    // SOSTransaction methods, writes to a dataSource require a transaction.
    SOSMergeResult (*dsMergeObject)(SOSTransactionRef txn, SOSObjectRef object, SOSObjectRef *createdObject, CFErrorRef *error);
    bool (*dsSetStateWithKey)(SOSDataSourceRef ds, SOSTransactionRef txn, CFStringRef pdmn, CFStringRef key, CFDataRef state, CFErrorRef *error);
    bool (*dsRestoreObject)(SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error);

    // SOSObject methods
    CFDataRef (*objectCopyDigest)(SOSObjectRef object, CFErrorRef *error);
    CFDateRef (*objectCopyModDate)(SOSObjectRef object, CFErrorRef *error);
    SOSObjectRef (*objectCreateWithPropertyList)(CFDictionaryRef plist, CFErrorRef *error);
    CFDictionaryRef (*objectCopyPropertyList)(SOSObjectRef object, CFErrorRef *error);
    CFDictionaryRef (*objectCopyBackup)(SOSObjectRef object, uint64_t handle, CFErrorRef *error);
};

//
// MARK: - SOSDataSource protocol implementation
//
static inline SOSEngineRef SOSDataSourceGetSharedEngine(SOSDataSourceRef ds, CFErrorRef *error) {
    return ds->engine;
}

static inline CFStringRef SOSDataSourceGetName(SOSDataSourceRef ds) {
    return ds->dsGetName(ds);
}

static inline void SOSDataSourceAddNotifyPhaseBlock(SOSDataSourceRef ds, SOSDataSourceNotifyBlock notifyBlock) {
    ds->dsAddNotifyPhaseBlock(ds, notifyBlock);
}

static inline SOSManifestRef SOSDataSourceCopyManifestWithViewNameSet(SOSDataSourceRef ds, CFSetRef viewNameSet, CFErrorRef *error) {
    return ds->dsCopyManifestWithViewNameSet(ds, viewNameSet, error);
}

static inline bool SOSDataSourceForEachObject(SOSDataSourceRef ds, SOSTransactionRef txn, SOSManifestRef manifest, CFErrorRef *error, void (^handleObject)(CFDataRef digest, SOSObjectRef object, bool *stop)) {
    return ds->dsForEachObject(ds, txn, manifest, error, handleObject);
}

static inline bool SOSDataSourceWith(SOSDataSourceRef ds, CFErrorRef *error,
                                     void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    return ds->dsWith(ds, error, kSOSDataSourceSOSTransaction, false, transaction);
}

static inline bool SOSDataSourceWithCommitQueue(SOSDataSourceRef ds, CFErrorRef *error,
                                     void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    return ds->dsWith(ds, error, kSOSDataSourceSOSTransaction, true, transaction);
}

static inline bool SOSDataSourceWithAPI(SOSDataSourceRef ds, bool isAPI, CFErrorRef *error,
                                     void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    return ds->dsWith(ds, error, isAPI ? kSOSDataSourceAPITransaction : kSOSDataSourceSOSTransaction, false, transaction);
}

static inline bool SOSDataSourceReadWithCommitQueue(SOSDataSourceRef ds, CFErrorRef *error,
                                                void(^perform)(SOSTransactionRef txn)) {
    return ds->dsReadWith(ds, error, kSOSDataSourceSOSTransaction, perform);
}


static inline CFDataRef SOSDataSourceCopyStateWithKey(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error)
{
    return ds->dsCopyStateWithKey(ds, key, pdmn, txn, error);
}

static inline CFDataRef SOSDataSourceCopyItemDataWithKeys(SOSDataSourceRef ds, CFDictionaryRef keys, CFErrorRef *error) {
    return ds->dsCopyItemDataWithKeys(ds, keys, error);
}

static inline bool SOSDataSourceDeleteStateWithKey(SOSDataSourceRef ds, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error)
{
    return ds->dsDeleteStateWithKey(ds, key, pdmn, txn, error);
}

static inline bool SOSDataSourceRelease(SOSDataSourceRef ds, CFErrorRef *error) {
    return !ds || ds->dsRelease(ds, error);
}

//
// MARK: - SOSTransaction
//

static inline SOSMergeResult SOSDataSourceMergeObject(SOSDataSourceRef ds, SOSTransactionRef txn, SOSObjectRef peersObject, SOSObjectRef *mergedObject, CFErrorRef *error) {
    return ds->dsMergeObject(txn, peersObject, mergedObject, error);
}

static inline bool SOSDataSourceSetStateWithKey(SOSDataSourceRef ds, SOSTransactionRef txn, CFStringRef key, CFStringRef pdmn, CFDataRef state, CFErrorRef *error)
{
    return ds->dsSetStateWithKey(ds, txn, key, pdmn, state, error);
}


//
// MARK: - SOSObject methods
//
static inline CFDataRef SOSObjectCopyDigest(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyDigest(object, error);
}

static inline CFDateRef SOSObjectCopyModificationDate(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyModDate(object, error);
}

static inline SOSObjectRef SOSObjectCreateWithPropertyList(SOSDataSourceRef ds, CFDictionaryRef plist, CFErrorRef *error) {
    return ds->objectCreateWithPropertyList(plist, error);
}

static inline CFDictionaryRef SOSObjectCopyPropertyList(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error) {
    return ds->objectCopyPropertyList(object, error);
}

static inline CFDictionaryRef SOSObjectCopyBackup(SOSDataSourceRef ds, SOSObjectRef object, uint64_t handle, CFErrorRef *error) {
    return ds->objectCopyBackup(object, handle, error);
}

static inline bool SOSObjectRestoreObject(SOSDataSourceRef ds, SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error) {
    return ds->dsRestoreObject(txn, handle, item, error);
}


//
// MARK: SOSDataSourceFactory helpers
//

static inline SOSEngineRef SOSDataSourceFactoryGetEngineForDataSourceName(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error)
{
    SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(factory, dataSourceName, error);
    SOSEngineRef engine = ds ? SOSDataSourceGetSharedEngine(ds, error) : (SOSEngineRef) NULL;
    SOSDataSourceRelease(ds, NULL); // TODO: Log this error?!
    
    return engine;
}

__END_DECLS

#endif /* !_SEC_SOSDATASOURCE_H_ */
