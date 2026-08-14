/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>

#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/oidsalg.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecureTransportPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecPolicyPriv.h>

#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecOCSPResponse.h"
#include "trust/trustd/OTATrustUtilities.h"
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/CertificateTransparency.h"

const CFStringRef kSecCTRetirementDateKey = CFSTR("expiry"); // For backwards compatibility, retirement date is represented with the "expiry" key
const CFStringRef kSecCTReadOnlyDateKey = CFSTR("frozen"); // For backwards compatibility, read-only date is represented with the "frozen" key
const CFStringRef kSecCTShardStartDateKey = CFSTR("start_inclusive");
const CFStringRef kSecCTShardEndDateKey = CFSTR("end_exclusive");
const CFStringRef kSecCTPublicKeyKey = CFSTR("key");

enum {
    kSecCTEntryTypeCert = 0,
    kSecCTEntryTypePreCert = 1,
};

/***

struct {
    Version sct_version;        // 1 byte
    LogID id;                   // 32 bytes
    uint64 timestamp;           // 8 bytes
    CtExtensions extensions;    // 2 bytes len field, + n bytes data
    digitally-signed struct {   // 1 byte hash alg, 1 byte sig alg, n bytes signature
        Version sct_version;
        SignatureType signature_type = certificate_timestamp;
        uint64 timestamp;
        LogEntryType entry_type;
        select(entry_type) {
        case x509_entry: ASN.1Cert;
        case precert_entry: PreCert;
        } signed_entry;
        CtExtensions extensions;
    };
} SignedCertificateTimestamp;

***/

static const
SecAsn1Oid *oidForSigAlg(SSL_HashAlgorithm hash, SSL_SignatureAlgorithm alg)
{
    switch(alg) {
        case SSL_SignatureAlgorithmRSA:
            switch (hash) {
                case SSL_HashAlgorithmSHA1:
                    return &CSSMOID_SHA1WithRSA;
                case SSL_HashAlgorithmSHA256:
                    return &CSSMOID_SHA256WithRSA;
                case SSL_HashAlgorithmSHA384:
                    return &CSSMOID_SHA384WithRSA;
                default:
                    break;
            }
        case SSL_SignatureAlgorithmECDSA:
            switch (hash) {
                case SSL_HashAlgorithmSHA1:
                    return &CSSMOID_ECDSA_WithSHA1;
                case SSL_HashAlgorithmSHA256:
                    return &CSSMOID_ECDSA_WithSHA256;
                case SSL_HashAlgorithmSHA384:
                    return &CSSMOID_ECDSA_WithSHA384;
                default:
                    break;
            }
        default:
            break;
    }

    return NULL;
}


static size_t SSLDecodeUint16(const uint8_t *p)
{
    return (p[0]<<8 | p[1]);
}

static uint8_t *SSLEncodeUint16(uint8_t *p, size_t len)
{
    p[0] = (len >> 8)&0xff;
    p[1] = (len & 0xff);
    return p+2;
}

static uint8_t *SSLEncodeUint24(uint8_t *p, size_t len)
{
    p[0] = (len >> 16)&0xff;
    p[1] = (len >> 8)&0xff;
    p[2] = (len & 0xff);
    return p+3;
}


static
uint64_t SSLDecodeUint64(const uint8_t *p)
{
    uint64_t u = 0;
    for(int i=0; i<8; i++) {
        u=(u<<8)|p[0];
        p++;
    }
    return u;
}


static CFDataRef copy_x509_entry_from_chain(SecPVCRef pvc)
{
    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 3+SecCertificateGetLength(leafCert));

    CFDataSetLength(data, 3+SecCertificateGetLength(leafCert));

    uint8_t *q = CFDataGetMutableBytePtr(data);
    q = SSLEncodeUint24(q, SecCertificateGetLength(leafCert));
    memcpy(q, SecCertificateGetBytePtr(leafCert), SecCertificateGetLength(leafCert));

    return data;
}


static CFDataRef copy_precert_entry_from_chain(SecPVCRef pvc)
{
    SecCertificateRef leafCert = NULL;
    SecCertificateRef issuer = NULL;
    CFDataRef issuerKeyHash = NULL;
    CFDataRef tbs_precert = NULL;
    CFMutableDataRef data= NULL;

    require_quiet(SecPVCGetCertificateCount(pvc)>=2, out); //we need the issuer key for precerts.
    leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    issuer = SecPVCGetCertificateAtIndex(pvc, 1);

    require(leafCert, out);
    require(issuer, out); // Those two would likely indicate an internal error, since we already checked the chain length above.
    issuerKeyHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(issuer);
    tbs_precert = SecCertificateCopyPrecertTBS(leafCert);

    require(issuerKeyHash, out);
    require(tbs_precert, out);
    data = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(issuerKeyHash) + 3 + CFDataGetLength(tbs_precert));
    CFDataSetLength(data, CFDataGetLength(issuerKeyHash) + 3 + CFDataGetLength(tbs_precert));

    uint8_t *q = CFDataGetMutableBytePtr(data);
    memcpy(q, CFDataGetBytePtr(issuerKeyHash), CFDataGetLength(issuerKeyHash)); q += CFDataGetLength(issuerKeyHash); // issuer key hash
    q = SSLEncodeUint24(q, CFDataGetLength(tbs_precert));
    memcpy(q, CFDataGetBytePtr(tbs_precert), CFDataGetLength(tbs_precert));

out:
    CFReleaseSafe(issuerKeyHash);
    CFReleaseSafe(tbs_precert);
    return data;
}

static
CFAbsoluteTime TimestampToCFAbsoluteTime(uint64_t ts)
{
    return (ts / 1000) - kCFAbsoluteTimeIntervalSince1970;
}

static
uint64_t TimestampFromCFAbsoluteTime(CFAbsoluteTime at)
{
    return (uint64_t)(at + kCFAbsoluteTimeIntervalSince1970) * 1000;
}

static bool isSCTValidForLogData(CFDictionaryRef logData, int entry_type, CFAbsoluteTime sct_time, CFAbsoluteTime cert_expiry_date) {
    /* only embedded SCTs can be used from retired logs. */
    if(entry_type==kSecCTEntryTypeCert && CFDictionaryContainsKey(logData, kSecCTRetirementDateKey)) {
        return false;
    }

    /* SCTs from after the transition to read-only are not valid (and indicate a operator failure) */
    CFDateRef frozen_date = CFDictionaryGetValue(logData, kSecCTReadOnlyDateKey);
    if (frozen_date && (sct_time > CFDateGetAbsoluteTime(frozen_date))) {
        secerror("Frozen CT log issued SCT after freezing (log=%@)\n", logData);
        return false;
    }

    /* If the log is temporally sharded, the certificate expiry date must be within the temporal shard window */
    CFDateRef start_inclusive = CFDictionaryGetValue(logData, kSecCTShardStartDateKey);
    CFDateRef end_exclusive = CFDictionaryGetValue(logData, kSecCTShardEndDateKey);
    if (start_inclusive && (cert_expiry_date < CFDateGetAbsoluteTime(start_inclusive))) {
        return false;
    }
    if (end_exclusive && (cert_expiry_date >= CFDateGetAbsoluteTime(end_exclusive))) {
        return false;
    }

    return true;
}


/*
   If the 'sct' is valid, add it to the validatingLogs dictionary.

   Inputs:
    - validatingLogs: mutable dictionary to which to add the log that validate this SCT.
    - sct: the SCT date
    - entry_type: 0 for x509 cert, 1 for precert.
    - entry: the cert or precert data.
    - vt: verification time timestamp (as used in SCTs: ms since 1970 Epoch)
    - trustedLog: Dictionary contain the Trusted Logs.

   The SCT is valid if:
    - It decodes properly.
    - Its timestamp is less than 'verifyTime'.
    - It is signed by a log in 'trustedLogs'.
    - If entry_type = 0, the log must be currently qualified.
    - If entry_type = 1, the log may be expired.

   If the SCT is valid, it's added to the validatinLogs dictionary using the log dictionary as the key, and the timestamp as value.
   If an entry for the same log already existing in the dictionary, the entry is replaced only if the timestamp of this SCT is earlier.

 */
static CFDictionaryRef getSCTValidatingLog(CFDataRef sct, int entry_type, CFDataRef entry, uint64_t vt, CFAbsoluteTime cert_expiry_date, CFDictionaryRef trustedLogs, CFAbsoluteTime *sct_at)
{
    uint8_t version;
    const uint8_t *logID;
    const uint8_t *timestampData;
    uint64_t timestamp;
    size_t extensionsLen;
    const uint8_t *extensionsData;
    uint8_t hashAlg;
    uint8_t sigAlg;
    size_t signatureLen;
    const uint8_t *signatureData;
    SecKeyRef pubKey = NULL;
    uint8_t *signed_data = NULL;
    const SecAsn1Oid *oid = NULL;
    SecAsn1AlgId algId;
    CFDataRef logIDData = NULL;
    CFDictionaryRef result = 0;

    const uint8_t *p = CFDataGetBytePtr(sct);
    size_t len = CFDataGetLength(sct);

    require(len>=43, out);

    version = p[0]; p++; len--;
    logID = p; p+=32; len-=32;
    timestampData = p; p+=8; len-=8;
    extensionsLen = SSLDecodeUint16(p); p+=2; len-=2;

    require(len>=extensionsLen, out);
    extensionsData = p; p+=extensionsLen; len-=extensionsLen;

    require(len>=4, out);
    hashAlg=p[0]; p++; len--;
    sigAlg=p[0]; p++; len--;
    signatureLen = SSLDecodeUint16(p); p+=2; len-=2;
    require(len==signatureLen, out); /* We do not tolerate any extra data after the signature */
    signatureData = p;

    /* verify version: only v1(0) is supported */
    if(version!=0) {
        secerror("SCT version unsupported: %d\n", version);
        goto out;
    }

    /* verify timestamp not in the future */
    timestamp = SSLDecodeUint64(timestampData);
    if(timestamp > vt) {
        secerror("SCT is in the future: %llu > %llu\n", timestamp, vt);
        goto out;
    }

    uint8_t *q;

    /* signed entry */
    size_t signed_data_len = 12 + CFDataGetLength(entry) + 2 + extensionsLen ;
    signed_data = malloc(signed_data_len);
    require(signed_data, out);
    q = signed_data;
    *q++ = version;
    *q++ = 0; // certificate_timestamp
    memcpy(q, timestampData, 8); q+=8;
    q = SSLEncodeUint16(q, entry_type); // logentry type: 0=cert 1=precert
    memcpy(q, CFDataGetBytePtr(entry), CFDataGetLength(entry)); q += CFDataGetLength(entry);
    q = SSLEncodeUint16(q, extensionsLen);
    memcpy(q, extensionsData, extensionsLen);

    logIDData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, logID, 32, kCFAllocatorNull);

    CFDictionaryRef logData = CFDictionaryGetValue(trustedLogs, logIDData);
    CFAbsoluteTime sct_time = TimestampToCFAbsoluteTime(timestamp);
    require(logData && isSCTValidForLogData(logData, entry_type, sct_time, cert_expiry_date), out);

    CFDataRef logKeyData = CFDictionaryGetValue(logData, kSecCTPublicKeyKey);
    require(logKeyData, out); // This failing would be an internal logic error
    pubKey = SecKeyCreateFromSubjectPublicKeyInfoData(kCFAllocatorDefault, logKeyData);
    require(pubKey, out);

    oid = oidForSigAlg(hashAlg, sigAlg);
    require(oid, out);

    algId.algorithm = *oid;
    algId.parameters.Data = NULL;
    algId.parameters.Length = 0;

    if(SecKeyDigestAndVerify(pubKey, &algId, signed_data, signed_data_len, signatureData, signatureLen)==0) {
        *sct_at = sct_time;
        result = logData;
    } else {
        secerror("SCT signature failed (log=%@)\n", logData);
    }

out:
    CFReleaseSafe(logIDData);
    CFReleaseSafe(pubKey);
    free(signed_data);
    return result;
}


static void addValidatingLog(CFMutableDictionaryRef validatingLogs, CFDictionaryRef log, CFAbsoluteTime sct_at)
{
    CFDateRef validated_time = CFDictionaryGetValue(validatingLogs, log);

    if(validated_time==NULL || (sct_at < CFDateGetAbsoluteTime(validated_time))) {
        CFDateRef sct_time = CFDateCreate(kCFAllocatorDefault, sct_at);
        CFDictionarySetValue(validatingLogs, log, sct_time);
        CFReleaseSafe(sct_time);
    }
}

static CFArrayRef copy_ocsp_scts(SecPVCRef pvc)
{
    CFMutableArrayRef SCTs = NULL;
    SecCertificateRef leafCert = NULL;
    SecCertificateRef issuer = NULL;
    CFArrayRef ocspResponsesData = NULL;
    SecOCSPRequestRef ocspRequest = NULL;

    ocspResponsesData = SecPathBuilderCopyOCSPResponses(pvc->builder);
    require_quiet(ocspResponsesData, out);

    require_quiet(SecPVCGetCertificateCount(pvc)>=2, out); //we need the issuer key for precerts.
    leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    issuer = SecPVCGetCertificateAtIndex(pvc, 1);

    require(leafCert, out);
    require(issuer, out); // not quiet: Those two would likely indicate an internal error, since we already checked the chain length above.
    ocspRequest = SecOCSPRequestCreate(leafCert, issuer);

    SCTs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(SCTs, out);

    CFArrayForEach(ocspResponsesData, ^(const void *value) {
        /* TODO: Should the builder already have the appropriate SecOCSPResponseRef ? */
        SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
        if(ocspResponse && SecOCSPGetResponseStatus(ocspResponse)==kSecOCSPSuccess) {
            SecOCSPSingleResponseRef ocspSingleResponse = SecOCSPResponseCopySingleResponse(ocspResponse, ocspRequest);
            if(ocspSingleResponse) {
                CFArrayRef singleResponseSCTs = SecOCSPSingleResponseCopySCTs(ocspSingleResponse);
                if(singleResponseSCTs) {
                    CFArrayAppendArray(SCTs, singleResponseSCTs, CFRangeMake(0, CFArrayGetCount(singleResponseSCTs)));
                    CFRelease(singleResponseSCTs);
                }
                SecOCSPSingleResponseDestroy(ocspSingleResponse);
            }
        }
        if(ocspResponse) SecOCSPResponseFinalize(ocspResponse);
    });

    if(CFArrayGetCount(SCTs)==0) {
        CFReleaseNull(SCTs);
    }

out:
    CFReleaseSafe(ocspResponsesData);
    if(ocspRequest)
        SecOCSPRequestFinalize(ocspRequest);

    return SCTs;
}

static bool find_validating_logs(SecPVCRef pvc, CFDictionaryRef trustedLogs,
                                 CFDictionaryRef CF_RETURNS_RETAINED * _Nonnull outCurrentLogsValidatingScts,
                                 CFDictionaryRef CF_RETURNS_RETAINED * _Nonnull outLogsValidatingEmbeddedScts,
                                 bool * _Nonnull out_at_least_one_currently_valid_external,
                                 bool * _Nonnull out_at_least_one_currently_valid_embedded) {
    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    CFArrayRef embeddedScts = SecCertificateCopySignedCertificateTimestamps(leafCert);
    CFArrayRef builderScts = SecPathBuilderCopySignedCertificateTimestamps(pvc->builder);
    CFArrayRef ocspScts = copy_ocsp_scts(pvc);
    CFDataRef precertEntry = copy_precert_entry_from_chain(pvc);
    CFDataRef x509Entry = copy_x509_entry_from_chain(pvc);
    __block CFAbsoluteTime certExpiry = SecCertificateNotValidAfter(leafCert);
    bool result = NO;

    // This eventually contain list of logs who validated the SCT.
    CFMutableDictionaryRef currentLogsValidatingScts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef logsValidatingEmbeddedScts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    uint64_t vt = TimestampFromCFAbsoluteTime(SecPVCGetVerifyTime(pvc));

    __block bool at_least_one_currently_valid_external = 0;
    __block bool at_least_one_currently_valid_embedded = 0;

    require(logsValidatingEmbeddedScts, out);
    require(currentLogsValidatingScts, out);

    /* Skip if there are no SCTs. */
    bool no_scts = (embeddedScts && CFArrayGetCount(embeddedScts) > 0) ||
                   (builderScts && CFArrayGetCount(builderScts) > 0) ||
                    (ocspScts && CFArrayGetCount(ocspScts) > 0);
    require_quiet(no_scts, out);

    if(trustedLogs && CFDictionaryGetCount(trustedLogs) > 0) { // Don't bother trying to validate SCTs if we don't have any trusted logs.
        if(embeddedScts && precertEntry) { // Don't bother if we could not get the precert.
            CFArrayForEach(embeddedScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 1, precertEntry, vt, certExpiry, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(logsValidatingEmbeddedScts, log, sct_at);
                    if(!CFDictionaryContainsKey(log, kSecCTRetirementDateKey)) {
                        addValidatingLog(currentLogsValidatingScts, log, sct_at);
                        at_least_one_currently_valid_embedded = true;
                    }
                }
            });
        }

        if(builderScts && x509Entry) { // Don't bother if we could not get the cert.
            CFArrayForEach(builderScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 0, x509Entry, vt, certExpiry, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(currentLogsValidatingScts, log, sct_at);
                    at_least_one_currently_valid_external = true;
                }
            });
        }

        if(ocspScts && x509Entry) {
            CFArrayForEach(ocspScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 0, x509Entry, vt, certExpiry, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(currentLogsValidatingScts, log, sct_at);
                    at_least_one_currently_valid_external = true;
                }
            });
        }
    }

    if (CFDictionaryGetCount(currentLogsValidatingScts) > 0) {
        result = true;
        *outCurrentLogsValidatingScts = CFRetainSafe(currentLogsValidatingScts);
        *outLogsValidatingEmbeddedScts = CFRetainSafe(logsValidatingEmbeddedScts);
        *out_at_least_one_currently_valid_embedded = at_least_one_currently_valid_embedded;
        *out_at_least_one_currently_valid_external = at_least_one_currently_valid_external;
    }

out:
    CFReleaseSafe(logsValidatingEmbeddedScts);
    CFReleaseSafe(currentLogsValidatingScts);
    CFReleaseSafe(builderScts);
    CFReleaseSafe(embeddedScts);
    CFReleaseSafe(ocspScts);
    CFReleaseSafe(precertEntry);
    CFReleaseSafe(x509Entry);
    return result;
}

/* For certificates issued before 4/21/2021:
 * Certificate lifetime | # of SCTs from separate logs
 *----------------------------------------------------
 * Less than 15 months  |   2
 * 15 to 27 months      |   3
 * 27 to 39 months      |   4
 * More than 39 months  |   5
 */
static bool verify_embedded_sct_policy_v1(SecCertificateRef leafCert, unsigned once_or_current_qualified_embedded) {
    __block bool result = false;
    __block int lifetime = 60; // in Months

    SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
        int _lifetime = lifetime;
        CFCalendarGetComponentDifference(zuluCalendar,
                                         SecCertificateNotValidBefore(leafCert),
                                         SecCertificateNotValidAfter(leafCert),
                                         0, "M", &_lifetime);
        lifetime = _lifetime;
    });

    unsigned requiredEmbeddedSctsCount = 5;

    if (lifetime < 15) {
        requiredEmbeddedSctsCount = 2;
    } else if (lifetime <= 27) {
        requiredEmbeddedSctsCount = 3;
    } else if (lifetime <= 39) {
        requiredEmbeddedSctsCount = 4;
    }

    if(once_or_current_qualified_embedded >= requiredEmbeddedSctsCount){
        result = true;
    }

    return result;
}

/* For certificates issued on or after 4/21/2021 00:00:00Z:
 * Certificate lifetime | # of SCTs from    | Maximum # of SCTs
 *                      | separate logs     | per log operator
 *----------------------------------------------------
 * 180 days or less     |   2               |   1
 * 181 to 825 days      |   3               |   2
 * 826 to 1187 days     |   4               |   N/A
 */
static bool verify_embedded_sct_policy_v2(SecCertificateRef leafCert, unsigned once_or_current_qualified_embedded) {
    bool result = false;
    CFAbsoluteTime lifetime = SecCertificateNotValidAfter(leafCert) - SecCertificateNotValidBefore(leafCert);

    unsigned requiredEmbeddedSctsCount = 4;
    if (lifetime <= 180*24*60*60) {
        requiredEmbeddedSctsCount = 2;
    } else if (lifetime <= 825*24*60*60) {
        requiredEmbeddedSctsCount = 3;
    }

    if (once_or_current_qualified_embedded >= requiredEmbeddedSctsCount){
        result = true;
    }

    return result;
}

static bool verify_tls_ct_policy(SecPVCRef pvc, CFDictionaryRef currentLogsValidatingScts, CFDictionaryRef logsValidatingEmbeddedScts,
                                 bool at_least_one_currently_valid_external, bool at_least_one_currently_valid_embedded,
                                 CFIndex * _Nonnull outTrustedSCTCount)
{
    if (!logsValidatingEmbeddedScts || !currentLogsValidatingScts) {
        return false;
    }

    __block CFAbsoluteTime issuanceTime = SecPVCGetVerifyTime(pvc);
    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    __block CFIndex trustedSCTCount = CFDictionaryGetCount(currentLogsValidatingScts);

    /* We now have 2 sets of logs that validated those SCTS, count them and make a final decision.

     Current Policy:
     is_ct = (A1 AND A2) OR (B1 AND B2).

     A1: embedded SCTs from 2+ to 5+ logs valid at issuance time
     A2: At least one embedded SCT from a currently valid log.

     B1: SCTs from 2 currently valid logs (from any source)
     B2: At least 1 external SCT from a currently valid log.

     */

    bool hasValidExternalSCT = (at_least_one_currently_valid_external && CFDictionaryGetCount(currentLogsValidatingScts)>=2);
    bool hasValidEmbeddedSCT = (at_least_one_currently_valid_embedded);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    bool result = false;

    if (hasValidEmbeddedSCT) {
        /* Calculate issuance time based on timestamp of SCTs from current logs */
        CFDictionaryForEach(currentLogsValidatingScts, ^(const void *key, const void *value) {
            CFDictionaryRef log = key;
            if(!CFDictionaryContainsKey(log, kSecCTRetirementDateKey)) {
                // Log is still qualified
                CFDateRef ts = (CFDateRef) value;
                CFAbsoluteTime timestamp = CFDateGetAbsoluteTime(ts);
                if(timestamp < issuanceTime) {
                    issuanceTime = timestamp;
                }
            }
        });
        SecCertificatePathVCSetIssuanceTime(path, issuanceTime);
    }
    if (hasValidExternalSCT) {
        /* Note: since external SCT validates this cert, we do not need to
           override issuance time here. If the cert also has a valid embedded
           SCT, issuanceTime will be calculated and set in the block above. */
        result = true;
    } else if (hasValidEmbeddedSCT) {
        /* Count Logs */
        __block unsigned once_or_current_qualified_embedded = 0;
        __block bool failed_once_check = false;
        CFDictionaryForEach(logsValidatingEmbeddedScts, ^(const void *key, const void *value) {
            CFDictionaryRef log = key;
            CFDateRef ts = value;
            CFDateRef expiry = CFDictionaryGetValue(log, kSecCTRetirementDateKey);
            if (expiry == NULL) {                                               // Currently qualified OR
                once_or_current_qualified_embedded++;
            } else if (CFDateCompare(ts, expiry, NULL) == kCFCompareLessThan && // Once qualified. That is, qualified at the time of SCT AND
                       issuanceTime < CFDateGetAbsoluteTime(expiry)) {          // at the time of issuance.)
                once_or_current_qualified_embedded++;
                trustedSCTCount++;
            } else {
                failed_once_check = true;
            }
        });

        CFAbsoluteTime apr_21_2021 = 640656000.0; // 21 April 2021 00:00:00 UTC
        if (SecCertificateNotValidBefore(leafCert) < apr_21_2021) {
            result = verify_embedded_sct_policy_v1(leafCert, once_or_current_qualified_embedded);
        } else {
            result = verify_embedded_sct_policy_v2(leafCert, once_or_current_qualified_embedded);
        }
    }

    *outTrustedSCTCount = trustedSCTCount;
    return result;
}

static void report_ct_analytics(SecPVCRef pvc, CFDictionaryRef currentLogsValidatingScts, CFIndex trustedSCTCount)
{
    /* Record analytics data for CT */
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(pvc->builder);
    if (!analytics) {
        return;
    }

    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    CFArrayRef embeddedScts = SecCertificateCopySignedCertificateTimestamps(leafCert);
    CFArrayRef builderScts = SecPathBuilderCopySignedCertificateTimestamps(pvc->builder);
    CFArrayRef ocspScts = copy_ocsp_scts(pvc);

    uint32_t sctCount = 0;
    /* Count the total number of SCTs we found and report where we got them */
    if (embeddedScts && CFArrayGetCount(embeddedScts) > 0) {
        analytics->sct_sources |= TA_SCTEmbedded;
        sctCount += CFArrayGetCount(embeddedScts);
    }
    if (builderScts && CFArrayGetCount(builderScts) > 0) {
        analytics->sct_sources |= TA_SCT_TLS;
        sctCount += CFArrayGetCount(builderScts);
    }
    if (ocspScts && CFArrayGetCount(ocspScts) > 0) {
        analytics->sct_sources |= TA_SCT_OCSP;
        sctCount += CFArrayGetCount(ocspScts);
    }
    /* Report how many of those SCTs were once or currently qualified */
    analytics->number_trusted_scts = (uint32_t)trustedSCTCount;
    /* Report how many SCTs we got */
    analytics->number_scts = sctCount;
    /* Only one current SCT -- close to failure */
    if (CFDictionaryGetCount(currentLogsValidatingScts) == 1) {
        analytics->ct_one_current = true;
    }

    CFReleaseNull(embeddedScts);
    CFReleaseNull(builderScts);
    CFReleaseNull(ocspScts);
}

void SecPolicyCheckCT(SecPVCRef pvc)
{
    CFDictionaryRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);
    if (!trustedLogs) {
        SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
        trustedLogs = SecOTAPKICopyTrustedCTLogs(otapkiref);
        CFReleaseSafe(otapkiref);
    }

    CFDictionaryRef currentLogsValidatingScts = NULL;
    CFDictionaryRef logsValidatingEmbeddedScts = NULL;
    bool at_least_one_currently_valid_external = false;
    bool at_least_one_currently_valid_embedded = false;
    CFIndex trustedSCTCount = 0;
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);

    SecCertificatePathVCSetIsCT(path, false);
    if (find_validating_logs(pvc, trustedLogs, &currentLogsValidatingScts, &logsValidatingEmbeddedScts, &at_least_one_currently_valid_external, &at_least_one_currently_valid_embedded)) {
        if (verify_tls_ct_policy(pvc, currentLogsValidatingScts, logsValidatingEmbeddedScts, at_least_one_currently_valid_external, at_least_one_currently_valid_embedded, &trustedSCTCount)) {
            SecCertificatePathVCSetIsCT(path, true);
        }
        report_ct_analytics(pvc, currentLogsValidatingScts, trustedSCTCount);
    }

    CFReleaseNull(currentLogsValidatingScts);
    CFReleaseNull(logsValidatingEmbeddedScts);
    CFReleaseNull(trustedLogs);
}

bool SecPolicyCheckNonTlsCT(SecPVCRef pvc)
{
    CFDictionaryRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);
    if (!trustedLogs) {
        SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
        trustedLogs = SecOTAPKICopyNonTlsTrustedCTLogs(otapkiref);
        CFReleaseSafe(otapkiref);
    }

    CFDictionaryRef currentLogsValidatingScts = NULL;
    CFDictionaryRef logsValidatingEmbeddedScts = NULL;
    bool at_least_one_currently_valid_external = false;
    bool at_least_one_currently_valid_embedded = false;
    CFIndex trustedSCTCount = 0;
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    bool result = false;

    SecCertificatePathVCSetIsCT(path, false);
    if (find_validating_logs(pvc, trustedLogs, &currentLogsValidatingScts, &logsValidatingEmbeddedScts, &at_least_one_currently_valid_external, &at_least_one_currently_valid_embedded)) {
        if (verify_tls_ct_policy(pvc, currentLogsValidatingScts, logsValidatingEmbeddedScts, at_least_one_currently_valid_external, at_least_one_currently_valid_embedded, &trustedSCTCount)) {
            result = true;
        }
        report_ct_analytics(pvc, currentLogsValidatingScts, trustedSCTCount);
    }

    CFReleaseNull(currentLogsValidatingScts);
    CFReleaseNull(logsValidatingEmbeddedScts);
    CFReleaseNull(trustedLogs);
    return result;
}
