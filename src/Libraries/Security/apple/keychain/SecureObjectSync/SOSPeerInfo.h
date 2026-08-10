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


#ifndef _SOSPEERINFO_H_
#define _SOSPEERINFO_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <corecrypto/ccdigest.h>

#include <Security/SecureObjectSync/SOSTypes.h>

__BEGIN_DECLS

typedef struct __OpaqueSOSPeerInfo   *SOSPeerInfoRef;

// Bumped to 3 from 2 so we can identify pre-iCDP peers and add the proper views.
#define PEERINFO_CURRENT_VERSION 3

enum {
    kSOSPeerVersion = 2,
    kSOSPeerV2BaseVersion = 2,
};


enum {
    SOSPeerCmpPubKeyHash = 0,
    SOSPeerCmpName = 1,
};
typedef uint32_t SOSPeerInfoCmpSelect;

CFTypeID SOSPeerInfoGetTypeID(void);

static inline bool isSOSPeerInfo(CFTypeRef obj) {
    return obj && (CFGetTypeID(obj) == SOSPeerInfoGetTypeID());
}

static inline SOSPeerInfoRef asSOSPeerInfo(CFTypeRef obj) {
    return isSOSPeerInfo(obj) ? (SOSPeerInfoRef) obj : NULL;
}

SOSPeerInfoRef SOSPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, CFDataRef backup_key, SecKeyRef signingKey,
                                 SecKeyRef octagonSigningKey, SecKeyRef octagonPeerEncryptionKey, bool supportsCKKS4All,
                                 CFErrorRef* error);

SOSPeerInfoRef SOSPeerInfoCreateWithTransportAndViews(CFAllocatorRef allocator, CFDictionaryRef gestalt, CFDataRef backup_key,
                                                      CFStringRef IDSID, CFStringRef transportType, CFBooleanRef preferIDS,
                                                      CFBooleanRef preferFragmentation, CFBooleanRef preferAckModel, CFSetRef enabledViews, SecKeyRef signingKey,
                                                      SecKeyRef octagonSigningKey, SecKeyRef octagonPeerEncryptionKey, bool supportsCKKS4All,
                                                      CFErrorRef* error);

SOSPeerInfoRef SOSPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error);

SOSPeerInfoRef SOSPeerInfoCreateCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCreateCurrentCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy,
                                            CFStringRef IDSID, CFStringRef transportType, CFBooleanRef preferIDS, CFBooleanRef preferFragmentation, CFBooleanRef preferAckModel, CFSetRef enabledViews,
                                            SecKeyRef signingKey, CFErrorRef* error);
bool SOSPeerInfoVersionIsCurrent(SOSPeerInfoRef pi);
bool SOSPeerInfoVersionHasV2Data(SOSPeerInfoRef pi);
SOSPeerInfoRef SOSPeerInfoCopyWithGestaltUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyWithBackupKeyUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDataRef backupKey, SecKeyRef signingKey, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyWithReplacedEscrowRecords(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef escrowRecords, SecKeyRef signingKey, CFErrorRef *error);


SOSPeerInfoRef SOSPeerInfoCopyWithViewsChange(CFAllocatorRef allocator, SOSPeerInfoRef toCopy,
                                              SOSViewActionCode action, CFStringRef viewname, SOSViewResultCode *retval,
                                              SecKeyRef signingKey, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyAsApplication(SOSPeerInfoRef pi, SecKeyRef userkey, SecKeyRef peerkey, CFErrorRef *error);

SOSPeerInfoRef SOSPeerInfoCopyWithPing(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, SecKeyRef signingKey, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyAsApplication(SOSPeerInfoRef pi, SecKeyRef userkey, SecKeyRef peerkey, CFErrorRef *error);

bool SOSPeerInfoUpdateDigestWithPublicKeyBytes(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                               ccdigest_ctx_t ctx, CFErrorRef *error);
bool SOSPeerInfoUpdateDigestWithDescription(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                            ccdigest_ctx_t ctx, CFErrorRef *error);

bool SOSPeerInfoApplicationVerify(SOSPeerInfoRef pi, SecKeyRef userkey, CFErrorRef *error);

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetApplicationDate(SOSPeerInfoRef pi);


//
// Transfered Data
//
bool SOSPeerInfoHasBackupKey(SOSPeerInfoRef peer);
CFDataRef SOSPeerInfoCopyBackupKey(SOSPeerInfoRef peer);

//
// DER Import Export
//
SOSPeerInfoRef SOSPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end);

SOSPeerInfoRef SOSPeerInfoCreateFromData(CFAllocatorRef allocator, CFErrorRef* error,
                                         CFDataRef peerinfo_data);

size_t      SOSPeerInfoGetDEREncodedSize(SOSPeerInfoRef peer, CFErrorRef *error);
uint8_t*    SOSPeerInfoEncodeToDER(SOSPeerInfoRef peer, CFErrorRef* error,
                                   const uint8_t* der, uint8_t* der_end);

CFDataRef SOSPeerInfoCopyEncodedData(SOSPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error);

//
// Gestalt info about the peer. It was fetched by the implementation on the other side.
// probably has what you're looking for..
//
CFTypeRef SOSPeerInfoLookupGestaltValue(SOSPeerInfoRef pi, CFStringRef key);
CFDictionaryRef SOSPeerInfoCopyPeerGestalt(SOSPeerInfoRef pi);
CFDictionaryRef SOSPeerGetGestalt(SOSPeerInfoRef pi);
CFStringRef SOSPeerInfoGetPeerName(SOSPeerInfoRef peer);

//
// Syntactic Sugar for some commone ones, might get deprectated at this level.
//

CFStringRef SOSPeerInfoGetPeerDeviceType(SOSPeerInfoRef peer);
CFIndex SOSPeerInfoGetPeerProtocolVersion(SOSPeerInfoRef peer);


// Stringified ID for this peer, not human readable.
CFStringRef SOSPeerInfoGetPeerID(SOSPeerInfoRef peer);
CFStringRef SOSPeerInfoGetSPID(SOSPeerInfoRef pi);

bool SOSPeerInfoPeerIDEqual(SOSPeerInfoRef pi, CFStringRef myPeerID);

CFIndex SOSPeerInfoGetVersion(SOSPeerInfoRef peer);

//
// Peer Info Gestalt Helpers
//
CFStringRef SOSPeerGestaltGetName(CFDictionaryRef gestalt);

// These are Mobile Gestalt questions. Not all Gestalt questions are carried.
CFTypeRef SOSPeerGestaltGetAnswer(CFDictionaryRef gestalt, CFStringRef question);

SecKeyRef SOSPeerInfoCopyPubKey(SOSPeerInfoRef peer, CFErrorRef *error);
SecKeyRef SOSPeerInfoCopyOctagonSigningPublicKey(SOSPeerInfoRef peer, CFErrorRef* error);
SecKeyRef SOSPeerInfoCopyOctagonEncryptionPublicKey(SOSPeerInfoRef peer, CFErrorRef* error);
bool SOSPeerInfoSetOctagonKeysInDescription(SOSPeerInfoRef peer,  SecKeyRef octagonSigningKey,
                                            SecKeyRef octagonEncryptionKey, CFErrorRef *error);
CFDataRef SOSPeerInfoGetAutoAcceptInfo(SOSPeerInfoRef peer);

CFComparisonResult SOSPeerInfoCompareByID(const void *val1, const void *val2, void *context);
CFComparisonResult SOSPeerInfoCompareByApplicationDate(const void *val1, const void *val2, void *context);

SOSPeerInfoRef SOSPeerInfoCreateRetirementTicket(CFAllocatorRef allocator, SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error);

CFStringRef SOSPeerInfoInspectRetirementTicket(SOSPeerInfoRef pi, CFErrorRef *error);

bool SOSPeerInfoRetireRetirementTicket(size_t max_days, SOSPeerInfoRef pi);

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetRetirementDate(SOSPeerInfoRef pi);

bool SOSPeerInfoIsRetirementTicket(SOSPeerInfoRef pi);

bool SOSPeerInfoIsCloudIdentity(SOSPeerInfoRef pi);

CF_RETURNS_RETAINED SOSPeerInfoRef SOSPeerInfoUpgradeSignatures(CFAllocatorRef allocator, SecKeyRef privKey, SecKeyRef perKey, SOSPeerInfoRef peer, CFErrorRef *error);

SOSViewResultCode SOSPeerInfoViewStatus(SOSPeerInfoRef pi, CFStringRef view, CFErrorRef *error);

CFSetRef SOSPeerInfoGetPermittedViews(SOSPeerInfoRef peer);
bool SOSPeerInfoIsEnabledView(SOSPeerInfoRef peer, CFStringRef viewName);
CFMutableSetRef SOSPeerInfoCopyEnabledViews(SOSPeerInfoRef peer);
void SOSPeerInfoWithEnabledViewSet(SOSPeerInfoRef pi, void (^operation)(CFSetRef enabled));
uint64_t SOSViewBitmaskFromSet(CFSetRef views);
uint64_t SOSPeerInfoViewBitMask(SOSPeerInfoRef pi);

bool SOSPeerInfoSupportsCKKSForAll(SOSPeerInfoRef peerInfo);
void SOSPeerInfoSetSupportsCKKSForAll(SOSPeerInfoRef peerInfo, bool supports);

bool SOSPeerInfoKVSOnly(SOSPeerInfoRef pi);
CFStringRef SOSPeerInfoCopyTransportType(SOSPeerInfoRef peer);
CFStringRef SOSPeerInfoCopyDeviceID(SOSPeerInfoRef peer);

/* octagon keys */
SOSPeerInfoRef CF_RETURNS_RETAINED
SOSPeerInfoSetOctagonSigningKey(CFAllocatorRef allocator,
                                SOSPeerInfoRef toCopy,
                                SecKeyRef octagonSigningKey,
                                SecKeyRef signingKey,
                                CFErrorRef *error);

SOSPeerInfoRef CF_RETURNS_RETAINED
SOSPeerInfoSetOctagonEncryptionKey(CFAllocatorRef allocator,
                                 SOSPeerInfoRef toCopy,
                                 SecKeyRef octagonEncryptionKey,
                                 SecKeyRef signingKey,
                                 CFErrorRef *error);

SOSPeerInfoRef CF_RETURNS_RETAINED
SOSPeerInfoSetOctagonKeys(CFAllocatorRef allocator,
                          SOSPeerInfoRef toCopy,
                          SecKeyRef octagonSigningKey,
                          SecKeyRef octagonEncryptionKey,
                          SecKeyRef signingKey,
                          CFErrorRef *error);

CFStringRef SOSPeerInfoCopySerialNumber(SOSPeerInfoRef pi);

void SOSPeerInfoLogState(char *category, SOSPeerInfoRef pi, SecKeyRef pubKey, CFStringRef myPID, char sigchr);

enum {
    SOSPeerInfo_unknown = 0,
    SOSPeerInfo_iCloud = 1,
    SOSPeerInfo_iOS = 2,
    SOSPeerInfo_macOS = 3,
    SOSPeerInfo_watchOS = 4,
    SOSPeerInfo_tvOS = 5,
};
typedef uint32_t SOSPeerInfoDeviceClass;

SOSPeerInfoDeviceClass SOSPeerInfoGetClass(SOSPeerInfoRef pi);

bool SOSPeerInfoSign(SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error);

__END_DECLS

#endif
