/*
 * Copyright (c) 2008-2009,2012-2014,2017 Apple Inc. All Rights Reserved.
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
 * SecTrustServer.h - certificate trust evaluation engine
 *
 *
 */

#ifndef _SECURITY_SECTRUSTSERVER_H_
#define _SECURITY_SECTRUSTSERVER_H_

#include <CoreFoundation/CFString.h>

#include <Security/SecTrust.h>
#include <Security/SecBasePriv.h> /* For errSecWaitForCallback. */
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/SecCertificateSource.h"
#include <mach/port.h>

__BEGIN_DECLS

typedef struct SecPathBuilder *SecPathBuilderRef;

typedef struct OpaqueSecPVC *SecPVCRef;

struct OpaqueSecPVC {
    SecPathBuilderRef builder;
    CFArrayRef policies;
    CFDictionaryRef callbacks;
    CFIndex policyIX;
    bool require_revocation_response;

    CFArrayRef leafDetails;
    SecTrustResultType leafResult;

    CFArrayRef details;
    SecTrustResultType result;
};

/* Completion callback. */
typedef void(*SecPathBuilderCompleted)(const void *userData,
    CFArrayRef chain, CFArrayRef details, CFDictionaryRef info,
    SecTrustResultType result);

/* Returns a new trust path builder and policy evaluation engine instance. */
SecPathBuilderRef SecPathBuilderCreate(dispatch_queue_t builderQueue, CFDataRef clientAuditToken,
    CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly,
    bool keychainsAllowed, CFArrayRef policies, CFArrayRef ocspResponse,
    CFArrayRef signedCertificateTimestamps, CFArrayRef trustedLogs,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups, CFArrayRef exceptions,
    SecPathBuilderCompleted completed, const void *userData);
void SecPathBuilderDestroy(SecPathBuilderRef builder);

/* engine states exposed for testing */
bool SecPathBuilderDidValidatePath(SecPathBuilderRef builder);
bool SecPathBuilderReportResult(SecPathBuilderRef builder);

/* Returns true if it's ok to perform network operations for this builder. */
bool SecPathBuilderCanAccessNetwork(SecPathBuilderRef builder);

/* Disable or enable network access for this builder if allow is false
   network access will be disabled. */
void SecPathBuilderSetCanAccessNetwork(SecPathBuilderRef builder, bool allow);

/* Get the stapled SCTs */
CFArrayRef SecPathBuilderCopySignedCertificateTimestamps(SecPathBuilderRef builder);
CFArrayRef SecPathBuilderCopyOCSPResponses(SecPathBuilderRef builder);
CFDictionaryRef SecPathBuilderCopyTrustedLogs(SecPathBuilderRef builder);

CFSetRef SecPathBuilderGetAllPaths(SecPathBuilderRef builder);
SecCertificatePathVCRef SecPathBuilderGetPath(SecPathBuilderRef builder);
SecCertificatePathVCRef SecPathBuilderGetBestPath(SecPathBuilderRef builder);
void SecPathBuilderSetPath(SecPathBuilderRef builder, SecCertificatePathVCRef path);
CFAbsoluteTime SecPathBuilderGetVerifyTime(SecPathBuilderRef builder);
CFIndex SecPathBuilderGetCertificateCount(SecPathBuilderRef builder);
SecCertificateRef SecPathBuilderGetCertificateAtIndex(SecPathBuilderRef builder, CFIndex ix);
CFArrayRef SecPathBuilderGetExceptions(SecPathBuilderRef builder);
bool SecPathBuilderHasTemporalParentChecks(SecPathBuilderRef builder);

/* Returns the isAnchored status of the path. The path builder sets isAnchored
 * based solely on whether the terminating cert has some sort of trust setting
 * on it. This check does NOT reflect whether that anchor is actually trusted,
 * as trust in an anchor is contextual to the policy being validated. */
bool SecPathBuilderIsAnchored(SecPathBuilderRef builder);
bool SecPathBuilderIsAnchorSource(SecPathBuilderRef builder, SecCertificateSourceRef source);
SecCertificateSourceRef SecPathBuilderGetAppAnchorSource(SecPathBuilderRef builder);

CFIndex SecPathBuilderGetPVCCount(SecPathBuilderRef builder);
SecPVCRef SecPathBuilderGetPVCAtIndex(SecPathBuilderRef builder, CFIndex ix);

/* Returns the first PVC that passed */
SecPVCRef SecPathBuilderGetResultPVC(SecPathBuilderRef builder);

void SecPathBuilderSetResultInPVCs(SecPathBuilderRef builder, CFStringRef key,
                                   CFIndex ix, CFTypeRef result, bool force);

/* This is an atomic pre-decrement operation */
unsigned int SecPathBuilderDecrementAsyncJobCount(SecPathBuilderRef builder);
void SecPathBuilderSetAsyncJobCount(SecPathBuilderRef builder, unsigned int jobCount);
unsigned int SecPathBuilderGetAsyncJobCount(SecPathBuilderRef builder);

CFMutableDictionaryRef SecPathBuilderGetInfo(SecPathBuilderRef builder);

/* Enable revocation checking if the rest of the policy checks succeed. */
CFStringRef SecPathBuilderGetRevocationMethod(SecPathBuilderRef builder);
void SecPathBuilderSetRevocationMethod(SecPathBuilderRef builder, CFStringRef method);

/* Require a online revocation response for the chain. */
bool SecPathBuilderGetCheckRevocationOnline(SecPathBuilderRef builder);
void SecPathBuilderSetCheckRevocationOnline(SecPathBuilderRef builder);

/* Only do networking for revocation if the chain is trusted */
bool SecPathBuilderGetCheckRevocationIfTrusted(SecPathBuilderRef builder);
void SecPathBuilderSetCheckRevocationIfTrusted(SecPathBuilderRef builder);

/* Core of the trust evaluation engine, this will invoke the completed
   callback and return false if the evaluation completed, or return true if
   the evaluation is still waiting for some external event (usually the
   network). */
bool SecPathBuilderStep(SecPathBuilderRef builder);

/* Return the dispatch queue to be used by this builder. */
dispatch_queue_t SecPathBuilderGetQueue(SecPathBuilderRef builder);

/* Return the client audit token associated with this path builder,
   which caller must release, or NULL if there is no external client. */
CFDataRef SecPathBuilderCopyClientAuditToken(SecPathBuilderRef builder);

/* Evaluate trust and call evaluated when done. */
void SecTrustServerEvaluateBlock(dispatch_queue_t builderQueue, CFDataRef clientAuditToken, CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef exceptions, void (^evaluated)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, CFArrayRef chain, CFErrorRef error));

/* Synchronously invoke SecTrustServerEvaluateBlock. */
SecTrustResultType SecTrustServerEvaluate(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef exceptions, CFArrayRef *details, CFDictionaryRef *info, CFArrayRef *chain, CFErrorRef *error);

/* TrustAnalytics builder types */
typedef CF_OPTIONS(uint8_t, TA_SCTSource) {
    TA_SCTEmbedded  = 1 << 0,
    TA_SCT_OCSP     = 1 << 1,
    TA_SCT_TLS      = 1 << 2,
};

typedef CF_OPTIONS(uint8_t, TAValidStatus) {
    TAValidDefinitelyOK = 1 << 0,
    TAValidProbablyOK = 1 << 1,
    TAValidProbablyRevoked = 1 << 2,
    TAValidDefinitelyRevoked = 1 << 3,
    TAValidDateConstrainedOK = 1 << 4,
    TAValidDateConstrainedRevoked = 1 << 5,
    TAValidPolicyConstrainedOK = 1 << 6,
    TAValidPolicyConstrainedDenied = 1 << 7,
};

typedef struct {
    uint64_t start_time;
    bool suspected_mitm;
    bool ca_fail_eku_check;
    bool tls_invalid_ku;
    // Certificate Transparency
    TA_SCTSource sct_sources;
    uint32_t number_scts;
    uint32_t number_trusted_scts;
    bool ct_one_current;
    // CAIssuer
    bool ca_issuer_cache_hit;
    bool ca_issuer_network;
    uint32_t ca_issuer_fetches;
    uint64_t ca_issuer_fetch_time;
    uint32_t ca_issuer_fetch_failed;
    bool ca_issuer_unsupported_data;
    bool ca_issuer_multiple_certs;
    // OCSP
    bool ocsp_no_check;
    bool ocsp_cache_hit;
    bool ocsp_network;
    uint32_t ocsp_fetches;
    uint64_t ocsp_fetch_time;
    uint32_t ocsp_fetch_failed;
    bool ocsp_validation_failed;
    bool ocsp_weak_hash;
    // Valid
    TAValidStatus valid_status;
    bool valid_trigger_ocsp;
    bool valid_require_ct;
    bool valid_known_intermediates_only;
    bool valid_unknown_intermediate;
} TrustAnalyticsBuilder;

TrustAnalyticsBuilder *SecPathBuilderGetAnalyticsData(SecPathBuilderRef builder);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTSERVER_H_ */
