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


#ifndef _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_
#define _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

#include <xpc/xpc.h>
#include <Security/SecKey.h>

#if TARGET_OS_TV
#define SOS_AVAILABLE false
#elif TARGET_OS_WATCH
#define SOS_AVAILABLE false
#elif TARGET_OS_BRIDGE
#define SOS_AVAILABLE false
#elif TARGET_OS_IOS
#define SOS_AVAILABLE true
#elif TARGET_OS_OSX
#define SOS_AVAILABLE true
#elif TARGET_OS_SIMULATOR
#define SOS_AVAILABLE true
#else
#define SOS_AVAILABLE false
#endif

#define IF_SOS_DISABLED if(!SOS_AVAILABLE)

__BEGIN_DECLS

// Use the kSecAttrViewHint* constants in SecItemPriv.h instead

extern const CFStringRef kSOSViewHintPCSMasterKey DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiCloudDrive DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSPhotos DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSCloudKit DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSEscrow DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSFDE DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSMailDrop DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiCloudBackup DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSNotes DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiMessage DEPRECATED_ATTRIBUTE;

extern const CFStringRef kSOSViewHintAppleTV DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintHomeKit DEPRECATED_ATTRIBUTE;

CFArrayRef SOSCCCopyConcurringPeerPeerInfo(CFErrorRef* error);

bool SOSCCPurgeUserCredentials(CFErrorRef* error);

CFStringRef SOSCCGetStatusDescription(SOSCCStatus status);
CFStringRef SOSCCGetViewResultDescription(SOSViewResultCode vrc);
bool SOSCCAccountHasPublicKey(CFErrorRef *error);

/*!
 @function SOSCCProcessSyncWithPeers
 @abstract Returns the peers for whom we handled syncing from the list send to us.
 @param peers Set of peerIDs to sync with
 @param backupPeers Set of backup peerIDs to sync with
 */
CFSetRef /* CFString */ SOSCCProcessSyncWithPeers(CFSetRef peers, CFSetRef backupPeers, CFErrorRef* error);

/*!
 @function SOSCCProcessSyncWithAllPeers
 @abstract Returns the information (string, hopefully URL) that will lead to an explanation of why you have an incompatible circle.
 @param error What went wrong if we returned NULL.
 */
SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers(CFErrorRef* error);

bool SOSCCProcessEnsurePeerRegistration(CFErrorRef* error);

bool SOSCCCleanupKVSKeys(CFErrorRef *error);


/*!
 @function SOSCCCopyMyPeerInfo
 @abstract Returns a copy of my peer info
 @param error What went wrong if we returned NULL
 */
SOSPeerInfoRef SOSCCCopyMyPeerInfo(CFErrorRef *error);

//
// Security Tool calls
//
CFDataRef SOSCCCopyRecoveryPublicKey(CFErrorRef *error);
CFDataRef SOSCCCopyInitialSyncData(SOSInitialSyncFlags flags, CFErrorRef *error);

void SOSCCForEachEngineStateAsStringFromArray(CFArrayRef states, void (^block)(CFStringRef oneStateString));

__END_DECLS

#endif
