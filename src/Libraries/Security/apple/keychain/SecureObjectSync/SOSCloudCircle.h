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

//
//  SOSCloudCircle.h
//

#ifndef _SECURITY_SOSCLOUDCIRCLE_H_
#define _SECURITY_SOSCLOUDCIRCLE_H_

#if __OBJC__
#import <Foundation/Foundation.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFError.h>

#include <Security/SecureObjectSync/SOSTypes.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

__BEGIN_DECLS


//
// CFError info for propogated errors
//

extern const CFStringRef kSOSErrorDomain;

enum {
    kSOSErrorPrivateKeyAbsent = 1,
    kSOSErrorPublicKeyAbsent = 2,

    kSOSErrorWrongPassword = 3,

    kSOSErrorNotReady = 4, // System not yet ready (before first unlock)

    kSOSErrorIncompatibleCircle = 5, // We saw an incompatible circle out there.
    kSOSInitialSyncFailed = 6,  //we timed out when syncing during approving from another device
    kSOSEntitlementMissing = 7,

    KSOSCantParseSOSMessage = 8,
};

//
// Types
//

typedef CF_OPTIONS(uint32_t, SOSInitialSyncFlags) {
    kSOSInitialSyncFlagTLKs = (1UL << 0),
    kSOSInitialSyncFlagiCloudIdentity = (1UL << 1),
    kSOSInitialSyncFlagTLKsRequestOnly = (1UL << 2), // Note that this overrides the other two flags, as it's used for aborting the piggybacking session early and returning a very small number of TLKs
};


enum {
    kSOSCCInCircle          = 0,
    kSOSCCNotInCircle       = 1,
    kSOSCCRequestPending    = 2,
    kSOSCCCircleAbsent      = 3,
    kSOSCCError             = -1,   // unable to determine circle status, inspect CFError to find out why
};

typedef int SOSCCStatus;

extern const char * kSOSCCCircleChangedNotification;
extern const char * kSOSCCViewMembershipChangedNotification;
extern const char * kSOSCCInitialSyncChangedNotification;
extern const char * kSOSCCHoldLockForInitialSync;
extern const char * kSOSCCPeerAvailable;
extern const char * kSOSCCRecoveryKeyChanged;
extern const char * kSOSCCCircleOctagonKeysChangedNotification;

/*!
 @function SOSCCSetUserCredentials
 @abstract Uses the user authentication credential (password) to create an internal EC Key Pair for authenticating Circle changes.
 @param user_label This string can be used for a label to tag the resulting credential data for persistent storage.
 @param user_password The user's password that's used as input to generate EC keys for Circle authenticating operations.
 @param error What went wrong if we returned false.
 @discussion This call needs to be made whenever a call that updates a Cloud Circle returns an error of kSOSErrorPrivateKeyAbsent (credential timeout) or kSOSErrorPublicKeyAbsent (programmer error).

     Any caller to SetUserCredential is asserting that they know the credential is correct.

     If you are uncertain (unable to verify) use TryUserCredentials, but if you can know it's better
     to call Set so we can recover from password change.
 */

bool SOSCCSetUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error);


/*!
 @function SOSCCSetUserCredentialsAndDSID
 @abstract Uses the user authentication credential (password) to create an internal EC Key Pair for authenticating Circle changes.  Passes the DSID to ensure user credentials are passed to the correct account.
 @param user_label This string can be used for a label to tag the resulting credential data for persistent storage.
 @param user_password The user's password that's used as input to generate EC keys for Circle authenticating operations.
 @param dsid This is a string of a dsid associated with an account
 @param error What went wrong if we returned false.
 @discussion This call needs to be made whenever a call that updates a Cloud Circle returns an error of kSOSErrorPrivateKeyAbsent (credential timeout) or kSOSErrorPublicKeyAbsent (programmer error).
 
 Any caller to SetUserCredential is asserting that they know the credential is correct.
 
 If you are uncertain (unable to verify) use TryUserCredentials, but if you can know it's better
 to call Set so we can recover from password change.
 */

bool SOSCCSetUserCredentialsAndDSID(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error);

/*!
 @function SOSCCTryUserCredentials
 @abstract Uses the user authentication credential (password) to create an internal EC Key Pair for authenticating Circle changes.
 @param user_label This string can be used for a label to tag the resulting credential data for persistent storage.
 @param user_password The user's password that's used as input to generate EC keys for Circle authenticating operations.
 @param error What went wrong if we returned false.
 @discussion When one of the user credential requiring calls below (almost all) need a credential it will fail with kSOSErrorPrivateKeyAbsent. If you don't have an outside way to confirm correctness of the password we will attempt to use the passed in value and if it doesn't match the public information we currently have we'll fail.
 */

bool SOSCCTryUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error);

/*!
 This variant adds the dsid to the call
 */

bool SOSCCTryUserCredentialsAndDSID(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error);

/*!
 @function SOSCCRegisterUserCredentials
 @abstract Deprecated name for SOSCCSetUserCredentials.
 */
bool SOSCCRegisterUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);

/*!
 @function SOSCCWaitForInitialSync
 @abstract returns true if it waited, false if we didn't due to some error
 @param error Error ref
 @return if we waited successfully
 */
bool SOSCCWaitForInitialSync(CFErrorRef* error);

/*!
 @function SOSCCCanAuthenticate
 @abstract Determines whether we currently have valid credentials to authenticate a circle operation.
 @param error What went wrong if we returned false.
 */

bool SOSCCCanAuthenticate(CFErrorRef *error);

/*!
 @function SOSCCThisDeviceIsInCircle
 @abstract Finds and returns if this devices status in the user's circle. 
 @param error What went wrong if we returned kSOSCCError.
 @result kSOSCCInCircle if we're in the circle.
 @discussion If we have an error figuring out if we're in the circle we return false and the error.
 */
SOSCCStatus SOSCCThisDeviceIsInCircle(CFErrorRef* error);

/*!
 @function SOSCCThisDeviceIsInCircleNonCached
 @abstract Finds and returns if this devices status in the user's circle.  This call is added explicitly for CDP.
 @param error What went wrong if we returned kSOSCCError.
 @result kSOSCCInCircle if we're in the circle.
 @discussion If we have an error figuring out if we're in the circle we return false and the error.
 */
SOSCCStatus SOSCCThisDeviceIsInCircleNonCached(CFErrorRef* error);

/*!
 @function SOSCCIsIcloudKeychainSyncing
 @abstract determines whether baseline keychain syncing is occuring (V0/V2)
 @result true if we're in the circle and baseline keychain syncing views 
    (kSOSViewAutofillPasswords/kSOSViewSafariCreditCards/kSOSViewWiFi/kSOSViewOtherSyncable
    are enabled. false otherwise.
 */

bool SOSCCIsIcloudKeychainSyncing(void);

/*!
 @function SOSCCIsSafariSyncing
 @abstract determines whether Safari keychain item syncing is occuring (kSOSViewAutofillPasswords/kSOSViewSafariCreditCards)
 @result true if we're in the circle and the kSOSViewAutofillPasswords/kSOSViewSafariCreditCards views are enabled. false otherwise.
 */

bool SOSCCIsSafariSyncing(void);

/*!
 @function SOSCCIsAppleTVSyncing
 @abstract determines whether appleTV keychain syncing is occuring (kSOSViewAppleTV)
 @result true if we're in the circle and the kSOSViewAppleTV view is enabled. false otherwise.
 */

bool SOSCCIsAppleTVSyncing(void);


/*!
 @function SOSCCIsHomeKitSyncing
 @abstract determines whether homekit keychain syncing is occuring (kSOSViewHomeKit)
 @result true if we're in the circle and the kSOSViewHomeKit view is enabled. false otherwise.
 */

bool SOSCCIsHomeKitSyncing(void);


/*!
 @function SOSCCIsWiFiSyncing
 @abstract determines whether WiFi keychain syncing is occuring (kSOSViewWiFi)
 @result true if we're in the circle and the kSOSViewWiFi view is enabled. false otherwise.
 */

bool SOSCCIsWiFiSyncing(void);

/*!
 @function SOSCCIsContinuityUnlockSyncing
 @abstract determines whether Continuity Unlock keychain syncing is occuring (kSOSViewContinuityUnlock)
 @result true if we're in the circle and the kSOSViewContinuityUnlock view is enabled. false otherwise.
.
 */

bool SOSCCIsContinuityUnlockSyncing(void);

/*!
 @function SOSCCRequestToJoinCircle
 @abstract Requests that this device join the circle.
 @param error What went wrong if we tried to join.
 @result true if we pushed the request out successfully. False if there was an error.
 @discussion Requests to join the user's circle or all the pending circles (other than his) if there are multiple pending circles.
 */
bool SOSCCRequestToJoinCircle(CFErrorRef* error);


/*!
 @function SOSCCRequestToJoinCircleAfterRestore
 @abstract Requests that this device join the circle and do the magic just after restore approval.
 @param error What went wrong if we tried to join.
 @result true if we joined or pushed a request out. False if we failed to try.
 @discussion Uses the cloud identity to get in the circle if it can. If it cannot it falls back on simple application.
 */
bool SOSCCRequestToJoinCircleAfterRestore(CFErrorRef* error);

/*!
 @function SOSCCAccountSetToNew
 @abstract reset account to new
 @param error What went wrong if we tried to refresh parameters
 @result true if we successfully reset the account object
 */
bool SOSCCAccountSetToNew(CFErrorRef *error);

/*!
 @function SOSCCResetToOffering
 @abstract Resets the cloud to offer this device's circle. 
 @param error What went wrong if we tried to post our circle.
 @result true if we posted the circle successfully. False if there was an error.
 */
bool SOSCCResetToOffering(CFErrorRef* error);

/*!
 @function SOSCCResetToEmpty
 @abstract Resets the cloud to a completely empty circle.
 @param error What went wrong if we tried to post our circle.
 @result true if we posted the circle successfully. False if there was an error.
 */
bool SOSCCResetToEmpty(CFErrorRef* error);

/*!
 @function SOSCCRemoveThisDeviceFromCircle
 @abstract Removes the current device from the circle.
 @param error What went wrong trying to remove ourselves.
 @result true if we posted the removal. False if there was an error.
 @discussion This removes us from the circle.
 */
bool SOSCCRemoveThisDeviceFromCircle(CFErrorRef* error);


/*!
 @function SOSCCRemoveThisDeviceFromCircle
 @abstract Removes a list of peers from the circle.
 @param peerList List of peers to ensure aren't in the circle
 @param error What went wrong trying to remove the peers.
 @result true if we posted a circle with none of the peers listed as members, false if we had an error.
 @discussion This removes peers in the list from the circle. One likely error is
             that we don't have the user credentail (need to prompt for password)
 */
bool SOSCCRemovePeersFromCircle(CFArrayRef peerList, CFErrorRef* error);

/*!
 @function SOSCCLoggedIntoAccount
 @param error value set if there are xpc errors.
 @abstract Notifies the account object that the device logged into an icloud account
 */
bool SOSCCLoggedIntoAccount(CFErrorRef* error);

/*!
 @function SOSCCLoggedOutOfAccount
 @param error value set if there are xpc errors.
 @abstract Removes the current device from the circle.  Clears the account object
 */
bool SOSCCLoggedOutOfAccount(CFErrorRef* error);

/*!
 @function SOSCCBailFromCircle_BestEffort
 @abstract Attempts to publish a retirement ticket for the current device.
 @param error What went wrong trying to remove ourselves.
 @result true if we posted the ticket. False if there was an error.
 @discussion This attempts to post a retirement ticket that should
 result in other devices removing this device from the circle.  It does so
 with a 5 second timeout.  The only use for this call is when doing a device
 erase.
 */
bool SOSCCBailFromCircle_BestEffort(uint64_t limit_in_seconds, CFErrorRef* error);

/*!
 @function SOSCCCopyApplicantPeerInfo
 @abstract Get the list of peers wishing admittance.
 @param error What went wrong.
 @result Array of PeerInfos for applying peers.
 */
CFArrayRef SOSCCCopyApplicantPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCCopyGenerationPeerInfo
 @abstract Get the list of generation count per circle.
 @param error What went wrong.
 @result Array of Circle generation counts.
 */
CFArrayRef SOSCCCopyGenerationPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCCopyValidPeerPeerInfo
 @abstract Get the list of valid peers.
 @param error What went wrong.
 @result Array of PeerInfos for applying valid peers.
 */
CFArrayRef SOSCCCopyValidPeerPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCValidateUserPublic
 @abstract Validate whether the account's user public key is trustworthy.
 @param error What went wrong.
 @result true if the user public key is trusted, false if not.
 */
bool SOSCCValidateUserPublic(CFErrorRef *error);

/*!
 @function SOSCCCopyNotValidPeerPeerInfo
 @abstract Get the list of not valid peers.
 @param error What went wrong.
 @result Array of PeerInfos for non-valid peers.
 */
CFArrayRef SOSCCCopyNotValidPeerPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCCopyRetirementPeerInfo
 @abstract Get the list of retired peers.
 @param error What went wrong.
 @result Array of PeerInfos for retired peers.
 */
CFArrayRef SOSCCCopyRetirementPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCCopyViewUnawarePeerInfo
 @abstract Copies all the peers who are in the circle but are unable to handle views.
 @param error What went wrong.
 @result   Array of peer infos.
 */
CFArrayRef SOSCCCopyViewUnawarePeerInfo(CFErrorRef* error);

/*
 * Keys to find data in engine state dictionary
 */
extern CFStringRef kSOSCCEngineStatePeerIDKey;
extern CFStringRef kSOSCCEngineStateManifestCountKey;
extern CFStringRef kSOSCCEngineStateSyncSetKey;
extern CFStringRef kSOSCCEngineStateCoderKey;
extern CFStringRef kSOSCCEngineStateManifestHashKey;

/*!
 @function SOSCCForEachEngineStateAsString
 @abstract Get a string for each peer to dump to your favorite location.
 @param error What went wrong.
 @result if we had an error.
 */
bool SOSCCForEachEngineStateAsString(CFErrorRef* error, void (^block)(CFStringRef oneStateString));

/*!
 @function SOSCCAcceptApplicants
 @abstract Accepts the applicants into the circle (requires that we recently had the user enter the credentials).
 @param applicants List of applicants to accept.
 @param error What went wrong if we tried to post our circle.
 @result true if we accepted the applicants. False if there was an error.
 */
bool SOSCCAcceptApplicants(CFArrayRef applicants, CFErrorRef* error);

/*!
 @function SOSCCRejectApplicants
 @abstract Rejects the applications for admission (requires that we recently had the user enter the credentials).
 @param applicants List of applicants to reject.
 @param error What went wrong if we tried to post our circle.
 @result true if we rejected the applicants. False if there was an error.
 */
bool SOSCCRejectApplicants(CFArrayRef applicants, CFErrorRef *error);

/*!
 @function SOSCCCopyPeerPeerInfo
 @abstract Returns peers in the circle (we may not be in it). 
 @param error What went wrong trying look at the circle.
 @result Returns a list of peers in the circle currently syncing.
 @discussion We get the list of all peers syncing in the circle.
 */
CFArrayRef SOSCCCopyPeerPeerInfo(CFErrorRef* error);

/*
 * Return values for SOSCCGetLastDepartureReason
 */
enum DepartureReason {
    kSOSDepartureReasonError = 0,
    kSOSNeverLeftCircle,       // We haven't ever left a circle
    kSOSWithdrewMembership,    // SOSCCRemoveThisDeviceFromCircle
    kSOSMembershipRevoked,     // Via reset or remote removal.
    kSOSLeftUntrustedCircle,   // We saw a circle we could no longer trust
    kSOSNeverAppliedToCircle,  // We've never applied to a circle
    kSOSDiscoveredRetirement,  // We discovered that we were retired.
    kSOSLostPrivateKey,        // We lost our private key
    kSOSPasswordChanged,        // We lost our public key, password change?
                                // <-- add additional departure reason codes HERE!
	kSOSNumDepartureReasons,   // ACHTUNG: this *MUST* be the last entry - ALWAYS!
};

/*!
 @function SOSCCGetLastDepartureReason
 @abstract Returns the code of why you left the circle.
 @param error What went wrong if we returned kSOSDepartureReasonError.
 */
enum DepartureReason SOSCCGetLastDepartureReason(CFErrorRef *error);

/*!
 @function SOSCCSetLastDepartureReason
 @abstract Manually set the code of why the circle was left.
 @param reason Custom departure reason be be set.
 @param error What went wrong if we returned false.
 */

bool SOSCCSetLastDepartureReason(enum DepartureReason reason, CFErrorRef *error);

/*
    Views
    
    Initial View List - To be expanded
 
    For now for any peer joining a circle we only enable:
    kSOSViewKeychainV0
*/

//
// -- Views that sync to os in (iOS in (7.1, 8.*) Mac OS in (10.9, 10.10)) peers
//

// kSOSViewKeychainV0 - All items in the original iCloud Keychain are in the views listed below
// It is defined by the query:
// class in (genp inet keys) and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and tkid = NULL

// kSOSViewWiFi - class = genp and  pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = apple and svce = AirPort
extern const CFStringRef kSOSViewWiFi;

// kSOSViewAutofillPasswords - class = inet and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = com.apple.cfnetwork
extern const CFStringRef kSOSViewAutofillPasswords;

// kSOSViewSafariCreditCards - class = genp and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = com.apple.safari.credit-cards
extern const CFStringRef kSOSViewSafariCreditCards;

// kSOSViewiCloudIdentity - class = keys and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = com.apple.security.sos
extern const CFStringRef kSOSViewiCloudIdentity;
// End of KeychainV0 views

// kSOSViewBackupBagV0 - class = genp and and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = com.apple.sbd
// (LEAVE OUT FOR NOW) and svce = SecureBackupService pdmn = ak acct = SecureBackupPublicKeybag
extern const CFStringRef kSOSViewBackupBagV0;

// kSOSViewOtherSyncable - An or of the following 5 queries:
// class = cert and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL
// class = genp and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp = "apple" and svce != "AirPort"
// class = genp and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp not in ("apple", "com.apple.safari.credit-cards", "com.apple.sbd")
// class = inet and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp not in ("com.apple.cfnetwork")
// class = keys and pdmn in (ak,ck,dk,aku,cku,dku) and vwht = NULL and agrp not in ("com.apple.security.sos")
extern const CFStringRef kSOSViewOtherSyncable;

//
// Views below this line all match a kSecAttrSyncViewHint attribute value that matches their name.
//

// PCS (Protected Cloud Storage) Views
extern const CFStringRef kSOSViewPCSMasterKey;
extern const CFStringRef kSOSViewPCSiCloudDrive;
extern const CFStringRef kSOSViewPCSPhotos;
extern const CFStringRef kSOSViewPCSCloudKit;
extern const CFStringRef kSOSViewPCSEscrow;
extern const CFStringRef kSOSViewPCSFDE;
extern const CFStringRef kSOSViewPCSMailDrop;
extern const CFStringRef kSOSViewPCSiCloudBackup;
extern const CFStringRef kSOSViewPCSNotes;
extern const CFStringRef kSOSViewPCSiMessage;
extern const CFStringRef kSOSViewPCSFeldspar;
extern const CFStringRef kSOSViewPCSSharing;

extern const CFStringRef kSOSViewAppleTV;
extern const CFStringRef kSOSViewHomeKit;
extern const CFStringRef kSOSViewContinuityUnlock;
extern const CFStringRef kSOSViewAccessoryPairing;
extern const CFStringRef kSOSViewNanoRegistry;
extern const CFStringRef kSOSViewWatchMigration;
extern const CFStringRef kCKKSViewEngram;
extern const CFStringRef kCKKSViewManatee;
extern const CFStringRef kCKKSViewAutoUnlock;
extern const CFStringRef kCKKSViewHealth;
extern const CFStringRef kCKKSViewApplePay;
extern const CFStringRef kCKKSViewHome;
extern const CFStringRef kCKKSViewLimitedPeersAllowed;


/*!
 @function SOSCCView
 @abstract Enable, disable or query status of a View for this peer.
 @param view The View for which the action should be performed.
 @param action The action code to take with the View
 @param error More description of the error if one occurred.
 @discussion
    For all actions any error return can fallback to kSOSCCGeneralViewError.  This is a catch-all until
    more code is written and specific additional error returns are identified.
    For kSOSCCViewEnable actions other possible return codes are:
        kSOSCCViewMember if the operation was successful and the peer has access to the View
        kSOSCCViewNotMember if the operation was a successful application to a View, yet the peer must be vetted by another peer.
        kSOSCCViewNotQualified if the device can't support prerequisite security capabilities
        kSOSCCNoSuchView if the CFStringRef doesn't match one of the known Views
    
    For kSOSCCViewDisable actions other possible return codes are:
        kSOSCCViewNotMember for successfully disabling the View
        kSOSCCNoSuchView if the CFStringRef doesn't match one of the known Views

    For kSOSCCViewQuery actions other possible return codes are:
        kSOSCCViewMember or kSOSCCDSNotMember for successful querying of the status for a View for this peer
        kSOSCCNoSuchView if the CFStringRef doesn't match one of the known Views
 
 */

SOSViewResultCode SOSCCView(CFStringRef view, SOSViewActionCode action, CFErrorRef *error);


/*!
 @function SOSCCViewSet
 @abstract Enable, disable or query status of a views for this peer.
 @param enabledviews The views (as CFSet) for which the action should be performed.
 @param disabledviews TODO
 @discussion
   This call enables bulk setting of views for a peer.  This is done for convenience as well as
   better performance; it requires less circle changes by grouping all the view enabling/disabling.
 
 Separate calls to SOSCCView is required to determine resulting view settings.
 */

bool SOSCCViewSet(CFSetRef enabledviews, CFSetRef disabledviews);
/*
 Security Attributes for PeerInfos
 
 Initial View List - To be expanded
 */


//
// Backup APIs
//

/*!
 @function SOSCCCopyMyPeerWithNewDeviceRecoverySecret
 @abstract Returns retained peer info for this device
 @param secret user provided entropy
 @param error What went wrong trying to register the new secret
 @result Returns our peer info.
 @discussion For miCSCs this creates a new wrapping of the view master key in the view bag protected by the secret.
 */
SOSPeerInfoRef SOSCCCopyMyPeerWithNewDeviceRecoverySecret(CFDataRef secret, CFErrorRef *error);

/*!
 @function SOSCopyDeviceBackupPublicKey
 @param entropy user provided entropy
 @param error what, if anything, went wrong creating the backup key
 @result returns the public key bytes which will be eventually generated by a call to SOSCCCopyMyPeerWithNewDeviceRecoverySecret. Note that this function does not change any state.
 */
CFDataRef SOSCopyDeviceBackupPublicKey(CFDataRef entropy, CFErrorRef *error);

/*!
 @function SOSCCRegisterSingleRecoverySecret
 @param aks_bag TODO
 @param error What went wrong trying to register the new secret
 @result true if we saved the bag, false if we had an error
 @discussion Asserts the keybag for use for backups when having a single secret. All views get backed up with this single bag.
 */
bool SOSCCRegisterSingleRecoverySecret(CFDataRef aks_bag, bool forV0Only, CFErrorRef *error);

/*!
 @function SOSCCCopyApplication
 @param error What went wrong getting the applicant peerInfo.
 @result PeerInfoRef that's an applicant peerinfo to be used as the start of an HSA2 piggyback entry.
 */

SOSPeerInfoRef SOSCCCopyApplication(CFErrorRef *error);

/*!
 @function SOSCCCopyCircleJoiningBlob
 @param applicant The peerInfo applicant to pre-approve for membership in the circle
 @param error Why this peerInfo wasn't accepted.
 @result DER blob containing the gencount and this peerkey signature of the current circle with the applicant as a member at the gencount.
 */
CFDataRef SOSCCCopyCircleJoiningBlob(SOSPeerInfoRef applicant, CFErrorRef *error);

/*!
 @function SOSCCJoinWithCircleJoiningBlob
 @param joiningBlob DER blob to be used to create a suitable circle for this pre-approved peer to join.
 @param error Why this peerInfo can't be accepted.
 @result true if this succeeded.
 */

bool SOSCCJoinWithCircleJoiningBlob(CFDataRef joiningBlob, PiggyBackProtocolVersion version, CFErrorRef *error);

/*!
 @function: bool SOSCCPeersHaveViewsEnabled(CFSetRef viewNames)
 @param viewNames the collection of views we want to know if other peers have enabled
 @result CFBooleanTrue if we are in circle and all views are enabled by at least one other peer,
         CFBooleanFalse if we are in circle and any of the views aren't avaialbe
         NULL if we have an error.
 */
CFBooleanRef SOSCCPeersHaveViewsEnabled(CFArrayRef viewNames, CFErrorRef *error);

/*!
 @function: bool SOSCCRegisterRecoveryPublicKey(CFDataRef recovery_key, CFErrorRef *error);
 @param recovery_key the cf data representation of the recovery public key
 Can be passed in as NULL to indicate the CFDataRef should be removed from the keychain
 @result CFBooleanTrue if the public key was successfully stored in the syncable keychain
 CFBooleanFalse if securityd could not store the recovery key (locked?)
 NULL if we have an error.
 */
bool SOSCCRegisterRecoveryPublicKey(CFDataRef recovery_key, CFErrorRef *error);

/*!
 @function: bool SOSCCMessageFromPeerIsPending(SOSPeerInfoRef peer, CFErrorRef *error)
 @param peer PeerInfo for the peer to ask about
 @param error failure if we fail
 @result true if we have a message pending that we haven't processed, false if we don't have one queued right now or an error occurred.
 */
bool SOSCCMessageFromPeerIsPending(SOSPeerInfoRef peer, CFErrorRef *error);

/*!
 @function: bool SOSCCSendToPeerIsPending(SOSPeerInfoRef peer, CFErrorRef *error)
 @param peer PeerInfo for the peer to ask about
 @param error failure if we fail
 @result true if we have an attempt to sync pending that we haven't processed, false if we don't have one queued right now or an error occurred.
 */
bool SOSCCSendToPeerIsPending(SOSPeerInfoRef peer, CFErrorRef *error);

#if __OBJC__
/*
 @function: SOSCCAccountGetPublicKey
 @param reply fetch the current user public key as SubjectPublicKeyInfoi
 */
void SOSCCAccountGetPublicKey(void (^reply)(BOOL trusted, NSData *data, NSError *error));

/*
 @function: SOSCCAccountGetAccountPrivateCredential
 @param reply fetch the current user public key as SubjectPublicKeyInfoi
 */
void SOSCCAccountGetAccountPrivateCredential(void (^complete)(NSData *data, NSError *error));

void SOSCCAccountGetKeyCircleGeneration(void (^reply)(NSData *data, NSError *error));

void SOSCCGhostBust(SOSAccountGhostBustingOptions options, void (^complete)(bool ghostsBusted, NSError *error));

void SOSCCGhostBustTriggerTimed(SOSAccountGhostBustingOptions options, void (^complete)(bool ghostsBusted, NSError *error));

void SOSCCGhostBustInfo(void (^complete)(NSData *json, NSError *error));

CFDataRef SOSCCCopyInitialSyncData(SOSInitialSyncFlags flags, CFErrorRef *error);

NSString * SOSCCCircleHash(NSError **error);

#endif

__END_DECLS

#endif
