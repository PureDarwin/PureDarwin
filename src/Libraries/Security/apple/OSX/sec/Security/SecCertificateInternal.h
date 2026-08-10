/*
 * Copyright (c) 2007-2019 Apple Inc. All Rights Reserved.
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
   SecCertificateInternal
*/

#ifndef _SECURITY_SECCERTIFICATEINTERNAL_H_
#define _SECURITY_SECCERTIFICATEINTERNAL_H_

#include <TargetConditionals.h>
#include <libDER/DER_Keys.h>

#include <Security/SecBase.h>
#include <Security/SecCertificatePriv.h>

#include <Security/certextensions.h>

// This file can only be included under the ios view of the headers.
// If you're not under that view, we'll forward declare the things you need here.
#if SECURITY_PROJECT_TAPI_HACKS && SEC_OS_OSX
typedef struct {
    bool                present;
    bool                critical;
    bool                isCA;
    bool                pathLenConstraintPresent;
    uint32_t            pathLenConstraint;
} SecCEBasicConstraints;

typedef struct {
    bool                present;
    bool                critical;
    bool                requireExplicitPolicyPresent;
    uint32_t            requireExplicitPolicy;
    bool                inhibitPolicyMappingPresent;
    uint32_t            inhibitPolicyMapping;
} SecCEPolicyConstraints;

typedef struct {
    DERItem policyIdentifier;
    DERItem policyQualifiers;
} SecCEPolicyInformation;

typedef struct {
    bool                    present;
    bool                    critical;
    size_t                  numPolicies;            // size of *policies;
    SecCEPolicyInformation  *policies;
} SecCECertificatePolicies;

typedef struct {
    DERItem issuerDomainPolicy;
    DERItem subjectDomainPolicy;
} SecCEPolicyMapping;

typedef struct {
    bool                present;
    bool                critical;
    size_t            numMappings;            // size of *mappings;
    SecCEPolicyMapping  *mappings;
} SecCEPolicyMappings;

typedef struct {
    bool             present;
    bool             critical;
    uint32_t         skipCerts;
} SecCEInhibitAnyPolicy;

#endif

__BEGIN_DECLS

SecSignatureHashAlgorithm SecSignatureHashAlgorithmForAlgorithmOid(const DERItem *algOid);

CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate);
CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate);

/* Return an array of CFURLRefs each of which is an crl distribution point for
   this certificate. */
CFArrayRef SecCertificateGetCRLDistributionPoints(SecCertificateRef certificate);

/* Return an array of CFURLRefs each of which is an caIssuer for this
   certificate. */
CFArrayRef SecCertificateGetCAIssuers(SecCertificateRef certificate);

/* Dump certificate for debugging. */
void SecCertificateShow(SecCertificateRef certificate);

/* Return the normalized name or NULL if it fails to parse */
CFDataRef SecDistinguishedNameCopyNormalizedContent(CFDataRef distinguished_name);

/* Return true iff the certificate has a subject. */
bool SecCertificateHasSubject(SecCertificateRef certificate);
/* Return true iff the certificate has a critical subject alt name. */
bool SecCertificateHasCriticalSubjectAltName(SecCertificateRef certificate);

/* Return the contents of the SubjectAltName extension. */
const DERItem * SecCertificateGetSubjectAltName(SecCertificateRef certificate);

/* Return true if certificate contains one or more critical extensions we
   are unable to parse. */
bool SecCertificateHasUnknownCriticalExtension(SecCertificateRef certificate);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecCertificateCopyAttributeDictionary(
	SecCertificateRef certificate);

/* Return a certificate from the attribute dictionary that was used to store
   this item in a keychain. */
SecCertificateRef SecCertificateCreateFromAttributeDictionary(
	CFDictionaryRef refAttributes);

/* Return a SecKeyRef for the public key embedded in the cert. */
#if TARGET_OS_OSX
SecKeyRef SecCertificateCopyPublicKey_ios(SecCertificateRef certificate)
    __OSX_DEPRECATED(__MAC_10_12, __MAC_10_14, "Use SecCertificateCopyKey instead.");
#endif

/* Return the SecCEBasicConstraints extension for this certificate if it
   has one. */
const SecCEBasicConstraints *
SecCertificateGetBasicConstraints(SecCertificateRef certificate);

/* Returns array of CFDataRefs containing the generalNames that are
   Permitted Subtree Name Constraints for this certificate if it has
   any. */
CFArrayRef SecCertificateGetPermittedSubtrees(SecCertificateRef certificate);

/* Returns array of CFDataRefs containing the generalNames that are
   Excluded Subtree Name Constraints for this certificate if it has
   any. */
CFArrayRef SecCertificateGetExcludedSubtrees(SecCertificateRef certificate);

/* Return the SecCEPolicyConstraints extension for this certificate if it
   has one. */
const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraints(SecCertificateRef certificate);

/* Return a dictionary from CFDataRef to CFArrayRef of CFDataRef
   representing the policyMapping extension of this certificate. */
const SecCEPolicyMappings *
SecCertificateGetPolicyMappings(SecCertificateRef certificate);

/* Return the SecCECertificatePolicies extension for this certificate if it
   has one. */
const SecCECertificatePolicies *
SecCertificateGetCertificatePolicies(SecCertificateRef certificate);

/* Returns UINT32_MAX if InhibitAnyPolicy extension is not present or invalid,
   returns the value of the SkipCerts field of the InhibitAnyPolicy extension
   otherwise. */
const SecCEInhibitAnyPolicy *
SecCertificateGetInhibitAnyPolicySkipCerts(SecCertificateRef certificate);

/* Return the public key algorithm and parameters for certificate.  */
const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithm(
	SecCertificateRef certificate);

/* Return the raw public key data for certificate.  */
const DERItem *SecCertificateGetPublicKeyData(SecCertificateRef certificate);

/* Return legacy property values for use by SecCertificateCopyValues. */
CFArrayRef SecCertificateCopyLegacyProperties(SecCertificateRef certificate);

// MARK: -
// MARK: Certificate Operations

OSStatus SecCertificateIsSignedBy(SecCertificateRef certificate,
    SecKeyRef issuerKey);

#ifndef SECURITY_PROJECT_TAPI_HACKS
void appendProperty(CFMutableArrayRef properties, CFStringRef propertyType,
    CFStringRef label, CFStringRef localizedLabel, CFTypeRef value, bool localized);
#endif

/* Utility functions. */
CFStringRef SecDERItemCopyOIDDecimalRepresentation(CFAllocatorRef allocator,
    const DERItem *oid);

#ifndef SECURITY_PROJECT_TAPI_HACKS
CFDataRef createNormalizedX501Name(CFAllocatorRef allocator,
	const DERItem *x501name);
#endif

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   an absoluteTime if the date was valid and properly decoded.  Return
   NULL_TIME otherwise. */
CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
    size_t length);

bool SecCertificateHasMarkerExtension(SecCertificateRef certificate, CFTypeRef oid);

bool SecCertificateHasOCSPNoCheckMarkerExtension(SecCertificateRef certificate);

typedef OSStatus (*parseGeneralNameCallback)(void *context,
                                             SecCEGeneralNameType type, const DERItem *value);
OSStatus SecCertificateParseGeneralNameContentProperty(DERTag tag,
                                         const DERItem *generalNameContent,
                                         void *context, parseGeneralNameCallback callback);

OSStatus SecCertificateParseGeneralNames(const DERItem *generalNames, void *context,
                                         parseGeneralNameCallback callback);

CFArrayRef SecCertificateCopyOrganizationFromX501NameContent(const DERItem *nameContent);

bool SecCertificateIsWeakKey(SecCertificateRef certificate);
bool SecCertificateIsAtLeastMinKeySize(SecCertificateRef certificate,
                                       CFDictionaryRef keySizes);
bool SecCertificateIsStrongKey(SecCertificateRef certificate);

extern const CFStringRef kSecSignatureDigestAlgorithmUnknown;
#ifndef SECURITY_PROJECT_TAPI_HACKS
extern const CFStringRef kSecSignatureDigestAlgorithmMD2;
extern const CFStringRef kSecSignatureDigestAlgorithmMD4;
extern const CFStringRef kSecSignatureDigestAlgorithmMD5;
extern const CFStringRef kSecSignatureDigestAlgorithmSHA1;
extern const CFStringRef kSecSignatureDigestAlgorithmSHA224;
extern const CFStringRef kSecSignatureDigestAlgorithmSHA256;
extern const CFStringRef kSecSignatureDigestAlgorithmSHA384;
extern const CFStringRef kSecSignatureDigestAlgorithmSHA512;
#endif

bool SecCertificateIsWeakHash(SecCertificateRef certificate);

CFDataRef SecCertificateCreateOidDataFromString(CFAllocatorRef allocator, CFStringRef string);
bool SecCertificateIsOidString(CFStringRef oid);

DERItem *SecCertificateGetExtensionValue(SecCertificateRef certificate, CFTypeRef oid);

CFArrayRef SecCertificateCopyRFC822NamesFromSubject(SecCertificateRef certificate);
CFArrayRef SecCertificateCopyDNSNamesFromSAN(SecCertificateRef certificate);
CFArrayRef SecCertificateCopyIPAddressDatas(SecCertificateRef certificate);

CFIndex SecCertificateGetUnparseableKnownExtension(SecCertificateRef certificate);

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEINTERNAL_H_ */
