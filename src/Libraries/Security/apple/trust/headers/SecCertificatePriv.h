/*
 * Copyright (c) 2002-2004,2006-2019 Apple Inc. All Rights Reserved.
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

/*!
    @header SecCertificatePriv
    The functions provided in SecCertificatePriv.h implement and manage a particular
    type of keychain item that represents a certificate.  You can store a
    certificate in a keychain, but a certificate can also be a transient
    object.

    You can use a certificate as a keychain item in most functions.
    Certificates are able to compute their parent certificates, and much more.
*/

#ifndef _SECURITY_SECCERTIFICATEPRIV_H_
#define _SECURITY_SECCERTIFICATEPRIV_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>
#include <stdbool.h>
#include <xpc/xpc.h>
#include <security_libDER/libDER/libDER.h>

#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificate.h>

__BEGIN_DECLS

#if SEC_OS_IPHONE
typedef CF_OPTIONS(uint32_t, SecKeyUsage) {
    kSecKeyUsageUnspecified      = 0u,
    kSecKeyUsageDigitalSignature = 1u << 0,
    kSecKeyUsageNonRepudiation   = 1u << 1,
    kSecKeyUsageContentCommitment= 1u << 1,
    kSecKeyUsageKeyEncipherment  = 1u << 2,
    kSecKeyUsageDataEncipherment = 1u << 3,
    kSecKeyUsageKeyAgreement     = 1u << 4,
    kSecKeyUsageKeyCertSign      = 1u << 5,
    kSecKeyUsageCRLSign          = 1u << 6,
    kSecKeyUsageEncipherOnly     = 1u << 7,
    kSecKeyUsageDecipherOnly     = 1u << 8,
    kSecKeyUsageCritical         = 1u << 31,
    kSecKeyUsageAll              = 0x7FFFFFFFu
};
#endif /* SEC_OS_IPHONE */

typedef CF_ENUM(uint32_t, SecCertificateEscrowRootType) {
    kSecCertificateBaselineEscrowRoot = 0,
    kSecCertificateProductionEscrowRoot = 1,
    kSecCertificateBaselinePCSEscrowRoot = 2,
    kSecCertificateProductionPCSEscrowRoot = 3,
    kSecCertificateBaselineEscrowBackupRoot = 4,        // v100 and v101
    kSecCertificateProductionEscrowBackupRoot = 5,
    kSecCertificateBaselineEscrowEnrollmentRoot = 6,    // v101 only
    kSecCertificateProductionEscrowEnrollmentRoot = 7,
};

/* The names of the files that contain the escrow certificates */
extern const CFStringRef kSecCertificateProductionEscrowKey;
extern const CFStringRef kSecCertificateProductionPCSEscrowKey;
extern const CFStringRef kSecCertificateEscrowFileName;

/* Return a certificate for the DER representation of this certificate.
 Return NULL if the passed-in data is not a valid DER-encoded X.509
 certificate. */
SecCertificateRef SecCertificateCreateWithBytes(CFAllocatorRef allocator,
                                                const UInt8 *bytes, CFIndex length)
__SEC_MAC_AND_IOS_UNKNOWN;
//__OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_UNKNOWN);

/* Returns a certificate from a pem blob.
 Return NULL if the passed-in data is not a valid DER-encoded X.509
 certificate. */
SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator, CFDataRef pem_certificate)
__SEC_MAC_AND_IOS_UNKNOWN;
//__OSX_AVAILABLE_STARTING(__MAC_10_12, __SEC_IPHONE_UNKNOWN);

/* Return the length of the DER representation of this certificate. */
CFIndex SecCertificateGetLength(SecCertificateRef certificate);

/* Return the bytes of the DER representation of this certificate. */
const UInt8 *SecCertificateGetBytePtr(SecCertificateRef certificate);

/* Return the SHA-1 hash of this certificate. */
CFDataRef SecCertificateGetSHA1Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the SHA-256 hash of this certificate. */
CFDataRef SecCertificateCopySHA256Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the SHA-1 hash of the public key in this certificate. */
CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the SHA-1 hash of the SubjectPublicKeyInfo sequence in this certificate. */
CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA1Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the SHA-256 hash of the SubjectPublicKeyInfo sequence in this certificate. */
CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA256Digest(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return an array of CFStringRefs representing the dns addresses in the
 certificate if any. */
CFArrayRef SecCertificateCopyDNSNames(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return an array of CFStringRefs representing the NTPrincipalNames in the
 certificate if any. */
CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Create a unified SecCertificateRef from a legacy keychain item and its data. */
SecCertificateRef SecCertificateCreateWithKeychainItem(CFAllocatorRef allocator,
                                                       CFDataRef der_certificate, CFTypeRef keychainItem)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Set a legacy item instance for a unified SecCertificateRef. */
OSStatus SecCertificateSetKeychainItem(SecCertificateRef certificate, CFTypeRef keychain_item)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return a keychain item reference, given a unified SecCertificateRef.
 Note: On OSX, for this function to succeed, the provided certificate must have been
 created by SecCertificateCreateWithKeychainItem, otherwise NULL is returned.
 */
CFTypeRef SecCertificateCopyKeychainItem(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/*!
 @function SecCertificateCopyIssuerSummary
 @abstract Return a simple string which hopefully represents a human understandable issuer.
 @param certificate SecCertificate object created with SecCertificateCreateWithData().
 @discussion All the data in this string comes from the certificate itself
 and thus it's in whatever language the certificate itself is in.
 @result A CFStringRef which the caller should CFRelease() once it's no longer needed.
 */
CFStringRef SecCertificateCopyIssuerSummary(SecCertificateRef certificate);

/* Return a string formatted according to RFC 2253 representing the complete
 subject of certificate. */
CFStringRef SecCertificateCopySubjectString(SecCertificateRef certificate);

CFMutableArrayRef SecCertificateCopySummaryProperties(
                                                      SecCertificateRef certificate, CFAbsoluteTime verifyTime)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the content of a DER encoded X.501 name (without the tag and length
 fields) for the receiving certificates issuer. */
CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the content of a DER encoded X.501 name (without the tag and length
 fields) for the receiving certificates subject. */
CFDataRef SecCertificateGetNormalizedSubjectContent(SecCertificateRef certificate)
    __SEC_MAC_AND_IOS_UNKNOWN;

/* Return the DER encoded issuer sequence for the certificate's issuer. */
CFDataRef SecCertificateCopyIssuerSequence(SecCertificateRef certificate);

/* Return the DER encoded subject sequence for the certificate's subject. */
CFDataRef SecCertificateCopySubjectSequence(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the ip addresses in the
 certificate if any. */
CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the email addresses in the
 certificate if any. */
CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the common names in the
 certificates subject if any. */
CFArrayRef SecCertificateCopyCommonNames(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the organization in the
 certificate's subject if any. */
CFArrayRef SecCertificateCopyOrganization(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the organizational unit in the
 certificate's subject if any. */
CFArrayRef SecCertificateCopyOrganizationalUnit(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the country in the
 certificate's subject if any. */
CFArrayRef SecCertificateCopyCountry(SecCertificateRef certificate);

/* Return a string with the company name of an ev leaf certificate. */
CFStringRef SecCertificateCopyCompanyName(SecCertificateRef certificate);

/* X.509 Certificate Version: 1, 2 or 3. */
CFIndex SecCertificateVersion(SecCertificateRef certificate);

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRef certificate);

/* Returns an array of CFDataRefs for all extended key usage oids or NULL */
CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRef certificate);

/*!
 @function SecCertificateIsValid
 @abstract Check certificate validity on a given date.
 @param certificate A certificate reference.
 @result Returns true if the specified date falls within the certificate's validity period, false otherwise.
 */
bool SecCertificateIsValid(SecCertificateRef certificate, CFAbsoluteTime verifyTime)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_2_0);

/*!
 @function SecCertificateNotValidBefore
 @abstract Obtain the starting date of the given certificate.
 @param certificate A certificate reference.
 @result Returns the absolute time at which the given certificate becomes valid,
 or 0 if this value could not be obtained.
 */
CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_2_0);

/*!
 @function SecCertificateNotValidAfter
 @abstract Obtain the expiration date of the given certificate.
 @param certificate A certificate reference.
 @result Returns the absolute time at which the given certificate expires,
 or 0 if this value could not be obtained.
 */
CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_2_0);

/*!
 @function SecCertificateIsSelfSigned
 @abstract Determine if the given certificate is self-signed.
 @param certRef A certificate reference.
 @param isSelfSigned Will be set to true on return if the certificate is self-signed, false otherwise.
 @result A result code. Returns errSecSuccess if the certificate's status can be determined.
 */
OSStatus SecCertificateIsSelfSigned(SecCertificateRef certRef, Boolean *isSelfSigned)
    __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_9_0);

/*!
 @function SecCertificateIsSelfSignedCA
 @abstract Determine if the given certificate is self-signed and has a basic
 constraints extension indicating it is a certificate authority.
 @param certificate A certificate reference.
 @result Returns true if the certificate is self-signed and has a basic
 constraints extension indicating it is a certificate authority, otherwise false.
 */
bool SecCertificateIsSelfSignedCA(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_9_0);

/*!
 @function SecCertificateIsCA
 @abstract Determine if the given certificate has a basic
 constraints extension indicating it is a certificate authority.
 @param certificate A certificate reference.
 @result Returns true if the certificate has a basic constraints
 extension indicating it is a certificate authority, otherwise false.
 */
bool SecCertificateIsCA(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_9_0);


/* Append certificate to xpc_certificates. */
bool SecCertificateAppendToXPCArray(SecCertificateRef certificate, xpc_object_t xpc_certificates, CFErrorRef *error);

/* Decode certificate from xpc_certificates[index] as encoded by SecCertificateAppendToXPCArray(). */
SecCertificateRef SecCertificateCreateWithXPCArrayAtIndex(xpc_object_t xpc_certificates, size_t index, CFErrorRef *error);

/* Return an xpc_array of data from an array of SecCertificateRefs. */
xpc_object_t SecCertificateArrayCopyXPCArray(CFArrayRef certificates, CFErrorRef *error);

/* Return an array of SecCertificateRefs from a xpc_object array of datas. */
CFArrayRef SecCertificateXPCArrayCopyArray(xpc_object_t xpc_certificates, CFErrorRef *error);

/*!
 @function SecCertificateCopyEscrowRoots
 @abstract Retrieve the array of valid escrow certificates for a given root type.
 @param escrowRootType An enumerated type indicating which root type to return.
 @result An array of zero or more escrow certificates matching the provided type.
 */
CFArrayRef SecCertificateCopyEscrowRoots(SecCertificateEscrowRootType escrowRootType)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecCertificateCopyAttributeDictionary(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*
 * Enumerated constants for signature hash algorithms.
 */
typedef CF_ENUM(uint32_t, SecSignatureHashAlgorithm){
    kSecSignatureHashAlgorithmUnknown = 0,
    kSecSignatureHashAlgorithmMD2 = 1,
    kSecSignatureHashAlgorithmMD4 = 2,
    kSecSignatureHashAlgorithmMD5 = 3,
    kSecSignatureHashAlgorithmSHA1 = 4,
    kSecSignatureHashAlgorithmSHA224 = 5,
    kSecSignatureHashAlgorithmSHA256 = 6,
    kSecSignatureHashAlgorithmSHA384 = 7,
    kSecSignatureHashAlgorithmSHA512 = 8
};

/*!
 @function SecCertificateGetSignatureHashAlgorithm
 @abstract Determine the hash algorithm used in a certificate's signature.
 @param certificate A certificate reference.
 @result Returns an enumerated value indicating the signature hash algorithm
 used in a certificate. If the hash algorithm is unsupported or cannot be
 obtained (e.g. because the supplied certificate reference is invalid), a
 value of 0 (kSecSignatureHashAlgorithmUnknown) is returned.
 */
SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecCertificateCopyProperties
 @abstract Return a property array for this certificate.
 @param certificate A reference to the certificate to evaluate.
 @result A property array. It is the caller's responsibility to CFRelease
 the returned array when it is no longer needed.
 See SecTrustCopySummaryPropertiesAtIndex on how to interpret this array.
 Unlike that function, this function returns a detailed description of the
 certificate. Note that localized description strings are returned by default.
 Use SecCertificateCopyLocalizedProperties if your code needs to explicitly
 specify whether the strings are localized.
 */
CFArrayRef SecCertificateCopyProperties(SecCertificateRef certificate);

/*!
 @function SecCertificateCopyLocalizedProperties
 @abstract Return a property array for this certificate.
 @param certificate A reference to the certificate to evaluate.
 @param localized A value which specifies whether string properties
 are localized. If false, description strings will not be localized.
 @result A property array. It is the caller's responsibility to CFRelease
 the returned array when it is no longer needed.
 See SecTrustCopySummaryPropertiesAtIndex on how to interpret this array.
 Unlike that function, this function returns a detailed description of the
 certificate.
 */
CFArrayRef SecCertificateCopyLocalizedProperties(SecCertificateRef certificate, Boolean localized)
    API_AVAILABLE(macos(10.15.1), ios(13.2), watchos(6.1), tvos(13.1));

/* Returns an array of CFDataRefs for all embedded SCTs */
CFArrayRef SecCertificateCopySignedCertificateTimestamps(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_9_0);

/* Return the precert TBSCertificate DER data - used for Certificate Transparency */
CFDataRef SecCertificateCopyPrecertTBS(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_9_0);

/* Returns a dictionary of dictionaries for system-trusted CT logs, indexed by the LogID */
CFDictionaryRef SecCertificateCopyTrustedCTLogs(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_15, __IPHONE_13_0);

/* Returns a dictionary for the CT log matching the provided
 * key ID, or NULL if no matching log is found.
 * And by keyID we mean LogID as specified in RFC 6962.
 */
CFDictionaryRef SecCertificateCopyCTLogForKeyID(CFDataRef keyID)
    __OSX_AVAILABLE_STARTING(__MAC_10_15, __IPHONE_13_0);

/* Return the auth capabilities bitmask from the iAP marker extension */
CF_RETURNS_RETAINED CFDataRef SecCertificateCopyiAPAuthCapabilities(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

typedef CF_ENUM(uint32_t, SeciAuthVersion) {
    kSeciAuthInvalid = 0,
    kSeciAuthVersion1 = 1, /* unused */
    kSeciAuthVersion2 = 2,
    kSeciAuthVersion3 = 3,
    kSeciAuthVersionSW = 4,
} __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/* Return the iAuth version indicated by the certificate. This function does
 * not guarantee that the certificate is valid, so the caller must still call
 * SecTrustEvaluate to guarantee that the certificate was properly issued */
SeciAuthVersion SecCertificateGetiAuthVersion(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/* Return the normalized name or NULL if it fails to parse */
CFDataRef SecDistinguishedNameCopyNormalizedSequence(CFDataRef distinguished_name)
    __OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_11_0);

/* Returns the Subject Key ID extension from the certificate or NULL if none */
CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_11_0);

/* Returns an array of SecCertificateRefs containing the iPhone Device CA and
 * its parent certificates. This interface is meant as a workaround and should
 * not be used without consulting the Security team. */
CFArrayRef SecCertificateCopyiPhoneDeviceCAChain(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_11_0);

typedef CF_ENUM(uint32_t, SeciAPSWAuthCapabilitiesType) {
    kSeciAPSWAuthGeneralCapabilities = 0,
    kSeciAPSWAuthAirPlayCapabilities = 1,
    kSeciAPSWAuthHomeKitCapabilities = 2,
} __OSX_AVAILABLE_STARTING(__MAC_10_13_4, __IPHONE_11_3);

/* Return the iAP SW Auth capabilities bitmask from the specificed
 * SeciAPSWAuthCapabilitiesType type marker extensions. */
CF_RETURNS_RETAINED
CFDataRef SecCertificateCopyiAPSWAuthCapabilities(SecCertificateRef certificate,
                                                  SeciAPSWAuthCapabilitiesType type)
    __OSX_AVAILABLE_STARTING(__MAC_10_13_4, __IPHONE_11_3);

/*!
 @function SecCertificateCopyExtensionValue
 @abstract Return the value in an extension of a certificate.
 @param certificate A reference to the certificate containing the desired extension
 @param extensionOID A CFData containing the binary value of ObjectIdentifier of the
 desired extension or a CFString containing the decimal value of the ObjectIdentifier.
 @param isCritical On return, a boolean value representing whether the extension was critical.
 @result If an extension exists in the certificate with the extensionOID, the returned CFData
 is the (unparsed) Value of the extension.
 @discussion If the certificate has multiple extensions with the same extension OID, the first
 extension with the input OID is returned.
 */
CF_RETURNS_RETAINED
CFDataRef SecCertificateCopyExtensionValue(SecCertificateRef certificate,
                                           CFTypeRef extensionOID, bool *isCritical)
    __OSX_AVAILABLE_STARTING(__MAC_10_13_4, __IPHONE_11_3);

/* Return an array of CFURLRefs each of which is an ocspResponder for this
 certificate. */
CFArrayRef SecCertificateGetOCSPResponders(SecCertificateRef certificate)
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));


/* Return the component type string in a component certificate. */
CF_RETURNS_RETAINED
CFStringRef SecCertificateCopyComponentType(SecCertificateRef certificate)
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

bool SecCertificateGetDeveloperIDDate(SecCertificateRef certificate, CFAbsoluteTime *time, CFErrorRef * CF_RETURNS_RETAINED error);

CFAbsoluteTime SecAbsoluteTimeFromDateContentWithError(DERTag tag, const uint8_t *bytes, size_t length, CFErrorRef *error);

/* Return the (last) attribute value from the Subject DN with the indicated Attribute OID.
 * This suits as a replacement for SecCertificateCopySubjectComponent */
CFStringRef SecCertificateCopySubjectAttributeValue(SecCertificateRef cert, DERItem *attributeOID)
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/* Return the external roots (for use with SecTrustSetAnchorCertificates) */
CFArrayRef SecCertificateCopyAppleExternalRoots(void)
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/*
 * Legacy functions (OS X only)
 */
#if SEC_OS_OSX
#include <Security/cssmtype.h>
#include <Security/x509defs.h>

/* Given a unified SecCertificateRef, return a copy with a legacy
 C++ ItemImpl-based Certificate instance. Only for internal use;
 legacy references cannot be used by SecCertificate API functions. */
SecCertificateRef SecCertificateCreateItemImplInstance(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);

/* Inverse of above; convert legacy Certificate instance to new ref. */
SecCertificateRef SecCertificateCreateFromItemImplInstance(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);


/* Convenience function to determine type of certificate instance. */
Boolean SecCertificateIsItemImplInstance(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);

/* Given a legacy C++ ItemImpl-based Certificate instance obtained with
 SecCertificateCreateItemImplInstance, return its clHandle pointer.
 Only for internal use. */
OSStatus SecCertificateGetCLHandle_legacy(SecCertificateRef certificate, CSSM_CL_HANDLE *clHandle)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);

/* Deprecated; use SecCertificateCopyCommonName() instead. */
OSStatus SecCertificateGetCommonName(SecCertificateRef certificate, CFStringRef *commonName)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_5, __IPHONE_NA, __IPHONE_NA, "SecCertificateGetCommonName is deprecated. Use SecCertificateCopyCommonName instead.");

/* Deprecated; use SecCertificateCopyEmailAddresses() instead. */
/* This should have been Copy instead of Get since the returned address is not autoreleased. */
OSStatus SecCertificateGetEmailAddress(SecCertificateRef certificate, CFStringRef *emailAddress)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_5, __IPHONE_NA, __IPHONE_NA, "SecCertificateGetEmailAddress is deprecated. Use SecCertificateCopyEmailAddresses instead.");

/*
 * Private API to infer a display name for a SecCertificateRef which
 * may or may not be in a keychain.
 */
OSStatus SecCertificateInferLabel(SecCertificateRef certificate, CFStringRef *label);

/*
 * Subset of the above, useful for both certs and CRLs.
 * Infer printable label for a given an CSSM_X509_NAME. Returns NULL
 * if no appropriate printable name found.
 */
const CSSM_DATA *SecInferLabelFromX509Name(
     const CSSM_X509_NAME *x509Name)
    DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/* Accessors for fields in the cached certificate */

/*!
     @function SecCertificateCopyFieldValues
     @abstract Retrieves the values for a particular field in a given certificate.
    @param certificate A valid SecCertificateRef to the certificate.
    @param field Pointer to the OID whose values should be returned.
    @param fieldValues On return, a zero terminated list of CSSM_DATA_PTR's.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion Return a zero terminated list of CSSM_DATA_PTR's with the
     values of the field specified by field.  Caller must call
     SecCertificateReleaseFieldValues to free the storage allocated by this call.
*/
OSStatus SecCertificateCopyFieldValues(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR **fieldValues)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateCopyFieldValues is deprecated. Use SecCertificateCopyValues instead.");

/*!
     @function SecCertificateReleaseFieldValues
     @abstract Release the storage associated with the values returned by SecCertificateCopyFieldValues.
    @param certificate A valid SecCertificateRef to the certificate.
    @param field Pointer to the OID whose values were returned by SecCertificateCopyFieldValues.
    @param fieldValues Pointer to a zero terminated list of CSSM_DATA_PTR's.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion Release the storage associated with the values returned by SecCertificateCopyFieldValues.
*/
OSStatus SecCertificateReleaseFieldValues(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR *fieldValues)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateReleaseFieldValues is deprecated. Use SecCertificateCopyValues instead.");

/*!
     @function SecCertificateCopyFirstFieldValue
     @abstract Return a CSSM_DATA_PTR with the value of the first field specified by field.
    @param certificate A valid SecCertificateRef to the certificate.
    @param field Pointer to the OID whose value should be returned.
    @param fieldValue On return, a CSSM_DATA_PTR to the field data.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion Return a CSSM_DATA_PTR with the value of the first field specified by field.  Caller must call
     SecCertificateReleaseFieldValue to free the storage allocated by this call.
*/
OSStatus SecCertificateCopyFirstFieldValue(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR *fieldValue)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateCopyFirstFieldValue is deprecated. Use SecCertificateCopyValues instead.");

/*!
     @function SecCertificateReleaseFirstFieldValue
     @abstract Release the storage associated with the values returned by SecCertificateCopyFirstFieldValue.
    @param certificate A valid SecCertificateRef to the certificate.
    @param field Pointer to the OID whose values were returned by SecCertificateCopyFieldValue.
    @param fieldValue The field data to release.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion Release the storage associated with the values returned by SecCertificateCopyFieldValue.
*/
OSStatus SecCertificateReleaseFirstFieldValue(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR fieldValue)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateReleaseFirstFieldValue is deprecated. Use SecCertificateCopyValues instead.");

/*!
    @function SecCertificateCopySubjectComponent
    @abstract Retrieves a component of the subject distinguished name of a given certificate.
    @param certificate A reference to the certificate from which to retrieve the common name.
     @param component A component oid naming the component desired. See <Security/oidsattr.h>.
    @param result On return, a reference to the string form of the component, if present in the subject.
          Your code must release this reference by calling the CFRelease function.
    @result A result code. See "Security Error Codes" (SecBase.h).
 */
OSStatus SecCertificateCopySubjectComponent(SecCertificateRef certificate, const CSSM_OID *component,
     CFStringRef *result)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateCopySubjectComponent is deprecated. Use SecCertificateCopySubjectAttributeValue instead.");

/*     Convenience functions for searching.
 */
OSStatus SecCertificateFindByIssuerAndSN(CFTypeRef keychainOrArray, const CSSM_DATA *issuer,
                                         const CSSM_DATA *serialNumber,      SecCertificateRef *certificate)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateFindByIssuerAndSN is deprecated. Use SecItemCopyMatching instead.");

OSStatus SecCertificateFindBySubjectKeyID(CFTypeRef keychainOrArray, const CSSM_DATA *subjectKeyID,
                                          SecCertificateRef *certificate)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateFindBySubjectKeyID is deprecated. Use SecItemCopyMatching instead.");

OSStatus SecCertificateFindByEmail(CFTypeRef keychainOrArray, const char *emailAddress,
                                   SecCertificateRef *certificate)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecCertificateFindByEmail is deprecated. Use SecItemCopyMatching instead.");

/* These should go to SecKeychainSearchPriv.h. */
OSStatus SecKeychainSearchCreateForCertificateByIssuerAndSN(CFTypeRef keychainOrArray, const CSSM_DATA *issuer,
                                                            const CSSM_DATA *serialNumber, SecKeychainSearchRef *searchRef)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecKeychainSearchCreateForCertificateByIssuerAndSN is deprecated. Use SecItemCopyMatching instead.");

OSStatus SecKeychainSearchCreateForCertificateByIssuerAndSN_CF(CFTypeRef keychainOrArray, CFDataRef issuer,
                                                               CFDataRef serialNumber, SecKeychainSearchRef *searchRef)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecKeychainSearchCreateForCertificateByIssuerAndSN_CF is deprecated. Use SecItemCopyMatching instead.");

OSStatus SecKeychainSearchCreateForCertificateBySubjectKeyID(CFTypeRef keychainOrArray, const CSSM_DATA *subjectKeyID,
                                                             SecKeychainSearchRef *searchRef)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecKeychainSearchCreateForCertificateBySubjectKeyID is deprecated. Use SecItemCopyMatching instead.");

OSStatus SecKeychainSearchCreateForCertificateByEmail(CFTypeRef keychainOrArray, const char *emailAddress,
                                                      SecKeychainSearchRef *searchRef)
    __OSX_AVAILABLE_BUT_DEPRECATED_MSG(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA, "SecKeychainSearchCreateForCertificateByEmail is deprecated. Use SecItemCopyMatching instead.");

/* Convenience function for generating digests; should be moved elsewhere. */
CSSM_RETURN SecDigestGetData(CSSM_ALGORITHMS alg, CSSM_DATA* digest, const CSSM_DATA* data)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_0, __MAC_10_12_4, __IPHONE_NA, __IPHONE_NA);

/* Return true iff certificate is valid as of verifyTime. */
/* DEPRECATED: Use SecCertificateIsValid instead. */
bool SecCertificateIsValidX(SecCertificateRef certificate, CFAbsoluteTime verifyTime)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_7, __MAC_10_9, __IPHONE_NA, __IPHONE_NA);

/*!
  @function SecCertificateCopyPublicKeySHA1DigestFromCertificateData
  @abstract Returns the SHA1 hash of the public key of a certificate or NULL
  @param allocator CFAllocator to allocate the certificate with.
  @param der_certificate DER encoded X.509 certificate.
  @result SHA1 hash of the public key of a certificate or NULL
*/
CFDataRef SecCertificateCopyPublicKeySHA1DigestFromCertificateData(CFAllocatorRef allocator,
                                                                   CFDataRef der_certificate)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_7, __MAC_10_13_2, __IPHONE_NA, __IPHONE_NA); // Likely incorrect.

#endif /* SEC_OS_OSX */

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEPRIV_H_ */
