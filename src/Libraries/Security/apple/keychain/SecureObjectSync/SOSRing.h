//
//  SOSRing.h
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#ifndef _sec_SOSRing_
#define _sec_SOSRing_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSGenCount.h"
#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include "keychain/SecureObjectSync/SOSConcordanceTrust.h"
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecKey.h>

typedef struct __OpaqueSOSRing *SOSRingRef;

enum {
    kSOSRingMember      = 0,
    kSOSRingNotInRing   = 1,
    kSOSRingApplicant   = 2,
    kSOSRingReject      = 3,
    kSOSRingRetired      = 4,
    kSOSRingError       = 99,
};
typedef int SOSRingStatus;

enum {
    kSOSRingBase = 0,
    kSOSRingBackup = 1,
    kSOSRingPeerKeyed = 2,
    kSOSRingEntropyKeyed = 3,
    kSOSRingPKKeyed = 4,
    kSOSRingRecovery = 5,
    kSOSRingTypeCount = 6,
    kSOSRingTypeError = 0xfbad,
};
typedef uint32_t SOSRingType;

CFTypeID SOSRingGetTypeID(void);

SOSRingRef SOSRingCreate(CFStringRef name, CFStringRef myPeerID, SOSRingType type, CFErrorRef *error);
bool SOSRingResetToEmpty(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error);
bool SOSRingGenerationSign(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingConcordanceSign(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error);
SOSConcordanceStatus SOSRingConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers,
                                             SOSRingRef knownRing, SOSRingRef proposedRing,
                                             SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                             CFStringRef excludePeerID, CFErrorRef *error);
bool SOSRingAccept(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingReject(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingSetPayload(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error);
CFDataRef SOSRingGetPayload(SOSRingRef ring, CFErrorRef *error);
CFSetRef SOSRingGetBackupViewset(SOSRingRef ring, CFErrorRef *error);
CFStringRef SOSRingGetBackupView(SOSRingRef ring, CFErrorRef *error);

bool SOSRingSetBackupKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, CFSetRef viewSet, SOSBackupSliceKeyBagRef bskb, CFErrorRef *error);

SOSBackupSliceKeyBagRef SOSRingCopyBackupSliceKeyBag(SOSRingRef ring, CFErrorRef *error);

bool SOSRingPeerTrusted(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingPKTrusted(SOSRingRef ring, SecKeyRef pubkey, CFErrorRef *error);

CFDataRef SOSRingCopyEncodedData(SOSRingRef ring, CFErrorRef *error);
SOSRingRef SOSRingCreateFromData(CFErrorRef* error, CFDataRef ring_data);

CFStringRef SOSRingGetName(SOSRingRef ring);
uint32_t SOSRingGetType(SOSRingRef ring);
SOSGenCountRef SOSRingGetGeneration(SOSRingRef ring);
uint32_t SOSRingGetVersion(SOSRingRef ring);
CFStringRef SOSRingGetIdentifier(SOSRingRef ring);
CFStringRef SOSRingGetLastModifier(SOSRingRef ring);

CFMutableSetRef SOSRingGetApplicants(SOSRingRef ring);

static inline bool isSOSRing(CFTypeRef object) {
    return object && (CFGetTypeID(object) == SOSRingGetTypeID());
}

bool SOSBackupRingSetViews(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFSetRef viewSet, CFErrorRef *error);
CFSetRef SOSBackupRingGetViews(SOSRingRef ring, CFErrorRef *error);

#endif /* defined(_sec_SOSRing_) */
