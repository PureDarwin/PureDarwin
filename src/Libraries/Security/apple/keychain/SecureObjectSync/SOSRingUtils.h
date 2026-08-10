//
//  SOSRingUtils.h
//  sec
//
//  Created by Richard Murphy on 1/28/15.
//
//

#ifndef _sec_SOSRingUtils_
#define _sec_SOSRingUtils_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFWrappers.h>
#include "keychain/SecureObjectSync/SOSGenCount.h"
#include "SOSRing.h"

#define ALLOCATOR NULL


struct __OpaqueSOSRing {
    CFRuntimeBase _base;
    CFMutableDictionaryRef unSignedInformation;
    CFMutableDictionaryRef signedInformation;
    CFMutableDictionaryRef signatures;      // Signatures keyed by peerid
    CFMutableDictionaryRef data;            // Anything for ring-specific rule support
};

static inline
void SOSRingAssertStable(SOSRingRef ring)
{
    assert(ring);
    assert(ring->unSignedInformation);
    assert(ring->signedInformation);
    assert(ring->signatures);
    assert(ring->data);
}

static inline
bool SOSRingIsStable(SOSRingRef ring) {
    return (ring) && (ring->unSignedInformation) && (ring->signedInformation) && (ring->signatures)&& (ring->data);
}

/* unSignedInformation Dictionary Keys */
extern CFStringRef sApplicantsKey;
extern CFStringRef sRejectionsKey;
extern CFStringRef sRetiredKey;
extern CFStringRef sLastPeerToModifyKey;

/* signedInformation Dictionary Keys */
extern CFStringRef sNameKey;
extern CFStringRef sVersion;
extern CFStringRef sTypeKey;
extern CFStringRef sIdentifierKey;
extern CFStringRef sGenerationKey;
extern CFStringRef sPeerIDsKey;
extern CFStringRef sRingVersionKey;

CF_RETURNS_RETAINED SOSRingRef SOSRingAllocate(void);
SOSRingRef SOSRingCreate_Internal(CFStringRef name, SOSRingType type, CFErrorRef *error);
SOSRingRef SOSRingCopyRing(SOSRingRef original, CFErrorRef *error);

bool SOSRingRemoveSignatures(SOSRingRef ring, CFErrorRef *error);
bool SOSRingVerifySignatureExists(SOSRingRef ring, SecKeyRef pubKey, CFErrorRef *error);
bool SOSRingVerify(SOSRingRef ring, SecKeyRef pubKey, CFErrorRef *error);
bool SOSRingVerifyPeerSigned(SOSRingRef ring, SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSRingGenerationSign_Internal(SOSRingRef ring, SecKeyRef privKey, CFErrorRef *error);
bool SOSRingConcordanceSign_Internal(SOSRingRef ring, SecKeyRef privKey, CFErrorRef *error);
SOSConcordanceStatus GetSignersStatus(CFSetRef peers, SOSRingRef signersRing, SOSRingRef statusRing,
                                      SecKeyRef userPubkey, CFStringRef excludePeerID, CFErrorRef *error);
SOSConcordanceStatus GetSignersStatus_Transitive(CFSetRef peers, SOSRingRef signersRing, SOSRingRef statusRing,
                                                 SecKeyRef userPubkey, CFStringRef excludePeerID, CFErrorRef *error);
SOSConcordanceStatus SOSRingUserKeyConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers, SOSRingRef knownRing, SOSRingRef proposedRing,
                                                    SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                    CFStringRef excludePeerID, CFErrorRef *error);
SOSConcordanceStatus SOSRingPeerKeyConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers, SOSRingRef knownRing, SOSRingRef proposedRing,
                                                    __unused SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                    CFStringRef excludePeerID, CFErrorRef *error);

bool SOSRingHasPeerWithID(SOSRingRef ring, CFStringRef peerid, CFErrorRef *error);

int SOSRingCountPeers(SOSRingRef ring);
CFStringRef SOSRingCopySignerList(SOSRingRef ring);
CFDictionaryRef SOSRingCopyPeerIDList(SOSRingRef ring);


int SOSRingCountApplicants(SOSRingRef ring);
bool SOSRingHasApplicant(SOSRingRef ring, CFStringRef peerID);
CFMutableSetRef SOSRingCopyApplicants(SOSRingRef ring);

int SOSRingCountRejections(SOSRingRef ring);
bool SOSRingHasRejection(SOSRingRef ring, CFStringRef peerID);
CFMutableSetRef SOSRingCopyRejections(SOSRingRef ring);
bool SOSRingHasPeerWithID(SOSRingRef ring, CFStringRef peerid, CFErrorRef *error);

// Use this to determine whether a ring your interogating is the "same one" that you think you're going to change.
bool SOSRingIsSame(SOSRingRef ring1, SOSRingRef ring2);

const char *SOSRingGetNameC(SOSRingRef ring);

void SOSRingGenerationIncrement(SOSRingRef ring);
bool SOSRingIsOlderGeneration(SOSRingRef olderRing, SOSRingRef newerRing);
void SOSRingGenerationCreateWithBaseline(SOSRingRef newring, SOSRingRef baseline);

bool SOSRingSetApplicants(SOSRingRef ring, CFMutableSetRef applicants);

bool SOSRingSetLastModifier(SOSRingRef ring, CFStringRef peerID);

bool SOSRingResetToEmpty_Internal(SOSRingRef ring, CFErrorRef *error);
bool SOSRingIsEmpty_Internal(SOSRingRef ring);
bool SOSRingIsOffering_Internal(SOSRingRef ring);


bool SOSRingAddApplicant(SOSRingRef ring, CFStringRef peerid);
bool SOSRingRemoveApplicant(SOSRingRef ring, CFStringRef peerid);

bool SOSRingAddRejection(SOSRingRef ring, CFStringRef peerid);
bool SOSRingRemoveRejection(SOSRingRef ring, CFStringRef peerid);
CFDataRef SOSRingGetPayload_Internal(SOSRingRef ring);
bool SOSRingSetPayload_Internal(SOSRingRef ring, CFDataRef payload);
CFSetRef SOSRingGetBackupViewset_Internal(SOSRingRef ring);
bool SOSRingSetBackupViewset_Internal(SOSRingRef ring, CFSetRef viewSet);
bool SOSRingSetPeerIDs(SOSRingRef ring, CFMutableSetRef peers);
int SOSRingCountPeerIDs(SOSRingRef ring);
bool SOSRingHasPeerID(SOSRingRef ring, CFStringRef peerID);
CFMutableSetRef SOSRingCopyPeerIDs(SOSRingRef ring);
void SOSRingAddAll(SOSRingRef ring, CFSetRef peerInfosOrIDs);
bool SOSRingAddPeerID(SOSRingRef ring, CFStringRef peerid);
bool SOSRingRemovePeerID(SOSRingRef ring, CFStringRef peerid);
void SOSRingForEachPeerID(SOSRingRef ring, void (^action)(CFStringRef peerID));

size_t SOSRingGetDEREncodedSize(SOSRingRef ring, CFErrorRef *error);
uint8_t* SOSRingEncodeToDER(SOSRingRef ring, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
SOSRingRef SOSRingCreateFromDER(CFErrorRef* error, const uint8_t** der_p, const uint8_t *der_end);

CFDictionaryRef SOSRingCreateRetirementTicket(SOSFullPeerInfoRef fpi, CFErrorRef *error);

#endif /* defined(_sec_SOSRingUtils_) */
