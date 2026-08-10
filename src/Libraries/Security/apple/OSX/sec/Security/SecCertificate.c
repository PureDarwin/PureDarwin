/*
 * Copyright (c) 2006-2019 Apple Inc. All Rights Reserved.
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
 * SecCertificate.c - CoreFoundation based certificate object
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include <Security/SecCertificateInternal.h>
#include <utilities/SecIOFormat.h>
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFCalendar.h>
#include <CoreFoundation/CFTimeZone.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <string.h>
#include <AssertMacros.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>
#include <libDER/asn1Types.h>
#include <libDER/oids.h>
#include <Security/SecBasePriv.h>
#include "SecRSAKey.h"
#include "SecFramework.h"
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include "SecSignatureVerificationSupport.h"
#include <stdbool.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecSCTUtils.h>
#include <utilities/array_size.h>
#include <stdlib.h>
#include <libkern/OSByteOrder.h>
#include <ctype.h>
#include <Security/SecInternal.h>
#include <Security/SecFrameworkStrings.h>
#include "SecBase64.h"
#include "AppleBaselineEscrowCertificates.h"
#include "AppleiPhoneDeviceCACertificates.h"
#include <ipc/securityd_client.h>
#include <Security/SecKeyInternal.h>
#include "AppleExternalRootCertificates.h"
#include <Security/SecInternalReleasePriv.h>

#pragma clang diagnostic ignored "-Wformat=2"

/* The minimum key sizes necessary to not be considered "weak" */
#define MIN_RSA_KEY_SIZE    128     // 1024-bit
#define MIN_EC_KEY_SIZE     20      // 160-bit

/* The minimum key sizes necessary to be considered "strong" */
#define MIN_STRONG_RSA_KEY_SIZE     256     // 2048-bit
#define MIN_STRONG_EC_KEY_SIZE      28      // 224-bit

#define IPv4ADDRLEN     4   // 4 octets
#define IPv6ADDRLEN     16  // 16 octets

#define MAX_EXTENSIONS 10000
#define MAX_ATTRIBUTE_TYPE_AND_VALUES 1024
#define MAX_CRL_DPS 1024
#define MAX_CERTIFICATE_POLICIES 8192
#define MAX_POLICY_MAPPINGS 8192
#define MAX_EKUS 8192
#define MAX_AIAS 1024

typedef struct SecCertificateExtension {
	DERItem extnID;
    bool critical;
    DERItem extnValue;
} SecCertificateExtension;

enum {
    kSecSelfSignedUnknown = 0,
    kSecSelfSignedFalse,
    kSecSelfSignedTrue,
};

struct __SecCertificate {
    CFRuntimeBase       _base;

    DERItem             _der;           /* Entire certificate in DER form. */
    DERItem             _tbs;           /* To Be Signed cert DER bytes. */
    DERAlgorithmId      _sigAlg;        /* Top level signature algorithm. */
    DERItem             _signature;     /* The content of the sig bit string. */

    UInt8               _version;
    DERItem             _serialNum;     /* Integer. */
    DERAlgorithmId      _tbsSigAlg;     /* sig alg MUST be same as _sigAlg. */
    DERItem             _issuer;        /* Sequence of RDN. */
    CFAbsoluteTime      _notBefore;
    CFAbsoluteTime      _notAfter;
    DERItem             _subject;       /* Sequence of RDN. */
    DERItem             _subjectPublicKeyInfo; /* SPKI (without tag/length) */
    DERAlgorithmId      _algId;         /* oid and params of _pubKeyDER. */
    DERItem             _pubKeyDER;     /* contents of bit string */
    DERItem             _issuerUniqueID;    /* bit string, optional */
    DERItem             _subjectUniqueID;   /* bit string, optional */

    bool                _foundUnknownCriticalExtension;

    /* Well known certificate extensions. */
    SecCEBasicConstraints       _basicConstraints;
    SecCEPolicyConstraints      _policyConstraints;
    SecCEPolicyMappings         _policyMappings;
    SecCECertificatePolicies    _certificatePolicies;
    SecCEInhibitAnyPolicy       _inhibitAnyPolicySkipCerts;

    /* If KeyUsage extension is not present this is 0, otherwise it's
       the value of the extension. */
    SecKeyUsage _keyUsage;

    /* OCTETS of SubjectKeyIdentifier extensions KeyIdentifier.
       Length = 0 if not present. */
    DERItem             _subjectKeyIdentifier;

    /* OCTETS of AuthorityKeyIdentifier extensions KeyIdentifier.
       Length = 0 if not present. */
    DERItem             _authorityKeyIdentifier;
    /* AuthorityKeyIdentifier extension _authorityKeyIdentifierIssuer and
       _authorityKeyIdentifierSerialNumber have non zero length if present.
       Both are either present or absent together.  */
    DERItem             _authorityKeyIdentifierIssuer;
    DERItem             _authorityKeyIdentifierSerialNumber;

    /* Subject alt name extension, if present.  Not malloced, it's just a
       pointer to an element in the _extensions array. */
    const SecCertificateExtension  *_subjectAltName;

    /* Parsed extension values. */

    /* Array of CFURLRefs containing the URI values of crlDistributionPoints. */
    CFMutableArrayRef   _crlDistributionPoints;

    /* Array of CFURLRefs containing the URI values of accessLocations of each
       id-ad-ocsp AccessDescription in the Authority Information Access
       extension. */
    CFMutableArrayRef   _ocspResponders;

    /* Array of CFURLRefs containing the URI values of accessLocations of each
       id-ad-caIssuers AccessDescription in the Authority Information Access
       extension. */
    CFMutableArrayRef   _caIssuers;

    /* Array of CFDataRefs containing the generalNames for permittedSubtrees
       Name Constraints.*/
    CFArrayRef          _permittedSubtrees;

    /* Array of CFDataRefs containing the generalNames for excludedSubtrees
     Name Constraints. */
    CFArrayRef          _excludedSubtrees;

    CFMutableArrayRef   _embeddedSCTs;

    /* All other (non known) extensions.   The _extensions array is malloced. */
    CFIndex             _extensionCount;
    SecCertificateExtension *_extensions;
    CFIndex             _unparseableKnownExtensionIndex;

    /* Optional cached fields. */
    SecKeyRef           _pubKey;
    CFDataRef           _der_data;
    CFArrayRef          _properties;
    CFDataRef           _serialNumber;
    CFDataRef           _normalizedIssuer;
    CFDataRef           _normalizedSubject;
    CFDataRef           _authorityKeyID;
    CFDataRef           _subjectKeyID;

    CFDataRef           _sha1Digest;
    CFTypeRef           _keychain_item;
    uint8_t             _isSelfSigned;

};

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecCertificateProductionEscrowKey, "ProductionEscrowKey");
SEC_CONST_DECL (kSecCertificateProductionPCSEscrowKey, "ProductionPCSEscrowKey");
SEC_CONST_DECL (kSecCertificateEscrowFileName, "AppleESCertificates");

/* Public Constants for property list keys. */
SEC_CONST_DECL (kSecPropertyKeyType, "type");
SEC_CONST_DECL (kSecPropertyKeyLabel, "label");
SEC_CONST_DECL (kSecPropertyKeyLocalizedLabel, "localized label");
SEC_CONST_DECL (kSecPropertyKeyValue, "value");

/* Public Constants for property list values. */
SEC_CONST_DECL (kSecPropertyTypeWarning, "warning");
SEC_CONST_DECL (kSecPropertyTypeError, "error");
SEC_CONST_DECL (kSecPropertyTypeSuccess, "success");
SEC_CONST_DECL (kSecPropertyTypeTitle, "title");
SEC_CONST_DECL (kSecPropertyTypeSection, "section");
SEC_CONST_DECL (kSecPropertyTypeData, "data");
SEC_CONST_DECL (kSecPropertyTypeString, "string");
SEC_CONST_DECL (kSecPropertyTypeURL, "url");
SEC_CONST_DECL (kSecPropertyTypeDate, "date");
SEC_CONST_DECL (kSecPropertyTypeArray, "array");
SEC_CONST_DECL (kSecPropertyTypeNumber, "number");

/* Extension parsing routine. */
typedef bool (*SecCertificateExtensionParser)(SecCertificateRef certificate,
	const SecCertificateExtension *extn);

/* Mapping from extension OIDs (as a DERItem *) to
   SecCertificateExtensionParser extension parsing routines. */
static CFDictionaryRef sExtensionParsers;

/* Forward declarations of static functions. */
static CFStringRef SecCertificateCopyDescription(CFTypeRef cf);
static void SecCertificateDestroy(CFTypeRef cf);
static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
    CFAbsoluteTime *absTime) __attribute__((__nonnull__));

/* Static functions. */
static CFStringRef SecCertificateCopyDescription(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
    CFStringRef subject = SecCertificateCopySubjectSummary(certificate);
    CFStringRef issuer = SecCertificateCopyIssuerSummary(certificate);
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<cert(%p) s: %@ i: %@>"), certificate, subject, issuer);
    CFReleaseSafe(issuer);
    CFReleaseSafe(subject);
    return desc;
}

static void SecCertificateDestroy(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
    if (certificate->_certificatePolicies.policies) {
        free(certificate->_certificatePolicies.policies);
        certificate->_certificatePolicies.policies = NULL;
    }
    if (certificate->_policyMappings.mappings) {
        free(certificate->_policyMappings.mappings);
        certificate->_policyMappings.mappings = NULL;
    }
    CFReleaseNull(certificate->_crlDistributionPoints);
    CFReleaseNull(certificate->_ocspResponders);
    CFReleaseNull(certificate->_caIssuers);
    if (certificate->_extensions) {
        free(certificate->_extensions);
        certificate->_extensions = NULL;
    }
    CFReleaseNull(certificate->_pubKey);
    CFReleaseNull(certificate->_der_data);
    CFReleaseNull(certificate->_properties);
    CFReleaseNull(certificate->_serialNumber);
    CFReleaseNull(certificate->_normalizedIssuer);
    CFReleaseNull(certificate->_normalizedSubject);
    CFReleaseNull(certificate->_authorityKeyID);
    CFReleaseNull(certificate->_subjectKeyID);
    CFReleaseNull(certificate->_sha1Digest);
    CFReleaseNull(certificate->_keychain_item);
    CFReleaseNull(certificate->_permittedSubtrees);
    CFReleaseNull(certificate->_excludedSubtrees);
}

static Boolean SecCertificateEqual(CFTypeRef cf1, CFTypeRef cf2) {
    SecCertificateRef cert1 = (SecCertificateRef)cf1;
    SecCertificateRef cert2 = (SecCertificateRef)cf2;
    if (cert1 == cert2)
        return true;
    if (!cert2 || cert1->_der.length != cert2->_der.length)
        return false;
    return !memcmp(cert1->_der.data, cert2->_der.data, cert1->_der.length);
}

/* Hash of the certificate is der length + signature length + last 4 bytes
   of signature. */
static CFHashCode SecCertificateHash(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
	size_t der_length = certificate->_der.length;
	size_t sig_length = certificate->_signature.length;
	size_t ix = (sig_length > 4) ? sig_length - 4 : 0;
	CFHashCode hashCode = 0;
	for (; ix < sig_length; ++ix)
		hashCode = (hashCode << 8) + certificate->_signature.data[ix];

	return (hashCode + der_length + sig_length);
}

/************************************************************************/
/************************* General Name Parsing *************************/
/************************************************************************/
/*
      GeneralName ::= CHOICE {
           otherName                       [0]     OtherName,
           rfc822Name                      [1]     IA5String,
           dNSName                         [2]     IA5String,
           x400Address                     [3]     ORAddress,
           directoryName                   [4]     Name,
           ediPartyName                    [5]     EDIPartyName,
           uniformResourceIdentifier       [6]     IA5String,
           iPAddress                       [7]     OCTET STRING,
           registeredID                    [8]     OBJECT IDENTIFIER}

      OtherName ::= SEQUENCE {
           type-id    OBJECT IDENTIFIER,
           value      [0] EXPLICIT ANY DEFINED BY type-id }

      EDIPartyName ::= SEQUENCE {
           nameAssigner            [0]     DirectoryString OPTIONAL,
           partyName               [1]     DirectoryString }
 */
OSStatus SecCertificateParseGeneralNameContentProperty(DERTag tag,
	const DERItem *generalNameContent,
	void *context, parseGeneralNameCallback callback) {
	switch (tag) {
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
		return callback(context, GNT_OtherName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 1:
		return callback(context, GNT_RFC822Name, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 2:
		return callback(context, GNT_DNSName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		return callback(context, GNT_X400Address, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
		return callback(context, GNT_DirectoryName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		return callback(context, GNT_EdiPartyName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
	{
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		DERDecodedInfo uriContent;
		require_noerr(DERDecodeItem(generalNameContent, &uriContent), badDER);
		require(uriContent.tag == ASN1_IA5_STRING, badDER);
		return callback(context, GNT_URI, &uriContent.content);
	}
	case ASN1_CONTEXT_SPECIFIC | 6:
		return callback(context, GNT_URI, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 7:
		return callback(context, GNT_IPAddress, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 8:
		return callback(context, GNT_RegisteredID, generalNameContent);
	default:
		goto badDER;
	}
badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseGeneralNamesContent(const DERItem *generalNamesContent,
	void *context, parseGeneralNameCallback callback) {
    DERSequence gnSeq;
    DERReturn drtn = DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo generalNameContent;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		OSStatus status = SecCertificateParseGeneralNameContentProperty(
			generalNameContent.tag, &generalNameContent.content, context,
				callback);
		if (status)
			return status;
	}
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return errSecSuccess;

badDER:
	return errSecInvalidCertificate;
}

OSStatus SecCertificateParseGeneralNames(const DERItem *generalNames, void *context,
	parseGeneralNameCallback callback) {
    DERDecodedInfo generalNamesContent;
    DERReturn drtn = DERDecodeItem(generalNames, &generalNamesContent);
    require_noerr_quiet(drtn, badDER);
    // GeneralNames ::= SEQUENCE SIZE (1..MAX)
    require_quiet(generalNamesContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
    require_quiet(generalNamesContent.content.length > 0, badDER); // not defining a max due to use of large number of SANs
    return parseGeneralNamesContent(&generalNamesContent.content, context,
		callback);
badDER:
	return errSecInvalidCertificate;
}

/************************************************************************/
/************************** X.509 Name Parsing **************************/
/************************************************************************/

typedef OSStatus (*parseX501NameCallback)(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX, bool localized);

static OSStatus parseRDNContent(const DERItem *rdnSetContent, void *context,
	parseX501NameCallback callback, bool localized) {
	DERSequence rdn;
	DERReturn drtn = DERDecodeSeqContentInit(rdnSetContent, &rdn);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo atvContent;
	CFIndex rdnIX = 0;
	while ((drtn = DERDecodeSeqNext(&rdn, &atvContent)) == DR_Success) {
		require_quiet(atvContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAttributeTypeAndValue atv;
		drtn = DERParseSequenceContent(&atvContent.content,
			DERNumAttributeTypeAndValueItemSpecs,
			DERAttributeTypeAndValueItemSpecs,
			&atv, sizeof(atv));
		require_noerr_quiet(drtn, badDER);
		require_quiet(atv.type.length != 0, badDER);
		OSStatus status = callback(context, &atv.type, &atv.value, rdnIX++, localized);
		if (status) {
			return status;
		}
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);

	return errSecSuccess;
badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseX501NameContent(const DERItem *x501NameContent, void *context,
                                     parseX501NameCallback callback, bool localized) {
    DERSequence derSeq;
    DERReturn drtn = DERDecodeSeqContentInit(x501NameContent, &derSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo currDecoded;
    int atv_count = 0;
    while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
        /*  RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue */
        require_quiet(currDecoded.tag == ASN1_CONSTR_SET, badDER);
        require_quiet(currDecoded.content.length > 0, badDER);
        OSStatus status = parseRDNContent(&currDecoded.content, context,
                                          callback, localized);
        if (status) {
            return status;
        }
        atv_count++;
        require_quiet(atv_count < MAX_ATTRIBUTE_TYPE_AND_VALUES, badDER);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);

    return errSecSuccess;

badDER:
    return errSecInvalidCertificate;
}

static OSStatus parseX501Name(const DERItem *x501Name, void *context,
	parseX501NameCallback callback, bool localized) {
	DERDecodedInfo x501NameContent;
	if (DERDecodeItem(x501Name, &x501NameContent) ||
        x501NameContent.tag != ASN1_CONSTR_SEQUENCE) {
		return errSecInvalidCertificate;
    } else {
        return parseX501NameContent(&x501NameContent.content, context,
			callback, localized);
    }
}

/************************************************************************/
/********************** Extension Parsing Routines **********************/
/************************************************************************/

static bool SecCEPSubjectKeyIdentifier(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERDecodedInfo keyIdentifier;
	DERReturn drtn = DERDecodeItem(&extn->extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	certificate->_subjectKeyIdentifier = keyIdentifier.content;

	return true;
badDER:
	secwarning("Invalid SubjectKeyIdentifier Extension");
    return false;
}

static bool SecCEPKeyUsage(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    SecKeyUsage keyUsage = extn->critical ? kSecKeyUsageCritical : 0;
    DERDecodedInfo bitStringContent;
    DERReturn drtn = DERDecodeItem(&extn->extnValue, &bitStringContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(bitStringContent.tag == ASN1_BIT_STRING, badDER);
    /* check that there's no extra bytes at the end */
    require_quiet(bitStringContent.content.data + bitStringContent.content.length == extn->extnValue.data + extn->extnValue.length, badDER);
    DERSize len = bitStringContent.content.length - 1;
    require_quiet(len == 1 || len == 2, badDER);
    DERByte numUnusedBits = bitStringContent.content.data[0];
    require_quiet(numUnusedBits < 8, badDER);
    /* Flip the bits in the bit string so the first bit is the lsb. */
    uint16_t bits = 8 * len - numUnusedBits;
    uint16_t value = bitStringContent.content.data[1];
    uint16_t mask;
    if (len > 1) {
        value = (value << 8) + bitStringContent.content.data[2];
        mask = 0x8000;
    } else {
        mask = 0x80;
    }
    uint16_t ix;
    for (ix = 0; ix < bits; ++ix) {
        if (value & mask) {
            keyUsage |= 1 << ix;
        }
        mask >>= 1;
    }
    certificate->_keyUsage = keyUsage;
    return true;
badDER:
    certificate->_keyUsage = kSecKeyUsageUnspecified;
    secwarning("Invalid KeyUsage Extension");
    return false;
}

static bool SecCEPPrivateKeyUsagePeriod(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static OSStatus verifySubjectAltGeneralName(void *context, SecCEGeneralNameType type,
                                            const DERItem *value) {
    // Nothing to do for now
    return errSecSuccess;
}

static bool SecCEPSubjectAltName(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    // Make sure that the SAN is parse-able
    require_noerr_quiet(SecCertificateParseGeneralNames(&extn->extnValue, NULL, verifySubjectAltGeneralName), badDER);
	certificate->_subjectAltName = extn;
    return true;
badDER:
    certificate->_subjectAltName = NULL;
    secwarning("Invalid SubjectAltName Extension");
    return false;
}

static bool SecCEPIssuerAltName(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static bool SecCEPBasicConstraints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERBasicConstraints basicConstraints;
	require_noerr_quiet(DERParseSequence(&extn->extnValue,
        DERNumBasicConstraintsItemSpecs, DERBasicConstraintsItemSpecs,
        &basicConstraints, sizeof(basicConstraints)), badDER);
    require_noerr_quiet(DERParseBooleanWithDefault(&basicConstraints.cA, false,
		&certificate->_basicConstraints.isCA), badDER);
    if (basicConstraints.pathLenConstraint.length != 0) {
        require_noerr_quiet(DERParseInteger(
            &basicConstraints.pathLenConstraint,
            &certificate->_basicConstraints.pathLenConstraint), badDER);
		certificate->_basicConstraints.pathLenConstraintPresent = true;
	}
    certificate->_basicConstraints.present = true;
	certificate->_basicConstraints.critical = extn->critical;
    return true;
badDER:
    certificate->_basicConstraints.present = false;
	secwarning("Invalid BasicConstraints Extension");
    return false;
}


/*
 * id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
 *
 * NameConstraints ::= SEQUENCE {
 * permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 * excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 * GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * GeneralSubtree ::= SEQUENCE {
 * base                    GeneralName,
 * minimum         [0]     BaseDistance DEFAULT 0,
 * maximum         [1]     BaseDistance OPTIONAL }
 *
 * BaseDistance ::= INTEGER (0..MAX)
 */
static DERReturn parseGeneralSubtrees(DERItem *derSubtrees, CFArrayRef *generalSubtrees) {
    CFMutableArrayRef gs = NULL;
    DERSequence gsSeq;
    DERReturn drtn = DERDecodeSeqContentInit(derSubtrees, &gsSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo gsContent;
    require_quiet(gs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                  &kCFTypeArrayCallBacks),
                  badDER);
    while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
        DERGeneralSubtree derGS;
        require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
        drtn = DERParseSequenceContent(&gsContent.content,
                                       DERNumGeneralSubtreeItemSpecs,
                                       DERGeneralSubtreeItemSpecs,
                                       &derGS, sizeof(derGS));
        require_noerr_quiet(drtn, badDER);
        /*
         * RFC 5280 4.2.1.10
         * Within this profile, the minimum and maximum fields are not used with
         * any name forms, thus, the minimum MUST be zero, and maximum MUST be
         * absent.
         *
         * Because minimum DEFAULT 0, absence equivalent to present and 0.
         */
        if (derGS.minimum.length) {
            uint32_t minimum;
            require_noerr_quiet(DERParseInteger(&derGS.minimum, &minimum),
                                badDER);
            require_quiet(minimum == 0, badDER);
        }
        require_quiet(derGS.maximum.length == 0, badDER);
        require_quiet(derGS.generalName.length != 0, badDER);

        CFDataRef generalName = NULL;
        require_quiet(generalName = CFDataCreate(kCFAllocatorDefault,
                                             derGS.generalName.data,
                                             derGS.generalName.length),
                                             badDER);
        CFArrayAppendValue(gs, generalName);
        CFReleaseNull(generalName);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);

    // since generalSubtrees is a pointer to an instance variable,
    // make sure we release the existing array before assignment.
    CFReleaseSafe(*generalSubtrees);
    *generalSubtrees = gs;

    return DR_Success;

badDER:
    CFReleaseNull(gs);
    secdebug("cert","failed to parse GeneralSubtrees");
    return drtn;
}

static bool SecCEPNameConstraints(SecCertificateRef certificate,
    const SecCertificateExtension *extn) {
    secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERNameConstraints nc;
    DERReturn drtn;
    drtn = DERParseSequence(&extn->extnValue,
                            DERNumNameConstraintsItemSpecs,
                            DERNameConstraintsItemSpecs,
                            &nc, sizeof(nc));
    require_noerr_quiet(drtn, badDER);
    if (nc.permittedSubtrees.length) {
        require_noerr_quiet(parseGeneralSubtrees(&nc.permittedSubtrees, &certificate->_permittedSubtrees), badDER);
    }
    if (nc.excludedSubtrees.length) {
        require_noerr_quiet(parseGeneralSubtrees(&nc.excludedSubtrees, &certificate->_excludedSubtrees), badDER);
    }

    return true;
badDER:
    secwarning("Invalid Name Constraints extension");
    return false;
}

static OSStatus appendCRLDPFromGeneralNames(void *context, SecCEGeneralNameType type,
                                                   const DERItem *value) {
    CFMutableArrayRef *crlDPs = (CFMutableArrayRef *)context;
    if (type == GNT_URI) {
        CFURLRef url = NULL;
        url = CFURLCreateWithBytes(NULL, value->data, value->length, kCFStringEncodingASCII, NULL);
        if (url) {
            if (!*crlDPs) {
                *crlDPs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            }
            CFArrayAppendValue(*crlDPs, url);
            CFRelease(url);
        }
    }
    return errSecSuccess;
}

/*
 id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 }

 CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint

 DistributionPoint ::= SEQUENCE {
    distributionPoint       [0]     DistributionPointName OPTIONAL,
    reasons                 [1]     ReasonFlags OPTIONAL,
    cRLIssuer               [2]     GeneralNames OPTIONAL }

 DistributionPointName ::= CHOICE {
    fullName                [0]     GeneralNames,
    nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
 */
static bool SecCEPCrlDistributionPoints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERSequence crlDPSeq;
    DERTag tag;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &crlDPSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    require_quiet(crlDPSeq.nextItem != crlDPSeq.end, badDER);
    DERDecodedInfo dpContent;
    int crldp_count = 0;
    while ((drtn = DERDecodeSeqNext(&crlDPSeq, &dpContent)) == DR_Success) {
        require_quiet(dpContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERDistributionPoint dp;
        drtn = DERParseSequenceContent(&dpContent.content, DERNumDistributionPointItemSpecs,
                                       DERDistributionPointItemSpecs, &dp, sizeof(dp));
        require_noerr_quiet(drtn, badDER);
        require_quiet(dp.distributionPoint.data || dp.cRLIssuer.data, badDER);
        if (dp.distributionPoint.data) {
            DERDecodedInfo dpName;
            drtn = DERDecodeItem(&dp.distributionPoint, &dpName);
            require_noerr_quiet(drtn, badDER);
            switch (dpName.tag) {
                case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
                    drtn = parseGeneralNamesContent(&dpName.content, &certificate->_crlDistributionPoints,
                                                    appendCRLDPFromGeneralNames);
                    require_noerr_quiet(drtn, badDER);
                    break;
                case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1:
                    /* RelativeDistinguishName. Nothing we can do with that. */
                    break;
                default:
                    goto badDER;
            }
        }
        if (dp.cRLIssuer.data) {
            drtn = parseGeneralNamesContent(&dp.cRLIssuer, &certificate->_crlDistributionPoints,
                                                   appendCRLDPFromGeneralNames);
            require_noerr_quiet(drtn, badDER);
        }
        crldp_count++;
        require_quiet(crldp_count < MAX_CRL_DPS, badDER);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    return true;
badDER:
    secwarning("Invalid CRL Distribution Points extension");
    return false;
}

/*
   certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation

   PolicyInformation ::= SEQUENCE {
        policyIdentifier   CertPolicyId,
        policyQualifiers   SEQUENCE SIZE (1..MAX) OF
                                PolicyQualifierInfo OPTIONAL }

   CertPolicyId ::= OBJECT IDENTIFIER

   PolicyQualifierInfo ::= SEQUENCE {
        policyQualifierId  PolicyQualifierId,
        qualifier          ANY DEFINED BY policyQualifierId }
*/
static bool SecCEPCertificatePolicies(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence piSeq;
    SecCEPolicyInformation *policies = NULL;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo piContent;
    DERSize policy_count = 0;
    while ((policy_count < MAX_CERTIFICATE_POLICIES) &&
           (drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        require_quiet(piContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        policy_count++;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    require_quiet(policy_count > 0, badDER);
    require_quiet(policies = (SecCEPolicyInformation *)malloc(sizeof(SecCEPolicyInformation) * policy_count),
                  badDER);
    drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    DERSize policy_ix = 0;
    while ((policy_ix < policy_count) &&
           (DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        DERPolicyInformation pi;
        drtn = DERParseSequenceContent(&piContent.content,
            DERNumPolicyInformationItemSpecs,
            DERPolicyInformationItemSpecs,
            &pi, sizeof(pi));
        require_noerr_quiet(drtn, badDER);
        policies[policy_ix].policyIdentifier = pi.policyIdentifier;
        policies[policy_ix++].policyQualifiers = pi.policyQualifiers;
    }
    certificate->_certificatePolicies.present = true;
    certificate->_certificatePolicies.critical = extn->critical;
    certificate->_certificatePolicies.numPolicies = policy_count;
    certificate->_certificatePolicies.policies = policies;
	return true;
badDER:
    if (policies)
        free(policies);
    certificate->_certificatePolicies.present = false;
	secwarning("Invalid CertificatePolicies Extension");
    return false;
}

/*
   id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }

   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
        issuerDomainPolicy      CertPolicyId,
        subjectDomainPolicy     CertPolicyId }
*/
static bool SecCEPPolicyMappings(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence pmSeq;
    SecCEPolicyMapping *mappings = NULL;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo pmContent;
    DERSize mapping_count = 0;
    while ((mapping_count < MAX_POLICY_MAPPINGS) &&
           (drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        mapping_count++;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    require_quiet(mapping_count > 0, badDER); // PolicyMappings ::= SEQUENCE SIZE (1..MAX)
    require_quiet(mappings = (SecCEPolicyMapping *)malloc(sizeof(SecCEPolicyMapping) * mapping_count),
                  badDER);
    drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    require_noerr_quiet(drtn, badDER);
    DERSize mapping_ix = 0;
    while ((mapping_ix < mapping_count) &&
           (DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERPolicyMapping pm;
        drtn = DERParseSequenceContent(&pmContent.content,
            DERNumPolicyMappingItemSpecs,
            DERPolicyMappingItemSpecs,
            &pm, sizeof(pm));
        require_noerr_quiet(drtn, badDER);
        mappings[mapping_ix].issuerDomainPolicy = pm.issuerDomainPolicy;
        mappings[mapping_ix++].subjectDomainPolicy = pm.subjectDomainPolicy;
    }
    certificate->_policyMappings.present = true;
    certificate->_policyMappings.critical = extn->critical;
    certificate->_policyMappings.numMappings = mapping_count;
    certificate->_policyMappings.mappings = mappings;
	return true;
badDER:
    if (mappings) {
        free(mappings);
    }
    certificate->_policyMappings.present = false;
	secwarning("Invalid PolicyMappings Extension");
    return false;
}

/*
AuthorityKeyIdentifier ::= SEQUENCE {
    keyIdentifier             [0] KeyIdentifier            OPTIONAL,
    authorityCertIssuer       [1] GeneralNames             OPTIONAL,
    authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
    -- authorityCertIssuer and authorityCertSerialNumber MUST both
    -- be present or both be absent

KeyIdentifier ::= OCTET STRING
*/
static bool SecCEPAuthorityKeyIdentifier(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERAuthorityKeyIdentifier akid;
	DERReturn drtn;
	drtn = DERParseSequence(&extn->extnValue,
		DERNumAuthorityKeyIdentifierItemSpecs,
		DERAuthorityKeyIdentifierItemSpecs,
		&akid, sizeof(akid));
	require_noerr_quiet(drtn, badDER);
	if (akid.keyIdentifier.length) {
		certificate->_authorityKeyIdentifier = akid.keyIdentifier;
	}
	if (akid.authorityCertIssuer.length ||
		akid.authorityCertSerialNumber.length) {
		require_quiet(akid.authorityCertIssuer.length &&
			akid.authorityCertSerialNumber.length, badDER);
		/* Perhaps put in a subsection called Authority Certificate Issuer. */
		certificate->_authorityKeyIdentifierIssuer = akid.authorityCertIssuer;
		certificate->_authorityKeyIdentifierSerialNumber = akid.authorityCertSerialNumber;
	}

	return true;
badDER:
	secwarning("Invalid AuthorityKeyIdentifier Extension");
    return false;
}

static bool SecCEPPolicyConstraints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERPolicyConstraints pc;
	DERReturn drtn;
	drtn = DERParseSequence(&extn->extnValue,
		DERNumPolicyConstraintsItemSpecs,
		DERPolicyConstraintsItemSpecs,
		&pc, sizeof(pc));
	require_noerr_quiet(drtn, badDER);
	if (pc.requireExplicitPolicy.length) {
        require_noerr_quiet(DERParseInteger(
            &pc.requireExplicitPolicy,
            &certificate->_policyConstraints.requireExplicitPolicy), badDER);
        certificate->_policyConstraints.requireExplicitPolicyPresent = true;
	}
	if (pc.inhibitPolicyMapping.length) {
        require_noerr_quiet(DERParseInteger(
            &pc.inhibitPolicyMapping,
            &certificate->_policyConstraints.inhibitPolicyMapping), badDER);
        certificate->_policyConstraints.inhibitPolicyMappingPresent = true;
	}

    certificate->_policyConstraints.present = true;
    certificate->_policyConstraints.critical = extn->critical;

    return true;
badDER:
    certificate->_policyConstraints.present = false;
	secwarning("Invalid PolicyConstraints Extension");
    return false;
}

static bool SecCEPExtendedKeyUsage(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERSequence ekuSeq;
    DERTag ekuTag;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &ekuTag, &ekuSeq);
    require_quiet((drtn == DR_Success) && (ekuTag == ASN1_CONSTR_SEQUENCE), badDER);
    require_quiet(ekuSeq.nextItem != ekuSeq.end, badDER); // ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
    DERDecodedInfo ekuContent;
    int eku_count = 0;
    while ((drtn = DERDecodeSeqNext(&ekuSeq, &ekuContent)) == DR_Success) {
        require_quiet(ekuContent.tag == ASN1_OBJECT_ID, badDER);
        eku_count++;
        require_quiet(eku_count < MAX_EKUS, badDER);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    return true;
badDER:
    secwarning("Invalidate EKU Extension");
    return false;
}

/*
   InhibitAnyPolicy ::= SkipCerts

   SkipCerts ::= INTEGER (0..MAX)
*/
static bool SecCEPInhibitAnyPolicy(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERDecodedInfo iapContent;
    require_noerr_quiet(DERDecodeItem(&extn->extnValue, &iapContent), badDER);
    require_quiet(iapContent.tag == ASN1_INTEGER, badDER);
    require_noerr_quiet(DERParseInteger(
        &iapContent.content,
        &certificate->_inhibitAnyPolicySkipCerts.skipCerts), badDER);

    certificate->_inhibitAnyPolicySkipCerts.present = true;
    certificate->_inhibitAnyPolicySkipCerts.critical = extn->critical;
    return true;
badDER:
    certificate->_inhibitAnyPolicySkipCerts.present = false;
	secwarning("Invalid InhibitAnyPolicy Extension");
    return false;
}

/*
   id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }

   AuthorityInfoAccessSyntax  ::=
           SEQUENCE SIZE (1..MAX) OF AccessDescription

   AccessDescription  ::=  SEQUENCE {
           accessMethod          OBJECT IDENTIFIER,
           accessLocation        GeneralName  }

   id-ad OBJECT IDENTIFIER ::= { id-pkix 48 }

   id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }

   id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
 */
static bool SecCEPAuthorityInfoAccess(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence adSeq;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &adSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    require_quiet(adSeq.nextItem != adSeq.end, badDER);
    DERDecodedInfo adContent;
    int aia_count = 0;
    while ((drtn = DERDecodeSeqNext(&adSeq, &adContent)) == DR_Success) {
        require_quiet(adContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAccessDescription ad;
		drtn = DERParseSequenceContent(&adContent.content,
			DERNumAccessDescriptionItemSpecs,
			DERAccessDescriptionItemSpecs,
			&ad, sizeof(ad));
		require_noerr_quiet(drtn, badDER);
        aia_count++;
        require_quiet(aia_count < MAX_AIAS, badDER);
        CFMutableArrayRef *urls;
        if (DEROidCompare(&ad.accessMethod, &oidAdOCSP))
            urls = &certificate->_ocspResponders;
        else if (DEROidCompare(&ad.accessMethod, &oidAdCAIssuer))
            urls = &certificate->_caIssuers;
        else
            continue;

        DERDecodedInfo generalNameContent;
        drtn = DERDecodeItem(&ad.accessLocation, &generalNameContent);
        require_noerr_quiet(drtn, badDER);
        switch (generalNameContent.tag) {
#if 0
        case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
            /* Technically I don't think this is valid, but there are certs out
               in the wild that use a constructed IA5String.   In particular the
               VeriSign Time Stamping Authority CA.cer does this.  */
#endif
        case ASN1_CONTEXT_SPECIFIC | 6:
        {
            CFURLRef url = CFURLCreateWithBytes(kCFAllocatorDefault,
                generalNameContent.content.data, generalNameContent.content.length,
                kCFStringEncodingASCII, NULL);
            if (url) {
                if (!*urls)
                    *urls = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                CFArrayAppendValue(*urls, url);
                CFRelease(url);
            }
            break;
        }
        default:
            secdebug("cert", "bad general name for id-ad-ocsp AccessDescription t: 0x%02llx v: %.*s",
                generalNameContent.tag, (int) generalNameContent.content.length, generalNameContent.content.data);
            goto badDER;
            break;
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return true;
badDER:
    secwarning("Invalid Authority Information Access extension");
    return false;
}

/* Apple Worldwide Developer Relations Certificate Authority subject name.
 * This is a DER sequence with the leading tag and length bytes removed,
 * to match what tbsCert.issuer contains.
 */
static const unsigned char Apple_WWDR_CA_Subject_Name[]={
                 0x31,0x0B,0x30,0x09,0x06,0x03,0x55,0x04,0x06,0x13,0x02,0x55,0x53,
  0x31,0x13,0x30,0x11,0x06,0x03,0x55,0x04,0x0A,0x0C,0x0A,0x41,0x70,0x70,0x6C,0x65,
  0x20,0x49,0x6E,0x63,0x2E,0x31,0x2C,0x30,0x2A,0x06,0x03,0x55,0x04,0x0B,0x0C,0x23,
  0x41,0x70,0x70,0x6C,0x65,0x20,0x57,0x6F,0x72,0x6C,0x64,0x77,0x69,0x64,0x65,0x20,
  0x44,0x65,0x76,0x65,0x6C,0x6F,0x70,0x65,0x72,0x20,0x52,0x65,0x6C,0x61,0x74,0x69,
  0x6F,0x6E,0x73,0x31,0x44,0x30,0x42,0x06,0x03,0x55,0x04,0x03,0x0C,0x3B,0x41,0x70,
  0x70,0x6C,0x65,0x20,0x57,0x6F,0x72,0x6C,0x64,0x77,0x69,0x64,0x65,0x20,0x44,0x65,
  0x76,0x65,0x6C,0x6F,0x70,0x65,0x72,0x20,0x52,0x65,0x6C,0x61,0x74,0x69,0x6F,0x6E,
  0x73,0x20,0x43,0x65,0x72,0x74,0x69,0x66,0x69,0x63,0x61,0x74,0x69,0x6F,0x6E,0x20,
  0x41,0x75,0x74,0x68,0x6F,0x72,0x69,0x74,0x79
};

static void checkForMissingRevocationInfo(SecCertificateRef certificate) {
	if (!certificate ||
		certificate->_crlDistributionPoints ||
		certificate->_ocspResponders) {
		/* We already have an OCSP or CRL URI (or no cert) */
		return;
	}
	/* Specify an appropriate OCSP responder if we recognize the issuer. */
	CFURLRef url = NULL;
	if (sizeof(Apple_WWDR_CA_Subject_Name) == certificate->_issuer.length &&
		!memcmp(certificate->_issuer.data, Apple_WWDR_CA_Subject_Name,
				sizeof(Apple_WWDR_CA_Subject_Name))) {
		const char *WWDR_OCSP_URI = "http://ocsp.apple.com/ocsp-wwdr01";
		url = CFURLCreateWithBytes(kCFAllocatorDefault,
				(const UInt8*)WWDR_OCSP_URI, strlen(WWDR_OCSP_URI),
				kCFStringEncodingASCII, NULL);
	}
	if (url) {
		CFMutableArrayRef *urls = &certificate->_ocspResponders;
		*urls = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(*urls, url);
		CFRelease(url);
	}
}

static bool SecCEPSubjectInfoAccess(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static bool SecCEPNetscapeCertType(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static bool SecCEPEntrustVersInfo(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static bool SecCEPEscrowMarker(SecCertificateRef certificate,
                               const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    return true;
}

static bool SecCEPOCSPNoCheck(SecCertificateRef certificate,
                              const SecCertificateExtension *extn) {
    secdebug("cert", "ocsp-nocheck critical: %s", extn->critical ? "yes" : "no");
    return true;
}

/* Dictionary key callback for comparing to DERItems. */
static Boolean SecDERItemEqual(const void *value1, const void *value2) {
	return DEROidCompare((const DERItem *)value1, (const DERItem *)value2);
}

/* Dictionary key callback calculating the hash of a DERItem. */
static CFHashCode SecDERItemHash(const void *value) {
	const DERItem *derItem = (const DERItem *)value;
	CFHashCode hash = derItem->length;
	DERSize ix = derItem->length > 8 ? derItem->length - 8 : 0;
	for (; ix < derItem->length; ++ix) {
		hash = (hash << 9) + (hash >> 23) + derItem->data[ix];
	}

	return hash;
}

/* Dictionary key callbacks using the above 2 functions. */
static const CFDictionaryKeyCallBacks SecDERItemKeyCallBacks = {
	0,					/* version */
	NULL,				/* retain */
	NULL,				/* release */
	NULL,				/* copyDescription */
	SecDERItemEqual,	/* equal */
	SecDERItemHash		/* hash */
};

static void SecCertificateInitializeExtensionParsers(void) {
	/* Build a dictionary that maps from extension OIDs to callback functions
     which can parse the extension of the type given. */
	static const void *extnOIDs[] = {
		&oidSubjectKeyIdentifier,
		&oidKeyUsage,
		&oidPrivateKeyUsagePeriod,
		&oidSubjectAltName,
		&oidIssuerAltName,
		&oidBasicConstraints,
		&oidNameConstraints,
		&oidCrlDistributionPoints,
		&oidCertificatePolicies,
		&oidPolicyMappings,
		&oidAuthorityKeyIdentifier,
		&oidPolicyConstraints,
		&oidExtendedKeyUsage,
		&oidInhibitAnyPolicy,
		&oidAuthorityInfoAccess,
		&oidSubjectInfoAccess,
		&oidNetscapeCertType,
		&oidEntrustVersInfo,
        &oidApplePolicyEscrowService,
        &oidOCSPNoCheck,
	};
	static const void *extnParsers[] = {
		SecCEPSubjectKeyIdentifier,
		SecCEPKeyUsage,
		SecCEPPrivateKeyUsagePeriod,
		SecCEPSubjectAltName,
		SecCEPIssuerAltName,
		SecCEPBasicConstraints,
		SecCEPNameConstraints,
		SecCEPCrlDistributionPoints,
		SecCEPCertificatePolicies,
		SecCEPPolicyMappings,
		SecCEPAuthorityKeyIdentifier,
		SecCEPPolicyConstraints,
		SecCEPExtendedKeyUsage,
        SecCEPInhibitAnyPolicy,
		SecCEPAuthorityInfoAccess,
		SecCEPSubjectInfoAccess,
		SecCEPNetscapeCertType,
		SecCEPEntrustVersInfo,
        SecCEPEscrowMarker,
        SecCEPOCSPNoCheck,
	};
	sExtensionParsers = CFDictionaryCreate(kCFAllocatorDefault, extnOIDs,
                                           extnParsers, array_size(extnOIDs),
                                           &SecDERItemKeyCallBacks, NULL);
}

CFGiblisWithFunctions(SecCertificate, NULL, NULL, SecCertificateDestroy, SecCertificateEqual, SecCertificateHash, NULL, SecCertificateCopyDescription, NULL, NULL, ^{
    SecCertificateInitializeExtensionParsers();
})

static bool isAppleExtensionOID(const DERItem *extnID)
{
    static const uint8_t appleExtensionArc[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x06 };
    static const uint8_t appleComponentExtensionArc[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x0b };
    static const uint8_t appleSigningExtensionArc[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x0c };
    static const uint8_t appleEncryptionExtensionArc[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x0d };
    static const uint8_t appleExternalEncryptionExtensionArc[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x0f };
    if (!extnID || !extnID->data || (extnID->length <= sizeof(appleExtensionArc))) {
        return false;
    }
    return (!memcmp(extnID->data, appleExtensionArc, sizeof(appleExtensionArc)) ||
            !memcmp(extnID->data, appleComponentExtensionArc, sizeof(appleComponentExtensionArc)) ||
            !memcmp(extnID->data, appleSigningExtensionArc, sizeof(appleSigningExtensionArc)) ||
            !memcmp(extnID->data, appleEncryptionExtensionArc, sizeof(appleEncryptionExtensionArc)) ||
            !memcmp(extnID->data, appleExternalEncryptionExtensionArc, sizeof(appleExternalEncryptionExtensionArc)));
}

static bool isCCCExtensionOID(const DERItem *extnID)
{
    static const uint8_t cccVehicleCA[] = { 0x2B,0x06,0x01,0x04,0x01,0x82,0xC4,0x69,0x05,0x09 };
    static const uint8_t cccIntermediateCA[] = { 0x2B,0x06,0x01,0x04,0x01,0x82,0xC4,0x69,0x05,0x08 };
    static const uint8_t cccVehicle[] = { 0x2B,0x06,0x01,0x04,0x01,0x82,0xC4,0x69,0x05,0x01 };
    if (!extnID || !extnID->data || (extnID->length != sizeof(cccVehicleCA))) {
        return false;
    }
    return (!memcmp(extnID->data, cccVehicleCA, sizeof(cccVehicleCA)) ||
            !memcmp(extnID->data, cccIntermediateCA, sizeof(cccIntermediateCA)) ||
            !memcmp(extnID->data, cccVehicle, sizeof(cccVehicle)));
}

/* Given the contents of an X.501 Name return the contents of a normalized
   X.501 name. */
CFDataRef createNormalizedX501Name(CFAllocatorRef allocator,
	const DERItem *x501name) {
    CFMutableDataRef result = CFDataCreateMutable(allocator, x501name->length);
    CFIndex length = x501name->length;
    CFDataSetLength(result, length);
    UInt8 *base = CFDataGetMutableBytePtr(result);

	DERSequence rdnSeq;
	DERReturn drtn = DERDecodeSeqContentInit(x501name, &rdnSeq);

	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo rdn;

    /* Always points to last rdn tag. */
    const DERByte *rdnTag = rdnSeq.nextItem;
    /* Offset relative to base of current rdn set tag. */
    CFIndex rdnTagLocation = 0;
	while ((drtn = DERDecodeSeqNext(&rdnSeq, &rdn)) == DR_Success) {
		require_quiet(rdn.tag == ASN1_CONSTR_SET, badDER);
		/* We don't allow empty RDNs. */
		require_quiet(rdn.content.length != 0, badDER);
        /* Length of the tag and length of the current rdn. */
        CFIndex rdnTLLength = rdn.content.data - rdnTag;
        CFIndex rdnContentLength = rdn.content.length;
        /* Copy the tag and length of the RDN. */
        memcpy(base + rdnTagLocation, rdnTag, rdnTLLength);

		DERSequence atvSeq;
		drtn = DERDecodeSeqContentInit(&rdn.content, &atvSeq);
        require_quiet(drtn == DR_Success, badDER);

        DERDecodedInfo atv;
        /* Always points to tag of current atv sequence. */
        const DERByte *atvTag = atvSeq.nextItem;
        /* Offset relative to base of current atv sequence tag. */
        CFIndex atvTagLocation = rdnTagLocation + rdnTLLength;
		while ((drtn = DERDecodeSeqNext(&atvSeq, &atv)) == DR_Success) {
			require_quiet(atv.tag == ASN1_CONSTR_SEQUENCE, badDER);
            /* Length of the tag and length of the current atv. */
            CFIndex atvTLLength = atv.content.data - atvTag;
            CFIndex atvContentLength = atv.content.length;
            /* Copy the tag and length of the atv and the atv itself. */
            memcpy(base + atvTagLocation, atvTag,
                atvTLLength + atv.content.length);

            /* Now decode the atv sequence. */
			DERAttributeTypeAndValue atvPair;
			drtn = DERParseSequenceContent(&atv.content,
				DERNumAttributeTypeAndValueItemSpecs,
				DERAttributeTypeAndValueItemSpecs,
				&atvPair, sizeof(atvPair));
			require_noerr_quiet(drtn, badDER);
			require_quiet(atvPair.type.length != 0, badDER);
            DERDecodedInfo value;
            drtn = DERDecodeItem(&atvPair.value, &value);
			require_noerr_quiet(drtn, badDER);

            /* (c) attribute values in PrintableString are not case sensitive
               (e.g., "Marianne Swanson" is the same as "MARIANNE SWANSON"); and

               (d) attribute values in PrintableString are compared after
               removing leading and trailing white space and converting internal
               substrings of one or more consecutive white space characters to a
               single space. */
            if (value.tag == ASN1_PRINTABLE_STRING) {
                /* Offset relative to base of current value tag. */
                CFIndex valueTagLocation = atvTagLocation + atvPair.value.data - atvTag;
                CFIndex valueTLLength = value.content.data - atvPair.value.data;
                CFIndex valueContentLength = value.content.length;

                /* Now copy all the bytes, but convert to upper case while
                   doing so and convert multiple whitespace chars into a
                   single space. */
                bool lastWasBlank = false;
                CFIndex valueLocation = valueTagLocation + valueTLLength;
                CFIndex valueCurrentLocation = valueLocation;
                CFIndex ix;
                for (ix = 0; ix < valueContentLength; ++ix) {
                    UInt8 ch = value.content.data[ix];
                    if (isblank(ch)) {
                        if (lastWasBlank) {
                            continue;
                        } else {
                            /* Don't insert a space for first character
                               we encounter. */
                            if (valueCurrentLocation > valueLocation) {
                                base[valueCurrentLocation++] = ' ';
                            }
                            lastWasBlank = true;
                        }
                    } else {
                        lastWasBlank = false;
                        if ('a' <= ch && ch <= 'z') {
                            base[valueCurrentLocation++] = ch + 'A' - 'a';
                        } else {
                            base[valueCurrentLocation++] = ch;
                        }
                    }
                }
                /* Finally if lastWasBlank remove the trailing space. */
                if (lastWasBlank && valueCurrentLocation > valueLocation) {
                    valueCurrentLocation--;
                }
                /* Adjust content length to normalized length. */
                valueContentLength = valueCurrentLocation - valueLocation;

                /* Number of bytes by which the length should be shorted. */
                CFIndex lengthDiff = value.content.length - valueContentLength;
                if (lengthDiff == 0) {
                    /* Easy case no need to adjust lengths. */
                } else {
                    /* Hard work we need to go back and fix up length fields
                       for:
                           1) The value itself.
                           2) The ATV Sequence containing type/value
                           3) The RDN Set containing one or more atv pairs.
                           4) The result.
                       */

                    /* Step 1 fix up length of value. */
                    /* Length of value tag and length minus the tag. */
                    DERSize newValueTLLength = valueTLLength - 1;
                    drtn = DEREncodeLength(valueContentLength,
                        base + valueTagLocation + 1, &newValueTLLength);
                    require(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newValueTLLength++;
                    CFIndex valueLLDiff = valueTLLength - newValueTLLength;
                    if (valueLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + valueTagLocation + newValueTLLength,
                            base + valueTagLocation + valueTLLength,
                            valueContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += valueLLDiff;
                    }

                    /* Step 2 fix up length of the enclosing ATV Sequence. */
                    atvContentLength -= lengthDiff;
                    DERSize newATVTLLength = atvTLLength - 1;
                    drtn = DEREncodeLength(atvContentLength,
                        base + atvTagLocation + 1, &newATVTLLength);
                    require(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newATVTLLength++;
                    CFIndex atvLLDiff = atvTLLength - newATVTLLength;
                    if (atvLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + atvTagLocation + newATVTLLength,
                            base + atvTagLocation + atvTLLength,
                            atvContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += atvLLDiff;
                        atvTLLength = newATVTLLength;
                    }

                    /* Step 3 fix up length of enclosing RDN Set. */
                    rdnContentLength -= lengthDiff;
                    DERSize newRDNTLLength = rdnTLLength - 1;
                    drtn = DEREncodeLength(rdnContentLength,
                        base + rdnTagLocation + 1, &newRDNTLLength);
                    require_quiet(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newRDNTLLength++;
                    CFIndex rdnLLDiff = rdnTLLength - newRDNTLLength;
                    if (rdnLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + rdnTagLocation + newRDNTLLength,
                            base + rdnTagLocation + rdnTLLength,
                            rdnContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += rdnLLDiff;
                        rdnTLLength = newRDNTLLength;

                        /* Adjust the locations that might have changed due to
                           this slide. */
                        atvTagLocation -= rdnLLDiff;
                    }
                    (void) lengthDiff; // No next object, silence analyzer
                }
            }
            atvTagLocation += atvTLLength + atvContentLength;
            atvTag = atvSeq.nextItem;
		}
        require_quiet(drtn == DR_EndOfSequence, badDER);
        rdnTagLocation += rdnTLLength + rdnContentLength;
        rdnTag = rdnSeq.nextItem;
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);
    /* Truncate the result to the proper length. */
    CFDataSetLength(result, rdnTagLocation);

	return result;

badDER:
    CFRelease(result);
    return NULL;
}

static CFDataRef SecDERItemCopySequence(DERItem *content) {
    DERSize seq_len_length = DERLengthOfLength(content->length);
    size_t sequence_length = 1 + seq_len_length + content->length;
    CFMutableDataRef sequence = CFDataCreateMutable(kCFAllocatorDefault,
                                                    sequence_length);
    CFDataSetLength(sequence, sequence_length);
    uint8_t *sequence_ptr = CFDataGetMutableBytePtr(sequence);
    *sequence_ptr++ = ONE_BYTE_ASN1_CONSTR_SEQUENCE;
    require_noerr_quiet(DEREncodeLength(content->length,
                                        sequence_ptr, &seq_len_length), out);
    sequence_ptr += seq_len_length;
    memcpy(sequence_ptr, content->data, content->length);
    return sequence;
out:
    CFReleaseSafe(sequence);
    return NULL;
}

static CFDataRef SecCopySequenceFromContent(CFDataRef content) {
    DERItem tmpItem;
    tmpItem.data = (void *)CFDataGetBytePtr(content);
    tmpItem.length = CFDataGetLength(content);

    return SecDERItemCopySequence(&tmpItem);
}

CFDataRef SecDistinguishedNameCopyNormalizedContent(CFDataRef distinguished_name)
{
    const DERItem name = { (unsigned char *)CFDataGetBytePtr(distinguished_name), CFDataGetLength(distinguished_name) };
    DERDecodedInfo content;
    /* Decode top level sequence into DERItem */
    if (!DERDecodeItem(&name, &content) && (content.tag == ASN1_CONSTR_SEQUENCE))
        return createNormalizedX501Name(kCFAllocatorDefault, &content.content);
    return NULL;
}

CFDataRef SecDistinguishedNameCopyNormalizedSequence(CFDataRef distinguished_name)
{
    if (!distinguished_name) { return NULL; }
    CFDataRef normalizedContent = SecDistinguishedNameCopyNormalizedContent(distinguished_name);
    if (!normalizedContent) { return NULL; }
    CFDataRef result = SecCopySequenceFromContent(normalizedContent);
    CFReleaseNull(normalizedContent);
    return result;
}

/* AUDIT[securityd]:
   certificate->_der is a caller provided data of any length (might be 0).

   Top level certificate decode.
 */
static bool SecCertificateParse(SecCertificateRef certificate)
{
	DERReturn drtn;

    check(certificate);
    require_quiet(certificate, badCert);
    CFAllocatorRef allocator = CFGetAllocator(certificate);

	/* top level decode */
	DERSignedCertCrl signedCert;
	drtn = DERParseSequence(&certificate->_der, DERNumSignedCertCrlItemSpecs,
		DERSignedCertCrlItemSpecs, &signedCert,
		sizeof(signedCert));
	require_noerr_quiet(drtn, badCert);
	/* Store tbs since we need to digest it for verification later on. */
	certificate->_tbs = signedCert.tbs;

	/* decode the TBSCert - it was saved in full DER form */
    DERTBSCert tbsCert;
	drtn = DERParseSequence(&signedCert.tbs,
		DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
		&tbsCert, sizeof(tbsCert));
	require_noerr_quiet(drtn, badCert);

	/* sequence we're given: decode the signedCerts Signature Algorithm. */
	/* This MUST be the same as the certificate->_tbsSigAlg with the exception
	   of the params field. */
	drtn = DERParseSequenceContent(&signedCert.sigAlg,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_sigAlg, sizeof(certificate->_sigAlg));
	require_noerr_quiet(drtn, badCert);

	/* The contents of signedCert.sig is a bit string whose contents
	   are the signature itself. */
    DERByte numUnusedBits;
	drtn = DERParseBitString(&signedCert.sig,
        &certificate->_signature, &numUnusedBits);
	require_noerr_quiet(drtn, badCert);

    /* Now decode the tbsCert. */

    /* First we turn the optional version into an int. */
    if (tbsCert.version.length) {
        DERDecodedInfo decoded;
        drtn = DERDecodeItem(&tbsCert.version, &decoded);
        require_noerr_quiet(drtn, badCert);
        require_quiet(decoded.tag == ASN1_INTEGER, badCert);
        require_quiet(decoded.content.length == 1, badCert);
        certificate->_version = decoded.content.data[0];
        if (certificate->_version > 2) {
            secwarning("Invalid certificate version (%d), must be 0..2",
                certificate->_version);
        }
        require_quiet(certificate->_version > 0, badCert);
        require_quiet(certificate->_version < 3, badCert);
    } else {
        certificate->_version = 0;
    }

    /* The serial number is in the tbsCert.serialNum - it was saved in
       INTEGER form without the tag and length. */
    certificate->_serialNum = tbsCert.serialNum;

    /* Note: RFC5280 4.1.2.2 limits serial number values to 20 octets.
       For now, we warn about larger values, but will still create the
       certificate with values up to 36 octets to avoid breaking some
       nonconforming certs with slightly longer serial numbers.
       We also explicitly allow serial numbers of 21 octets where the
       leading byte is 0x00 which is used to make a negative 20 octet
       value positive.
    */
    if (tbsCert.serialNum.length < 1 || tbsCert.serialNum.length > 21 ||
        (tbsCert.serialNum.length == 21 && tbsCert.serialNum.data[0] != 0x00)) {
        secwarning("Invalid serial number length (%ld), must be 1..20",
            tbsCert.serialNum.length);
    }
    require_quiet(tbsCert.serialNum.data != NULL &&
                  tbsCert.serialNum.length >= 1 &&
                  tbsCert.serialNum.length <= 37, badCert);
    certificate->_serialNumber = CFDataCreate(allocator,
        tbsCert.serialNum.data, tbsCert.serialNum.length);

	/* sequence we're given: decode the tbsCerts TBS Signature Algorithm. */
	drtn = DERParseSequenceContent(&tbsCert.tbsSigAlg,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_tbsSigAlg, sizeof(certificate->_tbsSigAlg));
	require_noerr_quiet(drtn, badCert);

	/* The issuer is in the tbsCert.issuer - it's a sequence without the tag
       and length fields. */
	certificate->_issuer = tbsCert.issuer;
    certificate->_normalizedIssuer = createNormalizedX501Name(allocator,
        &tbsCert.issuer);

	/* sequence we're given: decode the tbsCerts Validity sequence. */
    DERValidity validity;
	drtn = DERParseSequenceContent(&tbsCert.validity,
		DERNumValidityItemSpecs, DERValidityItemSpecs,
		&validity, sizeof(validity));
	require_noerr_quiet(drtn, badCert);
    require_quiet(derDateGetAbsoluteTime(&validity.notBefore,
        &certificate->_notBefore), badCert);
    require_quiet(derDateGetAbsoluteTime(&validity.notAfter,
        &certificate->_notAfter), badCert);

	/* The subject is in the tbsCert.subject - it's a sequence without the tag
       and length fields. */
	certificate->_subject = tbsCert.subject;
    certificate->_normalizedSubject = createNormalizedX501Name(allocator,
        &tbsCert.subject);

    /* Keep the SPKI around for CT */
    certificate->_subjectPublicKeyInfo = tbsCert.subjectPubKey;

	/* sequence we're given: encoded DERSubjPubKeyInfo */
	DERSubjPubKeyInfo pubKeyInfo;
	drtn = DERParseSequenceContent(&tbsCert.subjectPubKey,
		DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
		&pubKeyInfo, sizeof(pubKeyInfo));
	require_noerr_quiet(drtn, badCert);

	/* sequence we're given: decode the pubKeyInfos DERAlgorithmId */
	drtn = DERParseSequenceContent(&pubKeyInfo.algId,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_algId, sizeof(certificate->_algId));
	require_noerr_quiet(drtn, badCert);

	/* Now we can figure out the key's algorithm id and params based on
	   certificate->_algId.oid. */

	/* The contents of pubKeyInfo.pubKey is a bit string whose contents
	   are a PKCS1 format RSA key. */
	drtn = DERParseBitString(&pubKeyInfo.pubKey,
        &certificate->_pubKeyDER, &numUnusedBits);
	require_noerr_quiet(drtn, badCert);

	/* The contents of tbsCert.issuerID is a bit string. */
	certificate->_issuerUniqueID = tbsCert.issuerID;

	/* The contents of tbsCert.subjectID is a bit string. */
	certificate->_subjectUniqueID = tbsCert.subjectID;

	/* Extensions. */
    certificate->_unparseableKnownExtensionIndex = kCFNotFound;
    if (tbsCert.extensions.length) {
        CFIndex extensionCount = 0;
        DERSequence derSeq;
        DERTag tag;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag, &derSeq);
        require_noerr_quiet(drtn, badCert);
        require_quiet(tag == ASN1_CONSTR_SEQUENCE, badCert);
        DERDecodedInfo currDecoded;
        while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
#if 0
    /* ! = MUST recognize ? = SHOULD recognize
     */

    KnownExtension      _subjectKeyID;          /* ?SubjectKeyIdentifier     id-ce 14 */
    KnownExtension      _keyUsage;              /* !KeyUsage                 id-ce 15 */
    KnownExtension      _subjectAltName;        /* !SubjectAltName           id-ce 17 */
    KnownExtension      _basicConstraints;      /* !BasicConstraints         id-ce 19 */
    KnownExtension      _authorityKeyID;        /* ?AuthorityKeyIdentifier   id-ce 35 */
    KnownExtension      _extKeyUsage;           /* !ExtKeyUsage              id-ce 37 */
    KnownExtension      _netscapeCertType;      /* 2.16.840.1.113730.1.1 netscape 1 1 */
    KnownExtension      _qualCertStatements;    /* QCStatements             id-pe 3 */

    KnownExtension      _issuerAltName;         /* IssuerAltName            id-ce 18 */
    KnownExtension      _nameConstraints;       /* !NameConstraints          id-ce 30 */
    KnownExtension      _cRLDistributionPoints; /* CRLDistributionPoints    id-ce 31 */
    KnownExtension      _certificatePolicies;   /* !CertificatePolicies      id-ce 32 */
    KnownExtension      _policyMappings;        /* ?PolicyMappings           id-ce 33 */
    KnownExtension      _policyConstraints;     /* !PolicyConstraints        id-ce 36 */
    KnownExtension      _freshestCRL;           /* FreshestCRL              id-ce 46 */
    KnownExtension      _inhibitAnyPolicy;      /* !InhibitAnyPolicy         id-ce 54 */

    KnownExtension      _authorityInfoAccess;   /* AuthorityInfoAccess      id-pe 1 */
    KnownExtension      _subjectInfoAccess;     /* SubjectInfoAccess        id-pe 11 */
#endif

            extensionCount++;
        }
        require_quiet(drtn == DR_EndOfSequence, badCert);
        require_quiet(extensionCount > 0, badCert); // Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension

        /* Put some upper limit on the number of extensions allowed. */
        require_quiet(extensionCount < MAX_EXTENSIONS, badCert);
        certificate->_extensionCount = extensionCount;
        certificate->_extensions = malloc(sizeof(SecCertificateExtension) * (extensionCount > 0 ? extensionCount : 1));
        require_quiet(certificate->_extensions, badCert);

        CFIndex ix = 0;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag, &derSeq);
        require_noerr_quiet(drtn, badCert);
        for (ix = 0; ix < extensionCount; ++ix) {
            drtn = DERDecodeSeqNext(&derSeq, &currDecoded);
            require_quiet(drtn == DR_Success || (ix == extensionCount - 1 && drtn == DR_EndOfSequence), badCert);
            require_quiet(currDecoded.tag == ASN1_CONSTR_SEQUENCE, badCert);
            DERExtension extn;
            drtn = DERParseSequenceContent(&currDecoded.content,
                                           DERNumExtensionItemSpecs, DERExtensionItemSpecs,
                                           &extn, sizeof(extn));
            require_noerr_quiet(drtn, badCert);
            /* Copy stuff into certificate->extensions[ix]. */
            certificate->_extensions[ix].extnID = extn.extnID;
            require_noerr_quiet(drtn = DERParseBooleanWithDefault(&extn.critical, false,
                                                                  &certificate->_extensions[ix].critical), badCert);
            certificate->_extensions[ix].extnValue = extn.extnValue;

            SecCertificateExtensionParser parser =
            (SecCertificateExtensionParser)CFDictionaryGetValue(sExtensionParsers, &certificate->_extensions[ix].extnID);
            if (parser) {
                /* Invoke the parser. If the extension is critical and the
                 * parser fails, fail the cert. */
                bool parseResult = parser(certificate, &certificate->_extensions[ix]);
                if (!parseResult) {
                    certificate->_unparseableKnownExtensionIndex = ix;
                }
                require_quiet(parseResult || !certificate->_extensions[ix].critical, badCert);
            } else if (certificate->_extensions[ix].critical) {
                if (isAppleExtensionOID(&extn.extnID) || isCCCExtensionOID(&extn.extnID)) {
                    continue;
                }
                secdebug("cert", "Found unknown critical extension");
                certificate->_foundUnknownCriticalExtension = true;
            } else {
                secdebug("cert", "Found unknown non critical extension");
            }
        }
    }
    checkForMissingRevocationInfo(certificate);

	return true;

badCert:
	return false;
}


/* Public API functions. */
SecCertificateRef SecCertificateCreateWithBytes(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
	if (der_bytes == NULL) return NULL;
    if (der_length == 0) return NULL;

    CFIndex size = sizeof(struct __SecCertificate) + der_length;
    SecCertificateRef result = (SecCertificateRef)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0,
			sizeof(*result) - sizeof(result->_base));
		result->_der.data = ((DERByte *)result + sizeof(*result));
		result->_der.length = der_length;
		memcpy(result->_der.data, der_bytes, der_length);
		if (!SecCertificateParse(result)) {
			CFRelease(result);
			return NULL;
		}
    }
    return result;
}

/* @@@ Placeholder until <rdar://problem/5701851> iap submits a binary is fixed. */
SecCertificateRef SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length);

SecCertificateRef SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
    return SecCertificateCreateWithBytes(allocator, der_bytes, der_length);
}
/* @@@ End of placeholder. */

/* AUDIT[securityd](done):
   der_certificate is a caller provided data of any length (might be 0), only
   its cf type has been checked.
 */
SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator,
	CFDataRef der_certificate) {
	if (!der_certificate) {
		return NULL;
	}
	CFIndex size = sizeof(struct __SecCertificate);
	SecCertificateRef result = (SecCertificateRef)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0, size - sizeof(result->_base));
		result->_der_data = CFDataCreateCopy(allocator, der_certificate);
		result->_der.data = (DERByte *)CFDataGetBytePtr(result->_der_data);
		result->_der.length = CFDataGetLength(result->_der_data);
		if (!SecCertificateParse(result)) {
			CFRelease(result);
			return NULL;
		}
	}
	return result;
}

SecCertificateRef SecCertificateCreateWithKeychainItem(CFAllocatorRef allocator,
	CFDataRef der_certificate,
	CFTypeRef keychain_item)
{
	SecCertificateRef result = SecCertificateCreateWithData(allocator, der_certificate);
	if (result) {
		CFRetainSafe(keychain_item);
		result->_keychain_item = keychain_item;
	}
	return result;
}

OSStatus SecCertificateSetKeychainItem(SecCertificateRef certificate,
	CFTypeRef keychain_item)
{
	if (!certificate) {
		return errSecParam;
	}
	CFRetainSafe(keychain_item);
	CFReleaseSafe(certificate->_keychain_item);
	certificate->_keychain_item = keychain_item;
	return errSecSuccess;
}

CFDataRef SecCertificateCopyData(SecCertificateRef certificate) {
	check(certificate);
	CFDataRef result = NULL;
	if (!certificate) {
		return result;
	}
	if (certificate->_der_data) {
        CFRetain(certificate->_der_data);
        result = certificate->_der_data;
    } else {
		result = CFDataCreate(CFGetAllocator(certificate),
            certificate->_der.data, certificate->_der.length);
#if 0
		/* FIXME: If we wish to cache result we need to lock the certificate.
           Also this create 2 copies of the certificate data which is somewhat
           suboptimal. */
        CFRetain(result);
        certificate->_der_data = result;
#endif
	}

	return result;
}

CFIndex SecCertificateGetLength(SecCertificateRef certificate) {
	return certificate->_der.length;
}

const UInt8 *SecCertificateGetBytePtr(SecCertificateRef certificate) {
	return certificate->_der.data;
}

static bool SecCertificateIsCertificate(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
#ifndef IS_TRUSTTESTS
    /* TrustTests registers two SecCertificate TypeIDs, so we'll skip this check
     * in the tests and just let the tests crash if they pass the wrong object type. */
    if (CFGetTypeID(certificate) != SecCertificateGetTypeID()) {
        return false;
    }
#endif
    return true;
}

/* Used to recreate preCert from cert for Certificate Transparency */
CFDataRef SecCertificateCopyPrecertTBS(SecCertificateRef certificate)
{
    CFDataRef outData = NULL;
    DERItem tbsIn = certificate->_tbs;
    DERItem tbsOut = {0,};
    DERItem extensionsOut = {0,};
    DERItem *extensionsList = malloc(sizeof(DERItem)*certificate->_extensionCount); /* This maybe one too many */
    DERItemSpec *extensionsListSpecs = malloc(sizeof(DERItemSpec)*certificate->_extensionCount);
    DERTBSCert tbsCert;
    DERReturn drtn;

    require_quiet(extensionsList && extensionsListSpecs, out);

    /* decode the TBSCert - it was saved in full DER form */
    drtn = DERParseSequence(&tbsIn,
                            DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
                            &tbsCert, sizeof(tbsCert));
    require_noerr_quiet(drtn, out);

    /* Go over extensions and filter any SCT extension */
    CFIndex extensionsCount = 0;

    if (tbsCert.extensions.length) {
        DERSequence derSeq;
        DERTag tag;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag,
                                &derSeq);
        require_noerr_quiet(drtn, out);
        require_quiet(tag == ASN1_CONSTR_SEQUENCE, out);
        DERDecodedInfo currDecoded;
        while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {

            require_quiet(currDecoded.tag == ASN1_CONSTR_SEQUENCE, out);
            DERExtension extn;
            drtn = DERParseSequenceContent(&currDecoded.content,
                                           DERNumExtensionItemSpecs, DERExtensionItemSpecs,
                                           &extn, sizeof(extn));
            require_noerr_quiet(drtn, out);

            if (extn.extnID.length == oidGoogleEmbeddedSignedCertificateTimestamp.length &&
                !memcmp(extn.extnID.data, oidGoogleEmbeddedSignedCertificateTimestamp.data, extn.extnID.length))
                continue;

            extensionsList[extensionsCount] = currDecoded.content;
            extensionsListSpecs[extensionsCount].offset = sizeof(DERItem)*extensionsCount;
            extensionsListSpecs[extensionsCount].options = 0;
            extensionsListSpecs[extensionsCount].tag = ASN1_CONSTR_SEQUENCE;

            extensionsCount++;
        }

        require_quiet(drtn == DR_EndOfSequence, out);

    }

    /* Encode extensions */
    extensionsOut.length = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, extensionsList, extensionsCount, extensionsListSpecs);
    extensionsOut.data = malloc(extensionsOut.length);
    require_quiet(extensionsOut.data, out);
    drtn = DEREncodeSequence(ASN1_CONSTR_SEQUENCE, extensionsList, extensionsCount, extensionsListSpecs, extensionsOut.data, &extensionsOut.length);
    require_noerr_quiet(drtn, out);

    tbsCert.extensions = extensionsOut;

    tbsOut.length = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, &tbsCert, DERNumTBSCertItemSpecs, DERTBSCertItemSpecs);
    tbsOut.data = malloc(tbsOut.length);
    require_quiet(tbsOut.data, out);
    drtn = DEREncodeSequence(ASN1_CONSTR_SEQUENCE, &tbsCert, DERNumTBSCertItemSpecs, DERTBSCertItemSpecs, tbsOut.data, &tbsOut.length);
    require_noerr_quiet(drtn, out);

    outData = CFDataCreate(kCFAllocatorDefault, tbsOut.data, tbsOut.length);

out:
    if (extensionsOut.data) free(extensionsOut.data);
    if (tbsOut.data) free(tbsOut.data);
    if (extensionsList) free(extensionsList);
    if (extensionsListSpecs) free(extensionsListSpecs);
    return outData;

}

/* From rfc3280 - Appendix B.  ASN.1 Notes

   Object Identifiers (OIDs) are used throughout this specification to
   identify certificate policies, public key and signature algorithms,
   certificate extensions, etc.  There is no maximum size for OIDs.
   This specification mandates support for OIDs which have arc elements
   with values that are less than 2^28, that is, they MUST be between 0
   and 268,435,455, inclusive.  This allows each arc element to be
   represented within a single 32 bit word.  Implementations MUST also
   support OIDs where the length of the dotted decimal (see [RFC 2252],
   section 4.1) string representation can be up to 100 bytes
   (inclusive).  Implementations MUST be able to handle OIDs with up to
   20 elements (inclusive).  CAs SHOULD NOT issue certificates which
   contain OIDs that exceed these requirements.  Likewise, CRL issuers
   SHOULD NOT issue CRLs which contain OIDs that exceed these
   requirements.
*/

/* Oids longer than this are considered invalid. */
#define MAX_OID_SIZE				32

CFStringRef SecDERItemCopyOIDDecimalRepresentation(CFAllocatorRef allocator,
    const DERItem *oid) {

	if (oid->length == 0) {
        return SecCopyCertString(SEC_NULL_KEY);
    }
	if (oid->length > MAX_OID_SIZE) {
        return SecCopyCertString(SEC_OID_TOO_LONG_KEY);
    }

    CFMutableStringRef result = CFStringCreateMutable(allocator, 0);

	// The first two levels are encoded into one byte, since the root level
	// has only 3 nodes (40*x + y).  However if x = joint-iso-itu-t(2) then
	// y may be > 39, so we have to add special-case handling for this.
	uint32_t x = oid->data[0] / 40;
	uint32_t y = oid->data[0] % 40;
	if (x > 2)
	{
		// Handle special case for large y if x = 2
		y += (x - 2) * 40;
		x = 2;
	}
    CFStringAppendFormat(result, NULL, CFSTR("%u.%u"), x, y);

	uint32_t value = 0;
	for (x = 1; x < oid->length; ++x)
	{
		value = (value << 7) | (oid->data[x] & 0x7F);
        /* @@@ value may not span more than 4 bytes. */
        /* A max number of 20 values is allowed. */
		if (!(oid->data[x] & 0x80))
		{
            CFStringAppendFormat(result, NULL, CFSTR(".%" PRIu32), value);
			value = 0;
		}
	}
	return result;
}

static CFStringRef copyOidDescription(CFAllocatorRef allocator,
    const DERItem *oid, bool localized) {
	if (!oid || oid->length == 0) {
        return (localized) ? SecCopyCertString(SEC_NULL_KEY) : SEC_NULL_KEY;
    }

    CFStringRef name = SecDERItemCopyOIDDecimalRepresentation(allocator, oid);
    if (!localized) {
        return name;
    }

    /* Build the key we use to lookup the localized OID description. */
    CFMutableStringRef oidKey = CFStringCreateMutable(allocator,
        oid->length * 3 + 5);
    CFStringAppendFormat(oidKey, NULL, CFSTR("06 %02lX"), oid->length);
    for (DERSize ix = 0; ix < oid->length; ++ix) {
        CFStringAppendFormat(oidKey, NULL, CFSTR(" %02X"), oid->data[ix]);
    }
    CFStringRef locname = SecFrameworkCopyLocalizedString(oidKey, CFSTR("OID"));
    if (locname && !CFEqual(oidKey, locname)) {
        /* Found localized description string, so use it instead of OID. */
        CFReleaseSafe(name);
        name = locname;
    } else {
        CFReleaseSafe(locname);
    }
    CFRelease(oidKey);

    return name;
}

/* Return the ipAddress as a dotted quad for ipv4, or as 8 colon separated
   4 digit hex strings for ipv6.  Return NULL if the provided IP doesn't
   have a length of exactly 4 or 16 octets.
   Note: hex values are normalized to uppercase.
*/
static CFStringRef copyIPAddressContentDescription(CFAllocatorRef allocator,
	const DERItem *ip) {
    /* This is the IP Address as an OCTET STRING.
       For IPv4 it's 4 octets addr, or 8 octets, addr/mask.
       For IPv6 it's 16 octets addr, or 32 octets addr/mask.
    */
	CFStringRef value = NULL;
	if (ip->length == IPv4ADDRLEN) {
		value = CFStringCreateWithFormat(allocator, NULL,
			CFSTR("%u.%u.%u.%u"),
			ip->data[0], ip->data[1], ip->data[2], ip->data[3]);
	} else if (ip->length == IPv6ADDRLEN) {
		value = CFStringCreateWithFormat(allocator, NULL,
			CFSTR("%02X%02X:%02X%02X:%02X%02X:%02X%02X:"
			"%02X%02X:%02X%02X:%02X%02X:%02X%02X"),
			ip->data[0], ip->data[1], ip->data[2], ip->data[3],
			ip->data[4], ip->data[5], ip->data[6], ip->data[7],
			ip->data[8], ip->data[9], ip->data[10], ip->data[11],
			ip->data[12], ip->data[13], ip->data[14], ip->data[15]);
	}

	return value;
}

void appendProperty(CFMutableArrayRef properties, CFStringRef propertyType,
    CFStringRef label, CFStringRef localizedLabel, CFTypeRef value,
    bool localized) {
    CFDictionaryRef property;
    if (label) {
        CFStringRef ll = NULL;
        if (!localized) {
            /* use unlocalized label, overriding localizedLabel */
            ll = localizedLabel = (CFStringRef) CFRetainSafe(label);
        } else if (!localizedLabel) {
            /* copy localized label for unlocalized label */
            ll = localizedLabel = SecCopyCertString(label);
        }
        const void *all_keys[4];
        all_keys[0] = kSecPropertyKeyType;
        all_keys[1] = kSecPropertyKeyLabel;
        all_keys[2] = kSecPropertyKeyLocalizedLabel;
        all_keys[3] = kSecPropertyKeyValue;
        const void *property_values[] = {
            propertyType,
            label,
            localizedLabel,
            value,
        };
        property = CFDictionaryCreate(CFGetAllocator(properties),
            all_keys, property_values, value ? 4 : 3,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFReleaseSafe(ll);
    } else {
        const void *nolabel_keys[2];
        nolabel_keys[0] = kSecPropertyKeyType;
        nolabel_keys[1] = kSecPropertyKeyValue;
        const void *property_values[] = {
            propertyType,
            value,
        };
        property = CFDictionaryCreate(CFGetAllocator(properties),
            nolabel_keys, property_values, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    CFArrayAppendValue(properties, property);
    CFRelease(property);
}

/* YYMMDDhhmmZ */
#define UTC_TIME_NOSEC_ZULU_LEN			11
/* YYMMDDhhmmssZ */
#define UTC_TIME_ZULU_LEN				13
/* YYMMDDhhmmssThhmm */
#define UTC_TIME_LOCALIZED_LEN			17
/* YYYYMMDDhhmmssZ */
#define GENERALIZED_TIME_ZULU_LEN		15
/* YYYYMMDDhhmmssThhmm */
#define GENERALIZED_TIME_LOCALIZED_LEN	19

/* Parse 2 digits at (*p)[0] and (*p)[1] and return the result.  Also
   advance *p by 2. */
static inline int parseDecimalPair(const DERByte **p) {
    const DERByte *cp = *p;
    *p += 2;
    return 10 * (cp[0] - '0') + cp[1] - '0';
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime.
   Return a CFErrorRef in the error parameter if decoding fails.
   Note that this is needed to distinguish an error condition from a
   valid time which specifies 2001-01-01 00:00:00 (i.e. a value of 0).
*/
CFAbsoluteTime SecAbsoluteTimeFromDateContentWithError(DERTag tag,
	const uint8_t *bytes,
	size_t length,
	CFErrorRef *error) {
	if (error) {
		*error = NULL;
	}
	if (NULL == bytes || 0 == length) {
		goto decodeErr;
	}

	bool isUtcLength = false;
	bool isLocalized = false;
	bool noSeconds = false;
	switch (length) {
		case UTC_TIME_NOSEC_ZULU_LEN:       /* YYMMDDhhmmZ */
			isUtcLength = true;
			noSeconds = true;
			break;
		case UTC_TIME_ZULU_LEN:             /* YYMMDDhhmmssZ */
			isUtcLength = true;
			break;
		case GENERALIZED_TIME_ZULU_LEN:     /* YYYYMMDDhhmmssZ */
			break;
		case UTC_TIME_LOCALIZED_LEN:        /* YYMMDDhhmmssThhmm (where T=[+,-]) */
			isUtcLength = true;
			/*DROPTHROUGH*/
		case GENERALIZED_TIME_LOCALIZED_LEN:/* YYYYMMDDhhmmssThhmm (where T=[+,-]) */
			isLocalized = true;
			break;
		default:                            /* unknown format */
			goto decodeErr;
	}

	/* Make sure the der tag fits the thing inside it. */
	if (tag == ASN1_UTC_TIME) {
		if (!isUtcLength) {
			goto decodeErr;
		}
	} else if (tag == ASN1_GENERALIZED_TIME) {
		if (isUtcLength) {
			goto decodeErr;
		}
	} else {
		goto decodeErr;
	}

	const DERByte *cp = bytes;
	/* Check that all characters are digits, except if localized the timezone
	   indicator or if not localized the 'Z' at the end.  */
	DERSize ix;
	for (ix = 0; ix < length; ++ix) {
		if (!(isdigit(cp[ix]))) {
			if ((isLocalized && ix == length - 5 &&
				 (cp[ix] == '+' || cp[ix] == '-')) ||
				(!isLocalized && ix == length - 1 && cp[ix] == 'Z')) {
				continue;
			}
			goto decodeErr;
		}
	}

	/* Parse the date and time fields. */
	int year, month, day, hour, minute, second;
	if (isUtcLength) {
		year = parseDecimalPair(&cp);
		if (year < 50) {
			/* 0  <= year <  50 : assume century 21 */
			year += 2000;
		} else if (year < 70) {
			/* 50 <= year <  70 : illegal per PKIX */
			return false;
		} else {
			/* 70 <  year <= 99 : assume century 20 */
			year += 1900;
		}
	} else {
		year = 100 * parseDecimalPair(&cp) + parseDecimalPair(&cp);
	}
	month = parseDecimalPair(&cp);
	day = parseDecimalPair(&cp);
	hour = parseDecimalPair(&cp);
	minute = parseDecimalPair(&cp);
	if (noSeconds) {
		second = 0;
	} else {
		second = parseDecimalPair(&cp);
	}

	CFTimeInterval timeZoneOffset;
	if (isLocalized) {
		/* ZONE INDICATOR */
        int multiplier = *cp++ == '+' ? 60 : -60;
        timeZoneOffset = multiplier *
            (parseDecimalPair(&cp) * 60 + parseDecimalPair(&cp));
	} else {
		timeZoneOffset = 0;
	}

    secdebug("dateparse",
        "date %.*s year: %04d%02d%02d%02d%02d%02d%+05g",
        (int) length, bytes, year, month,
        day, hour, minute, second,
        timeZoneOffset / 60);

    static int mdays[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    int is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 1 : 0;
    /* Some basic checks on the date, allowing leap seconds */
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60
        || (month == 2 && day > mdays[month] - mdays[month - 1] + is_leap_year)
        || (month != 2 && day > mdays[month] - mdays[month - 1])) {
        /* Invalid date. */
        goto decodeErr;
    }

    int dy = year - 2001;
    if (dy < 0) {
        dy += 1;
        day -= 1;
    }
    int leap_days = dy / 4 - dy / 100 + dy / 400;
    day += ((year - 2001) * 365 + leap_days) + mdays[month - 1] - 1;
    if (month > 2)
        day += is_leap_year;

    CFAbsoluteTime absTime = (CFAbsoluteTime)((day * 24.0 + hour) * 60.0 + minute) * 60.0 + second;
    return absTime - timeZoneOffset;

decodeErr:
    if (error) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
    }
    return NULL_TIME;
}

CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
    size_t length) {
    return SecAbsoluteTimeFromDateContentWithError(tag, bytes, length, NULL);
}

__attribute__((__nonnull__)) static bool derDateContentGetAbsoluteTime(DERTag tag, const DERItem *date,
    CFAbsoluteTime *pabsTime)  {
    CFErrorRef error = NULL;
    CFAbsoluteTime absTime = SecAbsoluteTimeFromDateContentWithError(tag, date->data,
        date->length, &error);
    if (error) {
        secwarning("Invalid date specification in certificate (see RFC5280 4.1.2.5)");
        CFRelease(error);
        return false;
    }

    *pabsTime = absTime;
    return true;
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   true if the date was valid and properly decoded, also return the result in
   absTime.  Return false otherwise. */
__attribute__((__nonnull__)) static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
	CFAbsoluteTime *absTime) {
	if (dateChoice->length == 0) return false;

	DERDecodedInfo decoded;
	if (DERDecodeItem(dateChoice, &decoded))
		return false;

    return derDateContentGetAbsoluteTime(decoded.tag, &decoded.content,
        absTime);
}

static void appendDataProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel, const DERItem *der_data,
    bool localized) {
    CFDataRef data = CFDataCreate(CFGetAllocator(properties),
        der_data->data, der_data->length);
    appendProperty(properties, kSecPropertyTypeData, label, localizedLabel,
                   data, localized);
    CFRelease(data);
}

static void appendRelabeledProperty(CFMutableArrayRef properties,
                                    CFStringRef label,
                                    CFStringRef localizedLabel,
                                    const DERItem *der_data,
                                    CFStringRef labelFormat,
                                    bool localized) {
    CFStringRef newLabel =
        CFStringCreateWithFormat(CFGetAllocator(properties), NULL,
                                 labelFormat, label);
    CFStringRef ll = NULL;
    CFStringRef localizedLabelFormat = NULL;
    if (!localized) {
        /* use provided label and format strings; do not localize */
        ll = localizedLabel = (CFStringRef) CFRetainSafe(label);
        localizedLabelFormat = (CFStringRef) CFRetainSafe(labelFormat);
    } else {
        if (!localizedLabel) {
            /* copy localized label for provided label */
            ll = localizedLabel = SecCopyCertString(label);
        }
        /* copy localized format for provided format */
        localizedLabelFormat = SecCopyCertString(labelFormat);
    }

    CFStringRef newLocalizedLabel =
        CFStringCreateWithFormat(CFGetAllocator(properties), NULL,
                                 localizedLabelFormat, localizedLabel);
    CFReleaseSafe(ll);
    CFReleaseSafe(localizedLabelFormat);
    appendDataProperty(properties, newLabel, newLocalizedLabel, der_data, localized);
    CFReleaseSafe(newLabel);
    CFReleaseSafe(newLocalizedLabel);
}


static void appendUnparsedProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel,
    const DERItem *der_data, bool localized) {
    appendRelabeledProperty(properties, label, localizedLabel, der_data,
                            SEC_UNPARSED_KEY, localized);
}

static void appendInvalidProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *der_data, bool localized) {
    appendRelabeledProperty(properties, label, NULL, der_data,
                            SEC_INVALID_KEY, localized);
}

static void appendDateContentProperty(CFMutableArrayRef properties,
    CFStringRef label, DERTag tag,
    const DERItem *dateContent, bool localized) {
	CFAbsoluteTime absTime;
	if (!derDateContentGetAbsoluteTime(tag, dateContent, &absTime)) {
		/* Date decode failure; insert hex bytes instead. */
		return appendInvalidProperty(properties, label, dateContent, localized);
	}
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, NULL, date, localized);
    CFRelease(date);
}

static void appendDateProperty(CFMutableArrayRef properties,
    CFStringRef label, CFAbsoluteTime absTime, bool localized) {
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, NULL, date, localized);
    CFRelease(date);
}

static void appendValidityPeriodProperty(CFMutableArrayRef parent, CFStringRef label,
                                         SecCertificateRef certificate, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    appendDateProperty(properties, SEC_NOT_VALID_BEFORE_KEY,
                       certificate->_notBefore, localized);
    appendDateProperty(properties, SEC_NOT_VALID_AFTER_KEY,
                       certificate->_notAfter, localized);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties, localized);
    CFReleaseNull(properties);
}

static void appendIPAddressContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *ip, bool localized) {
	CFStringRef value =
		copyIPAddressContentDescription(CFGetAllocator(properties), ip);
	if (value) {
        appendProperty(properties, kSecPropertyTypeString, label, NULL, value, localized);
		CFRelease(value);
	} else {
		appendUnparsedProperty(properties, label, NULL, ip, localized);
	}
}

static void appendURLContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *urlContent, bool localized) {
    CFURLRef url = CFURLCreateWithBytes(CFGetAllocator(properties),
        urlContent->data, urlContent->length, kCFStringEncodingASCII, NULL);
    if (url) {
        appendProperty(properties, kSecPropertyTypeURL, label, NULL, url, localized);
        CFRelease(url);
    } else {
		appendInvalidProperty(properties, label, urlContent, localized);
    }
}

static void appendURLProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *url, bool localized) {
	DERDecodedInfo decoded;
	DERReturn drtn;

	drtn = DERDecodeItem(url, &decoded);
    if (drtn || decoded.tag != ASN1_IA5_STRING) {
		appendInvalidProperty(properties, label, url, localized);
    } else {
        appendURLContentProperty(properties, label, &decoded.content, localized);
    }
}

static void appendOIDProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef llabel, const DERItem *oid, bool localized) {
    CFStringRef oid_string =
        copyOidDescription(CFGetAllocator(properties), oid, localized);
    appendProperty(properties, kSecPropertyTypeString, label, llabel,
                   oid_string, localized);
    CFRelease(oid_string);
}

static void appendAlgorithmProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERAlgorithmId *algorithm, bool localized) {
    CFMutableArrayRef alg_props =
        CFArrayCreateMutable(CFGetAllocator(properties), 0,
            &kCFTypeArrayCallBacks);
    appendOIDProperty(alg_props, SEC_ALGORITHM_KEY, NULL,
                      &algorithm->oid, localized);
    if (algorithm->params.length) {
        if (algorithm->params.length == 2 &&
            algorithm->params.data[0] == ASN1_NULL &&
            algorithm->params.data[1] == 0) {
            CFStringRef value = SecCopyCertString(SEC_NONE_KEY);
            appendProperty(alg_props, kSecPropertyTypeString,
                           SEC_PARAMETERS_KEY, NULL, value, localized);
            CFRelease(value);
        } else {
            appendUnparsedProperty(alg_props, SEC_PARAMETERS_KEY, NULL,
                                   &algorithm->params, localized);
        }
    }
    appendProperty(properties, kSecPropertyTypeSection, label, NULL,
                   alg_props, localized);
    CFRelease(alg_props);
}

static void appendPublicKeyProperty(CFMutableArrayRef parent, CFStringRef label,
                                    SecCertificateRef certificate, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    /* Public key algorithm. */
    appendAlgorithmProperty(properties, SEC_PUBLIC_KEY_ALG_KEY,
                            &certificate->_algId, localized);

    /* Public Key Size */
    SecKeyRef publicKey = SecCertificateCopyKey(certificate);
    if (publicKey) {
        size_t sizeInBytes = SecKeyGetBlockSize(publicKey);
        CFStringRef sizeInBitsString = CFStringCreateWithFormat(allocator, NULL,
                                                                CFSTR("%ld"), (sizeInBytes*8));
        if (sizeInBitsString) {
            appendProperty(properties, kSecPropertyTypeString, SEC_PUBLIC_KEY_SIZE_KEY,
                           NULL, sizeInBitsString, localized);
        }
        CFReleaseNull(sizeInBitsString);
    }
    CFReleaseNull(publicKey);

    /* Consider breaking down an RSA public key into modulus and
     exponent? */
    appendDataProperty(properties, SEC_PUBLIC_KEY_DATA_KEY, NULL,
                       &certificate->_pubKeyDER, localized);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL,
                   properties, localized);
    CFReleaseNull(properties);
}

static void appendSignatureProperty(CFMutableArrayRef parent, CFStringRef label,
                                    SecCertificateRef certificate, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    appendAlgorithmProperty(properties, SEC_SIGNATURE_ALGORITHM_KEY,
                            &certificate->_tbsSigAlg, localized);

    appendDataProperty(properties, SEC_SIGNATURE_DATA_KEY, NULL,
                       &certificate->_signature, localized);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL,
                   properties, localized);
    CFReleaseNull(properties);
}

static void appendFingerprintsProperty(CFMutableArrayRef parent, CFStringRef label,
                                       SecCertificateRef certificate, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    CFDataRef sha256Fingerprint = SecCertificateCopySHA256Digest(certificate);
    if (sha256Fingerprint) {
        appendProperty(properties, kSecPropertyTypeData, SEC_SHA2_FINGERPRINT_KEY,
                       NULL, sha256Fingerprint, localized);
    }
    CFReleaseNull(sha256Fingerprint);

    appendProperty(properties, kSecPropertyTypeData, SEC_SHA1_FINGERPRINT_KEY,
                   NULL, SecCertificateGetSHA1Digest(certificate), localized);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL,
                   properties, localized);
    CFReleaseNull(properties);
}

static CFStringRef copyHexDescription(CFAllocatorRef allocator,
    const DERItem *blob) {
    CFIndex ix, length = blob->length /* < 24 ? blob->length : 24 */;
    CFMutableStringRef string = CFStringCreateMutable(allocator,
        blob->length * 3 - 1);
    for (ix = 0; ix < length; ++ix)
        if (ix == 0)
            CFStringAppendFormat(string, NULL, CFSTR("%02X"), blob->data[ix]);
        else
            CFStringAppendFormat(string, NULL, CFSTR(" %02X"), blob->data[ix]);

    return string;
}

static CFStringRef copyBlobString(CFAllocatorRef allocator,
    CFStringRef blobType, CFStringRef quanta,
    const DERItem *blob, bool localized) {
    CFStringRef localizedBlobType = (localized) ?
        SecCopyCertString(blobType) : (CFStringRef) CFRetainSafe(blobType);
    CFStringRef localizedQuanta = (localized) ?
        SecCopyCertString(quanta) : (CFStringRef) CFRetainSafe(quanta);
    /*  "format string for encoded field data (e.g. Sequence; 128 bytes; "
        "data = 00 00 ...)" */
    CFStringRef blobFormat = (localized) ?
        SecCopyCertString(SEC_BLOB_KEY) : SEC_BLOB_KEY;
    CFStringRef hex = copyHexDescription(allocator, blob);
    CFStringRef result = CFStringCreateWithFormat(allocator, NULL,
        blobFormat, localizedBlobType, blob->length, localizedQuanta, hex);
    CFRelease(hex);
    CFRelease(blobFormat);
    CFReleaseSafe(localizedQuanta);
    CFReleaseSafe(localizedBlobType);

    return result;
}

/* Return a string verbatim (unlocalized) from a DER field. */
static CFStringRef copyContentString(CFAllocatorRef allocator,
	const DERItem *string, CFStringEncoding encoding,
    bool printableOnly) {
    /* Strip potential bogus trailing zero from printable strings. */
    DERSize length = string->length;
    if (length && string->data[length - 1] == 0) {
        /* Don't mess with the length of UTF16 strings though. */
        if (encoding != kCFStringEncodingUTF16)
            length--;
    }
    /* A zero length string isn't considered printable. */
    if (!length && printableOnly)
        return NULL;

    /* Passing true for the 5th paramater to CFStringCreateWithBytes() makes
       it treat kCFStringEncodingUTF16 as big endian by default, whereas
       passing false makes it treat it as native endian by default.  */
    CFStringRef result = CFStringCreateWithBytes(allocator, string->data,
        length, encoding, encoding == kCFStringEncodingUTF16);
    if (result)
        return result;

    return printableOnly ? NULL : copyHexDescription(allocator, string);
}

/* From rfc3280 - Appendix B.  ASN.1 Notes

   CAs MUST force the serialNumber to be a non-negative integer, that
   is, the sign bit in the DER encoding of the INTEGER value MUST be
   zero - this can be done by adding a leading (leftmost) `00'H octet if
   necessary.  This removes a potential ambiguity in mapping between a
   string of octets and an integer value.

   As noted in section 4.1.2.2, serial numbers can be expected to
   contain long integers.  Certificate users MUST be able to handle
   serialNumber values up to 20 octets in length.  Conformant CAs MUST
   NOT use serialNumber values longer than 20 octets.
*/

/* Return the given numeric data as a string: decimal up to 64 bits,
   hex otherwise.
*/
static CFStringRef copyIntegerContentDescription(CFAllocatorRef allocator,
	const DERItem *integer) {
	uint64_t value = 0;
	CFIndex ix, length = integer->length;

	if (length == 0 || length > 8)
		return copyHexDescription(allocator, integer);

	for(ix = 0; ix < length; ++ix) {
		value <<= 8;
		value += integer->data[ix];
	}

    return CFStringCreateWithFormat(allocator, NULL, CFSTR("%llu"), value);
}

static CFStringRef copyDERThingContentDescription(CFAllocatorRef allocator,
	DERTag tag, const DERItem *derThing, bool printableOnly, bool localized) {
    if (!derThing) { return NULL; }
    switch(tag) {
    case ASN1_INTEGER:
    case ASN1_BOOLEAN:
        return printableOnly ? NULL : copyIntegerContentDescription(allocator, derThing);
    case ASN1_PRINTABLE_STRING:
    case ASN1_IA5_STRING:
        return copyContentString(allocator, derThing, kCFStringEncodingASCII, printableOnly);
    case ASN1_UTF8_STRING:
    case ASN1_GENERAL_STRING:
    case ASN1_UNIVERSAL_STRING:
        return copyContentString(allocator, derThing, kCFStringEncodingUTF8, printableOnly);
    case ASN1_T61_STRING:		// 20, also BER_TAG_TELETEX_STRING
    case ASN1_VIDEOTEX_STRING:   // 21
    case ASN1_VISIBLE_STRING:		// 26
        return copyContentString(allocator, derThing, kCFStringEncodingISOLatin1, printableOnly);
    case ASN1_BMP_STRING:   // 30
        return copyContentString(allocator, derThing, kCFStringEncodingUTF16, printableOnly);
    case ASN1_OCTET_STRING:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_BYTE_STRING_KEY, SEC_BYTES_KEY,
                           derThing, localized);
    case ASN1_BIT_STRING:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_BIT_STRING_KEY, SEC_BITS_KEY,
                           derThing, localized);
    case ASN1_CONSTR_SEQUENCE:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_SEQUENCE_KEY, SEC_BYTES_KEY,
                           derThing, localized);
    case ASN1_CONSTR_SET:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_SET_KEY, SEC_BYTES_KEY,
                           derThing, localized);
    case ASN1_OBJECT_ID:
        return printableOnly ? NULL : copyOidDescription(allocator, derThing, localized);
    default:
        if (printableOnly) {
            return NULL;
        } else {
            CFStringRef fmt = (localized) ?
                SecCopyCertString(SEC_NOT_DISPLAYED_KEY) : SEC_NOT_DISPLAYED_KEY;
            if (!fmt) { return NULL; }
            CFStringRef result = CFStringCreateWithFormat(allocator, NULL, fmt,
                (unsigned long)tag, (unsigned long)derThing->length);
            CFRelease(fmt);
            return result;
        }
    }
}

static CFStringRef copyDERThingDescription(CFAllocatorRef allocator,
	const DERItem *derThing, bool printableOnly, bool localized) {
	DERDecodedInfo decoded;
	DERReturn drtn;

	drtn = DERDecodeItem(derThing, &decoded);
    if (drtn) {
        /* TODO: Perhaps put something in the label saying we couldn't parse
           the DER? */
        return printableOnly ? NULL : copyHexDescription(allocator, derThing);
    } else {
        return copyDERThingContentDescription(allocator, decoded.tag,
            &decoded.content, false, localized);
    }
}

static void appendDERThingProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel,
    const DERItem *derThing, bool localized) {
    CFStringRef value = copyDERThingDescription(CFGetAllocator(properties),
        derThing, false, localized);
    if (value) {
        appendProperty(properties, kSecPropertyTypeString, label, localizedLabel,
                       value, localized);
    }
    CFReleaseSafe(value);
}

static OSStatus appendRDNProperty(void *context, const DERItem *rdnType,
                                  const DERItem *rdnValue, CFIndex rdnIX,
                                  bool localized) {
    CFMutableArrayRef properties = (CFMutableArrayRef)context;
    if (rdnIX > 0) {
        /* If there is more than one value pair we create a subsection for the
         second pair, and append things to the subsection for subsequent
         pairs. */
        CFIndex lastIX = CFArrayGetCount(properties) - 1;
        CFTypeRef lastValue = CFArrayGetValueAtIndex(properties, lastIX);
        if (rdnIX == 1) {
            /* Since this is the second rdn pair for a given rdn, we setup a
             new subsection for this rdn.  We remove the first property
             from the properties array and make it the first element in the
             subsection instead. */
            CFMutableArrayRef rdn_props = CFArrayCreateMutable(
                CFGetAllocator(properties), 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(rdn_props, lastValue);
            CFArrayRemoveValueAtIndex(properties, lastIX);
            appendProperty(properties, kSecPropertyTypeSection, NULL, NULL,
                           rdn_props, localized);
            properties = rdn_props;
            // rdn_props is now retained by the original properties array
            CFReleaseSafe(rdn_props);
        } else {
            /* Since this is the third or later rdn pair we have already
             created a subsection in the top level properties array.  Instead
             of appending to that directly we append to the array inside the
             subsection. */
            properties = (CFMutableArrayRef)CFDictionaryGetValue(
                (CFDictionaryRef)lastValue, kSecPropertyKeyValue);
        }
    }

    /* Finally we append the new rdn value to the property array. */
    CFStringRef label =
        SecDERItemCopyOIDDecimalRepresentation(CFGetAllocator(properties),
                                               rdnType);
    CFStringRef localizedLabel = copyOidDescription(CFGetAllocator(properties),
                                                    rdnType, localized);
    appendDERThingProperty(properties, label, localizedLabel,
                           rdnValue, localized);
    CFReleaseSafe(label);
    CFReleaseSafe(localizedLabel);
    return errSecSuccess;
}

static CFArrayRef createPropertiesForRDNContent(CFAllocatorRef allocator,
	const DERItem *rdnSetContent, bool localized) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseRDNContent(rdnSetContent, properties,
		appendRDNProperty, localized);
	if (status) {
        CFArrayRemoveAllValues(properties);
		appendInvalidProperty(properties, SEC_RDN_KEY, rdnSetContent,
                              localized);
	}

	return properties;
}

/*
    From rfc3739 - 3.1.2.  Subject

    When parsing the subject here are some tips for a short name of the cert.
      Choice   I:  commonName
      Choice  II:  givenName
      Choice III:  pseudonym

      The commonName attribute value SHALL, when present, contain a name
      of the subject.  This MAY be in the subject's preferred
      presentation format, or a format preferred by the CA, or some
      other format.  Pseudonyms, nicknames, and names with spelling
      other than defined by the registered name MAY be used.  To
      understand the nature of the name presented in commonName,
      complying applications MAY have to examine present values of the
      givenName and surname attributes, or the pseudonym attribute.

*/
static CFArrayRef createPropertiesForX501NameContent(CFAllocatorRef allocator,
	const DERItem *x501NameContent, bool localized) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseX501NameContent(x501NameContent, properties,
		appendRDNProperty, localized);
	if (status) {
        CFArrayRemoveAllValues(properties);
        appendInvalidProperty(properties, SEC_X501_NAME_KEY,
                              x501NameContent, localized);
	}

	return properties;
}

static CFArrayRef createPropertiesForX501Name(CFAllocatorRef allocator,
	const DERItem *x501Name, bool localized) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseX501Name(x501Name, properties, appendRDNProperty, localized);
	if (status) {
        CFArrayRemoveAllValues(properties);
        appendInvalidProperty(properties, SEC_X501_NAME_KEY,
                              x501Name, localized);
	}

	return properties;
}

static void appendIntegerProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *integer, bool localized) {
    CFStringRef string = copyIntegerContentDescription(
        CFGetAllocator(properties), integer);
    appendProperty(properties, kSecPropertyTypeString, label, NULL,
                   string, localized);
    CFRelease(string);
}

static void appendBoolProperty(CFMutableArrayRef properties,
    CFStringRef label, bool boolean, bool localized) {
    CFStringRef key = (boolean) ? SEC_YES_KEY : SEC_NO_KEY;
    CFStringRef value = (localized) ? SecCopyCertString(key) : key;
    appendProperty(properties, kSecPropertyTypeString, label, NULL,
                   value, localized);
    CFRelease(value);
}

static void appendBooleanProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *boolean,
    bool defaultValue, bool localized) {
    bool result;
    DERReturn drtn = DERParseBooleanWithDefault(boolean, defaultValue, &result);
    if (drtn) {
        /* Couldn't parse boolean; dump the raw unparsed data as hex. */
        appendInvalidProperty(properties, label, boolean, localized);
    } else {
        appendBoolProperty(properties, label, result, localized);
    }
}

static void appendSerialNumberProperty(CFMutableArrayRef parent, CFStringRef label,
                                       DERItem *serialNum, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    if (serialNum->length) {
        appendIntegerProperty(properties, SEC_SERIAL_NUMBER_KEY,
                              serialNum, localized);
        appendProperty(parent, kSecPropertyTypeSection, label, NULL,
                       properties, localized);
    }

    CFReleaseNull(properties);
}

static void appendBitStringContentNames(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *bitStringContent,
    const __nonnull CFStringRef *names, CFIndex namesCount,
    bool localized) {
    DERSize len = bitStringContent->length - 1;
    require_quiet(len == 1 || len == 2, badDER);
    DERByte numUnusedBits = bitStringContent->data[0];
    require_quiet(numUnusedBits < 8, badDER);
    uint_fast16_t bits = 8 * len - numUnusedBits;
    require_quiet(bits <= (uint_fast16_t)namesCount, badDER);
    uint_fast16_t value = bitStringContent->data[1];
    uint_fast16_t mask;
    if (len > 1) {
        value = (value << 8) + bitStringContent->data[2];
        mask = 0x8000;
    } else {
        mask = 0x80;
    }
    uint_fast16_t ix;
    CFStringRef fmt = (localized) ?
        SecCopyCertString(SEC_STRING_LIST_KEY) : SEC_STRING_LIST_KEY;
    CFStringRef string = NULL;
    for (ix = 0; ix < bits; ++ix) {
        CFStringRef localizedName = (localized) ? SecCopyCertString(names[ix]) : CFRetainSafe(names[ix]);
        if (value & mask) {
            if (string) {
                CFStringRef s =
                    CFStringCreateWithFormat(CFGetAllocator(properties),
                                             NULL, fmt, string, localizedName);
                CFRelease(string);
                string = s;
            } else {
                string = localizedName;
                CFRetainSafe(string);
            }
        }
        mask >>= 1;
        CFReleaseNull(localizedName);
    }
    CFRelease(fmt);
    appendProperty(properties, kSecPropertyTypeString, label, NULL,
                   string ? string : CFSTR(""), localized);
    CFReleaseSafe(string);
    return;
badDER:
    appendInvalidProperty(properties, label, bitStringContent, localized);
}

static void appendBitStringNames(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *bitString,
    const __nonnull CFStringRef *names, CFIndex namesCount,
    bool localized) {
    DERDecodedInfo bitStringContent;
    DERReturn drtn = DERDecodeItem(bitString, &bitStringContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(bitStringContent.tag == ASN1_BIT_STRING, badDER);
    appendBitStringContentNames(properties, label, &bitStringContent.content,
        names, namesCount, localized);
    return;
badDER:
    appendInvalidProperty(properties, label, bitString, localized);
}

static void appendKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    static const CFStringRef usageNames[] = {
        SEC_DIGITAL_SIGNATURE_KEY,
        SEC_NON_REPUDIATION_KEY,
        SEC_KEY_ENCIPHERMENT_KEY,
        SEC_DATA_ENCIPHERMENT_KEY,
        SEC_KEY_AGREEMENT_KEY,
        SEC_CERT_SIGN_KEY,
        SEC_CRL_SIGN_KEY,
        SEC_ENCIPHER_ONLY_KEY,
        SEC_DECIPHER_ONLY_KEY
    };
    appendBitStringNames(properties, SEC_USAGE_KEY, extnValue,
        usageNames, array_size(usageNames), localized);
}

static void appendPrivateKeyUsagePeriod(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    DERPrivateKeyUsagePeriod pkup;
    DERReturn drtn = DERParseSequence(extnValue,
        DERNumPrivateKeyUsagePeriodItemSpecs, DERPrivateKeyUsagePeriodItemSpecs,
        &pkup, sizeof(pkup));
    require_noerr_quiet(drtn, badDER);
    if (pkup.notBefore.length) {
        appendDateContentProperty(properties, SEC_NOT_VALID_BEFORE_KEY,
            ASN1_GENERALIZED_TIME, &pkup.notBefore, localized);
    }
    if (pkup.notAfter.length) {
        appendDateContentProperty(properties, SEC_NOT_VALID_AFTER_KEY,
            ASN1_GENERALIZED_TIME, &pkup.notAfter, localized);
    }
    return;
badDER:
    appendInvalidProperty(properties, SEC_PRIVATE_KU_PERIOD_KEY,
                          extnValue, localized);
}

static void appendStringContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *stringContent,
    CFStringEncoding encoding, bool localized) {
    CFStringRef string = CFStringCreateWithBytes(CFGetAllocator(properties),
            stringContent->data, stringContent->length, encoding, FALSE);
    if (string) {
        appendProperty(properties, kSecPropertyTypeString, label, NULL,
                       string, localized);
        CFRelease(string);
    } else {
        appendInvalidProperty(properties, label, stringContent, localized);
    }
}

/*
      OtherName ::= SEQUENCE {
           type-id    OBJECT IDENTIFIER,
           value      [0] EXPLICIT ANY DEFINED BY type-id }
*/
static void appendOtherNameContentProperty(CFMutableArrayRef properties,
	const DERItem *otherNameContent, bool localized) {
    DEROtherName on;
	DERReturn drtn = DERParseSequenceContent(otherNameContent,
        DERNumOtherNameItemSpecs, DEROtherNameItemSpecs,
        &on, sizeof(on));
	require_noerr_quiet(drtn, badDER);
	CFAllocatorRef allocator = CFGetAllocator(properties);
	CFStringRef label =
        SecDERItemCopyOIDDecimalRepresentation(allocator, &on.typeIdentifier);
	CFStringRef localizedLabel =
        copyOidDescription(allocator, &on.typeIdentifier, localized);
	CFStringRef value_string = copyDERThingDescription(allocator, &on.value,
                                                       false, localized);
	if (value_string) {
		appendProperty(properties, kSecPropertyTypeString, label,
                       localizedLabel, value_string, localized);
	} else {
        appendUnparsedProperty(properties, label, localizedLabel,
                               &on.value, localized);
    }
    CFReleaseSafe(value_string);
    CFReleaseSafe(label);
    CFReleaseSafe(localizedLabel);
    return;
badDER:
    appendInvalidProperty(properties, SEC_OTHER_NAME_KEY,
                          otherNameContent, localized);
}

/*
      GeneralName ::= CHOICE {
           otherName                       [0]     OtherName,
           rfc822Name                      [1]     IA5String,
           dNSName                         [2]     IA5String,
           x400Address                     [3]     ORAddress,
           directoryName                   [4]     Name,
           ediPartyName                    [5]     EDIPartyName,
           uniformResourceIdentifier       [6]     IA5String,
           iPAddress                       [7]     OCTET STRING,
           registeredID                    [8]     OBJECT IDENTIFIER}

      EDIPartyName ::= SEQUENCE {
           nameAssigner            [0]     DirectoryString OPTIONAL,
           partyName               [1]     DirectoryString }
 */
static bool appendGeneralNameContentProperty(CFMutableArrayRef properties,
    DERTag tag, const DERItem *generalName, bool localized) {
	switch (tag) {
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
		appendOtherNameContentProperty(properties, generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | 1:
		/* IA5String. */
		appendStringContentProperty(properties, SEC_EMAIL_ADDRESS_KEY,
			generalName, kCFStringEncodingASCII, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | 2:
		/* IA5String. */
		appendStringContentProperty(properties, SEC_DNS_NAME_KEY, generalName,
			kCFStringEncodingASCII, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		appendUnparsedProperty(properties, SEC_X400_ADDRESS_KEY, NULL,
			generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
	{
		CFArrayRef directory_plist =
			createPropertiesForX501Name(CFGetAllocator(properties),
				generalName, localized);
		appendProperty(properties, kSecPropertyTypeSection,
			SEC_DIRECTORY_NAME_KEY, NULL, directory_plist, localized);
		CFRelease(directory_plist);
		break;
	}
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		appendUnparsedProperty(properties, SEC_EDI_PARTY_NAME_KEY, NULL,
			generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		appendURLProperty(properties, SEC_URI_KEY, generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | 6:
		appendURLContentProperty(properties, SEC_URI_KEY, generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | 7:
		appendIPAddressContentProperty(properties, SEC_IP_ADDRESS_KEY,
			generalName, localized);
		break;
	case ASN1_CONTEXT_SPECIFIC | 8:
		appendOIDProperty(properties, SEC_REGISTERED_ID_KEY, NULL,
                          generalName, localized);
		break;
	default:
		goto badDER;
		break;
	}
	return true;
badDER:
	return false;
}

static void appendGeneralNameProperty(CFMutableArrayRef properties,
    const DERItem *generalName, bool localized) {
    DERDecodedInfo generalNameContent;
	DERReturn drtn = DERDecodeItem(generalName, &generalNameContent);
	require_noerr_quiet(drtn, badDER);
	if (appendGeneralNameContentProperty(properties, generalNameContent.tag,
		&generalNameContent.content, localized))
		return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAME_KEY,
                          generalName, localized);
}


/*
      GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 */
static void appendGeneralNamesContent(CFMutableArrayRef properties,
    const DERItem *generalNamesContent, bool localized) {
    DERSequence gnSeq;
    DERReturn drtn = DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo generalNameContent;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		if (!appendGeneralNameContentProperty(properties,
			generalNameContent.tag, &generalNameContent.content, localized)) {
			goto badDER;
		}
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAMES_KEY,
        generalNamesContent, localized);
}

static void appendGeneralNames(CFMutableArrayRef properties,
    const DERItem *generalNames, bool localized) {
    DERDecodedInfo generalNamesContent;
    DERReturn drtn = DERDecodeItem(generalNames, &generalNamesContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(generalNamesContent.tag == ASN1_CONSTR_SEQUENCE,
        badDER);
    appendGeneralNamesContent(properties, &generalNamesContent.content,
                              localized);
    return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAMES_KEY,
                          generalNames, localized);
}

/*
    BasicConstraints ::= SEQUENCE {
        cA                      BOOLEAN DEFAULT FALSE,
        pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
*/
static void appendBasicConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
	DERBasicConstraints basicConstraints;
	DERReturn drtn = DERParseSequence(extnValue,
        DERNumBasicConstraintsItemSpecs, DERBasicConstraintsItemSpecs,
        &basicConstraints, sizeof(basicConstraints));
	require_noerr_quiet(drtn, badDER);

    appendBooleanProperty(properties, SEC_CERT_AUTHORITY_KEY,
        &basicConstraints.cA, false, localized);

    if (basicConstraints.pathLenConstraint.length != 0) {
        appendIntegerProperty(properties, SEC_PATH_LEN_CONSTRAINT_KEY,
            &basicConstraints.pathLenConstraint, localized);
    }
    return;
badDER:
    appendInvalidProperty(properties, SEC_BASIC_CONSTRAINTS_KEY,
                          extnValue, localized);
}

/*
 * id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
 *
 * NameConstraints ::= SEQUENCE {
 * permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 * excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 * GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * GeneralSubtree ::= SEQUENCE {
 * base                    GeneralName,
 * minimum         [0]     BaseDistance DEFAULT 0,
 * maximum         [1]     BaseDistance OPTIONAL }
 *
 * BaseDistance ::= INTEGER (0..MAX)
 */
static void appendNameConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    DERNameConstraints nc;
    DERReturn drtn;
    drtn = DERParseSequence(extnValue,
                            DERNumNameConstraintsItemSpecs,
                            DERNameConstraintsItemSpecs,
                            &nc, sizeof(nc));
    require_noerr_quiet(drtn, badDER);
    if (nc.permittedSubtrees.length) {
        DERSequence gsSeq;
        require_noerr_quiet(DERDecodeSeqContentInit(&nc.permittedSubtrees, &gsSeq), badDER);
        DERDecodedInfo gsContent;
        while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
            DERGeneralSubtree derGS;
            require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
            drtn = DERParseSequenceContent(&gsContent.content,
                                           DERNumGeneralSubtreeItemSpecs,
                                           DERGeneralSubtreeItemSpecs,
                                           &derGS, sizeof(derGS));
            require_noerr_quiet(drtn, badDER);
            if (derGS.minimum.length) {
                appendIntegerProperty(properties, SEC_PERMITTED_MINIMUM_KEY,
                                      &derGS.minimum, localized);
            }
            if (derGS.maximum.length) {
                appendIntegerProperty(properties, SEC_PERMITTED_MAXIMUM_KEY,
                                      &derGS.maximum, localized);
            }
            if (derGS.generalName.length) {
                CFMutableArrayRef base = CFArrayCreateMutable(allocator, 0,
                                                                   &kCFTypeArrayCallBacks);
                appendProperty(properties, kSecPropertyTypeSection,
                               SEC_PERMITTED_NAME_KEY, NULL, base, localized);
                appendGeneralNameProperty(base, &derGS.generalName, localized);
                CFRelease(base);
            }
        }
        require_quiet(drtn == DR_EndOfSequence, badDER);
    }
    if (nc.excludedSubtrees.length) {
        DERSequence gsSeq;
        require_noerr_quiet(DERDecodeSeqContentInit(&nc.excludedSubtrees, &gsSeq), badDER);
        DERDecodedInfo gsContent;
        while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
            DERGeneralSubtree derGS;
            require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
            drtn = DERParseSequenceContent(&gsContent.content,
                                           DERNumGeneralSubtreeItemSpecs,
                                           DERGeneralSubtreeItemSpecs,
                                           &derGS, sizeof(derGS));
            require_noerr_quiet(drtn, badDER);
            if (derGS.minimum.length) {
                appendIntegerProperty(properties, SEC_EXCLUDED_MINIMUM_KEY,
                                      &derGS.minimum, localized);
            }
            if (derGS.maximum.length) {
                appendIntegerProperty(properties, SEC_EXCLUDED_MAXIMUM_KEY,
                                      &derGS.maximum, localized);
            }
            if (derGS.generalName.length) {
                CFMutableArrayRef base = CFArrayCreateMutable(allocator, 0,
                                                              &kCFTypeArrayCallBacks);
                appendProperty(properties, kSecPropertyTypeSection,
                               SEC_EXCLUDED_NAME_KEY, NULL, base, localized);
                appendGeneralNameProperty(base, &derGS.generalName, localized);
                CFRelease(base);
            }
        }
        require_quiet(drtn == DR_EndOfSequence, badDER);
    }

    return;
badDER:
    appendInvalidProperty(properties, SEC_NAME_CONSTRAINTS_KEY,
                          extnValue, localized);
}

/*
   CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint

   DistributionPoint ::= SEQUENCE {
        distributionPoint       [0]     DistributionPointName OPTIONAL,
        reasons                 [1]     ReasonFlags OPTIONAL,
        cRLIssuer               [2]     GeneralNames OPTIONAL }

   DistributionPointName ::= CHOICE {
        fullName                [0]     GeneralNames,
        nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }

   ReasonFlags ::= BIT STRING {
        unused                  (0),
        keyCompromise           (1),
        cACompromise            (2),
        affiliationChanged      (3),
        superseded              (4),
        cessationOfOperation    (5),
        certificateHold         (6),
        privilegeWithdrawn      (7),
        aACompromise            (8) }
*/
static void appendCrlDistributionPoints(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    DERTag tag;
    DERSequence dpSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &dpSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo dpSeqContent;
    while ((drtn = DERDecodeSeqNext(&dpSeq, &dpSeqContent)) == DR_Success) {
        require_quiet(dpSeqContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERDistributionPoint dp;
        drtn = DERParseSequenceContent(&dpSeqContent.content,
            DERNumDistributionPointItemSpecs,
            DERDistributionPointItemSpecs,
            &dp, sizeof(dp));
        require_noerr_quiet(drtn, badDER);
        if (dp.distributionPoint.length) {
            DERDecodedInfo distributionPointName;
            drtn = DERDecodeItem(&dp.distributionPoint, &distributionPointName);
            require_noerr_quiet(drtn, badDER);
            if (distributionPointName.tag ==
                (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0)) {
                /* Full Name */
                appendGeneralNamesContent(properties,
                    &distributionPointName.content, localized);
            } else if (distributionPointName.tag ==
                (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1)) {
				CFArrayRef rdn_props = createPropertiesForRDNContent(allocator,
					&dp.reasons, localized);
				appendProperty(properties, kSecPropertyTypeSection,
					SEC_NAME_REL_CRL_ISSUER_KEY, NULL, rdn_props, localized);
				CFRelease(rdn_props);
            } else {
                goto badDER;
            }
        }
        if (dp.reasons.length) {
            static const CFStringRef reasonNames[] = {
                SEC_UNUSED_KEY,
                SEC_KEY_COMPROMISE_KEY,
                SEC_CA_COMPROMISE_KEY,
                SEC_AFFILIATION_CHANGED_KEY,
                SEC_SUPERSEDED_KEY,
                SEC_CESSATION_OF_OPER_KEY,
                SEC_CERTIFICATE_HOLD_KEY,
                SEC_PRIV_WITHDRAWN_KEY,
                SEC_AA_COMPROMISE_KEY
            };
            appendBitStringContentNames(properties, SEC_REASONS_KEY,
                &dp.reasons,
                reasonNames, array_size(reasonNames), localized);
        }
        if (dp.cRLIssuer.length) {
            CFMutableArrayRef crlIssuer = CFArrayCreateMutable(allocator, 0,
                &kCFTypeArrayCallBacks);
            appendProperty(properties, kSecPropertyTypeSection,
                SEC_CRL_ISSUER_KEY, NULL, crlIssuer, localized);
            CFRelease(crlIssuer);
            appendGeneralNames(crlIssuer, &dp.cRLIssuer, localized);
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_CRL_DISTR_POINTS_KEY,
                          extnValue, localized);
}

/*
    Decode a sequence of integers into a comma separated list of ints.
*/
static void appendIntegerSequenceContent(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *intSequenceContent,
    bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
	DERSequence intSeq;
    CFStringRef fmt = NULL, value = NULL, intDesc = NULL, v = NULL;
	DERReturn drtn = DERDecodeSeqContentInit(intSequenceContent, &intSeq);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo intContent;
    fmt = (localized) ?
        SecCopyCertString(SEC_STRING_LIST_KEY) : SEC_STRING_LIST_KEY;
    require_quiet(fmt, badDER);
	while ((drtn = DERDecodeSeqNext(&intSeq, &intContent)) == DR_Success) {
		require_quiet(intContent.tag == ASN1_INTEGER, badDER);
		intDesc = copyIntegerContentDescription(
			allocator, &intContent.content);
        require_quiet(intDesc, badDER);
		if (value) {
            v = CFStringCreateWithFormat(allocator, NULL, fmt, value, intDesc);
            CFReleaseNull(value);
            require_quiet(v, badDER);
            value = v;
		} else {
			value = CFStringCreateMutableCopy(allocator, 0, intDesc);
            require_quiet(value, badDER);
		}
        CFReleaseNull(intDesc);
	}
    CFReleaseNull(fmt);
	require_quiet(drtn == DR_EndOfSequence, badDER);
	if (value) {
		appendProperty(properties, kSecPropertyTypeString, label, NULL,
                       value, localized);
		CFRelease(value);
		return;
	}
	/* DROPTHOUGH if !value. */
badDER:
    CFReleaseNull(fmt);
    CFReleaseNull(intDesc);
    CFReleaseNull(value);
	appendInvalidProperty(properties, label, intSequenceContent, localized);
}

static void appendCertificatePolicies(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    CFStringRef piLabel = NULL, piFmt = NULL, lpiLabel = NULL;
    CFStringRef pqLabel = NULL, pqFmt = NULL, lpqLabel = NULL;
    DERTag tag;
    DERSequence piSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo piContent;
    int pin = 1;
    while ((drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        require_quiet(piContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERPolicyInformation pi;
        drtn = DERParseSequenceContent(&piContent.content,
            DERNumPolicyInformationItemSpecs,
            DERPolicyInformationItemSpecs,
            &pi, sizeof(pi));
        require_noerr_quiet(drtn, badDER);
        piLabel = CFStringCreateWithFormat(allocator, NULL,
            SEC_POLICY_IDENTIFIER_KEY, pin);
        require_quiet(piLabel, badDER);
        piFmt = (localized) ?
            SecCopyCertString(SEC_POLICY_IDENTIFIER_KEY) : SEC_POLICY_IDENTIFIER_KEY;
        require_quiet(piFmt, badDER);
        lpiLabel = CFStringCreateWithFormat(allocator, NULL, piFmt, pin++);
        require_quiet(lpiLabel, badDER);
        CFReleaseNull(piFmt);
        appendOIDProperty(properties, piLabel, lpiLabel,
                          &pi.policyIdentifier, localized);
        CFReleaseNull(piLabel);
        CFReleaseNull(lpiLabel);
        if (pi.policyQualifiers.length == 0)
            continue;

        DERSequence pqSeq;
        drtn = DERDecodeSeqContentInit(&pi.policyQualifiers, &pqSeq);
        require_noerr_quiet(drtn, badDER);
        DERDecodedInfo pqContent;
        int pqn = 1;
        while ((drtn = DERDecodeSeqNext(&pqSeq, &pqContent)) == DR_Success) {
            DERPolicyQualifierInfo pqi;
            drtn = DERParseSequenceContent(&pqContent.content,
                DERNumPolicyQualifierInfoItemSpecs,
                DERPolicyQualifierInfoItemSpecs,
                &pqi, sizeof(pqi));
            require_noerr_quiet(drtn, badDER);
            DERDecodedInfo qualifierContent;
            drtn = DERDecodeItem(&pqi.qualifier, &qualifierContent);
            require_noerr_quiet(drtn, badDER);
            pqLabel = CFStringCreateWithFormat(allocator, NULL,
                SEC_POLICY_QUALIFIER_KEY, pqn);
            require_quiet(pqLabel, badDER);
            pqFmt = (localized) ?
                SecCopyCertString(SEC_POLICY_QUALIFIER_KEY) : SEC_POLICY_QUALIFIER_KEY;
            require_quiet(pqFmt, badDER);
            lpqLabel = CFStringCreateWithFormat(allocator, NULL, pqFmt, pqn++);
            require_quiet(lpqLabel, badDER);
            CFReleaseNull(pqFmt);
            appendOIDProperty(properties, pqLabel, lpqLabel,
                              &pqi.policyQualifierID, localized);
            CFReleaseNull(pqLabel);
            CFReleaseNull(lpqLabel);
            if (DEROidCompare(&oidQtCps, &pqi.policyQualifierID)) {
                require_quiet(qualifierContent.tag == ASN1_IA5_STRING, badDER);
                appendURLContentProperty(properties, SEC_CPS_URI_KEY,
                                         &qualifierContent.content, localized);
            } else if (DEROidCompare(&oidQtUNotice, &pqi.policyQualifierID)) {
                require_quiet(qualifierContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
                DERUserNotice un;
                drtn = DERParseSequenceContent(&qualifierContent.content,
                    DERNumUserNoticeItemSpecs,
                    DERUserNoticeItemSpecs,
                    &un, sizeof(un));
                require_noerr_quiet(drtn, badDER);
                if (un.noticeRef.length) {
                    DERNoticeReference nr;
                    drtn = DERParseSequenceContent(&un.noticeRef,
                        DERNumNoticeReferenceItemSpecs,
                        DERNoticeReferenceItemSpecs,
                        &nr, sizeof(nr));
                    require_noerr_quiet(drtn, badDER);
                    appendDERThingProperty(properties,
                        SEC_ORGANIZATION_KEY, NULL,
                        &nr.organization, localized);
					appendIntegerSequenceContent(properties,
						SEC_NOTICE_NUMBERS_KEY, &nr.noticeNumbers, localized);
                }
                if (un.explicitText.length) {
                    appendDERThingProperty(properties, SEC_EXPLICIT_TEXT_KEY,
                        NULL, &un.explicitText, localized);
                }
            } else {
                appendUnparsedProperty(properties, SEC_QUALIFIER_KEY, NULL,
                    &pqi.qualifier, localized);
            }
        }
        require_quiet(drtn == DR_EndOfSequence, badDER);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    CFReleaseNull(piFmt);
    CFReleaseNull(piLabel);
    CFReleaseNull(lpiLabel);
    CFReleaseNull(pqFmt);
    CFReleaseNull(pqLabel);
    CFReleaseNull(lpqLabel);
    appendInvalidProperty(properties, SEC_CERT_POLICIES_KEY,
                          extnValue, localized);
}

static void appendSubjectKeyIdentifier(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
	DERReturn drtn;
    DERDecodedInfo keyIdentifier;
	drtn = DERDecodeItem(extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	appendDataProperty(properties, SEC_KEY_IDENTIFIER_KEY, NULL,
		&keyIdentifier.content, localized);

	return;
badDER:
    appendInvalidProperty(properties, SEC_SUBJ_KEY_ID_KEY,
        extnValue, localized);
}

/*
AuthorityKeyIdentifier ::= SEQUENCE {
    keyIdentifier             [0] KeyIdentifier            OPTIONAL,
    authorityCertIssuer       [1] GeneralNames             OPTIONAL,
    authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
    -- authorityCertIssuer and authorityCertSerialNumber MUST both
    -- be present or both be absent

KeyIdentifier ::= OCTET STRING
*/
static void appendAuthorityKeyIdentifier(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
	DERAuthorityKeyIdentifier akid;
	DERReturn drtn;
	drtn = DERParseSequence(extnValue,
		DERNumAuthorityKeyIdentifierItemSpecs,
		DERAuthorityKeyIdentifierItemSpecs,
		&akid, sizeof(akid));
	require_noerr_quiet(drtn, badDER);
	if (akid.keyIdentifier.length) {
		appendDataProperty(properties, SEC_KEY_IDENTIFIER_KEY, NULL,
			&akid.keyIdentifier, localized);
	}
	if (akid.authorityCertIssuer.length ||
		akid.authorityCertSerialNumber.length) {
		require_quiet(akid.authorityCertIssuer.length &&
			akid.authorityCertSerialNumber.length, badDER);
		/* Perhaps put in a subsection called Authority Certificate Issuer. */
		appendGeneralNamesContent(properties,
			&akid.authorityCertIssuer, localized);
		appendIntegerProperty(properties, SEC_AUTH_CERT_SERIAL_KEY,
			&akid.authorityCertSerialNumber, localized);
	}

	return;
badDER:
    appendInvalidProperty(properties, SEC_AUTHORITY_KEY_ID_KEY,
                          extnValue, localized);
}

/*
   PolicyConstraints ::= SEQUENCE {
        requireExplicitPolicy           [0] SkipCerts OPTIONAL,
        inhibitPolicyMapping            [1] SkipCerts OPTIONAL }

   SkipCerts ::= INTEGER (0..MAX)
*/
static void appendPolicyConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
	DERPolicyConstraints pc;
	DERReturn drtn;
	drtn = DERParseSequence(extnValue,
		DERNumPolicyConstraintsItemSpecs,
		DERPolicyConstraintsItemSpecs,
		&pc, sizeof(pc));
	require_noerr_quiet(drtn, badDER);
	if (pc.requireExplicitPolicy.length) {
		appendIntegerProperty(properties, SEC_REQUIRE_EXPL_POLICY_KEY,
                              &pc.requireExplicitPolicy, localized);
	}
	if (pc.inhibitPolicyMapping.length) {
		appendIntegerProperty(properties, SEC_INHIBIT_POLICY_MAP_KEY,
                              &pc.inhibitPolicyMapping, localized);
	}

	return;

badDER:
    appendInvalidProperty(properties, SEC_POLICY_CONSTRAINTS_KEY,
                          extnValue, localized);
}

/*
extendedKeyUsage EXTENSION ::= {
        SYNTAX SEQUENCE SIZE (1..MAX) OF KeyPurposeId
        IDENTIFIED BY id-ce-extKeyUsage }

KeyPurposeId ::= OBJECT IDENTIFIER
*/
static void appendExtendedKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    DERTag tag;
    DERSequence derSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &derSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo currDecoded;
    while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
        require_quiet(currDecoded.tag == ASN1_OBJECT_ID, badDER);
        appendOIDProperty(properties, SEC_PURPOSE_KEY, NULL,
            &currDecoded.content, localized);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_EXTENDED_KEY_USAGE_KEY,
                          extnValue, localized);
}

/*
   id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }

   AuthorityInfoAccessSyntax  ::=
           SEQUENCE SIZE (1..MAX) OF AccessDescription

   AccessDescription  ::=  SEQUENCE {
           accessMethod          OBJECT IDENTIFIER,
           accessLocation        GeneralName  }

   id-ad OBJECT IDENTIFIER ::= { id-pkix 48 }

   id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }

   id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
*/
static void appendInfoAccess(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    DERTag tag;
    DERSequence adSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &adSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo adContent;
    while ((drtn = DERDecodeSeqNext(&adSeq, &adContent)) == DR_Success) {
        require_quiet(adContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAccessDescription ad;
		drtn = DERParseSequenceContent(&adContent.content,
			DERNumAccessDescriptionItemSpecs,
			DERAccessDescriptionItemSpecs,
			&ad, sizeof(ad));
		require_noerr_quiet(drtn, badDER);
        appendOIDProperty(properties, SEC_ACCESS_METHOD_KEY, NULL,
                          &ad.accessMethod, localized);
		//TODO: Do something with SEC_ACCESS_LOCATION_KEY
        appendGeneralNameProperty(properties, &ad.accessLocation, localized);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_AUTH_INFO_ACCESS_KEY,
                          extnValue, localized);
}

static void appendNetscapeCertType(CFMutableArrayRef properties,
    const DERItem *extnValue, bool localized) {
    static const CFStringRef certTypes[] = {
        SEC_SSL_CLIENT_KEY,
        SEC_SSL_SERVER_KEY,
        SEC_SMIME_KEY,
        SEC_OBJECT_SIGNING_KEY,
        SEC_RESERVED_KEY,
        SEC_SSL_CA_KEY,
        SEC_SMIME_CA_KEY,
        SEC_OBJECT_SIGNING_CA_KEY
    };
    appendBitStringNames(properties, SEC_USAGE_KEY, extnValue,
        certTypes, array_size(certTypes), localized);
}

static bool appendPrintableDERSequence(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *sequence, bool localized) {
    DERTag tag;
    DERSequence derSeq;
    DERReturn drtn = DERDecodeSeqInit(sequence, &tag, &derSeq);
    require_noerr_quiet(drtn, badSequence);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badSequence);
    DERDecodedInfo currDecoded;
    bool appendedSomething = false;
    while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
		switch (currDecoded.tag)
		{
			case 0:                             // 0
			case ASN1_SEQUENCE:                 // 16
			case ASN1_SET:                      // 17
				// skip constructed object lengths
				break;

			case ASN1_UTF8_STRING:              // 12
			case ASN1_NUMERIC_STRING:           // 18
			case ASN1_PRINTABLE_STRING:         // 19
			case ASN1_T61_STRING:               // 20, also ASN1_TELETEX_STRING
			case ASN1_VIDEOTEX_STRING:          // 21
			case ASN1_IA5_STRING:               // 22
			case ASN1_GRAPHIC_STRING:           // 25
			case ASN1_VISIBLE_STRING:           // 26, also ASN1_ISO646_STRING
			case ASN1_GENERAL_STRING:           // 27
			case ASN1_UNIVERSAL_STRING:         // 28
			{
                CFStringRef string =
                    copyDERThingContentDescription(CFGetAllocator(properties),
                        currDecoded.tag, &currDecoded.content, false, localized);
                require_quiet(string, badSequence);

                appendProperty(properties, kSecPropertyTypeString, label, NULL,
                    string, localized);
                CFReleaseNull(string);
				appendedSomething = true;
                break;
			}
			default:
				break;
		}
    }
    require_quiet(drtn == DR_EndOfSequence, badSequence);
	return appendedSomething;
badSequence:
    return false;
}

static void appendExtension(CFMutableArrayRef parent,
    const SecCertificateExtension *extn,
    bool localized) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);
    const DERItem
        *extnID = &extn->extnID,
        *extnValue = &extn->extnValue;
    CFStringRef label = NULL;
    CFStringRef localizedLabel = NULL;

    appendBoolProperty(properties, SEC_CRITICAL_KEY, extn->critical, localized);
    require_quiet(extnID, xit);

	bool handled = true;
	/* Extensions that we know how to handle ourselves... */
	if (extnID->length == oidSubjectKeyIdentifier.length &&
		!memcmp(extnID->data, oidSubjectKeyIdentifier.data, extnID->length - 1))
	{
		switch (extnID->data[extnID->length - 1]) {
		case 14: /* SubjectKeyIdentifier     id-ce 14 */
			appendSubjectKeyIdentifier(properties, extnValue, localized);
			break;
		case 15: /* KeyUsage                 id-ce 15 */
			appendKeyUsage(properties, extnValue, localized);
			break;
		case 16: /* PrivateKeyUsagePeriod    id-ce 16 */
			appendPrivateKeyUsagePeriod(properties, extnValue, localized);
			break;
		case 17: /* SubjectAltName           id-ce 17 */
		case 18: /* IssuerAltName            id-ce 18 */
			appendGeneralNames(properties, extnValue, localized);
			break;
		case 19: /* BasicConstraints         id-ce 19 */
			appendBasicConstraints(properties, extnValue, localized);
			break;
		case 30: /* NameConstraints          id-ce 30 */
			appendNameConstraints(properties, extnValue, localized);
			break;
		case 31: /* CRLDistributionPoints    id-ce 31 */
			appendCrlDistributionPoints(properties, extnValue, localized);
			break;
		case 32: /* CertificatePolicies      id-ce 32 */
			appendCertificatePolicies(properties, extnValue, localized);
			break;
		case 33: /* PolicyMappings           id-ce 33 */
			handled = false;
			break;
		case 35: /* AuthorityKeyIdentifier   id-ce 35 */
			appendAuthorityKeyIdentifier(properties, extnValue, localized);
			break;
		case 36: /* PolicyConstraints        id-ce 36 */
			appendPolicyConstraints(properties, extnValue, localized);
			break;
		case 37: /* ExtKeyUsage              id-ce 37 */
			appendExtendedKeyUsage(properties, extnValue, localized);
			break;
		case 46: /* FreshestCRL              id-ce 46 */
			handled = false;
			break;
		case 54: /* InhibitAnyPolicy         id-ce 54 */
			handled = false;
			break;
		default:
			handled = false;
			break;
		}
	} else if (extnID->length == oidAuthorityInfoAccess.length &&
		!memcmp(extnID->data, oidAuthorityInfoAccess.data, extnID->length - 1))
	{
		switch (extnID->data[extnID->length - 1]) {
		case  1: /* AuthorityInfoAccess      id-pe 1 */
			appendInfoAccess(properties, extnValue, localized);
			break;
		case  3: /* QCStatements             id-pe 3 */
			handled = false;
			break;
		case 11: /* SubjectInfoAccess        id-pe 11 */
			appendInfoAccess(properties, extnValue, localized);
			break;
		default:
			handled = false;
			break;
		}
	} else if (DEROidCompare(extnID, &oidNetscapeCertType)) {
		/* 2.16.840.1.113730.1.1 netscape 1 1 */
		appendNetscapeCertType(properties, extnValue, localized);
	} else {
		handled = false;
	}

	if (!handled) {
		/* Try to parse and display printable string(s). */
		if (appendPrintableDERSequence(properties, SEC_DATA_KEY, extnValue, localized)) {
			/* Nothing to do here appendPrintableDERSequence did the work. */
		} else {
			/* Couldn't parse extension; dump the raw unparsed data as hex. */
			appendUnparsedProperty(properties, SEC_DATA_KEY, NULL, extnValue, localized);
		}
	}
    label = SecDERItemCopyOIDDecimalRepresentation(allocator, extnID);
    localizedLabel = copyOidDescription(allocator, extnID, localized);
    appendProperty(parent, kSecPropertyTypeSection, label, localizedLabel,
                   properties, localized);
xit:
    CFReleaseSafe(localizedLabel);
    CFReleaseSafe(label);
    CFReleaseSafe(properties);
}

/* Different types of summary types from least desired to most desired. */
enum SummaryType {
    kSummaryTypeNone,
    kSummaryTypePrintable,
    kSummaryTypeOrganizationName,
    kSummaryTypeOrganizationalUnitName,
    kSummaryTypeCommonName,
};

struct Summary {
    enum SummaryType type;
    CFStringRef summary;
    CFStringRef description;
};

static OSStatus obtainSummaryFromX501Name(void *context,
    const DERItem *type, const DERItem *value, CFIndex rdnIX,
    bool localized) {
    struct Summary *summary = (struct Summary *)context;
    enum SummaryType stype = kSummaryTypeNone;
    CFStringRef string = NULL;
    if (DEROidCompare(type, &oidCommonName)) {
        stype = kSummaryTypeCommonName;
    } else if (DEROidCompare(type, &oidOrganizationalUnitName)) {
        stype = kSummaryTypeOrganizationalUnitName;
    } else if (DEROidCompare(type, &oidOrganizationName)) {
        stype = kSummaryTypeOrganizationName;
    } else if (DEROidCompare(type, &oidDescription)) {
        string = copyDERThingDescription(kCFAllocatorDefault, value,
                                         true, localized);
        if (string) {
            if (summary->description) {
                CFStringRef fmt = (localized) ?
                    SecCopyCertString(SEC_STRING_LIST_KEY) : SEC_STRING_LIST_KEY;
                CFStringRef newDescription = CFStringCreateWithFormat(kCFAllocatorDefault,
                    NULL, fmt, string, summary->description);
                CFRelease(fmt);
                CFRelease(summary->description);
                summary->description = newDescription;
            } else {
                summary->description = string;
                CFRetain(string);
            }
            stype = kSummaryTypePrintable;
        }
    } else {
        stype = kSummaryTypePrintable;
    }

    /* Build a string with all instances of the most desired
       component type in reverse order encountered comma separated list,
       The order of desirability is defined by enum SummaryType. */
    if (summary->type <= stype) {
        if (!string) {
            string = copyDERThingDescription(kCFAllocatorDefault, value,
                                             true, localized);
        }
        if (string) {
            if (summary->type == stype) {
                CFStringRef fmt = (localized) ?
                    SecCopyCertString(SEC_STRING_LIST_KEY) : SEC_STRING_LIST_KEY;
                CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault,
                    NULL, fmt, string, summary->summary);
                CFRelease(fmt);
                CFRelease(string);
                string = newSummary;
            } else {
                summary->type = stype;
            }
            CFReleaseSafe(summary->summary);
            summary->summary = string;
        }
    } else {
        CFReleaseSafe(string);
    }

	return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectSummary(SecCertificateRef certificate) {
    struct Summary summary = {};
	OSStatus status = parseX501NameContent(&certificate->_subject, &summary, obtainSummaryFromX501Name, true);
    if (status != errSecSuccess) {
        return NULL;
    }
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef fmt = SecCopyCertString(SEC_COMMON_NAME_DESC_KEY);
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, summary.summary, summary.description);
            CFRelease(fmt);
            CFRelease(summary.summary);
            summary.summary = newSummary;
        }
        CFRelease(summary.description);
    }

    if (!summary.summary) {
        /* If we didn't find a suitable printable string in the subject at all, we try
           the first email address in the certificate instead. */
        CFArrayRef names = SecCertificateCopyRFC822Names(certificate);
        if (!names) {
            /* If we didn't find any email addresses in the certificate, we try finding
               a DNS name instead. */
            names = SecCertificateCopyDNSNames(certificate);
        }
        if (names) {
            summary.summary = CFArrayGetValueAtIndex(names, 0);
            CFRetain(summary.summary);
            CFRelease(names);
        }
    }

	return summary.summary;
}

CFStringRef SecCertificateCopyIssuerSummary(SecCertificateRef certificate) {
    struct Summary summary = {};
	OSStatus status = parseX501NameContent(&certificate->_issuer, &summary, obtainSummaryFromX501Name, true);
    if (status != errSecSuccess) {
        return NULL;
    }
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef fmt = SecCopyCertString(SEC_COMMON_NAME_DESC_KEY);
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, summary.summary, summary.description);
            CFRelease(fmt);
            CFRelease(summary.summary);
            summary.summary = newSummary;
        }
        CFRelease(summary.description);
    }

	return summary.summary;
}

/* Return the earliest date on which all certificates in this chain are still
   valid. */
static CFAbsoluteTime SecCertificateGetChainsLastValidity(
    SecCertificateRef certificate) {
    CFAbsoluteTime earliest = certificate->_notAfter;
#if 0
    while (certificate->_parent) {
        certificate = certificate->_parent;
        if (earliest > certificate->_notAfter)
            earliest = certificate->_notAfter;
    }
#endif

    return earliest;
}

/* Return the latest date on which all certificates in this chain will be
   valid. */
static CFAbsoluteTime SecCertificateGetChainsFirstValidity(
    SecCertificateRef certificate) {
    CFAbsoluteTime latest = certificate->_notBefore;
#if 0
    while (certificate->_parent) {
        certificate = certificate->_parent;
        if (latest < certificate->_notBefore)
            latest = certificate->_notBefore;
    }
#endif

    return latest;
}

bool SecCertificateIsValid(SecCertificateRef certificate,
	CFAbsoluteTime verifyTime) {
    return certificate && certificate->_notBefore <= verifyTime &&
		verifyTime <= certificate->_notAfter;
}

CFIndex SecCertificateVersion(SecCertificateRef certificate) {
	return certificate->_version + 1;
}

CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRef certificate) {
	return certificate->_notBefore;
}

CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate) {
	return certificate->_notAfter;
}

CFMutableArrayRef SecCertificateCopySummaryProperties(
    SecCertificateRef certificate, CFAbsoluteTime verifyTime) {
    CFAllocatorRef allocator = CFGetAllocator(certificate);
    CFMutableArrayRef summary = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);
    bool localized = true;

    /* First we put the subject summary name. */
    CFStringRef ssummary = SecCertificateCopySubjectSummary(certificate);
    if (ssummary) {
        appendProperty(summary, kSecPropertyTypeTitle,
            NULL, NULL, ssummary, localized);
        CFRelease(ssummary);
    }

    /* Let see if this certificate is currently valid. */
    CFStringRef label;
    CFAbsoluteTime when;
    CFStringRef message;
    CFStringRef ptype;
    if (verifyTime > certificate->_notAfter) {
        label = SEC_EXPIRED_KEY;
        when = certificate->_notAfter;
        ptype = kSecPropertyTypeError;
        message = SEC_CERT_EXPIRED_KEY;
    } else if (certificate->_notBefore > verifyTime) {
        label = SEC_VALID_FROM_KEY;
        when = certificate->_notBefore;
        ptype = kSecPropertyTypeError;
        message = SEC_CERT_NOT_YET_VALID_KEY;
    } else {
        CFAbsoluteTime last = SecCertificateGetChainsLastValidity(certificate);
        CFAbsoluteTime first = SecCertificateGetChainsFirstValidity(certificate);
        if (verifyTime > last) {
            label = SEC_EXPIRED_KEY;
            when = last;
            ptype = kSecPropertyTypeError;
            message = SEC_ISSUER_EXPIRED_KEY;
        } else if (verifyTime < first) {
            label = SEC_VALID_FROM_KEY;
            when = first;
            ptype = kSecPropertyTypeError;
            message = SEC_ISSR_NOT_YET_VALID_KEY;
        } else {
            label = SEC_EXPIRES_KEY;
            when = certificate->_notAfter;
            ptype = kSecPropertyTypeSuccess;
            message = SEC_CERT_VALID_KEY;
        }
    }

    appendDateProperty(summary, label, when, localized);
    CFStringRef lmessage = SecCopyCertString(message);
    appendProperty(summary, ptype, NULL, NULL, lmessage, localized);
    CFRelease(lmessage);

	return summary;
}

CFArrayRef SecCertificateCopyLegacyProperties(SecCertificateRef certificate) {
    /*
       This function replicates the content returned by SecCertificateCopyProperties
       prior to 10.12.4, providing stable return values for SecCertificateCopyValues.
       Unlike SecCertificateCopyProperties, it does not cache the result and
       assumes the caller will do so.
    */
    CFAllocatorRef allocator = CFGetAllocator(certificate);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator,
        0, &kCFTypeArrayCallBacks);

    /* Subject Name */
    CFArrayRef subject_plist = createPropertiesForX501NameContent(allocator,
        &certificate->_subject, false);
    appendProperty(properties, kSecPropertyTypeSection, CFSTR("Subject Name"),
        NULL, subject_plist, false);
    CFRelease(subject_plist);

    /* Issuer Name */
    CFArrayRef issuer_plist = createPropertiesForX501NameContent(allocator,
        &certificate->_issuer, false);
    appendProperty(properties, kSecPropertyTypeSection, CFSTR("Issuer Name"),
        NULL, issuer_plist, false);
    CFRelease(issuer_plist);

    /* Version */
    CFStringRef versionString = CFStringCreateWithFormat(allocator,
        NULL, CFSTR("%d"), certificate->_version + 1);
    appendProperty(properties, kSecPropertyTypeString, CFSTR("Version"),
        NULL, versionString, false);
    CFRelease(versionString);

    /* Serial Number */
    if (certificate->_serialNum.length) {
        appendIntegerProperty(properties, CFSTR("Serial Number"),
            &certificate->_serialNum, false);
    }

    /* Signature Algorithm */
    appendAlgorithmProperty(properties, CFSTR("Signature Algorithm"),
        &certificate->_tbsSigAlg, false);

    /* Validity dates */
    appendDateProperty(properties, CFSTR("Not Valid Before"), certificate->_notBefore, false);
    appendDateProperty(properties, CFSTR("Not Valid After"), certificate->_notAfter, false);

    if (certificate->_subjectUniqueID.length) {
        appendDataProperty(properties, CFSTR("Subject Unique ID"),
            NULL, &certificate->_subjectUniqueID, false);
    }
    if (certificate->_issuerUniqueID.length) {
        appendDataProperty(properties, CFSTR("Issuer Unique ID"),
            NULL, &certificate->_issuerUniqueID, false);
    }

    /* Public Key Algorithm */
    appendAlgorithmProperty(properties, CFSTR("Public Key Algorithm"),
        &certificate->_algId, false);

    /* Public Key Data */
    appendDataProperty(properties, CFSTR("Public Key Data"),
        NULL, &certificate->_pubKeyDER, false);

    /* Signature */
    appendDataProperty(properties, CFSTR("Signature"),
        NULL, &certificate->_signature, false);

    /* Extensions */
    CFIndex ix;
    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        appendExtension(properties, &certificate->_extensions[ix], false);
    }

    /* Fingerprints */
    appendFingerprintsProperty(properties, CFSTR("Fingerprints"), certificate, false);

    return properties;
}

static CFArrayRef CopyProperties(SecCertificateRef certificate, Boolean localized) {
	if (!certificate->_properties) {
		CFAllocatorRef allocator = CFGetAllocator(certificate);
		CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
			&kCFTypeArrayCallBacks);
        require_quiet(properties, out);

        /* First we put the Subject Name in the property list. */
        CFArrayRef subject_plist = createPropertiesForX501NameContent(allocator,
                                                                      &certificate->_subject,
                                                                      localized);
        if (subject_plist) {
            appendProperty(properties, kSecPropertyTypeSection,
                           SEC_SUBJECT_NAME_KEY, NULL, subject_plist, localized);
        }
        CFReleaseNull(subject_plist);

        /* Next we put the Issuer Name in the property list. */
        CFArrayRef issuer_plist = createPropertiesForX501NameContent(allocator,
                                                                     &certificate->_issuer,
                                                                     localized);
        if (issuer_plist) {
            appendProperty(properties, kSecPropertyTypeSection,
                           SEC_ISSUER_NAME_KEY, NULL, issuer_plist, localized);
        }
        CFReleaseNull(issuer_plist);

        /* Version */
        CFStringRef fmt = SecCopyCertString(SEC_CERT_VERSION_VALUE_KEY);
        CFStringRef versionString = NULL;
        if (fmt) {
            versionString = CFStringCreateWithFormat(allocator, NULL, fmt,
                                                     certificate->_version + 1);
        }
        CFReleaseNull(fmt);
        if (versionString) {
            appendProperty(properties, kSecPropertyTypeString,
                           SEC_VERSION_KEY, NULL, versionString, localized);
        }
        CFReleaseNull(versionString);

		/* Serial Number */
        appendSerialNumberProperty(properties, SEC_SERIAL_NUMBER_KEY, &certificate->_serialNum, localized);

        /* Validity dates. */
        appendValidityPeriodProperty(properties, SEC_VALIDITY_PERIOD_KEY, certificate, localized);

        if (certificate->_subjectUniqueID.length) {
            appendDataProperty(properties, SEC_SUBJECT_UNIQUE_ID_KEY, NULL,
                &certificate->_subjectUniqueID, localized);
        }
        if (certificate->_issuerUniqueID.length) {
            appendDataProperty(properties, SEC_ISSUER_UNIQUE_ID_KEY, NULL,
                &certificate->_issuerUniqueID, localized);
        }

        appendPublicKeyProperty(properties, SEC_PUBLIC_KEY_KEY, certificate, localized);

        CFIndex ix;
        for (ix = 0; ix < certificate->_extensionCount; ++ix) {
            appendExtension(properties, &certificate->_extensions[ix], localized);
        }

        /* Signature */
        appendSignatureProperty(properties, SEC_SIGNATURE_KEY, certificate, localized);

        appendFingerprintsProperty(properties, SEC_FINGERPRINTS_KEY, certificate, localized);

		certificate->_properties = properties;
	}

out:
    CFRetainSafe(certificate->_properties);
	return certificate->_properties;
}

CFArrayRef SecCertificateCopyProperties(SecCertificateRef certificate) {
    /*
       Wrapper function which defaults to localized string properties
       for compatibility with prior releases.
    */
    return CopyProperties(certificate, true);
}

CFArrayRef SecCertificateCopyLocalizedProperties(SecCertificateRef certificate, Boolean localized) {
    /*
       Wrapper function which permits caller to specify whether
       localized string properties are used.
    */
    return CopyProperties(certificate, localized);
}

/* Unified serial number API */
CFDataRef SecCertificateCopySerialNumberData(
	SecCertificateRef certificate,
	CFErrorRef *error)
{
	if (!certificate) {
		if (error) {
			*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
		}
		return NULL;
	}
	if (certificate->_serialNumber) {
		CFRetain(certificate->_serialNumber);
	}
	return certificate->_serialNumber;
}

#if TARGET_OS_OSX && TARGET_CPU_ARM64
/* force this implementation to be _SecCertificateCopySerialNumber on arm64 macOS.
   note: the legacy function in SecCertificate.cpp is now _SecCertificateCopySerialNumber$LEGACYMAC
   when both TARGET_OS_OSX and TARGET_CPU_ARM64 are true.
 */
extern CFDataRef SecCertificateCopySerialNumber_ios(SecCertificateRef certificate) __asm("_SecCertificateCopySerialNumber");
CFDataRef SecCertificateCopySerialNumber_ios(SecCertificateRef certificate) {
    return SecCertificateCopySerialNumberData(certificate, NULL);
}
#endif

#if !TARGET_OS_OSX
CFDataRef SecCertificateCopySerialNumber(SecCertificateRef certificate)
{
	return SecCertificateCopySerialNumberData(certificate, NULL);
}
#endif

CFDataRef SecCertificateGetNormalizedIssuerContent(
    SecCertificateRef certificate) {
    return certificate->_normalizedIssuer;
}

CFDataRef SecCertificateGetNormalizedSubjectContent(
    SecCertificateRef certificate) {
    return certificate->_normalizedSubject;
}

/* Verify that certificate was signed by issuerKey. */
OSStatus SecCertificateIsSignedBy(SecCertificateRef certificate,
    SecKeyRef issuerKey) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    /* Setup algId in SecAsn1AlgId format. */
    SecAsn1AlgId algId;
#pragma clang diagnostic pop
    algId.algorithm.Length = certificate->_tbsSigAlg.oid.length;
    algId.algorithm.Data = certificate->_tbsSigAlg.oid.data;
    algId.parameters.Length = certificate->_tbsSigAlg.params.length;
    algId.parameters.Data = certificate->_tbsSigAlg.params.data;

    /* RFC5280 4.1.1.2, 4.1.2.3 requires the actual signature algorithm
       must match the specified algorithm in the TBSCertificate. */
	bool sigAlgMatch = DEROidCompare(&certificate->_sigAlg.oid,
                         &certificate->_tbsSigAlg.oid);
    if (!sigAlgMatch) {
        secwarning("Signature algorithm mismatch in certificate (see RFC5280 4.1.1.2)");
    }

    CFErrorRef error = NULL;
    if (!sigAlgMatch ||
        !SecVerifySignatureWithPublicKey(issuerKey, &algId,
        certificate->_tbs.data, certificate->_tbs.length,
        certificate->_signature.data, certificate->_signature.length, &error))
    {
#if !defined(NDEBUG)
        secdebug("verify", "signature verify failed: %" PRIdOSStatus, (error) ? (OSStatus)CFErrorGetCode(error) : errSecNotSigner);
#endif
        CFReleaseSafe(error);
        return errSecNotSigner;
    }

    return errSecSuccess;
}

const DERItem * SecCertificateGetSubjectAltName(SecCertificateRef certificate) {
    if (!certificate->_subjectAltName) {
        return NULL;
    }
    return &certificate->_subjectAltName->extnValue;
}

/* Convert IPv4 address string to canonical data format (4 bytes) */
static bool convertIPv4Address(CFStringRef name, CFDataRef *dataIP) {
    /* IPv4: 4 octets in decimal separated by dots. */
    bool result = false;
    /* Check size */
    if (CFStringGetLength(name) < 7 || /* min size is #.#.#.# */
        CFStringGetLength(name) > 15) { /* max size is ###.###.###.### */
        return false;
    }

    CFCharacterSetRef allowed = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("0123456789."));
    CFCharacterSetRef disallowed = CFCharacterSetCreateInvertedSet(NULL, allowed);
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(NULL, name, CFSTR("."));
    CFIndex i, count = (parts) ? CFArrayGetCount(parts) : 0;

    /* Check character set */
    if (CFStringFindCharacterFromSet(name, disallowed,
                CFRangeMake(0, CFStringGetLength(name)),
                kCFCompareForcedOrdering, NULL)) {
        goto out;
    }

    /* Check number of labels */
    if (CFArrayGetCount(parts) != 4) {
        goto out;
    }

    /* Check each label and convert */
    for (i = 0; i < count; i++) {
        CFStringRef octet = (CFStringRef) CFArrayGetValueAtIndex(parts, i);
        char *cString = CFStringToCString(octet);
        uint32_t value = atoi(cString);
        free(cString);
        if (value > 255) {
            goto out;
        } else {
            uint8_t byte = value;
            CFDataAppendBytes(data, &byte, 1);
        }
    }
    result = true;
    if (dataIP) {
        *dataIP = (CFDataRef) CFRetain(data);
    }

out:
    CFReleaseNull(data);
    CFReleaseNull(parts);
    CFReleaseNull(allowed);
    CFReleaseNull(disallowed);
    return result;
}

/* Convert IPv6 address string to canonical data format (16 bytes) */
static bool convertIPv6Address(CFStringRef name, CFDataRef *dataIP) {
    /* IPv6: 8 16-bit fields with colon delimiters. */
    /* Note: we don't support conversion of hybrid IPv4-mapped addresses here. */
    bool result = false;
    CFMutableStringRef addr = NULL;
    CFIndex length = (name) ? CFStringGetLength(name) : 0;
    /* Sanity check size */
    if (length < 2 ||  /* min size is '::' */
        length > 41) { /* max size is '[####:####:####:####:####:####:####:####]' */
        return result;
    }
    /* Remove literal brackets, if present */
    if (CFStringHasPrefix(name, CFSTR("[")) && CFStringHasSuffix(name, CFSTR("]"))) {
        CFStringRef tmpName = CFStringCreateWithSubstring(NULL, name, CFRangeMake(1, length-2));
        if (tmpName) {
            addr = CFStringCreateMutableCopy(NULL, 0, tmpName);
            CFRelease(tmpName);
        }
    }
    if (NULL == addr) {
        addr = CFStringCreateMutableCopy(NULL, 0, name);
    }
    CFStringUppercase(addr, CFLocaleGetSystem());

    CFCharacterSetRef allowed = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("0123456789ABCDEF:"));
    CFCharacterSetRef disallowed = CFCharacterSetCreateInvertedSet(NULL, allowed);
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(NULL, addr, CFSTR(":"));
    CFIndex i, count = (parts) ? CFArrayGetCount(parts) : 0;

    /* Check character set */
    if (CFStringFindCharacterFromSet(addr, disallowed,
                CFRangeMake(0, CFStringGetLength(addr)),
                kCFCompareForcedOrdering, NULL)) {
        goto out;
    }

    /* Check number of fields (no fewer than 3, no more than 8) */
    if (CFArrayGetCount(parts) < 3 || CFArrayGetCount(parts) > 8) {
        goto out;
    }

    /* Check each field and convert to network-byte-order value */
    for (i = 0; i < count; i++) {
        uint16_t svalue = 0;
        CFStringRef fieldValue = (CFStringRef) CFArrayGetValueAtIndex(parts, i);
        char *cString = CFStringToCString(fieldValue);
        length = (cString) ? strlen(cString) : 0;
        if (length == 0) {
            /* empty value indicates one or more zeros in the address */
            if (i == 0 || i == count-1) { /* leading or trailing part of '::' */
                CFDataAppendBytes(data, (const UInt8 *)&svalue, 2);
            } else { /* determine how many fields are missing, then zero-fill */
                CFIndex z, missing = (8 - count) + 1;
                for (z = 0; z < missing; z++) {
                    CFDataAppendBytes(data, (const UInt8 *)&svalue, 2);
                }
            }
        } else if (length <= 4) {
            /* valid field value is 4 characters or less */
            unsigned long value = strtoul(cString, NULL, 16);
            svalue = htons(value & 0xFFFF);
            CFDataAppendBytes(data, (const UInt8 *)&svalue, 2);
        }
        free(cString);
    }
    if (CFDataGetLength(data) != IPv6ADDRLEN) {
        goto out; /* after expansion, data must be exactly 16 bytes */
    }

    result = true;
    if (dataIP) {
        *dataIP = (CFDataRef) CFRetain(data);
    }

out:
    CFReleaseNull(data);
    CFReleaseNull(parts);
    CFReleaseNull(allowed);
    CFReleaseNull(disallowed);
    CFReleaseNull(addr);
    return result;
}

static bool convertIPAddress(CFStringRef string, CFDataRef *dataIP) {
    if (NULL == string) {
        return false;
    }
    if (convertIPv4Address(string, dataIP) ||
        convertIPv6Address(string, dataIP)) {
        return true;
    }
    return false;
}

bool SecFrameworkIsIPAddress(CFStringRef string) {
    return convertIPAddress(string, NULL);
}

CFDataRef SecFrameworkCopyIPAddressData(CFStringRef string) {
    CFDataRef data = NULL;
    if (!convertIPAddress(string, &data)) {
        return NULL;
    }
    return data;
}

static OSStatus appendIPAddressesFromGeneralNames(void *context, SecCEGeneralNameType gnType, const DERItem *generalName) {
    CFMutableArrayRef ipAddresses = (CFMutableArrayRef)context;
    if (gnType == GNT_IPAddress) {
        if (generalName->length == IPv4ADDRLEN || generalName->length == IPv6ADDRLEN) {
            CFDataRef address = CFDataCreate(NULL, generalName->data, generalName->length);
            CFArrayAppendValue(ipAddresses, address);
            CFReleaseNull(address);
        } else {
            return errSecInvalidCertificate;
        }
    }
    return errSecSuccess;
}

CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRef certificate) {
    CFArrayRef ipAddresses = SecCertificateCopyIPAddressDatas(certificate);
    if (!ipAddresses) {
        return NULL;
    }

    // Convert data IP addresses to strings
    __block CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayForEach(ipAddresses, ^(const void *value) {
        CFDataRef address = (CFDataRef)value;
        DERItem der_address = { (unsigned char *)CFDataGetBytePtr(address), CFDataGetLength(address) };
        CFStringRef string = copyIPAddressContentDescription(NULL, &der_address);
        if (string) {
            CFArrayAppendValue(result, string);
            CFRelease(string);
        }
    });
    CFReleaseNull(ipAddresses);
    if (CFArrayGetCount(result) == 0) {
        CFReleaseNull(result);
    }

    return result;
}

CFArrayRef SecCertificateCopyIPAddressDatas(SecCertificateRef certificate) {
    /* These can only exist in the subject alt name. */
    if (!certificate->_subjectAltName)
        return NULL;

    CFMutableArrayRef ipAddresses = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    OSStatus status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
        ipAddresses, appendIPAddressesFromGeneralNames);
    if (status || CFArrayGetCount(ipAddresses) == 0) {
        CFRelease(ipAddresses);
        ipAddresses = NULL;
    }
    return ipAddresses;
}

static OSStatus appendDNSNamesFromGeneralNames(void *context, SecCEGeneralNameType gnType,
	const DERItem *generalName) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (gnType == GNT_DNSName) {
		CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault,
			generalName->data, generalName->length,
			kCFStringEncodingUTF8, FALSE);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

/* Return true if the passed in string matches the
   Preferred name syntax from sections 2.3.1. in RFC 1035.
   With the added check that we disallow empty dns names.
   Also in order to support wildcard DNSNames we allow for the '*'
   character anywhere in a dns component where we currently allow
   a letter.

	<domain> ::= <subdomain> | " "

	<subdomain> ::= <label> | <subdomain> "." <label>

	<label> ::= <letter> [ [ <ldh-str> ] <let-dig> ]

    RFC 3696 redefined labels as:
    <label> ::= <let-dig> [ [ <ldh-str> ] <let-dig> ]
    with the caveat that the highest-level labels is never all-numeric.

	<ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>

	<let-dig-hyp> ::= <let-dig> | "-"

	<let-dig> ::= <letter> | <digit>

	<letter> ::= any one of the 52 alphabetic characters A through Z in
	upper case and a through z in lower case

	<digit> ::= any one of the ten digits 0 through 9
   */
bool SecFrameworkIsDNSName(CFStringRef string) {
    CFStringInlineBuffer buf = {};
	CFIndex ix, labelLength = 0, length = CFStringGetLength(string);
	/* From RFC 1035 2.3.4. Size limits:
	   labels          63 octets or less
	   names           255 octets or less */
	require_quiet(length <= 255, notDNS);
	CFRange range = { 0, length };
	CFStringInitInlineBuffer(string, &buf, range);
	enum {
		kDNSStateInital,
		kDNSStateAfterDot,
		kDNSStateAfterAlpha,
		kDNSStateAfterDigit,
		kDNSStateAfterDash,
	} state = kDNSStateInital;
    Boolean labelHasAlpha = false;

	for (ix = 0; ix < length; ++ix) {
		UniChar ch = CFStringGetCharacterFromInlineBuffer(&buf, ix);
		labelLength++;
		if (ch == '.') {
			require_quiet(labelLength <= 64 &&
				(state == kDNSStateAfterAlpha || state == kDNSStateAfterDigit),
				notDNS);
			state = kDNSStateAfterDot;
            labelHasAlpha = false;
			labelLength = 0;
		} else if (('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z')  ||
			ch == '*') {
			state = kDNSStateAfterAlpha;
            labelHasAlpha = true;
		} else if ('0' <= ch && ch <= '9') {
			state = kDNSStateAfterDigit;
		} else if (ch == '-') {
			require_quiet(state == kDNSStateAfterAlpha ||
				state == kDNSStateAfterDigit ||
				state == kDNSStateAfterDash, notDNS);
			state = kDNSStateAfterDash;
		} else {
			goto notDNS;
		}
	}

	/* We don't allow a dns name to end in a dot or dash.  */
	require_quiet(labelLength <= 63 &&
		(state == kDNSStateAfterAlpha || state == kDNSStateAfterDigit),
		notDNS);

    /* Additionally, the rightmost label must have letters in it. */
    require_quiet(labelHasAlpha == true, notDNS);

	return true;
notDNS:
	return false;
}

static OSStatus appendDNSNamesFromX501Name(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidCommonName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true, localized);
		if (string) {
			if (SecFrameworkIsDNSName(string)) {
				/* We found a common name that is formatted like a valid
				   dns name. */
				CFArrayAppendValue(dnsNames, string);
			}
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

static CF_RETURNS_RETAINED CFArrayRef filterIPAddresses(CFArrayRef CF_CONSUMED dnsNames)
{
    __block CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayForEach(dnsNames, ^(const void *value) {
        CFStringRef name = (CFStringRef)value;
        if (!SecFrameworkIsIPAddress(name)) {
            CFArrayAppendValue(result, name);
        }
    });
    CFReleaseNull(dnsNames);
    if (CFArrayGetCount(result) == 0) {
        CFReleaseNull(result);
    }

    return result;
}

CFArrayRef SecCertificateCopyDNSNamesFromSAN(SecCertificateRef certificate) {
    CFMutableArrayRef dnsNames = CFArrayCreateMutable(kCFAllocatorDefault,
                                                      0, &kCFTypeArrayCallBacks);
    if (certificate->_subjectAltName) {
        if (SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
                                            dnsNames, appendDNSNamesFromGeneralNames) != errSecSuccess) {
            CFReleaseNull(dnsNames);
            return NULL;
        }
    }

    /* appendDNSNamesFromGeneralNames allows IP addresses, we don't want those for this function */
    return filterIPAddresses(dnsNames);
}

/* Not everything returned by this function is going to be a proper DNS name,
   we also return the certificates common name entries from the subject,
   assuming they look like dns names as specified in RFC 1035.

   To preserve bug for bug compatibility, we can't use SecCertificateCopyDNSNamesFromSAN
   because that function filters out IP Addresses. This function is Private, but
   SecCertificateCopyValues uses it and that's Public. */
CFArrayRef SecCertificateCopyDNSNames(SecCertificateRef certificate) {
    /* RFC 2818 section 3.1.  Server Identity
     [...]
     If a subjectAltName extension of type dNSName is present, that MUST
     be used as the identity. Otherwise, the (most specific) Common Name
     field in the Subject field of the certificate MUST be used. Although
     the use of the Common Name is existing practice, it is deprecated and
     Certification Authorities are encouraged to use the dNSName instead.
     [...]

     This implies that if we found 1 or more DNSNames in the
     subjectAltName, we should not use the Common Name of the subject as
     a DNSName.
     */

    /* return SAN DNS names if they exist */
    if (certificate->_subjectAltName) {
        CFMutableArrayRef sanNames = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        OSStatus status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
                                                          sanNames, appendDNSNamesFromGeneralNames);
        if (status == errSecSuccess && sanNames && CFArrayGetCount(sanNames) > 0) {
            return sanNames;
        }
        CFReleaseNull(sanNames);
    }

    /* fall back to return DNS names in the Common Name */
    CFMutableArrayRef dnsNames = CFArrayCreateMutable(kCFAllocatorDefault,
                                                      0, &kCFTypeArrayCallBacks);
    OSStatus status = parseX501NameContent(&certificate->_subject, dnsNames,
                                           appendDNSNamesFromX501Name, true);
    if (status || CFArrayGetCount(dnsNames) == 0) {
        CFReleaseNull(dnsNames);
    }
    return dnsNames;
}

static OSStatus appendRFC822NamesFromGeneralNames(void *context,
	SecCEGeneralNameType gnType, const DERItem *generalName) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (gnType == GNT_RFC822Name) {
		CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault,
			generalName->data, generalName->length,
			kCFStringEncodingASCII, FALSE);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

static OSStatus appendRFC822NamesFromX501Name(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidEmailAddress)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true, localized);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRef certificate) {
	/* These can exist in the subject alt name or in the subject. */
	CFMutableArrayRef rfc822Names = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = errSecSuccess;
	if (certificate->_subjectAltName) {
		status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
			rfc822Names, appendRFC822NamesFromGeneralNames);
	}
	if (!status) {
		status = parseX501NameContent(&certificate->_subject, rfc822Names,
			appendRFC822NamesFromX501Name, true);
	}
	if (status || CFArrayGetCount(rfc822Names) == 0) {
		CFRelease(rfc822Names);
		rfc822Names = NULL;
	}
	return rfc822Names;
}

OSStatus SecCertificateCopyEmailAddresses(SecCertificateRef certificate, CFArrayRef * __nonnull CF_RETURNS_RETAINED emailAddresses) {
    if (!certificate || !emailAddresses) {
        return errSecParam;
    }
    *emailAddresses = SecCertificateCopyRFC822Names(certificate);
    if (*emailAddresses == NULL) {
        *emailAddresses = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    }
    return errSecSuccess;
}

CFArrayRef SecCertificateCopyRFC822NamesFromSubject(SecCertificateRef certificate) {
    CFMutableArrayRef rfc822Names = CFArrayCreateMutable(kCFAllocatorDefault,
                                                         0, &kCFTypeArrayCallBacks);
    OSStatus status = parseX501NameContent(&certificate->_subject, rfc822Names,
                                      appendRFC822NamesFromX501Name, true);
    if (status || CFArrayGetCount(rfc822Names) == 0) {
        CFRelease(rfc822Names);
        rfc822Names = NULL;
    }
    return rfc822Names;
}

static OSStatus appendCommonNamesFromX501Name(void *context,
    const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableArrayRef commonNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidCommonName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true, localized);
		if (string) {
            CFArrayAppendValue(commonNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyCommonNames(SecCertificateRef certificate) {
	CFMutableArrayRef commonNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status;
    status = parseX501NameContent(&certificate->_subject, commonNames,
        appendCommonNamesFromX501Name, true);
	if (status || CFArrayGetCount(commonNames) == 0) {
		CFRelease(commonNames);
		commonNames = NULL;
	}
	return commonNames;
}

OSStatus SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName)
{
    if (!certificate) {
        return errSecParam;
    }
    CFArrayRef commonNames = SecCertificateCopyCommonNames(certificate);
    if (!commonNames) {
        return errSecInternal;
    }

    if (commonName) {
        CFIndex count = CFArrayGetCount(commonNames);
        *commonName = CFRetainSafe(CFArrayGetValueAtIndex(commonNames, count-1));
    }
    CFReleaseSafe(commonNames);
    return errSecSuccess;
}

static OSStatus appendOrganizationFromX501Name(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableArrayRef organization = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidOrganizationName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true, localized);
		if (string) {
			CFArrayAppendValue(organization, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyOrganizationFromX501NameContent(const DERItem *nameContent) {
    CFMutableArrayRef organization = CFArrayCreateMutable(kCFAllocatorDefault,
                                                          0, &kCFTypeArrayCallBacks);
    OSStatus status;
    status = parseX501NameContent(nameContent, organization,
                                  appendOrganizationFromX501Name, true);
    if (status || CFArrayGetCount(organization) == 0) {
        CFRelease(organization);
        organization = NULL;
    }
    return organization;
}

CFArrayRef SecCertificateCopyOrganization(SecCertificateRef certificate) {
    return SecCertificateCopyOrganizationFromX501NameContent(&certificate->_subject);
}

static OSStatus appendOrganizationalUnitFromX501Name(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableArrayRef organizationalUnit = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidOrganizationalUnitName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true, localized);
		if (string) {
			CFArrayAppendValue(organizationalUnit, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyOrganizationalUnit(SecCertificateRef certificate) {
	CFMutableArrayRef organizationalUnit = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status;
	status = parseX501NameContent(&certificate->_subject, organizationalUnit,
        appendOrganizationalUnitFromX501Name, true);
	if (status || CFArrayGetCount(organizationalUnit) == 0) {
		CFRelease(organizationalUnit);
		organizationalUnit = NULL;
	}
	return organizationalUnit;
}

static OSStatus appendCountryFromX501Name(void *context,
    const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
    CFMutableArrayRef countries = (CFMutableArrayRef)context;
    if (DEROidCompare(type, &oidCountryName)) {
        CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
                                                     value, true, localized);
        if (string) {
            CFArrayAppendValue(countries, string);
            CFRelease(string);
        } else {
            return errSecInvalidCertificate;
        }
    }
    return errSecSuccess;
}

CFArrayRef SecCertificateCopyCountry(SecCertificateRef certificate) {
    CFMutableArrayRef countries = CFArrayCreateMutable(kCFAllocatorDefault,
                                                                0, &kCFTypeArrayCallBacks);
    OSStatus status;
    status = parseX501NameContent(&certificate->_subject, countries,
                                  appendCountryFromX501Name, true);
    if (status || CFArrayGetCount(countries) == 0) {
        CFRelease(countries);
        countries = NULL;
    }
    return countries;
}

typedef struct {
    DERItem *attributeOID;
    CFStringRef *result;
} ATV_Context;

static OSStatus copyAttributeValueFromX501Name(void *context, const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
    ATV_Context *ctx = (ATV_Context *)context;
    if (DEROidCompare(type, ctx->attributeOID)) {
        CFStringRef string = copyDERThingDescription(kCFAllocatorDefault, value, true, localized);
        if (string) {
            CFAssignRetained(*ctx->result, string);
        } else {
            return errSecInvalidCertificate;
        }
    }
    return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectAttributeValue(SecCertificateRef cert, DERItem *attributeOID) {
    CFStringRef result = NULL;
    ATV_Context context = {
        .attributeOID = attributeOID,
        .result = &result,
    };
    OSStatus status = parseX501NameContent(&cert->_subject, &context, copyAttributeValueFromX501Name, false);
    if (status) {
        CFReleaseNull(result);
    }
    return result;
}

const SecCEBasicConstraints *
SecCertificateGetBasicConstraints(SecCertificateRef certificate) {
	if (certificate->_basicConstraints.present)
		return &certificate->_basicConstraints;
	else
		return NULL;
}

CFArrayRef SecCertificateGetPermittedSubtrees(SecCertificateRef certificate) {
    return (certificate->_permittedSubtrees);
}

CFArrayRef SecCertificateGetExcludedSubtrees(SecCertificateRef certificate) {
    return (certificate->_excludedSubtrees);
}

const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraints(SecCertificateRef certificate) {
	if (certificate->_policyConstraints.present)
		return &certificate->_policyConstraints;
	else
		return NULL;
}

const SecCEPolicyMappings *
SecCertificateGetPolicyMappings(SecCertificateRef certificate) {
    if (certificate->_policyMappings.present) {
        return &certificate->_policyMappings;
    } else {
        return NULL;
    }
}

const SecCECertificatePolicies *
SecCertificateGetCertificatePolicies(SecCertificateRef certificate) {
	if (certificate->_certificatePolicies.present)
		return &certificate->_certificatePolicies;
	else
		return NULL;
}

const SecCEInhibitAnyPolicy *
SecCertificateGetInhibitAnyPolicySkipCerts(SecCertificateRef certificate) {
    if (certificate->_inhibitAnyPolicySkipCerts.present) {
        return &certificate->_inhibitAnyPolicySkipCerts;
    } else {
        return NULL;
    }
}

static OSStatus appendNTPrincipalNamesFromGeneralNames(void *context,
	SecCEGeneralNameType gnType, const DERItem *generalName) {
	CFMutableArrayRef ntPrincipalNames = (CFMutableArrayRef)context;
	if (gnType == GNT_OtherName) {
        DEROtherName on;
        DERReturn drtn = DERParseSequenceContent(generalName,
            DERNumOtherNameItemSpecs, DEROtherNameItemSpecs,
            &on, sizeof(on));
        require_noerr_quiet(drtn, badDER);
        if (DEROidCompare(&on.typeIdentifier, &oidMSNTPrincipalName)) {
            CFStringRef string;
            require_quiet(string = copyDERThingDescription(kCFAllocatorDefault,
                &on.value, true, true), badDER);
            CFArrayAppendValue(ntPrincipalNames, string);
            CFRelease(string);
		}
	}
	return errSecSuccess;

badDER:
    return errSecInvalidCertificate;

}

CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRef certificate) {
	CFMutableArrayRef ntPrincipalNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = errSecSuccess;
	if (certificate->_subjectAltName) {
		status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
			ntPrincipalNames, appendNTPrincipalNamesFromGeneralNames);
	}
	if (status || CFArrayGetCount(ntPrincipalNames) == 0) {
		CFRelease(ntPrincipalNames);
		ntPrincipalNames = NULL;
	}
	return ntPrincipalNames;
}

static OSStatus appendToRFC2253String(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableStringRef string = (CFMutableStringRef)context;
    /*
                    CN      commonName
                    L       localityName
                    ST      stateOrProvinceName
                    O       organizationName
                    OU      organizationalUnitName
                    C       countryName
                    STREET  streetAddress
                    DC      domainComponent
                    UID     userid
    */
    /* Prepend a + if this is not the first RDN in an RDN set.
       Otherwise prepend a , if this is not the first RDN. */
    if (rdnIX > 0)
        CFStringAppend(string, CFSTR("+"));
    else if (CFStringGetLength(string)) {
        CFStringAppend(string, CFSTR(","));
    }

    CFStringRef label, oid = NULL;
    /* @@@ Consider changing this to a dictionary lookup keyed by the
       decimal representation. */
	if (DEROidCompare(type, &oidCommonName)) {
        label = CFSTR("CN");
    } else if (DEROidCompare(type, &oidLocalityName)) {
        label = CFSTR("L");
    } else if (DEROidCompare(type, &oidStateOrProvinceName)) {
        label = CFSTR("ST");
    } else if (DEROidCompare(type, &oidOrganizationName)) {
        label = CFSTR("O");
    } else if (DEROidCompare(type, &oidOrganizationalUnitName)) {
        label = CFSTR("OU");
    } else if (DEROidCompare(type, &oidCountryName)) {
        label = CFSTR("C");
#if 0
    } else if (DEROidCompare(type, &oidStreetAddress)) {
        label = CFSTR("STREET");
    } else if (DEROidCompare(type, &oidDomainComponent)) {
        label = CFSTR("DC");
    } else if (DEROidCompare(type, &oidUserID)) {
        label = CFSTR("UID");
#endif
    } else {
        label = oid = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, type);
    }

    CFStringAppend(string, label);
    CFStringAppend(string, CFSTR("="));
    CFStringRef raw = NULL;
    if (!oid)
        raw = copyDERThingDescription(kCFAllocatorDefault, value, true, localized);

    if (raw) {
        /* Append raw to string while escaping:
           a space or "#" character occurring at the beginning of the string
           a space character occurring at the end of the string
           one of the characters ",", "+", """, "\", "<", ">" or ";"
        */
        CFStringInlineBuffer buffer = {};
        CFIndex ix, length = CFStringGetLength(raw);
        CFRange range = { 0, length };
        CFStringInitInlineBuffer(raw, &buffer, range);
        for (ix = 0; ix < length; ++ix) {
            UniChar ch = CFStringGetCharacterFromInlineBuffer(&buffer, ix);
            if (ch < 0x20) {
                CFStringAppendFormat(string, NULL, CFSTR("\\%02X"), ch);
            } else if (ch == ',' || ch == '+' || ch == '"' || ch == '\\' ||
                ch == '<' || ch == '>' || ch == ';' ||
                (ch == ' ' && (ix == 0 || ix == length - 1)) ||
                (ch == '#' && ix == 0)) {
                UniChar chars[] = { '\\', ch };
                CFStringAppendCharacters(string, chars, 2);
            } else {
                CFStringAppendCharacters(string, &ch, 1);
            }
        }
        CFRelease(raw);
    } else {
        /* Append the value in hex. */
        CFStringAppend(string, CFSTR("#"));
        DERSize ix;
        for (ix = 0; ix < value->length; ++ix)
            CFStringAppendFormat(string, NULL, CFSTR("%02X"), value->data[ix]);
    }

    CFReleaseSafe(oid);

	return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectString(SecCertificateRef certificate) {
	CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
	OSStatus status = parseX501NameContent(&certificate->_subject, string, appendToRFC2253String, true);
	if (status || CFStringGetLength(string) == 0) {
		CFRelease(string);
		string = NULL;
	}
	return string;
}

static OSStatus appendToCompanyNameString(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX, bool localized) {
	CFMutableStringRef string = (CFMutableStringRef)context;
    if (CFStringGetLength(string) != 0)
        return errSecSuccess;

    if (!DEROidCompare(type, &oidOrganizationName))
        return errSecSuccess;

    CFStringRef raw;
    raw = copyDERThingDescription(kCFAllocatorDefault, value, true, localized);
    if (!raw)
        return errSecSuccess;
    CFStringAppend(string, raw);
    CFRelease(raw);

	return errSecSuccess;
}

CFStringRef SecCertificateCopyCompanyName(SecCertificateRef certificate) {
	CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
	OSStatus status = parseX501NameContent(&certificate->_subject, string,
        appendToCompanyNameString, true);
	if (status || CFStringGetLength(string) == 0) {
		CFRelease(string);
		string = NULL;
	}
	return string;
}

CFDataRef SecCertificateCopyIssuerSequence(
    SecCertificateRef certificate) {
    return SecDERItemCopySequence(&certificate->_issuer);
}

CFDataRef SecCertificateCopySubjectSequence(
    SecCertificateRef certificate) {
    return SecDERItemCopySequence(&certificate->_subject);
}

CFDataRef SecCertificateCopyNormalizedIssuerSequence(SecCertificateRef certificate) {
    if (!certificate || !certificate->_normalizedIssuer) {
        return NULL;
    }
    return SecCopySequenceFromContent(certificate->_normalizedIssuer);
}

CFDataRef SecCertificateCopyNormalizedSubjectSequence(SecCertificateRef certificate) {
    if (!certificate || !certificate->_normalizedSubject) {
        return NULL;
    }
    return SecCopySequenceFromContent(certificate->_normalizedSubject);
}

const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithm(
	SecCertificateRef certificate) {
	return &certificate->_algId;
}

const DERItem *SecCertificateGetPublicKeyData(SecCertificateRef certificate) {
	return &certificate->_pubKeyDER;
}

#if TARGET_OS_OSX
#if TARGET_CPU_ARM64
/* force this implementation to be _SecCertificateCopyPublicKey on arm64 macOS.
   note: the legacy function in SecCertificate.cpp is now _SecCertificateCopyPublicKey$LEGACYMAC
   when both TARGET_OS_OSX and TARGET_CPU_ARM64 are true.
 */
extern __nullable SecKeyRef SecCertificateCopyPublicKey_ios(SecCertificateRef certificate) __asm("_SecCertificateCopyPublicKey");
#endif /* TARGET_CPU_ARM64 */
__nullable SecKeyRef SecCertificateCopyPublicKey_ios(SecCertificateRef certificate)
#else /* !TARGET_OS_OSX */
__nullable SecKeyRef SecCertificateCopyPublicKey(SecCertificateRef certificate)
#endif
{
    return SecCertificateCopyKey(certificate);
}

SecKeyRef SecCertificateCopyKey(SecCertificateRef certificate) {
    if (certificate->_pubKey == NULL) {
        const DERAlgorithmId *algId =
        SecCertificateGetPublicKeyAlgorithm(certificate);
        const DERItem *keyData = SecCertificateGetPublicKeyData(certificate);
        const DERItem *params = NULL;
        if (algId->params.length != 0) {
            params = &algId->params;
        }
        SecAsn1Oid oid1 = { .Data = algId->oid.data, .Length = algId->oid.length };
        SecAsn1Item params1 = {
            .Data = params ? params->data : NULL,
            .Length = params ? params->length : 0
        };
        SecAsn1Item keyData1 = {
            .Data = keyData ? keyData->data : NULL,
            .Length = keyData ? keyData->length : 0
        };
        certificate->_pubKey = SecKeyCreatePublicFromDER(kCFAllocatorDefault, &oid1, &params1,
                                                         &keyData1);
    }

    return CFRetainSafe(certificate->_pubKey);
}

static CFIndex SecCertificateGetPublicKeyAlgorithmIdAndSize(SecCertificateRef certificate, size_t *keySizeInBytes) {
    CFIndex keyAlgID = kSecNullAlgorithmID;
    size_t size = 0;

    SecKeyRef pubKey = NULL;
    require_quiet(certificate, out);
    require_quiet(pubKey = SecCertificateCopyKey(certificate) ,out);
    size = SecKeyGetBlockSize(pubKey);
    keyAlgID = SecKeyGetAlgorithmId(pubKey);

out:
    CFReleaseNull(pubKey);
    if (keySizeInBytes) { *keySizeInBytes = size; }
    return keyAlgID;
}

/*
 * Public keys in certificates may be considered "weak" or "strong" or neither
 * (that is, in between). Certificates using weak keys are not trusted at all.
 * Certificates using neither strong nor weak keys are only trusted in certain
 * contexts. SecPolicy and SecPolicyServer define the contexts by which we enforce
 * these (or stronger) key size trust policies.
 */
bool SecCertificateIsWeakKey(SecCertificateRef certificate) {
    if (!certificate) { return true; }

    bool weak = true;
    size_t size = 0;
    switch (SecCertificateGetPublicKeyAlgorithmIdAndSize(certificate, &size)) {
        case kSecRSAAlgorithmID:
            if (MIN_RSA_KEY_SIZE <= size) weak = false;
            break;
        case kSecECDSAAlgorithmID:
            if (MIN_EC_KEY_SIZE <= size) weak = false;
            break;
        default:
            weak = true;
    }
    return weak;
}

bool SecCertificateIsStrongKey(SecCertificateRef certificate) {
    if (!certificate) { return false; }

    bool strong = false;
    size_t size = 0;
    switch (SecCertificateGetPublicKeyAlgorithmIdAndSize(certificate, &size)) {
        case kSecRSAAlgorithmID:
            if (MIN_STRONG_RSA_KEY_SIZE <= size) strong = true;
            break;
        case kSecECDSAAlgorithmID:
            if (MIN_STRONG_EC_KEY_SIZE <= size) strong = true;
            break;
        default:
            strong = false;
    }
    return strong;
}

bool SecCertificateIsWeakHash(SecCertificateRef certificate) {
    if (!certificate) { return true; }
    SecSignatureHashAlgorithm certAlg = 0;
    certAlg = SecCertificateGetSignatureHashAlgorithm(certificate);
    if (certAlg == kSecSignatureHashAlgorithmUnknown ||
        certAlg == kSecSignatureHashAlgorithmMD2 ||
        certAlg == kSecSignatureHashAlgorithmMD4 ||
        certAlg == kSecSignatureHashAlgorithmMD5 ||
        certAlg == kSecSignatureHashAlgorithmSHA1) {
        return true;
    }
    return false;
}

bool SecCertificateIsAtLeastMinKeySize(SecCertificateRef certificate,
                                       CFDictionaryRef keySizes) {
    if (!certificate) { return false; }

    bool goodSize = false;
    size_t size = 0;
    CFNumberRef minSize;
    size_t minSizeInBits;
    switch (SecCertificateGetPublicKeyAlgorithmIdAndSize(certificate, &size)) {
        case kSecRSAAlgorithmID:
            if(CFDictionaryGetValueIfPresent(keySizes, kSecAttrKeyTypeRSA, (const void**)&minSize)
               && minSize && CFNumberGetValue(minSize, kCFNumberLongType, &minSizeInBits)) {
                if (size >= (size_t)(minSizeInBits+7)/8) goodSize = true;
            }
            break;
        case kSecECDSAAlgorithmID:
            if(CFDictionaryGetValueIfPresent(keySizes, kSecAttrKeyTypeEC, (const void**)&minSize)
               && minSize && CFNumberGetValue(minSize, kCFNumberLongType, &minSizeInBits)) {
                if (size >= (size_t)(minSizeInBits+7)/8) goodSize = true;
            }
            break;
        default:
            goodSize = false;
    }
    return goodSize;
}

CFDataRef SecCertificateGetSHA1Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_der.data) {
        return NULL;
    }
    if (!certificate->_sha1Digest) {
        certificate->_sha1Digest =
            SecSHA1DigestCreate(CFGetAllocator(certificate),
                certificate->_der.data, certificate->_der.length);
    }
    return certificate->_sha1Digest;
}

CFDataRef SecCertificateCopySHA256Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_der.data) {
        return NULL;
    }
    return SecSHA256DigestCreate(CFGetAllocator(certificate),
                                 certificate->_der.data, certificate->_der.length);
}

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRef certificate) {
    CFDataRef digest = NULL;
    CFDataRef issuer = SecCertificateCopyIssuerSequence(certificate);
    if (issuer) {
        digest = SecSHA1DigestCreate(kCFAllocatorDefault,
            CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
        CFRelease(issuer);
    }
    return digest;
}

CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_pubKeyDER.data) {
        return NULL;
    }
    return SecSHA1DigestCreate(CFGetAllocator(certificate),
        certificate->_pubKeyDER.data, certificate->_pubKeyDER.length);
}

static CFDataRef SecCertificateCopySPKIEncoded(SecCertificateRef certificate) {
    /* SPKI is saved without the tag/length by libDER, so we need to re-encode */
    if (!certificate || !certificate->_subjectPublicKeyInfo.data) {
        return NULL;
    }
    DERSize size = DERLengthOfItem(ASN1_CONSTR_SEQUENCE, certificate->_subjectPublicKeyInfo.length);
    if (size < certificate->_subjectPublicKeyInfo.length) {
        return NULL;
    }
    uint8_t *temp = malloc(size);
    if (!temp) {
        return NULL;
    }
    DERReturn drtn = DEREncodeItem(ASN1_CONSTR_SEQUENCE,
                                   certificate->_subjectPublicKeyInfo.length,
                                   certificate->_subjectPublicKeyInfo.data,
                                   temp, &size);
    CFDataRef encodedSPKI = NULL;
    if (drtn == DR_Success) {
        encodedSPKI = CFDataCreate(NULL, temp, size);
    }
    free(temp);
    return encodedSPKI;
}

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA1Digest(SecCertificateRef certificate) {
    CFDataRef encodedSPKI = SecCertificateCopySPKIEncoded(certificate);
    if (!encodedSPKI) { return NULL; }
    CFDataRef hash = SecSHA1DigestCreate(CFGetAllocator(certificate),
                                         CFDataGetBytePtr(encodedSPKI),
                                         CFDataGetLength(encodedSPKI));
    CFReleaseNull(encodedSPKI);
    return hash;
}

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA256Digest(SecCertificateRef certificate) {
    CFDataRef encodedSPKI = SecCertificateCopySPKIEncoded(certificate);
    if (!encodedSPKI) { return NULL; }
    CFDataRef hash = SecSHA256DigestCreate(CFGetAllocator(certificate),
                                           CFDataGetBytePtr(encodedSPKI),
                                           CFDataGetLength(encodedSPKI));
    CFReleaseNull(encodedSPKI);
    return hash;
}

CFTypeRef SecCertificateCopyKeychainItem(SecCertificateRef certificate)
{
	if (!certificate) {
		return NULL;
	}
	CFRetainSafe(certificate->_keychain_item);
	return certificate->_keychain_item;
}

CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate) {
	if (!certificate) {
		return NULL;
	}
	if (!certificate->_authorityKeyID &&
		certificate->_authorityKeyIdentifier.length) {
		certificate->_authorityKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_authorityKeyIdentifier.data,
			certificate->_authorityKeyIdentifier.length);
	}

    return certificate->_authorityKeyID;
}

CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate) {
	if (!certificate) {
		return NULL;
	}
	if (!certificate->_subjectKeyID &&
		certificate->_subjectKeyIdentifier.length) {
		certificate->_subjectKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_subjectKeyIdentifier.data,
			certificate->_subjectKeyIdentifier.length);
	}

    return certificate->_subjectKeyID;
}

CFArrayRef SecCertificateGetCRLDistributionPoints(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_crlDistributionPoints;
}

CFArrayRef SecCertificateGetOCSPResponders(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_ocspResponders;
}

CFArrayRef SecCertificateGetCAIssuers(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_caIssuers;
}

bool SecCertificateHasCriticalSubjectAltName(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
    return certificate->_subjectAltName &&
        certificate->_subjectAltName->critical;
}

bool SecCertificateHasSubject(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
	/* Since the _subject field is the content of the subject and not the
	   whole thing, we can simply check for a 0 length subject here. */
	return certificate->_subject.length != 0;
}

bool SecCertificateHasUnknownCriticalExtension(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
    return certificate->_foundUnknownCriticalExtension;
}

/* Private API functions. */
void SecCertificateShow(SecCertificateRef certificate) {
	check(certificate);
	fprintf(stderr, "SecCertificate instance %p:\n", certificate);
		fprintf(stderr, "\n");
}

#ifndef STANDALONE
CFDictionaryRef SecCertificateCopyAttributeDictionary(
	SecCertificateRef certificate) {
	if (!SecCertificateIsCertificate(certificate)) {
		return NULL;
	}
	CFAllocatorRef allocator = CFGetAllocator(certificate);
	CFNumberRef certificateType = NULL;
	CFNumberRef certificateEncoding = NULL;
	CFStringRef label = NULL;
	CFStringRef alias = NULL;
	CFDataRef skid = NULL;
	CFDataRef pubKeyDigest = NULL;
	CFDataRef certData = NULL;
	CFDictionaryRef dict = NULL;

	DICT_DECLARE(11);

	/* CSSM_CERT_X_509v1, CSSM_CERT_X_509v2 or CSSM_CERT_X_509v3 */
	SInt32 ctv = certificate->_version + 1;
	SInt32 cev = 3; /* CSSM_CERT_ENCODING_DER */
	certificateType = CFNumberCreate(allocator, kCFNumberSInt32Type, &ctv);
	require_quiet(certificateType != NULL, out);
	certificateEncoding = CFNumberCreate(allocator, kCFNumberSInt32Type, &cev);
	require_quiet(certificateEncoding != NULL, out);
	certData = SecCertificateCopyData(certificate);
	require_quiet(certData != NULL, out);
	skid = SecCertificateGetSubjectKeyID(certificate);
	require_quiet(certificate->_pubKeyDER.data != NULL && certificate->_pubKeyDER.length > 0, out);
	pubKeyDigest = SecSHA1DigestCreate(allocator, certificate->_pubKeyDER.data,
		certificate->_pubKeyDER.length);
	require_quiet(pubKeyDigest != NULL, out);
#if 0
	/* We still need to figure out how to deal with multi valued attributes. */
	alias = SecCertificateCopyRFC822Names(certificate);
	label = SecCertificateCopySubjectSummary(certificate);
#else
	alias = NULL;
	label = NULL;
#endif

	DICT_ADDPAIR(kSecClass, kSecClassCertificate);
	DICT_ADDPAIR(kSecAttrCertificateType, certificateType);
	DICT_ADDPAIR(kSecAttrCertificateEncoding, certificateEncoding);
	if (label) {
		DICT_ADDPAIR(kSecAttrLabel, label);
	}
	if (alias) {
		DICT_ADDPAIR(kSecAttrAlias, alias);
	}
	if (isData(certificate->_normalizedSubject)) {
		DICT_ADDPAIR(kSecAttrSubject, certificate->_normalizedSubject);
	}
	require_quiet(isData(certificate->_normalizedIssuer), out);
	DICT_ADDPAIR(kSecAttrIssuer, certificate->_normalizedIssuer);
	require_quiet(isData(certificate->_serialNumber), out);
	DICT_ADDPAIR(kSecAttrSerialNumber, certificate->_serialNumber);
	if (skid) {
		DICT_ADDPAIR(kSecAttrSubjectKeyID, skid);
	}
	DICT_ADDPAIR(kSecAttrPublicKeyHash, pubKeyDigest);
	DICT_ADDPAIR(kSecValueData, certData);
	dict = DICT_CREATE(allocator);

out:
	CFReleaseSafe(label);
	CFReleaseSafe(alias);
	CFReleaseSafe(pubKeyDigest);
	CFReleaseSafe(certData);
	CFReleaseSafe(certificateEncoding);
	CFReleaseSafe(certificateType);

	return dict;
}

SecCertificateRef SecCertificateCreateFromAttributeDictionary(
	CFDictionaryRef refAttributes) {
	/* @@@ Support having an allocator in refAttributes. */
 	CFAllocatorRef allocator = NULL;
	CFDataRef data = CFDictionaryGetValue(refAttributes, kSecValueData);
	return data ? SecCertificateCreateWithData(allocator, data) : NULL;
}
#endif

static bool _SecCertificateIsSelfSigned(SecCertificateRef certificate) {
    if (certificate->_isSelfSigned == kSecSelfSignedUnknown) {
        certificate->_isSelfSigned = kSecSelfSignedFalse;
        SecKeyRef publicKey = NULL;
        require(SecCertificateIsCertificate(certificate), out);
        require(publicKey = SecCertificateCopyKey(certificate), out);
        CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
        CFDataRef normalizedSubject =
        SecCertificateGetNormalizedSubjectContent(certificate);
        require_quiet(normalizedIssuer && normalizedSubject &&
                      CFEqual(normalizedIssuer, normalizedSubject), out);

        CFDataRef authorityKeyID = SecCertificateGetAuthorityKeyID(certificate);
        CFDataRef subjectKeyID = SecCertificateGetSubjectKeyID(certificate);
        if (authorityKeyID) {
            require_quiet(subjectKeyID && CFEqual(subjectKeyID, authorityKeyID), out);
        }

        require_noerr_quiet(SecCertificateIsSignedBy(certificate, publicKey), out);

        certificate->_isSelfSigned = kSecSelfSignedTrue;
    out:
        CFReleaseSafe(publicKey);
    }

    return (certificate->_isSelfSigned == kSecSelfSignedTrue);
}

bool SecCertificateIsCA(SecCertificateRef certificate) {
    bool result = false;
    require(SecCertificateIsCertificate(certificate), out);
    if (SecCertificateVersion(certificate) >= 3) {
        const SecCEBasicConstraints *basicConstraints = SecCertificateGetBasicConstraints(certificate);
        result = (basicConstraints && basicConstraints->isCA);
    }
    else {
        result = _SecCertificateIsSelfSigned(certificate);
    }
out:
    return result;
}

bool SecCertificateIsSelfSignedCA(SecCertificateRef certificate) {
    return (_SecCertificateIsSelfSigned(certificate) && SecCertificateIsCA(certificate));
}

OSStatus SecCertificateIsSelfSigned(SecCertificateRef certificate, Boolean *isSelfSigned) {
    if (!SecCertificateIsCertificate(certificate)) {
        return errSecInvalidCertificate;
    }
    if (!isSelfSigned) {
        return errSecParam;
    }
    *isSelfSigned = _SecCertificateIsSelfSigned(certificate);
    return errSecSuccess;
}

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRef certificate) {
    if (!certificate) {
        return kSecKeyUsageUnspecified;
    }
    return certificate->_keyUsage;
}

CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRef certificate)
{
    CFMutableArrayRef extended_key_usage_oids =
        CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require_quiet(certificate && extended_key_usage_oids, out);
    int ix;
    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oidExtendedKeyUsage.length &&
            !memcmp(extn->extnID.data, oidExtendedKeyUsage.data, extn->extnID.length)) {
            DERTag tag;
            DERSequence derSeq;
            DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &derSeq);
            require_noerr_quiet(drtn, out);
            require_quiet(tag == ASN1_CONSTR_SEQUENCE, out);
            DERDecodedInfo currDecoded;

            while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
                require_quiet(currDecoded.tag == ASN1_OBJECT_ID, out);
                CFDataRef oid = CFDataCreate(kCFAllocatorDefault,
                    currDecoded.content.data, currDecoded.content.length);
                require_quiet(oid, out);
                CFArrayAppendValue(extended_key_usage_oids, oid);
                CFReleaseNull(oid);
            }
            require_quiet(drtn == DR_EndOfSequence, out);
            return extended_key_usage_oids;
        }
    }
out:
    CFReleaseSafe(extended_key_usage_oids);
    return NULL;
}

CFArrayRef SecCertificateCopySignedCertificateTimestamps(SecCertificateRef certificate)
{
    require_quiet(certificate, out);
    int ix;

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oidGoogleEmbeddedSignedCertificateTimestamp.length &&
            !memcmp(extn->extnID.data, oidGoogleEmbeddedSignedCertificateTimestamp.data, extn->extnID.length)) {
            /* Got the SCT oid */
            DERDecodedInfo sctList;
            DERReturn drtn = DERDecodeItem(&extn->extnValue, &sctList);
            require_noerr_quiet(drtn, out);
            require_quiet(sctList.tag == ASN1_OCTET_STRING, out);
            return SecCreateSignedCertificateTimestampsArrayFromSerializedSCTList(sctList.content.data, sctList.content.length);
        }
    }
out:
    return NULL;
}


static bool matches_expected(DERItem der, CFTypeRef expected) {
    if (der.length > 1) {
        DERDecodedInfo decoded;
        DERDecodeItem(&der, &decoded);
        switch (decoded.tag) {
            case ASN1_NULL:
            {
                return decoded.content.length == 0 && expected == NULL;
            }
            break;

            case ASN1_IA5_STRING:
            case ASN1_UTF8_STRING: {
                if (isString(expected)) {
                    CFStringRef expectedString = (CFStringRef) expected;
                    CFStringRef itemString = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, decoded.content.data, decoded.content.length, kCFStringEncodingUTF8, false, kCFAllocatorNull);

                    bool result = (kCFCompareEqualTo == CFStringCompare(expectedString, itemString, 0));
                    CFReleaseNull(itemString);
                    return result;
                }
            }
            break;

            case ASN1_OCTET_STRING: {
                if (isData(expected)) {
                    CFDataRef expectedData = (CFDataRef) expected;
                    CFDataRef itemData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, decoded.content.data, decoded.content.length, kCFAllocatorNull);

                    bool result = CFEqual(expectedData, itemData);
                    CFReleaseNull(itemData);
                    return result;
                }
            }
            break;

            case ASN1_INTEGER: {
                SInt32 expected_value = 0;
                if (isString(expected))
                {
                    CFStringRef aStr = (CFStringRef)expected;
                    expected_value = CFStringGetIntValue(aStr);
                }
                else if (isNumber(expected))
                {
                    CFNumberGetValue(expected, kCFNumberSInt32Type, &expected_value);
                }

                uint32_t num_value = 0;
                if (!DERParseInteger(&decoded.content, &num_value))
                {
                    return ((uint32_t)expected_value == num_value);
                }
            }
            break;

            default:
                break;
        }
    }

    return false;
}

static bool cert_contains_marker_extension_value(SecCertificateRef certificate, CFDataRef oid, CFTypeRef expectedValue)
{
    CFIndex ix;
    const uint8_t *oid_data = CFDataGetBytePtr(oid);
    size_t oid_len = CFDataGetLength(oid);

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oid_len
            && !memcmp(extn->extnID.data, oid_data, extn->extnID.length))
        {
			return matches_expected(extn->extnValue, expectedValue);
        }
    }
    return false;
}

static bool cert_contains_marker_extension(SecCertificateRef certificate, CFTypeRef oid)
{
    return cert_contains_marker_extension_value(certificate, oid, NULL);
}

struct search_context {
    bool found;
    SecCertificateRef certificate;
};

static bool GetDecimalValueOfString(CFStringRef string, uint32_t* value)
{
    CFCharacterSetRef nonDecimalDigit = CFCharacterSetCreateInvertedSet(NULL, CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit));
    bool result = false;

    if ( CFStringGetLength(string) > 0
      && !CFStringFindCharacterFromSet(string, nonDecimalDigit, CFRangeMake(0, CFStringGetLength(string)), kCFCompareForcedOrdering, NULL))
    {
        if (value)
            *value = CFStringGetIntValue(string);
        result = true;
    }

    CFReleaseNull(nonDecimalDigit);

    return result;
}

bool SecCertificateIsOidString(CFStringRef oid)
{
    if (!oid) return false;
    if (2 >= CFStringGetLength(oid)) return false;
    bool result = true;

    /* oid string only has the allowed characters */
    CFCharacterSetRef decimalOid = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("0123456789."));
    CFCharacterSetRef nonDecimalOid = CFCharacterSetCreateInvertedSet(NULL, decimalOid);
    if (CFStringFindCharacterFromSet(oid, nonDecimalOid, CFRangeMake(0, CFStringGetLength(oid)), kCFCompareForcedOrdering, NULL)) {
        result = false;
    }

    /* first arc is allowed */
    UniChar firstArc[2];
    CFRange firstTwo = {0, 2};
    CFStringGetCharacters(oid, firstTwo, firstArc);
    if (firstArc[1] != '.' ||
        (firstArc[0] != '0' && firstArc[0] != '1' && firstArc[0] != '2')) {
        result = false;
    }

    CFReleaseNull(decimalOid);
    CFReleaseNull(nonDecimalOid);

    return result;
}

CFDataRef SecCertificateCreateOidDataFromString(CFAllocatorRef allocator, CFStringRef string)
{
    CFMutableDataRef currentResult = NULL;
    CFDataRef encodedResult = NULL;

    CFArrayRef parts = NULL;
    CFIndex count = 0;

    if (!string || !SecCertificateIsOidString(string))
        goto exit;

    parts = CFStringCreateArrayBySeparatingStrings(allocator, string, CFSTR("."));

    if (!parts)
        goto exit;

    count = CFArrayGetCount(parts);
    if (count == 0)
        goto exit;

    // assume no more than 5 bytes needed to represent any part of the oid,
    // since we limit parts to 32-bit values,
    // but the first two parts only need 1 byte
    currentResult = CFDataCreateMutable(allocator, 1+(count-2)*5);

    CFStringRef part;
    uint32_t x;
    uint8_t firstByte;

    part = CFArrayGetValueAtIndex(parts, 0);

    if (!GetDecimalValueOfString(part, &x) || x > 6)
        goto exit;

    firstByte = x * 40;


    if (count > 1) {
        part = CFArrayGetValueAtIndex(parts, 1);

        if (!GetDecimalValueOfString(part, &x) || x > 39)
            goto exit;

        firstByte += x;
    }

    CFDataAppendBytes(currentResult, &firstByte, 1);

    for (CFIndex i = 2; i < count && GetDecimalValueOfString(CFArrayGetValueAtIndex(parts, i), &x); ++i) {
        uint8_t b[5] = {0, 0, 0, 0, 0};
        b[4] = (x & 0x7F);
        b[3] = 0x80 | ((x >> 7) & 0x7F);
        b[2] = 0x80 | ((x >> 14) & 0x7F);
        b[1] = 0x80 | ((x >> 21) & 0x7F);
        b[0] = 0x80 | ((x >> 28) & 0x7F);

        // Skip the unused extension bytes.
        size_t skipBytes = 0;
        while (b[skipBytes] == 0x80)
            ++skipBytes;

        CFDataAppendBytes(currentResult, b + skipBytes, sizeof(b) - skipBytes);
    }

    encodedResult = currentResult;
    currentResult = NULL;

exit:
    CFReleaseNull(parts);
    CFReleaseNull(currentResult);

    return encodedResult;
}

static void check_for_marker(const void *key, const void *value, void *context)
{
    struct search_context * search_ctx = (struct search_context *) context;
    CFStringRef key_string = (CFStringRef) key;
    CFTypeRef value_ref = (CFTypeRef) value;

    // If we could have short-circuited the iteration
    // we would have, but the best we can do
    // is not waste time comparing once a match
    // was found.
    if (search_ctx->found)
        return;

    if (!isString(key_string))
        return;

    CFDataRef key_data = SecCertificateCreateOidDataFromString(NULL, key_string);

    if (!isData(key_data)) {
        CFReleaseNull(key_data);
        return;
    }

    if (cert_contains_marker_extension_value(search_ctx->certificate, key_data, value_ref))
        search_ctx->found = true;

    CFReleaseNull(key_data);
}

//
// CFType Ref is either:
//
// CFData - OID to match with no data permitted
// CFString - decimal OID to match
// CFDictionary - OID -> Value table for expected values Single Object or Array
// CFArray - Array of the above.
//
// This returns true if any of the requirements are met.
bool SecCertificateHasMarkerExtension(SecCertificateRef certificate, CFTypeRef oids)
{
    if (NULL == certificate || NULL == oids) {
        return false;
    } else if (CFGetTypeID(oids) == CFArrayGetTypeID()) {
        CFIndex ix, length = CFArrayGetCount(oids);
        for (ix = 0; ix < length; ix++)
            if (SecCertificateHasMarkerExtension(certificate, CFArrayGetValueAtIndex((CFArrayRef)oids, ix)))
                return true;
    } else if (CFGetTypeID(oids) == CFDictionaryGetTypeID()) {
        struct search_context context = { .found = false, .certificate = certificate };
        CFDictionaryApplyFunction((CFDictionaryRef) oids, &check_for_marker, &context);
        return context.found;
    } else if (CFGetTypeID(oids) == CFDataGetTypeID()) {
        return cert_contains_marker_extension(certificate, oids);
    } else if (CFGetTypeID(oids) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, oids);
        if (dataOid == NULL) return false;
        bool result = cert_contains_marker_extension(certificate, dataOid);
        CFReleaseNull(dataOid);
        return result;
    }
    return false;
}

// Since trust evaluation checks for the id-pkix-ocsp-nocheck OID marker
// in every certificate, this function caches the OID data once instead of
// parsing the same OID string each time.
//
bool SecCertificateHasOCSPNoCheckMarkerExtension(SecCertificateRef certificate)
{
    static CFDataRef sOCSPNoCheckOIDData = NULL;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        sOCSPNoCheckOIDData = SecCertificateCreateOidDataFromString(NULL, CFSTR("1.3.6.1.5.5.7.48.1.5"));
    });
    return SecCertificateHasMarkerExtension(certificate, sOCSPNoCheckOIDData);
}

static DERItem *cert_extension_value_for_marker(SecCertificateRef certificate, CFDataRef oid) {
    CFIndex ix;
    const uint8_t *oid_data = CFDataGetBytePtr(oid);
    size_t oid_len = CFDataGetLength(oid);

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oid_len
            && !memcmp(extn->extnID.data, oid_data, extn->extnID.length))
        {
            return (DERItem *)&extn->extnValue;
        }
    }
    return NULL;
}

//
// CFType Ref is either:
//
// CFData - OID to match with no data permitted
// CFString - decimal OID to match
//
DERItem *SecCertificateGetExtensionValue(SecCertificateRef certificate, CFTypeRef oid) {
    if (!certificate || !oid) {
        return NULL;
    }

    if(CFGetTypeID(oid) == CFDataGetTypeID()) {
        return cert_extension_value_for_marker(certificate, oid);
    } else if (CFGetTypeID(oid) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, oid);
        if (dataOid == NULL) return NULL;
        DERItem *result = cert_extension_value_for_marker(certificate, dataOid);
        CFReleaseNull(dataOid);
        return result;
    }

    return NULL;
}

CFDataRef SecCertificateCopyExtensionValue(SecCertificateRef certificate, CFTypeRef extensionOID, bool *isCritical) {
    if (!certificate || !extensionOID) {
        return NULL;
    }

    CFDataRef oid = NULL, extensionValue = NULL;
    if (CFGetTypeID(extensionOID) == CFDataGetTypeID()) {
        oid = CFRetainSafe(extensionOID);
    } else if (CFGetTypeID(extensionOID) == CFStringGetTypeID()) {
        oid = SecCertificateCreateOidDataFromString(NULL, extensionOID);
    }
    if (!oid) {
        return NULL;
    }

    CFIndex ix;
    const uint8_t *oid_data = CFDataGetBytePtr(oid);
    size_t oid_len = CFDataGetLength(oid);

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oid_len
            && !memcmp(extn->extnID.data, oid_data, extn->extnID.length))
        {
            if (isCritical) {
                *isCritical = extn->critical;
            }
            extensionValue = CFDataCreate(NULL, extn->extnValue.data, extn->extnValue.length);
            break;
        }
    }

    CFReleaseNull(oid);
    return extensionValue;
}

CFDataRef SecCertificateCopyiAPAuthCapabilities(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    CFDataRef extensionData = NULL;
    DERItem *extensionValue = NULL;
    extensionValue = SecCertificateGetExtensionValue(certificate,
                                                     CFSTR("1.2.840.113635.100.6.36"));
    require_quiet(extensionValue, out);
    /* The extension is a octet string containing the DER-encoded 32-byte octet string */
    require_quiet(extensionValue->length == 34, out);
    DERDecodedInfo decodedValue;
    require_noerr_quiet(DERDecodeItem(extensionValue, &decodedValue), out);
    if (decodedValue.tag == ASN1_OCTET_STRING) {
        require_quiet(decodedValue.content.length == 32, out);
        extensionData = CFDataCreate(NULL, decodedValue.content.data,
                                     decodedValue.content.length);
    } else {
        require_quiet(extensionValue->data[33] == 0x00 &&
                      extensionValue->data[32] == 0x00, out);
        extensionData = CFDataCreate(NULL, extensionValue->data, 32);
    }
out:
    return extensionData;
}

#if 0
/* From iapd IAPAuthenticationTypes.h */
typedef struct  IapCertSerialNumber
{
    uint8_t   xservID;            // Xserver ID
    uint8_t   hsmID;              // Hardware security module ID (generated cert)
    uint8_t   delimiter01;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   dateYear;           // Date year  cert was issued
    uint8_t   dateMonth;          // Date month cert was issued
    uint8_t   dateDay;            // Date day   cert was issued
    uint8_t   delimiter02;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   devClass;           // iAP device class (maps to lingo permissions)
    uint8_t   delimiter03;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   batchNumHi;         // Batch number high byte (15:08)
    uint8_t   batchNumLo;         // Batch number low  byte (07:00)
    uint8_t   delimiter04;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   serialNumHi;        // Serial number high   byte (23:16)
    uint8_t   serialNumMid;       // Serial number middle byte (15:08)
    uint8_t   serialNumLo;        // Serial number low    byte (07:00)

}   IapCertSerialNumber_t, *pIapCertSerialNumber_t;
#endif

#define IAP_CERT_FIELD_DELIMITER        0xAA    // "Apple_Accessory" delimiter
SeciAuthVersion SecCertificateGetiAuthVersion(SecCertificateRef certificate) {
    if (!certificate) {
        return kSeciAuthInvalid;
    }
    if (NULL != SecCertificateGetExtensionValue(certificate,
                                                CFSTR("1.2.840.113635.100.6.36"))) {
        /* v3 Capabilities Extension */
        return kSeciAuthVersion3;
    }
    if (NULL != SecCertificateGetExtensionValue(certificate,
                                                CFSTR("1.2.840.113635.100.6.59.1"))) {
        /* SW Auth General Capabilities Extension */
        return kSeciAuthVersionSW;
    }
    DERItem serialNumber = certificate->_serialNum;
    require_quiet(serialNumber.data, out);
    require_quiet(serialNumber.length == 15, out);
    require_quiet(serialNumber.data[2] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[6] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[8] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[11] == IAP_CERT_FIELD_DELIMITER, out);
    return kSeciAuthVersion2;
out:
    return kSeciAuthInvalid;
}

static CFStringRef SecCertificateiAPSWAuthCapabilitiesTypeToOID(SeciAPSWAuthCapabilitiesType type) {
    CFStringRef extensionOID = NULL;
    /* Get the oid for the type */
    if (type == kSeciAPSWAuthGeneralCapabilities) {
        extensionOID = CFSTR("1.2.840.113635.100.6.59.1");
    } else if (type == kSeciAPSWAuthAirPlayCapabilities) {
        extensionOID = CFSTR("1.2.840.113635.100.6.59.2");
    } else if (type == kSeciAPSWAuthHomeKitCapabilities) {
        extensionOID = CFSTR("1.2.840.113635.100.6.59.3");
    }
    return extensionOID;
}

CFDataRef SecCertificateCopyiAPSWAuthCapabilities(SecCertificateRef certificate, SeciAPSWAuthCapabilitiesType type) {
    if (!certificate) {
        return NULL;
    }
    CFDataRef extensionData = NULL;
    DERItem *extensionValue = NULL;
    CFStringRef extensionOID = SecCertificateiAPSWAuthCapabilitiesTypeToOID(type);
    require_quiet(extensionOID, out);
    extensionValue = SecCertificateGetExtensionValue(certificate, extensionOID);
    require_quiet(extensionValue, out);
    /* The extension is a octet string containing the DER-encoded variable-length octet string */
    DERDecodedInfo decodedValue;
    require_noerr_quiet(DERDecodeItem(extensionValue, &decodedValue), out);
    if (decodedValue.tag == ASN1_OCTET_STRING) {
        extensionData = CFDataCreate(NULL, decodedValue.content.data,
                                     decodedValue.content.length);
    }
out:
    return extensionData;
}

CFStringRef SecCertificateCopyComponentType(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    CFStringRef componentType = NULL;
    DERItem *extensionValue = SecCertificateGetExtensionValue(certificate, CFSTR("1.2.840.113635.100.11.1"));
    require_quiet(extensionValue, out);
    /* The componentType is an IA5String */
    DERDecodedInfo decodedValue;
    require_noerr_quiet(DERDecodeItem(extensionValue, &decodedValue), out);
    if (decodedValue.tag == ASN1_IA5_STRING) {
        componentType = CFStringCreateWithBytes(NULL, decodedValue.content.data, decodedValue.content.length, kCFStringEncodingASCII, false);
    }
out:
    return componentType;
}

SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFDataRef pem_certificate)
{
    static const char begin_cert[] = "-----BEGIN CERTIFICATE-----\n";
    static const char end_cert[] = "-----END CERTIFICATE-----\n";
    uint8_t *base64_data = NULL;
    SecCertificateRef cert = NULL;
    const unsigned char *data = CFDataGetBytePtr(pem_certificate);
    const size_t length = CFDataGetLength(pem_certificate);
    char *begin = strnstr((const char *)data, begin_cert, length);
    char *end = strnstr((const char *)data, end_cert, length);
    if (!begin || !end)
        return NULL;
    begin += sizeof(begin_cert) - 1;
    size_t base64_length = SecBase64Decode(begin, end - begin, NULL, 0);
    if (base64_length && (base64_length < (size_t)CFDataGetLength(pem_certificate))) {
        require_quiet(base64_data = calloc(1, base64_length), out);
        require_action_quiet(base64_length = SecBase64Decode(begin, end - begin, base64_data, base64_length), out, free(base64_data));
        cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, base64_data, base64_length);
        free(base64_data);
    }
out:
    return cert;
}


//
// -- MARK -- XPC encoding/decoding
//

bool SecCertificateAppendToXPCArray(SecCertificateRef certificate, xpc_object_t xpc_certificates, CFErrorRef *error) {
    if (!certificate)
        return true; // NOOP

    size_t length = SecCertificateGetLength(certificate);
    const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
#if SECTRUST_VERBOSE_DEBUG
	secerror("cert=0x%lX length=%d bytes=0x%lX", (uintptr_t)certificate, (int)length, (uintptr_t)bytes);
#endif
    if (!length || !bytes) {
        return SecError(errSecParam, error, CFSTR("failed to der encode certificate"));
	}
    xpc_array_set_data(xpc_certificates, XPC_ARRAY_APPEND, bytes, length);
    return true;
}

SecCertificateRef SecCertificateCreateWithXPCArrayAtIndex(xpc_object_t xpc_certificates, size_t index, CFErrorRef *error) {
    SecCertificateRef certificate = NULL;
    size_t length = 0;
    const uint8_t *bytes = xpc_array_get_data(xpc_certificates, index, &length);
    if (bytes) {
        certificate = SecCertificateCreateWithBytes(kCFAllocatorDefault, bytes, length);
    }
    if (!certificate) {
        SecError(errSecParam, error, CFSTR("certificates[%zu] failed to decode"), index);
	}
    return certificate;
}

xpc_object_t SecCertificateArrayCopyXPCArray(CFArrayRef certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates;
    require_action_quiet(xpc_certificates = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(certificates);
    for (ix = 0; ix < count; ++ix) {
		SecCertificateRef certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, ix);
    #if SECTRUST_VERBOSE_DEBUG
		CFIndex length = SecCertificateGetLength(certificate);
		const UInt8 *bytes = SecCertificateGetBytePtr(certificate);
		secerror("idx=%d of %d; cert=0x%lX length=%ld bytes=0x%lX", (int)ix, (int)count, (uintptr_t)certificate, (size_t)length, (uintptr_t)bytes);
    #endif
        if (!SecCertificateAppendToXPCArray(certificate, xpc_certificates, error)) {
            xpc_release(xpc_certificates);
            xpc_certificates = NULL;
			break;
        }
    }

exit:
    return xpc_certificates;
}

CFArrayRef SecCertificateXPCArrayCopyArray(xpc_object_t xpc_certificates, CFErrorRef *error) {
    CFMutableArrayRef certificates = NULL;
    require_action_quiet(xpc_get_type(xpc_certificates) == XPC_TYPE_ARRAY, exit,
                         SecError(errSecParam, error, CFSTR("certificates xpc value is not an array")));
    size_t count = xpc_array_get_count(xpc_certificates);
    require_action_quiet(certificates = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create CFArray of capacity %zu"), count));

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificateCreateWithXPCArrayAtIndex(xpc_certificates, ix, error);
        if (!cert) {
            CFRelease(certificates);
            return NULL;
        }
        CFArraySetValueAtIndex(certificates, ix, cert);
        CFRelease(cert);
    }

exit:
    return certificates;
}

#define do_if_registered(sdp, ...) if (gTrustd && gTrustd->sdp) { return gTrustd->sdp(__VA_ARGS__); }


static CFArrayRef CopyEscrowCertificates(SecCertificateEscrowRootType escrowRootType, CFErrorRef* error)
{
	__block CFArrayRef result = NULL;

	do_if_registered(ota_CopyEscrowCertificates, escrowRootType, error);

	securityd_send_sync_and_do(kSecXPCOpOTAGetEscrowCertificates, error,
		^bool(xpc_object_t message, CFErrorRef *blockError)
		{
			xpc_dictionary_set_uint64(message, "escrowType", (uint64_t)escrowRootType);
			return true;
		},
        ^bool(xpc_object_t response, CFErrorRef *blockError)
		{
			xpc_object_t xpc_array = xpc_dictionary_get_value(response, kSecXPCKeyResult);

			if (response && (NULL != xpc_array)) {
				result = (CFArrayRef)_CFXPCCreateCFObjectFromXPCObject(xpc_array);
			}
			else {
				return SecError(errSecInternal, blockError, CFSTR("Did not get the Escrow certificates"));
			}
			return result != NULL;
		});
	return result;
}

CFArrayRef SecCertificateCopyEscrowRoots(SecCertificateEscrowRootType escrowRootType)
{
	CFArrayRef result = NULL;
	int iCnt;
	CFDataRef certData = NULL;
	int numRoots = 0;

	if (kSecCertificateBaselineEscrowRoot == escrowRootType ||
		kSecCertificateBaselinePCSEscrowRoot == escrowRootType ||
		kSecCertificateBaselineEscrowBackupRoot == escrowRootType ||
		kSecCertificateBaselineEscrowEnrollmentRoot == escrowRootType)
	{
		// The request is for the base line certificates.
		// Use the hard coded data to generate the return array.
		struct RootRecord** pEscrowRoots;
		switch (escrowRootType) {
			case kSecCertificateBaselineEscrowRoot:
				numRoots = kNumberOfBaseLineEscrowRoots;
				pEscrowRoots = kBaseLineEscrowRoots;
				break;
			case kSecCertificateBaselinePCSEscrowRoot:
				numRoots = kNumberOfBaseLinePCSEscrowRoots;
				pEscrowRoots = kBaseLinePCSEscrowRoots;
				break;
			case kSecCertificateBaselineEscrowBackupRoot:
				numRoots = kNumberOfBaseLineEscrowBackupRoots;
				pEscrowRoots = kBaseLineEscrowBackupRoots;
				break;
			case kSecCertificateBaselineEscrowEnrollmentRoot:
			default:
				numRoots = kNumberOfBaseLineEscrowEnrollmentRoots;
				pEscrowRoots = kBaseLineEscrowEnrollmentRoots;
				break;
		}

		// Get the hard coded set of roots
		SecCertificateRef baseLineCerts[numRoots];
		struct RootRecord* pRootRecord = NULL;

		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			pRootRecord = pEscrowRoots[iCnt];
			if (NULL != pRootRecord && pRootRecord->_length > 0 && NULL != pRootRecord->_bytes) {
				certData = CFDataCreate(kCFAllocatorDefault, pRootRecord->_bytes, pRootRecord->_length);
				if (NULL != certData) {
					baseLineCerts[iCnt] = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
					CFRelease(certData);
				}
			}
		}
		result = CFArrayCreate(kCFAllocatorDefault, (const void **)baseLineCerts, numRoots, &kCFTypeArrayCallBacks);
		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			if (NULL != baseLineCerts[iCnt]) {
				CFRelease(baseLineCerts[iCnt]);
			}
		}
	} else {
		// The request is for the current certificates.
		CFErrorRef error = NULL;
		CFArrayRef cert_datas = CopyEscrowCertificates(escrowRootType, &error);
		if (NULL != error || NULL == cert_datas) {
			if (NULL != error) {
				CFRelease(error);
			}
			if (NULL != cert_datas) {
				CFRelease(cert_datas);
			}
			return result;
		}

		numRoots = (int)(CFArrayGetCount(cert_datas));

		SecCertificateRef assetCerts[numRoots];
		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			certData = (CFDataRef)CFArrayGetValueAtIndex(cert_datas, iCnt);
			if (NULL != certData) {
				SecCertificateRef aCertRef = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
				assetCerts[iCnt] = aCertRef;
			}
			else {
				assetCerts[iCnt] = NULL;
			}
		}

		if (numRoots > 0) {
			result = CFArrayCreate(kCFAllocatorDefault, (const void **)assetCerts, numRoots, &kCFTypeArrayCallBacks);
			for (iCnt = 0; iCnt < numRoots; iCnt++) {
				if (NULL != assetCerts[iCnt]) {
					CFRelease(assetCerts[iCnt]);
				}
			}
		}
		CFReleaseSafe(cert_datas);
	}
	return result;
}

static CFDictionaryRef CopyTrustedCTLogs(CFErrorRef* error)
{
    __block CFDictionaryRef result = NULL;

    // call function directly and return if we are built in server mode
    do_if_registered(sec_ota_pki_copy_trusted_ct_logs, error);

    securityd_send_sync_and_do(kSecXPCOpOTAPKICopyTrustedCTLogs, error,
       ^bool(xpc_object_t message, CFErrorRef *blockError) {
        // input: set message parameters here
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        // output: get dictionary from response object
        xpc_object_t xpc_dictionary = NULL;
        if (response) {
            xpc_dictionary = xpc_dictionary_get_value(response, kSecXPCKeyResult);
        }
        if (xpc_dictionary && (xpc_get_type(xpc_dictionary) == XPC_TYPE_DICTIONARY)) {
            result = (CFDictionaryRef)_CFXPCCreateCFObjectFromXPCObject(xpc_dictionary);
        } else {
            return SecError(errSecInternal, blockError, CFSTR("Unable to get CT logs"));
        }
        return result != NULL;
    });
    return result;
}

#define CTLOG_KEYID_LENGTH 32  /* key id data length */

static CFDictionaryRef CopyCTLogForKeyID(CFDataRef keyID, CFErrorRef* error)
{
    __block CFDictionaryRef result = NULL;
    if (!isData(keyID)) {
        (void) SecError(errSecParam, error, CFSTR("keyID was not a valid CFDataRef"));
        return result;
    }
    const void *p = CFDataGetBytePtr(keyID);
    if (!p || CFDataGetLength(keyID) != CTLOG_KEYID_LENGTH) {
        (void) SecError(errSecParam, error, CFSTR("keyID data was not the expected length"));
        return result;
    }
    // call function directly and return if we are built in server mode
    do_if_registered(sec_ota_pki_copy_ct_log_for_keyid, keyID, error);

    securityd_send_sync_and_do(kSecXPCOpOTAPKICopyCTLogForKeyID, error,
       ^bool(xpc_object_t message, CFErrorRef *blockError) {
        // input: set message parameters here
        xpc_dictionary_set_data(message, kSecXPCData, p, CTLOG_KEYID_LENGTH);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        // output: get dictionary from response object
        xpc_object_t xpc_dictionary = NULL;
        if (response) {
            xpc_dictionary = xpc_dictionary_get_value(response, kSecXPCKeyResult);
        }
        if (xpc_dictionary && (xpc_get_type(xpc_dictionary) == XPC_TYPE_DICTIONARY)) {
            result = (CFDictionaryRef)_CFXPCCreateCFObjectFromXPCObject(xpc_dictionary);
        } else {
            return SecError(errSecInternal, blockError, CFSTR("Unable to match CT log"));
        }
        return result != NULL;
    });
    return result;
}

CFDictionaryRef SecCertificateCopyTrustedCTLogs(void)
{
    CFErrorRef localError = NULL;
    CFDictionaryRef result = CopyTrustedCTLogs(&localError);
    CFReleaseSafe(localError);
    return result;
}

CFDictionaryRef SecCertificateCopyCTLogForKeyID(CFDataRef keyID)
{
    CFErrorRef localError = NULL;
    CFDictionaryRef result = CopyCTLogForKeyID(keyID, &localError);
    CFReleaseSafe(localError);
    return result;
}

SEC_CONST_DECL (kSecSignatureDigestAlgorithmUnknown, "SignatureDigestUnknown");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD2, "SignatureDigestMD2");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD4, "SignatureDigestMD4");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD5, "SignatureDigestMD5");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA1, "SignatureDigestSHA1");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA224, "SignatureDigestSHA224");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA256, "SignatureDigestSHA256");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA384, "SignatureDigestSHA284");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA512, "SignatureDigestSHA512");

SecSignatureHashAlgorithm SecSignatureHashAlgorithmForAlgorithmOid(const DERItem *algOid)
{
    SecSignatureHashAlgorithm result = kSecSignatureHashAlgorithmUnknown;
    while (algOid) {
        if (!algOid->data || !algOid->length) {
            break;
        }
        /* classify the signature algorithm OID into one of our known types */
        if (DEROidCompare(algOid, &oidSha512Ecdsa) ||
            DEROidCompare(algOid, &oidSha512Rsa) ||
            DEROidCompare(algOid, &oidSha512)) {
            result = kSecSignatureHashAlgorithmSHA512;
            break;
        }
        if (DEROidCompare(algOid, &oidSha384Ecdsa) ||
            DEROidCompare(algOid, &oidSha384Rsa) ||
            DEROidCompare(algOid, &oidSha384)) {
            result = kSecSignatureHashAlgorithmSHA384;
            break;
        }
        if (DEROidCompare(algOid, &oidSha256Ecdsa) ||
            DEROidCompare(algOid, &oidSha256Rsa) ||
            DEROidCompare(algOid, &oidSha256)) {
            result = kSecSignatureHashAlgorithmSHA256;
            break;
        }
        if (DEROidCompare(algOid, &oidSha224Ecdsa) ||
            DEROidCompare(algOid, &oidSha224Rsa) ||
            DEROidCompare(algOid, &oidSha224)) {
            result = kSecSignatureHashAlgorithmSHA224;
            break;
        }
        if (DEROidCompare(algOid, &oidSha1Ecdsa) ||
            DEROidCompare(algOid, &oidSha1Rsa) ||
            DEROidCompare(algOid, &oidSha1Dsa) ||
            DEROidCompare(algOid, &oidSha1DsaOIW) ||
            DEROidCompare(algOid, &oidSha1DsaCommonOIW) ||
            DEROidCompare(algOid, &oidSha1RsaOIW) ||
            DEROidCompare(algOid, &oidSha1Fee) ||
            DEROidCompare(algOid, &oidSha1)) {
            result = kSecSignatureHashAlgorithmSHA1;
            break;
        }
        if (DEROidCompare(algOid, &oidMd5Rsa) ||
            DEROidCompare(algOid, &oidMd5Fee) ||
            DEROidCompare(algOid, &oidMd5)) {
            result = kSecSignatureHashAlgorithmMD5;
            break;
        }
        if (DEROidCompare(algOid, &oidMd4Rsa) ||
            DEROidCompare(algOid, &oidMd4)) {
            result = kSecSignatureHashAlgorithmMD4;
            break;
        }
        if (DEROidCompare(algOid, &oidMd2Rsa) ||
            DEROidCompare(algOid, &oidMd2)) {
            result = kSecSignatureHashAlgorithmMD2;
            break;
        }
        break;
    }

    return result;
}

SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef certificate)
{
	DERAlgorithmId *algId = (certificate) ? &certificate->_tbsSigAlg : NULL;
	const DERItem *algOid = (algId) ? &algId->oid : NULL;
    return SecSignatureHashAlgorithmForAlgorithmOid(algOid);
}

CFArrayRef SecCertificateCopyiPhoneDeviceCAChain(void) {
    CFMutableArrayRef result = NULL;
    SecCertificateRef iPhoneDeviceCA = NULL, iPhoneCA = NULL, appleRoot = NULL;

    require_quiet(iPhoneDeviceCA = SecCertificateCreateWithBytes(NULL, _AppleiPhoneDeviceCA, sizeof(_AppleiPhoneDeviceCA)),
                  errOut);
    require_quiet(iPhoneCA = SecCertificateCreateWithBytes(NULL, _AppleiPhoneCA, sizeof(_AppleiPhoneCA)),
                  errOut);
    require_quiet(appleRoot = SecCertificateCreateWithBytes(NULL, _AppleRootCA, sizeof(_AppleRootCA)),
                  errOut);

    require_quiet(result = CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks), errOut);
    CFArrayAppendValue(result, iPhoneDeviceCA);
    CFArrayAppendValue(result, iPhoneCA);
    CFArrayAppendValue(result, appleRoot);

errOut:
    CFReleaseNull(iPhoneDeviceCA);
    CFReleaseNull(iPhoneCA);
    CFReleaseNull(appleRoot);
    return result;
}

bool SecCertificateGetDeveloperIDDate(SecCertificateRef certificate, CFAbsoluteTime *time, CFErrorRef *error) {
    if (!certificate || !time) {
        return SecError(errSecParam, error, CFSTR("DeveloperID Date parsing: missing required input"));
    }
    DERItem *extensionValue = SecCertificateGetExtensionValue(certificate, CFSTR("1.2.840.113635.100.6.1.33"));
    if (!extensionValue) {
        return SecError(errSecMissingRequiredExtension, error, CFSTR("DeveloperID Date parsing: extension not found"));
    }
    DERDecodedInfo decodedValue;
    if (DERDecodeItem(extensionValue, &decodedValue) != DR_Success) {
        return SecError(errSecDecode, error, CFSTR("DeveloperID Date parsing: extension value failed to decode"));
    }
    /* The extension value is a DERGeneralizedTime encoded in a UTF8String */
    CFErrorRef localError = NULL;
    if (decodedValue.tag == ASN1_UTF8_STRING) {
         *time = SecAbsoluteTimeFromDateContentWithError(ASN1_GENERALIZED_TIME, decodedValue.content.data, decodedValue.content.length, &localError);
    } else {
        return SecError(errSecDecode, error, CFSTR("DeveloperID Date parsing: extension value wrong tag"));
    }
    return CFErrorPropagate(localError, error);
}

CFIndex SecCertificateGetUnparseableKnownExtension(SecCertificateRef certificate) {
    if (!certificate) {
        return kCFNotFound;
    }
    return certificate->_unparseableKnownExtensionIndex;
}

CFArrayRef SecCertificateCopyAppleExternalRoots(void) {
    CFMutableArrayRef result = NULL;
    SecCertificateRef appleExternalECRoot = NULL, testAppleExternalECRoot = NULL;

    require_quiet(appleExternalECRoot = SecCertificateCreateWithBytes(NULL, _AppleExternalECRootCA, sizeof(_AppleExternalECRootCA)),
                  errOut);
    require_quiet(testAppleExternalECRoot = SecCertificateCreateWithBytes(NULL, _TestAppleExternalECRootCA,
                                                                          sizeof(_TestAppleExternalECRootCA)),
                  errOut);

    require_quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    CFArrayAppendValue(result, appleExternalECRoot);
    if (SecIsInternalRelease()) {
        CFArrayAppendValue(result, testAppleExternalECRoot);
    }

errOut:
    CFReleaseNull(appleExternalECRoot);
    CFReleaseNull(testAppleExternalECRoot);
    return result;
}
