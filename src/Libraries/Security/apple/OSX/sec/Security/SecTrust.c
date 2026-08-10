/*
 * Copyright (c) 2006-2018 Apple Inc. All Rights Reserved.
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
 * SecTrust.c - CoreFoundation based certificate trust evaluator
 *
 */

#include <Security/SecTrustPriv.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecTrustStatusCodes.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyCerts.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecuritydXPC.h>
#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecFrameworkStrings.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFError.h>
#include <AssertMacros.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <os/activity.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <utilities/der_plist.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecXPCError.h>

#include "SecRSAKey.h"
#include <libDER/oids.h>

#include <ipc/securityd_client.h>

#include "trust/trustd/SecTrustServer.h"

#pragma clang diagnostic ignored "-Wformat=2"

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecCertificateDetailSHA1Digest, "SHA1Digest");
SEC_CONST_DECL (kSecCertificateDetailStatusCodes, "StatusCodes");
SEC_CONST_DECL (kSecCertificateExceptionResetCount, "ExceptionResetCount");

SEC_CONST_DECL (kSecTrustInfoExtendedValidationKey, "ExtendedValidation");
SEC_CONST_DECL (kSecTrustInfoCompanyNameKey, "CompanyName");
SEC_CONST_DECL (kSecTrustInfoRevocationKey, "Revocation");
SEC_CONST_DECL (kSecTrustInfoRevocationValidUntilKey, "RevocationValidUntil");
SEC_CONST_DECL (kSecTrustInfoCertificateTransparencyKey, "CertificateTransparency");

/* This is the "real" trust validity date which includes all inputs. */
SEC_CONST_DECL (kSecTrustInfoResultNotBefore, "TrustResultNotBefore");
SEC_CONST_DECL (kSecTrustInfoResultNotAfter, "TrustResultNotAfter");

/* Public trust result constants */
SEC_CONST_DECL (kSecTrustEvaluationDate, "TrustEvaluationDate");
SEC_CONST_DECL (kSecTrustExtendedValidation, "TrustExtendedValidation");
SEC_CONST_DECL (kSecTrustOrganizationName, "Organization");
SEC_CONST_DECL (kSecTrustResultValue, "TrustResultValue");
SEC_CONST_DECL (kSecTrustRevocationChecked, "TrustRevocationChecked");
SEC_CONST_DECL (kSecTrustRevocationReason, "TrustRevocationReason");
SEC_CONST_DECL (kSecTrustResultDetails, "TrustResultDetails");
SEC_CONST_DECL (kSecTrustCertificateTransparency, "TrustCertificateTransparency");
SEC_CONST_DECL (kSecTrustCertificateTransparencyWhiteList, "TrustCertificateTransparencyWhiteList");

/* This value is actually incorrect as this constant only refers to the revocation expiration
 * not the trust expiration. But it's API. */
SEC_CONST_DECL (kSecTrustRevocationValidUntilDate, "TrustExpirationDate");

#pragma mark -
#pragma mark SecTrust

/********************************************************
 ****************** SecTrust object *********************
 ********************************************************/
struct __SecTrust {
    CFRuntimeBase           _base;
    CFArrayRef              _certificates;
    CFArrayRef              _anchors;
    CFTypeRef               _policies;
    CFArrayRef              _responses;
    CFArrayRef              _SCTs;
    CFArrayRef              _trustedLogs;
    CFDateRef               _verifyDate;
    CFArrayRef              _chain;
    SecKeyRef               _publicKey;
    CFArrayRef              _details;
    CFDictionaryRef         _info;
    CFArrayRef              _exceptions;

    /* Note that a value of kSecTrustResultInvalid (0)
     * indicates the trust must be (re)evaluated; any
     * functions which modify trust parameters in a way
     * that would invalidate the current result must set
     * this value back to kSecTrustResultInvalid.
     */
    SecTrustResultType      _trustResult;

    /* If true we don't trust any anchors other than the ones in _anchors. */
    bool                    _anchorsOnly;
    /* If false we shouldn't search keychains for parents or anchors. */
    bool                    _keychainsAllowed;

    /* Data blobs for legacy CSSM_TP_APPLE_EVIDENCE_INFO structure,
     * to support callers of SecTrustGetResult on OS X. Since fields of
     * one structure contain pointers into the other, these cannot be
     * serialized; if a SecTrust is being serialized or copied, these values
     * should just be initialized to NULL in the copy and built when needed. */
    void*                   _legacy_info_array;
    void*                   _legacy_status_array;

    /* Dispatch queue for thread-safety */
    dispatch_queue_t        _trustQueue;

    /* === IMPORTANT! ===
     * Any change to this structure definition
     * must also be made in the TSecTrust structure,
     * located in SecTrust.cpp. To avoid problems,
     * new fields should always be appended at the
     * end of the structure.
     */
};

/* Forward declarations of static functions. */
static OSStatus SecTrustEvaluateIfNecessary(SecTrustRef trust);
static void SecTrustEvaluateIfNecessaryFastAsync(SecTrustRef trust,
												 dispatch_queue_t queue,
												 void (^handler)(OSStatus status));

/* Static functions. */
static CFStringRef SecTrustCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecTrustRef trust = (SecTrustRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<SecTrustRef: %p>"), trust);
}

static void SecTrustDestroy(CFTypeRef cf) {
    SecTrustRef trust = (SecTrustRef)cf;

    dispatch_release_null(trust->_trustQueue);
    CFReleaseNull(trust->_certificates);
    CFReleaseNull(trust->_policies);
    CFReleaseNull(trust->_responses);
    CFReleaseNull(trust->_SCTs);
    CFReleaseNull(trust->_trustedLogs);
    CFReleaseNull(trust->_verifyDate);
    CFReleaseNull(trust->_anchors);
    CFReleaseNull(trust->_chain);
    CFReleaseNull(trust->_publicKey);
    CFReleaseNull(trust->_details);
    CFReleaseNull(trust->_info);
    CFReleaseNull(trust->_exceptions);

    if (trust->_legacy_info_array) {
        free(trust->_legacy_info_array);
    }
    if (trust->_legacy_status_array) {
        free(trust->_legacy_status_array);
    }
}

/* Public API functions. */
CFGiblisFor(SecTrust)

OSStatus SecTrustCreateWithCertificates(CFTypeRef certificates,
    CFTypeRef policies, SecTrustRef *trust) {
    OSStatus status = errSecParam;
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFArrayRef l_certs = NULL, l_policies = NULL;
    SecTrustRef result = NULL;
    dispatch_queue_t queue = NULL;

    check(certificates);
    check(trust);
    CFTypeID certType = CFGetTypeID(certificates);
    if (certType == CFArrayGetTypeID()) {
        CFIndex idx, count = CFArrayGetCount(certificates);
        /* We need at least 1 certificate. */
        require_quiet(count > 0, errOut);
        l_certs = (CFArrayRef) CFArrayCreateMutable(allocator, count,
            &kCFTypeArrayCallBacks);
        if (!l_certs) {
            status = errSecAllocate;
            goto errOut;
        }
        for (idx = 0; idx < count; idx++) {
            CFTypeRef val = CFArrayGetValueAtIndex(certificates, idx);
            if (val && CFGetTypeID(val) == SecCertificateGetTypeID()) {
                CFArrayAppendValue((CFMutableArrayRef)l_certs, val);
            } else {
                secerror("BUG IN SECURITY CLIENT: certificates array contains non-certificate value");
            }
        }
        require_quiet(count == CFArrayGetCount(l_certs), errOut);
    } else if (certType == SecCertificateGetTypeID()) {
        l_certs = CFArrayCreate(allocator, &certificates, 1,
            &kCFTypeArrayCallBacks);
    } else {
        secerror("BUG IN SECURITY CLIENT: certificates contains unsupported value type");
        goto errOut;
    }
    if (!l_certs) {
        status = errSecAllocate;
        goto errOut;
    }

    if (!policies) {
        CFTypeRef policy = SecPolicyCreateBasicX509();
        l_policies = CFArrayCreate(allocator, &policy, 1,
            &kCFTypeArrayCallBacks);
        CFRelease(policy);
    } else if (CFGetTypeID(policies) == CFArrayGetTypeID()) {
        CFIndex idx, count = CFArrayGetCount(policies);
        /* We need at least 1 policy. */
        require_quiet(count > 0, errOut);
        l_policies = (CFArrayRef) CFArrayCreateMutable(allocator, count,
            &kCFTypeArrayCallBacks);
        if (!l_policies) {
            status = errSecAllocate;
            goto errOut;
        }
        for (idx = 0; idx < count; idx++) {
            CFTypeRef val = CFArrayGetValueAtIndex(policies, idx);
            if (val && CFGetTypeID(val) == SecPolicyGetTypeID()) {
                CFArrayAppendValue((CFMutableArrayRef)l_policies, val);
            } else {
                secerror("BUG IN SECURITY CLIENT: policies array contains non-policy value");
            }
        }
        require_quiet(count == CFArrayGetCount(l_policies), errOut);
    } else if (CFGetTypeID(policies) == SecPolicyGetTypeID()) {
        l_policies = CFArrayCreate(allocator, &policies, 1,
            &kCFTypeArrayCallBacks);
    } else {
        secerror("BUG IN SECURITY CLIENT: policies contains unsupported value type");
        goto errOut;
    }
    if (!l_policies) {
        status = errSecAllocate;
        goto errOut;
    }

    queue = dispatch_queue_create("trust", DISPATCH_QUEUE_SERIAL);
    if (!queue) {
        status = errSecAllocate;
        goto errOut;
    }

    CFIndex size = sizeof(struct __SecTrust);
    require_quiet(result = (SecTrustRef)_CFRuntimeCreateInstance(allocator,
        SecTrustGetTypeID(), size - sizeof(CFRuntimeBase), 0), errOut);
    memset((char*)result + sizeof(result->_base), 0,
        sizeof(*result) - sizeof(result->_base));
    status = errSecSuccess;

errOut:
    if (status) {
        CFReleaseSafe(result);
        CFReleaseSafe(l_certs);
        CFReleaseSafe(l_policies);
        dispatch_release_null(queue);
    } else {
        result->_certificates = l_certs;
        result->_policies = l_policies;
        result->_keychainsAllowed = true;
        result->_trustQueue = queue;
        if (trust)
            *trust = result;
        else
            CFReleaseSafe(result);
    }
    return status;
}

OSStatus SecTrustCopyInputCertificates(SecTrustRef trust, CFArrayRef *certificates) {
    if (!trust || !certificates) {
        return errSecParam;
    }
    __block CFArrayRef certArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        certArray = CFArrayCreateCopy(NULL, trust->_certificates);
    });
    if (!certArray) {
        return errSecAllocate;
    }
    *certificates = certArray;
    return errSecSuccess;
}

OSStatus SecTrustAddToInputCertificates(SecTrustRef trust, CFTypeRef certificates) {
    if (!trust || !certificates) {
        return errSecParam;
    }
    __block CFMutableArrayRef newCertificates = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        newCertificates = CFArrayCreateMutableCopy(NULL, 0, trust->_certificates);
    });

    if (isArray(certificates)) {
        CFIndex originalCount = CFArrayGetCount(newCertificates);
        CFArrayForEach(certificates, ^(const void *value) {
            if (CFGetTypeID(value) == SecCertificateGetTypeID()) {
                CFArrayAppendValue(newCertificates, value);
            } else {
                secerror("BUG IN SECURITY CLIENT: certificates array contains non-certificate value");
            }
        });
        if (CFArrayGetCount(newCertificates) != originalCount + CFArrayGetCount(certificates)) {
            CFReleaseNull(newCertificates);
            return errSecParam;
        }
    } else if (CFGetTypeID(certificates) == SecCertificateGetTypeID()) {
        CFArrayAppendValue(newCertificates, certificates);
    } else {
        secerror("BUG IN SECURITY CLIENT: certificates contains unsupported value type");
        CFReleaseNull(newCertificates);
        return errSecParam;
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseNull(trust->_certificates);
        trust->_certificates = (CFArrayRef)newCertificates;
    });

    return errSecSuccess;
}

void SecTrustSetNeedsEvaluation(SecTrustRef trust) {
    check(trust);
    if (trust) {
        dispatch_sync(trust->_trustQueue, ^{
            trust->_trustResult = kSecTrustResultInvalid;
        });
    }
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust,
    Boolean anchorCertificatesOnly) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        trust->_anchorsOnly = anchorCertificatesOnly;
    });

    return errSecSuccess;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust,
    CFArrayRef anchorCertificates) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    __block CFArrayRef anchorsArray = NULL;
    if (anchorCertificates) {
        if (CFGetTypeID(anchorCertificates) == CFArrayGetTypeID()) {
            CFIndex idx, count = CFArrayGetCount(anchorCertificates);
            anchorsArray = (CFArrayRef) CFArrayCreateMutable(NULL, count,
                &kCFTypeArrayCallBacks);
            if (!anchorsArray) {
                return errSecAllocate;
            }
            for (idx = 0; idx < count; idx++) {
                CFTypeRef val = CFArrayGetValueAtIndex(anchorCertificates, idx);
                if (val && CFGetTypeID(val) == SecCertificateGetTypeID()) {
                    CFArrayAppendValue((CFMutableArrayRef)anchorsArray, val);
                } else {
                    secerror("BUG IN SECURITY CLIENT: anchorCertificates array contains non-certificate value");
                }
            }
            if (count != CFArrayGetCount(anchorsArray)) {
                CFReleaseSafe(anchorsArray);
                return errSecParam;
            }
        } else {
            return errSecParam;
        }
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseSafe(trust->_anchors);
        trust->_anchors = anchorsArray;
        trust->_anchorsOnly = (anchorCertificates != NULL);
    });

    return errSecSuccess;
}

OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust,
    CFArrayRef *anchors) {
    if (!trust|| !anchors) {
        return errSecParam;
    }
    __block CFArrayRef anchorsArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_anchors) {
            anchorsArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_anchors);
        }
    });

    *anchors = anchorsArray;
    return errSecSuccess;
}

// Return false on error, true on success.
static bool to_bool_error_request(enum SecXPCOperation op, CFErrorRef *error) {
    __block bool result = false;
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        result = !(blockError && *blockError);
        return true;
    });
    return result;
}

Boolean SecTrustFlushResponseCache(CFErrorRef *error) {
    CFErrorRef localError = NULL;
    os_activity_t activity = os_activity_create("SecTrustFlushResponseCache", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    bool result = TRUSTD_XPC(sec_ocsp_cache_flush, to_bool_error_request, &localError);
    os_release(activity);
    if (error) {
        *error = localError;
    } else if (localError) {
        CFRelease(localError);
    }
    return result;
}

OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, CFTypeRef responseData) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    __block CFArrayRef responseArray = NULL;
    if (responseData) {
        if (CFGetTypeID(responseData) == CFArrayGetTypeID()) {
            CFIndex idx, count = CFArrayGetCount(responseData);
            responseArray = (CFArrayRef) CFArrayCreateMutable(NULL, count,
                &kCFTypeArrayCallBacks);
            if (!responseArray) {
                return errSecAllocate;
            }
            for (idx = 0; idx < count; idx++) {
                CFTypeRef val = CFArrayGetValueAtIndex(responseData, idx);
                if (isData(val)) {
                    CFArrayAppendValue((CFMutableArrayRef)responseArray, val);
                } else {
                    secerror("BUG IN SECURITY CLIENT: responseData array contains non-data value");
                }
            }
            if (count != CFArrayGetCount(responseArray)) {
                CFReleaseSafe(responseArray);
                return errSecParam;
            }
        } else if (CFGetTypeID(responseData) == CFDataGetTypeID()) {
            responseArray = CFArrayCreate(kCFAllocatorDefault, &responseData, 1,
                                          &kCFTypeArrayCallBacks);
        } else {
            secerror("BUG IN SECURITY CLIENT: responseData contains unsupported value type");
            return errSecParam;
        }
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseSafe(trust->_responses);
        trust->_responses = responseArray;
    });

    return errSecSuccess;
}

OSStatus SecTrustSetSignedCertificateTimestamps(SecTrustRef trust, CFArrayRef sctArray) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_SCTs, sctArray);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetTrustedLogs(SecTrustRef trust, CFArrayRef trustedLogs) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_trustedLogs, trustedLogs);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate) {
    if (!trust || !isDate(verifyDate)) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_verifyDate, verifyDate);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef newPolicies) {
    if (!trust || !newPolicies) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    check(newPolicies);

    __block CFArrayRef policyArray = NULL;
    if (CFGetTypeID(newPolicies) == CFArrayGetTypeID()) {
        CFIndex idx, count = CFArrayGetCount(newPolicies);
        /* We need at least 1 policy. */
        if (!(count > 0)) {
            return errSecParam;
        }
        policyArray = (CFArrayRef) CFArrayCreateMutable(NULL, count,
            &kCFTypeArrayCallBacks);
        if (!policyArray) {
            return errSecAllocate;
        }
        for (idx = 0; idx < count; idx++) {
            CFTypeRef val = CFArrayGetValueAtIndex(newPolicies, idx);
            if (val && CFGetTypeID(val) == SecPolicyGetTypeID()) {
                CFArrayAppendValue((CFMutableArrayRef)policyArray, val);
            } else {
                secerror("BUG IN SECURITY CLIENT: newPolicies array contains non-policy value");
            }
        }
        if (count != CFArrayGetCount(policyArray)) {
            CFReleaseSafe(policyArray);
            return errSecParam;
        }
    } else if (CFGetTypeID(newPolicies) == SecPolicyGetTypeID()) {
        policyArray = CFArrayCreate(kCFAllocatorDefault, &newPolicies, 1,
            &kCFTypeArrayCallBacks);
    } else {
        secerror("BUG IN SECURITY CLIENT: newPolicies contains unsupported value type");
        return errSecParam;
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseSafe(trust->_policies);
        trust->_policies = policyArray;
    });

    return errSecSuccess;
}

OSStatus SecTrustSetPinningPolicyName(SecTrustRef trust, CFStringRef policyName) {
    if (!trust || !policyName) {
        return errSecParam;
    }

    SecTrustSetNeedsEvaluation(trust);

    dispatch_sync(trust->_trustQueue, ^{
        CFArrayForEach(trust->_policies, ^(const void *value) {
            SecPolicyRef policy = (SecPolicyRef)value;
            SecPolicySetName(policy, policyName);
            secinfo("SecPinningDb", "Set %@ as name on all policies", policyName);
        });
    });
    return errSecSuccess;
}

OSStatus SecTrustSetKeychainsAllowed(SecTrustRef trust, Boolean allowed) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        trust->_keychainsAllowed = allowed;
    });

    return errSecSuccess;
}

OSStatus SecTrustGetKeychainsAllowed(SecTrustRef trust, Boolean *allowed) {
    if (!trust || !allowed) {
        return errSecParam;
    }
    *allowed = trust->_keychainsAllowed;

    return errSecSuccess;
}

OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef *policies) {
	if (!trust|| !policies) {
		return errSecParam;
	}
    __block CFArrayRef policyArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policyArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_policies);
    });
	if (!policyArray) {
		return errSecAllocate;
	}
	*policies = policyArray;
	return errSecSuccess;
}

static OSStatus SecTrustSetOptionInPolicies(CFArrayRef policies, CFStringRef key, CFTypeRef value) {
    OSStatus status = errSecSuccess;
    require_action(policies && CFGetTypeID(policies) == CFArrayGetTypeID(), out, status = errSecInternal);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = NULL;
        require_action_quiet(policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i), out, status = errSecInternal);
        CFMutableDictionaryRef options = NULL;
        require_action_quiet(options = CFDictionaryCreateMutableCopy(NULL, 0, policy->_options), out, status = errSecAllocate);
        CFDictionarySetValue(options, key, value);
        CFReleaseNull(policy->_options);
        policy->_options = options;
    }
out:
    return status;
}

static OSStatus SecTrustRemoveOptionInPolicies(CFArrayRef policies, CFStringRef key) {
    OSStatus status = errSecSuccess;
    require_action(policies && CFGetTypeID(policies) == CFArrayGetTypeID(), out, status = errSecInternal);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = NULL;
        require_action_quiet(policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i), out, status = errSecInternal);
        if (CFDictionaryGetValue(policy->_options, key)) {
            CFMutableDictionaryRef options = NULL;
            require_action_quiet(options = CFDictionaryCreateMutableCopy(NULL, 0, policy->_options), out, status = errSecAllocate);
            CFDictionaryRemoveValue(options, key);
            CFReleaseNull(policy->_options);
            policy->_options = options;
        }
    }
out:
    return status;
}

static CF_RETURNS_RETAINED CFArrayRef SecTrustCopyOptionsFromPolicies(CFArrayRef policies, CFStringRef key) {
    CFMutableArrayRef foundValues = NULL;
    foundValues = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i);
        CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
        if (value) {
            CFArrayAppendValue(foundValues, value);
        }
    }
    if (!CFArrayGetCount(foundValues)) {
        CFReleaseNull(foundValues);
        return NULL;
    }
    else {
        return foundValues;
    }
}

/* The only effective way to disable network fetch is within the policy options:
 * presence of the kSecPolicyCheckNoNetworkAccess key in any of the policies
 * will prevent network access for fetching.
 * The current SecTrustServer implementation doesn't distinguish between network
 * access for revocation and network access for fetching.
 */
OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch) {
	if (!trust) {
		return errSecParam;
	}
    __block OSStatus status = errSecSuccess;
    __block bool oldAllowFetch = true;
    dispatch_sync(trust->_trustQueue, ^{
        CFArrayRef foundValues = SecTrustCopyOptionsFromPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess);
        if (foundValues) {
            /* We only disable fetch if there is a policy option set for NoNetworkAccess, so the old fetch
             * status was false (don't allow network) if we find this option set in the policies. */
            oldAllowFetch = false;
            CFReleaseNull(foundValues);
        }
        if (!allowFetch) {
            status = SecTrustSetOptionInPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);
        } else {
            status = SecTrustRemoveOptionInPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess);
        }
    });
    /* If we switched from NoNetworkAccess to allowing access, we need to re-run the trust evaluation */
    if (allowFetch && !oldAllowFetch) {
        SecTrustSetNeedsEvaluation(trust);
    }
	return status;
}

OSStatus SecTrustGetNetworkFetchAllowed(SecTrustRef trust, Boolean *allowFetch) {
	if (!trust || !allowFetch) {
		return errSecParam;
	}
    __block CFArrayRef foundValues = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        foundValues = SecTrustCopyOptionsFromPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess);
    });
    if (foundValues) {
        CFArrayForEach(foundValues, ^(CFTypeRef value) {
            if (isBoolean(value)) {
                *allowFetch = !CFBooleanGetValue(value);
            }
        });
    } else {
        *allowFetch = true;
    }
    CFReleaseNull(foundValues);
	return errSecSuccess;
}

OSStatus SecTrustSetPinningException(SecTrustRef trust) {
    if (!trust) { return errSecParam; }
    __block OSStatus status = errSecSuccess;
    dispatch_sync(trust->_trustQueue, ^{
        status = SecTrustRemoveOptionInPolicies(trust->_policies, kSecPolicyCheckPinningRequired);
    });
    return status;
}

CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust) {
    __block CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
    if (!trust) {
        return verifyTime;
    }
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_verifyDate) {
            verifyTime = CFDateGetAbsoluteTime(trust->_verifyDate);
        } else {
            trust->_verifyDate = CFDateCreate(CFGetAllocator(trust), verifyTime);
        }
    });

    return verifyTime;
}

CFArrayRef SecTrustGetDetails(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    return trust->_details;
}

OSStatus SecTrustGetTrustResult(SecTrustRef trust,
    SecTrustResultType *result) {
    if (!trust || !result) {
        return errSecParam;
    }
    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        *result = trust->_trustResult;
    });
    return errSecSuccess;
}

static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix) {
    CFDictionaryRef exception = NULL;
    __block CFArrayRef exceptions = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        exceptions = CFRetainSafe(trust->_exceptions);
    });
    if (!exceptions || ix >= CFArrayGetCount(exceptions)) {
        goto out;
    }

    SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    if (!certificate) {
        goto out;
    }

    exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, ix);
    if (CFGetTypeID(exception) != CFDictionaryGetTypeID()) {
        exception = NULL;
        goto out;
    }

    /* If the exception contains the current certificates sha1Digest in the
       kSecCertificateDetailSHA1Digest key then we use it otherwise we ignore it. */
    CFDataRef sha1Digest = SecCertificateGetSHA1Digest(certificate);
    CFTypeRef digestValue = CFDictionaryGetValue(exception, kSecCertificateDetailSHA1Digest);
    if (!digestValue || !CFEqual(sha1Digest, digestValue))
        exception = NULL;

out:
    CFReleaseSafe(exceptions);
    return exception;
}

CFArrayRef SecTrustCopyFilteredDetails(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });

    /* Done automatically by the Policy Server with SecPVCIsExceptedError */
    return details;
}

Boolean SecTrustIsExpiredOnly(SecTrustRef trust) {
    /* Returns true if one or more certificates in the chain have expired,
     * expiration is an error (i.e. is not covered by existing trust settings),
     * and it is the only error encountered.
     * Returns false if the certificate is valid, or if the trust chain has
     * other errors beside expiration.
     */
    Boolean result = false;
    Boolean foundExpired = false;
    CFArrayRef details = SecTrustCopyFilteredDetails(trust);
    require(details != NULL, out);

    CFIndex ix, pathLength = CFArrayGetCount(details);
    for (ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex count = (detail) ? CFDictionaryGetCount(detail) : 0;
        require(count <= 1, out);
        if (count) {
            CFBooleanRef valid = (CFBooleanRef)CFDictionaryGetValue(detail, kSecPolicyCheckTemporalValidity);
            require(isBoolean(valid) && CFEqual(valid, kCFBooleanFalse), out);
            foundExpired = true;
        }
    }
    result = foundExpired;
out:
    CFReleaseSafe(details);
    return result;
}

#if TARGET_OS_IPHONE
static CFArrayRef SecTrustCreatePolicyAnchorsArray(const UInt8* certData, CFIndex certLength)
{
    CFArrayRef array = NULL;
    CFAllocatorRef allocator = kCFAllocatorDefault;
    SecCertificateRef cert = SecCertificateCreateWithBytes(allocator, certData, certLength);
    if (cert) {
        array = CFArrayCreate(allocator, (const void **)&cert, 1, &kCFTypeArrayCallBacks);
        CFReleaseSafe(cert);
    }
    return array;
}
#endif

static void SecTrustAddPolicyAnchors(SecTrustRef trust)
{
    /* Provide anchor certificates specifically required by certain policies.
       This is used to evaluate test policies where the anchor is not provided
       in the root store and may not be able to be supplied by the caller.
     */
    if (!trust) { return; }
    __block CFArrayRef policies = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policies = CFRetain(trust->_policies);
    });
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
        if (policy) {
            #if TARGET_OS_IPHONE
            if (CFEqual(policy->_oid, kSecPolicyAppleTestSMPEncryption)) {
                __block CFArrayRef policyAnchors = SecTrustCreatePolicyAnchorsArray(_SEC_TestAppleRootCAECC, sizeof(_SEC_TestAppleRootCAECC));
                dispatch_sync(trust->_trustQueue, ^{
                    CFReleaseSafe(trust->_anchors);
                    trust->_anchors = policyAnchors;
                });
                trust->_anchorsOnly = true;
                break;
            }
            #endif
        }
    }
    CFReleaseSafe(policies);
}

// uncomment for verbose debug logging (debug builds only)
//#define CERT_TRUST_DUMP 1

#if CERT_TRUST_DUMP
static void sectrustlog(int priority, const char *format, ...)
{
#ifndef NDEBUG
	// log everything
#else
	if (priority < LOG_NOTICE) // log warnings and errors
#endif
	{
		va_list list;
		va_start(list, format);
		vsyslog(priority, format, list);
		va_end(list);
	}
}

static void sectrustshow(CFTypeRef obj, const char *context)
{
#ifndef NDEBUG
	CFStringRef desc = CFCopyDescription(obj);
	if (!desc) return;

	CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(desc), kCFStringEncodingUTF8) + 1;
	char* buffer = (char*) malloc(length);
	if (buffer) {
		Boolean converted = CFStringGetCString(desc, buffer, length, kCFStringEncodingUTF8);
		if (converted) {
			const char *prefix = (context) ? context : "";
			const char *separator = (context) ? " " : "";
			sectrustlog(LOG_NOTICE, "%s%s%s", prefix, separator, buffer);
		}
		free(buffer);
	}
	CFRelease(desc);
#endif
}

static void cert_trust_dump(SecTrustRef trust) {
	SecCertificateRef leaf = SecTrustGetCertificateAtIndex(trust, 0);
	CFStringRef name = (leaf) ? SecCertificateCopySubjectSummary(leaf) : NULL;
	secerror("leaf \"%@\"", name);
	secerror(": result = %d", (int) trust->_trustResult);
	if (trust->_chain) {
		CFIndex ix, count = CFArrayGetCount(trust->_chain);
		CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
		for (ix = 0; ix < count; ix++) {
			SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_chain, ix);
			if (cert) {
				CFArrayAppendValue(chain, cert);
			}
		}
		sectrustshow(chain, "chain:");
		CFReleaseSafe(chain);
	}
	secerror(": %ld certificates, %ld anchors, %ld policies,  %ld details",
		(trust->_certificates) ? (long)CFArrayGetCount(trust->_certificates) : 0,
		(trust->_anchors) ? (long)CFArrayGetCount(trust->_anchors) : 0,
		(trust->_policies) ? (long)CFArrayGetCount(trust->_policies) : 0,
		(trust->_details) ? (long)CFArrayGetCount(trust->_details) : 0);

	sectrustshow(trust->_verifyDate, "verify date:");
	sectrustshow(trust->_certificates, "certificates:");
	sectrustshow(trust->_anchors, "anchors:");
	sectrustshow(trust->_policies, "policies:");
	sectrustshow(trust->_details, "details:");
	sectrustshow(trust->_info, "info:");

	CFReleaseSafe(name);
}
#else
static void cert_trust_dump(SecTrustRef trust) {}
#endif

static void SecTrustLogFailureDescription(SecTrustRef trust, SecTrustResultType trustResult)
{
    if (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified) {
        return;
    }

    CFStringRef failureDesc = SecTrustCopyFailureDescription(trust);
    secerror("Trust evaluate failure:%{public}@", failureDesc);
    CFRelease(failureDesc);
}

static OSStatus SecTrustEvaluateInternal(SecTrustRef trust, SecTrustResultType *result) {
    if (result) {
        *result = kSecTrustResultInvalid;
    }
    if (!trust) {
        return errSecParam;
    }
    OSStatus status = SecTrustEvaluateIfNecessary(trust);
    if (status) {
        return status;
    }
    /* post-process trust result based on exceptions */
    __block SecTrustResultType trustResult = kSecTrustResultInvalid;
    dispatch_sync(trust->_trustQueue, ^{
        trustResult = trust->_trustResult;
    });

    SecTrustLogFailureDescription(trust, trustResult);


    if (result) {
        *result = trustResult;
    }

    return status;
}

OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *result) {
    return SecTrustEvaluateInternal(trust, result);
}

static CFStringRef SecTrustCopyChainSummary(SecTrustRef trust) {
    CFMutableStringRef summary = CFStringCreateMutable(NULL, 0);
    __block CFArrayRef chain = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        chain = trust->_chain;
    });
    CFIndex ix, count = CFArrayGetCount(chain);
    for (ix = 0; ix < count; ix++) {
        if (ix != 0) { CFStringAppend(summary, CFSTR(",")); }
        CFStringRef certSummary = SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(chain, ix));
        CFStringAppendFormat(summary, NULL, CFSTR("\"%@\""), certSummary);
        CFReleaseNull(certSummary);
    }
    return summary;
}

#define SecCopyTrustString(KEY) SecFrameworkCopyLocalizedString(KEY, CFSTR("Trust"))

struct checkmap_entry_s {
    SecTrustErrorSubType type;
    OSStatus status;
    const CFStringRef errorKey;
};
typedef struct checkmap_entry_s checkmap_entry_t;

const checkmap_entry_t checkmap[] = {
#undef POLICYCHECKMACRO
#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
{ __PC_SUBTYPE_##SUBTYPE , OSSTATUS, SEC_TRUST_ERROR_##NAME },
#include "SecPolicyChecks.list"
};

static OSStatus SecTrustCopyErrorStrings(SecTrustRef trust,
                                     CFStringRef * CF_RETURNS_RETAINED simpleError,
                                     CFStringRef * CF_RETURNS_RETAINED fullError) {
    if (!simpleError || !fullError) {
        return errSecParam;
    }
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
    if (!details)
        return errSecInternal;

    /* We need to map the policy check constants to indexes into our checkmap table. */
    static dispatch_once_t onceToken;
    static CFArrayRef policyChecks = NULL;
    dispatch_once(&onceToken, ^{
        CFMutableArrayRef _policyChecks = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        #undef POLICYCHECKMACRO
        #define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, ERRORSTRING) \
        CFArrayAppendValue(_policyChecks, kSecPolicyCheck##NAME);
        #include "SecPolicyChecks.list"
        policyChecks = _policyChecks;
    });

    /* Build the errors for each cert in the detailed results array */
    __block CFMutableStringRef fullMutableError = CFStringCreateMutable(NULL, 0);
    __block SecTrustErrorSubType simpleErrorSubType = kSecTrustErrorSubTypeInvalid;
    __block OSStatus simpleErrorStatus = errSecInternalError;
    __block CFIndex simpleErrorCertIndex = kCFNotFound;
    __block CFIndex ix;
    CFIndex count = CFArrayGetCount(details);
    for (ix = 0; ix < count; ix++) {
        CFDictionaryRef perCertDetails = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        if (CFDictionaryGetCount(perCertDetails) == 0) { continue; } // no errors on this cert

        /* Get the cert summary and start the full error details string for this cert */
        CFStringRef certSummary = SecCertificateCopySubjectSummary(SecTrustGetCertificateAtIndex(trust, ix));
        CFStringRef format = SecCopyTrustString(SEC_TRUST_CERTIFICATE_ERROR);
        CFStringAppendFormat(fullMutableError, NULL, format,
                             ix, certSummary);
        CFReleaseNull(certSummary);
        CFReleaseNull(format);

        /* Figure out the errors */
        __block bool firstError = true;
        CFDictionaryForEach(perCertDetails, ^(const void *key, const void * __unused value) {
            CFIndex policyCheckIndex = CFArrayGetFirstIndexOfValue(policyChecks, CFRangeMake(0, CFArrayGetCount(policyChecks)), key);
            if ((policyCheckIndex < 0) || ((size_t)policyCheckIndex >= sizeof(checkmap)/sizeof(checkmap[0]))) {
                secwarning("unknown failure key in details dictionary: %@", key);
                return;
            }
            /* Keep track of the highest priority error encountered during this evaluation.
             * If multiple certs have errors of the same subtype we keep the lowest indexed cert. */
            if (simpleErrorSubType > checkmap[policyCheckIndex].type) {
                simpleErrorSubType = checkmap[policyCheckIndex].type;
                simpleErrorCertIndex = ix;
                simpleErrorStatus = checkmap[policyCheckIndex].status;
            }
            /* Add this error to the full error */
            if (!firstError) { CFStringAppend(fullMutableError, CFSTR(", ")); }
            CFStringRef errorString = SecCopyTrustString(checkmap[policyCheckIndex].errorKey);
            CFStringAppend(fullMutableError, errorString);
            CFReleaseNull(errorString);
            firstError = false;
        });
        CFStringAppend(fullMutableError, CFSTR(";"));
    }
    CFReleaseNull(details);

    /* Build the simple error */
    if (simpleErrorCertIndex == kCFNotFound) { simpleErrorCertIndex = 0; }
    CFStringRef format = NULL;
    CFStringRef certSummary = SecCertificateCopySubjectSummary(SecTrustGetCertificateAtIndex(trust, simpleErrorCertIndex));
    switch (simpleErrorSubType) {
        case kSecTrustErrorSubTypeBlocked: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_BLOCKED);
            break;
        }
        case kSecTrustErrorSubTypeRevoked: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_REVOKED);
            break;
        }
        case kSecTrustErrorSubTypeKeySize: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_KEYSIZE);
            break;
        }
        case kSecTrustErrorSubTypeWeakHash: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_WEAKHASH);
            break;
        }
        case kSecTrustErrorSubTypeDenied: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_DENIED);
            break;
        }
        case kSecTrustErrorSubTypeCompliance: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_COMPLIANCE);
            break;
        }
        case kSecTrustErrorSubTypeExpired: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_EXPIRED);
            break;
        }
        case kSecTrustErrorSubTypeTrust: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_TRUST);
            break;
        }
        case kSecTrustErrorSubTypeName: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_NAME);
            break;
        }
        case kSecTrustErrorSubTypeUsage: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_USAGE);
            break;
        }
        case kSecTrustErrorSubTypePinning: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_PINNING);
            CFAssignRetained(certSummary, SecTrustCopyChainSummary(trust));
            break;
        }
        default: {
            format = SecCopyTrustString(SEC_TRUST_ERROR_SUBTYPE_INVALID);
            break;
        }
    }
    if (format && certSummary) {
        *simpleError = CFStringCreateWithFormat(NULL, NULL, format, certSummary);
    }
    CFReleaseNull(format);
    CFReleaseNull(certSummary);
    *fullError = fullMutableError;
    return simpleErrorStatus;
}

static CF_RETURNS_RETAINED CFErrorRef SecTrustCopyError(SecTrustRef trust) {
    if (!trust) { return NULL; }
    OSStatus status = errSecSuccess;
    __block SecTrustResultType trustResult = kSecTrustResultInvalid;
    dispatch_sync(trust->_trustQueue, ^{
        trustResult = trust->_trustResult;
    });
    if (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified) {
        return NULL;
    }

    CFStringRef detailedError = NULL;
    CFStringRef simpleError = NULL;
    status = SecTrustCopyErrorStrings(trust, &simpleError, &detailedError);
    /* failure to obtain either string must not cause a failure to create the CFErrorRef */
    if (!simpleError) {
        simpleError = SecCopyErrorMessageString(status, NULL);
    }
    if (!detailedError) {
        detailedError = SecCopyErrorMessageString(status, NULL);
    }
    CFDictionaryRef userInfo = CFDictionaryCreate(NULL, (const void **)&kCFErrorLocalizedDescriptionKey,
                                                            (const void **)&detailedError, 1,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFErrorRef underlyingError = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, userInfo);
    CFReleaseNull(userInfo);
    CFReleaseNull(detailedError);

    const void *keys[] = { kCFErrorLocalizedDescriptionKey, kCFErrorUnderlyingErrorKey };
    const void *values[] = { simpleError, underlyingError };
    userInfo = CFDictionaryCreate(NULL, keys, values, 2,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
    CFErrorRef error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, userInfo);
    CFReleaseNull(userInfo);
    CFReleaseNull(simpleError);
    CFReleaseNull(underlyingError);
    return error;
}

bool SecTrustEvaluateWithError(SecTrustRef trust, CFErrorRef *error) {
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    OSStatus status = SecTrustEvaluateInternal(trust, &trustResult);
    if (status == errSecSuccess && (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified)) {
        if (error) {
            *error = NULL;
        }
        return true;
    }
    if (error) {
        if (status != errSecSuccess) {
            *error = SecCopyLastError(status);
        } else {
            *error = SecTrustCopyError(trust);
        }
    }
    return false;
}

OSStatus SecTrustEvaluateAsync(SecTrustRef trust,
	dispatch_queue_t queue, SecTrustCallback result)
{
	CFRetainSafe(trust);
	dispatch_async(queue, ^{
		SecTrustResultType trustResult;
		if (errSecSuccess != SecTrustEvaluateInternal(trust, &trustResult)) {
			trustResult = kSecTrustResultInvalid;
		}
		result(trust, trustResult);
		CFReleaseSafe(trust);
	});
	return errSecSuccess;
}


OSStatus SecTrustEvaluateFastAsync(SecTrustRef trust,
	dispatch_queue_t queue, SecTrustCallback result)
{
	if (trust == NULL || queue == NULL || result == NULL) {
		return errSecParam;
	}

	dispatch_assert_queue(queue);
	SecTrustEvaluateIfNecessaryFastAsync(trust, queue, ^(OSStatus status) {
		if (status != noErr) {
			result(trust, kSecTrustResultInvalid);
			return;
		}
		__block SecTrustResultType trustResult = kSecTrustResultInvalid;
		dispatch_sync(trust->_trustQueue, ^{
			trustResult = trust->_trustResult;
		});
        SecTrustLogFailureDescription(trust, trustResult);


		result(trust, trustResult);
	});
	return errSecSuccess;
}

OSStatus SecTrustEvaluateAsyncWithError(SecTrustRef trust, dispatch_queue_t queue, SecTrustWithErrorCallback callback)
{
    if (trust == NULL || queue == NULL || callback == NULL) {
        return errSecParam;
    }

    dispatch_assert_queue(queue);
    SecTrustEvaluateIfNecessaryFastAsync(trust, queue, ^(OSStatus status) {
        if (status != noErr) {
            CFErrorRef error = SecCopyLastError(status);
            callback(trust, false, error);
            CFReleaseNull(error);
            return;
        }

        __block SecTrustResultType trustResult = kSecTrustResultInvalid;
        dispatch_sync(trust->_trustQueue, ^{
            trustResult = trust->_trustResult;
        });
        SecTrustLogFailureDescription(trust, trustResult);


        CFErrorRef error = SecTrustCopyError(trust);
        bool result = (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified);
        callback(trust, result, error);
        CFReleaseNull(error);
    });

    return errSecSuccess;
}

static bool append_certificate_to_xpc_array(SecCertificateRef certificate, xpc_object_t xpc_certificates);
static xpc_object_t copy_xpc_certificates_array(CFArrayRef certificates);
xpc_object_t copy_xpc_policies_array(CFArrayRef policies);
OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required);

static bool append_certificate_to_xpc_array(SecCertificateRef certificate, xpc_object_t xpc_certificates) {
    if (!certificate) {
        return true; // NOOP
	}
    size_t length = SecCertificateGetLength(certificate);
    const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
    if (!length || !bytes) {
		return false;
	}
    xpc_array_set_data(xpc_certificates, XPC_ARRAY_APPEND, bytes, length);
    return true;
}

static xpc_object_t copy_xpc_certificates_array(CFArrayRef certificates) {
    xpc_object_t xpc_certificates = xpc_array_create(NULL, 0);
	if (!xpc_certificates) {
		return NULL;
	}
    CFIndex ix, count = CFArrayGetCount(certificates);
    for (ix = 0; ix < count; ++ix) {
		SecCertificateRef certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, ix);
    #if SECTRUST_VERBOSE_DEBUG
		size_t length = SecCertificateGetLength(certificate);
		const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
		secerror("idx=%d of %d; cert=0x%lX length=%ld bytes=0x%lX", (int)ix, (int)count, (uintptr_t)certificate, (size_t)length, (uintptr_t)bytes);
    #endif
        if (!append_certificate_to_xpc_array(certificate, xpc_certificates)) {
            xpc_release(xpc_certificates);
            xpc_certificates = NULL;
			break;
        }
    }
	return xpc_certificates;
}

static bool SecXPCDictionarySetCertificates(xpc_object_t message, const char *key, CFArrayRef certificates, CFErrorRef *error) {
	xpc_object_t xpc_certificates = copy_xpc_certificates_array(certificates);
    if (!xpc_certificates) {
		SecError(errSecAllocate, error, CFSTR("failed to create xpc_array of certificates"));
        return false;
	}
    xpc_dictionary_set_value(message, key, xpc_certificates);
    xpc_release(xpc_certificates);
    return true;
}

static bool SecXPCDictionarySetPolicies(xpc_object_t message, const char *key, CFArrayRef policies, CFErrorRef *error) {
    xpc_object_t xpc_policies = copy_xpc_policies_array(policies);
    if (!xpc_policies) {
		SecError(errSecAllocate, error, CFSTR("failed to create xpc_array of policies"));
        return false;
	}
    xpc_dictionary_set_value(message, key, xpc_policies);
    xpc_release(xpc_policies);
    return true;
}


static bool CFDataAppendToXPCArray(CFDataRef data, xpc_object_t xpc_data_array, CFErrorRef *error) {
    if (!data)
        return true; // NOOP

    size_t length = CFDataGetLength(data);
    const uint8_t *bytes = CFDataGetBytePtr(data);
    if (!length || !bytes)
        return SecError(errSecParam, error, CFSTR("invalid CFDataRef"));

    xpc_array_set_data(xpc_data_array, XPC_ARRAY_APPEND, bytes, length);
    return true;
}


static xpc_object_t CFDataArrayCopyXPCArray(CFArrayRef data_array, CFErrorRef *error) {
    xpc_object_t xpc_data_array;
    require_action_quiet(xpc_data_array = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(data_array);
    for (ix = 0; ix < count; ++ix) {
        if (!CFDataAppendToXPCArray((CFDataRef)CFArrayGetValueAtIndex(data_array, ix), xpc_data_array, error)) {
            xpc_release(xpc_data_array);
            return NULL;
        }
    }

exit:
    return xpc_data_array;
}

static bool SecXPCDictionarySetDataArray(xpc_object_t message, const char *key, CFArrayRef data_array, CFErrorRef *error) {
    xpc_object_t xpc_data_array = CFDataArrayCopyXPCArray(data_array, error);
    if (!xpc_data_array)
        return false;
    xpc_dictionary_set_value(message, key, xpc_data_array);
    xpc_release(xpc_data_array);
    return true;
}

static bool SecXPCDictionaryCopyChainOptional(xpc_object_t message, const char *key, CFArrayRef *path, CFErrorRef *error) {
    xpc_object_t xpc_path = xpc_dictionary_get_value(message, key);
    CFMutableArrayRef output = NULL;
    size_t count = 0;
    if (!xpc_path) {
        *path = NULL;
        return true;
    }
    require_action_quiet(xpc_get_type(xpc_path) == XPC_TYPE_ARRAY, exit, SecError(errSecDecode, error, CFSTR("xpc_path value is not an array")));
    require_action_quiet(count = xpc_array_get_count(xpc_path), exit, SecError(errSecDecode, error, CFSTR("xpc_path array count == 0")));
    output = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithXPCArrayAtIndex(xpc_path, ix, error);
        if (certificate) {
            CFArrayAppendValue(output, certificate);
            CFReleaseNull(certificate);
        } else {
            CFReleaseNull(output);
            break;
        }
    }

exit:
    if (output) {
        *path = output;
        return true;
    }
    return false;
}

static int SecXPCDictionaryGetNonZeroInteger(xpc_object_t message, const char *key, CFErrorRef *error) {
    int64_t value = xpc_dictionary_get_int64(message, key);
    if (!value) {
        SecError(errSecInternal, error, CFSTR("object for key %s is 0"), key);
    }
    return (int)value;
}

static SecTrustResultType handle_trust_evaluate_xpc(enum SecXPCOperation op, CFArrayRef certificates,
                                                    CFArrayRef anchors, bool anchorsOnly,
                                                    bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses,
                                                    CFArrayRef SCTs, CFArrayRef trustedLogs,
                                                    CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef exceptions,
                                                    CFArrayRef *details, CFDictionaryRef *info, CFArrayRef *chain, CFErrorRef *error)
{
    __block SecTrustResultType tr = kSecTrustResultInvalid;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        if (!SecXPCDictionarySetCertificates(message, kSecTrustCertificatesKey, certificates, blockError))
            return false;
        if (anchors && !SecXPCDictionarySetCertificates(message, kSecTrustAnchorsKey, anchors, blockError))
            return false;
        if (anchorsOnly)
            xpc_dictionary_set_bool(message, kSecTrustAnchorsOnlyKey, anchorsOnly);
        xpc_dictionary_set_bool(message, kSecTrustKeychainsAllowedKey, keychainsAllowed);
        if (!SecXPCDictionarySetPolicies(message, kSecTrustPoliciesKey, policies, blockError))
            return false;
        if (responses && !SecXPCDictionarySetDataArray(message, kSecTrustResponsesKey, responses, blockError))
            return false;
        if (SCTs && !SecXPCDictionarySetDataArray(message, kSecTrustSCTsKey, SCTs, blockError))
            return false;
        if (trustedLogs && !SecXPCDictionarySetPList(message, kSecTrustTrustedLogsKey, trustedLogs, blockError))
            return false;
        xpc_dictionary_set_double(message, kSecTrustVerifyDateKey, verifyTime);
        if (exceptions && !SecXPCDictionarySetPList(message, kSecTrustExceptionsKey, exceptions, blockError))
            return false;
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        secdebug("trust", "response: %@", response);
        return SecXPCDictionaryCopyArrayOptional(response, kSecTrustDetailsKey, details, blockError) &&
        SecXPCDictionaryCopyDictionaryOptional(response, kSecTrustInfoKey, info, blockError) &&
        SecXPCDictionaryCopyChainOptional(response, kSecTrustChainKey, chain, blockError) &&
        ((tr = SecXPCDictionaryGetNonZeroInteger(response, kSecTrustResultKey, blockError)) != kSecTrustResultInvalid);
    });
    return tr;
}

typedef void (^trust_handler_t)(SecTrustResultType tr, CFErrorRef error);

static void handle_trust_evaluate_xpc_async(dispatch_queue_t replyq, trust_handler_t trustHandler,
											enum SecXPCOperation op, CFArrayRef certificates,
											CFArrayRef anchors, bool anchorsOnly,
											bool keychainsAllowed, CFArrayRef policies,
											CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs,
											CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups,
											CFArrayRef exceptions, CFArrayRef *details,
											CFDictionaryRef *info, CFArrayRef *chain)
{
	securityd_send_async_and_do(op, replyq, ^bool(xpc_object_t message, CFErrorRef *error) {
		if (!SecXPCDictionarySetCertificates(message, kSecTrustCertificatesKey, certificates, error))
			return false;
		if (anchors && !SecXPCDictionarySetCertificates(message, kSecTrustAnchorsKey, anchors, error))
			return false;
		if (anchorsOnly)
			xpc_dictionary_set_bool(message, kSecTrustAnchorsOnlyKey, anchorsOnly);
		xpc_dictionary_set_bool(message, kSecTrustKeychainsAllowedKey, keychainsAllowed);
		if (!SecXPCDictionarySetPolicies(message, kSecTrustPoliciesKey, policies, error))
			return false;
		if (responses && !SecXPCDictionarySetDataArray(message, kSecTrustResponsesKey, responses, error))
			return false;
		if (SCTs && !SecXPCDictionarySetDataArray(message, kSecTrustSCTsKey, SCTs, error))
			return false;
		if (trustedLogs && !SecXPCDictionarySetPList(message, kSecTrustTrustedLogsKey, trustedLogs, error))
			return false;
		xpc_dictionary_set_double(message, kSecTrustVerifyDateKey, verifyTime);
		if (exceptions && !SecXPCDictionarySetPList(message, kSecTrustExceptionsKey, exceptions, error))
			return false;
		return true;
	}, ^(xpc_object_t response, CFErrorRef error) {
		secdebug("trust", "response: %@", response);
		if (response == NULL || error != NULL) {
			trustHandler(kSecTrustResultInvalid, error);
			return;
		}
		SecTrustResultType tr = kSecTrustResultInvalid;
		CFErrorRef error2 = NULL;
		if (SecXPCDictionaryCopyArrayOptional(response, kSecTrustDetailsKey, details, &error2) &&
			SecXPCDictionaryCopyDictionaryOptional(response, kSecTrustInfoKey, info, &error2) &&
			SecXPCDictionaryCopyChainOptional(response, kSecTrustChainKey, chain, &error2)) {
			tr = SecXPCDictionaryGetNonZeroInteger(response, kSecTrustResultKey, &error2);
		}
		trustHandler(tr, error2);
		CFReleaseNull(error2);
	});
}

OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required) {
	OSStatus result = errSecSuccess;
	CFIndex index, count;
	count = (array) ? CFArrayGetCount(array) : 0;
	if (!count && required) {
		secerror("no %@ in array!", arrayItemType);
		result = errSecParam;
	}
	for (index = 0; index < count; index++) {
		CFTypeRef item = (CFTypeRef) CFArrayGetValueAtIndex(array, index);
		if (!item) {
			secerror("%@ %@ (index %d)", arrayItemType, CFSTR("reference is nil"), (int)index);
			result = errSecParam;
			continue;
		}
		if (CFGetTypeID(item) != itemTypeID) {
			secerror("%@ %@ (index %d)", arrayItemType, CFSTR("is not the expected CF type"), (int)index);
			result = errSecParam;
		}
		// certificates
		if (CFGetTypeID(item) == SecCertificateGetTypeID()) {
			SecCertificateRef certificate = (SecCertificateRef) item;
			CFIndex length = SecCertificateGetLength(certificate);
			const UInt8 *bytes = SecCertificateGetBytePtr(certificate);
			if (!length) {
				secerror("%@ %@ (index %d)", arrayItemType, CFSTR("has zero length"), (int)index);
				result = errSecParam;
			}
			if (!bytes) {
				secerror("%@ %@ (index %d)", arrayItemType, CFSTR("has nil bytes"), (int)index);
				result = errSecParam;
			}
		#if SECTRUST_VERBOSE_DEBUG
			secerror("%@[%d] of %d = %ld bytes @ 0x%lX", arrayItemType, (int)index, (int)count, (size_t)length, (uintptr_t)bytes);
		#endif
		}
		// policies
		if (CFGetTypeID(item) == SecPolicyGetTypeID()) {
			SecPolicyRef policy = (SecPolicyRef) item;
			CFStringRef oidStr = policy->_oid;
			if (!oidStr || (CFGetTypeID(oidStr) != CFStringGetTypeID())) {
				oidStr = CFSTR("has invalid OID string!");
				secerror("%@ %@ (index %d)", arrayItemType, oidStr, (int)index);
			}
		#if SECTRUST_VERBOSE_DEBUG
			secerror("%@[%d] of %d = \"%@\" 0x%lX", arrayItemType, (int)index, (int)count, oidStr, (uintptr_t)policy);
		#endif
		}
	}
	return result;
}

static OSStatus SecTrustValidateInput(SecTrustRef trust) {
	OSStatus status, result = errSecSuccess;

	// certificates (required)
	status = validate_array_of_items(trust->_certificates, CFSTR("certificate"), SecCertificateGetTypeID(), true);
	if (status) result = status;
	// anchors (optional)
	status = validate_array_of_items(trust->_anchors, CFSTR("input anchor"), SecCertificateGetTypeID(), false);
	if (status) result = status;
	// policies (required??)
	status = validate_array_of_items(trust->_policies, CFSTR("policy"), SecPolicyGetTypeID(), true);
	if (status) result = status;
	// _responses, _SCTs, _trustedLogs, ...
	// verify time: SecTrustGetVerifyTime(trust)
	// access groups: SecAccessGroupsGetCurrent()

	return result;
}

static CFArrayRef SecTrustGetCurrentAccessGroups(void) {
    static CFArrayRef accessGroups = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        accessGroups =  CFArrayCreateForCFTypes(kCFAllocatorDefault,
                                                CFSTR("com.apple.trustd"),
                                                CFSTR("com.apple.trusttests"),
                                                NULL);
    });
    return accessGroups;
}

static OSStatus SecTrustEvaluateIfNecessary(SecTrustRef trust) {
    __block OSStatus result;
    check(trust);
    if (!trust)
        return errSecParam;

    __block CFAbsoluteTime verifyTime = SecTrustGetVerifyTime(trust);
    SecTrustAddPolicyAnchors(trust);
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_trustResult != kSecTrustResultInvalid) {
            result = errSecSuccess;
            return;
        }

        trust->_trustResult = kSecTrustResultOtherError; /* to avoid potential recursion */

        CFReleaseNull(trust->_chain);
        CFReleaseNull(trust->_details);
        CFReleaseNull(trust->_info);
        if (trust->_legacy_info_array) {
            free(trust->_legacy_info_array);
            trust->_legacy_info_array = NULL;
        }
        if (trust->_legacy_status_array) {
            free(trust->_legacy_status_array);
            trust->_legacy_status_array = NULL;
        }

        os_activity_initiate("SecTrustEvaluateIfNecessary", OS_ACTIVITY_FLAG_DEFAULT, ^{
            SecTrustValidateInput(trust);

            /* @@@ Consider an optimization where we keep a side dictionary with the SHA1 hash of ever SecCertificateRef we send, so we only send potential duplicates once, and have the server respond with either just the SHA1 hash of a certificate, or the complete certificate in the response depending on whether the client already sent it, so we don't send back certificates to the client it already has. */
            result = SecOSStatusWith(^bool (CFErrorRef *error) {
                trust->_trustResult = TRUSTD_XPC(sec_trust_evaluate,
                                                    handle_trust_evaluate_xpc,
                                                    trust->_certificates, trust->_anchors, trust->_anchorsOnly, trust->_keychainsAllowed,
                                                    trust->_policies, trust->_responses, trust->_SCTs, trust->_trustedLogs,
                                                    verifyTime, SecTrustGetCurrentAccessGroups(), trust->_exceptions,
                                                    &trust->_details, &trust->_info, &trust->_chain, error);
                if (trust->_trustResult == kSecTrustResultInvalid /* TODO check domain */ &&
                    SecErrorGetOSStatus(*error) == errSecNotAvailable &&
                    CFArrayGetCount(trust->_certificates)) {
                    /* We failed to talk to securityd.  The only time this should
                     happen is when we are running prior to launchd enabling
                     registration of services.  This currently happens when we
                     are running from the ramdisk.   To make ASR happy we initialize
                     _chain and return success with a failure as the trustResult, to
                     make it seem like we did a cert evaluation, so ASR can extract
                     the public key from the leaf. */
                    SecCertificateRef leafCert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
                    CFArrayRef leafCertArray = CFArrayCreate(NULL, (const void**)&leafCert, 1, &kCFTypeArrayCallBacks);
                    trust->_chain = leafCertArray;
                    if (error)
                        CFReleaseNull(*error);
                    return true;
                }
                return trust->_trustResult != kSecTrustResultInvalid;
            });
        });
    });
    return result;
}

// IMPORTANT: this MUST be called on the provided queue as it will call the handler synchronously
// if no asynchronous work is needed
static void SecTrustEvaluateIfNecessaryFastAsync(SecTrustRef trust,
												 dispatch_queue_t queue,
												 void (^handler)(OSStatus status)) {
	check(trust);
	check(queue);
	check(handler);
	if (handler == NULL) {
		return;
	}
	if (trust == NULL || queue == NULL) {
		handler(errSecParam);
		return;
	}

	__block bool shouldReturnSuccess = false;
	__block CFAbsoluteTime verifyTime = SecTrustGetVerifyTime(trust);
	SecTrustAddPolicyAnchors(trust);
	dispatch_sync(trust->_trustQueue, ^{
		if (trust->_trustResult != kSecTrustResultInvalid) {
			shouldReturnSuccess = true;
			return;
		}

		trust->_trustResult = kSecTrustResultOtherError; /* to avoid potential recursion */

		CFReleaseNull(trust->_chain);
		CFReleaseNull(trust->_details);
		CFReleaseNull(trust->_info);
		if (trust->_legacy_info_array) {
			free(trust->_legacy_info_array);
			trust->_legacy_info_array = NULL;
		}
		if (trust->_legacy_status_array) {
			free(trust->_legacy_status_array);
			trust->_legacy_status_array = NULL;
		}

		os_activity_t activity = os_activity_create("SecTrustEvaluateIfNecessaryFastAsync",
													OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
		__block struct os_activity_scope_state_s activityState;
		os_activity_scope_enter(activity, &activityState);
		os_release(activity);

		SecTrustValidateInput(trust);

		CFRetainSafe(trust);
		TRUSTD_XPC_ASYNC(sec_trust_evaluate,
						 handle_trust_evaluate_xpc_async,
						 queue,
		 ^(SecTrustResultType tr, CFErrorRef error) {
			 __block OSStatus result = errSecInternalError;
			 dispatch_sync(trust->_trustQueue, ^{
				 trust->_trustResult = tr;
				 if (trust->_trustResult == kSecTrustResultInvalid /* TODO check domain */ &&
					 SecErrorGetOSStatus(error) == errSecNotAvailable &&
					 CFArrayGetCount(trust->_certificates)) {
					 /* We failed to talk to securityd.  The only time this should
					  happen is when we are running prior to launchd enabling
					  registration of services.  This currently happens when we
					  are running from the ramdisk.   To make ASR happy we initialize
					  _chain and return success with a failure as the trustResult, to
					  make it seem like we did a cert evaluation, so ASR can extract
					  the public key from the leaf. */
					 SecCertificateRef leafCert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
					 CFArrayRef leafCertArray = CFArrayCreate(NULL, (const void**)&leafCert, 1, &kCFTypeArrayCallBacks);
					 trust->_chain = leafCertArray;
					 result = errSecSuccess;
					 return;
				 }
				 result = SecOSStatusWith(^bool (CFErrorRef *error2) {
					 if (error2 != NULL) {
						 *error2 = error;
					 }
					 return trust->_trustResult != kSecTrustResultInvalid;
				 });
			 });
			 os_activity_scope_leave(&activityState);
			 handler(result);
			 CFReleaseSafe(trust);
		 },
						 trust->_certificates, trust->_anchors, trust->_anchorsOnly, trust->_keychainsAllowed,
						 trust->_policies, trust->_responses, trust->_SCTs, trust->_trustedLogs,
						 verifyTime, SecTrustGetCurrentAccessGroups(), trust->_exceptions,
						 &trust->_details, &trust->_info, &trust->_chain);
	});
	if (shouldReturnSuccess) {
		handler(errSecSuccess);
	}
}

/* Helper for the qsort below. */
static int compare_strings(const void *a1, const void *a2) {
    CFStringRef s1 = *(CFStringRef *)a1;
    CFStringRef s2 = *(CFStringRef *)a2;
    return (int) CFStringCompare(s1, s2, kCFCompareForcedOrdering);
}

CFStringRef SecTrustCopyFailureDescription(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    CFMutableStringRef reason = CFStringCreateMutable(NULL, 0);
    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
    CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
    for (CFIndex ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex dCount = CFDictionaryGetCount(detail);
        if (dCount) {
            if (ix == 0)
                CFStringAppend(reason, CFSTR(" [leaf"));
            else if (ix == pathLength - 1)
                CFStringAppend(reason, CFSTR(" [root"));
            else
                CFStringAppendFormat(reason, NULL, CFSTR(" [ca%" PRIdCFIndex ), ix);

            const void *keys[dCount];
            CFDictionaryGetKeysAndValues(detail, &keys[0], NULL);
            qsort(&keys[0], dCount, sizeof(keys[0]), compare_strings);
            for (CFIndex kix = 0; kix < dCount; ++kix) {
                CFStringRef key = keys[kix];
                const void *value = CFDictionaryGetValue(detail, key);
                CFStringAppendFormat(reason, NULL, CFSTR(" %@%@"), key,
                                     (CFGetTypeID(value) == CFBooleanGetTypeID()
                                      ? CFSTR("") : value));
            }
            CFStringAppend(reason, CFSTR("]"));
        }
    }
    CFReleaseSafe(details);
    return reason;
}

SecKeyRef SecTrustCopyKey(SecTrustRef trust)
{
    if (!trust) {
        return NULL;
    }
    __block SecKeyRef publicKey = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_publicKey) {
            publicKey = CFRetainSafe(trust->_publicKey);
            return;
        }
        SecCertificateRef leaf = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
        trust->_publicKey = SecCertificateCopyKey(leaf);
        if (trust->_publicKey) {
            publicKey = CFRetainSafe(trust->_publicKey);
        }
    });
    /* If we couldn't get a public key from the leaf cert alone. */
    if (!publicKey) {
        SecTrustEvaluateIfNecessary(trust);
        dispatch_sync(trust->_trustQueue, ^{
            if (trust->_chain) {
                SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_chain, 0);
                trust->_publicKey = SecCertificateCopyKey(cert);
                publicKey = CFRetainSafe(trust->_publicKey);
            }
        });
    }
    return publicKey;
}

#if TARGET_OS_OSX
/* On OS X we need SecTrustCopyPublicKey to give us a CDSA-based SecKeyRef,
   so we will refer to this one internally as SecTrustCopyPublicKey_ios,
   and call it from SecTrustCopyPublicKey.
 */
SecKeyRef SecTrustCopyPublicKey_ios(SecTrustRef trust)
#else
SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust)
#endif
{
    return SecTrustCopyKey(trust);
}

CFIndex SecTrustGetCertificateCount(SecTrustRef trust) {
    if (!trust) {
        return 0;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFIndex certCount = 1;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_chain) {
            certCount = CFArrayGetCount(trust->_chain);
        }
    });
	return certCount;
}

SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust,
    CFIndex ix) {
    if (!trust) {
        return NULL;
    }
    __block SecCertificateRef cert = NULL;
    if (ix == 0) {
        dispatch_sync(trust->_trustQueue, ^{
            cert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
        });
        return cert;
    }
    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_chain) {
            cert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_chain, ix);
        }
    });
    return cert;
}

CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFDictionaryRef info = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        info = CFRetainSafe(trust->_info);
    });
    return info;
}

CFArrayRef SecTrustGetTrustExceptionsArray(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    __block CFArrayRef exceptions = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        exceptions = trust->_exceptions;
    });
    return exceptions;
}

CFDataRef SecTrustCopyExceptions(SecTrustRef trust) {
    /* Stash the old exceptions and run an evaluation with no exceptions filtered.  */
    __block CFArrayRef oldExceptions = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_exceptions) {
            oldExceptions = trust->_exceptions;
            trust->_exceptions = NULL;
        }
    });
    if (oldExceptions) {
        SecTrustSetNeedsEvaluation(trust);
    }

    /* Create the new exceptions based on an unfiltered eval. */
    __block CFArrayRef details = NULL;
    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
    CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
    CFMutableArrayRef exceptions = CFArrayCreateMutable(kCFAllocatorDefault, pathLength, &kCFTypeArrayCallBacks);
    /* Fetch the current exceptions epoch and tag each exception with it. */
    CFErrorRef exceptionResetCountError = NULL;
    uint64_t exceptionResetCount = SecTrustGetExceptionResetCount(&exceptionResetCountError);
    secinfo("trust", "The current exceptions epoch is %llu. (%{public}s)", exceptionResetCount, exceptionResetCountError ? "Error" : "OK");
    CFNumberRef exceptionResetCountRef = CFNumberCreate(NULL, kCFNumberSInt64Type, &exceptionResetCount);
    CFIndex ix;
    for (ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex detailCount = CFDictionaryGetCount(detail);
        CFMutableDictionaryRef exception;
        if (ix == 0 || detailCount > 0) {
            exception = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, detailCount + 1, detail);
            SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
            CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
            CFDictionaryAddValue(exception, kSecCertificateDetailSHA1Digest, digest);
            if (exceptionResetCount && !exceptionResetCountError && exceptionResetCountRef) {
                CFDictionaryAddValue(exception, kSecCertificateExceptionResetCount, exceptionResetCountRef);
            }
        } else {
            /* Add empty exception dictionaries for non leaf certs which have no exceptions to save space. */
            exception = (CFMutableDictionaryRef)CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
        CFArrayAppendValue(exceptions, exception);
        CFReleaseNull(exception);
    }

    /* Restore the stashed exceptions. */
    if (oldExceptions) {
        dispatch_sync(trust->_trustQueue, ^{
            trust->_exceptions = oldExceptions;
        });
        SecTrustSetNeedsEvaluation(trust);
    }

    /* Remove any trailing empty dictionaries to save even more space (we skip the leaf
       since it will never be empty). */
    for (ix = pathLength; ix-- > 1;) {
        CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, ix);
        if (CFDictionaryGetCount(exception) == 0) {
            CFArrayRemoveValueAtIndex(exceptions, ix);
        } else {
            break;
        }
    }

    CFDataRef encodedExceptions = CFPropertyListCreateData(kCFAllocatorDefault,
        exceptions, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    CFRelease(exceptions);
    CFReleaseSafe(details);
    CFReleaseSafe(exceptionResetCountRef);
    CFReleaseSafe(exceptionResetCountError);
    return encodedExceptions;
}

static bool SecTrustExceptionsValidForThisEpoch(CFArrayRef exceptions) {
    if (!exceptions) {
        return false;
    }
    CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, 0);

    CFErrorRef currentExceptionResetCountError = NULL;
    uint64_t currentExceptionResetCount = SecTrustGetExceptionResetCount(&currentExceptionResetCountError);
    secinfo("trust", "The current exceptions epoch is %llu. (%{public}s)", currentExceptionResetCount, currentExceptionResetCountError ? "Error" : "OK");
    /* Fail closed: if we were unable to get the current exceptions epoch consider the exceptions invalid. */
    if (currentExceptionResetCountError) {
        secerror("Failed to get the current exceptions epoch.");
        CFReleaseNull(currentExceptionResetCountError);
        return false;
    }
    /* If this is the first epoch ever there is no point in checking whether any exceptions belong in the past. */
    if (currentExceptionResetCount == 0) {
        return true;
    }

    CFNumberRef resetCountRef = CFDictionaryGetValue(exception, kSecCertificateExceptionResetCount);
    if (!resetCountRef) {
        secerror("Failed to get the exception's epoch.");
        return false;
    }

    uint64_t exceptionResetCount;
    if (!CFNumberGetValue(resetCountRef, kCFNumberSInt64Type, &exceptionResetCount)) {
        secerror("Failed to parse the current exceptions epoch as a uint64.");
        return false;
    }

    if (exceptionResetCount != currentExceptionResetCount) {
        secerror("The current exception's epoch (%llu) is not the current epoch. (%llu)", exceptionResetCount, currentExceptionResetCount);
        return false;
    }

    secinfo("trust", "Exceptions are valid for the current exceptions epoch. (%llu)", currentExceptionResetCount);
    return true;
}

bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef encodedExceptions) {
	if (!trust) {
		return false;
	}
	CFArrayRef exceptions = NULL;

	if (NULL != encodedExceptions) {
		exceptions = (CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
			encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
	}

	if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
		CFRelease(exceptions);
		exceptions = NULL;
	}

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseSafe(trust->_exceptions);
        trust->_exceptions = exceptions;
    });

    /* We changed the exceptions -- so we need to re-evaluate */
    SecTrustSetNeedsEvaluation(trust);

	/* If there is a valid exception entry for our current leaf we're golden. */
    if (SecTrustGetExceptionForCertificateAtIndex(trust, 0) && SecTrustExceptionsValidForThisEpoch(exceptions)) {
		return true;
    }

	/* The passed in exceptions didn't match our current leaf, so we discard it. */
    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseNull(trust->_exceptions);
    });
	return false;
}

#if TARGET_OS_OSX
OSStatus
SecTrustSetOptions(SecTrustRef trustRef, SecTrustOptionFlags options)
{
    /* bridge to support API functionality for legacy callers */
    OSStatus status = errSecSuccess;

    /* No options or none that trigger the exceptions behavior */
    if (!options  ||
        0 == (options & (kSecTrustOptionAllowExpired |
                         kSecTrustOptionImplicitAnchors |
                         kSecTrustOptionAllowExpiredRoot))) {
        return status;
    }

    __block CFMutableArrayRef exceptions = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!exceptions) { return errSecAllocate; }

    /* Add the new options to the old_exceptions when those exceptions are tied to a particular cert.
     * If not tied to a particular cert, we reset the exceptions based on the input options. */
    CFArrayRef old_exceptions = SecTrustGetTrustExceptionsArray(trustRef);
    if (old_exceptions && SecTrustGetExceptionForCertificateAtIndex(trustRef, 0)) {
        CFIndex ix, count = CFArrayGetCount(old_exceptions);
        for (ix = 0; ix < count; ix++) {
            CFMutableDictionaryRef exception_dictionary = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)CFArrayGetValueAtIndex(old_exceptions, ix));
            if (!exception_dictionary) { status = errSecAllocate; goto out; }
            if ((options & kSecTrustOptionAllowExpired) != 0) {
                CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckTemporalValidity, kCFBooleanFalse);
            }
            if ((options & (kSecTrustOptionImplicitAnchors | kSecTrustOptionAllowExpiredRoot)) != 0) {
                /* Check that root is self-signed. */
                Boolean isSelfSigned = false;
                SecCertificateRef cert = SecTrustGetCertificateAtIndex(trustRef, ix);
                if (cert && (errSecSuccess == SecCertificateIsSelfSigned(cert, &isSelfSigned)) &&
                    isSelfSigned) {
                    if ((options & kSecTrustOptionImplicitAnchors) != 0) {
                        CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckAnchorTrusted, kCFBooleanFalse);
                    } else if ((options & kSecTrustOptionAllowExpiredRoot) != 0) {
                        CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckTemporalValidity, kCFBooleanFalse);
                    }
                }
            }
            CFArrayAppendValue(exceptions, exception_dictionary);
            CFReleaseNull(exception_dictionary);
        }
    } else {
        /* Create a new exceptions array. Warning, this takes advantage of implementation details of the exceptions mechanism. */
        CFMutableDictionaryRef exception_dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                                &kCFTypeDictionaryValueCallBacks);
        if (!exception_dictionary) { status = errSecAllocate; goto out; }
        if ((options & kSecTrustOptionAllowExpired) != 0) {
            CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckTemporalValidity, kCFBooleanFalse);
        }
        if ((options & kSecTrustOptionAllowExpiredRoot) != 0) {
            CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckValidRoot, kCFBooleanFalse);
        }
        if ((options & kSecTrustOptionImplicitAnchors) != 0) {
            CFDictionaryAddValue(exception_dictionary, kSecPolicyCheckAnchorTrusted, kCFBooleanFalse);
        }
        CFArrayAppendValue(exceptions, exception_dictionary);
        CFReleaseNull(exception_dictionary);
    }

    /* Set exceptions */
    dispatch_sync(trustRef->_trustQueue, ^{
        CFReleaseSafe(trustRef->_exceptions);
        trustRef->_exceptions = CFRetainSafe(exceptions);
    });
    /* We changed the exceptions -- so we need to re-evaluate */
    SecTrustSetNeedsEvaluation(trustRef);

out:
    CFReleaseNull(exceptions);
    return status;
}
#endif

CFArrayRef SecTrustCopySummaryPropertiesAtIndex(SecTrustRef trust, CFIndex ix) {
    CFMutableArrayRef summary;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    summary = SecCertificateCopySummaryProperties(certificate,
        SecTrustGetVerifyTime(trust));
    /* FIXME Add more details in the failure case. */

    return summary;
}

CFArrayRef SecTrustCopyDetailedPropertiesAtIndex(SecTrustRef trust, CFIndex ix) {
    CFArrayRef summary;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    summary = SecCertificateCopyProperties(certificate);

    return summary;
}

struct TrustFailures {
    bool badLinkage;
    bool unknownCritExtn;
    bool untrustedAnchor;
    bool missingIntermediate;
    bool hostnameMismatch;
    bool policyFail;
    bool invalidCert;
    bool weakKey;
    bool weakHash;
    bool revocation;
};

static void applyDetailProperty(const void *_key, const void *_value,
    void *context) {
    CFStringRef key = (CFStringRef)_key;
    struct TrustFailures *tf = (struct TrustFailures *)context;
    if (CFGetTypeID(_value) != CFBooleanGetTypeID()) {
        /* Value isn't a CFBooleanRef, oh no! */
        return;
    }
    CFBooleanRef value = (CFBooleanRef)_value;
    if (CFBooleanGetValue(value)) {
        /* Not an actual failure so we don't report it. */
        return;
    }

    /* @@@ FIXME: Report a different return value when something is in the
       details but masked out by an exception and use that below for display
       purposes. */
    if (CFEqual(key, kSecPolicyCheckIdLinkage)) {
        tf->badLinkage = true;
    } else if (CFEqual(key, kSecPolicyCheckCriticalExtensions)) {
        tf->unknownCritExtn = true;
    } else if (CFEqual(key, kSecPolicyCheckAnchorTrusted)
        || CFEqual(key, kSecPolicyCheckAnchorSHA256)
        || CFEqual(key, kSecPolicyCheckAnchorApple)) {
        tf->untrustedAnchor = true;
    } else if (CFEqual(key, kSecPolicyCheckMissingIntermediate)) {
        tf->missingIntermediate = true;
    } else if (CFEqual(key, kSecPolicyCheckSSLHostname)) {
        tf->hostnameMismatch = true;
    } else if (CFEqual(key, kSecPolicyCheckTemporalValidity)) {
        tf->invalidCert = true;
    } else if (CFEqual(key, kSecPolicyCheckWeakKeySize)
               || CFEqualSafe(key, kSecPolicyCheckKeySize)
               || CFEqualSafe(key, kSecPolicyCheckSystemTrustedWeakKey)) {
        tf->weakKey = true;
    } else if (CFEqual(key, kSecPolicyCheckWeakSignature)
               || CFEqualSafe(key, kSecPolicyCheckSignatureHashAlgorithms)
               || CFEqualSafe(key, kSecPolicyCheckSystemTrustedWeakHash)) {
        tf->weakHash = true;
    } else if (CFEqual(key, kSecPolicyCheckRevocation)) {
        tf->revocation = true;
    } else {
        /* Anything else is a policy failure. */
        tf->policyFail = true;
    }
}

static void appendError(CFMutableArrayRef properties, CFStringRef error, bool localized) {
    CFStringRef localizedError = NULL;
    if (!error) {
        return;
    } else if (localized) {
        //%%% "SecCertificate" should be changed to "Certificate": rdar://37517120
        localizedError = SecFrameworkCopyLocalizedString(error, CFSTR("SecCertificate"));
    } else {
        localizedError = (CFStringRef) CFRetainSafe(error);
    }
    appendProperty(properties, kSecPropertyTypeError, NULL, NULL,
                   localizedError, localized);
    CFReleaseNull(localizedError);
}

#if TARGET_OS_OSX
/* OS X properties array has a different structure and is implemented SecTrust.cpp. */
CFArrayRef SecTrustCopyProperties_ios(SecTrustRef trust)
#else
CFArrayRef SecTrustCopyProperties(SecTrustRef trust)
#endif
{
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    bool localized = true;
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
    if (!details) {
        return NULL;
    }

    struct TrustFailures tf = {};

    CFIndex ix, count = CFArrayGetCount(details);
    for (ix = 0; ix < count; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)
            CFArrayGetValueAtIndex(details, ix);
        /* We now have a detail dictionary for certificate at index ix, with
           a key value pair for each failed policy check.  Let's convert it
           from Ro-Man form into something a Hu-Man can understand. */
        CFDictionaryApplyFunction(detail, applyDetailProperty, &tf);
    }

    CFMutableArrayRef properties = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    /* The badLinkage and unknownCritExtn failures are short circuited, since
       you can't recover from those errors. */
    if (tf.badLinkage) {
        appendError(properties, CFSTR("Invalid certificate chain linkage."), localized);
    } else if (tf.unknownCritExtn) {
        appendError(properties, CFSTR("One or more unsupported critical extensions found."), localized);
    } else {
        if (tf.untrustedAnchor) {
            appendError(properties, CFSTR("Root certificate is not trusted."), localized);
        }
        if (tf.missingIntermediate) {
            appendError(properties, CFSTR("Unable to build chain to root certificate."), localized);
        }
        if (tf.hostnameMismatch) {
            appendError(properties, CFSTR("Hostname mismatch."), localized);
        }
        if (tf.policyFail) {
            appendError(properties, CFSTR("Policy requirements not met."), localized);
        }
        if (tf.invalidCert) {
            appendError(properties, CFSTR("One or more certificates have expired or are not valid yet."), localized);
        }
        if (tf.weakKey) {
            appendError(properties, CFSTR("One or more certificates is using a weak key size."), localized);
        }
        if (tf.weakHash) {
            appendError(properties, CFSTR("One or more certificates is using a weak signature algorithm."), localized);
        }
        if (tf.revocation) {
            appendError(properties, CFSTR("One or more certificates have been revoked."), localized);
        }
    }

    if (CFArrayGetCount(properties) == 0) {
        /* The certificate chain is trusted, return an empty plist */
        CFReleaseNull(properties);
    }

    CFReleaseNull(details);
    return properties;
}

#if TARGET_OS_OSX
static void _AppendStatusCode(CFMutableArrayRef array, OSStatus statusCode) {
    if (!array) {
        return;
    }
    SInt32 num = statusCode;
    CFNumberRef numRef = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
    if (!numRef) {
        return;
    }
    CFArrayAppendValue(array, numRef);
    CFRelease(numRef);
}
#endif

static CFArrayRef _SecTrustCopyDetails(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
#if TARGET_OS_OSX
    // Include status codes in the per-certificate details (rdar://27930542)
    CFMutableArrayRef newDetails = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!newDetails) {
        CFReleaseSafe(details);
        return NULL;
    }
    CFIndex index, chainLen = (details) ? CFArrayGetCount(details) : 0;
    for (index = 0; index < chainLen; index++) {
        CFDictionaryRef detailDict = CFArrayGetValueAtIndex(details, index);
        CFMutableDictionaryRef newDict = CFDictionaryCreateMutableCopy(NULL, 0, detailDict);
        CFMutableArrayRef statusCodes = CFArrayCreateMutable(kCFAllocatorDefault,
            0, &kCFTypeArrayCallBacks);
        if (statusCodes) {
            CFIndex i, numCodes = 0;
            SInt32 *codes = SecTrustCopyStatusCodes(trust, index, &numCodes);
            if (codes) {
                for (i = 0; i < numCodes; i++) {
                    OSStatus scode = (OSStatus)codes[i];
                    _AppendStatusCode(statusCodes, scode);
                }
                free(codes);
            }
            if (CFArrayGetCount(statusCodes) > 0) {
                CFDictionarySetValue(newDict, kSecCertificateDetailStatusCodes, statusCodes);
            }
            CFRelease(statusCodes);
        }
        if (newDict) {
            CFArrayAppendValue(newDetails, newDict);
            CFRelease(newDict);
        }
    }
    CFReleaseSafe(details);
    return newDetails;
#else
    return details;
#endif
}

CFDictionaryRef SecTrustCopyResult(SecTrustRef trust) {
    // Builds and returns a dictionary of evaluation results.
    if (!trust) {
        return NULL;
    }
    __block CFMutableDictionaryRef results = CFDictionaryCreateMutable(NULL, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = _SecTrustCopyDetails(trust);

    dispatch_sync(trust->_trustQueue, ^{
        // kSecTrustResultDetails (per-cert results)
        if (details) {
            CFDictionarySetValue(results, (const void *)kSecTrustResultDetails, (const void *)details);
            CFRelease(details);
        }

        // kSecTrustResultValue (overall trust result)
        CFNumberRef numValue = CFNumberCreate(NULL, kCFNumberSInt32Type, &trust->_trustResult);
        if (numValue) {
            CFDictionarySetValue(results, (const void *)kSecTrustResultValue, (const void *)numValue);
            CFRelease(numValue);
        }
        CFDictionaryRef info = trust->_info;
        if (trust->_trustResult == kSecTrustResultInvalid || !info) {
            return; // we have nothing more to add
        }

        // kSecTrustEvaluationDate
        CFDateRef evaluationDate = trust->_verifyDate;
        if (evaluationDate) {
            CFDictionarySetValue(results, (const void *)kSecTrustEvaluationDate, (const void *)evaluationDate);
        }

        // kSecTrustCertificateTransparency
        CFBooleanRef ctValue;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoCertificateTransparencyKey, (const void **)&ctValue)) {
            CFDictionarySetValue(results, (const void *)kSecTrustCertificateTransparency, (const void *)ctValue);
        }

        // kSecTrustExtendedValidation
        CFBooleanRef evValue;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoExtendedValidationKey, (const void **)&evValue)) {
            CFDictionarySetValue(results, (const void *)kSecTrustExtendedValidation, (const void *)evValue);
        }

        // kSecTrustOrganizationName
        CFStringRef organizationName;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoCompanyNameKey, (const void **)&organizationName)) {
            CFDictionarySetValue(results, (const void *)kSecTrustOrganizationName, (const void *)organizationName);
        }

        // kSecTrustRevocationChecked
        CFBooleanRef revocationChecked;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationChecked, (const void **)&revocationChecked)) {
            CFDictionarySetValue(results, (const void *)kSecTrustRevocationChecked, (const void *)revocationChecked);
        }

        // kSecTrustRevocationReason
        CFNumberRef revocationReason;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationReason, (const void **)&revocationReason)) {
            CFDictionarySetValue(results, (const void *)kSecTrustRevocationReason, (const void *)revocationReason);
        }

        // kSecTrustRevocationValidUntilDate
        CFDateRef validUntilDate;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationValidUntilDate, (const void **)&validUntilDate)) {
            CFDictionarySetValue(results, (const void *)kSecTrustRevocationValidUntilDate, (const void *)validUntilDate);
        }
    });

    return results;
}

#define do_if_registered(sdp, ...) if (gTrustd && gTrustd->sdp) { return gTrustd->sdp(__VA_ARGS__); }

static bool xpc_dictionary_entry_is_type(xpc_object_t dictionary, const char *key, xpc_type_t type) {
    xpc_object_t value = xpc_dictionary_get_value(dictionary, key);
    return value && (xpc_get_type(value) == type);
}

static uint64_t do_ota_pki_op (enum SecXPCOperation op, CFErrorRef *error) {
    uint64_t num = 0;
    xpc_object_t message = securityd_create_message(op, error);
    if (message) {
        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response && xpc_dictionary_entry_is_type(response, kSecXPCKeyResult, XPC_TYPE_UINT64)) {
            num = (int64_t) xpc_dictionary_get_uint64(response, kSecXPCKeyResult);
        }
        if (response && error && xpc_dictionary_entry_is_type(response, kSecXPCKeyError, XPC_TYPE_DICTIONARY)) {
            xpc_object_t xpc_error = xpc_dictionary_get_value(response, kSecXPCKeyError);
            if (xpc_error) {
                *error = SecCreateCFErrorWithXPCObject(xpc_error);
            }
        }
        xpc_release_safe(message);
        xpc_release_safe(response);
    }
    return num;
}

// version 0 -> error, so we need to start at version 1 or later.
uint64_t SecTrustGetTrustStoreVersionNumber(CFErrorRef *error) {
    do_if_registered(sec_ota_pki_trust_store_version, error);

    os_activity_t activity = os_activity_create("SecTrustGetTrustStoreVersionNumber", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    uint64_t num = do_ota_pki_op(sec_ota_pki_trust_store_version_id, error);

    os_release(activity);
    return num;
}

uint64_t SecTrustGetAssetVersionNumber(CFErrorRef *error) {
    do_if_registered(sec_ota_pki_asset_version, error);

    os_activity_t activity = os_activity_create("SecTrustGetAssetVersionNumber", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    uint64_t num = do_ota_pki_op(sec_ota_pki_asset_version_id, error);

    os_release(activity);
    return num;
}

uint64_t SecTrustOTAPKIGetUpdatedAsset(CFErrorRef *error) {
	do_if_registered(sec_ota_pki_get_new_asset, error);

    os_activity_t activity = os_activity_create("SecTrustOTAPKIGetUpdatedAsset", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    uint64_t num = do_ota_pki_op(kSecXPCOpOTAPKIGetNewAsset, error);

    os_release(activity);
	return num;
}

uint64_t SecTrustOTASecExperimentGetUpdatedAsset(CFErrorRef *error) {
    do_if_registered(sec_ota_secexperiment_get_new_asset, error);

    os_activity_t activity = os_activity_create("SecTrustOTASecExperimentGetUpdatedAsset", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    uint64_t num = do_ota_pki_op(kSecXPCOpOTASecExperimentGetNewAsset, error);

    os_release(activity);
    return num;
}

CFDictionaryRef SecTrustOTASecExperimentCopyAsset(CFErrorRef *error) {
    __block CFDictionaryRef result = NULL;
    do_if_registered(sec_ota_secexperiment_get_asset, error);

    securityd_send_sync_and_do(kSecXPCOpOTASecExperimentGetAsset, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        // input: set message parameters here
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        // output: get array from response object
        xpc_object_t xpc_dict = NULL;
        if (response) {
            xpc_dict = xpc_dictionary_get_value(response, kSecXPCKeyResult);
        }
        if (xpc_dict && (xpc_get_type(xpc_dict) == XPC_TYPE_DICTIONARY)) {
            result = (CFDictionaryRef)_CFXPCCreateCFObjectFromXPCObject(xpc_dict);
        } else {
            return SecError(errSecInternal, blockError, CFSTR("Unable to get SecExperiment Assets"));
        }
        return result != NULL;
    });
    return result;
}

bool SecTrustTriggerValidUpdate(CFErrorRef *error) {
    do_if_registered(sec_valid_update, error);

    os_activity_t activity = os_activity_create("SecTrustTriggerValidUpdate", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    uint64_t num = do_ota_pki_op(kSecXPCOpValidUpdate, error);

    os_release(activity);
    return num;
}

bool SecTrustReportTLSAnalytics(CFStringRef eventName, xpc_object_t eventAttributes, CFErrorRef *error) {
    if (!eventName || !eventAttributes) {
        return false;
    }
    do_if_registered(sec_networking_analytics_report, eventName, eventAttributes, error);

    os_activity_t activity = os_activity_create("SecTrustReportTLSAnalytics", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block bool result = false;
    securityd_send_sync_and_do(kSecXPCOpNetworkingAnalyticsReport, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        if (!SecXPCDictionarySetString(message, kSecTrustEventNameKey, eventName, block_error)) {
            return false;
        }
        xpc_dictionary_set_value(message, kSecTrustEventAttributesKey, eventAttributes);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, block_error);
        return true;
    });

    os_release(activity);
    return result;
}

bool SecTrustReportNetworkingAnalytics(const char *eventNameString, xpc_object_t eventAttributes) {
    if (!eventNameString || !eventAttributes) {
        return false;
    }

    CFStringRef eventName = CFStringCreateWithCString(kCFAllocatorDefault, eventNameString, kCFStringEncodingUTF8);
    if (!eventName) {
        secerror("CFStringCreateWithCString failed");
        return false;
    }

    CFErrorRef error = NULL;
    if (gTrustd && gTrustd->sec_networking_analytics_report) {
        bool result = gTrustd->sec_networking_analytics_report(eventName, eventAttributes, &error);
        if (error != NULL) {
            secerror("SecTrustReportNetworkingAnalytics failed with error: %d", (int)CFErrorGetCode(error));
        }
        CFReleaseNull(eventName);
        CFReleaseNull(error);
        return result;
    }

    os_activity_t activity = os_activity_create("SecTrustReportNetworkingAnalytics", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block bool result = false;
    securityd_send_sync_and_do(kSecXPCOpNetworkingAnalyticsReport, &error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        if (!SecXPCDictionarySetString(message, kSecTrustEventNameKey, eventName, block_error)) {
            return false;
        }
        xpc_dictionary_set_value(message, kSecTrustEventAttributesKey, eventAttributes);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, block_error);
        return true;
    });

    if (error != NULL) {
        secerror("SecTrustReportNetworkingAnalytics failed with error: %d", (int)CFErrorGetCode(error));
    }
    CFReleaseNull(error);
    CFReleaseNull(eventName);

    os_release(activity);
    return result;
}


/*
 * This function performs an evaluation of the leaf certificate only, and
 * does so in the process that called it. Its primary use is in SecItemCopyMatching
 * when kSecMatchPolicy is in the dictionary.
 */
OSStatus SecTrustEvaluateLeafOnly(SecTrustRef trust, SecTrustResultType *result) {
    if (!trust) {
        return errSecParam;
    }
    OSStatus status = errSecSuccess;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    if((status = SecTrustValidateInput(trust))) {
        return status;
    }

    struct OpaqueSecLeafPVC pvc;
    SecCertificateRef leaf = SecTrustGetCertificateAtIndex(trust, 0);
    __block CFArrayRef policies = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policies = CFRetainSafe(trust->_policies);
    });
    SecLeafPVCInit(&pvc, leaf, policies, SecTrustGetVerifyTime(trust));

    if(!SecLeafPVCLeafChecks(&pvc)) {
        trustResult = kSecTrustResultRecoverableTrustFailure;
    } else {
        trustResult = kSecTrustResultUnspecified;
    }

    /* Set trust result validity dates. This is a short window based on verifyTime. */
    CFAbsoluteTime resultNotBefore = SecTrustGetVerifyTime(trust);
    CFAbsoluteTime resultNotAfter = resultNotBefore + TRUST_TIME_LEEWAY;
    CFDateRef notBeforeDate = CFDateCreate(NULL, resultNotBefore);
    CFDateRef notAfterDate = CFDateCreate(NULL, resultNotAfter);

    /* Set other result context information */
    dispatch_sync(trust->_trustQueue, ^{
        trust->_trustResult = trustResult;
        trust->_details = CFRetainSafe(pvc.details);
        trust->_info = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
        CFMutableArrayRef leafCert = CFArrayCreateMutableCopy(NULL, 1, trust->_certificates);
        trust->_chain = leafCert;
        if (notBeforeDate) { CFDictionarySetValue(trust->_info, kSecTrustInfoResultNotBefore, notBeforeDate); }
        if (notAfterDate) { CFDictionarySetValue(trust->_info, kSecTrustInfoResultNotAfter, notAfterDate); }
    });

    SecLeafPVCDelete(&pvc);

    /* log to syslog when there is a trust failure */
    if (trustResult != kSecTrustResultUnspecified) {
        CFStringRef failureDesc = SecTrustCopyFailureDescription(trust);
        secerror("%@", failureDesc);
        CFRelease(failureDesc);
    }

    if (result) {
        *result = trustResult;
    }

    CFReleaseSafe(policies);
    CFReleaseSafe(notBeforeDate);
    CFReleaseSafe(notAfterDate);
    return status;
}

static void deserializeCert(const void *value, void *context) {
    CFDataRef certData = (CFDataRef)value;
    if (isData(certData)) {
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, certData);
        if (cert) {
            CFArrayAppendValue((CFMutableArrayRef)context, cert);
            CFRelease(cert);
        }
    }
}

static CF_RETURNS_RETAINED CFArrayRef SecCertificateArrayDeserialize(CFArrayRef serializedCertificates) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(serializedCertificates), errOut);
    CFIndex count = CFArrayGetCount(serializedCertificates);
    result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_certs = { 0, count };
    CFArrayApplyFunction(serializedCertificates, all_certs, deserializeCert, result);
errOut:
    return result;
}

static void serializeCertificate(const void *value, void *context) {
    SecCertificateRef cert = (SecCertificateRef)value;
    if (cert && SecCertificateGetTypeID() == CFGetTypeID(cert)) {
        CFDataRef certData = SecCertificateCopyData(cert);
        if (certData) {
            CFArrayAppendValue((CFMutableArrayRef)context, certData);
            CFRelease(certData);
        }
    }
}

static CF_RETURNS_RETAINED CFArrayRef SecCertificateArraySerialize(CFArrayRef certificates) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(certificates), errOut);
    CFIndex count = CFArrayGetCount(certificates);
    result = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    CFRange all_certificates = { 0, count};
    CFArrayApplyFunction(certificates, all_certificates, serializeCertificate, result);
errOut:
    return result;
}

static CFPropertyListRef SecTrustCopyPlist(SecTrustRef trust) {
    __block CFMutableDictionaryRef output = NULL;
    output = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);

    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_certificates) {
            CFArrayRef serializedCerts = SecCertificateArraySerialize(trust->_certificates);
            if (serializedCerts) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustCertificatesKey), serializedCerts);
                CFRelease(serializedCerts);
            }
        }
        if (trust->_anchors) {
            CFArrayRef serializedAnchors = SecCertificateArraySerialize(trust->_anchors);
            if (serializedAnchors) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsKey), serializedAnchors);
                CFRelease(serializedAnchors);
            }
        }
        if (trust->_policies) {
            CFArrayRef serializedPolicies = SecPolicyArrayCreateSerialized(trust->_policies);
            if (serializedPolicies) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustPoliciesKey), serializedPolicies);
                CFRelease(serializedPolicies);
            }
        }
        if (trust->_responses) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustResponsesKey), trust->_responses);
        }
        if (trust->_SCTs) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustSCTsKey), trust->_SCTs);
        }
        if (trust->_trustedLogs) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustTrustedLogsKey), trust->_trustedLogs);
        }
        if (trust->_verifyDate) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustVerifyDateKey), trust->_verifyDate);
        }
        if (trust->_chain) {
            CFArrayRef serializedCerts = SecCertificateArraySerialize(trust->_chain);
            if (serializedCerts) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustChainKey), serializedCerts);
                CFRelease(serializedCerts);
            }
        }
        if (trust->_details) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustDetailsKey), trust->_details);
        }
        if (trust->_info) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustInfoKey), trust->_info);
        }
        if (trust->_exceptions) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustExceptionsKey), trust->_exceptions);
        }
        CFNumberRef trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &trust->_trustResult);
        if (trustResult) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustResultKey), trustResult);
        }
        CFReleaseNull(trustResult);
        if (trust->_anchorsOnly) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsOnlyKey), kCFBooleanTrue);
        } else {
            CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsOnlyKey), kCFBooleanFalse);
        }
        if (trust->_keychainsAllowed) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustKeychainsAllowedKey), kCFBooleanTrue);
        } else {
            CFDictionaryAddValue(output, CFSTR(kSecTrustKeychainsAllowedKey), kCFBooleanFalse);
        }
    });

    return output;
}

CFDataRef SecTrustSerialize(SecTrustRef trust, CFErrorRef *error) {
    CFPropertyListRef plist = NULL;
    CFDataRef derTrust = NULL;
    require_action_quiet(trust, out,
                         SecError(errSecParam, error, CFSTR("null trust input")));
    require_action_quiet(plist = SecTrustCopyPlist(trust), out,
                         SecError(errSecDecode, error, CFSTR("unable to create trust plist")));
    require_quiet(derTrust = CFPropertyListCreateDERData(NULL, plist, error), out);

out:
    CFReleaseNull(plist);
    return derTrust;
}

static OSStatus SecTrustCreateFromPlist(CFPropertyListRef plist, SecTrustRef CF_RETURNS_RETAINED *trust) {
    OSStatus status = errSecParam;
    SecTrustRef output = NULL;
    CFTypeRef serializedCertificates = NULL, serializedPolicies = NULL, serializedAnchors = NULL;
    CFNumberRef trustResultNum = NULL;
    CFArrayRef certificates = NULL, policies = NULL, anchors = NULL, responses = NULL,
                SCTs = NULL, trustedLogs = NULL, details = NULL, exceptions = NULL, chain = NULL;
    CFDateRef verifyDate = NULL;
    CFDictionaryRef info = NULL;

    require_quiet(CFDictionaryGetTypeID() == CFGetTypeID(plist), out);
    require_quiet(serializedCertificates = CFDictionaryGetValue(plist, CFSTR(kSecTrustCertificatesKey)), out);
    require_quiet(certificates = SecCertificateArrayDeserialize(serializedCertificates), out);
    require_quiet(serializedPolicies = CFDictionaryGetValue(plist, CFSTR(kSecTrustPoliciesKey)), out);
    require_quiet(policies = SecPolicyArrayCreateDeserialized(serializedPolicies), out);
    require_noerr_quiet(status = SecTrustCreateWithCertificates(certificates, policies, &output), out);

    serializedAnchors = CFDictionaryGetValue(plist, CFSTR(kSecTrustAnchorsKey));
    if (isArray(serializedAnchors)) {
        anchors = SecCertificateArrayDeserialize(serializedAnchors);
        output->_anchors = anchors;
    }
    responses = CFDictionaryGetValue(plist, CFSTR(kSecTrustResponsesKey));
    if (isArray(responses)) {
        output->_responses = CFRetainSafe(responses);
    }
    SCTs = CFDictionaryGetValue(plist, CFSTR(kSecTrustSCTsKey));
    if (isArray(SCTs)) {
        output->_SCTs = CFRetainSafe(SCTs);
    }
    trustedLogs = CFDictionaryGetValue(plist, CFSTR(kSecTrustTrustedLogsKey));
    if (isArray(trustedLogs)) {
        output->_trustedLogs = CFRetainSafe(trustedLogs);
    }
    verifyDate = CFDictionaryGetValue(plist, CFSTR(kSecTrustVerifyDateKey));
    if (isDate(verifyDate)) {
        output->_verifyDate = CFRetainSafe(verifyDate);
    }
    chain = CFDictionaryGetValue(plist, CFSTR(kSecTrustChainKey));
    if (isArray(chain)) {
        output->_chain = SecCertificateArrayDeserialize(chain);
    }
    details = CFDictionaryGetValue(plist, CFSTR(kSecTrustDetailsKey));
    if (isArray(details)) {
        output->_details = CFRetainSafe(details);
    }
    info = CFDictionaryGetValue(plist, CFSTR(kSecTrustInfoKey));
    if (isDictionary(info)) {
        output->_info = CFRetainSafe(info);
    }
    exceptions = CFDictionaryGetValue(plist, CFSTR(kSecTrustExceptionsKey));
    if (isArray(exceptions)) {
        output->_exceptions = CFRetainSafe(exceptions);
    }
    int32_t trustResult = -1;
    trustResultNum = CFDictionaryGetValue(plist, CFSTR(kSecTrustResultKey));
    if (isNumber(trustResultNum) && CFNumberGetValue(trustResultNum, kCFNumberSInt32Type, &trustResult) &&
        (trustResult >= 0)) {
        output->_trustResult = trustResult;
    } else {
        status = errSecParam;
    }
    if (CFDictionaryGetValue(plist, CFSTR(kSecTrustAnchorsOnlyKey)) == kCFBooleanTrue) {
        output->_anchorsOnly = true;
    } /* false is set by default */
    if (CFDictionaryGetValue(plist, CFSTR(kSecTrustKeychainsAllowedKey)) == kCFBooleanFalse) {
        output->_keychainsAllowed = false;
    } /* true is set by default */

out:
    if (errSecSuccess == status && trust) {
        *trust = output;
    }
    CFReleaseNull(policies);
    CFReleaseNull(certificates);
    return status;
}

SecTrustRef SecTrustDeserialize(CFDataRef serializedTrust, CFErrorRef *error) {
    SecTrustRef trust = NULL;
    CFPropertyListRef plist = NULL;
    OSStatus status = errSecSuccess;
    require_action_quiet(serializedTrust, out,
                         SecError(errSecParam, error, CFSTR("null serialized trust input")));
    require_quiet(plist = CFPropertyListCreateWithDERData(NULL, serializedTrust,
                                                          kCFPropertyListImmutable, NULL, error), out);
    require_noerr_action_quiet(status = SecTrustCreateFromPlist(plist, &trust), out,
                               SecError(status, error, CFSTR("unable to create trust ref")));

out:
    CFReleaseNull(plist);
    return trust;
}

static uint64_t to_uint_error_request(enum SecXPCOperation op, CFErrorRef *error) {
    __block uint64_t result = 0;

    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *localError) {
        uint64_t temp_result =  xpc_dictionary_get_uint64(response, kSecXPCKeyResult);
        /* plists do not support unsigned integers. */
        if (temp_result <= INT64_MAX) {
            result = temp_result;
        } else {
            secerror("Invalid exceptions epoch.");
            if (error) {
                *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ERANGE, NULL);
            }
        }
        if (error && localError && *localError) {
            *error = *localError;
        }
        return result;
    });

    return result;
}

uint64_t SecTrustGetExceptionResetCount(CFErrorRef *error) {
    os_activity_t activity = os_activity_create("SecTrustExceptionGetResetCount", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    uint64_t exceptionResetCount = TRUSTD_XPC(sec_trust_get_exception_reset_count, to_uint_error_request, error);
    os_release(activity);
    if (error && *error) {
        secerror("Failed to get the exceptions epoch.");
    }
    secinfo("trust", "The exceptions epoch is %lld.", exceptionResetCount);

    return exceptionResetCount;
}

OSStatus SecTrustIncrementExceptionResetCount(CFErrorRef *error) {
    OSStatus status = errSecInternal;

    os_activity_t activity = os_activity_create("SecTrustIncrementExceptionResetCount", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    bool success = TRUSTD_XPC(sec_trust_increment_exception_reset_count, to_bool_error_request, error);
    os_release(activity);
    if ((error && *error) || !success) {
        secerror("Failed to increment the exceptions epoch.");
        return status;
    }
    status = errSecSuccess;

    return status;
}
