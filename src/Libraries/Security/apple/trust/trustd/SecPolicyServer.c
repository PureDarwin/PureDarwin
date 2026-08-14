/*
 * Copyright (c) 2008-2020 Apple Inc. All Rights Reserved.
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
 * SecPolicyServer.c - Engine for evaluating certificate paths against trust policies.
 */

#include <AssertMacros.h>
#include <wctype.h>
#include <asl.h>

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFTimeZone.h>
#include <CoreFoundation/CFNumber.h>
#include <sys/codesign.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>
#include <libDER/oids.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAppleAnchorPriv.h>
#include <utilities/debugging.h>
#include <utilities/SecInternalReleasePriv.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsalg.h>
#include <security_asn1/oidsocsp.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTask.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecKeyPriv.h>

#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/policytree.h"
#include "trust/trustd/nameconstraints.h"
#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/SecRevocationServer.h"
#include "trust/trustd/SecCertificateServer.h"
#include "trust/trustd/SecCertificateSource.h"
#include "trust/trustd/SecOCSPResponse.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "trust/trustd/OTATrustUtilities.h"
#include "trust/trustd/personalization.h"
#include "trust/trustd/CertificateTransparency.h"

#if !TARGET_OS_IPHONE
#include <Security/SecTaskPriv.h>
#endif

/* Set this to 1 to dump the ocsp responses received in DER form in /tmp. */
#ifndef DUMP_OCSPRESPONSES
#define DUMP_OCSPRESPONSES  0
#endif

#if DUMP_OCSPRESPONSES

#include <unistd.h>
#include <fcntl.h>

static void secdumpdata(CFDataRef data, const char *name) {
    int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    write(fd, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(fd);
}

#endif


/********************************************************
 ****************** SecPolicy object ********************
 ********************************************************/

static SecTrustSettingsResult SecPVCGetTrustSettingsResult(SecPVCRef pvc, SecCertificateRef certificate, CFArrayRef constraints);

static CFMutableDictionaryRef gSecPolicyLeafCallbacks = NULL;
static CFMutableDictionaryRef gSecPolicyPathCallbacks = NULL;

static CFArrayRef SecPolicyAnchorDigestsForEVPolicy(const DERItem *policyOID)
{
	CFArrayRef result = NULL;
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL == otapkiRef)
	{
		return result;
	}

	CFDictionaryRef evToPolicyAnchorDigest = SecOTAPKICopyEVPolicyToAnchorMapping(otapkiRef);
	CFRelease(otapkiRef);

    if (NULL == evToPolicyAnchorDigest)
    {
        return result;
    }

    CFArrayRef roots = NULL;
    CFStringRef oid = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, policyOID);
    if (oid && evToPolicyAnchorDigest)
	{
        result = (CFArrayRef)CFDictionaryGetValue(evToPolicyAnchorDigest, oid);
		if (roots && CFGetTypeID(result) != CFArrayGetTypeID())
		{
            secerror("EVRoot.plist has non array value");
            result = NULL;
        }
        CFRelease(oid);
    }
    CFReleaseSafe(evToPolicyAnchorDigest);
    return result;
}


bool SecPolicyIsEVPolicy(const DERItem *policyOID) {
    return SecPolicyAnchorDigestsForEVPolicy(policyOID);
}

static bool SecPolicyRootCACertificateIsEV(SecCertificateRef certificate,
    policy_set_t valid_policies) {
    CFDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;
    /* Ensure that this certificate is a valid anchor for one of the
       certificate policy oids specified in the leaf. */
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    policy_set_t ix;
    bool good_ev_anchor = false;
    for (ix = valid_policies; ix; ix = ix->oid_next) {
        CFArrayRef digests = SecPolicyAnchorDigestsForEVPolicy(&ix->oid);
        if (digests && CFArrayContainsValue(digests,
            CFRangeMake(0, CFArrayGetCount(digests)), digest)) {
            secdebug("ev", "found anchor for policy oid");
            good_ev_anchor = true;
            break;
        }
    }
    require_action_quiet(good_ev_anchor, notEV, secnotice("ev", "anchor not in plist"));

    CFAbsoluteTime october2006 = 178761600;
    if (SecCertificateNotValidBefore(certificate) >= october2006) {
        require_action_quiet(SecCertificateVersion(certificate) >= 3, notEV,
                             secnotice("ev", "Anchor issued after October 2006 and is not v3"));
    }
    if (SecCertificateVersion(certificate) >= 3
        && SecCertificateNotValidBefore(certificate) >= october2006) {
        const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
        require_action_quiet(bc && bc->isCA == true, notEV,
                             secnotice("ev", "Anchor has invalid basic constraints"));
        SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
        require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
            == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV,
                             secnotice("ev", "Anchor has invalid key usage %u", ku));
    }

    /* At least RSA 2048 or ECC NIST P-256. */
    require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    const void *keys[] = { kSecAttrKeyTypeRSA, kSecAttrKeyTypeEC };
    const void *values[] = { rsaSize, ecSize };
    require_quiet(keySizes = CFDictionaryCreate(NULL, keys, values, 2,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks), notEV);
    require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                         secnotice("ev", "Anchor's public key is too weak for EV"));

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}

static bool SecPolicySubordinateCACertificateCouldBeEV(SecCertificateRef certificate) {
    CFMutableDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;

    const SecCECertificatePolicies *cp;
    cp = SecCertificateGetCertificatePolicies(certificate);
    require_action_quiet(cp && cp->numPolicies > 0, notEV,
                         secnotice("ev", "SubCA missing certificate policies"));
    CFArrayRef cdp = SecCertificateGetCRLDistributionPoints(certificate);
    require_action_quiet(cdp && CFArrayGetCount(cdp) > 0, notEV,
                         secnotice("ev", "SubCA missing CRLDP"));
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    require_action_quiet(bc && bc->isCA == true, notEV,
                         secnotice("ev", "SubCA has invalid basic constraints"));
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
        == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV,
                         secnotice("ev", "SubCA has invalid key usage %u", ku));

    /* 6.1.5 Key Sizes */
    CFAbsoluteTime jan2011 = 315532800;
    CFAbsoluteTime jan2014 = 410227200;
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    require_quiet(keySizes = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks), notEV);
    CFDictionaryAddValue(keySizes, kSecAttrKeyTypeEC, ecSize);
    if (SecCertificateNotValidBefore(certificate) < jan2011 ||
        SecCertificateNotValidAfter(certificate) < jan2014) {
        /* At least RSA 1024 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 1024), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "SubCA's public key is too small for issuance before 2011 or expiration before 2014"));
    } else {
        /* At least RSA 2028 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "SubCA's public key is too small for issuance after 2010 or expiration after 2013"));
    }

    /* 7.1.3 Algorithm Object Identifiers */
    CFAbsoluteTime jan2016 = 473299200;
    if (SecCertificateNotValidBefore(certificate) > jan2016) {
        /* SHA-2 only */
        require_action_quiet(SecCertificateGetSignatureHashAlgorithm(certificate) > kSecSignatureHashAlgorithmSHA1,
                             notEV, secnotice("ev", "SubCA was issued with SHA-1 after 2015"));
    }

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}

/********************************************************
 **************** SecPolicy Callbacks *******************
 ********************************************************/
static void SecPolicyCheckCriticalExtensions(SecPVCRef pvc,
	CFStringRef key) {
}

static void SecPolicyCheckIdLinkage(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	CFDataRef parentSubjectKeyID = NULL;
	for (ix = count - 1; ix >= 0; --ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		/* If the previous certificate in the chain had a SubjectKeyID,
		   make sure it matches the current certificates AuthorityKeyID. */
		if (parentSubjectKeyID) {
			/* @@@ According to RFC 2459 neither AuthorityKeyID nor
			   SubjectKeyID can be critical.  Currenty we don't check
			   for this. */
			CFDataRef authorityKeyID = SecCertificateGetAuthorityKeyID(cert);
			if (authorityKeyID) {
				if (!CFEqual(parentSubjectKeyID, authorityKeyID)) {
					/* AuthorityKeyID doesn't match issuers SubjectKeyID. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				}
			}
		}

		parentSubjectKeyID = SecCertificateGetSubjectKeyID(cert);
	}
}

static void SecPolicyCheckKeyUsage(SecPVCRef pvc,
	CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xku = CFDictionaryGetValue(policy->_options, key);
    if (!SecPolicyCheckCertKeyUsage(leaf, xku)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckKeyUsageReportOnly(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xku = CFDictionaryGetValue(policy->_options, key);
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(pvc->builder);
    if (analytics && !SecPolicyCheckCertKeyUsage(leaf, xku)) {
        analytics->tls_invalid_ku = true;
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckExtendedKeyUsage(SecPVCRef pvc, CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xeku = CFDictionaryGetValue(policy->_options, key);
    /* leaf check enforced */
    if (!SecPolicyCheckCertExtendedKeyUsage(leaf, xeku)){
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }

    /* subCA check produces metrics */
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(pvc->builder);
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    if (count > 2 && analytics) {
        for (ix = 1; ix < count - 1 ; ++ix) {
            SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
            CFArrayRef ekus = SecCertificateCopyExtendedKeyUsage(cert);
            if (ekus && CFArrayGetCount(ekus) &&  // subCA has ekus
                !SecPolicyCheckCertExtendedKeyUsage(cert, CFSTR("2.5.29.37.0")) && // but not the anyEKU
                !SecPolicyCheckCertExtendedKeyUsage(cert, xeku)) { // and not the EKUs specified by the policy
                analytics->ca_fail_eku_check = true;
            }
            CFReleaseNull(ekus);
        }
    }
}

static void SecPolicyCheckBasicConstraints(SecPVCRef pvc,
	CFStringRef key) {
	//SecPolicyCheckBasicContraintsCommon(pvc, key, false);
}

static void SecPolicyCheckNonEmptySubject(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef pvcValue = CFDictionaryGetValue(policy->_options, key);
	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertNonEmptySubject(cert, pvcValue)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
	}
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSSLHostname(SecPVCRef pvc,
	CFStringRef key) {
	/* @@@ Consider what to do if the caller passes in no hostname.  Should
	   we then still fail if the leaf has no dnsNames or IPAddresses at all? */
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef hostName = (CFStringRef)
		CFDictionaryGetValue(policy->_options, key);
    if (!isString(hostName)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    bool dnsMatch = SecPolicyCheckCertSSLHostname(leaf, hostName);

	if (!dnsMatch) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}

}

/* AUDIT[securityd](done):
 policy->_options is a caller provided dictionary, only its cf type has
 been checked.
 */
static void SecPolicyCheckEmail(SecPVCRef pvc, CFStringRef key) {
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef email = (CFStringRef)CFDictionaryGetValue(policy->_options, key);
    if (!isString(email)) {
        /* We can't return an error here and making the evaluation fail
         won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);

	if (!SecPolicyCheckCertEmail(leaf, email)) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckTemporalValidity(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		if (!SecCertificateIsValid(cert, verifyTime)) {
			/* Intermediate certificate has expired. */
			if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
				return;
		}
	}
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckIssuerCommonName(SecPVCRef pvc,
	CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    if (count < 2) {
		/* Can't check intermediates common name if there is no intermediate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        return;
    }

	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 1);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFStringRef commonName =
        (CFStringRef)CFDictionaryGetValue(policy->_options, key);
    if (!isString(commonName)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonName(cert, commonName)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonName(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef common_name = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonName(cert, common_name)) {
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonNamePrefix(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFStringRef prefix = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(prefix)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonNamePrefix(cert, prefix)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonNameTEST(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef common_name = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonNameTEST(cert, common_name)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckNotValidBefore(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDateRef date = (CFDateRef)CFDictionaryGetValue(policy->_options, key);
    if (!isDate(date)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertNotValidBefore(cert, date)) {
		if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
			return;
	}
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckChainLength(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex count = SecPVCGetCertificateCount(pvc);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFNumberRef chainLength =
        (CFNumberRef)CFDictionaryGetValue(policy->_options, key);
    CFIndex value;
    if (!chainLength || CFGetTypeID(chainLength) != CFNumberGetTypeID() ||
        !CFNumberGetValue(chainLength, kCFNumberCFIndexType, &value)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (value != count) {
		/* Chain length doesn't match policy requirement. */
		if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
			return;
    }
}

static bool isDigestInPolicy(SecPVCRef pvc, CFStringRef key, CFDataRef digest) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    bool foundMatch = false;
    if (isData(value))
        foundMatch = CFEqual(digest, value);
    else if (isArray(value))
        foundMatch = CFArrayContainsValue((CFArrayRef) value, CFRangeMake(0, CFArrayGetCount((CFArrayRef) value)), digest);
    else {
        /* @@@ We only support Data and Array but we can't return an error here so.
         we let the evaluation fail (not much help) and assert in debug. */
        assert(false);
    }

    return foundMatch;
}

static void SecPolicyCheckAnchorSHA256(SecPVCRef pvc, CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    CFDataRef anchorSHA256 = NULL;
    anchorSHA256 = SecCertificateCopySHA256Digest(cert);

    if (!isDigestInPolicy(pvc, key, anchorSHA256)) {
        SecPVCSetResult(pvc, kSecPolicyCheckAnchorSHA256, count-1, kCFBooleanFalse);
    }

    CFReleaseNull(anchorSHA256);
    return;
}

/*
   Check the SHA256 of SPKI of the first intermediate CA certificate in the path.
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckIntermediateSPKISHA256(SecPVCRef pvc,
                                                 CFStringRef key) {
    SecCertificateRef cert = NULL;
    CFDataRef digest = NULL;

    if (SecPVCGetCertificateCount(pvc) < 2) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 0, kCFBooleanFalse);
        return;
    }

    cert = SecPVCGetCertificateAtIndex(pvc, 1);
    digest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert);

    if (!isDigestInPolicy(pvc, key, digest)) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 1, kCFBooleanFalse);
    }
    CFReleaseNull(digest);
}

/*
   Check the SPKI SHA256 of CA certificates in the path
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckCAspkiSHA256(SecPVCRef pvc,
                                       CFStringRef key) {
    SecCertificateRef cert = NULL;
    CFDataRef digest = NULL;

    if (SecPVCGetCertificateCount(pvc) < 2) {
        SecPVCSetResult(pvc, kSecPolicyCheckCAspkiSHA256, 0, kCFBooleanFalse);
        return;
    }

    bool spkiSHA256match = false;
    CFIndex count = SecPVCGetCertificateCount(pvc);
    for (CFIndex i = 1; i < count && spkiSHA256match == false; i++) {
        cert = SecPVCGetCertificateAtIndex(pvc, i);
        digest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert);

        if (isDigestInPolicy(pvc, key, digest)) {
            spkiSHA256match = true;
        }

        CFReleaseNull(digest);
    }

    if (spkiSHA256match == true) {
        return;
    }

    for (CFIndex i = 1; i < count; i++) {
        SecPVCSetResult(pvc, kSecPolicyCheckCAspkiSHA256, i, kCFBooleanFalse);
    }
}

/*
  Check the SPKI SHA256 of the leaf certificate.
  policy->_options is a caller provided dictionary, only its cf type has
  been checked.
*/
static void SecPolicyCheckLeafSPKISHA256(SecPVCRef pvc,
                                         CFStringRef key) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);

    CFArrayRef leafSPKISHA256 = CFDictionaryGetValue(policy->_options, key);
    if (isArray(leafSPKISHA256) == false) {
        /* @@@ We can't return an error here and making the evaluation fail
        won't help much either. */
        return;
    }

    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    CFDataRef digest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(leaf);
    if (!digest) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        return;
    }

    if (!CFArrayContainsValue(leafSPKISHA256, CFRangeMake(0, CFArrayGetCount(leafSPKISHA256)), digest)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }

    CFReleaseNull(digest);
}

/*
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckAnchorApple(SecPVCRef pvc,
                                      CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    SecAppleTrustAnchorFlags flags = 0;


    bool foundMatch = SecIsAppleTrustAnchor(cert, flags);

    if (!foundMatch)
        if (!SecPVCSetResult(pvc, kSecPolicyCheckAnchorApple, 0, kCFBooleanFalse))
            return;

    return;
}


/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectOrganization(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef org = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(org)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectOrganization(cert, org)) {
		/* Leaf Subject Organization mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
}

static void SecPolicyCheckSubjectOrganizationalUnit(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef orgUnit = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(orgUnit)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectOrganizationalUnit(cert, orgUnit)) {
        /* Leaf Subject Organization mismatch. */
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckEAPTrustedServerNames(SecPVCRef pvc,
	CFStringRef key) {
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFArrayRef trustedServerNames = (CFArrayRef)
		CFDictionaryGetValue(policy->_options, key);
    /* No names specified means we accept any name. */
    if (!trustedServerNames)
        return;
    if (!isArray(trustedServerNames)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    if (!SecPolicyCheckCertEAPTrustedServerNames(leaf, trustedServerNames)) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
}

static const unsigned char UTN_USERFirst_Hardware_Serial[][16] = {
{ 0xd8, 0xf3, 0x5f, 0x4e, 0xb7, 0x87, 0x2b, 0x2d, 0xab, 0x06, 0x92, 0xe3, 0x15, 0x38, 0x2f, 0xb0 },
{ 0x92, 0x39, 0xd5, 0x34, 0x8f, 0x40, 0xd1, 0x69, 0x5a, 0x74, 0x54, 0x70, 0xe1, 0xf2, 0x3f, 0x43 },
{ 0xb0, 0xb7, 0x13, 0x3e, 0xd0, 0x96, 0xf9, 0xb5, 0x6f, 0xae, 0x91, 0xc8, 0x74, 0xbd, 0x3a, 0xc0 },
{ 0xe9, 0x02, 0x8b, 0x95, 0x78, 0xe4, 0x15, 0xdc, 0x1a, 0x71, 0x0a, 0x2b, 0x88, 0x15, 0x44, 0x47 },
{ 0x39, 0x2a, 0x43, 0x4f, 0x0e, 0x07, 0xdf, 0x1f, 0x8a, 0xa3, 0x05, 0xde, 0x34, 0xe0, 0xc2, 0x29 },
{ 0x3e, 0x75, 0xce, 0xd4, 0x6b, 0x69, 0x30, 0x21, 0x21, 0x88, 0x30, 0xae, 0x86, 0xa8, 0x2a, 0x71 },
{ 0xd7, 0x55, 0x8f, 0xda, 0xf5, 0xf1, 0x10, 0x5b, 0xb2, 0x13, 0x28, 0x2b, 0x70, 0x77, 0x29, 0xa3 },
{ 0x04, 0x7e, 0xcb, 0xe9, 0xfc, 0xa5, 0x5f, 0x7b, 0xd0, 0x9e, 0xae, 0x36, 0xe1, 0x0c, 0xae, 0x1e },
{ 0xf5, 0xc8, 0x6a, 0xf3, 0x61, 0x62, 0xf1, 0x3a, 0x64, 0xf5, 0x4f, 0x6d, 0xc9, 0x58, 0x7c, 0x06 } };

static const unsigned char UTN_USERFirst_Hardware_Normalized_Issuer[] = {
  0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55,
  0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x02,
  0x55, 0x54, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13,
  0x0e, 0x53, 0x41, 0x4c, 0x54, 0x20, 0x4c, 0x41, 0x4b, 0x45, 0x20, 0x43,
  0x49, 0x54, 0x59, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55, 0x04, 0x0a,
  0x13, 0x15, 0x54, 0x48, 0x45, 0x20, 0x55, 0x53, 0x45, 0x52, 0x54, 0x52,
  0x55, 0x53, 0x54, 0x20, 0x4e, 0x45, 0x54, 0x57, 0x4f, 0x52, 0x4b, 0x31,
  0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x18, 0x48, 0x54,
  0x54, 0x50, 0x3a, 0x2f, 0x2f, 0x57, 0x57, 0x57, 0x2e, 0x55, 0x53, 0x45,
  0x52, 0x54, 0x52, 0x55, 0x53, 0x54, 0x2e, 0x43, 0x4f, 0x4d, 0x31, 0x1f,
  0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x16, 0x55, 0x54, 0x4e,
  0x2d, 0x55, 0x53, 0x45, 0x52, 0x46, 0x49, 0x52, 0x53, 0x54, 0x2d, 0x48,
  0x41, 0x52, 0x44, 0x57, 0x41, 0x52, 0x45
};
static const unsigned int UTN_USERFirst_Hardware_Normalized_Issuer_len = 151;


static void SecPolicyCheckBlackListedLeaf(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    CFDataRef issuer = cert ? SecCertificateGetNormalizedIssuerContent(cert) : NULL;

    if (issuer && (CFDataGetLength(issuer) == (CFIndex)UTN_USERFirst_Hardware_Normalized_Issuer_len) &&
        (0 == memcmp(UTN_USERFirst_Hardware_Normalized_Issuer, CFDataGetBytePtr(issuer),
            UTN_USERFirst_Hardware_Normalized_Issuer_len)))
    {
        CFDataRef serial = SecCertificateCopySerialNumberData(cert, NULL);
        if (serial) {
            CFIndex serial_length = CFDataGetLength(serial);
            const uint8_t *serial_ptr = CFDataGetBytePtr(serial);

            while ((serial_length > 0) && (*serial_ptr == 0)) {
                serial_ptr++;
                serial_length--;
            }

            if (serial_length == (CFIndex)sizeof(*UTN_USERFirst_Hardware_Serial)) {
                unsigned int i;
                for (i = 0; i < array_size(UTN_USERFirst_Hardware_Serial); i++)
                {
                    if (0 == memcmp(UTN_USERFirst_Hardware_Serial[i],
                        serial_ptr, sizeof(*UTN_USERFirst_Hardware_Serial)))
                    {
                        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
                        CFReleaseSafe(serial);
                        return;
                    }
                }
            }
            CFRelease(serial);
        }
    }

	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef blackListedKeys = SecOTAPKICopyBlackListSet(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != blackListedKeys)
		{
			/* Check for blacklisted intermediates keys. */
			CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
			if (dgst)
			{
				/* Check dgst against blacklist. */
				if (CFSetContainsValue(blackListedKeys, dgst))
				{
					SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
				}
				CFRelease(dgst);
			}
			CFRelease(blackListedKeys);
		}
	}
}

static void SecPolicyCheckGrayListedLeaf(SecPVCRef pvc, CFStringRef key)
{
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef grayListedKeys = SecOTAPKICopyGrayList(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != grayListedKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);

			CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
			if (dgst)
			{
				/* Check dgst against gray. */
				if (CFSetContainsValue(grayListedKeys, dgst))
				{
					SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
				}
				CFRelease(dgst);
			}
			CFRelease(grayListedKeys);
		}
	}
}

static void SecPolicyCheckLeafMarkerOid(SecPVCRef pvc, CFStringRef key)
{
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    if (!SecPolicyCheckCertLeafMarkerOid(cert, value)) {
        // Don't use 'key' because that may be the legacy key (see <rdar://34537018>)
        // Force because we may have gotten the legacy key instead of the new key in the policy
        SecPVCSetResultForced(pvc, kSecPolicyCheckLeafMarkerOid, 0, kCFBooleanFalse, true);
    }
}

static void SecPolicyCheckLeafMarkerOidWithoutValueCheck(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    if (!SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(cert, value)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}


/*
 * The value is a dictionary. The dictionary contains keys indicating
 * whether the value is for Prod or QA. The values are the same as
 * in the options dictionary for SecPolicyCheckLeafMarkerOid.
 */
static void SecPolicyCheckLeafMarkersProdAndQA(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDictionaryRef value = CFDictionaryGetValue(policy->_options, key);
    CFTypeRef prodValue = CFDictionaryGetValue(value, kSecPolicyLeafMarkerProd);

    if (!SecPolicyCheckCertLeafMarkerOid(cert, prodValue)) {
        bool result = false;
        if (!result) {
            // Don't use 'key' because that may be the legacy key (see <rdar://34537018>)
            // Force because we may have gotten the legacy key instead of the new key in the policy
            SecPVCSetResultForced(pvc, kSecPolicyCheckLeafMarkersProdAndQA, 0, kCFBooleanFalse, true);
        }
    }
}

static void SecPolicyCheckIntermediateMarkerOid(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (SecCertificateHasMarkerExtension(cert, value))
            return;
    }
    // Don't use 'key' because that may be the legacy key (see <rdar://34537018>)
    // Force because we may have gotten the legacy key instead of the new key in the policy
    SecPVCSetResultForced(pvc, kSecPolicyCheckIntermediateMarkerOid, 0, kCFBooleanFalse, true);
}

static void SecPolicyCheckIntermediateMarkerOidWithoutValueCheck(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(cert, value)) {
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
    }
}

static void SecPolicyCheckIntermediateEKU(SecPVCRef pvc, CFStringRef key)
{
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFTypeRef peku = CFDictionaryGetValue(policy->_options, key);

	for (ix = 1; ix < count - 1; ix++) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertExtendedKeyUsage(cert, peku)) {
			SecPVCSetResult(pvc, key, ix, kCFBooleanFalse);
		}
	}
}

static void SecPolicyCheckIntermediateOrganization(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef organization = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertSubjectOrganization(cert, organization)) {
            // Don't use 'key' because that may be the legacy key (see <rdar://34537018>)
            // Force because we may have gotten the legacy key instead of the new key in the policy
            SecPVCSetResultForced(pvc, kSecPolicyCheckIntermediateOrganization, ix, kCFBooleanFalse, true);
        }
    }
}

static void SecPolicyCheckIntermediateCountry(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef country = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertSubjectCountry(cert, country)) {
            // Don't use 'key' because that may be the legacy key (see <rdar://34537018>)
            // Force because we may have gotten the legacy key instead of the new key in the policy
            SecPVCSetResultForced(pvc, kSecPolicyCheckIntermediateCountry, ix, kCFBooleanFalse, true);
        }
    }
}

/****************************************************************************
 *********************** New rfc5280 Chain Validation ***********************
 ****************************************************************************/

#define POLICY_MAPPING 1
#define POLICY_SUBTREES 1

/* rfc5280 basic cert processing. */
static void SecPolicyCheckBasicCertificateProcessing(SecPVCRef pvc,
	CFStringRef key) {
    /* Inputs */
    //cert_path_t path;
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    /* 64 bits cast: worst case here is we truncate the number of cert, and the validation may fail */
    assert((unsigned long)count<=UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
    uint32_t n = (uint32_t)count;

    bool is_anchored = SecPathBuilderIsAnchored(pvc->builder);
    bool is_anchor_trusted = false;
    if (is_anchored) {
        CFArrayRef constraints = SecCertificatePathVCGetUsageConstraintsAtIndex(path, n - 1);
        if (CFArrayGetCount(constraints) == 0) {
            /* Given that the path builder has already indicated the last cert in this chain has
             * trust set on it, empty constraints means trusted. */
            is_anchor_trusted = true;
        } else {
            /* Determine whether constraints say to trust this cert for this PVC. */
            SecTrustSettingsResult tsResult = SecPVCGetTrustSettingsResult(pvc, SecCertificatePathVCGetCertificateAtIndex(path, n - 1),
                                                                           constraints);
            if (tsResult == kSecTrustSettingsResultTrustRoot || tsResult == kSecTrustSettingsResultTrustAsRoot) {
                is_anchor_trusted = true;
            }
        }
    }

    if (is_anchor_trusted) {
        /* If the anchor is trusted we don't process the last cert in the
           chain (root). */
        n--;
    } else {
        Boolean isSelfSigned = false;
        (void) SecCertificateIsSelfSigned(SecCertificatePathVCGetCertificateAtIndex(path, n - 1), &isSelfSigned);
        if (isSelfSigned) {
            /* Add a detail for the root not being trusted. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckAnchorTrusted,
                                       n - 1, kCFBooleanFalse, true)) {
                return;
            }
        } else {
            /* Add a detail for the missing intermediate. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckMissingIntermediate,
                                       n - 1, kCFBooleanFalse, true)) {
                return;
            }
        }
    }

    CFAbsoluteTime verify_time = SecPVCGetVerifyTime(pvc);
    //policy_set_t user_initial_policy_set = NULL;
    //trust_anchor_t anchor;

    /* Initialization */
#if POLICY_SUBTREES
    CFMutableArrayRef permitted_subtrees = NULL;
    CFMutableArrayRef excluded_subtrees = NULL;
    /* set the initial subtrees to the trusted anchor's subtrees, if it has them */
    SecCertificateRef anchor = SecCertificatePathVCGetRoot(path);
    CFArrayRef anchor_permitted_subtrees = SecCertificateGetPermittedSubtrees(anchor);
    if (is_anchor_trusted && anchor_permitted_subtrees) {
        permitted_subtrees = CFArrayCreateMutableCopy(NULL, 0, anchor_permitted_subtrees);
    } else {
        permitted_subtrees = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }

    CFArrayRef anchor_excluded_subtrees = SecCertificateGetExcludedSubtrees(anchor);
    if (is_anchor_trusted && anchor_excluded_subtrees) {
        excluded_subtrees = CFArrayCreateMutableCopy(NULL, 0, anchor_excluded_subtrees);
    } else {
        excluded_subtrees = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }

    require_action_quiet(permitted_subtrees != NULL, errOut,
                         SecPVCSetResultForced(pvc, kSecPolicyCheckNameConstraints, 0, kCFBooleanFalse, true));
    require_action_quiet(excluded_subtrees != NULL, errOut,
                         SecPVCSetResultForced(pvc, kSecPolicyCheckNameConstraints, 0, kCFBooleanFalse, true));
#endif

    if (!SecCertificatePathVCVerifyPolicyTree(path, is_anchor_trusted)) {
        if (!SecPVCSetResultForced(pvc, kSecPolicyCheckPolicyConstraints, 0, kCFBooleanFalse, true)) {
            goto errOut;
        }
    }

#if 0
    /* Path builder ensures we only get cert chains with proper issuer
       chaining with valid signatures along the way. */
    algorithm_id_t working_public_key_algorithm = anchor->public_key_algorithm;
    SecKeyRef working_public_key = anchor->public_key;
    x500_name_t working_issuer_name = anchor->issuer_name;
#endif
    uint32_t i, max_path_length = n;
    SecCertificateRef cert = NULL;
    for (i = 1; i <= n; ++i) {
        /* Process Cert */
        cert = SecPVCGetCertificateAtIndex(pvc, n - i);
        bool is_self_issued = SecCertificatePathVCIsCertificateAtIndexSelfIssued(SecPathBuilderGetPath(pvc->builder), n - i);

        /* (a) Verify the basic certificate information. */
        /* @@@ Ensure that cert was signed with working_public_key_algorithm
           using the working_public_key and the working_public_key_parameters. */

        /* Already done by chain builder. */
        if (!SecCertificateIsValid(cert, verify_time)) {
            if (!SecPVCSetResult(pvc, kSecPolicyCheckTemporalValidity, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }
        if (SecCertificateIsWeakKey(cert)) {
            if (!SecPVCSetResult(pvc, kSecPolicyCheckWeakKeySize, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }
        if (!SecPolicyCheckCertWeakSignature(cert, NULL)) {
            if (!SecPVCSetResult(pvc, kSecPolicyCheckWeakSignature, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }

        /* @@@ cert.issuer == working_issuer_name. */

#if POLICY_SUBTREES
        /* (b) (c) */
        if (!is_self_issued || i == n) {
            bool found = false;
            /* Verify certificate Subject Name and SubjectAltNames are not within any of the excluded_subtrees */
            if(excluded_subtrees && CFArrayGetCount(excluded_subtrees)) {
                if ((errSecSuccess != SecNameContraintsMatchSubtrees(cert, excluded_subtrees, &found, false)) || found) {
                    secnotice("policy", "name in excluded subtrees");
                    if(!SecPVCSetResultForced(pvc, kSecPolicyCheckNameConstraints, n - i, kCFBooleanFalse, true)) { goto errOut; }
                }
            }
            /* Verify certificate Subject Name and SubjectAltNames are within the permitted_subtrees */
            if(permitted_subtrees && CFArrayGetCount(permitted_subtrees)) {
               if ((errSecSuccess != SecNameContraintsMatchSubtrees(cert, permitted_subtrees, &found, true)) || !found) {
                   secnotice("policy", "name not in permitted subtrees");
                   if(!SecPVCSetResultForced(pvc, kSecPolicyCheckNameConstraints, n - i, kCFBooleanFalse, true)) { goto errOut; }
               }
            }
        }
#endif
        /* (d) (e) (f) handled by SecCertificatePathVCVerifyPolicyTree */

        /* If Last Cert in Path */
        if (i == n)
            break;

        /* Prepare for Next Cert */
        /* (a) (b) Done by SecCertificatePathVCVerifyPolicyTree */
        /* (c)(d)(e)(f)  Done by SecPathBuilderGetNext and SecCertificatePathVCVerify */
#if POLICY_SUBTREES
        /* (g) If a name constraints extension is included in the certificate, modify the permitted_subtrees and excluded_subtrees state variables.
         */
        CFArrayRef permitted_subtrees_in_cert = SecCertificateGetPermittedSubtrees(cert);
        if (permitted_subtrees_in_cert) {
            SecNameConstraintsIntersectSubtrees(permitted_subtrees, permitted_subtrees_in_cert);
        }

        // could do something smart here to avoid inserting the exact same constraint
        CFArrayRef excluded_subtrees_in_cert = SecCertificateGetExcludedSubtrees(cert);
        if (excluded_subtrees_in_cert) {
            CFIndex num_trees = CFArrayGetCount(excluded_subtrees_in_cert);
            CFRange range = { 0, num_trees };
            CFArrayAppendArray(excluded_subtrees, excluded_subtrees_in_cert, range);
        }
#endif
        /* (h), (i), (j) done by SecCertificatePathVCVerifyPolicyTree */

        /* (k) Checked in chain builder pre signature verify already. SecPVCParentCertificateChecks */

        /* (l) */
        if (!is_self_issued) {
            if (max_path_length > 0) {
                max_path_length--;
            } else {
                /* max_path_len exceeded, illegal. */
                if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicConstraintsPathLen,
                                           n - i, kCFBooleanFalse, true)) {
                    goto errOut;
                }
            }
        }
        /* (m) */
        const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(cert);
        if (bc && bc->pathLenConstraintPresent
            && bc->pathLenConstraint < max_path_length) {
            max_path_length = bc->pathLenConstraint;
        }

        /* (n) If a key usage extension is present, verify that the keyCertSign bit is set. */
        /* Checked in chain builder pre signature verify already. SecPVCParentCertificateChecks */

        /* (o) Recognize and process any other critical extension present in the certificate. Process any other recognized non-critical extension present in the certificate that is relevant to path processing. */
        if (SecCertificateHasUnknownCriticalExtension(cert)) {
			/* Certificate contains one or more unknown critical extensions. */
			if (!SecPVCSetResult(pvc, kSecPolicyCheckCriticalExtensions, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
		}

        if (SecCertificateGetUnparseableKnownExtension(cert) != kCFNotFound) {
            /* Certificate contains one or more known exensions where parsing failed. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckUnparseableExtension, n-i, kCFBooleanFalse, true)) {
                goto errOut;
            }
        }

    } /* end loop over certs in path */
    /* Wrap up */
    /* (a) (b) done by SecCertificatePathVCVerifyPolicyTree */
    /* (c) */
    /* (d) */
    /* If the subjectPublicKeyInfo field of the certificate contains an algorithm field with null parameters or parameters are omitted, compare the certificate subjectPublicKey algorithm to the working_public_key_algorithm. If the certificate subjectPublicKey algorithm and the
working_public_key_algorithm are different, set the working_public_key_parameters to null. */
    /* (e) */
    /* (f) Recognize and process any other critical extension present in the certificate n. Process any other recognized non-critical extension present in certificate n that is relevant to path processing. */
    if (SecCertificateHasUnknownCriticalExtension(cert)) {
        /* Certificate contains one or more unknown critical extensions. */
        if (!SecPVCSetResult(pvc, kSecPolicyCheckCriticalExtensions, 0, kCFBooleanFalse)) {
            goto errOut;
        }
    }

    if (SecCertificateGetUnparseableKnownExtension(cert) != kCFNotFound) {
        /* Certificate contains one or more known exensions where parsing failed. */
        if (!SecPVCSetResultForced(pvc, kSecPolicyCheckUnparseableExtension, 0, kCFBooleanFalse, true)) {
            goto errOut;
        }
    }
    /* (g) done by SecCertificatePathVCVerifyPolicyTree */

errOut:
    CFReleaseNull(permitted_subtrees);
    CFReleaseNull(excluded_subtrees);
}

static policy_set_t policies_for_cert(SecCertificateRef cert) {
    policy_set_t policies = NULL;
    const SecCECertificatePolicies *cp =
        SecCertificateGetCertificatePolicies(cert);
    size_t policy_ix, policy_count = cp ? cp->numPolicies : 0;
    for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
        policy_set_add(&policies, &cp->policies[policy_ix].policyIdentifier);
    }
    return policies;
}

static void SecPolicyCheckEV(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    policy_set_t valid_policies = NULL;

    /* 6.1.7. Key Usage Purposes */
    if (count) {
        CFAbsoluteTime jul2016 = 489024000;
        SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
        if (SecCertificateNotValidBefore(leaf) > jul2016 && count < 3) {
            /* Root CAs may not sign subscriber certificates after 30 June 2016. */
            if (SecPVCSetResultForced(pvc, key,
                    0, kCFBooleanFalse, true)) {
                return;
            }
        }
    }

	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        policy_set_t policies = policies_for_cert(cert);
        if (ix == 0) {
            /* Subscriber */
            /* anyPolicy in the leaf isn't allowed for EV, so only init
               valid_policies if we have real policies. */
            if (!policy_set_contains(policies, &oidAnyPolicy)) {
                valid_policies = policies;
                policies = NULL;
            }
        } else if (ix < count - 1) {
            /* Subordinate CA */
            if (!SecPolicySubordinateCACertificateCouldBeEV(cert)) {
                secnotice("ev", "subordinate certificate is not ev");
                if (SecPVCSetResultForced(pvc, key,
                    ix, kCFBooleanFalse, true)) {
                    policy_set_free(valid_policies);
                    policy_set_free(policies);
                    return;
                }
            }
            policy_set_intersect(&valid_policies, policies);
        } else {
            /* Root CA */
            if (!SecPolicyRootCACertificateIsEV(cert, valid_policies)) {
                secnotice("ev", "anchor certificate is not ev");
                if (SecPVCSetResultForced(pvc, key,
                    ix, kCFBooleanFalse, true)) {
                    policy_set_free(valid_policies);
                    policy_set_free(policies);
                    return;
                }
            }
        }
        policy_set_free(policies);
        if (!valid_policies) {
            secnotice("ev", "valid_policies set is empty: chain not ev");
            /* If we ever get into a state where no policies are valid anymore
               this can't be an ev chain. */
            if (SecPVCSetResultForced(pvc, key,
                ix, kCFBooleanFalse, true)) {
                return;
            }
        }
	}

    policy_set_free(valid_policies);

    /* (a) EV Subscriber Certificates   Each EV Certificate issued by the CA to a
Subscriber MUST contain an OID defined by the CA in the certificate’s
certificatePolicies extension that: (i) indicates which CA policy statement relates
to that certificate, (ii) asserts the CA’s adherence to and compliance with these
Guidelines, and (iii), by pre-agreement with the Application Software Vendor,
marks the certificate as being an EV Certificate.
(b) EV Subordinate CA Certificates
(1) Certificates issued to Subordinate CAs that are not controlled by the issuing
CA MUST contain one or more OIDs defined by the issuing CA that
explicitly identify the EV Policies that are implemented by the Subordinate
CA;
(2) Certificates issued to Subordinate CAs that are controlled by the Root CA
MAY contain the special anyPolicy OID (2.5.29.32.0).
(c) Root CA Certificates  Root CA Certificates SHOULD NOT contain the
certificatePolicies or extendedKeyUsage extensions.
*/
}

static bool checkPolicyOidData(SecPVCRef pvc, CFDataRef oid) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    DERItem	key_value;
    key_value.data = (DERByte *)CFDataGetBytePtr(oid);
    key_value.length = (DERSize)CFDataGetLength(oid);

    for (ix = 0; ix < count; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        policy_set_t policies = policies_for_cert(cert);

        if (policy_set_contains(policies, &key_value)) {
            policy_set_free(policies);
            return true;
        }
        policy_set_free(policies);
    }
    return false;
}

static void SecPolicyCheckCertificatePolicy(SecPVCRef pvc, CFStringRef key)
{
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    bool result = false;

	if (CFGetTypeID(value) == CFDataGetTypeID())
	{
        result = checkPolicyOidData(pvc, value);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, value);
        if (dataOid) {
            result = checkPolicyOidData(pvc, dataOid);
            CFRelease(dataOid);
        }
    }
    if(!result) {
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}


static void SecPolicyCheckRevocation(SecPVCRef pvc,
	CFStringRef key) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    if (isString(value)) {
        SecPathBuilderSetRevocationMethod(pvc->builder, value);
    }
}

static void SecPolicyCheckRevocationResponseRequired(SecPVCRef pvc,
	CFStringRef key) {
    pvc->require_revocation_response = true;
    secdebug("policy", "revocation response required");
}

static void SecPolicyCheckRevocationOnline(SecPVCRef pvc, CFStringRef key) {
    SecPathBuilderSetCheckRevocationOnline(pvc->builder);
}

static void SecPolicyCheckRevocationIfTrusted(SecPVCRef pvc, CFStringRef key) {
    SecPathBuilderSetCheckRevocationIfTrusted(pvc->builder);
}

static void SecPolicyCheckNoNetworkAccess(SecPVCRef pvc,
    CFStringRef key) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    if (value == kCFBooleanTrue) {
        SecPathBuilderSetCanAccessNetwork(pvc->builder, false);
    } else {
        SecPathBuilderSetCanAccessNetwork(pvc->builder, true);
    }
}

static void SecPolicyCheckWeakKeySize(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (cert && SecCertificateIsWeakKey(cert)) {
            /* Intermediate certificate has a weak key. */
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckKeySize(SecPVCRef pvc, CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDictionaryRef keySizes = CFDictionaryGetValue(policy->_options, key);
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);

    /* Don't check key size for user-anchored leafs */
    if (SecPVCIsAnchorPerConstraints(pvc, kSecUserAnchorSource, leaf)) {
        return;
    }
#if TARGET_OS_OSX
    if (SecPVCIsAnchorPerConstraints(pvc, kSecLegacyAnchorSource, leaf)) {
        return;
    }
#endif

    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecCertificateIsAtLeastMinKeySize(cert, keySizes)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckWeakSignature(SecPVCRef pvc, CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef pvcValue = CFDictionaryGetValue(policy->_options, key);
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertWeakSignature(cert, pvcValue)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckSignatureHashAlgorithms(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix = 0, count = SecPVCGetCertificateCount(pvc);

    /* Ignore (a non-self-signed) anchor if it's trusted by the user */
    bool userAnchored = SecPVCIsAnchorPerConstraints(pvc, kSecUserAnchorSource, SecPVCGetCertificateAtIndex(pvc, count - 1));
#if TARGET_OS_OSX
    userAnchored = userAnchored || SecPVCIsAnchorPerConstraints(pvc, kSecLegacyAnchorSource,
                                                                SecPVCGetCertificateAtIndex(pvc, count - 1));
#endif
    if (SecPathBuilderIsAnchored(pvc->builder) && userAnchored) {
        count--;
    }

    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFSetRef disallowedHashAlgorithms = CFDictionaryGetValue(policy->_options, key);
    while (ix < count) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        /* note that these checks skip self-signed certs */
        if (!SecPolicyCheckCertSignatureHashAlgorithms(cert, disallowedHashAlgorithms)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
        ix++;
    }
}

static bool leaf_is_on_weak_hash_whitelist(SecPVCRef pvc) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    require_quiet(leaf, out);

    /* And now a special snowflake from our tests */

    /* subject:/C=AU/ST=NSW/L=St Leonards/O=VODAFONE HUTCHISON AUSTRALIA PTY LIMITED/OU=Technology Shared Services/CN=mybill.vodafone.com.au */
    /* issuer :/C=UK/O=Vodafone Group/CN=Vodafone (Corporate Services 2009) */
    /* Not After : May 26 09:37:50 2017 GMT */
    static const uint8_t vodafone[] = {
        0xde, 0x77, 0x63, 0x97, 0x79, 0x47, 0xee, 0x6e, 0xc1, 0x3a,
        0x7b, 0x3b, 0xad, 0x43, 0x88, 0xa9, 0x66, 0x59, 0xa8, 0x18
    };

    CFDataRef leafFingerprint = SecCertificateGetSHA1Digest(leaf);
    require_quiet(leafFingerprint, out);
    const unsigned int len = 20;
    const uint8_t *dp = CFDataGetBytePtr(leafFingerprint);
    if (dp && (!memcmp(vodafone, dp, len))) {
        return true;
    }

out:
    return false;
}

static bool SecPVCKeyIsConstraintPolicyOption(SecPVCRef pvc, CFStringRef key);

static void SecPolicyCheckSystemTrustedWeakHash(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);

    Boolean keyInPolicy = false;
    CFArrayRef policies = pvc->policies;
    CFIndex policyIX, policyCount = CFArrayGetCount(policies);
    for (policyIX = 0; policyIX < policyCount; ++policyIX) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
        if (policy && CFDictionaryContainsKey(policy->_options, key)) {
            keyInPolicy = true;
        }
    }

    /* We only enforce this check when *both* of the following are true:
     *  1. One of the certs in the path has this usage constraint, and
     *  2. One of the policies in the PVC has this key
     * (As compared to normal policy options which require only one to be true..) */
    require_quiet(SecPVCKeyIsConstraintPolicyOption(pvc, key) &&
                  keyInPolicy, out);

    /* Ignore the anchor if it's trusted */
    if (SecPathBuilderIsAnchored(pvc->builder)) {
        count--;
    }
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (SecCertificateIsWeakHash(cert)) {
            if (!leaf_is_on_weak_hash_whitelist(pvc)) {
                if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse)) {
                    return;
                }
            }
        }
    }
out:
    return;
}

static void SecPolicyCheckSystemTrustedWeakKey(SecPVCRef pvc,
                                                CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);

    Boolean keyInPolicy = false;
    CFArrayRef policies = pvc->policies;
    CFIndex policyIX, policyCount = CFArrayGetCount(policies);
    for (policyIX = 0; policyIX < policyCount; ++policyIX) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
        if (policy && CFDictionaryContainsKey(policy->_options, key)) {
            keyInPolicy = true;
        }
    }

    /* We only enforce this check when *both* of the following are true:
     *  1. One of the certs in the path has this usage constraint, and
     *  2. One of the policies in the PVC has this key
     * (As compared to normal policy options which require only one to be true..) */
    require_quiet(SecPVCKeyIsConstraintPolicyOption(pvc, key) &&
                  keyInPolicy, out);

    /* Ignore the anchor if it's trusted */
    if (SecPathBuilderIsAnchored(pvc->builder)) {
        count--;
    }
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecCertificateIsStrongKey(cert)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse)) {
                return;
            }
        }

    } /* Cert loop */
out:
    return;
}

static void SecPolicyCheckPinningRequired(SecPVCRef pvc, CFStringRef key) {
    /* Pinning is disabled on the system, skip. */
    if (SecIsInternalRelease()) {
        if (CFPreferencesGetAppBooleanValue(CFSTR("AppleServerAuthenticationNoPinning"),
                                            CFSTR("com.apple.security"), NULL)) {
            return;
        }
    }

    CFArrayRef policies = pvc->policies;
    CFIndex policyIX, policyCount = CFArrayGetCount(policies);
    for (policyIX = 0; policyIX < policyCount; ++policyIX) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
        CFStringRef policyName = SecPolicyGetName(policy);
        if (CFEqualSafe(policyName, kSecPolicyNameSSLServer)) {
            /* policy required pinning, but we didn't use a pinning policy */
            if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse)) {
                return;
            }
        }
    }
}

static bool is_configured_test_system_root(SecCertificateRef root, CFStringRef preference) {
    if (!SecIsInternalRelease()) {
        return false;
    }
    bool result = false;
    CFDataRef rootHash = SecCertificateCopySHA256Digest(root);
    CFTypeRef value = CFPreferencesCopyAppValue(preference, CFSTR("com.apple.security"));
    require_quiet(isData(value), out);
    require_quiet(CFEqual(rootHash, value), out);
    result = true;

out:
    CFReleaseNull(value);
    CFReleaseNull(rootHash);
    return result;
}

static bool is_ct_excepted_domain(CFStringRef hostname, CFStringRef exception) {
    if (kCFCompareEqualTo == CFStringCompare(exception, hostname, kCFCompareCaseInsensitive)) {
        /* exact match */
        return true;
    } else if (CFStringHasPrefix(exception, CFSTR("."))) {
        /* subdomains */
        CFIndex elength = CFStringGetLength(exception);
        CFIndex hlength = CFStringGetLength(hostname);
        if (hlength > elength) {
            CFRange compareRange = { hlength - elength, elength };
            if (kCFCompareEqualTo == CFStringCompareWithOptions(hostname, exception, compareRange, kCFCompareCaseInsensitive)) {
                return true;
            }
        } else if (hlength + 1 == elength) {
            CFRange compareRange = { 1, hlength };
            if (kCFCompareEqualTo == CFStringCompareWithOptions(exception, hostname, compareRange, kCFCompareCaseInsensitive)) {
                return true;
            }
        }
    }
    return false;
}

static OSStatus is_subtree_dn_with_org(void *context, SecCEGeneralNameType gnType, const DERItem *generalName) {
    if (gnType != GNT_DirectoryName) {
        return errSecInternal;
    }

    DERDecodedInfo subtreeName_content;
    if (DR_Success != DERDecodeItem(generalName, &subtreeName_content) || subtreeName_content.tag != ASN1_CONSTR_SEQUENCE) {
        return errSecDecode;
    }

    CFArrayRef subtree_orgs = SecCertificateCopyOrganizationFromX501NameContent(&subtreeName_content.content);
    if (subtree_orgs) {
        CFReleaseNull(subtree_orgs);
        return errSecSuccess;
    }
    return errSecInternal;
}

static bool has_ct_excepted_key(SecCertificatePathVCRef path, CFDictionaryRef exception) {
    __block bool result = false;
    CFDataRef exceptionHash = CFDictionaryGetValue(exception, kSecCTExceptionsSPKIHashKey);

    /* exception for a leaf is always allowed */
    SecCertificateRef leaf = SecCertificatePathVCGetCertificateAtIndex(path, 0);
    CFDataRef spkiHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(leaf);
    if (CFEqualSafe(exceptionHash, spkiHash)) {
        result = true;
    }
    CFReleaseNull(spkiHash);

    if (result) { return result; }

    /* exceptions for CAs */
    for (CFIndex certIX = 1; certIX < SecCertificatePathVCGetCount(path); certIX++) {
        SecCertificateRef ca = SecCertificatePathVCGetCertificateAtIndex(path, certIX);

        spkiHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(ca);
        if (!CFEqualSafe(exceptionHash, spkiHash)) {
            CFReleaseNull(spkiHash);
            continue;
        }
        CFReleaseNull(spkiHash);

        /* this CA matches but exceptions for CAs have constraints */
        if (SecCertificateGetPermittedSubtrees(ca)) {
            /* Constrained CAs have to have a Distinguished Name permitted subtree with an Organization attribute */
            CFArrayForEach(SecCertificateGetPermittedSubtrees(ca), ^(const void *value) {
                CFDataRef subtree = (CFDataRef)value;
                const DERItem general_name = { (unsigned char *)CFDataGetBytePtr(subtree), CFDataGetLength(subtree) };
                DERDecodedInfo general_name_content;
                if (DR_Success == DERDecodeItem(&general_name, &general_name_content)) {
                    OSStatus status = SecCertificateParseGeneralNameContentProperty(general_name_content.tag,
                                                                                    &general_name_content.content,
                                                                                    NULL,
                                                                                    is_subtree_dn_with_org);
                    if (status == errSecSuccess) {
                        result = true;
                    }
                }
            });
        }

        if (!result) {
            /* The Organization attribute(s) in the CA subject have to exactly match the Organization attribute(s) in the leaf */
            CFArrayRef leafOrgs = SecCertificateCopyOrganization(leaf);
            CFArrayRef caOrgs = SecCertificateCopyOrganization(ca);
            if (caOrgs && leafOrgs && CFEqualSafe(leafOrgs, caOrgs)) {
                result = true;
            }
            CFReleaseNull(leafOrgs);
            CFReleaseNull(caOrgs);
        }

        if (result) {
            break;
        }
    }

    return result;
}

static bool is_ct_excepted(SecPVCRef pvc) {
    CFDictionaryRef ct_exceptions = _SecTrustStoreCopyCTExceptions(NULL, NULL);
    if (!ct_exceptions) {
        return false;
    }

    __block bool result = false;
    CFArrayRef domainExceptions = CFDictionaryGetValue(ct_exceptions, kSecCTExceptionsDomainsKey);
    if (domainExceptions) {
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        CFStringRef hostname = CFDictionaryGetValue(policy->_options, kSecPolicyCheckSSLHostname);
        if (hostname) {
            CFArrayForEach(domainExceptions, ^(const void *value) {
                result = result || is_ct_excepted_domain(hostname, value);
            });
        }
    }
    if (result) {
        secinfo("policy", "domain-based CT exception applied");
        CFReleaseNull(ct_exceptions);
        return result;
    }

    CFArrayRef keyExceptions = CFDictionaryGetValue(ct_exceptions, kSecCTExceptionsCAsKey);
    if (keyExceptions) {
        __block SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
        CFArrayForEach(keyExceptions, ^(const void *value) {
            result = result || has_ct_excepted_key(path, value);
        });
    }

    if (result) {
        secinfo("policy" , "key-based CT exceptions applied");
    }

    CFReleaseNull(ct_exceptions);
    return result;
}

/* <rdar://45466778> some Apple servers not getting certs with embedded SCTs */
static bool is_ct_allowlisted_cert(SecCertificateRef leaf) {
    if (CFPreferencesGetAppBooleanValue(CFSTR("DisableCTAllowlist"), CFSTR("com.apple.security"), NULL) ||
        CFPreferencesGetAppBooleanValue(CFSTR("DisableCTAllowlistApple"), CFSTR("com.apple.security"), NULL)) {
        return false;
    }

    /* subject:/CN=basejumper.apple.com/OU=management:idms.group.110621/O=Apple Inc./ST=California/C=US */
    /* issuer :/CN=Apple IST CA 2 - G1/OU=Certification Authority/O=Apple Inc./C=US */
    static const uint8_t basejumper_hash[] = {
        0x23, 0x32, 0x0b, 0x5a, 0x24, 0xd8, 0x4d, 0x27, 0x8c, 0x43, 0xc9, 0xed, 0x22, 0xed, 0x87, 0xb7,
        0xc5, 0x51, 0x43, 0x55, 0xa9, 0x84, 0x79, 0x5a, 0x77, 0xb9, 0xad, 0x0f, 0x88, 0x14, 0x61, 0xac,
    };

    bool result = false;
    CFDataRef leaf_fingerprint = SecCertificateCopySHA256Digest(leaf);
    const uint8_t *dp = CFDataGetBytePtr(leaf_fingerprint);
    if (dp && !memcmp(basejumper_hash, dp, CC_SHA256_DIGEST_LENGTH)) {
        result = true;
    }
    CFReleaseNull(leaf_fingerprint);
    return result;
}

static void SecPolicyCheckSystemTrustedCTRequired(SecPVCRef pvc) {
    SecCertificateSourceRef appleAnchorSource = NULL;
    SecOTAPKIRef otaref = SecOTAPKICopyCurrentOTAPKIRef();
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    CFDictionaryRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);

    /* Skip this check if we haven't done the CT checks yet */
    require_quiet(SecCertificatePathVCIsPathValidated(path), out);

    /* We only enforce this check when all of the following are true:
     * 0. Kill Switch not enabled */
    require_quiet(!SecOTAPKIKillSwitchEnabled(otaref, kOTAPKIKillSwitchCT), out);

    /*  1. Not a pinning policy */
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    require_quiet(CFEqualSafe(SecPolicyGetName(policy),kSecPolicyNameSSLServer), out);

    /*  2. Device has checked in to MobileAsset for a current log list within the last 60 days.
     *     Or the caller passed in the trusted log list. */
    require_quiet(SecOTAPKIAssetStalenessLessThanSeconds(otaref, kSecOTAPKIAssetStalenessDisable) || trustedLogs, out);

    /*  3. Leaf issuance date is on or after 16 Oct 2018 at 00:00:00 AM UTC and not expired. */
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    require_quiet(SecCertificateNotValidBefore(leaf) >= 561340800.0 &&
                  SecCertificateIsValid(leaf, SecPVCGetVerifyTime(pvc)), out);

    /*  4. Chain is anchored with root in the system anchor source but not the Apple anchor source
     *     with certain excepted CAs and configurable included CAs. */
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef root = SecPVCGetCertificateAtIndex(pvc, count - 1);
    appleAnchorSource = SecMemoryCertificateSourceCreate(SecGetAppleTrustAnchors(false));
    require_quiet(SecPathBuilderIsAnchored(pvc->builder), out);
    require_quiet((SecCertificateSourceContains(kSecSystemAnchorSource, root) &&
                   appleAnchorSource && !SecCertificateSourceContains(appleAnchorSource, root) &&
                   !is_ct_allowlisted_cert(leaf)) ||
                  is_configured_test_system_root(root, CFSTR("TestCTRequiredSystemRoot")), out);

    if (!SecCertificatePathVCIsCT(path) && !is_ct_excepted(pvc)) {
        /* Set failure. By not using the Forced variant, we implicitly check that this
         * policy had this options set. */
        SecPVCSetResult(pvc, kSecPolicyCheckSystemTrustedCTRequired, 0, kCFBooleanFalse);
    }

out:
    CFReleaseNull(trustedLogs);
    CFReleaseNull(otaref);
    if (appleAnchorSource) {
        SecMemoryCertificateSourceDestroy(appleAnchorSource);
    }
}

static bool SecPolicyCheckSystemTrustValidityPeriodMaximums(CFAbsoluteTime notBefore, CFAbsoluteTime notAfter) {
    CFAbsoluteTime jul2016 = 489024000.0; // 1 July 2016 00:00:00 UTC
    CFAbsoluteTime mar2018 = 541555200.0; // 1 March 2018 00:00:00 UTC
    CFAbsoluteTime sep2020 = 620611200.0; // 1 September 2020 00:00:00 UTC
    if (notBefore < jul2016) {
        /* Validity Period no greater than 60 months.
         60 months is no more than 5 years and 2 leap days (and 1 hour slip). */
        CFAbsoluteTime maxPeriod = 60*60*24*(365*5+2) + 3600;
        if (notAfter - notBefore > maxPeriod) {
            secnotice("policy", "System-trusted leaf validity period is more than 60 months");
            return false;
        }
    } else if (notBefore < mar2018) {
        /* Validity Period no greater than 39 months.
         39 months is no more than 3 years, 2 31-day months,
         1 30-day month, and 1 leap day (and 1 hour slip) */
        CFAbsoluteTime maxPeriod = 60*60*24*(365*3+2*31+30+1) + 3600;
        if (notAfter - notBefore > maxPeriod) {
            secnotice("policy", "System-trusted leaf validity period longer than 39 months and issued after 30 June 2016");
            return false;
        }
    } else if (notBefore < sep2020) {
        /* Validity Period no greater than 825 days (and 1 hour slip). */
        CFAbsoluteTime maxPeriod = 60*60*24*825 + 3600;
        if (notAfter - notBefore > maxPeriod) {
            secnotice("policy", "System-trusted leaf validity period longer than 825 days and issued on or after 1 March 2018");
            return false;
        }
    } else {
        /* Validity Period no greater than 398 days (and no slip). HT211025 */
        CFAbsoluteTime maxPeriod = 60*60*24*398;
        if (notAfter - notBefore > maxPeriod) {
            secnotice("policy", "System-trusted leaf validity period longer than 398 days and issued on or after 1 September 2020");
            return false;
        }
    }
    return true;
}

static bool SecPolicyCheckOtherTrustValidityPeriodMaximums(CFAbsoluteTime notBefore, CFAbsoluteTime notAfter) {
    /* Check whether we will enforce the validity period maximum for a non-system trusted leaf. */
    if (SecIsInternalRelease()) {
        if (CFPreferencesGetAppBooleanValue(CFSTR("IgnoreMaximumValidityPeriod"),
                                            CFSTR("com.apple.security"), NULL)) {
            return true;
        }
    }
    CFAbsoluteTime jul2019 = 583628400.0; // 1 July 2019 00:00:00 UTC
    if (notBefore > jul2019) {
        /* Validity Period no greater than 825 days (and 1 hour slip). */
        CFAbsoluteTime maxPeriod = 60*60*24*825 + 3600;
        if (notAfter - notBefore > maxPeriod) {
            secnotice("policy", "Non-system-trusted leaf validity period longer than 825 days and issued on or after 1 July 2019");
            return false;
        }
    }
    return true;
}

static void SecPolicyCheckValidityPeriodMaximums(SecPVCRef pvc, CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    CFAbsoluteTime notAfter = SecCertificateNotValidAfter(leaf);
    CFAbsoluteTime notBefore = SecCertificateNotValidBefore(leaf);

    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef root = SecPVCGetCertificateAtIndex(pvc, count - 1);
    if (SecCertificateSourceContains(kSecSystemAnchorSource, root) || is_configured_test_system_root(root, CFSTR("TestSystemRoot"))) {
        if (!SecPolicyCheckSystemTrustValidityPeriodMaximums(notBefore, notAfter)) {
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
        return;
    }

    /* Don't check validity periods against maximums for user-anchored leafs */
    if (SecPVCIsAnchorPerConstraints(pvc, kSecUserAnchorSource, leaf)) {
        return;
    }
#if TARGET_OS_OSX
    if (SecPVCIsAnchorPerConstraints(pvc, kSecLegacyAnchorSource, leaf)) {
        return;
    }
#endif

    /* all other trust */
    if (!SecPolicyCheckOtherTrustValidityPeriodMaximums(notBefore, notAfter)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckServerAuthEKU(SecPVCRef pvc, CFStringRef key) {
    /* The ExtendedKeyUsage check will verify a looser version of this check for all TLS server certs.
     * Here we want to be stricter (enforcing that there is an EKU extension and that it contains the
     * one true Server Auth EKU OID) for system and app anchors */
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef root = SecPVCGetCertificateAtIndex(pvc, count - 1);
    if (SecCertificateSourceContains(kSecSystemAnchorSource, root) || is_configured_test_system_root(root, CFSTR("TestSystemRoot"))) {
        /* all system-anchored chains must be compliant */
        if (!SecPolicyCheckCertExtendedKeyUsage(leaf, CFSTR("1.3.6.1.5.5.7.3.1"))) { // server auth EKU
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
        /* if any subCA cert has an EKU, it must have the server auth EKU */
        if (count > 2) { // chain has subCAs
            for (int ix = 1; ix < count - 1; ix++) { // iterate through subCAs
                SecCertificateRef subCA = SecPVCGetCertificateAtIndex(pvc, ix);
                CFArrayRef eku = NULL;
                if ((eku = SecCertificateCopyExtendedKeyUsage(subCA)) && CFArrayGetCount(eku)) { // subCA has EKU set
                    if (!SecPolicyCheckCertExtendedKeyUsage(subCA, CFSTR("1.3.6.1.5.5.7.3.1")) && // check server auth EKU
                        !SecPolicyCheckCertExtendedKeyUsage(subCA, CFSTR("2.5.29.37.0"))) { // check anyEKU
                        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
                    }
                }
                CFReleaseNull(eku);
            }
        }
        return;
    }

    /* skip user/admin-anchored chains */
    if (SecPVCIsAnchorPerConstraints(pvc, kSecUserAnchorSource, root)) {
        return;
    }
#if TARGET_OS_OSX
    if (SecPVCIsAnchorPerConstraints(pvc, kSecLegacyAnchorSource, root)) {
        return;
    }
#endif


    /* All other anchor types must be compliant if issued on or after 1 July 2019 */
    CFAbsoluteTime notBefore = SecCertificateNotValidBefore(leaf);
    CFAbsoluteTime jul2019 = 583628400.0; // 1 July 2019 00:00:00 UTC
    if (notBefore > jul2019) {
        if (!SecPolicyCheckCertExtendedKeyUsage(leaf, CFSTR("1.3.6.1.5.5.7.3.1"))) { // server auth EKU
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
    }
}

static void SecPolicyCheckCTRequired(SecPVCRef pvc, CFStringRef key) {
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    SecCertificatePathVCSetRequiresCT(path, kSecPathCTRequiredOverridable);
}

static void SecPolicyCheckNotCA(SecPVCRef pvc, CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    if (SecCertificateIsCA(leaf)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckNonTlsCTRequired(SecPVCRef pvc, CFStringRef key) {
    // Skip if kill switch enabled or log list not updated
    SecOTAPKIRef otaref = SecOTAPKICopyCurrentOTAPKIRef();
    CFDictionaryRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);
    if (!SecOTAPKIKillSwitchEnabled(otaref, kOTAPKIKillSwitchNonTLSCT) &&
        (SecOTAPKIAssetStalenessLessThanSeconds(otaref, kSecOTAPKIAssetStalenessDisable) || trustedLogs)) {
        // Check CT against the non-TLS log list
        if (!SecPolicyCheckNonTlsCT(pvc)) {
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
    }
    CFReleaseNull(otaref);
    CFReleaseNull(trustedLogs);
}

void SecPolicyServerInitialize(void) {
	gSecPolicyLeafCallbacks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, NULL);
	gSecPolicyPathCallbacks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, NULL);

#undef POLICYCHECKMACRO
#define __PC_ADD_CHECK_(NAME)
#define __PC_ADD_CHECK_L(NAME) CFDictionaryAddValue(gSecPolicyLeafCallbacks, kSecPolicyCheck##NAME, SecPolicyCheck##NAME);
#define __PC_ADD_CHECK_A(NAME) CFDictionaryAddValue(gSecPolicyPathCallbacks, kSecPolicyCheck##NAME, SecPolicyCheck##NAME);

#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
__PC_ADD_CHECK_##LEAFCHECK(NAME) \
__PC_ADD_CHECK_##PATHCHECK(NAME)
#include "OSX/sec/Security/SecPolicyChecks.list"

    /* Some of these don't follow the naming conventions but are in the Pinning DB.
     * <rdar://34537018> fix policy check constant values */
    CFDictionaryAddValue(gSecPolicyLeafCallbacks, CFSTR("CheckLeafMarkerOid"), SecPolicyCheckLeafMarkerOid);
    CFDictionaryAddValue(gSecPolicyLeafCallbacks, CFSTR("CheckLeafMarkersProdAndQA"), SecPolicyCheckLeafMarkersProdAndQA);
    CFDictionaryAddValue(gSecPolicyPathCallbacks, CFSTR("CheckIntermediateMarkerOid"), SecPolicyCheckIntermediateMarkerOid);
    CFDictionaryAddValue(gSecPolicyPathCallbacks, CFSTR("CheckIntermediateCountry"), SecPolicyCheckIntermediateCountry);
    CFDictionaryAddValue(gSecPolicyPathCallbacks, CFSTR("CheckIntermediateOrganization"), SecPolicyCheckIntermediateOrganization);
}

// MARK: -
// MARK: SecPVCRef
/********************************************************
 ****************** SecPVCRef Functions *****************
 ********************************************************/

void SecPVCInit(SecPVCRef pvc, SecPathBuilderRef builder, CFArrayRef policies) {
    secdebug("alloc", "pvc %p", pvc);
    // Weird logging policies crashes.
    //secdebug("policy", "%@", policies);

    // Zero the pvc struct so only non-zero fields need to be explicitly set
    memset(pvc, 0, sizeof(struct OpaqueSecPVC));
    pvc->builder = builder;
    pvc->policies = policies;
    if (policies)
        CFRetain(policies);
    pvc->result = kSecTrustResultUnspecified;

    CFMutableDictionaryRef certDetail = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                  &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
    pvc->leafDetails = CFArrayCreate(kCFAllocatorDefault, (const void **)&certDetail,
                                         1, &kCFTypeArrayCallBacks);
    CFRelease(certDetail);
}

void SecPVCDelete(SecPVCRef pvc) {
    secdebug("alloc", "delete pvc %p", pvc);
    CFReleaseNull(pvc->policies);
    CFReleaseNull(pvc->details);
    CFReleaseNull(pvc->leafDetails);
}

void SecPVCSetPath(SecPVCRef pvc, SecCertificatePathVCRef path) {
    secdebug("policy", "%@", path);
    pvc->policyIX = 0;
    pvc->result = kSecTrustResultUnspecified;
    CFReleaseNull(pvc->details);
}

void SecPVCComputeDetails(SecPVCRef pvc, SecCertificatePathVCRef path) {
    pvc->policyIX = 0;

    /* Since we don't run the LeafChecks again, we need to preserve the
     * result the leaf had. */
    CFIndex ix, pathLength = SecCertificatePathVCGetCount(path);
    CFMutableArrayRef details = CFArrayCreateMutableCopy(kCFAllocatorDefault,
                                                         pathLength, pvc->leafDetails);
    for (ix = 1; ix < pathLength; ++ix) {
        CFMutableDictionaryRef certDetail = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                      &kCFTypeDictionaryKeyCallBacks,
                                                                      &kCFTypeDictionaryValueCallBacks);
        CFArrayAppendValue(details, certDetail);
        CFRelease(certDetail);
    }
    CFRetainAssign(pvc->details, details);
    pvc->result = pvc->leafResult;
    CFReleaseSafe(details);
}

SecPolicyRef SecPVCGetPolicy(SecPVCRef pvc) {
	return (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, pvc->policyIX);
}

CFIndex SecPVCGetCertificateCount(SecPVCRef pvc) {
	return SecPathBuilderGetCertificateCount(pvc->builder);
}

SecCertificateRef SecPVCGetCertificateAtIndex(SecPVCRef pvc, CFIndex ix) {
	return SecPathBuilderGetCertificateAtIndex(pvc->builder, ix);
}

CFAbsoluteTime SecPVCGetVerifyTime(SecPVCRef pvc) {
    return SecPathBuilderGetVerifyTime(pvc->builder);
}

static bool SecPVCIsExceptedError(SecPVCRef pvc, CFIndex ix, CFStringRef key, CFTypeRef value) {
    CFArrayRef exceptions = SecPathBuilderGetExceptions(pvc->builder);
    if (!exceptions) { return false; }
    CFIndex exceptionsCount = CFArrayGetCount(exceptions);

    /* There are two types of exceptions:
     *  1. Those that are built from SecTrustCopyExceptions, which are particular to the
     *  certs in the chain -- as indicated by the SHA1 digest in the exception dictionary.
     *  2. On macOS, those built from SecTrustSetOptions, which are generic excepted errors.
     */
#if TARGET_OS_OSX
    CFDictionaryRef options = CFArrayGetValueAtIndex(exceptions, 0);
    if (!isDictionary(options)) {
        return false;
    }
    /* Type 2 */
    if (exceptionsCount == 1 && (ix > 0 || !CFDictionaryContainsKey(options, kSecCertificateDetailSHA1Digest))) {
        /* SHA1Digest not allowed */
        if (CFDictionaryContainsKey(options, kSecCertificateDetailSHA1Digest)) { return false; }
        /* Key excepted */
        if (CFDictionaryContainsKey(options, key)) {
            /* Special case -- AnchorTrusted only for self-signed certs */
            if (CFEqual(kSecPolicyCheckAnchorTrusted, key)) {
                Boolean isSelfSigned = false;
                SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(pvc->builder, ix);
                if (!cert || (errSecSuccess != SecCertificateIsSelfSigned(cert, &isSelfSigned)) || !isSelfSigned) {
                    return false;
                }
            }
            return true;
        } else if (CFEqual(key, kSecPolicyCheckTemporalValidity) && CFDictionaryContainsKey(options, kSecPolicyCheckValidRoot)) {
            /* Another special case - ValidRoot excepts Valid only for self-signed certs */
            Boolean isSelfSigned = false;
            SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(pvc->builder, ix);
            if (!cert || (errSecSuccess != SecCertificateIsSelfSigned(cert, &isSelfSigned)) || !isSelfSigned) {
                return false;
            }
            return true;
        }
    }
#endif

    /* Type 1 */
    if (ix >= exceptionsCount) { return false; }
    CFDictionaryRef exception = CFArrayGetValueAtIndex(exceptions, ix);
    if (!isDictionary(exception)) {
        return false;
    }

    /* Compare the cert hash */
    if (!CFDictionaryContainsKey(exception, kSecCertificateDetailSHA1Digest)) { return false; }
    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(pvc->builder, ix);
    if (!CFEqual(SecCertificateGetSHA1Digest(cert), CFDictionaryGetValue(exception, kSecCertificateDetailSHA1Digest))) {
        return false;
    }

    /* Key Excepted */
    CFTypeRef exceptionValue = CFDictionaryGetValue(exception, key);
    if (exceptionValue && CFEqual(value, exceptionValue)) {
        /* Only change result if PVC is already ok */
        if (SecPVCIsOkResult(pvc)) {
            // Chains that pass due to exceptions get Proceed result.
            pvc->result = kSecTrustResultProceed;
        }
        return true;
    }

    return false;
}

static int32_t detailKeyToCssmErr(CFStringRef key) {
    int32_t result = 0;

    if (CFEqual(key, kSecPolicyCheckSSLHostname)) {
        result = -2147408896; // CSSMERR_APPLETP_HOSTNAME_MISMATCH
    }
    else if (CFEqual(key, kSecPolicyCheckEmail)) {
        result = -2147408872; // CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND
    }
    else if (CFEqual(key, kSecPolicyCheckTemporalValidity)) {
        result = -2147409654; // CSSMERR_TP_CERT_EXPIRED
    }

    return result;
}

static bool SecPVCMeetsConstraint(SecPVCRef pvc, SecCertificateRef certificate, CFDictionaryRef constraint);

static bool SecPVCIsAllowedError(SecPVCRef pvc, CFIndex ix, CFStringRef key) {
    bool result = false;
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    CFArrayRef constraints = SecCertificatePathVCGetUsageConstraintsAtIndex(path, ix);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
    CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);

    for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
        CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
        CFNumberRef allowedErrorNumber = NULL;
        if (!isDictionary(constraint)) {
            continue;
        }
        allowedErrorNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsAllowedError);
        int32_t allowedErrorValue = 0;
        if (!isNumber(allowedErrorNumber) || !CFNumberGetValue(allowedErrorNumber, kCFNumberSInt32Type, &allowedErrorValue)) {
            continue;
        }

        if (SecPVCMeetsConstraint(pvc, cert, constraint)) {
            if (allowedErrorValue == detailKeyToCssmErr(key)) {
                result = true;
                break;
            }
        }
    }
    return result;
}

static bool SecPVCKeyIsConstraintPolicyOption(SecPVCRef pvc, CFStringRef key) {
    CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    for (certIX = 0; certIX < certCount; certIX++) {
        SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
        CFArrayRef constraints = SecCertificatePathVCGetUsageConstraintsAtIndex(path, certIX);
        CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);
        for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
            CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
            if (!isDictionary(constraint)) {
                continue;
            }

            CFDictionaryRef policyOptions = NULL;
            policyOptions = (CFDictionaryRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyOptions);
            if (policyOptions && isDictionary(policyOptions) &&
                CFDictionaryContainsKey(policyOptions, key)) {
                return true;
            }
        }
    }
    return false;
}

static SecTrustResultType trust_result_for_key(CFStringRef key) {
    SecTrustResultType result = kSecTrustResultRecoverableTrustFailure;
#undef POLICYCHECKMACRO
#define __PC_TYPE_MEMBER_ false
#define __PC_TYPE_MEMBER_R false
#define __PC_TYPE_MEMBER_F true
#define __PC_TYPE_MEMBER_D true

#define __TRUSTRESULT_  kSecTrustResultRecoverableTrustFailure
#define __TRUSTRESULT_F kSecTrustResultFatalTrustFailure
#define __TRUSTRESULT_D kSecTrustResultDeny
#define __TRUSTRESULT_R kSecTrustResultRecoverableTrustFailure

#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
if (__PC_TYPE_MEMBER_##TRUSTRESULT && CFEqual(key,CFSTR(#NAME))) { \
    result = __TRUSTRESULT_##TRUSTRESULT; \
}
#include "OSX/sec/Security/SecPolicyChecks.list"
    return result;
}


/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
bool SecPVCSetResultForcedWithTrustResult(SecPVCRef pvc, CFStringRef key, CFIndex ix, CFTypeRef result, bool force,
                                          SecTrustResultType overrideDefaultTR) {

    /* If this is not something the current policy cares about ignore
       this error and return true so our caller continues evaluation. */
    if (!force) {
        /* Either the policy or the usage constraints have to have this key */
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        if (!(SecPVCKeyIsConstraintPolicyOption(pvc, key) ||
            (policy && CFDictionaryContainsKey(policy->_options, key)))) {
            return true;
        }
    }

    /* Get the default trust result for this key and override it if the caller needs to
     * set a different trust result than the default. */
    SecTrustResultType trustResult = trust_result_for_key(key);
    if (overrideDefaultTR != kSecTrustResultInvalid) {
        trustResult = overrideDefaultTR;
    }

    /* only recoverable errors can be allowed/excepted */
    if (trustResult == kSecTrustResultRecoverableTrustFailure) {
        /* Check to see if the SecTrustSettings for the certificate in question
         tell us to ignore this error. */
        if (SecPVCIsAllowedError(pvc, ix, key)) {
            secinfo("policy", "cert[%d]: skipped allowed error %@", (int) ix, key);
            return true;
        }

        /* Check to see if exceptions tells us to ignore this error. */
        if (SecPVCIsExceptedError(pvc, ix, key, result)) {
            secinfo("policy", "cert[%d]: skipped exception error %@", (int) ix, key);
            return true;
        }
    }

    secnotice("policy", "cert[%d]: %@ =(%s)[%s]> %@", (int) ix, key,
              (pvc->callbacks == gSecPolicyLeafCallbacks ? "leaf"
               : (pvc->callbacks == gSecPolicyPathCallbacks ? "path"
                  : "custom")),
              (force ? "force" : ""), result);

	/* Avoid resetting deny or fatal to recoverable */
    if (SecPVCIsOkResult(pvc) || trustResult == kSecTrustResultFatalTrustFailure) {
        pvc->result = trustResult;
    } else if (trustResult == kSecTrustResultDeny &&
               pvc->result == kSecTrustResultRecoverableTrustFailure) {
        pvc->result = trustResult;
    }

	if (!pvc->details)
		return false;

	CFMutableDictionaryRef detail =
		(CFMutableDictionaryRef)CFArrayGetValueAtIndex(pvc->details, ix);
	if (!detail) {
		secerror("SecPVCSetResultForced: failed to get detail at index %ld (array length %ld)",
				ix, CFArrayGetCount(pvc->details));
		return false;
	}

	/* Perhaps detail should have an array of results per key?  As it stands
       in the case of multiple policy failures the last failure stands.  */
	CFDictionarySetValue(detail, key, result);

	return true;
}

bool SecPVCSetResultForced(SecPVCRef pvc, CFStringRef key, CFIndex ix, CFTypeRef result, bool force) {
    return SecPVCSetResultForcedWithTrustResult(pvc, key, ix, result, force, kSecTrustResultInvalid);
}

bool SecPVCSetResult(SecPVCRef pvc,
	CFStringRef key, CFIndex ix, CFTypeRef result) {
    return SecPVCSetResultForced(pvc, key, ix, result, false);
}

/* AUDIT[securityd](done):
   key(ok) is a caller provided.
   value(ok, unused) is a caller provided.
 */
static void SecPVCValidateKey(const void *key, const void *value,
	void *context) {
	SecPVCRef pvc = (SecPVCRef)context;

	/* If our caller doesn't want full details and we failed earlier there is
	   no point in doing additional checks. */
	if (!SecPVCIsOkResult(pvc) && !pvc->details)
		return;

	SecPolicyCheckFunction fcn = (SecPolicyCheckFunction)
		CFDictionaryGetValue(pvc->callbacks, key);

	if (!fcn) {
        /* "Optional" policy checks. This may be a new key from the
         * pinning DB which is not implemented in this OS version. Log a
         * warning, and on debug builds fail evaluation, to encourage us
         * to ensure that checks are synchronized across the same build. */
        if (pvc->callbacks == gSecPolicyLeafCallbacks) {
            if (!CFDictionaryContainsKey(gSecPolicyPathCallbacks, key)) {
                secwarning("policy: unknown policy key %@, skipping", key);
#if DEBUG
                pvc->result = kSecTrustResultOtherError;
#endif
            }
        } else if (pvc->callbacks == gSecPolicyPathCallbacks) {
            if (!CFDictionaryContainsKey(gSecPolicyLeafCallbacks, key)) {
                secwarning("policy: unknown policy key %@, skipping", key);
#if DEBUG
                pvc->result = kSecTrustResultOtherError;
#endif
            }
        } else {
            /* Non standard validation phase, nothing is optional. */
            pvc->result = kSecTrustResultOtherError;
        }
		return;
	}

	fcn(pvc, (CFStringRef)key);
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
SecTrustResultType SecPVCLeafChecks(SecPVCRef pvc) {
    /* We need to compute details for the leaf. */
    CFRetainAssign(pvc->details, pvc->leafDetails);

    CFArrayRef policies = pvc->policies;
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, ix);
        pvc->policyIX = ix;
        /* Validate all keys for all policies. */
        pvc->callbacks = gSecPolicyLeafCallbacks;
        CFDictionaryApplyFunction(policy->_options, SecPVCValidateKey, pvc);
	}

    pvc->leafResult = pvc->result;
    CFRetainAssign(pvc->leafDetails, pvc->details);

    return pvc->result;
}

bool SecPVCIsOkResult(SecPVCRef pvc) {
    if (pvc->result == kSecTrustResultRecoverableTrustFailure ||
        pvc->result == kSecTrustResultDeny ||
        pvc->result == kSecTrustResultFatalTrustFailure ||
        pvc->result == kSecTrustResultOtherError) {
        return false;
    }
    return true;
}

bool SecPVCParentCertificateChecks(SecPVCRef pvc, CFIndex ix) {
    /* Check stuff common to intermediate and anchors. */
    CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
    CFIndex anchor_ix = SecPVCGetCertificateCount(pvc) - 1;

    if (!SecCertificateIsValid(cert, verifyTime)) {
        /* Certificate has expired. */
        if (!SecPVCSetResult(pvc, kSecPolicyCheckTemporalValidity, ix, kCFBooleanFalse)) {
            goto errOut;
        }
    }

    if (SecCertificateIsWeakKey(cert)) {
        /* Certificate uses weak key. */
        if (!SecPVCSetResult(pvc, kSecPolicyCheckWeakKeySize, ix, kCFBooleanFalse)) {
            goto errOut;
        }
    }

    if (!SecPolicyCheckCertWeakSignature(cert, NULL)) {
        /* Certificate uses weak hash. */
        if (!SecPVCSetResult(pvc, kSecPolicyCheckWeakSignature, ix, kCFBooleanFalse)) {
            goto errOut;
        }
    }

    /* (k) Basic constraints only relevant for v3 and later. */
    if (SecCertificateVersion(cert) >= 3) {
        const SecCEBasicConstraints *bc =
        SecCertificateGetBasicConstraints(cert);
        if (!bc) {
            /* Basic constraints not present, illegal. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicConstraints,
                                       ix, kCFBooleanFalse, true)) {
                goto errOut;
            }
        } else if (!bc->isCA) {
            /* Basic constraints not marked as isCA, illegal. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicConstraintsCA,
                                       ix, kCFBooleanFalse, true)) {
                goto errOut;
            }
        }
    }
    /* For a v1 or v2 certificate in an intermediate slot (not a leaf and
     not an anchor), we additionally require that the certificate chain
     does not end in a v3 or later anchor. [rdar://32204517] */
    else if (ix > 0 && ix < anchor_ix) {
        SecCertificateRef anchor = SecPVCGetCertificateAtIndex(pvc, anchor_ix);
        if (SecCertificateVersion(anchor) >= 3) {
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicConstraints,
                                       ix, kCFBooleanFalse, true)) {
                goto errOut;
            }
        }
    }
    /* (l) max_path_length is checked elsewhere. */

    /* (n) If a key usage extension is present, verify that the keyCertSign bit is set. */
    SecKeyUsage keyUsage = SecCertificateGetKeyUsage(cert);
    if (keyUsage && !(keyUsage & kSecKeyUsageKeyCertSign)) {
        if (!SecPVCSetResultForced(pvc, kSecPolicyCheckKeyUsage,
                                   ix, kCFBooleanFalse, true)) {
            goto errOut;
        }
    }

errOut:
    return SecPVCIsOkResult(pvc);
}

static bool SecPVCBlackListedKeyChecks(SecPVCRef pvc, CFIndex ix) {
    /* Check stuff common to intermediate and anchors. */

	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef blackListedKeys = SecOTAPKICopyBlackListSet(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != blackListedKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
			CFIndex count = SecPVCGetCertificateCount(pvc);
			bool is_last = (ix == count - 1);
			bool is_anchor = (is_last && SecPathBuilderIsAnchored(pvc->builder));
			if (!is_anchor) {
				/* Check for blacklisted intermediate issuer keys. */
				CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
				if (dgst) {
					/* Check dgst against blacklist. */
					if (CFSetContainsValue(blackListedKeys, dgst)) {
						/* Check allow list for this blacklisted issuer key,
						   which is the authority key of the issued cert at ix-1.
						*/
						SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
						bool allowed = path && SecCertificatePathVCIsAllowlisted(path);
						if (!allowed) {
							SecPVCSetResultForced(pvc, kSecPolicyCheckBlackListedKey,
							                      ix, kCFBooleanFalse, true);
						}
					}
					CFRelease(dgst);
				}
			}
			CFRelease(blackListedKeys);
			return SecPVCIsOkResult(pvc);
		}
	}
	// Assume OK
	return true;
}

static bool SecPVCGrayListedKeyChecks(SecPVCRef pvc, CFIndex ix)
{
	/* Check stuff common to intermediate and anchors. */
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef grayListKeys = SecOTAPKICopyGrayList(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != grayListKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
			CFIndex count = SecPVCGetCertificateCount(pvc);
			bool is_last = (ix == count - 1);
			bool is_anchor = (is_last && SecPathBuilderIsAnchored(pvc->builder));
			if (!is_anchor) {
				/* Check for gray listed intermediate issuer keys. */
				CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
				if (dgst) {
					/* Check dgst against gray list. */
					if (CFSetContainsValue(grayListKeys, dgst)) {
						/* Check allow list for this graylisted issuer key,
						   which is the authority key of the issued cert at ix-1.
						*/
						SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
						bool allowed = path && SecCertificatePathVCIsAllowlisted(path);
						if (!allowed) {
							SecPVCSetResultForced(pvc, kSecPolicyCheckGrayListedKey,
							                      ix, kCFBooleanFalse, true);
						}
					}
					CFRelease(dgst);
				}
			}
			CFRelease(grayListKeys);
			return SecPVCIsOkResult(pvc);
		}
	}
	// Assume ok
	return true;
}

static bool SecPVCContainsPolicy(SecPVCRef pvc, CFStringRef searchOid, CFStringRef searchName, CFIndex *policyIX) {
    if (!isString(searchName) && !isString(searchOid)) {
        return false;
    }
	CFArrayRef policies = pvc->policies;
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, ix);
		CFStringRef policyName = SecPolicyGetName(policy);
        CFStringRef policyOid = SecPolicyGetOidString(policy);
        /* Prefer a match of both name and OID */
        if (searchOid && searchName && policyOid && policyName) {
            if (CFEqual(searchOid, policyOid) &&
                CFEqual(searchName, policyName)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
            /* <rdar://40617139> sslServer trust settings need to apply to evals using policyName pinning
             * but make sure we don't use this for SSL Client trust settings or policies. */
            if (CFEqual(searchOid, policyOid) &&
                CFEqual(searchName, kSecPolicyNameSSLServer) && !CFEqual(policyName, kSecPolicyNameSSLClient)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
        /* Next best is just OID. */
        if (!searchName && searchOid && policyOid) {
            if (CFEqual(searchOid, policyOid)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
        if (!searchOid && searchName && policyName) {
            if (CFEqual(searchName, policyName)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
	}
	return false;
}

static bool SecPVCContainsString(SecPVCRef pvc, CFIndex policyIX, CFStringRef stringValue) {
    if (!isString(stringValue)) {
        return false;
    }
    bool result = false;

    /* <rdar://27754596> Previous versions of macOS null-terminated the string, so we need to strip it. */
    CFStringRef tmpStringValue = NULL;
    if (CFStringGetCharacterAtIndex(stringValue, CFStringGetLength(stringValue) -1) == (UniChar)0x0000) {
        tmpStringValue = CFStringCreateTruncatedCopy(stringValue, CFStringGetLength(stringValue) - 1);
    } else {
        tmpStringValue = CFStringCreateCopy(NULL, stringValue);
    }
    /* Some users have strings that only contain the null-termination, so we need to check that we
     * still have a string. */
    require(tmpStringValue, out);

    if (policyIX >= 0 && policyIX < CFArrayGetCount(pvc->policies)) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, policyIX);
        /* Have to look for all the possible locations of name string */
        CFStringRef policyString = NULL;
        policyString = CFDictionaryGetValue(policy->_options, kSecPolicyCheckSSLHostname);
        if (!policyString) {
            policyString = CFDictionaryGetValue(policy->_options, kSecPolicyCheckEmail);
        }
        if (policyString && (CFStringCompare(tmpStringValue, policyString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
            result = true;
            goto out;
        }

        CFArrayRef policyStrings = NULL;
        policyStrings = CFDictionaryGetValue(policy->_options, kSecPolicyCheckEAPTrustedServerNames);
        if (policyStrings && CFArrayContainsValue(policyStrings,
                                                  CFRangeMake(0, CFArrayGetCount(policyStrings)),
                                                  tmpStringValue)) {
            result = true;
            goto out;
        }
    }

out:
    CFReleaseNull(tmpStringValue);
    return result;
}


static uint32_t ts_key_usage_for_kuNumber(CFNumberRef keyUsageNumber) {
    uint32_t ourTSKeyUsage = 0;
    uint32_t keyUsage = 0;
    if (keyUsageNumber &&
        CFNumberGetValue(keyUsageNumber, kCFNumberSInt32Type, &keyUsage)) {
        if (keyUsage & kSecKeyUsageDigitalSignature) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseSignature;
        }
        if (keyUsage & kSecKeyUsageDataEncipherment) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseEnDecryptData;
        }
        if (keyUsage & kSecKeyUsageKeyEncipherment) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseEnDecryptKey;
        }
        if (keyUsage & kSecKeyUsageKeyAgreement) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseKeyExchange;
        }
        if (keyUsage == kSecKeyUsageAll) {
            ourTSKeyUsage = kSecTrustSettingsKeyUseAny;
        }
    }
    return ourTSKeyUsage;
}

static uint32_t ts_key_usage_for_policy(SecPolicyRef policy) {
    uint32_t ourTSKeyUsage = 0;
    CFTypeRef policyKeyUsageType = NULL;

    policyKeyUsageType = (CFTypeRef)CFDictionaryGetValue(policy->_options, kSecPolicyCheckKeyUsage);
    if (isArray(policyKeyUsageType)) {
        CFIndex ix, count = CFArrayGetCount(policyKeyUsageType);
        for (ix = 0; ix < count; ix++) {
            CFNumberRef policyKeyUsageNumber = NULL;
            policyKeyUsageNumber = (CFNumberRef)CFArrayGetValueAtIndex(policyKeyUsageType, ix);
            ourTSKeyUsage |= ts_key_usage_for_kuNumber(policyKeyUsageNumber);
        }
    } else if (isNumber(policyKeyUsageType)) {
        ourTSKeyUsage |= ts_key_usage_for_kuNumber(policyKeyUsageType);
    }

    return ourTSKeyUsage;
}

static bool SecPVCContainsTrustSettingsKeyUsage(SecPVCRef pvc,
    SecCertificateRef certificate, CFIndex policyIX, CFNumberRef keyUsageNumber) {
    int64_t keyUsageValue = 0;
    uint32_t ourKeyUsage = 0;

    if (!isNumber(keyUsageNumber) || !CFNumberGetValue(keyUsageNumber, kCFNumberSInt64Type, &keyUsageValue)) {
        return false;
    }

    if (keyUsageValue == kSecTrustSettingsKeyUseAny) {
        return true;
    }

    /* We're using the key for revocation if we have the OCSPSigner policy.
     * @@@ If we support CRLs, we'd need to check for that policy here too.
     */
    if (SecPVCContainsPolicy(pvc, kSecPolicyAppleOCSPSigner, NULL, NULL)) {
        ourKeyUsage |= kSecTrustSettingsKeyUseSignRevocation;
    }

    /* We're using the key for verifying a cert if it's a root/intermediate
     * in the chain. If the cert isn't in the path yet, we're about to add it,
     * so it's a root/intermediate. If there is no path, this is the leaf.
     */
    CFIndex pathIndex = -1;
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    if (path) {
        pathIndex = SecCertificatePathVCGetIndexOfCertificate(path, certificate);
    } else {
        pathIndex = 0;
    }
    if (pathIndex != 0) {
        ourKeyUsage |= kSecTrustSettingsKeyUseSignCert;
    }

    /* The rest of the key usages may be specified by the policy(ies). */
    if (policyIX >= 0 && policyIX < CFArrayGetCount(pvc->policies)) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, policyIX);
        ourKeyUsage |= ts_key_usage_for_policy(policy);
    } else {
        /* Get key usage from ALL policies */
        CFIndex ix, count = CFArrayGetCount(pvc->policies);
        for (ix = 0; ix < count; ix++) {
            SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, ix);
            ourKeyUsage |= ts_key_usage_for_policy(policy);
        }
    }

    if (ourKeyUsage == (uint32_t)(keyUsageValue & 0x00ffffffff)) {
        return true;
    }

    return false;
}

#if TARGET_OS_OSX
#include <Security/SecTrustedApplicationPriv.h>
#include <Security/SecTask.h>
#include <Security/SecTaskPriv.h>
#include <bsm/libbsm.h>
#include <libproc.h>

extern OSStatus SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement);

static bool SecPVCCallerIsApplication(CFDataRef clientAuditToken, CFTypeRef appRef) {
    bool result = false;
    audit_token_t auditToken = {};
    SecTaskRef task = NULL;
    SecRequirementRef requirement = NULL;
    CFStringRef stringRequirement = NULL;

    require_quiet(appRef && clientAuditToken, out);
    require(CFGetTypeID(appRef) == SecTrustedApplicationGetTypeID(), out);
    require_noerr(SecTrustedApplicationCopyRequirement((SecTrustedApplicationRef)appRef, &requirement), out);
    require(requirement, out);
    require_noerr(SecRequirementsCopyString(requirement, kSecCSDefaultFlags, &stringRequirement), out);
    require(stringRequirement, out);

    require(sizeof(auditToken) == CFDataGetLength(clientAuditToken), out);
    CFDataGetBytes(clientAuditToken, CFRangeMake(0, sizeof(auditToken)), (uint8_t *)&auditToken);
    require(task = SecTaskCreateWithAuditToken(NULL, auditToken), out);

    if(errSecSuccess == SecTaskValidateForRequirement(task, stringRequirement)) {
        result = true;
    }

out:
    CFReleaseNull(task);
    CFReleaseNull(requirement);
    CFReleaseNull(stringRequirement);
    return result;
}
#endif

static bool SecPVCContainsTrustSettingsPolicyOption(SecPVCRef pvc, CFDictionaryRef options) {
    if (!isDictionary(options)) {
        return false;
    }

    /* Push */
    CFDictionaryRef currentCallbacks = pvc->callbacks;

    /* We need to run the leaf and path checks using these options. */
    pvc->callbacks = gSecPolicyLeafCallbacks;
    CFDictionaryApplyFunction(options, SecPVCValidateKey, pvc);

    pvc->callbacks = gSecPolicyPathCallbacks;
    CFDictionaryApplyFunction(options, SecPVCValidateKey, pvc);

    /* Pop */
    pvc->callbacks = currentCallbacks;

    /* Our work here is done; no need to claim a match */
    return false;
}

static bool SecPVCMeetsConstraint(SecPVCRef pvc, SecCertificateRef certificate, CFDictionaryRef constraint) {
    CFStringRef policyOid = NULL, policyString = NULL, policyName = NULL;
    CFNumberRef keyUsageNumber = NULL;
    CFTypeRef trustedApplicationData = NULL;
    CFDictionaryRef policyOptions = NULL;

    bool policyMatch = false, policyStringMatch = false, applicationMatch = false ,
         keyUsageMatch = false, policyOptionMatch = false;
    bool result = false;

#if TARGET_OS_OSX
    /* OS X returns a SecPolicyRef in the constraints. Convert to the oid string. */
    SecPolicyRef policy = NULL;
    policy = (SecPolicyRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicy);
    policyOid = (policy) ? policy->_oid : NULL;
#else
    policyOid = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicy);
#endif
    policyName = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyName);
    policyString = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyString);
    keyUsageNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsKeyUsage);
    policyOptions = (CFDictionaryRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyOptions);

    CFIndex policyIX = -1;
    policyMatch = SecPVCContainsPolicy(pvc, policyOid, policyName, &policyIX);
    policyStringMatch = SecPVCContainsString(pvc, policyIX, policyString);
    keyUsageMatch = SecPVCContainsTrustSettingsKeyUsage(pvc, certificate, policyIX, keyUsageNumber);
    policyOptionMatch = SecPVCContainsTrustSettingsPolicyOption(pvc, policyOptions);

#if TARGET_OS_OSX
    trustedApplicationData =  CFDictionaryGetValue(constraint, kSecTrustSettingsApplication);
    CFDataRef clientAuditToken = SecPathBuilderCopyClientAuditToken(pvc->builder);
    applicationMatch = SecPVCCallerIsApplication(clientAuditToken, trustedApplicationData);
    CFReleaseNull(clientAuditToken);
#else
    if(CFDictionaryContainsKey(constraint, kSecTrustSettingsApplication)) {
        secerror("kSecTrustSettingsApplication is not yet supported on this platform");
    }
#endif

    /* If we either didn't find the parameter in the dictionary or we got a match
     * against that parameter, for all possible parameters in the dictionary, then
     * this trust setting result applies to the output. */
    if (((!policyOid && !policyName) || policyMatch) &&
        (!policyString || policyStringMatch) &&
        (!trustedApplicationData || applicationMatch) &&
        (!keyUsageNumber || keyUsageMatch) &&
        (!policyOptions || policyOptionMatch)) {
        result = true;
    }

    return result;
}

static SecTrustSettingsResult SecPVCGetTrustSettingsResult(SecPVCRef pvc, SecCertificateRef certificate, CFArrayRef constraints) {
    SecTrustSettingsResult result = kSecTrustSettingsResultInvalid;
    CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);
    for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
        CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
        if (!isDictionary(constraint)) {
            continue;
        }

        CFNumberRef resultNumber = NULL;
        resultNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsResult);
        uint32_t resultValue = kSecTrustSettingsResultInvalid;
        if (!isNumber(resultNumber) || !CFNumberGetValue(resultNumber, kCFNumberSInt32Type, &resultValue)) {
            /* no SecTrustSettingsResult entry defaults to TrustRoot*/
            resultValue = kSecTrustSettingsResultTrustRoot;
        }

        if (SecPVCMeetsConstraint(pvc, certificate, constraint)) {
            result = resultValue;
            break;
        }
    }
    return result;
}

/* This function assumes that the input source is an anchor source */
bool SecPVCIsAnchorPerConstraints(SecPVCRef pvc, SecCertificateSourceRef source, SecCertificateRef certificate) {
    __block bool result = false;
    CFArrayRef constraints = NULL;
    constraints = SecCertificateSourceCopyUsageConstraints(source, certificate);

    /* Unrestricted certificates:
     *      -those that come from anchor sources with no constraints
     *      -self-signed certificates with empty contraints arrays
     */
    Boolean selfSigned = false;
    require(errSecSuccess == SecCertificateIsSelfSigned(certificate, &selfSigned), out);
    if ((NULL == source->copyUsageConstraints) ||
        (constraints && (CFArrayGetCount(constraints) == 0) && selfSigned)) {
        secinfo("trust", "unrestricted anchor%s",
                (NULL == source->copyUsageConstraints) ? " source" : "");
        result = true;
        goto out;
    }

    /* Get the trust settings result for the PVC. Only one PVC need match to
     * trigger the anchor behavior -- policy validation will handle whether the
     * path is truly anchored for that PVC. */
    require_quiet(constraints, out);
    SecTrustSettingsResult settingsResult = kSecTrustSettingsResultInvalid;
    settingsResult = SecPVCGetTrustSettingsResult(pvc,
                                                  certificate,
                                                  constraints);
    if ((selfSigned && settingsResult == kSecTrustSettingsResultTrustRoot) ||
        (!selfSigned && settingsResult == kSecTrustSettingsResultTrustAsRoot)) {
        // For our purposes, this is an anchor.
        secinfo("trust", "complex trust settings anchor");
        result = true;
    }

    if (settingsResult == kSecTrustSettingsResultDeny) {
        /* We consider denied certs "anchors" because the trust decision
         is set regardless of building the chain further. The policy
         validation will handle rejecting this chain. */
        secinfo("trust", "complex trust settings denied anchor");
        result = true;
    }

out:
    CFReleaseNull(constraints);
    return result;
}

static void SecPVCCheckUsageConstraints(SecPVCRef pvc) {
    CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    for (certIX = 0; certIX < certCount; certIX++) {
        SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
        CFArrayRef constraints = SecCertificatePathVCGetUsageConstraintsAtIndex(path, certIX);
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, certIX);
        SecTrustSettingsResult result = SecPVCGetTrustSettingsResult(pvc, cert, constraints);

        /* Set the pvc trust result based on the usage constraints and anchor source. */
        if (result == kSecTrustSettingsResultDeny) {
            SecPVCSetResultForced(pvc, kSecPolicyCheckUsageConstraints, certIX, kCFBooleanFalse, true);
        } else if ((result == kSecTrustSettingsResultTrustRoot || result == kSecTrustSettingsResultTrustAsRoot ||
                    result == kSecTrustSettingsResultInvalid) && SecPVCIsOkResult(pvc)) {
            /* If we already think the PVC is ok and this cert is from one of the user/
             * admin anchor sources, trustRoot, trustAsRoot, and Invalid (no constraints),
             * all mean we should use the special "Proceed" trust result. */
            if (SecPathBuilderIsAnchorSource(pvc->builder, kSecUserAnchorSource) &&
                SecCertificateSourceContains(kSecUserAnchorSource, cert)) {
                pvc->result = kSecTrustResultProceed;
            }
#if TARGET_OS_OSX
            if (SecPathBuilderIsAnchorSource(pvc->builder, kSecLegacyAnchorSource) &&
                SecCertificateSourceContains(kSecLegacyAnchorSource, cert)) {
                pvc->result = kSecTrustResultProceed;
            }
#endif
        }
    }
}

static const UInt8 kTestDateConstraintsRoot[kSecPolicySHA256Size] = {
    0x51,0xA0,0xF3,0x1F,0xC0,0x1D,0xEC,0x87,0x32,0xB6,0xFD,0x13,0x6A,0x43,0x4D,0x6C,
    0x87,0xCD,0x62,0xE0,0x38,0xB4,0xFB,0xD6,0x40,0xB0,0xFD,0x62,0x4D,0x1F,0xCF,0x6D
};
static const UInt8 kWS_CA1_G2[kSecPolicySHA256Size] = {
    0xD4,0x87,0xA5,0x6F,0x83,0xB0,0x74,0x82,0xE8,0x5E,0x96,0x33,0x94,0xC1,0xEC,0xC2,
    0xC9,0xE5,0x1D,0x09,0x03,0xEE,0x94,0x6B,0x02,0xC3,0x01,0x58,0x1E,0xD9,0x9E,0x16
};
static const UInt8 kWS_CA1_NEW[kSecPolicySHA256Size] = {
    0x4B,0x22,0xD5,0xA6,0xAE,0xC9,0x9F,0x3C,0xDB,0x79,0xAA,0x5E,0xC0,0x68,0x38,0x47,
    0x9C,0xD5,0xEC,0xBA,0x71,0x64,0xF7,0xF2,0x2D,0xC1,0xD6,0x5F,0x63,0xD8,0x57,0x08
};
static const UInt8 kWS_CA2_NEW[kSecPolicySHA256Size] = {
    0xD6,0xF0,0x34,0xBD,0x94,0xAA,0x23,0x3F,0x02,0x97,0xEC,0xA4,0x24,0x5B,0x28,0x39,
    0x73,0xE4,0x47,0xAA,0x59,0x0F,0x31,0x0C,0x77,0xF4,0x8F,0xDF,0x83,0x11,0x22,0x54
};
static const UInt8 kWS_ECC[kSecPolicySHA256Size] = {
    0x8B,0x45,0xDA,0x1C,0x06,0xF7,0x91,0xEB,0x0C,0xAB,0xF2,0x6B,0xE5,0x88,0xF5,0xFB,
    0x23,0x16,0x5C,0x2E,0x61,0x4B,0xF8,0x85,0x56,0x2D,0x0D,0xCE,0x50,0xB2,0x9B,0x02
};
static const UInt8 kSC_SFSCA[kSecPolicySHA256Size] = {
    0xC7,0x66,0xA9,0xBE,0xF2,0xD4,0x07,0x1C,0x86,0x3A,0x31,0xAA,0x49,0x20,0xE8,0x13,
    0xB2,0xD1,0x98,0x60,0x8C,0xB7,0xB7,0xCF,0xE2,0x11,0x43,0xB8,0x36,0xDF,0x09,0xEA
};
static const UInt8 kSC_SHA2[kSecPolicySHA256Size] = {
    0xE1,0x78,0x90,0xEE,0x09,0xA3,0xFB,0xF4,0xF4,0x8B,0x9C,0x41,0x4A,0x17,0xD6,0x37,
    0xB7,0xA5,0x06,0x47,0xE9,0xBC,0x75,0x23,0x22,0x72,0x7F,0xCC,0x17,0x42,0xA9,0x11
};
static const UInt8 kSC_G2[kSecPolicySHA256Size] = {
    0xC7,0xBA,0x65,0x67,0xDE,0x93,0xA7,0x98,0xAE,0x1F,0xAA,0x79,0x1E,0x71,0x2D,0x37,
    0x8F,0xAE,0x1F,0x93,0xC4,0x39,0x7F,0xEA,0x44,0x1B,0xB7,0xCB,0xE6,0xFD,0x59,0x95
};

static void SecPVCCheckIssuerDateConstraints(SecPVCRef pvc) {
    static CFSetRef sConstrainedRoots = NULL;
    static dispatch_once_t _t;
    dispatch_once(&_t, ^{
        const UInt8 *v_hashes[] = {
            kWS_CA1_G2, kWS_CA1_NEW, kWS_CA2_NEW, kWS_ECC,
            kSC_SFSCA, kSC_SHA2, kSC_G2, kTestDateConstraintsRoot
        };
        CFMutableSetRef set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
        CFIndex ix, count = sizeof(v_hashes)/sizeof(*v_hashes);
        for (ix=0; ix<count; ix++) {
            CFDataRef hash = CFDataCreateWithBytesNoCopy(NULL, v_hashes[ix],
                kSecPolicySHA256Size, kCFAllocatorNull);
            if (hash) {
                CFSetAddValue(set, hash);
                CFRelease(hash);
            }
        }
        sConstrainedRoots = set;
    });

    bool shouldDeny = false;
    CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    for (certIX = certCount - 1; certIX >= 0 && !shouldDeny; certIX--) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, certIX);
        CFDataRef sha256 = SecCertificateCopySHA256Digest(cert);
        if (sha256 && CFSetContainsValue(sConstrainedRoots, sha256)) {
            /* matched a constrained root; check notBefore dates on all its children. */
            CFIndex childIX = certIX;
            while (--childIX >= 0) {
                SecCertificateRef child = SecPVCGetCertificateAtIndex(pvc, childIX);
                /* 1 Dec 2016 00:00:00 GMT */
                if (child && (CFAbsoluteTime)502243200.0 <= SecCertificateNotValidBefore(child)) {
                    SecPVCSetResultForced(pvc, kSecPolicyCheckBlackListedKey, certIX, kCFBooleanFalse, true);
                    shouldDeny = true;
                    break;
                }
            }
        }
        CFReleaseNull(sha256);
    }
}

static bool SecPVCPolicyPermitsCTRequired(SecPVCRef pvc) {
#if TARGET_OS_BRIDGE
    return false;
#else // !TARGET_OS_BRIDGE
    if (!pvc || !pvc->policies) {
        return false;
    }
    SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0);
    if (!policy) {
        return false;
    }
    // SSL policy
    CFStringRef policyName = SecPolicyGetName(policy);
    if (CFEqualSafe(policyName, kSecPolicyNameSSLServer)) {
        return true;
    }
    // SSL policy by another name
    CFDictionaryRef options = policy->_options;
    if (options && CFDictionaryGetValue(options, kSecPolicyCheckSSLHostname)) {
        return true;
    }
    // Policy explicitly requires CT
    if (options && CFDictionaryGetValue(options, kSecPolicyCheckCTRequired)) {
        return true;
    }
    return false;
#endif // !TARGET_OS_BRIDGE
}

/* ASSUMPTIONS:
   1. SecPVCCheckRequireCTConstraints must be called after SecPolicyCheckCT,
      so earliest issuance time has already been obtained from SCTs.
   2. If the issuance time value is 0 (i.e. 2001-01-01) or earlier, we
      assume it was not set, and thus we did not have CT info.
*/
static void SecPVCCheckRequireCTConstraints(SecPVCRef pvc) {
    SecCertificatePathVCRef path = (pvc) ? SecPathBuilderGetPath(pvc->builder) : NULL;
    if (!path) {
        return;
    }
    /* If we are evaluating for a SSL server authentication policy, make sure
       SCT issuance time is prior to the earliest not-after date constraint.
       Note that CT will already be required if there is a not-after date
       constraint present (set in SecRVCProcessValidDateConstraints).
    */
    if (SecPVCPolicyPermitsCTRequired(pvc)) {
        CFIndex ix, certCount = SecCertificatePathVCGetCount(path);
        SecCertificateRef certificate = SecPathBuilderGetCertificateAtIndex(pvc->builder, 0);
        CFAbsoluteTime earliestNotAfter = 31556908800.0;  /* default: 3001-01-01 00:00:00-0000 */
        CFAbsoluteTime issuanceTime = SecCertificatePathVCIssuanceTime(path);
        if (issuanceTime <= 0) {
            /* if not set (or prior to 2001-01-01), use leaf's not-before time. */
            issuanceTime = SecCertificateNotValidBefore(certificate);
        }
        for (ix = 0; ix < certCount; ix++) {
            SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, ix);
            if (!rvc || !rvc->valid_info || !rvc->valid_info->hasDateConstraints || !rvc->valid_info->notAfterDate) {
                continue;
            }
            /* Found CA certificate with a not-after date constraint. */
            CFAbsoluteTime caNotAfter = CFDateGetAbsoluteTime(rvc->valid_info->notAfterDate);
            if (caNotAfter < earliestNotAfter) {
                earliestNotAfter = caNotAfter;
            }
            if (issuanceTime > earliestNotAfter) {
                /* Issuance time violates the not-after date constraint. */
                secnotice("rvc", "certificate issuance time (%f) is later than allowed value (%f)",
                          issuanceTime, earliestNotAfter);
                SecRVCSetValidDeterminedErrorResult(rvc);
                break;
            }
        }
    }
    /* If path is CT validated, nothing further to do here. */
    if (SecCertificatePathVCIsCT(path)) {
        return;
    }

    /* Path is not CT validated, so check if CT was required. */
    SecPathCTPolicy ctp = SecCertificatePathVCRequiresCT(path);
    if (ctp <= kSecPathCTNotRequired || !SecPVCPolicyPermitsCTRequired(pvc)) {
        return;
    }

    /* We need to have a recent log list or the CT check may have failed due to the list being out of date.
     * Also, honor the CT kill switch. */
    SecOTAPKIRef otaref = SecOTAPKICopyCurrentOTAPKIRef();
    CFDictionaryRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);
    if (!SecOTAPKIKillSwitchEnabled(otaref, kOTAPKIKillSwitchCT) &&
        (SecOTAPKIAssetStalenessLessThanSeconds(otaref, kSecOTAPKIAssetStalenessDisable) || trustedLogs)) {
        /* CT was required. Error is always set on leaf certificate. */
        if (ctp != kSecPathCTRequiredOverridable) {
            /* Normally kSecPolicyCheckCTRequired is recoverable */
            SecPVCSetResultForcedWithTrustResult(pvc, kSecPolicyCheckCTRequired, 0, kCFBooleanFalse, true,
                                                 kSecTrustResultFatalTrustFailure);
        } else {
            SecPVCSetResultForced(pvc, kSecPolicyCheckCTRequired, 0, kCFBooleanFalse, true);
        }
    }
    CFReleaseNull(otaref);
    CFReleaseNull(trustedLogs);
}

/* "Deep" copy the details array */
static CFArrayRef CF_RETURNS_RETAINED SecPVCCopyDetailsArray(SecPVCRef pvc) {
    CFArrayRef details = pvc->details;
    CFMutableArrayRef copiedDetails = CFArrayCreateMutable(NULL, CFArrayGetCount(details), &kCFTypeArrayCallBacks);
    CFArrayForEach(details, ^(const void *value) {
        CFMutableDictionaryRef copiedValue = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)value);
        CFArrayAppendValue(copiedDetails, copiedValue);
        CFReleaseNull(copiedValue);
    });
    return copiedDetails;
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
void SecPVCPathChecks(SecPVCRef pvc) {
    secdebug("policy", "begin path: %@", SecPathBuilderGetPath(pvc->builder));
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    /* This needs to be initialized before we call any function that might call
       SecPVCSetResultForced(). */
    pvc->policyIX = 0;
    SecPolicyCheckIdLinkage(pvc, kSecPolicyCheckIdLinkage);
    if (SecPVCIsOkResult(pvc) || pvc->details) {
        SecPolicyCheckBasicCertificateProcessing(pvc,
            kSecPolicyCheckBasicCertificateProcessing);
    }

    CFArrayRef policies = pvc->policies;
    CFIndex count = CFArrayGetCount(policies);
    for (; pvc->policyIX < count; ++pvc->policyIX) {
        /* Validate all keys for all policies. */
        pvc->callbacks = gSecPolicyPathCallbacks;
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        CFDictionaryApplyFunction(policy->_options, SecPVCValidateKey, pvc);
        if (!SecPVCIsOkResult(pvc) && !pvc->details)
            return;
    }

    // Reset
    pvc->policyIX = 0;

    /* Check whether the TrustSettings say to deny a cert in the path. */
    SecPVCCheckUsageConstraints(pvc);

    /* Check for Blocklisted certs */
    SecPVCCheckIssuerDateConstraints(pvc);
    CFIndex ix;
    count = SecCertificatePathVCGetCount(path);
    for (ix = 1; ix < count; ix++) {
        SecPVCGrayListedKeyChecks(pvc, ix);
        SecPVCBlackListedKeyChecks(pvc, ix);
    }

    /* Path-based check tests. */
    if (!SecCertificatePathVCIsPathValidated(path)) {
        bool ev_check_ok = false;
        if (SecCertificatePathVCIsOptionallyEV(path)) {
            SecTrustResultType pre_ev_check_result = pvc->result;
            CFArrayRef pre_ev_check_details = pvc->details ? SecPVCCopyDetailsArray(pvc) : NULL;
            SecPolicyCheckEV(pvc, kSecPolicyCheckExtendedValidation);
            ev_check_ok = SecPVCIsOkResult(pvc);
            /* If ev checking failed, we still want to accept this chain
             as a non EV one, if it was valid as such. */
            pvc->result = pre_ev_check_result;
            CFAssignRetained(pvc->details, pre_ev_check_details);
        }

        /* Check for CT */
        /* This call will set the value of pvc->is_ct, but won't change the result (pvc->result) */
        SecPolicyCheckCT(pvc);

        /* Certs are only EV if they are also CT verified (when the Kill Switch isn't enabled and against a recent log list) */
        SecOTAPKIRef otaref = SecOTAPKICopyCurrentOTAPKIRef();
        if (ev_check_ok && (SecCertificatePathVCIsCT(path) || SecOTAPKIKillSwitchEnabled(otaref, kOTAPKIKillSwitchCT) ||
                            !SecOTAPKIAssetStalenessLessThanSeconds(otaref, kSecOTAPKIAssetStalenessDisable))) {
            SecCertificatePathVCSetIsEV(path, true);
        }
        CFReleaseNull(otaref);
    }

    /* Say that we did the expensive path checks (that we want to skip on the second call) */
    SecCertificatePathVCSetPathValidated(SecPathBuilderGetPath(pvc->builder));

    /* Check that this path meets CT constraints. */
    SecPVCCheckRequireCTConstraints(pvc);
    SecPolicyCheckSystemTrustedCTRequired(pvc);

    /* Check that this path meets known-intermediate constraints. */
    SecPathBuilderCheckKnownIntermediateConstraints(pvc->builder);

    secdebug("policy", "end %strusted path: %@",
        (SecPVCIsOkResult(pvc) ? "" : "not "), SecPathBuilderGetPath(pvc->builder));

    return;
}

void SecPVCPathCheckRevocationResponsesReceived(SecPVCRef pvc) {
#if TARGET_OS_WATCH
     /* Since we don't currently allow networking on watchOS,
      * don't enforce the revocation-required check here. (32728029) */
    bool required = false;
#else
    bool required = true;
#endif
    SecCertificatePathVCRef path = SecPathBuilderGetPath(pvc->builder);
    CFIndex ix, certCount = SecCertificatePathVCGetCount(path);
    for (ix = 0; ix < certCount; ix++) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, ix);
        /* Do we have a valid revocation response? */
        if (SecRVCGetEarliestNextUpdate(rvc) == NULL_TIME) {
            /* No valid revocation response.
             * Do we require revocation (for that cert per the
             * SecCertificateVCRef, or per the pvc)? */
            if (required && (SecCertificatePathVCIsRevocationRequiredForCertificateAtIndex(path, ix) ||
                ((ix == 0) && pvc->require_revocation_response))) {
                SecPVCSetResultForced(pvc, kSecPolicyCheckRevocationResponseRequired,
                                      ix, kCFBooleanFalse, true);
            }
            /* Do we have a definitive Valid revocation result for this cert? */
            if (SecRVCHasDefinitiveValidInfo(rvc) && SecRVCHasRevokedValidInfo(rvc)) {
                SecRVCSetValidDeterminedErrorResult(rvc);
            }
        }
    }
}
