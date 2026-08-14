/*
 * Copyright (c) 2003-2018 Apple Inc. All Rights Reserved.
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
 *
 * OTATrustUtilities.h
 */

#ifndef _OTATRUSTUTILITIES_H_
#define _OTATRUSTUTILITIES_H_  1

#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <stdio.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

// Opaque type that holds the data for a specific version of the OTA PKI assets
typedef struct _OpaqueSecOTAPKI *SecOTAPKIRef;

// Returns the trust server workloop
dispatch_queue_t SecTrustServerGetWorkloop(void);

// Convert a trusted CT log array to a trusted CT log dictionary, indexed by the LogID
CF_RETURNS_RETAINED
CFDictionaryRef SecOTAPKICreateTrustedCTLogsDictionaryFromArray(CFArrayRef trustedCTLogsArray);

// Get a reference to the current OTA PKI asset data
// Caller is responsible for releasing the returned SecOTAPKIRef
CF_EXPORT CF_RETURNS_RETAINED
SecOTAPKIRef SecOTAPKICopyCurrentOTAPKIRef(void);

// Accessor to retrieve a copy of the current black listed key.
// Caller is responsible for releasing the returned CFSetRef
CF_EXPORT
CFSetRef SecOTAPKICopyBlackListSet(SecOTAPKIRef otapkiRef);

// Accessor to retrieve a copy of the current gray listed key.
// Caller is responsible for releasing the returned CFSetRef
CF_EXPORT
CFSetRef SecOTAPKICopyGrayList(SecOTAPKIRef otapkiRef);

// Accessor to retrieve a copy of the current allow list dictionary.
// Caller is responsible for releasing the returned CFDictionaryRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyAllowList(SecOTAPKIRef otapkiRef);

// Accessor to retrieve a copy of the allow list for a specific authority key ID.
// Caller is responsible for releasing the returned CFArrayRef
CF_EXPORT
CFArrayRef SecOTAPKICopyAllowListForAuthKeyID(SecOTAPKIRef otapkiRef, CFStringRef authKeyID);

// Accessor to retrieve a copy of the current trusted certificate transparency logs.
// Caller is responsible for releasing the returned CFArrayRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyTrustedCTLogs(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the path of the current pinning list.
// Caller is responsible for releasing the returned CFURLRef
CF_EXPORT
CFURLRef SecOTAPKICopyPinningList(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the array of Escrow certificates.
// Caller is responsible for releasing the returned CFArrayRef
CF_EXPORT
CFArrayRef SecOTAPKICopyEscrowCertificates(uint32_t escrowRootType, SecOTAPKIRef otapkiRef);

// Accessor to retrieve the dictionary of EV Policy OIDs to Anchor digest.
// Caller is responsible for releasing the returned CFDictionaryRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyEVPolicyToAnchorMapping(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the dictionary of anchor digest to file offset.
// Caller is responsible for releasing the returned CFDictionaryRef
CF_EXPORT
CFDictionaryRef SecOTAPKICopyAnchorLookupTable(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the pointer to the top of the anchor certs file.
// Caller should NOT free the returned pointer.  The caller should hold
// a reference to the SecOTAPKIRef object until finished with
// the returned pointer.
CF_EXPORT
const char* SecOTAPKIGetAnchorTable(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the full path to the valid update snapshot resource.
// The return value may be NULL if the resource does not exist.
// Caller should NOT free the returned pointer.  The caller should hold
// a reference to the SecOTAPKIRef object until finished with
// the returned pointer.
CF_EXPORT
const char* SecOTAPKIGetValidUpdateSnapshot(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the full path to the valid database snapshot resource.
// The return value may be NULL if the resource does not exist.
// Caller should NOT free the returned pointer.  The caller should hold
// a reference to the SecOTAPKIRef object until finished with
// the returned pointer.
CF_EXPORT
const char* SecOTAPKIGetValidDatabaseSnapshot(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the current valid snapshot version.
CF_EXPORT
CFIndex SecOTAPKIGetValidSnapshotVersion(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the current valid snapshot format.
CF_EXPORT
CFIndex SecOTAPKIGetValidSnapshotFormat(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the OTAPKI trust store version
// Note: Trust store is not mutable by assets
CF_EXPORT
uint64_t SecOTAPKIGetTrustStoreVersion(SecOTAPKIRef otapkiRef);

// Accessor to retrieve the OTAPKI asset version
CF_EXPORT
uint64_t SecOTAPKIGetAssetVersion(SecOTAPKIRef otapkiRef);

// Accessors to retrieve the last check in time for the OTAPKI asset
CF_EXPORT
CFDateRef SecOTAPKICopyLastAssetCheckInDate(SecOTAPKIRef otapkiRef);

#define kSecOTAPKIAssetStalenessAtRisk (60*60*24*30) // 30 days
#define kSecOTAPKIAssetStalenessWarning (60*60*24*45) // 45 days
#define kSecOTAPKIAssetStalenessDisable (60*60*24*60) // 60 days
bool SecOTAPKIAssetStalenessLessThanSeconds(SecOTAPKIRef otapkiRef, CFTimeInterval seconds);

#if __OBJC__
// SPI to return the current sampling rate for the event name
// This rate is actually n where we sample 1 out of every n
NSNumber *SecOTAPKIGetSamplingRateForEvent(SecOTAPKIRef otapkiRef, NSString *eventName);
#endif // __OBJC__

CFArrayRef SecOTAPKICopyAppleCertificateAuthorities(SecOTAPKIRef otapkiRef);

extern const CFStringRef kOTAPKIKillSwitchCT;
extern const CFStringRef kOTAPKIKillSwitchNonTLSCT;
bool SecOTAPKIKillSwitchEnabled(SecOTAPKIRef otapkiRef, CFStringRef switchKey);

// SPI to return the array of currently trusted Escrow certificates
CF_EXPORT
CFArrayRef SecOTAPKICopyCurrentEscrowCertificates(uint32_t escrowRootType, CFErrorRef* error);

// SPI to return the array of currently (TLS) trusted CT logs
CF_EXPORT
CFDictionaryRef SecOTAPKICopyCurrentTrustedCTLogs(CFErrorRef* error);

// SPI to return the array of currently non-TLS trusted CT logs
CF_EXPORT
CFDictionaryRef SecOTAPKICopyNonTlsTrustedCTLogs(SecOTAPKIRef otapkiRef);

// SPI to return dictionary of CT log matching specified key id */
CF_EXPORT
CFDictionaryRef SecOTAPKICopyCTLogForKeyID(CFDataRef keyID, CFErrorRef* error);

// SPI to return the current OTA PKI trust store version
// Note: Trust store is not mutable by assets
CF_EXPORT
uint64_t SecOTAPKIGetCurrentTrustStoreVersion(CFErrorRef* CF_RETURNS_RETAINED  error);

// SPI to return the current OTA PKI asset version
CF_EXPORT
uint64_t SecOTAPKIGetCurrentAssetVersion(CFErrorRef* error);

// SPI to return the current OTA SecExperiment asset version
CF_EXPORT
uint64_t SecOTASecExperimentGetCurrentAssetVersion(CFErrorRef* error);

// SPI to reset the current OTA PKI asset version to the version shipped
// with the system
CF_EXPORT
uint64_t SecOTAPKIResetCurrentAssetVersion(CFErrorRef* CF_RETURNS_RETAINED  error);

// SPI to signal trustd to get a new set of trust data
// Always returns the current asset version. Returns an error with
// a reason if the update was not successful.
CF_EXPORT
uint64_t SecOTAPKISignalNewAsset(CFErrorRef* CF_RETURNS_RETAINED error);

// SPI to signal trustd to get a new set of SecExperiment data
// Always returns the current asset version. Returns an error with
// a reason if the update was not successful.
CF_EXPORT
uint64_t SecOTASecExperimentGetNewAsset(CFErrorRef* error);

// SPI to copy current SecExperiment asset data
CF_EXPORT
CFDictionaryRef SecOTASecExperimentCopyAsset(CFErrorRef* error);

/* "Internal" interfaces for tests */
#if !TARGET_OS_BRIDGE && __OBJC__
BOOL UpdateOTACheckInDate(void);
void UpdateKillSwitch(NSString *key, bool value);
#endif

__END_DECLS

#endif /* _OTATRUSTUTILITIES_H_ */
