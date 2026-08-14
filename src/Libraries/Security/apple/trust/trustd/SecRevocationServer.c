/*
 * Copyright (c) 2008-2019 Apple Inc. All Rights Reserved.
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

/*
 * SecRevocationServer.c - Engine for evaluating certificate revocation.
 */

#include <AssertMacros.h>

#include <mach/mach_time.h>

#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>

#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecOCSPRequest.h"
#include "trust/trustd/SecOCSPResponse.h"
#include "trust/trustd/SecOCSPCache.h"
#include "trust/trustd/SecRevocationDb.h"
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecRevocationNetworking.h"

#include "trust/trustd/SecRevocationServer.h"

// MARK: SecORVCRef
/********************************************************
 ****************** OCSP RVC Functions ******************
 ********************************************************/
const CFAbsoluteTime kSecDefaultOCSPResponseTTL = 24.0 * 60.0 * 60.0;
const CFAbsoluteTime kSecOCSPResponseOnlineTTL = 5.0 * 60.0;
#define OCSP_RESPONSE_TIMEOUT       (3 * NSEC_PER_SEC)

static void SecORVCFinish(SecORVCRef orvc) {
    secdebug("alloc", "finish orvc %p", orvc);
    if (orvc->ocspRequest) {
        SecOCSPRequestFinalize(orvc->ocspRequest);
        orvc->ocspRequest = NULL;
    }
    if (orvc->ocspResponse) {
        SecOCSPResponseFinalize(orvc->ocspResponse);
        orvc->ocspResponse = NULL;
        if (orvc->ocspSingleResponse) {
            SecOCSPSingleResponseDestroy(orvc->ocspSingleResponse);
            orvc->ocspSingleResponse = NULL;
        }
    }
    memset(orvc, 0, sizeof(struct OpaqueSecORVC));
}

/* Process a verified ocsp response for a given cert. Return true if the
 certificate status was obtained. */
static bool SecOCSPSingleResponseProcess(SecOCSPSingleResponseRef this,
                                         SecORVCRef rvc) {
    bool processed;
    switch (this->certStatus) {
        case CS_Good:
            secdebug("ocsp", "CS_Good for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as valid until a given date (nextUpdate if we have one)
             in the info dictionary. */
            //cert.revokeCheckGood(true);
            rvc->nextUpdate = this->nextUpdate == NULL_TIME ? this->thisUpdate + kSecDefaultOCSPResponseTTL : this->nextUpdate;
            rvc->thisUpdate = this->thisUpdate;
            processed = true;
            break;
        case CS_Revoked:
            secdebug("ocsp", "CS_Revoked for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as revoked (with reason) at revocation date in
             the info dictionary, or perhaps we should use a different key per
             reason?   That way a client using exceptions can ignore some but
             not all reasons. */
            SInt32 reason = this->crlReason;
            CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                  cfreason, true);
            SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
            if (path) {
                SecCertificatePathVCSetRevocationReasonForCertificateAtIndex(path, rvc->certIX, cfreason);
            }
            CFRelease(cfreason);
            processed = true;
            break;
        case CS_Unknown:
            /* not an error, no per-cert status, nothing here */
            secdebug("ocsp", "CS_Unknown for cert %" PRIdCFIndex, rvc->certIX);
            processed = false;
            break;
        default:
            secnotice("ocsp", "BAD certStatus (%d) for cert %" PRIdCFIndex,
                      (int)this->certStatus, rvc->certIX);
            processed = false;
            break;
    }

    return processed;
}

void SecORVCUpdatePVC(SecORVCRef rvc) {
    if (rvc->ocspSingleResponse) {
        SecOCSPSingleResponseProcess(rvc->ocspSingleResponse, rvc);
    }
    if (rvc->ocspResponse) {
        rvc->nextUpdate = SecOCSPResponseGetExpirationTime(rvc->ocspResponse);
        rvc->thisUpdate = SecOCSPResponseProducedAt(rvc->ocspResponse);
    }
}

typedef void (^SecOCSPEvaluationCompleted)(SecTrustResultType tr);

static void
SecOCSPEvaluateCompleted(const void *userData,
                         CFArrayRef chain, CFArrayRef details, CFDictionaryRef info,
                         SecTrustResultType result) {
    SecOCSPEvaluationCompleted evaluated = (SecOCSPEvaluationCompleted)userData;
    evaluated(result);
    Block_release(evaluated);

}

static bool SecOCSPResponseEvaluateSigner(SecORVCRef rvc, CFArrayRef signers, CFArrayRef issuers, CFAbsoluteTime verifyTime) {
    __block bool evaluated = false;
    bool trusted = false;
    if (!signers || !issuers) {
        return trusted;
    }

    /* Verify the signer chain against the OCSPSigner policy, using the issuer chain as anchors. */
    const void *ocspSigner = SecPolicyCreateOCSPSigner();
    CFArrayRef policies = CFArrayCreate(kCFAllocatorDefault,
                                        &ocspSigner, 1, &kCFTypeArrayCallBacks);
    CFRelease(ocspSigner);

    SecOCSPEvaluationCompleted completed = Block_copy(^(SecTrustResultType result) {
        if (result == kSecTrustResultProceed || result == kSecTrustResultUnspecified) {
            evaluated = true;
        }
    });

    CFDataRef clientAuditToken = SecPathBuilderCopyClientAuditToken(rvc->builder);
    SecPathBuilderRef oBuilder = SecPathBuilderCreate(NULL, clientAuditToken,
                                                      signers, issuers, true, false,
                                                      policies, NULL, NULL,  NULL,
                                                      verifyTime, NULL, NULL,
                                                      SecOCSPEvaluateCompleted, completed);
    /* disable network access to avoid recursion */
    SecPathBuilderSetCanAccessNetwork(oBuilder, false);

    /* Build the chain(s), evaluate them, call the completed block, free the block and builder */
    SecPathBuilderStep(oBuilder);
    CFReleaseNull(clientAuditToken);
    CFReleaseNull(policies);

    /* verify the public key of the issuer signed the OCSP signer */
    if (evaluated) {
        SecCertificateRef issuer = NULL, signer = NULL;
        SecKeyRef issuerPubKey = NULL;

        issuer = (SecCertificateRef)CFArrayGetValueAtIndex(issuers, 0);
        signer = (SecCertificateRef)CFArrayGetValueAtIndex(signers, 0);

        if (issuer) {
            issuerPubKey = SecCertificateCopyKey(issuer);
        }
        if (signer && issuerPubKey && (errSecSuccess == SecCertificateIsSignedBy(signer, issuerPubKey))) {
            trusted = true;
        } else {
            secnotice("ocsp", "ocsp signer cert not signed by issuer");
        }
        CFReleaseNull(issuerPubKey);
    }

    return trusted;
}

static bool SecOCSPResponseVerify(SecOCSPResponseRef ocspResponse, SecORVCRef rvc, CFAbsoluteTime verifyTime) {
    bool trusted;
    SecCertificatePathVCRef issuers = SecCertificatePathVCCopyFromParent(SecPathBuilderGetPath(rvc->builder), rvc->certIX + 1);
    SecCertificateRef issuer = issuers ? CFRetainSafe(SecCertificatePathVCGetCertificateAtIndex(issuers, 0)) : NULL;
    CFArrayRef signers = SecOCSPResponseCopySigners(ocspResponse);
    SecCertificateRef signer = SecOCSPResponseCopySigner(ocspResponse, issuer);

    if (signer && signers) {
        if (issuer && CFEqual(signer, issuer)) {
            /* We already know we trust issuer since it's the issuer of the
             * cert we are verifying. */
            secinfo("ocsp", "ocsp responder: %@ response signed by issuer",
                    rvc->responder);
            trusted = true;
        } else {
            secinfo("ocsp", "ocsp responder: %@ response signed by cert issued by issuer",
                    rvc->responder);
            CFMutableArrayRef signerCerts = NULL;
            CFArrayRef issuerCerts = NULL;

            /* Ensure the signer cert is the 0th cert for trust evaluation */
            signerCerts = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(signerCerts, signer);
            CFArrayAppendArray(signerCerts, signers, CFRangeMake(0, CFArrayGetCount(signers)));

            if (issuers) {
                issuerCerts = SecCertificatePathVCCopyCertificates(issuers);
            }

            if (SecOCSPResponseEvaluateSigner(rvc, signerCerts, issuerCerts, verifyTime)) {
                secdebug("ocsp", "response satisfies ocspSigner policy (%@)",
                         rvc->responder);
                trusted = true;
            } else {
                /* @@@ We don't trust the cert so don't use this response. */
                secnotice("ocsp", "ocsp response signed by certificate which "
                          "does not satisfy ocspSigner policy");
                trusted = false;
            }
            CFReleaseNull(signerCerts);
            CFReleaseNull(issuerCerts);
        }
    } else {
        /* @@@ No signer found for this ocsp response, discard it. */
        secnotice("ocsp", "ocsp responder: %@ no signer found for response",
                  rvc->responder);
        trusted = false;
    }

#if DUMP_OCSPRESPONSES
    char buf[40];
    snprintf(buf, 40, "/tmp/ocspresponse%ld%s.der",
             rvc->certIX, (trusted ? "t" : "u"));
    secdumpdata(ocspResponse->data, buf);
#endif
    CFReleaseNull(issuers);
    CFReleaseNull(issuer);
    CFReleaseNull(signers);
    CFReleaseNull(signer);
    return trusted;
}

void SecORVCConsumeOCSPResponse(SecORVCRef rvc, SecOCSPResponseRef ocspResponse /*CF_CONSUMED*/,
                                CFTimeInterval maxAge, bool updateCache, bool fromCache) {
    SecOCSPSingleResponseRef sr = NULL;
    require_quiet(ocspResponse, errOut);
    SecOCSPResponseStatus orStatus = SecOCSPGetResponseStatus(ocspResponse);
    require_action_quiet(orStatus == kSecOCSPSuccess, errOut,
                         secnotice("ocsp", "responder: %@ returned status: %d",  rvc->responder, orStatus));
    require_action_quiet(sr = SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest), errOut,
                         secnotice("ocsp",  "ocsp responder: %@ did not include status of requested cert", rvc->responder));
    // Check if this response is fresher than any (cached) response we might still have in the rvc.
    require_quiet(!rvc->ocspSingleResponse || rvc->ocspSingleResponse->thisUpdate < sr->thisUpdate, errOut);

    CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
#if TARGET_OS_IPHONE
    /* Check the OCSP response signature and verify the response if not pulled from the cache.
     * Performance optimization since we don't write invalid responses to the cache. */
    if (!fromCache) {
        require_quiet(SecOCSPResponseVerify(ocspResponse, rvc,
                                            sr->certStatus == CS_Revoked ? SecOCSPResponseProducedAt(ocspResponse) : verifyTime), errOut);
    }
#else
    /* Always check the OCSP response signature and verify the response (since the cache is user-modifiable). */
    require_quiet(SecOCSPResponseVerify(ocspResponse, rvc,
                                        sr->certStatus == CS_Revoked ? SecOCSPResponseProducedAt(ocspResponse) : verifyTime), errOut);
#endif

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if (analytics && SecOCSPResponseIsWeakHash(ocspResponse)) {
        analytics->ocsp_weak_hash = true;
    }

    // If we get here, we have a properly signed ocsp response
    // but we haven't checked dates yet.

    bool sr_valid = SecOCSPSingleResponseCalculateValidity(sr, kSecDefaultOCSPResponseTTL, verifyTime);
    if (sr_valid) {
        rvc->rvc->revocation_checked = true;
    }
    if (sr->certStatus == CS_Good) {
        // Side effect of SecOCSPResponseCalculateValidity sets ocspResponse->expireTime
        require_quiet(sr_valid && SecOCSPResponseCalculateValidity(ocspResponse, maxAge, kSecDefaultOCSPResponseTTL, verifyTime), errOut);
    } else if (sr->certStatus == CS_Revoked) {
        // Expire revoked responses when the subject certificate itself expires.
        ocspResponse->expireTime = SecCertificateNotValidAfter(SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX));
    }

    // Ok we like the new response, let's toss the old one.
    if (updateCache)
        SecOCSPCacheReplaceResponse(rvc->ocspResponse, ocspResponse, rvc->responder, verifyTime);

    if (rvc->ocspResponse) SecOCSPResponseFinalize(rvc->ocspResponse);
    rvc->ocspResponse = ocspResponse;
    ocspResponse = NULL;

    if (rvc->ocspSingleResponse) SecOCSPSingleResponseDestroy(rvc->ocspSingleResponse);
    rvc->ocspSingleResponse = sr;
    sr = NULL;

    rvc->done = sr_valid;

errOut:
    if (sr) SecOCSPSingleResponseDestroy(sr);
    if (ocspResponse) SecOCSPResponseFinalize(ocspResponse);
}

static SecORVCRef SecORVCCreate(SecRVCRef rvc, SecPathBuilderRef builder, CFIndex certIX) {
    SecORVCRef orvc = NULL;
    orvc = malloc(sizeof(struct OpaqueSecORVC));
    secdebug("alloc", "orvc %p", orvc);
    if (orvc) {
        memset(orvc, 0, sizeof(struct OpaqueSecORVC));
        orvc->builder = builder;
        orvc->rvc = rvc;
        orvc->certIX = certIX;

        SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(builder, certIX);
        if (SecPathBuilderGetCertificateCount(builder) > (certIX + 1)) {
            SecCertificateRef issuer = SecPathBuilderGetCertificateAtIndex(builder, certIX + 1);
            orvc->ocspRequest = SecOCSPRequestCreate(cert, issuer);
        }
    }
    return orvc;
}

static void SecORVCProcessStapledResponses(SecORVCRef rvc) {
    /* Get stapled OCSP responses */
    CFArrayRef ocspResponsesData = SecPathBuilderCopyOCSPResponses(rvc->builder);

    if(ocspResponsesData) {
        secdebug("rvc", "Checking stapled responses for cert %ld", rvc->certIX);
        CFArrayForEach(ocspResponsesData, ^(const void *value) {
            SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
            SecORVCConsumeOCSPResponse(rvc, ocspResponse, NULL_TIME, false, false);
        });
        CFRelease(ocspResponsesData);
    }
}

void SecRVCDelete(SecRVCRef rvc) {
    secdebug("alloc", "delete rvc %p", rvc);
    if (rvc->orvc) {
        SecORVCFinish(rvc->orvc);
        free(rvc->orvc);
        rvc->orvc = NULL;
    }
    if (rvc->valid_info) {
        CFReleaseNull(rvc->valid_info);
    }
}

// Forward declaration
static void SecRVCSetFinishedWithoutNetwork(SecRVCRef rvc);

static void SecRVCInit(SecRVCRef rvc, SecPathBuilderRef builder, CFIndex certIX) {
    secdebug("alloc", "rvc %p", rvc);
    rvc->builder = builder;
    rvc->certIX = certIX;
    rvc->orvc = SecORVCCreate(rvc, builder, certIX);
    if (!rvc->orvc) {
        SecRVCDelete(rvc);
        SecRVCSetFinishedWithoutNetwork(rvc);
    } else {
        rvc->done = false;
    }
}

static bool SecRVCShouldCheckOCSP(SecRVCRef rvc) {
    return true;
}

static bool SecRVCPolicyConstraintsPermitPolicy(SecValidPolicy *constraints, CFIndex count, SecPolicyRef policy) {
    if (!constraints || !policy) {
        return true; /* nothing to constrain */
    }
    SecValidPolicy policyType = kSecValidPolicyAny;
    CFStringRef policyName = SecPolicyGetName(policy);
    /* determine if the policy is a candidate for being constrained */
    if (CFEqualSafe(policyName, kSecPolicyNameSSLServer) ||
               CFEqualSafe(policyName, kSecPolicyNameEAPServer) ||
               CFEqualSafe(policyName, kSecPolicyNameIPSecServer)) {
        policyType = kSecValidPolicyServerAuthentication;
    } else if (CFEqualSafe(policyName, kSecPolicyNameSSLClient) ||
               CFEqualSafe(policyName, kSecPolicyNameEAPClient) ||
               CFEqualSafe(policyName, kSecPolicyNameIPSecClient)) {
        policyType = kSecValidPolicyClientAuthentication;
    } else if (CFEqualSafe(policyName, kSecPolicyNameSMIME)) {
        policyType = kSecValidPolicyEmailProtection;
    } else if (CFEqualSafe(policyName, kSecPolicyNameCodeSigning)) {
        policyType = kSecValidPolicyCodeSigning;
    } else if (CFEqualSafe(policyName, kSecPolicyNameTimeStamping)) {
        policyType = kSecValidPolicyTimeStamping;
    }
    if (policyType == kSecValidPolicyAny) {
        return true; /* policy not subject to constraint */
    }
    /* policy is subject to constraint; do the constraints allow it? */
    bool result = false;
    for (CFIndex ix = 0; ix < count; ix++) {
        SecValidPolicy allowedPolicy = constraints[ix];
        if (allowedPolicy == kSecValidPolicyAny ||
            allowedPolicy == policyType) {
            result = true;
            break;
        }
    }
    if (!result) {
        secnotice("rvc", "%@ not allowed by policy constraints on issuing CA", policyName);
    }
    return result;
}

static bool SecRVCGetPolicyConstraints(CFDataRef data, SecValidPolicy **constraints, CFIndex *count) {
    /* Sanity-check the input policy constraints data, returning pointer and
     * count values in output arguments. Function result is true if successful.
     *
     * The first byte of the policy constraints data contains the number of entries,
     * followed by an array of 0..n policy constraint values of type SecValidPolicy.
     * The maximum number of defined policies is not expected to approach 127, i.e.
     * the largest value which can be expressed in a signed byte.
     */
    bool result = false;
    CFIndex length = 0;
    SecValidPolicy *p = NULL;
    if (data) {
        length = CFDataGetLength(data);
        p = (SecValidPolicy *)CFDataGetBytePtr(data);
    }
    /* Verify that count is 0 or greater, and equal to remaining number of bytes */
    CFIndex c = (length > 0) ? *p++ : -1;
    if (c < 0 || c != (length - 1)) {
        secerror("invalid policy constraints array");
    } else {
        if (constraints) {
            *constraints = p;
        }
        if (count) {
            *count = c;
        }
        result = true;
    }
    return result;
}

static void SecRVCProcessValidPolicyConstraints(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    if (!rvc->valid_info->hasPolicyConstraints) {
        return;
    }
    CFIndex count = 0;
    SecValidPolicy *constraints = NULL;
    if (!SecRVCGetPolicyConstraints(rvc->valid_info->policyConstraints, &constraints, &count)) {
        return;
    }
    secdebug("rvc", "found policy constraints for cert at index %ld", rvc->certIX);

    /* check that policies being verified are permitted by the policy constraints */
    bool policyDeniedByConstraints = false;
    CFIndex ix, initialPVCCount = SecPathBuilderGetPVCCount(rvc->builder);
    for (ix = 0; ix < initialPVCCount; ix++) {
        SecPVCRef pvc = SecPathBuilderGetPVCAtIndex(rvc->builder, ix);
        CFArrayRef policies = CFRetainSafe(pvc->policies);
        CFIndex policyCount = (policies) ? CFArrayGetCount(policies) : 0;
        for (CFIndex policyIX = 0; policyIX < policyCount; policyIX++) {
            SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
            if (!SecRVCPolicyConstraintsPermitPolicy(constraints, count, policy)) {
                policyDeniedByConstraints = true;
                if (rvc->valid_info->overridable) {
                    SecPVCSetResultForcedWithTrustResult(pvc, kSecPolicyCheckIssuerPolicyConstraints, rvc->certIX,
                                                         kCFBooleanFalse, true, kSecTrustResultRecoverableTrustFailure);
                } else {
                    SecPVCSetResultForced(pvc, kSecPolicyCheckIssuerPolicyConstraints, rvc->certIX, kCFBooleanFalse, true);
                }
            }
        }
        CFReleaseSafe(policies);
    }
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if (analytics) {
        TAValidStatus status = (policyDeniedByConstraints) ? TAValidPolicyConstrainedDenied : TAValidPolicyConstrainedOK;
        analytics->valid_status |= status;
    }
}

static void SecRVCProcessValidDateConstraints(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    if (!rvc->valid_info->hasDateConstraints) {
        return;
    }
    SecCertificateRef certificate = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    if (!certificate) {
        return;
    }
    CFAbsoluteTime certIssued = SecCertificateNotValidBefore(certificate);
    CFAbsoluteTime caNotBefore = -3155760000.0; /* default: 1901-01-01 00:00:00-0000 */
    CFAbsoluteTime caNotAfter = 31556908800.0;  /* default: 3001-01-01 00:00:00-0000 */
    if (rvc->valid_info->notBeforeDate) {
        caNotBefore = CFDateGetAbsoluteTime(rvc->valid_info->notBeforeDate);
    }
    if (rvc->valid_info->notAfterDate) {
        caNotAfter = CFDateGetAbsoluteTime(rvc->valid_info->notAfterDate);
        /* per the Valid specification, if this date is in the past, we need to check CT. */
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if (caNotAfter < now) {
            rvc->valid_info->requireCT = true;
        }
    }
    if ((certIssued < caNotBefore) && (rvc->certIX > 0)) {
        /* not-before constraint is only applied to leaf certificate, for now. */
        return;
    }

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if ((certIssued < caNotBefore) || (certIssued > caNotAfter)) {
        /* We are outside the constrained validity period. */
        secnotice("rvc", "certificate issuance date not within the allowed range for this CA%s",
                  (rvc->valid_info->overridable) ? "" : " (non-recoverable error)");
        if (analytics) {
            analytics->valid_status |= TAValidDateConstrainedRevoked;
        }
        if (rvc->valid_info->overridable) {
            /* error is recoverable, treat certificate as untrusted
               (note this date check is different from kSecPolicyCheckTemporalValidity) */
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckGrayListedKey, rvc->certIX,
                                          kCFBooleanFalse, true);
        } else {
            /* error is non-overridable, treat certificate as revoked */
            SInt32 reason = 0; /* unspecified reason code */
            CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                          cfreason, true);
            SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
            if (path) {
                SecCertificatePathVCSetRevocationReasonForCertificateAtIndex(path, rvc->certIX, cfreason);
            }
            CFReleaseNull(cfreason);
        }
    } else if (analytics) {
        analytics->valid_status |= TAValidDateConstrainedOK;
    }
}

bool SecRVCHasDefinitiveValidInfo(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info) {
        return false;
    }
    SecValidInfoRef info = rvc->valid_info;
    /* outcomes as defined in Valid server specification */
    if (info->format == kSecValidInfoFormatSerial ||
        info->format == kSecValidInfoFormatSHA256) {
        if (info->noCACheck || info->complete || info->isOnList) {
            return true;
        }
    } else { /* info->format == kSecValidInfoFormatNto1 */
        if (info->noCACheck || (info->complete && !info->isOnList)) {
            return true;
        }
    }
    return false;
}

bool SecRVCHasRevokedValidInfo(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info) {
        return false;
    }
    SecValidInfoRef info = rvc->valid_info;
    /* either not present on an allowlist, or present on a blocklist */
    return (!info->isOnList && info->valid) || (info->isOnList && !info->valid);
}

void SecRVCSetValidDeterminedErrorResult(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    if (rvc->valid_info->overridable) {
        /* error is recoverable, treat certificate as untrusted */
        SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckGrayListedLeaf, rvc->certIX,
                                      kCFBooleanFalse, true);
        return;
    }
    /* error is fatal at this point */
    if (!SecRVCHasRevokedValidInfo(rvc) || rvc->valid_info->noCACheck) {
        /* result key should indicate blocked instead of revoked,
         * but result must be non-recoverable */
        SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckBlackListedLeaf, rvc->certIX,
                                      kCFBooleanFalse, true);
        return;
    }
    SInt32 reason = 0; /* unspecified, since the Valid db doesn't tell us */
    CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
    SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                  cfreason, true);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
    if (path) {
        SecCertificatePathVCSetRevocationReasonForCertificateAtIndex(path, rvc->certIX, cfreason);
    }
    CFReleaseNull(cfreason);
}

bool SecRVCRevocationChecked(SecRVCRef rvc) {
    return rvc->revocation_checked;
}

static void SecRVCProcessValidInfoResults(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
    SecValidInfoRef info = rvc->valid_info;

    bool definitive = SecRVCHasDefinitiveValidInfo(rvc);
    bool revoked = SecRVCHasRevokedValidInfo(rvc);

    /* set analytics */
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if (analytics) {
        if (revoked) {
            analytics->valid_status |= definitive ? TAValidDefinitelyRevoked : TAValidProbablyRevoked;
        } else {
            analytics->valid_status |= definitive ? TAValidDefinitelyOK : TAValidProbablyOK;
        }
        analytics->valid_require_ct |= info->requireCT;
        analytics->valid_known_intermediates_only |= info->knownOnly;
    }

    /* Handle no-ca cases */
    if (info->noCACheck) {
        bool allowed = (info->valid && info->complete && info->isOnList);
        if (revoked) {
            /* definitely revoked */
            SecRVCSetValidDeterminedErrorResult(rvc);
        } else if (allowed) {
            /* definitely not revoked (allowlisted) */
            SecCertificatePathVCSetIsAllowlisted(path, true);
            rvc->revocation_checked = true;
        }
        /* no-ca is definitive; no need to check further. */
        secdebug("validupdate", "rvc: definitely %s cert %" PRIdCFIndex,
                 (allowed) ? "allowed" : "revoked", rvc->certIX);
        rvc->done = true;
        return;
    }

    /* Handle policy constraints, if present. */
    SecRVCProcessValidPolicyConstraints(rvc);

    /* Handle date constraints, if present.
     * Note: a not-after date may set the CT requirement,
     * so check requireCT after this function is called. */
    SecRVCProcessValidDateConstraints(rvc);

    /* Set CT requirement on path, if present. */
    if (info->requireCT) {
        SecPathCTPolicy ctp = kSecPathCTRequired;
        if (info->overridable) {
            ctp = kSecPathCTRequiredOverridable;
        }
        SecCertificatePathVCSetRequiresCT(path, ctp);
    }

    /* Trigger OCSP for any non-definitive or revoked cases */
    if (!definitive || revoked) {
        info->checkOCSP = true;
    }

    if (info->checkOCSP) {
        CFIndex count = SecPathBuilderGetCertificateCount(rvc->builder);
        CFIndex issuerIX = rvc->certIX + 1;
        if (issuerIX >= count) {
            /* cannot perform a revocation check on the last cert in the
               chain, since we don't have its issuer. */
            return;
        }
        secdebug("validupdate", "rvc: %s%s cert %" PRIdCFIndex " (will check OCSP)",
                 (info->complete) ? "" : "possibly ", (info->valid) ? "allowed" : "revoked",
                 rvc->certIX);
        SecPathBuilderSetRevocationMethod(rvc->builder, kSecPolicyCheckRevocationAny);
        if (analytics) {
            /* Valid DB results caused us to do OCSP */
            analytics->valid_trigger_ocsp = true;
        }
    }
}

static bool SecRVCCheckValidInfoDatabase(SecRVCRef rvc) {
    /* Skip checking for OCSP Signer verification */
    if (SecPathBuilderGetPVCCount(rvc->builder) == 1) {
        SecPVCRef pvc = SecPathBuilderGetPVCAtIndex(rvc->builder, 0);
        if (!pvc) { return false; }
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0);
        CFStringRef policyName = (policy) ? SecPolicyGetName(policy) : NULL;
        if (policyName && CFEqual(policyName, CFSTR("OCSPSigner"))) {
            return false;
        }
    }

    /* Make sure revocation db info is up-to-date.
     * We don't care if the builder is allowed to access the network because
     * the network fetching does not block the trust evaluation. */
    SecRevocationDbCheckNextUpdate();

    /* Check whether we have valid db info for this cert,
     given the cert and its issuer */
    SecValidInfoRef info = NULL;
    CFIndex count = SecPathBuilderGetCertificateCount(rvc->builder);
    if (count) {
        bool isSelfSigned = false;
        SecCertificateRef cert = NULL;
        SecCertificateRef issuer = NULL;
        CFIndex issuerIX = rvc->certIX + 1;
        if (count > issuerIX) {
            issuer = SecPathBuilderGetCertificateAtIndex(rvc->builder, issuerIX);
        } else if (count == issuerIX) {
            CFIndex rootIX = SecCertificatePathVCSelfSignedIndex(SecPathBuilderGetPath(rvc->builder));
            if (rootIX == rvc->certIX) {
                issuer = SecPathBuilderGetCertificateAtIndex(rvc->builder, rootIX);
                isSelfSigned = true;
            }
        }
        cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
        if (!isSelfSigned) {
            /* skip revocation db check for self-signed certificates [33137065] */
            info = SecRevocationDbCopyMatching(cert, issuer);
        }
        SecValidInfoSetAnchor(info, SecPathBuilderGetCertificateAtIndex(rvc->builder, count-1));
    }
    if (info) {
        SecValidInfoRef old_info = rvc->valid_info;
        rvc->valid_info = info;
        if (old_info) {
            CFReleaseNull(old_info);
        }
        return true;
    }
    return false;
}

static void SecRVCCheckRevocationCaches(SecRVCRef rvc) {
    /* Don't check OCSP cache if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc) && (rvc->orvc->ocspRequest)) {
        secdebug("ocsp", "Checking cached responses for cert %ld", rvc->certIX);
        SecOCSPResponseRef response = NULL;
        if (SecPathBuilderGetCheckRevocationOnline(rvc->builder)) {
            CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
            response = SecOCSPCacheCopyMatchingWithMinInsertTime(rvc->orvc->ocspRequest, NULL, now - kSecOCSPResponseOnlineTTL);
        } else {
            response = SecOCSPCacheCopyMatching(rvc->orvc->ocspRequest, NULL);
        }
        SecORVCConsumeOCSPResponse(rvc->orvc, response, NULL_TIME, false, true);
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
        if (rvc->orvc->done && analytics) {
            /* We found a valid OCSP response in the cache */
            analytics->ocsp_cache_hit = true;
        }
    }
}

static void SecRVCUpdatePVC(SecRVCRef rvc) {
    SecRVCProcessValidInfoResults(rvc); /* restore the results we got from Valid */
    if (rvc->orvc) { SecORVCUpdatePVC(rvc->orvc); }
}

static void SecRVCSetFinishedWithoutNetwork(SecRVCRef rvc) {
    rvc->done = true;
    SecRVCUpdatePVC(rvc);
    (void)SecPathBuilderDecrementAsyncJobCount(rvc->builder);
}

static bool SecRVCFetchNext(SecRVCRef rvc) {
    bool OCSP_fetch_finished = true;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    /* Don't send OCSP request only if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc)) {
        SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
        SecCertificateRef cert = SecCertificatePathVCGetCertificateAtIndex(path, rvc->certIX);
        OCSP_fetch_finished = SecORVCBeginFetches(rvc->orvc, cert);
        if (analytics && !OCSP_fetch_finished) {
            /* We did a network OCSP fetch, set report appropriately */
            analytics->ocsp_network = true;
        }
    }
    if (OCSP_fetch_finished) {
        /* we didn't start an OCSP background job for this cert */
        (void)SecPathBuilderDecrementAsyncJobCount(rvc->builder);
    }
    return OCSP_fetch_finished;
}

/* The SecPathBuilder state machine calls SecPathBuilderCheckRevocation twice --
 * once in the ValidatePath state, and again in the ComputeDetails state. In the
 * ValidatePath state we've not yet run the path checks, so for callers who set
 * kSecRevocationCheckIfTrusted, we don't do any networking on that first call.
 * Here, if we've already done revocation before (so we're in ComputeDetails now),
 * we need to recheck (and enable networking) for trusted chains and
 * kSecRevocationCheckIfTrusted. Otherwise, we skip the checks to save on the processing
 * but update the PVCs with the revocation results from the last check. */
static bool SecRevocationDidCheckRevocation(SecPathBuilderRef builder, bool *first_check_done) {
    SecCertificatePathVCRef path = SecPathBuilderGetPath(builder);
    if (!SecCertificatePathVCIsRevocationDone(path)) {
        return false;
    }
    if (first_check_done) {
        *first_check_done = true;
    }

    SecPVCRef resultPVC = SecPathBuilderGetResultPVC(builder);
    bool recheck = false;
    if (SecPathBuilderGetCheckRevocationIfTrusted(builder) && SecPVCIsOkResult(resultPVC)) {
        recheck = true;
        secdebug("rvc", "Rechecking revocation because network now allowed");
    } else {
        secdebug("rvc", "Not rechecking revocation");
    }

    if (recheck) {
        // reset the RVCs for the second pass
        SecCertificatePathVCDeleteRVCs(path);
    } else {
        CFIndex certIX, certCount = SecPathBuilderGetCertificateCount(builder);
        for (certIX = 0; certIX < certCount; ++certIX) {
            SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
            if (rvc) {
                SecRVCUpdatePVC(rvc);
            }
        }
    }

    return !recheck;
}

static bool SecRevocationCanAccessNetwork(SecPathBuilderRef builder, bool first_check_done) {
    /* CheckRevocationIfTrusted overrides NoNetworkAccess for revocation */
    if (SecPathBuilderGetCheckRevocationIfTrusted(builder)) {
        if (first_check_done) {
            /* We're on the second pass. We need to now allow networking for revocation.
             * SecRevocationDidCheckRevocation takes care of not running a second pass
             * if the chain isn't trusted. */
            return true;
        } else {
            /* We're on the first pass of the revocation checks, where we aren't
             * supposed to do networking because we don't know if the chain
             * is trusted yet. */
            return false;
        }
    }
    return SecPathBuilderCanAccessNetwork(builder);
}

void SecPathBuilderCheckKnownIntermediateConstraints(SecPathBuilderRef builder) {
    SecCertificatePathVCRef path = SecPathBuilderGetPath(builder);
    if (!path) {
        return;
    }
    /* only perform this check once per path! */
    CFIndex certIX = kCFNotFound;
    if (SecCertificatePathVCCheckedIssuers(path)) {
        certIX = SecCertificatePathVCUnknownCAIndex(path);
        goto checkedIssuers;
    }
    /* check full path: start with anchor and decrement to leaf */
    bool parentConstrained = false;
    CFIndex certCount = SecPathBuilderGetCertificateCount(builder);
    for (certIX = certCount - 1; certIX >= 0; --certIX) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
        if (!rvc) {
            continue;
        }
        if (parentConstrained && !rvc->valid_info) {
            /* Parent had the known-only constraint, but our issuer is unknown.
               Bump index to point back at the issuer since it fails the constraint. */
            certIX++;
            break;
        }
        parentConstrained = (rvc->valid_info && rvc->valid_info->knownOnly);
        if (parentConstrained) {
            secdebug("validupdate", "Valid db found a known-intermediate constraint on %@ (index=%ld)",
                     rvc->valid_info->issuerHash, certIX+1);
            if (certIX == 0) {
                /* check special case: unknown constrained CA in leaf position */
                SecCertificateRef cert = SecCertificatePathVCGetCertificateAtIndex(path, certIX);
                if (cert && SecCertificateIsCA(cert) && !SecRevocationDbContainsIssuer(cert)) {
                    /* leaf is a CA which violates the constraint */
                    break;
                }
            }
        }
    }
    /* At this point, certIX will either be -1, indicating no CA was found
       which failed a known-intermediates-only constraint on its parent, or it
       will be the index of the first unknown CA which fails the constraint. */
    if (certIX >= 0) {
        secnotice("validupdate", "CA at index %ld violates known-intermediate constraint", certIX);
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
        if (analytics) {
            analytics->valid_unknown_intermediate = true;
        }
    }
    SecCertificatePathVCSetUnknownCAIndex(path, certIX);
    SecCertificatePathVCSetCheckedIssuers(path, true);

checkedIssuers:
    if (certIX >= 0) {
        /* Error is set on CA certificate which failed the constraint. */
        SecRVCSetValidDeterminedErrorResult(SecCertificatePathVCGetRVCAtIndex(path, certIX));
    }
}

bool SecPathBuilderCheckRevocation(SecPathBuilderRef builder) {
    secdebug("rvc", "checking revocation");
    CFIndex certIX, certCount = SecPathBuilderGetCertificateCount(builder);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(builder);
    if (certCount <= 1) {
        /* Can't verify without an issuer; we're done */
        return true;
    }

    bool first_check_done = false;
    if (SecRevocationDidCheckRevocation(builder, &first_check_done)) {
        return true;
    }

    /* Setup things so we check revocation status of all certs. */
    SecCertificatePathVCAllocateRVCs(path, certCount);

    /* Note that if we are multi threaded and a job completes after it
     is started but before we return from this function, we don't want
     a callback to decrement asyncJobCount to zero before we finish issuing
     all the jobs. To avoid this we pretend we issued certCount async jobs,
     and decrement pvc->asyncJobCount for each cert that we don't start a
     background fetch for. We include the root, even though we'll never start
     an async job for it so that we count all active threads for this eval. */
    SecPathBuilderSetAsyncJobCount(builder, (unsigned int)(certCount));

    /* Loop though certificates again and issue an ocsp fetch if the
     revocation status checking isn't done yet (and we have an issuer!) */
    for (certIX = 0; certIX < certCount; ++certIX) {
        secdebug("rvc", "checking revocation for cert: %ld", certIX);
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
        if (!rvc) {
            continue;
        }

        SecRVCInit(rvc, builder, certIX);

        /* RFC 6960: id-pkix-ocsp-nocheck extension says that we shouldn't check revocation. */
        if (SecCertificateHasOCSPNoCheckMarkerExtension(SecCertificatePathVCGetCertificateAtIndex(path, certIX)))
        {
            secdebug("rvc", "skipping revocation checks for no-check cert: %ld", certIX);
            TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
            if (analytics) {
                /* This certificate has OCSP No-Check, so add to reporting analytics */
                analytics->ocsp_no_check = true;
            }
            SecRVCSetFinishedWithoutNetwork(rvc);
        }

        if (rvc->done) {
            continue;
        }

#if !TARGET_OS_BRIDGE
        /* Check valid database first (separate from OCSP response cache) */
        if (SecRVCCheckValidInfoDatabase(rvc)) {
            SecRVCProcessValidInfoResults(rvc);
        }
#endif
        /* Any other revocation method requires an issuer certificate to verify the response;
         * skip the last cert in the chain since it doesn't have one. */
        if (certIX + 1 >= certCount) {
            continue;
        }

        /* Ignore stapled OCSP responses only if CRLs are enabled and the
         * policy specifically requested CRLs only. */
        if (SecRVCShouldCheckOCSP(rvc)) {
            /*  If we have any OCSP stapled responses, check those first */
            SecORVCProcessStapledResponses(rvc->orvc);
        }

#if TARGET_OS_BRIDGE
        /* The bridge has no writeable storage and no network. Nothing else we can
         * do here. */
        SecRVCSetFinishedWithoutNetwork(rvc);
        continue;
#else // !TARGET_OS_BRIDGE
        /* Then check the caches for revocation results. */
        SecRVCCheckRevocationCaches(rvc);

        /* The check is done if we found cached responses from either method. */
        if (rvc->done || rvc->orvc->done) {
            secdebug("rvc", "found cached response for cert: %ld", certIX);
            SecRVCSetFinishedWithoutNetwork(rvc);
            continue;
        }

        /* If we got a cached response that is no longer valid (which can only be true for
         * revoked responses), let's try to get a fresher response even if no one asked.
         * This check resolves unrevocation events after the nextUpdate time. */
        bool old_cached_response = (!rvc->done && rvc->orvc->ocspResponse);

        /* Check whether CA revocation additions match an issuer above us in this path.
         * If so, we will attempt revocation checking below. */
        bool has_ca_revocation = (certIX < SecCertificatePathVCIndexOfCAWithRevocationAdditions(path));

        /* If the cert is EV or if revocation checking was explicitly enabled, attempt to fire off an
         async http request for this cert's revocation status, unless we already successfully checked
         the revocation status of this cert based on the cache or stapled responses.  */
        bool allow_fetch = SecRevocationCanAccessNetwork(builder, first_check_done) &&
            (SecCertificatePathVCIsEV(path) || SecCertificatePathVCIsOptionallyEV(path) ||
             SecPathBuilderGetRevocationMethod(builder) || old_cached_response || has_ca_revocation);
        if (rvc->done || !allow_fetch) {
            /* We got a cache hit or we aren't allowed to access the network */
            SecRVCUpdatePVC(rvc);
            /* We didn't really start any background jobs for this cert. */
            (void)SecPathBuilderDecrementAsyncJobCount(builder);
        } else {
            (void)SecRVCFetchNext(rvc);
        }
#endif // !TARGET_OS_BRIDGE
    }

    /* Return false if there are still async jobs running. */
    /* builder->asyncJobCount is atomic, so we know that if the job count is 0, all other
     * threads are finished. If the job count is > 0, other threads will decrement the job
     * count and SecPathBuilderStep to crank the state machine when the job count is 0. */
    return (SecPathBuilderDecrementAsyncJobCount(builder) == 0);
}

CFAbsoluteTime SecRVCGetEarliestNextUpdate(SecRVCRef rvc) {
    CFAbsoluteTime enu = NULL_TIME;
    if (!rvc || !rvc->orvc) { return enu; }
    enu = rvc->orvc->nextUpdate;
    return enu;
}

CFAbsoluteTime SecRVCGetLatestThisUpdate(SecRVCRef rvc) {
    if (!rvc || !rvc->orvc) { return -DBL_MAX; }
    return rvc->orvc->thisUpdate;
}
