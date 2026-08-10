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
 * SecPolicyLeafCallbacks.c - Callbacks for SecPolicy for verifying leafs
 */

#include <AssertMacros.h>
#include <CoreFoundation/CFDictionary.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecInternalReleasePriv.h>
#include <wctype.h>
#include <dlfcn.h>
#include <libDER/oids.h>

/*
 * MARK: SecPolicyCheckCert Functions
 * All SecPolicyCheckCert* return false if the cert fails the check and true if it succeeds.
 */

typedef bool (*SecPolicyCheckCertFunction)(SecCertificateRef cert, CFTypeRef pvcValue);

/* This one is different from SecPolicyCheckCriticalExtensions because
 that one is an empty stub. The CriticalExtensions check is done in
 SecPolicyCheckBasicCertificateProcessing. */
bool SecPolicyCheckCertCriticalExtensions(SecCertificateRef cert, CFTypeRef __unused pvcValue) {
    if (SecCertificateHasUnknownCriticalExtension(cert)) {
        /* Certificate contains one or more unknown critical extensions. */
        return false;
    }
    return true;
}

static bool keyusage_allows(SecKeyUsage keyUsage, CFTypeRef xku) {
    if (!xku || CFGetTypeID(xku) != CFNumberGetTypeID())
        return false;

    SInt32 dku;
    CFNumberGetValue((CFNumberRef)xku, kCFNumberSInt32Type, &dku);
    SecKeyUsage ku = (SecKeyUsage)dku;
    /* kSecKeyUsageUnspecified is a special sentinel for allowing a missing KU extension */
    if (ku == kSecKeyUsageUnspecified) {
        return (keyUsage == ku);
    }
    return (keyUsage & ku) == ku;
}

bool SecPolicyCheckCertKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue) {
    SecKeyUsage keyUsage = SecCertificateGetKeyUsage(cert);
    bool match = false;
    CFTypeRef xku = pvcValue;
    if (isArray(xku)) {
        CFIndex ix, count = CFArrayGetCount(xku);
        for (ix = 0; ix < count; ++ix) {
            CFTypeRef ku = CFArrayGetValueAtIndex(xku, ix);
            if (keyusage_allows(keyUsage, ku)) {
                match = true;
                break;
            }
        }
    } else {
        match = keyusage_allows(keyUsage, xku);
    }
    return match;
}

static bool extendedkeyusage_allows(CFArrayRef extendedKeyUsage,
                                    CFDataRef xeku) {
    if (!xeku)
        return false;
    if (extendedKeyUsage) {
        CFRange all = { 0, CFArrayGetCount(extendedKeyUsage) };
        return CFArrayContainsValue(extendedKeyUsage, all, xeku);
    } else {
        /* Certificate has no extended key usage, only a match if the policy
         contains a 0 length CFDataRef. */
        return CFDataGetLength((CFDataRef)xeku) == 0;
    }
}

static bool isExtendedKeyUsageAllowed(CFArrayRef extendedKeyUsage,
                                      CFTypeRef xeku) {
    if (!xeku) {
        return false;
    }
    if(CFGetTypeID(xeku) == CFDataGetTypeID()) {
        return extendedkeyusage_allows(extendedKeyUsage, xeku);
    } else if (CFGetTypeID(xeku) == CFStringGetTypeID()) {
        CFDataRef eku = SecCertificateCreateOidDataFromString(NULL, xeku);
        if (eku) {
            bool result = extendedkeyusage_allows(extendedKeyUsage, eku);
            CFRelease(eku);
            return result;
        }
    }
    return false;
}

bool SecPolicyCheckCertExtendedKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFArrayRef certExtendedKeyUsage = SecCertificateCopyExtendedKeyUsage(cert);
    bool match = false;
    CFTypeRef xeku = pvcValue;
    if (isArray(xeku)) {
        CFIndex ix, count = CFArrayGetCount(xeku);
        for (ix = 0; ix < count; ix++) {
            CFTypeRef eku = CFArrayGetValueAtIndex(xeku, ix);
            if (isExtendedKeyUsageAllowed(certExtendedKeyUsage, eku)) {
                match = true;
                break;
            }
        }
    } else {
        match = isExtendedKeyUsageAllowed(certExtendedKeyUsage, xeku);
    }
    CFReleaseSafe(certExtendedKeyUsage);
    return match;
}

bool SecPolicyCheckCertNonEmptySubject(SecCertificateRef cert, CFTypeRef __unused pvcValue) {
    /* If the certificate has a subject, or
     if it doesn't, and it's the leaf and not a CA,
     and also has a critical subjectAltName extension it's valid. */
    if (!SecCertificateHasSubject(cert)) {
        if (SecCertificateIsCA(cert)) {
            /* CA certificate has empty subject. */
            return false;
        } else {
            if (!SecCertificateHasCriticalSubjectAltName(cert)) {
                /* Leaf certificate with empty subject does not have
                 a critical subject alt name extension. */
                return false;
            }
        }
    }
    return true;
}

/* We have a wildcard reference identifier that looks like "*." followed by 2 or
   more labels. Use CFNetwork's function for determining if those labels comprise
   a top-level domain. We need to dlopen since CFNetwork is a client of ours. */
typedef bool (*CFNIsTLD_f)(CFStringRef domain);
bool SecDNSIsTLD(CFStringRef reference) {
    bool result = false; /* fail open for allocation and symbol lookup failures */
    static CFNIsTLD_f CFNIsDomainTopLevelFunctionPtr = NULL;
    static dispatch_once_t onceToken;
    CFStringRef presentedDomain = NULL;

    dispatch_once(&onceToken, ^{
        void *framework = dlopen("/System/Library/Frameworks/CFNetwork.framework/CFNetwork", RTLD_LAZY);
        if (framework) {
            CFNIsDomainTopLevelFunctionPtr = dlsym(framework, "_CFHostIsDomainTopLevelForCertificatePolicy");
        }
    });

    require_quiet(CFNIsDomainTopLevelFunctionPtr, out);
    CFIndex referenceLen = CFStringGetLength(reference);

    /* reference identifier is too short, we should fail it */
    require_action_quiet(referenceLen > 2, out, result = true);

    require_quiet(presentedDomain = CFStringCreateWithSubstring(NULL, reference,
                                                                CFRangeMake(2, referenceLen - 2)),
                  out);
    result = CFNIsDomainTopLevelFunctionPtr(presentedDomain);

out:
    CFReleaseNull(presentedDomain);
    return result;
}

/* Compare hostname, to a server name obtained from the server's cert
 Obtained from the SubjectAltName or the CommonName entry in the Subject.
 Limited wildcard checking is performed here as outlined in RFC 6125
 Section 6.4.3.

 We adhere to the (SHOULD NOT) guidance in rules 1 and 2, and we choose
 never to accept partial-label wildcards even though they are allowed by
 rule 3.

 We use the language from RFC 6125, particularly the following definitions:

 presented identifier:  An identifier that is presented by a server to
 a client within a PKIX certificate when the client attempts to
 establish secure communication with the server; the certificate
 can include one or more presented identifiers of different types,
 and if the server hosts more than one domain then the certificate
 might present distinct identifiers for each domain.

 reference identifier:  An identifier, constructed from a source
 domain and optionally an application service type, used by the
 client for matching purposes when examining presented identifiers.

 */
static bool SecDNSMatch(CFStringRef reference, CFStringRef presented) {
    CFArrayRef referenceLabels = NULL, presentedLabels = NULL;
    bool result = false;

    /* A trailing '.' in the reference identifier is allowed as a mechanism
     to force TLS renegotiation. Strip it before parsing labels. */
    CFIndex referenceLen = CFStringGetLength(reference);
    require_quiet(referenceLen > 0, noMatch);
    if ('.' == CFStringGetCharacterAtIndex(reference, referenceLen - 1)) {
        CFStringRef truncatedReference = CFStringCreateWithSubstring(NULL, reference,
                                                                     CFRangeMake(0, referenceLen - 1));
        referenceLabels = CFStringCreateArrayBySeparatingStrings(NULL, truncatedReference, CFSTR("."));
        CFReleaseNull(truncatedReference);
        require_quiet(referenceLabels, noMatch);
    } else {
    require_quiet(referenceLabels = CFStringCreateArrayBySeparatingStrings(NULL, reference, CFSTR(".")),
                  noMatch);
    }

    require_quiet(presentedLabels = CFStringCreateArrayBySeparatingStrings(NULL, presented, CFSTR(".")),
                  noMatch);

    /* Reference Identifier and Presented Identifier must have the same number of labels
       because a wildcard in the presented identifier can only match a single label in the
       reference identifier. */
    require_quiet(CFArrayGetCount(referenceLabels) == CFArrayGetCount(presentedLabels), noMatch);

    CFIndex ix, count = CFArrayGetCount(referenceLabels);
    for (ix = count - 1; ix >= 0; ix--) {
        CFStringRef rlabel = NULL, plabel = NULL;
        require_quiet(rlabel = CFArrayGetValueAtIndex(referenceLabels, ix), noMatch);
        require_quiet(plabel = CFArrayGetValueAtIndex(presentedLabels, ix), noMatch);
        if (CFEqual(plabel, CFSTR("*"))) {
            /* must only occur in left-most label */
            require_quiet(ix == 0, noMatch);

            /* must not occur before single-label TLD */
            require_quiet(count > 2 && ix != count - 2, noMatch);

            /* must not occur before a multi-label gTLD */
            require_quiet(!SecDNSIsTLD(presented), noMatch);
        } else {
            /* partial-label wildcards are disallowed */
            CFRange partialRange = CFStringFind(plabel, CFSTR("*"), 0);
            require_quiet(partialRange.location == kCFNotFound && partialRange.length == 0 ,
                          noMatch);

            /* not a wildcard, so labels must match exactly */
            require_quiet(CFStringCompare(rlabel, plabel, kCFCompareCaseInsensitive) == kCFCompareEqualTo, noMatch);
        }
    }

    result = true;

noMatch:
    CFReleaseNull(referenceLabels);
    CFReleaseNull(presentedLabels);
    return result;
}

bool SecPolicyCheckCertSSLHostname(SecCertificateRef cert, CFTypeRef pvcValue) {
    /* @@@ Consider what to do if the caller passes in no hostname.  Should
     we then still fail if the leaf has no dnsNames or IPAddresses at all? */
    CFStringRef hostName = pvcValue;
    if (!isString(hostName)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }

    bool dnsMatch = false;
    CFArrayRef dnsNames = SecCertificateCopyDNSNamesFromSAN(cert);
    if (dnsNames) {
        CFIndex ix, count = CFArrayGetCount(dnsNames);
        for (ix = 0; ix < count; ++ix) {
            CFStringRef dns = (CFStringRef)CFArrayGetValueAtIndex(dnsNames, ix);
            if (SecDNSMatch(hostName, dns)) {
                dnsMatch = true;
                break;
            }
        }
        CFRelease(dnsNames);
    }

    if (!dnsMatch) {
        /* Check whether hostname is an IPv4 or IPv6 address */
        CFDataRef hostIPData = SecFrameworkCopyIPAddressData(hostName);
        if (hostIPData) {
            /* Check address against IP addresses in the SAN extension,
               obtained by SecCertificateCopyIPAddresses. Comparisons
               must always use the canonical data representation of the
               address, since string notation may omit zeros, etc. */
            CFArrayRef ipAddresses = SecCertificateCopyIPAddressDatas(cert);
            CFIndex ix, count = (ipAddresses) ? CFArrayGetCount(ipAddresses) : 0;
            for (ix = 0; ix < count && !dnsMatch; ++ix) {
                CFDataRef ipAddress = (CFDataRef)CFArrayGetValueAtIndex(ipAddresses, ix);
                if (CFEqualSafe(hostIPData, ipAddress)) {
                    dnsMatch = true;
                }
            }
            CFReleaseSafe(ipAddresses);
            CFReleaseSafe(hostIPData);
        }
    }

    return dnsMatch;
}

bool SecPolicyCheckCertEmail(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef email = pvcValue;
    bool match = false;
    if (!isString(email)) {
        /* We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }

    CFArrayRef addrs = SecCertificateCopyRFC822Names(cert);
    if (addrs) {
        CFIndex ix, count = CFArrayGetCount(addrs);
        for (ix = 0; ix < count; ++ix) {
            CFStringRef addr = (CFStringRef)CFArrayGetValueAtIndex(addrs, ix);
            if (!CFStringCompare(email, addr, kCFCompareCaseInsensitive)) {
                match = true;
                break;
            }
        }
        CFRelease(addrs);
    }

    return match;
}

bool SecPolicyCheckCertTemporalValidity(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFAbsoluteTime verifyTime = CFDateGetAbsoluteTime(pvcValue);
    if (!SecCertificateIsValid(cert, verifyTime)) {
        /* Leaf certificate has expired. */
        return false;
    }
    return true;
}

bool SecPolicyCheckCertSubjectCommonNamePrefix(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef prefix = pvcValue;
    bool match = true;
    if (!isString(prefix)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
    if (!commonNames || CFArrayGetCount(commonNames) != 1 ||
        !CFStringHasPrefix(CFArrayGetValueAtIndex(commonNames, 0), prefix)) {
        /* Common Name prefix mismatch. */
        match = false;
    }
    CFReleaseSafe(commonNames);
    return match;
}

bool SecPolicyCheckCertSubjectCommonName(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef common_name = pvcValue;
    bool match = true;
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
    if (!commonNames || CFArrayGetCount(commonNames) != 1 ||
        !CFEqual(common_name, CFArrayGetValueAtIndex(commonNames, 0))) {
        /* Common Name mismatch. */
        match = false;
    }
    CFReleaseSafe(commonNames);
    return match;
}

bool SecPolicyCheckCertSubjectCommonNameTEST(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef common_name = pvcValue;
    bool match = true;
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
    if (!commonNames || CFArrayGetCount(commonNames) != 1) {
        CFStringRef cert_common_name = CFArrayGetValueAtIndex(commonNames, 0);
        CFStringRef test_common_name = common_name ?
        CFStringCreateWithFormat(kCFAllocatorDefault,
                                 NULL, CFSTR("TEST %@ TEST"), common_name) :
        NULL;
        if (!CFEqual(common_name, cert_common_name) &&
            (!test_common_name || !CFEqual(test_common_name, cert_common_name)))
        /* Common Name mismatch. */
            match = false;
        CFReleaseSafe(test_common_name);
    }
    CFReleaseSafe(commonNames);
    return match;
}

bool SecPolicyCheckCertNotValidBefore(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFDateRef date = pvcValue;
    if (!isDate(date)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFAbsoluteTime at = CFDateGetAbsoluteTime(date);
    if (SecCertificateNotValidBefore(cert) <= at) {
        /* Leaf certificate has not valid before that is too old. */
        return false;
    }
    return true;
}

bool SecPolicyCheckCertSubjectOrganization(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef org = pvcValue;
    bool match = true;
    if (!isString(org)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef organization = SecCertificateCopyOrganization(cert);
    if (!organization || CFArrayGetCount(organization) != 1 ||
        !CFEqual(org, CFArrayGetValueAtIndex(organization, 0))) {
        /* Leaf Subject Organization mismatch. */
        match = false;
    }
    CFReleaseSafe(organization);
    return match;
}

bool SecPolicyCheckCertSubjectOrganizationalUnit(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef orgUnit = pvcValue;
    bool match = true;
    if (!isString(orgUnit)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef organizationalUnit = SecCertificateCopyOrganizationalUnit(cert);
    if (!organizationalUnit || CFArrayGetCount(organizationalUnit) != 1 ||
        !CFEqual(orgUnit, CFArrayGetValueAtIndex(organizationalUnit, 0))) {
        /* Leaf Subject Organizational Unit mismatch. */
        match = false;
    }
    CFReleaseSafe(organizationalUnit);
    return match;
}

bool SecPolicyCheckCertSubjectCountry(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFStringRef country = pvcValue;
    bool match = true;
    if (!isString(country)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }
    CFArrayRef certCountry = SecCertificateCopyCountry(cert);
    if (!certCountry || CFArrayGetCount(certCountry) != 1 ||
        !CFEqual(country, CFArrayGetValueAtIndex(certCountry, 0))) {
        /* Subject Country mismatch. */
        match = false;
    }
    CFReleaseSafe(certCountry);
    return match;
}

bool SecPolicyCheckCertEAPTrustedServerNames(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFArrayRef trustedServerNames = pvcValue;
    /* No names specified means we accept any name. */
    if (!trustedServerNames)
        return true;
    if (!isArray(trustedServerNames)) {
        /* @@@ We can't return an error here and making the evaluation fail
         won't help much either. */
        return false;
    }

    CFIndex tsnCount = CFArrayGetCount(trustedServerNames);
    bool dnsMatch = false;
    CFArrayRef dnsNames = SecCertificateCopyDNSNames(cert);
    if (dnsNames) {
        CFIndex ix, count = CFArrayGetCount(dnsNames);
        // @@@ This is O(N^2) unfortunately we can't do better easily unless
        // we don't do wildcard matching. */
        for (ix = 0; !dnsMatch && ix < count; ++ix) {
            CFStringRef dns = (CFStringRef)CFArrayGetValueAtIndex(dnsNames, ix);
            CFIndex tix;
            for (tix = 0; tix < tsnCount; ++tix) {
                CFStringRef serverName =
                (CFStringRef)CFArrayGetValueAtIndex(trustedServerNames, tix);
                if (!isString(serverName)) {
                    /* @@@ We can't return an error here and making the
                     evaluation fail won't help much either. */
                    CFReleaseSafe(dnsNames);
                    return false;
                }
                /* we purposefully reverse the arguments here such that dns names
                 from the cert are matched against a server name list, where
                 the server names list can contain wildcards and the dns name
                 cannot.  References: http://support.microsoft.com/kb/941123
                 It's easy to find occurrences where people tried to use
                 wildcard certificates and were told that those don't work
                 in this context. */
                if (SecDNSMatch(dns, serverName)) {
                    dnsMatch = true;
                    break;
                }
            }
        }
        CFRelease(dnsNames);
    }

    return dnsMatch;
}

bool SecPolicyCheckCertLeafMarkerOid(SecCertificateRef cert, CFTypeRef pvcValue) {
    if (pvcValue && SecCertificateHasMarkerExtension(cert, pvcValue)) {
        return true;
    }

    return false;
}

bool SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(SecCertificateRef cert,
                                                      CFTypeRef pvcValue) {
    if (CFGetTypeID(pvcValue) == CFArrayGetTypeID()) {
        CFIndex ix, length = CFArrayGetCount(pvcValue);
        for (ix = 0; ix < length; ix++)
            if (SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(cert,
                    CFArrayGetValueAtIndex((CFArrayRef)pvcValue, ix))) {
                return true;
            }
    } else if (CFGetTypeID(pvcValue) == CFDataGetTypeID()  ||
               CFGetTypeID(pvcValue) == CFStringGetTypeID()) {
        return (NULL != SecCertificateGetExtensionValue(cert, pvcValue));
    }
    return false;
}

/*
 * The value is a dictionary. The dictionary contains keys indicating
 * whether the value is for Prod or QA. The values are the same as
 * in the options dictionary for SecPolicyCheckLeafMarkerOid.
 */
bool SecPolicyCheckCertLeafMarkersProdAndQA(SecCertificateRef cert, CFTypeRef pvcValue)
{
    CFTypeRef prodValue = CFDictionaryGetValue(pvcValue, kSecPolicyLeafMarkerProd);

    if (!SecPolicyCheckCertLeafMarkerOid(cert, prodValue)) {
        bool result = false;
        return result;
    }
    return true;
}

static CFSetRef copyCertificatePolicies(SecCertificateRef cert) {
    CFMutableSetRef policies = NULL;
    policies = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    if (!policies) return NULL;

    const SecCECertificatePolicies *cp = SecCertificateGetCertificatePolicies(cert);
    size_t policy_ix, policy_count = cp ? cp->numPolicies : 0;
    for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
        CFDataRef oidData = NULL;
        DERItem *policyOID = &cp->policies[policy_ix].policyIdentifier;
        oidData = CFDataCreate(kCFAllocatorDefault, policyOID->data, policyOID->length);
        CFSetAddValue(policies, oidData);
        CFReleaseSafe(oidData);
    }
    return policies;
}

static bool checkPolicyOidData(SecCertificateRef cert , CFDataRef oid) {
    CFSetRef policies = copyCertificatePolicies(cert);
    bool found = false;
    if (policies && CFSetContainsValue(policies, oid)) {
        found = true;
    }
    CFReleaseSafe(policies);
    return found;
}

/* This one is different from SecPolicyCheckCertificatePolicyOid because
   that one checks the whole chain. (And uses policy_set_t...) */
bool SecPolicyCheckCertCertificatePolicy(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFTypeRef value = pvcValue;
    bool result = false;

    if (CFGetTypeID(value) == CFDataGetTypeID())
    {
        result = checkPolicyOidData(cert, value);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, value);
        if (dataOid) {
            result = checkPolicyOidData(cert, dataOid);
            CFRelease(dataOid);
        }
    }
    return result;
}

bool SecPolicyCheckCertWeakKeySize(SecCertificateRef cert, CFTypeRef __unused pvcValue) {
    if (cert && SecCertificateIsWeakKey(cert)) {
        /* Leaf certificate has a weak key. */
        return false;
    }
    return true;
}

bool SecPolicyCheckCertKeySize(SecCertificateRef cert, CFTypeRef pvcValue) {
    CFDictionaryRef keySizes = pvcValue;
    if (!SecCertificateIsAtLeastMinKeySize(cert, keySizes)) {
        return false;
    }
    return true;
}

bool SecPolicyCheckCertWeakSignature(SecCertificateRef cert, CFTypeRef __unused pvcValue) {
    bool result = true;
    CFMutableArrayRef disallowedHashes = CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks);
    if (!disallowedHashes) {
        return result;
    }
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD2);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD4);
    CFArrayAppendValue(disallowedHashes, kSecSignatureDigestAlgorithmMD5);

    /* Weak Signature failures only for non-self-signed certs */
    Boolean isSelfSigned = false;
    OSStatus status = SecCertificateIsSelfSigned(cert, &isSelfSigned);
    if (!SecPolicyCheckCertSignatureHashAlgorithms(cert, disallowedHashes) && (status != errSecSuccess || !isSelfSigned)) {
        result = false;
    }
    CFReleaseSafe(disallowedHashes);
    return result;
}

static CFStringRef convertSignatureHashAlgorithm(SecSignatureHashAlgorithm algorithmEnum) {
    const void *digests[] = { kSecSignatureDigestAlgorithmUnknown,
        kSecSignatureDigestAlgorithmMD2,
        kSecSignatureDigestAlgorithmMD4,
        kSecSignatureDigestAlgorithmMD5,
        kSecSignatureDigestAlgorithmSHA1,
        kSecSignatureDigestAlgorithmSHA224,
        kSecSignatureDigestAlgorithmSHA256,
        kSecSignatureDigestAlgorithmSHA384,
        kSecSignatureDigestAlgorithmSHA512,
    };
    return digests[algorithmEnum];
}

bool SecPolicyCheckCertSignatureHashAlgorithms(SecCertificateRef cert, CFTypeRef pvcValue) {
    /* skip signature algorithm check on self-signed certs */
    Boolean isSelfSigned = false;
    OSStatus status = SecCertificateIsSelfSigned(cert, &isSelfSigned);
    if ((status == errSecSuccess) && isSelfSigned) {
        return true;
    }

    CFArrayRef disallowedHashAlgorithms = pvcValue;
    CFStringRef certAlg = convertSignatureHashAlgorithm(SecCertificateGetSignatureHashAlgorithm(cert));
    if (CFArrayContainsValue(disallowedHashAlgorithms, CFRangeMake(0,CFArrayGetCount(disallowedHashAlgorithms)), certAlg)) {
        return false;
    }
    return true;
}

bool SecPolicyCheckCertUnparseableExtension(SecCertificateRef cert, CFTypeRef pvcValue) {
    if (SecCertificateGetUnparseableKnownExtension(cert) != kCFNotFound) {
        return false;
    }
    return true;
}

bool SecPolicyCheckCertNotCA(SecCertificateRef cert, CFTypeRef pvcValue) {
    if (SecCertificateIsCA(cert)) {
        return false;
    }
    return true;
}

/*
 * MARK: SecLeafPVC functions
 */
static CFDictionaryRef SecLeafPVCCopyCallbacks(void) {
    CFMutableDictionaryRef leafCallbacks = NULL;
    leafCallbacks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                        &kCFTypeDictionaryKeyCallBacks, NULL);

#undef POLICYCHECKMACRO
#define __PC_ADD_CHECK_(NAME)
#define __PC_ADD_CHECK_O(NAME) CFDictionaryAddValue(leafCallbacks, \
kSecPolicyCheck##NAME, SecPolicyCheckCert##NAME);

#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
__PC_ADD_CHECK_##LEAFONLY(NAME)
#include "SecPolicyChecks.list"

    return leafCallbacks;
}

void SecLeafPVCInit(SecLeafPVCRef pvc, SecCertificateRef leaf, CFArrayRef policies,
                    CFAbsoluteTime verifyTime) {
    secdebug("alloc", "leafpvc %p", pvc);
    // Weird logging policies crashes.
    //secdebug("policy", "%@", policies);
    pvc->leaf = CFRetainSafe(leaf);
    pvc->policies = CFRetainSafe(policies);
    pvc->verifyTime = verifyTime;
    pvc->callbacks = SecLeafPVCCopyCallbacks();
    pvc->policyIX = 0;
    pvc->result = true;

    CFMutableDictionaryRef certDetail = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
    pvc->details = CFArrayCreate(kCFAllocatorDefault, (const void **)&certDetail, 1,
                                 &kCFTypeArrayCallBacks);
    CFRelease(certDetail);
}


void SecLeafPVCDelete(SecLeafPVCRef pvc) {
    secdebug("alloc", "delete leaf pvc %p", pvc);
    CFReleaseNull(pvc->policies);
    CFReleaseNull(pvc->details);
    CFReleaseNull(pvc->callbacks);
    CFReleaseNull(pvc->leaf);
}

static bool SecLeafPVCSetResultForced(SecLeafPVCRef pvc,
                                      CFStringRef key, CFIndex ix, CFTypeRef result, bool force) {

    secdebug("policy", "cert[%d]: %@ =(%s)[%s]> %@", (int) ix, key, "leaf",
             (force ? "force" : ""), result);

    /* If this is not something the current policy cares about ignore
     this error and return true so our caller continues evaluation. */
    if (!force) {
        /* @@@ The right long term fix might be to check if none of the passed
         in policies contain this key, since not all checks are run for all
         policies. */
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, pvc->policyIX);
        if (policy && !CFDictionaryContainsKey(policy->_options, key))
            return true;
    }

    /* @@@ Check to see if the SecTrustSettings for the certificate in question
     tell us to ignore this error. */
    pvc->result = false;
    if (!pvc->details)
        return false;

    CFMutableDictionaryRef detail =
    (CFMutableDictionaryRef)CFArrayGetValueAtIndex(pvc->details, ix);

    /* Perhaps detail should have an array of results per key?  As it stands
     in the case of multiple policy failures the last failure stands.  */
    CFDictionarySetValue(detail, key, result);

    return true;
}

static bool SecLeafPVCSetResult(SecLeafPVCRef pvc,
                                CFStringRef key, CFIndex ix, CFTypeRef result) {
    return SecLeafPVCSetResultForced(pvc, key, ix, result, false);
}

static void SecLeafPVCValidateKey(const void *key, const void *value,
                                  void *context) {
    SecLeafPVCRef pvc = (SecLeafPVCRef)context;

    /* If our caller doesn't want full details and we failed earlier there is
     no point in doing additional checks. */
    if (!pvc->result && !pvc->details)
        return;

    SecPolicyCheckCertFunction fcn = (SecPolicyCheckCertFunction) CFDictionaryGetValue(pvc->callbacks, key);
    if (!fcn) {
        pvc->result = false;
        return;
    }

    /* kSecPolicyCheckTemporalValidity is special */
    if (CFEqual(key, kSecPolicyCheckTemporalValidity)) {
        CFDateRef verifyDate = CFDateCreate(NULL, pvc->verifyTime);
        if(!fcn(pvc->leaf, verifyDate)) {
            SecLeafPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
        CFReleaseSafe(verifyDate);
    } else {
        /* get pvcValue from current policy */
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, pvc->policyIX);
        if (!policy) {
            pvc->result = false;
            return;
        }
        CFTypeRef pvcValue = (CFTypeRef)CFDictionaryGetValue(policy->_options, key);
        if(!fcn(pvc->leaf, pvcValue)) {
            SecLeafPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
    }
}

bool SecLeafPVCLeafChecks(SecLeafPVCRef pvc) {
    pvc->result = true;
    CFArrayRef policies = pvc->policies;
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, ix);
        pvc->policyIX = ix;
        /* Validate all keys for all policies. */
        CFDictionaryApplyFunction(policy->_options, SecLeafPVCValidateKey, pvc);
        if (!pvc->result && !pvc->details)
            return pvc->result;
    }
    return pvc->result;
}
