/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
 @header SOSPeer
 The functions provided in SOSPeer provide an interface to a
 secure object syncing peer in a circle
 */

#ifndef _SOSPEER_H_
#define _SOSPEER_H_

#include "keychain/SecureObjectSync/SOSCoder.h"
#include "keychain/SecureObjectSync/SOSDataSource.h" // For SOSEngineRef
#include "utilities/SecAKSWrappers.h" // TODO: Layer violation -> move to datasource or make schema based

__BEGIN_DECLS

// PeerMetaRef are used to paas info about peers between accout and engine as well as to serialize in the peerstate.
typedef CFTypeRef SOSPeerMetaRef;


// peerID is not optional everything else is.
SOSPeerMetaRef SOSPeerMetaCreateWithComponents(CFStringRef peerID, CFSetRef views, CFDataRef keybag);

// peerID and state are both required.  State is excepted to contain the set of views for this peer.
SOSPeerMetaRef SOSPeerMetaCreateWithState(CFStringRef peerID, CFDictionaryRef state);

CFTypeRef SOSPeerOrStateSetViewsKeyBagAndCreateCopy(CFTypeRef peerOrState, CFSetRef views, CFDataRef keyBag);
CFTypeRef SOSPeerOrStateSetViewsAndCopyState(CFTypeRef peerOrState, CFSetRef views);
bool SOSPeerMapEntryIsBackup(const void *mapEntry);

// peerID will always be returned, views, and publicKey might not be.
CFStringRef SOSPeerMetaGetComponents(SOSPeerMetaRef peerMeta, CFSetRef *views, CFDataRef *keybag, CFErrorRef *error);


typedef struct __OpaqueSOSPeer *SOSPeerRef;

CFTypeID SOSPeerGetTypeID(void);

void SOSPeerMarkDigestsInUse(SOSPeerRef peer, struct SOSDigestVector *mdInUse);
void SOSPeerAddManifestsInUse(SOSPeerRef peer, CFMutableDictionaryRef mfc);
bool SOSPeerDidReceiveRemovalsAndAdditions(SOSPeerRef peer, SOSManifestRef absentFromRemote, SOSManifestRef additionsFromRemote,
                                           SOSManifestRef unwantedFromRemote, SOSManifestRef local, CFErrorRef *error);
bool SOSPeerDataSourceWillCommit(SOSPeerRef peer, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error);
bool SOSPeerDataSourceWillChange(SOSPeerRef peer, SOSDataSourceRef dataSource, SOSDataSourceTransactionSource source, CFArrayRef changes, CFErrorRef *error);
bool SOSPeerWriteAddEvent(FILE *journalFile, keybag_handle_t kbhandle, SOSDataSourceRef dataSource, SOSObjectRef object, CFErrorRef *error);

// Create a peer from an archived state.
SOSPeerRef SOSPeerCreateWithState(SOSEngineRef engine, CFStringRef peer_id, CFDictionaryRef state, CFErrorRef *error);

// Return encoded peerState dictionary
CFDictionaryRef SOSPeerCopyState(SOSPeerRef peer, CFErrorRef *error);

// (Re)initialize from a peerState dictionary
bool SOSPeerSetState(SOSPeerRef peer, SOSEngineRef engine, CFDictionaryRef state, CFErrorRef *error);
void SOSPeerSetOTRTimer(SOSPeerRef peer, dispatch_source_t timer);
dispatch_source_t SOSPeerGetOTRTimer(SOSPeerRef peer);
void SOSPeerRemoveOTRTimerEntry(SOSPeerRef peer);
bool SOSPeerTimerForPeerExist(SOSPeerRef peer);
    
//
//
//

CFIndex SOSPeerGetVersion(SOSPeerRef peer);
CFStringRef SOSPeerGetID(SOSPeerRef peer);
bool SOSPeersEqual(SOSPeerRef peerA, SOSPeerRef peerB);

uint64_t SOSPeerNextSequenceNumber(SOSPeerRef peer);
uint64_t SOSPeerGetMessageVersion(SOSPeerRef peer);

//
// MARK: State tracking helpers
//

// Return true if the peer needs saving.
bool SOSPeerDidConnect(SOSPeerRef peer);
bool SOSPeerMustSendMessage(SOSPeerRef peer);
void SOSPeerSetMustSendMessage(SOSPeerRef peer, bool must);

bool SOSPeerSendObjects(SOSPeerRef peer);
void SOSPeerSetSendObjects(SOSPeerRef peer, bool sendObjects);

bool SOSPeerHasBeenInSync(SOSPeerRef peer);
void SOSPeerSetHasBeenInSync(SOSPeerRef peer, bool hasBeenInSync);

SOSManifestRef SOSPeerGetProposedManifest(SOSPeerRef peer);
SOSManifestRef SOSPeerGetConfirmedManifest(SOSPeerRef peer);
void SOSPeerSetConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed);
void SOSPeerAddProposedManifest(SOSPeerRef peer, SOSManifestRef pending);
void SOSPeerSetProposedManifest(SOSPeerRef peer, SOSManifestRef pending);
void SOSPeerAddLocalManifest(SOSPeerRef peer, SOSManifestRef local);
SOSManifestRef SOSPeerGetPendingObjects(SOSPeerRef peer);
void SOSPeerSetPendingObjects(SOSPeerRef peer, SOSManifestRef pendingObjects);
SOSManifestRef SOSPeerGetUnwantedManifest(SOSPeerRef peer);
void SOSPeerSetUnwantedManifest(SOSPeerRef peer, SOSManifestRef unwantedManifest);

SOSManifestRef SOSPeerCopyManifestForDigest(SOSPeerRef peer, CFDataRef digest);

CFSetRef SOSPeerGetViewNameSet(SOSPeerRef peer);
void SOSPeerSetViewNameSet(SOSPeerRef peer, CFSetRef views);

CFDataRef SOSPeerGetKeyBag(SOSPeerRef peer);
void SOSPeerKeyBagDidChange(SOSPeerRef peer);
void SOSPeerSetKeyBag(SOSPeerRef peer, CFDataRef keyBag);
// Write a reset event to the journal if mustSendMessage is true.
bool SOSPeerWritePendingReset(SOSPeerRef peer, CFErrorRef *error);

//
// MARK: Backup Peers
//

// TODO: Layer violation -> move to datasource or make schema based
bool SOSPeerAppendToJournal(SOSPeerRef peer, CFErrorRef *error, void(^with)(FILE *journalFile, keybag_handle_t kbhandle));
int SOSPeerHandoffFD(SOSPeerRef peer, CFErrorRef *error);

void SOSBackupPeerPostNotification(const char *reason);

//
// MARK: RateLimiting
//
void SOSPeerSetRateLimiter(SOSPeerRef peer, CFTypeRef limiter);
CFTypeRef SOSPeerGetRateLimiter(SOSPeerRef peer);
bool SOSPeerShouldRateLimit(CFArrayRef attributes, SOSPeerRef peer);

__END_DECLS

#endif /* !_SOSPEER_H_ */
