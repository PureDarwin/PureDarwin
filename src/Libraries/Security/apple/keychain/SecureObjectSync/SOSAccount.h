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
 @header SOSAccount.h
 The functions provided in SOSCircle.h provide an interface to a
 secure object syncing circle for a single class
 */

#ifndef _SOSACCOUNT_H_
#define _SOSACCOUNT_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSCircle.h"
#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSRing.h"
#include "keychain/SecureObjectSync/SOSRecoveryKeyBag.h"
#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#include <dispatch/dispatch.h>

extern NSString* const kSOSIdentityStatusCompleteIdentity;
extern NSString* const kSOSIdentityStatusKeyOnly;
extern NSString* const kSOSIdentityStatusPeerOnly;

@class SOSAccount;

__BEGIN_DECLS

#define RETIREMENT_FINALIZATION_SECONDS (24*60*60)

typedef void (^SOSAccountCircleMembershipChangeBlock)(SOSAccount* account,
                                                      SOSCircleRef new_circle,
                                                      CFSetRef added_peers, CFSetRef removed_peers,
                                                      CFSetRef added_applicants, CFSetRef removed_applicants);

CFTypeID SOSAccountGetTypeID(void);

SOSAccount*  SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory);

//
// MARK: Credential management
//

SecKeyRef SOSAccountGetTrustedPublicCredential(SOSAccount*  account, CFErrorRef* error);

SecKeyRef SOSAccountGetPrivateCredential(SOSAccount*  account, CFErrorRef* error);
CFDataRef SOSAccountGetCachedPassword(SOSAccount*  account, CFErrorRef* error);
void      SOSAccountStashAccountKey(SOSAccount* account);
SecKeyRef SOSAccountCopyStashedUserPrivateKey(SOSAccount* account, CFErrorRef *error);

void SOSAccountSetParameters(SOSAccount*  account, CFDataRef parameters);

void SOSAccountPurgePrivateCredential(SOSAccount*  account);

void SOSAccountRestartPrivateCredentialTimer(SOSAccount*  account);

bool SOSAccountTryUserCredentials(SOSAccount*  account,
                                  CFStringRef user_account, CFDataRef user_password,
                                  CFErrorRef *error);

bool SOSAccountTryUserPrivateKey(SOSAccount* account, SecKeyRef user_private, CFErrorRef *error);

bool SOSAccountValidateAccountCredential(SOSAccount* account, SecKeyRef accountPrivateKey, CFErrorRef *error);
bool SOSAccountAssertStashedAccountCredential(SOSAccount* account, CFErrorRef *error);
bool SOSAccountAssertUserCredentials(SOSAccount*  account,
                                     CFStringRef user_account, CFDataRef user_password,
                                     CFErrorRef *error);

bool SOSAccountRetryUserCredentials(SOSAccount*  account);
void SOSAccountSetUnTrustedUserPublicKey(SOSAccount*  account, SecKeyRef publicKey);

bool SOSAccountGenerationSignatureUpdate(SOSAccount*  account, CFErrorRef *error);

//
// MARK: Circle management
//

bool SOSAccountUpdateCircle(SOSAccount*  account, SOSCircleRef circle, CFErrorRef *error);
void SOSTransportEachMessage(SOSAccount*  account, CFDictionaryRef updates, CFErrorRef *error);


CFStringRef SOSAccountGetSOSCCStatusString(SOSCCStatus status);
SOSCCStatus SOSAccountGetSOSCCStatusFromString(CFStringRef status);
bool SOSAccountJoinCircles(SOSAccountTransaction* aTxn, CFErrorRef* error);
bool SOSAccountJoinCirclesAfterRestore(SOSAccountTransaction* aTxn, CFErrorRef* error);
bool SOSAccountRemovePeersFromCircle(SOSAccount*  account, CFArrayRef peers, CFErrorRef* error);
bool SOSAccountBail(SOSAccount*  account, uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSAccountAcceptApplicants(SOSAccount*  account, CFArrayRef applicants, CFErrorRef* error);
bool SOSAccountRejectApplicants(SOSAccount*  account, CFArrayRef applicants, CFErrorRef* error);

bool SOSValidateUserPublic(SOSAccount*  account, CFErrorRef* error);

void SOSAccountForEachCirclePeerExceptMe(SOSAccount*  account, void (^action)(SOSPeerInfoRef peer));

CFArrayRef SOSAccountCopyApplicants(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyGeneration(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyValidPeers(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyPeersToListenTo(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyNotValidPeers(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyRetired(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyViewUnaware(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyPeers(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyActivePeers(SOSAccount*  account, CFErrorRef *error);
CFArrayRef CF_RETURNS_RETAINED SOSAccountCopyActiveValidPeers(SOSAccount*  account, CFErrorRef *error);
CFArrayRef SOSAccountCopyConcurringPeers(SOSAccount*  account, CFErrorRef *error);

bool SOSAccountIsAccountIdentity(SOSAccount*  account, SOSPeerInfoRef peer_info, CFErrorRef *error);

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccount*  account, CFErrorRef* error);

//
// MARK: iCloud Identity
//
bool SOSAccountRemoveIncompleteiCloudIdentities(SOSAccount*  account, SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error);

//
// MARK: Change blocks
//
void SOSAccountAddChangeBlock(SOSAccount*  a, SOSAccountCircleMembershipChangeBlock changeBlock);
void SOSAccountRemoveChangeBlock(SOSAccount*  a, SOSAccountCircleMembershipChangeBlock changeBlock);


//
// MARK: Local device gestalt change.
//
CFDictionaryRef SOSAccountCopyGestalt(SOSAccount*  account);

CFDictionaryRef SOSAccountCopyV2Dictionary(SOSAccount*  account);

void SOSAccountPendDisableViewSet(SOSAccount*  account, CFSetRef disabledViews);

void SOSAccountUpdateOutOfSyncViews(SOSAccountTransaction* aTxn, CFSetRef viewsInSync);
void SOSAccountPeerGotInSync(SOSAccountTransaction* aTxn, CFStringRef peerID, CFSetRef views);

bool SOSAccountHandleParametersChange(SOSAccount*  account, CFDataRef updates, CFErrorRef *error);

//
// MARK: Local device key access from account object - can call without lock without endangering peerinfo.
//
SecKeyRef SOSAccountCopyDevicePrivateKey(SOSAccount* account, CFErrorRef *error);
SecKeyRef SOSAccountCopyDevicePublicKey(SOSAccount* account, CFErrorRef *error);

//
// MARK: Requests for syncing later
//
bool SOSAccountRequestSyncWithAllPeers(SOSAccountTransaction* txn, CFErrorRef *error);
CF_RETURNS_RETAINED CFMutableSetRef SOSAccountSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peerIDs, CFErrorRef *error);
CFSetRef SOSAccountSyncWithPeersOverKVS(SOSAccountTransaction* txn,  CFSetRef peers);
bool SOSAccountInflateTransports(SOSAccount* account, CFStringRef circleName, CFErrorRef *error);

void
SOSAccountTriggerSyncWithBackupPeer(CFStringRef peer);

//
// MARK: Outgoing/Sync functions
//      

bool SOSAccountSyncWithKVSPeerWithMessage(SOSAccountTransaction* txn, CFStringRef peerid, CFDataRef message, CFErrorRef *error);

CF_RETURNS_RETAINED CFSetRef SOSAccountProcessSyncWithPeers(SOSAccountTransaction* txn, CFSetRef /* CFStringRef */ peers, CFSetRef /* CFStringRef */ backupPeers, CFErrorRef *error);
CF_RETURNS_RETAINED CFSetRef SOSAccountCopyBackupPeersAndForceSync(SOSAccountTransaction* txn, CFErrorRef *error);

//
// MARK: Cleanup functions
//

bool SOSAccountScanForRetired(SOSAccount*  account, SOSCircleRef circle, CFErrorRef *error);
CF_RETURNS_RETAINED SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccount*  account, SOSCircleRef starting_circle, CFErrorRef *error);

//
// MARK: Backup functions
//

bool SOSAccountIsBackupRingEmpty(SOSAccount*  account, CFStringRef viewName);
bool SOSAccountNewBKSBForView(SOSAccount*  account, CFStringRef viewName, CFErrorRef *error);

void SOSAccountProcessBackupRings(SOSAccount*  account);
bool SOSAccountValidateBackupRingForView(SOSAccount*  account, CFStringRef viewName, CFErrorRef *error);
bool SOSAccountSetBackupPublicKey(SOSAccountTransaction* aTxn, CFDataRef backupKey, CFErrorRef *error);
bool SOSAccountRemoveBackupPublickey(SOSAccountTransaction* aTxn, CFErrorRef *error);
bool SOSAccountBackupUpdateBackupPublicKey(SOSAccount *account, CFDataRef backupKey);
bool SOSAccountSetBSKBagForAllSlices(SOSAccount*  account, CFDataRef backupSlice, bool setupV0Only, CFErrorRef *error);

CF_RETURNS_RETAINED SOSBackupSliceKeyBagRef SOSAccountBackupSliceKeyBagForView(SOSAccount*  account, CFStringRef viewName, CFErrorRef* error);

//
// MARK: Recovery Public Key Functions
//
bool SOSAccountRegisterRecoveryPublicKey(SOSAccountTransaction* txn, CFDataRef recovery_key, CFErrorRef *error);
CFDataRef SOSAccountCopyRecoveryPublicKey(SOSAccountTransaction* txn, CFErrorRef *error);
bool SOSAccountClearRecoveryPublicKey(SOSAccountTransaction* txn, CFDataRef recovery_key, CFErrorRef *error);


// Internal calls that sets or clears Recovery Keys for the Account Object Provided by Clients
bool SOSAccountSetRecoveryKey(SOSAccount* account, CFDataRef pubData, CFErrorRef *error);
bool SOSAccountRemoveRecoveryKey(SOSAccount* account, CFErrorRef *error);


CFDataRef SOSAccountCopyRecoveryPublic(CFAllocatorRef allocator, SOSAccount* account, CFErrorRef *error);
bool SOSAccountRecoveryKeyIsInBackupAndCurrentInView(SOSAccount* account, CFStringRef viewname);
bool SOSAccountSetRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccount* account, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error);
SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccount* account, CFErrorRef *error);
void SOSAccountEnsureRecoveryRing(SOSAccount* account);

//
// MARK: Private functions
//

dispatch_queue_t SOSAccountGetQueue(SOSAccount*  account);

typedef bool (^SOSAccountSendBlock)(CFStringRef key, CFDataRef message, CFErrorRef *error);

//
// MARK: Utility functions
//

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests);

//
// MARK: HSA2 Piggyback Support Functions
//
SOSPeerInfoRef SOSAccountCopyApplication(SOSAccount*  account, CFErrorRef*);
CFDataRef SOSAccountCopyCircleJoiningBlob(SOSAccount*  account, SOSPeerInfoRef applicant, CFErrorRef *error);
bool SOSAccountJoinWithCircleJoiningBlob(SOSAccount*  account, CFDataRef joiningBlob, PiggyBackProtocolVersion version, CFErrorRef *error);
CFDataRef SOSAccountCopyInitialSyncData(SOSAccount* account, SOSInitialSyncFlags flags, CFErrorRef *error);
    
//
// MARK: Initial-Sync
//
CFMutableSetRef SOSAccountCopyUnsyncedInitialViews(SOSAccount*  account);

//
// MARK: State Logging
//
void SOSAccountLogState(SOSAccount*  account);
void SOSAccountLogViewState(SOSAccount*  account);
void SOSAccountConsiderLoggingEngineState(SOSAccountTransaction* txn);

//
// MARK: Checking other peer views
//

CFBooleanRef SOSAccountPeersHaveViewsEnabled(SOSAccount*  account, CFArrayRef viewNames, CFErrorRef *error);

void SOSAccountSetTestSerialNumber(SOSAccount*  account, CFStringRef serial);
SOSViewResultCode SOSAccountVirtualV0Behavior(SOSAccount*  account, SOSViewActionCode actionCode);


bool SOSAccountIsPeerRetired(SOSAccount* account, CFSetRef peers);
void SOSAccountNotifyOfChange(SOSAccount* account, SOSCircleRef oldCircle, SOSCircleRef newCircle);


//
// MARK: Syncing status functions
//
bool SOSAccountMessageFromPeerIsPending(SOSAccountTransaction* txn, SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSAccountSendToPeerIsPending(SOSAccountTransaction* txn, SOSPeerInfoRef peer, CFErrorRef *error);

//
// MARK: OTR
//
void SOSAccountResetOTRNegotiationCoder(SOSAccount* account, CFStringRef peerid);
void SOSAccountTimerFiredSendNextMessage(SOSAccountTransaction* txn, NSString* peerid, NSString* accessGroup);

NSArray<NSDictionary *>* SOSAccountGetAllTLKs(void);
NSArray<NSDictionary *>* SOSAccountGetSelectedTLKs(void);

CF_RETURNS_RETAINED CFMutableArrayRef SOSAccountCopyiCloudIdentities(SOSAccount* account);

bool SOSAccountEvaluateKeysAndCircle(SOSAccountTransaction *txn, CFErrorRef *block_error);

//
// MARK: Remove V0 Peers
bool SOSAccountRemoveV0Clients(SOSAccount *account, CFErrorRef *error);


__END_DECLS

#endif /* !_SOSACCOUNT_H_ */
