/*
 * Copyright (c) 2017-2020 Apple Inc. All Rights Reserved.
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
 * SecCertificateServer.c - SecCertificate and SecCertificatePathVC types
 *  with additonal validation context.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <AssertMacros.h>

#include <libDER/libDER.h>
#include <libDER/oids.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <Security/SecTrustSettingsPriv.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "trust/trustd/policytree.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/SecRevocationServer.h"
#include "trust/trustd/SecTrustStoreServer.h"

// MARK: -
// MARK: SecCertificateVC
/********************************************************
 ************* SecCertificateVC object ***************
 ********************************************************/

struct SecCertificateVC {
    CFRuntimeBase       _base;
    SecCertificateRef   certificate;
    CFArrayRef          usageConstraints;
    CFNumberRef         revocationReason;
    bool                optionallyEV;
    bool                isWeakHash;
    bool                require_revocation_response;
};
CFGiblisWithHashFor(SecCertificateVC)

static void SecCertificateVCDestroy(CFTypeRef cf) {
    SecCertificateVCRef cvc = (SecCertificateVCRef) cf;
    CFReleaseNull(cvc->certificate);
    CFReleaseNull(cvc->usageConstraints);
    CFReleaseNull(cvc->revocationReason);
}

static Boolean SecCertificateVCCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SecCertificateVCRef cv1 = (SecCertificateVCRef) cf1;
    SecCertificateVCRef cv2 = (SecCertificateVCRef) cf2;
    if (!CFEqual(cv1->certificate, cv2->certificate)) {
        return false;
    }
    /* CertificateVCs are the same if either does not have usage constraints. */
    if (cv1->usageConstraints && cv2->usageConstraints &&
         !CFEqual(cv1->usageConstraints, cv2->usageConstraints)) {
        return false;
    }

    return true;
}

static CFHashCode SecCertificateVCHash(CFTypeRef cf) {
    SecCertificateVCRef cvc = (SecCertificateVCRef) cf;
    CFHashCode hashCode = 0;
    hashCode += CFHash(cvc->certificate);
    if (cvc->usageConstraints) {
        hashCode += CFHash(cvc->usageConstraints);
    }
    return hashCode;
}

static CFStringRef SecCertificateVCCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecCertificateVCRef cvc = (SecCertificateVCRef)cf;
    return CFCopyDescription(cvc->certificate);
}

static bool SecCertificateVCCouldBeEV(SecCertificateRef certificate) {
    CFMutableDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;

    /* 3. Subscriber Certificate. */

    /* (a) certificate Policies */
    const SecCECertificatePolicies *cp;
    cp = SecCertificateGetCertificatePolicies(certificate);
    require_quiet(cp && cp->numPolicies > 0, notEV);
    /* Now find at least one policy in here that has a qualifierID of id-qt 2
     and a policyQualifier that is a URI to the CPS and an EV policy OID. */
    uint32_t ix = 0;
    bool found_ev_anchor_for_leaf_policy = false;
    for (ix = 0; ix < cp->numPolicies; ++ix) {
        if (SecPolicyIsEVPolicy(&cp->policies[ix].policyIdentifier)) {
            found_ev_anchor_for_leaf_policy = true;
        }
    }
    require_quiet(found_ev_anchor_for_leaf_policy, notEV);

    /* (b) cRLDistributionPoint
     (c) authorityInformationAccess
     BRv1.3.4: MUST be present with OCSP Responder unless stapled response.
     */

    /* (d) basicConstraints
     If present, the cA field MUST be set false. */
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    if (bc) {
        require_action_quiet(bc->isCA == false, notEV,
                             secnotice("ev", "Leaf has invalid basic constraints"));
    }

    /* (e) keyUsage. */
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    if (ku) {
        require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)) == 0, notEV,
                             secnotice("ev", "Leaf has invalid key usage %u", ku));
    }

#if 0
    /* The EV Cert Spec errata specifies this, though this is a check for SSL
     not specifically EV. */

    /* (e) extKeyUsage

     Either the value id-kp-serverAuth [RFC5280] or id-kp-clientAuth [RFC5280] or both values MUST be present. Other values SHOULD NOT be present. */
    SecCertificateCopyExtendedKeyUsage(certificate);
#endif

    /* 6.1.5 Key Sizes */
    CFAbsoluteTime jan2014 = 410227200;
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    require_quiet(keySizes = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks), notEV);
    CFDictionaryAddValue(keySizes, kSecAttrKeyTypeEC, ecSize);
    if (SecCertificateNotValidBefore(certificate) < jan2014) {
        /* At least RSA 1024 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 1024), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "Leaf's public key is too small for issuance before 2014"));
    } else {
        /* At least RSA 2028 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "Leaf's public key is too small for issuance after 2013"));
    }

    /* 6.3.2 Validity Periods */
    // Will be checked by the policy server (see SecPolicyCheckValidityPeriodMaximums)

    /* 7.1.3 Algorithm Object Identifiers */
    CFAbsoluteTime jan2016 = 473299200;
    if (SecCertificateNotValidBefore(certificate) > jan2016) {
        /* SHA-2 only */
        require_action_quiet(SecCertificateGetSignatureHashAlgorithm(certificate) > kSecSignatureHashAlgorithmSHA1,
                             notEV, secnotice("ev", "Leaf was issued with SHA-1 after 2015"));
    }

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}


SecCertificateVCRef SecCertificateVCCreate(SecCertificateRef certificate, CFArrayRef usageConstraints) {
    if (!certificate) { return NULL; }
    CFIndex size = sizeof(struct SecCertificateVC);
    SecCertificateVCRef result =
    (SecCertificateVCRef)_CFRuntimeCreateInstance(kCFAllocatorDefault,
                                                      SecCertificateVCGetTypeID(), size - sizeof(CFRuntimeBase), 0);
    if (!result)
        return NULL;
    result->certificate = CFRetainSafe(certificate);
    result->isWeakHash = SecCertificateIsWeakHash(certificate);
    result->optionallyEV = SecCertificateVCCouldBeEV(certificate);

    CFArrayRef emptyArray = NULL;
    if (!usageConstraints) {
        require_action_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit, CFReleaseNull(result));
        usageConstraints = emptyArray;
    }
    result->usageConstraints = CFRetainSafe(usageConstraints);
exit:
    CFReleaseNull(emptyArray);
    return result;
}

// MARK: -
// MARK: SecCertificatePathVC
/********************************************************
 ************* SecCertificatePathVC object ***************
 ********************************************************/
struct SecCertificatePathVC {
    CFRuntimeBase       _base;
    CFIndex             count;

    /* Index of next parent source to search for parents. */
    CFIndex             nextParentSource;

    /* Index of last certificate in chain whose signature has been verified.
     0 means nothing has been checked.  1 means the leaf has been verified
     against its issuer, etc. */
    CFIndex             lastVerifiedSigner;

    /* Index of first self issued certificate in the chain.  -1 mean there is
     none.  0 means the leaf is self signed.  */
    CFIndex             selfIssued;

    /* True iff cert at index selfIssued does in fact self verify. */
    bool                isSelfSigned;

    /* True if the root of this path is an anchor. Trustedness of the
     * anchor is determined by the PVC. */
    bool                isAnchored;

    policy_tree_t       policy_tree;
    uint8_t             policy_tree_verification_result;

    bool                isEV;
    bool                isCT;
    bool                is_allowlisted;
    bool                hasStrongHashes;

    /* revocationCAIndex contains the index of the last CA with a SPKI-based
     * revocation match, or -1 (kCFNotFound) if no CA match was found.
     * A value of 0 means the path has not yet been checked for a match. */
    CFIndex             revocationCAIndex;

    void *              rvcs;
    CFIndex             rvcCount;

    /* This is the score of the path after determining acceptance. */
    CFIndex             score;

    bool                pathValidated;

    /* If checkedIssuers is true, then the value of unknownCAIndex contains
     * the index of the first CA which violates known-only constraints, or
     * -1 if all CA certificates are either known or not constrained. */
    bool                checkedIssuers;
    CFIndex             unknownCAIndex;

    /* Enumerated value to determine whether CT is required for the leaf
     * certificate (because a CA in the path has a require-ct constraint).
     * If non-zero, CT is required; value indicates overridable status. */
    SecPathCTPolicy     requiresCT;

    /* Issuance time, as determined by earliest SCT timestamp for leaf. */
    CFAbsoluteTime      issuanceTime;

    SecCertificateVCRef certificates[];
};
CFGiblisWithHashFor(SecCertificatePathVC)

static void SecCertificatePathVCPrunePolicyTree(SecCertificatePathVCRef certificatePath) {
    if (certificatePath->policy_tree) {
        policy_tree_prune(&certificatePath->policy_tree);
    }
}

void SecCertificatePathVCDeleteRVCs(SecCertificatePathVCRef path) {
    if (path->rvcs) {
        CFIndex certIX, certCount = path->rvcCount;
        for (certIX = 0; certIX < certCount; ++certIX) {
            SecRVCRef rvc = &((SecRVCRef)path->rvcs)[certIX];
            SecRVCDelete(rvc);
        }
        free(path->rvcs);
        path->rvcs = NULL;
    }
}

static void SecCertificatePathVCDestroy(CFTypeRef cf) {
    SecCertificatePathVCRef certificatePath = (SecCertificatePathVCRef) cf;
    CFIndex ix;
    SecCertificatePathVCDeleteRVCs(certificatePath);
    SecCertificatePathVCPrunePolicyTree(certificatePath);
    for (ix = 0; ix < certificatePath->count; ++ix) {
        CFReleaseNull(certificatePath->certificates[ix]);
    }
}

static Boolean SecCertificatePathVCCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SecCertificatePathVCRef cp1 = (SecCertificatePathVCRef) cf1;
    SecCertificatePathVCRef cp2 = (SecCertificatePathVCRef) cf2;
    if (cp1->count != cp2->count)
        return false;
    CFIndex ix;
    for (ix = 0; ix < cp1->count; ++ix) {
        if (!CFEqual(cp1->certificates[ix], cp2->certificates[ix]))
            return false;
    }

    return true;
}

static CFHashCode SecCertificatePathVCHash(CFTypeRef cf) {
    SecCertificatePathVCRef certificatePath = (SecCertificatePathVCRef) cf;
    CFHashCode hashCode = 0;
    CFIndex ix;
    for (ix = 0; ix < certificatePath->count; ++ix) {
        hashCode += CFHash(certificatePath->certificates[ix]);
    }
    return hashCode;
}

static CFStringRef SecCertificatePathVCCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecCertificatePathVCRef certificatePath = (SecCertificatePathVCRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
                         CFSTR("<%@ certs: "), typeStr);
    CFRelease(typeStr);
    CFIndex ix;
    for (ix = 0; ix < certificatePath->count; ++ix) {
        if (ix > 0) {
            CFStringAppend(desc, CFSTR(", "));
        }
        CFStringRef str = CFCopyDescription(certificatePath->certificates[ix]);
        CFStringAppend(desc, str);
        CFRelease(str);
    }
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}

/* Create a new certificate path from an old one. */
SecCertificatePathVCRef SecCertificatePathVCCreate(SecCertificatePathVCRef path,
                                               SecCertificateRef certificate, CFArrayRef usageConstraints) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
    check(certificate);
    CFIndex count;
    CFIndex selfIssued, lastVerifiedSigner;
    bool isSelfSigned;
    if (path) {
        count = path->count + 1;
        lastVerifiedSigner = path->lastVerifiedSigner;
        selfIssued = path->selfIssued;
        isSelfSigned = path->isSelfSigned;
    } else {
        count = 1;
        lastVerifiedSigner = 0;
        selfIssued = -1;
        isSelfSigned = false;
    }

    CFIndex size = sizeof(struct SecCertificatePathVC) +
    count * sizeof(SecCertificateRef);
    SecCertificatePathVCRef result =
    (SecCertificatePathVCRef)_CFRuntimeCreateInstance(allocator,
                                                    SecCertificatePathVCGetTypeID(), size - sizeof(CFRuntimeBase), 0);
    if (!result)
        return NULL;

    memset((char*)result + sizeof(result->_base), 0,
           sizeof(*result) - sizeof(result->_base));

    result->count = count;
    result->lastVerifiedSigner = lastVerifiedSigner;
    result->selfIssued = selfIssued;
    result->isSelfSigned = isSelfSigned;
    CFIndex ix;
    for (ix = 0; ix < count - 1; ++ix) {
        result->certificates[ix] = path->certificates[ix];
        CFRetain(result->certificates[ix]);
    }

    SecCertificateVCRef cvc = SecCertificateVCCreate(certificate, usageConstraints);
    result->certificates[count - 1] = cvc;

    return result;
}

SecCertificatePathVCRef SecCertificatePathVCCopyFromParent(
                                                       SecCertificatePathVCRef path, CFIndex skipCount) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFIndex count;
    CFIndex selfIssued, lastVerifiedSigner;
    bool isSelfSigned;

    /* Ensure we are at least returning a path of length 1. */
    if (skipCount < 0 || path->count < 1 + skipCount)
        return NULL;

    count = path->count - skipCount;
    lastVerifiedSigner = path->lastVerifiedSigner > skipCount
    ? path->lastVerifiedSigner - skipCount : 0;
    selfIssued = path->selfIssued >= skipCount
    ? path->selfIssued - skipCount : -1;
    isSelfSigned = path->selfIssued >= 0 ? path->isSelfSigned : false;

    CFIndex size = sizeof(struct SecCertificatePathVC) +
    count * sizeof(SecCertificateRef);
    SecCertificatePathVCRef result =
    (SecCertificatePathVCRef)_CFRuntimeCreateInstance(allocator,
                                                    SecCertificatePathVCGetTypeID(), size - sizeof(CFRuntimeBase), 0);
    if (!result)
        return NULL;

    memset((char*)result + sizeof(result->_base), 0,
           sizeof(*result) - sizeof(result->_base));

    result->count = count;
    result->lastVerifiedSigner = lastVerifiedSigner;
    result->selfIssued = selfIssued;
    result->isSelfSigned = isSelfSigned;
    result->isAnchored = path->isAnchored;
    CFIndex ix;
    for (ix = 0; ix < count; ++ix) {
        CFIndex pathIX = ix + skipCount;
        result->certificates[ix] = path->certificates[pathIX];
        CFRetain(result->certificates[ix]);
    }

    return result;
}

SecCertificatePathVCRef SecCertificatePathVCCopyAddingLeaf(SecCertificatePathVCRef path,
                                                       SecCertificateRef leaf) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFIndex count;
    CFIndex selfIssued, lastVerifiedSigner;
    bool isSelfSigned;

    /* First make sure the new leaf is signed by path's current leaf. */
    SecKeyRef issuerKey = SecCertificatePathVCCopyPublicKeyAtIndex(path, 0);
    if (!issuerKey)
        return NULL;
    OSStatus status = SecCertificateIsSignedBy(leaf, issuerKey);
    CFRelease(issuerKey);
    if (status)
        return NULL;

    count = path->count + 1;
    lastVerifiedSigner = path->lastVerifiedSigner + 1;
    selfIssued = path->selfIssued;
    isSelfSigned = path->isSelfSigned;

    CFIndex size = sizeof(struct SecCertificatePathVC) +
    count * sizeof(SecCertificateRef);
    SecCertificatePathVCRef result =
    (SecCertificatePathVCRef)_CFRuntimeCreateInstance(allocator,
                                                    SecCertificatePathVCGetTypeID(), size - sizeof(CFRuntimeBase), 0);
    if (!result)
        return NULL;

    memset((char*)result + sizeof(result->_base), 0,
           sizeof(*result) - sizeof(result->_base));

    result->count = count;
    result->lastVerifiedSigner = lastVerifiedSigner;
    result->selfIssued = selfIssued;
    result->isSelfSigned = isSelfSigned;
    result->isAnchored = path->isAnchored;

    CFIndex ix;
    for (ix = 1; ix < count; ++ix) {
        result->certificates[ix] = path->certificates[ix - 1];
        CFRetain(result->certificates[ix]);
    }
    SecCertificateVCRef leafVC = SecCertificateVCCreate(leaf, NULL);
    result->certificates[0] = leafVC;

    return result;
}

CFArrayRef SecCertificatePathVCCopyCertificates(SecCertificatePathVCRef path) {
    CFMutableArrayRef outCerts = NULL;
    size_t count = path->count;
    require_quiet(outCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit);
    SecCertificatePathVCForEachCertificate(path, ^(SecCertificateRef cert, bool * __unused stop) {
        CFArrayAppendValue(outCerts, cert);
    });
exit:
    return outCerts;
}

CFArrayRef SecCertificatePathVCCreateSerialized(SecCertificatePathVCRef path) {
    CFMutableArrayRef serializedCerts = NULL;
    require_quiet(path, exit);
    size_t count = path->count;
    require_quiet(serializedCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit);
    SecCertificatePathVCForEachCertificate(path, ^(SecCertificateRef cert, bool * __unused stop) {
        CFDataRef certData = SecCertificateCopyData(cert);
        if (certData) {
            CFArrayAppendValue(serializedCerts, certData);
            CFRelease(certData);
        }
    });
exit:
    return serializedCerts;
}


/* Record the fact that we found our own root cert as our parent
 certificate. */
void SecCertificatePathVCSetSelfIssued(
                                     SecCertificatePathVCRef certificatePath) {
    if (certificatePath->selfIssued >= 0) {
        secdebug("trust", "%@ is already issued at %" PRIdCFIndex, certificatePath,
                 certificatePath->selfIssued);
        return;
    }
    secdebug("trust", "%@ is self issued", certificatePath);
    certificatePath->selfIssued = certificatePath->count - 1;

    /* now check that the selfIssued cert was actually self-signed */
    if (certificatePath->selfIssued >= 0 && !certificatePath->isSelfSigned) {
        SecCertificateVCRef certVC = certificatePath->certificates[certificatePath->selfIssued];
        Boolean isSelfSigned = false;
        OSStatus status = SecCertificateIsSelfSigned(certVC->certificate, &isSelfSigned);
        if ((status == errSecSuccess) && isSelfSigned) {
            certificatePath->isSelfSigned = true;
        } else {
            certificatePath->selfIssued = -1;
        }
    }
}

void SecCertificatePathVCSetIsAnchored(
                                     SecCertificatePathVCRef certificatePath) {
    secdebug("trust", "%@ is anchored", certificatePath);
    certificatePath->isAnchored = true;

    /* Now check if that anchor (last cert) was actually self-signed.
     * In the non-anchor case, this is handled by SecCertificatePathVCSetSelfIssued.
     * Because anchored chains immediately go into the candidate bucket in the trust
     * server, we need to ensure that the self-signed/self-issued members are set
     * for the purposes of scoring. */
    if (!certificatePath->isSelfSigned && certificatePath->count > 0) {
        SecCertificateVCRef certVC = certificatePath->certificates[certificatePath->count - 1];
        Boolean isSelfSigned = false;
        OSStatus status = SecCertificateIsSelfSigned(certVC->certificate, &isSelfSigned);
        if ((status == errSecSuccess) && isSelfSigned) {
            certificatePath->isSelfSigned = true;
            if (certificatePath->selfIssued == -1) {
                certificatePath->selfIssued = certificatePath->count - 1;
            }
        }
    }
}

/* Return the index of the first non anchor certificate in the chain that is
 self signed counting from the leaf up.  Return -1 if there is none. */
CFIndex SecCertificatePathVCSelfSignedIndex(
                                          SecCertificatePathVCRef certificatePath) {
    if (certificatePath->isSelfSigned)
        return certificatePath->selfIssued;
    return -1;
}

Boolean SecCertificatePathVCIsAnchored(
                                     SecCertificatePathVCRef certificatePath) {
    return certificatePath->isAnchored;
}


void SecCertificatePathVCSetNextSourceIndex(
                                          SecCertificatePathVCRef certificatePath, CFIndex sourceIndex) {
    certificatePath->nextParentSource = sourceIndex;
}

CFIndex SecCertificatePathVCGetNextSourceIndex(
                                             SecCertificatePathVCRef certificatePath) {
    return certificatePath->nextParentSource;
}

CFIndex SecCertificatePathVCGetCount(
                                   SecCertificatePathVCRef certificatePath) {
    check(certificatePath);
    return certificatePath ? certificatePath->count : 0;
}

SecCertificateRef SecCertificatePathVCGetCertificateAtIndex(
                                                          SecCertificatePathVCRef certificatePath, CFIndex ix) {
    if (!certificatePath || ix < 0 || ix >= certificatePath->count) {
        return NULL;
    }
    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    return cvc ? cvc->certificate : NULL;
}

void SecCertificatePathVCForEachCertificate(SecCertificatePathVCRef path, void(^operation)(SecCertificateRef certificate, bool *stop)) {
    bool stop = false;
    CFIndex ix, count = path->count;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateVCRef cvc = path->certificates[ix];
        operation(cvc->certificate, &stop);
        if (stop) { break; }
    }
}

CFIndex SecCertificatePathVCGetIndexOfCertificate(SecCertificatePathVCRef path,
                                                SecCertificateRef certificate) {
    CFIndex ix, count = path->count;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateVCRef cvc = path->certificates[ix];
        if (CFEqual(cvc->certificate, certificate))
            return ix;
    }
    return kCFNotFound;
}

/* Return the root certificate for certificatePath.  Note that root is just
 the top of the path as far as it is constructed.  It may or may not be
 trusted or self signed.  */
SecCertificateRef SecCertificatePathVCGetRoot(
                                            SecCertificatePathVCRef certificatePath) {
    return SecCertificatePathVCGetCertificateAtIndex(certificatePath,
                                                   SecCertificatePathVCGetCount(certificatePath) - 1);
}

SecKeyRef SecCertificatePathVCCopyPublicKeyAtIndex(
                                                 SecCertificatePathVCRef certificatePath, CFIndex ix) {
    SecCertificateRef certificate =
    SecCertificatePathVCGetCertificateAtIndex(certificatePath, ix);
    return SecCertificateCopyKey(certificate);
}

CFArrayRef SecCertificatePathVCGetUsageConstraintsAtIndex(
                                                        SecCertificatePathVCRef certificatePath, CFIndex ix) {
    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    return cvc->usageConstraints;
}

void SecCertificatePathVCSetUsageConstraintsAtIndex(SecCertificatePathVCRef certificatePath,
                                                  CFArrayRef newConstraints, CFIndex ix) {
    CFArrayRef emptyArray = NULL;
    if (!newConstraints) {
        require_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit);
        newConstraints = emptyArray;
    }

    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    cvc->usageConstraints = CFRetainSafe(newConstraints);
exit:
    CFReleaseNull(emptyArray);
    return;
}

SecPathVerifyStatus SecCertificatePathVCVerify(SecCertificatePathVCRef certificatePath) {
    check(certificatePath);
    if (!certificatePath)
        return kSecPathVerifyFailed;
    for (;
         certificatePath->lastVerifiedSigner < certificatePath->count - 1;
         ++certificatePath->lastVerifiedSigner) {
        SecKeyRef issuerKey =
        SecCertificatePathVCCopyPublicKeyAtIndex(certificatePath,
                                               certificatePath->lastVerifiedSigner + 1);
        if (!issuerKey)
            return kSecPathVerifiesUnknown;
        SecCertificateVCRef cvc = certificatePath->certificates[certificatePath->lastVerifiedSigner];
        OSStatus status = SecCertificateIsSignedBy(cvc->certificate,
                                                   issuerKey);
        CFRelease(issuerKey);
        if (status) {
            return kSecPathVerifyFailed;
        }
    }

    return kSecPathVerifySuccess;
}

/* Is the the issuer of the last cert a subject of a previous cert in the chain.See <rdar://33136765>. */
bool SecCertificatePathVCIsCycleInGraph(SecCertificatePathVCRef path) {
    bool isCircle = false;
    CFDataRef issuer = SecCertificateGetNormalizedIssuerContent(SecCertificatePathVCGetRoot(path));
    if (!issuer) { return isCircle; }
    CFIndex ix = path->count - 2;
    for (; ix >= 0; ix--) {
        SecCertificateVCRef cvc = path->certificates[ix];
        CFDataRef subject = SecCertificateGetNormalizedSubjectContent(cvc->certificate);
        if (subject && CFEqual(issuer, subject)) {
            isCircle = true;
            break;
        }
    }
    return isCircle;
}

bool SecCertificatePathVCIsValid(SecCertificatePathVCRef certificatePath, CFAbsoluteTime verifyTime) {
    __block bool result = true;
    SecCertificatePathVCForEachCertificate(certificatePath, ^(SecCertificateRef certificate, bool *stop) {
        if (!SecCertificateIsValid(certificate, verifyTime)) {
            result = false;
        }
    });
    return result;
}

CFAbsoluteTime SecCertificatePathVCGetMaximumNotBefore(SecCertificatePathVCRef certificatePath) {
    __block CFAbsoluteTime notBefore = -DBL_MAX;
    SecCertificatePathVCForEachCertificate(certificatePath, ^(SecCertificateRef certificate, bool *stop) {
        CFAbsoluteTime certNotBefore = SecCertificateNotValidBefore(certificate);
        if (certNotBefore > notBefore) {
            notBefore = certNotBefore;
        }
    });
    return notBefore;
}

CFAbsoluteTime SecCertificatePathVCGetMinimumNotAfter(SecCertificatePathVCRef certificatePath) {
    __block CFAbsoluteTime notAfter = DBL_MAX;
    SecCertificatePathVCForEachCertificate(certificatePath, ^(SecCertificateRef certificate, bool *stop) {
        CFAbsoluteTime certNotAfter = SecCertificateNotValidAfter(certificate);
        if (certNotAfter < notAfter) {
            notAfter = certNotAfter;
        }
    });
    return notAfter;
}

bool SecCertificatePathVCHasWeakHash(SecCertificatePathVCRef certificatePath) {
    CFIndex ix, count = certificatePath->count;

    if (certificatePath->hasStrongHashes) {
        return false;
    }

    if (SecCertificatePathVCIsAnchored(certificatePath)) {
        /* For anchored paths, don't check the hash algorithm of the anchored cert,
         * since we already decided to trust it. */
        count--;
    }
    for (ix = 0; ix < count; ++ix) {
        if (certificatePath->certificates[ix]->isWeakHash) {
            return true;
        }
    }
    certificatePath->hasStrongHashes = true;
    return false;
}

bool SecCertificatePathVCHasWeakKeySize(SecCertificatePathVCRef certificatePath) {
    __block CFDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    __block bool result = false;

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-224 or larger. */
    require(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), errOut);
    require(ecSize = CFNumberCreateWithCFIndex(NULL, 224), errOut);
    const void *keys[] = { kSecAttrKeyTypeRSA, kSecAttrKeyTypeEC };
    const void *values[] = { rsaSize, ecSize };
    require(keySizes = CFDictionaryCreate(NULL, keys, values, 2,
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
    SecCertificatePathVCForEachCertificate(certificatePath, ^(SecCertificateRef certificate, bool *stop) {
        if (!SecCertificateIsAtLeastMinKeySize(certificate, keySizes)) {
            result = true;
            *stop = true;
        }
    });

errOut:
    CFReleaseSafe(keySizes);
    CFReleaseSafe(rsaSize);
    CFReleaseSafe(ecSize);
    return result;
}

/* Return a score for this certificate chain. */
CFIndex SecCertificatePathVCScore(SecCertificatePathVCRef certificatePath, CFAbsoluteTime verifyTime) {
    CFIndex score = 0;

    /* Paths that don't verify score terribly.c */
    if (certificatePath->lastVerifiedSigner != certificatePath->count - 1) {
        secdebug("trust", "lvs: %" PRIdCFIndex " count: %" PRIdCFIndex,
                 certificatePath->lastVerifiedSigner, certificatePath->count);
        score -= 100000;
    }

    if (certificatePath->isAnchored) {
        /* Anchored paths for the win! */
        score += 10000;
    }

    if (certificatePath->isSelfSigned && (certificatePath->selfIssued == certificatePath->count - 1)) {
        /* Chains that terminate in a self-signed certificate are preferred,
         even if they don't end in an anchor. */
        score += 1000;
        /* Shorter chains ending in a self-signed cert are preferred. */
        score -= 1 * certificatePath->count;
    } else {
        /* Longer chains are preferred when the chain doesn't end in a self-signed cert. */
        score += 1 * certificatePath->count;
    }

    if (SecCertificatePathVCIsValid(certificatePath, verifyTime)) {
        score += 100;
    }

    if (!SecCertificatePathVCHasWeakHash(certificatePath)) {
        score += 10;
    }

    if (!SecCertificatePathVCHasWeakKeySize(certificatePath)) {
        score += 10;
    }

    return score;
}

CFIndex SecCertificatePathVCGetScore(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return 0; }
    return certificatePath->score;
}

void SecCertificatePathVCSetScore(SecCertificatePathVCRef certificatePath, CFIndex score) {
    /* We may "score" the same path twice -- if we "accept" a path but then
     * decide to keep looking for a better one, we we process the same path
     * again in "reject" which creates a lower score. Don't replace a higher
     * score with a lower score. Use reset below to post-reject a path. */
    if (score > certificatePath->score) {
        certificatePath->score = score;
    }
}

void SecCertificatePathVCResetScore(SecCertificatePathVCRef certificatePath) {
    certificatePath->score = 0;
}

void *SecCertificatePathVCGetRVCAtIndex(SecCertificatePathVCRef certificatePath, CFIndex ix) {
    if (ix >= certificatePath->rvcCount) {
        return NULL;
    }
    return &((SecRVCRef)certificatePath->rvcs)[ix];
}

bool SecCertificatePathVCIsRevocationDone(SecCertificatePathVCRef certificatePath) {
    return (bool)certificatePath->rvcs;
}

void SecCertificatePathVCAllocateRVCs(SecCertificatePathVCRef certificatePath, CFIndex certCount) {
    certificatePath->rvcs = calloc(sizeof(struct OpaqueSecRVC), certCount);
    certificatePath->rvcCount = certCount;
}

/* Return 0 if any certs revocation checking failed, or the earliest date on
 which one of the used revocation validation tokens (ocsp response or
 crl) expires.  */
/* This function returns 0 to indicate revocation checking was not completed
 for this certificate chain, otherwise returns the date at which the first
 piece of revocation checking info we used expires.  */
CFAbsoluteTime SecCertificatePathVCGetEarliestNextUpdate(SecCertificatePathVCRef path) {
    CFIndex certIX, certCount = path->count;
    CFAbsoluteTime enu = NULL_TIME;
    if (certCount <= 1 || !path->rvcs) {
        return enu;
    }

    for (certIX = 0; certIX < path->rvcCount; ++certIX) {
        SecRVCRef rvc = &((SecRVCRef)path->rvcs)[certIX];
        CFAbsoluteTime thisCertNextUpdate = SecRVCGetEarliestNextUpdate(rvc);
        if (thisCertNextUpdate == 0) {
            if (certIX > 0) {
                /* We allow for CA certs to not be revocation checked if they
                 have no ocspResponders to check against, but the leaf
                 must be checked in order for us to claim we did revocation
                 checking. */
                SecCertificateRef cert = SecCertificatePathVCGetCertificateAtIndex(path, rvc->certIX);
                CFArrayRef ocspResponders = NULL;
                ocspResponders = SecCertificateGetOCSPResponders(cert);
                if (!ocspResponders || CFArrayGetCount(ocspResponders) == 0) {
                    /* We can't check this cert so we don't consider it a soft
                     failure that we didn't. */
                    continue;
                }
            }
            /* Make sure to always skip roots for whom we can't check revocation */
            if (certIX == certCount - 1) {
                continue;
            }
            secdebug("rvc", "revocation checking soft failure for cert: %ld",
                     certIX);
            enu = thisCertNextUpdate;
            break;
        }
        if (enu == 0 || thisCertNextUpdate < enu) {
            enu = thisCertNextUpdate;
        }
    }

    secdebug("rvc", "revocation valid until: %lg", enu);
    return enu;
}

CFAbsoluteTime SecCertificatePathVCGetLatestThisUpdate(SecCertificatePathVCRef path) {
    CFIndex certIX, certCount = path->count;
    CFAbsoluteTime ltu = -DBL_MAX;
    if (certCount <= 1 || !path->rvcs) {
        return ltu;
    }

    for (certIX = 0; certIX < path->rvcCount; ++certIX) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
        CFAbsoluteTime thisCertThisUpdate = SecRVCGetLatestThisUpdate(rvc);
        if (thisCertThisUpdate > ltu) {
            ltu = thisCertThisUpdate;
        }
    }

    secdebug("rvc", "revocation valid starting: %lg", ltu);
    return ltu;
}

bool SecCertificatePathVCRevocationCheckedAllCerts(SecCertificatePathVCRef path) {
    CFIndex certIX, certCount = path->count;
    if (certCount <= 1 || !path->rvcs) {
        /* If there is only one certificate, it's the root, so revocation checking is irrelevant. */
        return false;
    }

    for (certIX = 0; certIX < path->rvcCount - 1; ++certIX) {
        SecRVCRef rvc = &((SecRVCRef)path->rvcs)[certIX];
        if (!SecRVCRevocationChecked(rvc)) {
            secdebug("rvc", "revocation has not been checked for all certs (not checked for cert %ld)", certIX);
            return false;
        }
    }

    secdebug("rvc", "revocation has been checked for all certs");
    return true;
}

void SecCertificatePathVCSetRevocationReasonForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                  CFIndex ix, CFNumberRef revocationReason) {
    if (ix > certificatePath->count - 1) { return; }
    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    cvc->revocationReason = CFRetainSafe(revocationReason);
}

CFNumberRef SecCertificatePathVCGetRevocationReason(SecCertificatePathVCRef certificatePath) {
    for (CFIndex ix = 0; ix < certificatePath->count; ix++) {
        SecCertificateVCRef cvc = certificatePath->certificates[ix];
        if (cvc->revocationReason) {
            return cvc->revocationReason;
        }
    }
    return NULL;
}

bool SecCertificatePathVCIsRevocationRequiredForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                   CFIndex ix) {
    if (ix > certificatePath->count - 1) { return false; }
    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    return cvc->require_revocation_response;
}

void SecCertificatePathVCSetRevocationRequiredForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                    CFIndex ix) {
    if (ix > certificatePath->count - 1) { return; }
    SecCertificateVCRef cvc = certificatePath->certificates[ix];
    cvc->require_revocation_response = true;
}

bool SecCertificatePathVCCheckedIssuers(SecCertificatePathVCRef certificatePath) {
    return certificatePath->checkedIssuers;
}

void SecCertificatePathVCSetCheckedIssuers(SecCertificatePathVCRef certificatePath, bool checked) {
    certificatePath->checkedIssuers = checked;
}

CFIndex SecCertificatePathVCUnknownCAIndex(SecCertificatePathVCRef certificatePath) {
    return certificatePath->unknownCAIndex;
}

void SecCertificatePathVCSetUnknownCAIndex(SecCertificatePathVCRef certificatePath, CFIndex index) {
    certificatePath->unknownCAIndex = index;
}

bool SecCertificatePathVCIsPathValidated(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return false; }
    return certificatePath->pathValidated;
}

void SecCertificatePathVCSetPathValidated(SecCertificatePathVCRef certificatePath) {
    certificatePath->pathValidated = true;
}

bool SecCertificatePathVCIsEV(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return false; }
    return certificatePath->isEV;
}

void SecCertificatePathVCSetIsEV(SecCertificatePathVCRef certificatePath, bool isEV) {
    certificatePath->isEV = isEV;
}

bool SecCertificatePathVCIsOptionallyEV(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return false; }
    return certificatePath->certificates[0]->optionallyEV;
}

bool SecCertificatePathVCIsCT(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return false; }
    return certificatePath->isCT;
}

void SecCertificatePathVCSetIsCT(SecCertificatePathVCRef certificatePath, bool isCT) {
    certificatePath->isCT = isCT;
}

SecPathCTPolicy SecCertificatePathVCRequiresCT(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return kSecPathCTNotRequired; }
    return certificatePath->requiresCT;
}

void SecCertificatePathVCSetRequiresCT(SecCertificatePathVCRef certificatePath, SecPathCTPolicy requiresCT) {
    if (certificatePath->requiresCT > requiresCT) {
        return; /* once set, CT policy may be only be changed to a more strict value */
    }
    certificatePath->requiresCT = requiresCT;
}

static bool has_ca_additions_key(SecCertificatePathVCRef path, CFDictionaryRef ca_entry) {
    bool result = false;
    CFDataRef hash = CFDictionaryGetValue(ca_entry, kSecCARevocationSPKIHashKey);
    if (!hash) {
        return false;
    }
    /* only check issuing CAs and not the leaf */
    for (CFIndex certIX = 1; certIX < SecCertificatePathVCGetCount(path); certIX++) {
        SecCertificateRef ca = SecCertificatePathVCGetCertificateAtIndex(path, certIX);
        CFDataRef spkiHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(ca);
        bool matched = CFEqualSafe(hash, spkiHash);
        CFReleaseNull(spkiHash);
        if (!matched) {
            continue;
        }
        /* this SPKI is a match; remember highest index */
        if (certIX > path->revocationCAIndex) {
            path->revocationCAIndex = certIX;
        }
        result = true;
    }
    return result;
}

static void SecCertificatePathVCCheckCARevocationAdditions(SecCertificatePathVCRef path) {
    CFDictionaryRef additions = _SecTrustStoreCopyCARevocationAdditions(NULL, NULL);
    path->revocationCAIndex = kCFNotFound;
    if (!additions) {
        return;
    }

    __block bool result = false;
    CFArrayRef ca_list = CFDictionaryGetValue(additions, kSecCARevocationAdditionsKey);
    if (ca_list) {
        CFArrayForEach(ca_list, ^(const void *value) {
            result = result || has_ca_additions_key(path, value);
        });
    }

    if (result) {
        secinfo("ocsp", "key-based CA revocation applies at index %lld",
                (long long)path->revocationCAIndex);
    }

    CFReleaseNull(additions);
    return;
}

CFIndex SecCertificatePathVCIndexOfCAWithRevocationAdditions(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return kCFNotFound; }
    if (0 == certificatePath->revocationCAIndex) {
        /* we haven't checked this path yet, do it now */
        SecCertificatePathVCCheckCARevocationAdditions(certificatePath);
    }
    return certificatePath->revocationCAIndex;
}

CFAbsoluteTime SecCertificatePathVCIssuanceTime(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return 0; }
    return certificatePath->issuanceTime;
}

void SecCertificatePathVCSetIssuanceTime(SecCertificatePathVCRef certificatePath, CFAbsoluteTime issuanceTime) {
    certificatePath->issuanceTime = issuanceTime;
}

bool SecCertificatePathVCIsAllowlisted(SecCertificatePathVCRef certificatePath) {
    if (!certificatePath) { return false; }
    return certificatePath->is_allowlisted;
}

void SecCertificatePathVCSetIsAllowlisted(SecCertificatePathVCRef certificatePath, bool isAllowlisted) {
    certificatePath->is_allowlisted = isAllowlisted;
}

/* MARK: policy_tree path verification */
struct policy_tree_add_ctx {
    oid_t p_oid;
    policy_qualifier_t p_q;
};

/* For each node of depth i-1 in the valid_policy_tree where P-OID is in the expected_policy_set, create a child node as follows: set the valid_policy to P-OID, set the qualifier_set to P-Q, and set the expected_policy_set to {P-OID}. */
static bool policy_tree_add_if_match(policy_tree_t node, void *ctx) {
    struct policy_tree_add_ctx *info = (struct policy_tree_add_ctx *)ctx;
    policy_set_t policy_set;
    for (policy_set = node->expected_policy_set;
         policy_set;
         policy_set = policy_set->oid_next) {
        if (oid_equal(policy_set->oid, info->p_oid)) {
            policy_tree_add_child(node, &info->p_oid, info->p_q);
            return true;
        }
    }
    return false;
}

/* If the valid_policy_tree includes a node of depth i-1 with the valid_policy anyPolicy, generate a child node with the following values: set the valid_policy to P-OID, set the qualifier_set to P-Q, and set the expected_policy_set to {P-OID}. */
static bool policy_tree_add_if_any(policy_tree_t node, void *ctx) {
    struct policy_tree_add_ctx *info = (struct policy_tree_add_ctx *)ctx;
    if (oid_equal(node->valid_policy, oidAnyPolicy)) {
        policy_tree_add_child(node, &info->p_oid, info->p_q);
        return true;
    }
    return false;
}

/* Return true iff node has a child with a valid_policy equal to oid. */
static bool policy_tree_has_child_with_oid(policy_tree_t node,
                                           const oid_t *oid) {
    policy_tree_t child;
    for (child = node->children; child; child = child->siblings) {
        if (oid_equal(child->valid_policy, (*oid))) {
            return true;
        }
    }
    return false;
}

/* For each node in the valid_policy_tree of depth i-1, for each value in the expected_policy_set (including anyPolicy) that does not appear in a child node, create a child node with the following values: set the valid_policy to the value from the expected_policy_set in the parent node, set the qualifier_set to AP-Q, and set the expected_policy_set to the value in the valid_policy from this node. */
static bool policy_tree_add_expected(policy_tree_t node, void *ctx) {
    policy_qualifier_t p_q = (policy_qualifier_t)ctx;
    policy_set_t policy_set;
    bool added_node = false;
    for (policy_set = node->expected_policy_set;
         policy_set;
         policy_set = policy_set->oid_next) {
        if (!policy_tree_has_child_with_oid(node, &policy_set->oid)) {
            policy_tree_add_child(node, &policy_set->oid, p_q);
            added_node = true;
        }
    }
    return added_node;
}

/* For each node where ID-P is the valid_policy, set expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
static bool policy_tree_map_if_match(policy_tree_t node, void *ctx) {
    /* Can't map oidAnyPolicy. */
    if (oid_equal(node->valid_policy, oidAnyPolicy))
        return false;

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    policy_set_t policy_set = NULL;
    /* Generate the policy_set of sdps for matching idp */
    for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
        const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
        if (oid_equal(node->valid_policy, mapping->issuerDomainPolicy)) {
            policy_set_t p_node = (policy_set_t)malloc(sizeof(*policy_set));
            p_node->oid = mapping->subjectDomainPolicy;
            p_node->oid_next = policy_set ? policy_set : NULL;
            policy_set = p_node;
        }
    }
    if (policy_set) {
        policy_tree_set_expected_policy(node, policy_set);
        return true;
    }
    return false;
}

/* If no node of depth i in the valid_policy_tree has a valid_policy of ID-P but there is a node of depth i with a valid_policy of anyPolicy, then generate a child node of the node of depth i-1 that has a valid_policy of anyPolicy as follows:
 (i)   set the valid_policy to ID-P;
 (ii)  set the qualifier_set to the qualifier set of the policy anyPolicy in the certificate policies extension of certificate i; and
 (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
static bool policy_tree_map_if_any(policy_tree_t node, void *ctx) {
    if (!oid_equal(node->valid_policy, oidAnyPolicy)) {
        return false;
    }

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    CFMutableDictionaryRef mappings = NULL;
    CFDataRef idp = NULL;
    CFDataRef sdp = NULL;
    require_quiet(mappings = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks),
                  errOut);
    /* First we need to walk the mappings to generate the dictionary idp->sdps */
    for (mapping_ix = 0; mapping_ix < mapping_count; mapping_ix++) {
        oid_t issuerDomainPolicy = pm->mappings[mapping_ix].issuerDomainPolicy;
        oid_t subjectDomainPolicy = pm->mappings[mapping_ix].subjectDomainPolicy;
        idp = CFDataCreateWithBytesNoCopy(NULL, issuerDomainPolicy.data, issuerDomainPolicy.length, kCFAllocatorNull);
        sdp = CFDataCreateWithBytesNoCopy(NULL, subjectDomainPolicy.data, subjectDomainPolicy.length, kCFAllocatorNull);
        CFMutableArrayRef sdps = (CFMutableArrayRef)CFDictionaryGetValue(mappings, idp);
        if (sdps) {
            CFArrayAppendValue(sdps, sdp);
        } else {
            require_quiet(sdps = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                      &kCFTypeArrayCallBacks), errOut);
            CFArrayAppendValue(sdps, sdp);
            CFDictionarySetValue(mappings, idp, sdps);
            CFRelease(sdps);
        }
        CFReleaseNull(idp);
        CFReleaseNull(sdp);
    }

    /* Now we use the dictionary to generate the new nodes */
    CFDictionaryForEach(mappings, ^(const void *key, const void *value) {
        CFDataRef idp = key;
        CFArrayRef sdps = value;

        /* (i)   set the valid_policy to ID-P; */
        oid_t p_oid;
        p_oid.data = (uint8_t *)CFDataGetBytePtr(idp);
        p_oid.length = CFDataGetLength(idp);

        /* (ii)  set the qualifier_set to the qualifier set of the policy anyPolicy in the certificate policies extension of certificate i */
        policy_qualifier_t p_q = node->qualifier_set;

        /* (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension.  */
        __block policy_set_t p_expected = NULL;
        CFArrayForEach(sdps, ^(const void *value) {
            policy_set_t p_node = (policy_set_t)malloc(sizeof(*p_expected));
            p_node->oid.data = (void *)CFDataGetBytePtr(value);
            p_node->oid.length = CFDataGetLength(value);
            p_node->oid_next = p_expected ? p_expected : NULL;
            p_expected = p_node;
        });

        policy_tree_add_sibling(node, &p_oid, p_q, p_expected);
    });
    CFReleaseNull(mappings);
    return true;

errOut:
    CFReleaseNull(mappings);
    CFReleaseNull(idp);
    CFReleaseNull(sdp);
    return false;
}

static bool policy_tree_map_delete_if_match(policy_tree_t node, void *ctx) {
    /* Can't map oidAnyPolicy. */
    if (oid_equal(node->valid_policy, oidAnyPolicy))
        return false;

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    /* If this node matches any of the idps, delete it. */
    for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
        const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
        if (oid_equal(node->valid_policy, mapping->issuerDomainPolicy)) {
            policy_tree_remove_node(&node);
            break;
        }
    }
    return true;
}

bool SecCertificatePathVCIsCertificateAtIndexSelfIssued(SecCertificatePathVCRef path, CFIndex ix) {
    /* The SecCertificatePath only tells us the last self-issued cert.
     * The chain may have more than one self-issued cert, so we need to
     * do the comparison. */
    bool result = false;
    SecCertificateRef cert = SecCertificatePathVCGetCertificateAtIndex(path, ix);
    CFDataRef issuer = SecCertificateCopyNormalizedIssuerSequence(cert);
    CFDataRef subject = SecCertificateCopyNormalizedSubjectSequence(cert);
    if (issuer && subject && CFEqual(issuer, subject)) {
        result = true;
    }
    CFReleaseNull(issuer);
    CFReleaseNull(subject);
    return result;
}

enum {
    kSecPolicyTreeVerificationUnknown = 0,
    kSecPolicyTreeVerificationFalse,
    kSecPolicyTreeVerificationTrue,
};

/* RFC 5280 policy tree processing */
bool SecCertificatePathVCVerifyPolicyTree(SecCertificatePathVCRef path, bool anchor_trusted) {
    if (!path) { return false; }
    if (path->policy_tree_verification_result != kSecPolicyTreeVerificationUnknown) {
        return (path->policy_tree_verification_result == kSecPolicyTreeVerificationTrue);
    }

    /* Path Validation initialization */
    bool result = false;
    path->policy_tree_verification_result = kSecPolicyTreeVerificationFalse;
    bool initial_policy_mapping_inhibit = false;
    bool initial_explicit_policy = false;
    bool initial_any_policy_inhibit = false;

    SecCertificatePathVCPrunePolicyTree(path);
    path->policy_tree = policy_tree_create(&oidAnyPolicy, NULL);

    assert((unsigned long)path->count<=UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
    uint32_t n = (uint32_t)path->count;
    if (anchor_trusted) {
        n--;
    }

    uint32_t explicit_policy = initial_explicit_policy ? 0 : n + 1;
    uint32_t inhibit_any_policy = initial_any_policy_inhibit ? 0 : n + 1;
    uint32_t policy_mapping = initial_policy_mapping_inhibit ? 0 : n + 1;

    SecCertificateRef cert = NULL;
    uint32_t i;
    for (i = 1; i <= n; ++i) {
        /* Process Cert */
        cert = SecCertificatePathVCGetCertificateAtIndex(path, n - i);
        bool is_self_issued = SecCertificatePathVCIsCertificateAtIndexSelfIssued(path, n - i);

        /* (d) */
        if (path->policy_tree) {
            const SecCECertificatePolicies *cp =
            SecCertificateGetCertificatePolicies(cert);
            size_t policy_ix, policy_count = cp ? cp->numPolicies : 0;
            for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
                const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                oid_t p_oid = policy->policyIdentifier;
                policy_qualifier_t p_q = &policy->policyQualifiers;
                struct policy_tree_add_ctx ctx = { p_oid, p_q };
                if (!oid_equal(p_oid, oidAnyPolicy)) {
                    if (!policy_tree_walk_depth(path->policy_tree, i - 1,
                                                policy_tree_add_if_match, &ctx)) {
                        policy_tree_walk_depth(path->policy_tree, i - 1,
                                               policy_tree_add_if_any, &ctx);
                    }
                }
            }
            /* The certificate policies extension includes the policy
             anyPolicy with the qualifier set AP-Q and either
             (a) inhibit_anyPolicy is greater than 0 or
             (b) i < n and the certificate is self-issued. */
            if (inhibit_any_policy > 0 || (i < n && is_self_issued)) {
                for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
                    const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                    oid_t p_oid = policy->policyIdentifier;
                    policy_qualifier_t p_q = &policy->policyQualifiers;
                    if (oid_equal(p_oid, oidAnyPolicy)) {
                        policy_tree_walk_depth(path->policy_tree, i - 1,
                                               policy_tree_add_expected, (void *)p_q);
                    }
                }
            }

            policy_tree_prune_childless(&path->policy_tree, i - 1);
            /* (e) */
            if (!cp) {
                SecCertificatePathVCPrunePolicyTree(path);
            }
        }

        /* (f) Verify that either explicit_policy is greater than 0 or the
         valid_policy_tree is not equal to NULL. */
        if (!path->policy_tree && explicit_policy == 0) {
            /* valid_policy_tree is empty and explicit policy is 0, illegal. */
            secnotice("policy", "policy tree failure on cert %u", n - i);
            goto errOut;
        }
        /* If Last Cert in Path */
        if (i == n)
            break;

        /* Prepare for Next Cert */
        /* (a) verify that anyPolicy does not appear as an
         issuerDomainPolicy or a subjectDomainPolicy */
        const SecCEPolicyMappings *pm = SecCertificateGetPolicyMappings(cert);
        if (pm && pm->present) {
            size_t mapping_ix, mapping_count = pm->numMappings;
            for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
                const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
                if (oid_equal(mapping->issuerDomainPolicy, oidAnyPolicy)
                    || oid_equal(mapping->subjectDomainPolicy, oidAnyPolicy)) {
                    /* Policy mapping uses anyPolicy, illegal. */
                    secnotice("policy", "policy mapping anyPolicy failure %u", n - i);
                    goto errOut;
                }
            }

            /* (b) */
            /* (1) If the policy_mapping variable is greater than 0 */
            if (policy_mapping > 0 && path->policy_tree) {
                if (!policy_tree_walk_depth(path->policy_tree, i,
                                            policy_tree_map_if_match, (void *)pm)) {
                    /* If no node of depth i in the valid_policy_tree has a valid_policy of ID-P but there is a node of depth i with a valid_policy of anyPolicy, then generate a child node of the node of depth i-1. */
                    policy_tree_walk_depth(path->policy_tree, i, policy_tree_map_if_any, (void *)pm);
                }
            } else if (path->policy_tree) {
                /* (i)    delete each node of depth i in the valid_policy_tree
                 where ID-P is the valid_policy. */
                policy_tree_walk_depth(path->policy_tree, i,
                                       policy_tree_map_delete_if_match, (void *)pm);
                /* (ii)   If there is a node in the valid_policy_tree of depth
                 i-1 or less without any child nodes, delete that
                 node.  Repeat this step until there are no nodes of
                 depth i-1 or less without children. */
                policy_tree_prune_childless(&path->policy_tree, i - 1);
            }
        }

        /* (h) */
        if (!is_self_issued) {
            if (explicit_policy)
                explicit_policy--;
            if (policy_mapping)
                policy_mapping--;
            if (inhibit_any_policy)
                inhibit_any_policy--;
        }
        /* (i) */
        const SecCEPolicyConstraints *pc =
        SecCertificateGetPolicyConstraints(cert);
        if (pc) {
            if (pc->requireExplicitPolicyPresent
                && pc->requireExplicitPolicy < explicit_policy) {
                explicit_policy = pc->requireExplicitPolicy;
            }
            if (pc->inhibitPolicyMappingPresent
                && pc->inhibitPolicyMapping < policy_mapping) {
                policy_mapping = pc->inhibitPolicyMapping;
            }
        }
        /* (j) */
        const SecCEInhibitAnyPolicy *iap = SecCertificateGetInhibitAnyPolicySkipCerts(cert);
        if (iap && iap->skipCerts < inhibit_any_policy) {
            inhibit_any_policy = iap->skipCerts;
        }

    } /* end of path for loop */

    /* Wrap up */
    cert = SecCertificatePathVCGetCertificateAtIndex(path, 0);
    /* (a) */
    if (explicit_policy)
        explicit_policy--;
    /* (b) */
    const SecCEPolicyConstraints *pc = SecCertificateGetPolicyConstraints(cert);
    if (pc) {
        if (pc->requireExplicitPolicyPresent
            && pc->requireExplicitPolicy == 0) {
            explicit_policy = 0;
        }
    }

    /* (g) Calculate the intersection of the valid_policy_tree and the user-initial-policy-set, as follows */

    if (path->policy_tree) {
#if !defined(NDEBUG)
        policy_tree_dump(path->policy_tree);
#endif
        /* (g3c4) */
        //policy_tree_prune_childless(&pvc->valid_policy_tree, n - 1);
    }

    /* If either (1) the value of explicit_policy variable is greater than
     zero or (2) the valid_policy_tree is not NULL, then path processing
     has succeeded. */
    if (!path->policy_tree && explicit_policy == 0) {
        /* valid_policy_tree is empty and explicit policy is 0, illegal. */
        secnotice("policy", "policy tree failure on leaf");
        goto errOut;
    }

    path->policy_tree_verification_result = kSecPolicyTreeVerificationTrue;
    result = true;

errOut:
    return result;
}
