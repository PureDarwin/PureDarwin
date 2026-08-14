/*
 *  Copyright (c) 2004-2018 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecCmsSignerInfo.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSSIGNERINFO_H_
#define _SECURITY_SECCMSSIGNERINFO_H_  1

#include <Security/SecCmsBase.h>

#include <Security/SecTrust.h>
#include <CoreFoundation/CFDate.h>

__BEGIN_DECLS

#if TARGET_OS_OSX
extern SecCmsSignerInfoRef
SecCmsSignerInfoCreate(SecCmsMessageRef cmsg, SecIdentityRef identity, SECOidTag digestalgtag)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);

#else // !TARGET_OSX

extern SecCmsSignerInfoRef
SecCmsSignerInfoCreate(SecCmsSignedDataRef sigd, SecIdentityRef identity, SECOidTag digestalgtag)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern SecCmsSignerInfoRef
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg, CSSM_DATA_PTR subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
extern SecCmsSignerInfoRef
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsSignedDataRef sigd, const SecAsn1Item *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
/*!
     @function
     @abstract Destroy a SignerInfo data structure.
 */
extern void
SecCmsSignerInfoDestroy(SecCmsSignerInfoRef si)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
#endif

/*!
    @function
 */
extern SecCmsVerificationStatus
SecCmsSignerInfoGetVerificationStatus(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern SECOidData *
SecCmsSignerInfoGetDigestAlg(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern SECOidTag
SecCmsSignerInfoGetDigestAlgTag(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern CFArrayRef
SecCmsSignerInfoGetCertList(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Return the signing time, in UTCTime format, of a CMS signerInfo.
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a pointer to XXXX (what?)
    @result A return value of NULL is an error.
 */
extern OSStatus
SecCmsSignerInfoGetSigningTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime *stime);

/*!
     @function
     @abstract Return the data in the signed Codesigning Hash Agility attribute.
     @param sinfo SignerInfo data for this signer, pointer to a CFDataRef for attribute value
     @discussion Returns a CFDataRef containing the value of the attribute
     @result A return value of SECFailure is an error.
 */
extern OSStatus
SecCmsSignerInfoGetAppleCodesigningHashAgility(SecCmsSignerInfoRef sinfo, CFDataRef *sdata);

/*!
     @function
     @abstract Return the data in the signed Codesigning Hash Agility V2 attribute.
     @param sinfo SignerInfo data for this signer, pointer to a CFDictionaryRef for attribute values
     @discussion Returns a CFDictionaryRef containing the values of the attribute. V2 encodes the
        hash agility values using DER.
     @result A return value of SECFailure is an error.
 */
extern OSStatus
SecCmsSignerInfoGetAppleCodesigningHashAgilityV2(SecCmsSignerInfoRef sinfo, CFDictionaryRef *sdict);

/*!
     @function SecCmsSignerInfoGetAppleExpirationTime
     @abstract Return the expriation time, in CFAbsoluteTime, of a CMS signerInfo.
     @param sinfo SignerInfo data for this signer.
     @discussion Returns a CFAbsoluteTime
     @result A return value of SECFailure is an error.
 */
extern OSStatus
SecCmsSignerInfoGetAppleExpirationTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime *etime);

/*!
    @function
    @abstract Return the signing cert of a CMS signerInfo.
    @discussion The certs in the enclosing SignedData must have been imported already.
 */
extern SecCertificateRef
SecCmsSignerInfoGetSigningCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Return the common name of the signer.
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a CFStringRef containing the common name of the signer.
    @result A return value of NULL is an error.
 */
extern CF_RETURNS_RETAINED CFStringRef
SecCmsSignerInfoGetSignerCommonName(SecCmsSignerInfoRef sinfo);

/*!
    @function
    @abstract Return the email address of the signer
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a CFStringRef containing the name of the signer.
    @result A return value of NULL is an error.
 */
extern CF_RETURNS_RETAINED CFStringRef
SecCmsSignerInfoGetSignerEmailAddress(SecCmsSignerInfoRef sinfo);

/*!
    @function
    @abstract Add the signing time to the authenticated (i.e. signed) attributes of "signerinfo".
    @discussion This is expected to be included in outgoing signed
                messages for email (S/MIME) but is likely useful in other situations.

                This should only be added once; a second call will do nothing.

                XXX This will probably just shove the current time into "signerinfo"
                but it will not actually get signed until the entire item is
                processed for encoding.  Is this (expected to be small) delay okay?
 */
extern OSStatus
SecCmsSignerInfoAddSigningTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t);

/*!
    @function
    @abstract Add a SMIMECapabilities attribute to the authenticated (i.e. signed) attributes of "signerinfo".
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME).
 */
extern OSStatus
SecCmsSignerInfoAddSMIMECaps(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Add a SMIMEEncryptionKeyPreferences attribute to the authenticated (i.e. signed) attributes of "signerinfo".
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME).
 */
OSStatus
SecCmsSignerInfoAddSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Add a SMIMEEncryptionKeyPreferences attribute to the authenticated (i.e. signed) attributes of "signerinfo", using the OID prefered by Microsoft.
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME), if compatibility with Microsoft mail clients is wanted.
 */
OSStatus
SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Countersign a signerinfo.
 */
extern OSStatus
SecCmsSignerInfoAddCounterSignature(SecCmsSignerInfoRef signerinfo,
				    SECOidTag digestalg, SecIdentityRef identity);

/*!
     @function
     @abstract Add the Apple Codesigning Hash Agility attribute to the authenticated (i.e. signed) attributes of "signerinfo".
     @discussion This is expected to be included in outgoing Apple code signatures.
*/
OSStatus
SecCmsSignerInfoAddAppleCodesigningHashAgility(SecCmsSignerInfoRef signerinfo, CFDataRef attrValue);

/*!
     @function
     @abstract Add the Apple Codesigning Hash Agility V2 attribute to the authenticated (i.e. signed) attributes of "signerinfo".
     @discussion This is expected to be included in outgoing Apple code signatures. V2 encodes the hash agility values using DER.
     The dictionary should have CFNumberRef keys, corresponding to SECOidTags for digest algorithms, and CFDataRef values,
     corresponding to the digest value for that digest algorithm.
 */
OSStatus
SecCmsSignerInfoAddAppleCodesigningHashAgilityV2(SecCmsSignerInfoRef signerinfo, CFDictionaryRef attrValues);

/*!
     @function SecCmsSignerInfoAddAppleExpirationTime
     @abstract Add the expiration time to the authenticated (i.e. signed) attributes of "signerinfo".
     @discussion This is expected to be included in outgoing signed messages for Asset Receipts but is likely
     useful in other situations. This should only be added once; a second call will do nothing.
     @result A result of SECFailure indicates an error adding the attribute.
 */
extern OSStatus
SecCmsSignerInfoAddAppleExpirationTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t);

/*!
    @function
    @abstract The following needs to be done in the S/MIME layer code after signature of a signerinfo has been verified.
    @param signerinfo The SecCmsSignerInfo object for which we verified the signature.
    @result The preferred encryption certificate of the user who signed this message will be added to the users default Keychain and it will be marked as the preferred certificate to use when sending that person messages from now on.
 */
extern OSStatus
SecCmsSignerInfoSaveSMIMEProfile(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Set cert chain inclusion mode for this signer.
 */
extern OSStatus
SecCmsSignerInfoIncludeCerts(SecCmsSignerInfoRef signerinfo, SecCmsCertChainMode cm, SECCertUsage usage);

/*! @functiongroup CMS misc utility functions */
/*!
    @function
    Convert a SecCmsVerificationStatus to a human readable string.
 */
extern const char *
SecCmsUtilVerificationStatusToString(SecCmsVerificationStatus vs);

/*
 * Preference domain and key for the Microsoft ECDSA compatibility flag.
 * Default if not present is TRUE, meaning we generate ECDSA-signed messages
 * which are compatible with Microsoft Entourage. FALSE means we adhere to
 * the spec (RFC 3278 section 2.1.1).
 */
#define kMSCompatibilityDomain    "com.apple.security.smime"
#define kMSCompatibilityMode    CFSTR("MSCompatibilityMode")

/*!
 @function SecCmsSignerInfoCopyCertFromEncryptionKeyPreference
 @abstract Copy the certificate specified in the encryption key preference.
 @param signerinfo The SecCmsSignerInfo object for which we verified the signature.
 @result The preferred encryption certificate of the user who signed this message, if found.
 @discussion This function should be called after the signer info has been verified.
 */
SecCertificateRef SecCmsSignerInfoCopyCertFromEncryptionKeyPreference(SecCmsSignerInfoRef signerinfo);

#if TARGET_OS_OSX
/* MARK: Timestamping support */

extern OSStatus
SecCmsSignerInfoVerifyUnAuthAttrs(SecCmsSignerInfoRef signerinfo)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

extern OSStatus
SecCmsSignerInfoVerifyUnAuthAttrsWithPolicy(SecCmsSignerInfoRef signerinfo,CFTypeRef timeStampPolicy)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern CSSM_DATA *
SecCmsSignerInfoGetEncDigest(SecCmsSignerInfoRef signerinfo)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
#pragma clang diagnostic pop

extern CFArrayRef
SecCmsSignerInfoGetTimestampCertList(SecCmsSignerInfoRef signerinfo)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

extern SecCertificateRef
SecCmsSignerInfoGetTimestampSigningCert(SecCmsSignerInfoRef signerinfo)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/*!
     @function
     @abstract Return the timestamp time, in UTCTime format, of a CMS signerInfo.
     @param sinfo SignerInfo data for this signer.
     @discussion Returns a pointer to XXXX (what?)
     @result A return value of NULL is an error.
 */
OSStatus
SecCmsSignerInfoGetTimestampTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime *stime)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/*!
     @function
     @abstract Return the timestamp time, in UTCTime format, of a CMS signerInfo.
     @param sinfo SignerInfo data for this signer, timeStampPolicy the policy to verify the timestamp signer
     @discussion Returns a pointer to XXXX (what?)
     @result A return value of NULL is an error.
 */
OSStatus
SecCmsSignerInfoGetTimestampTimeWithPolicy(SecCmsSignerInfoRef sinfo, CFTypeRef timeStampPolicy, CFAbsoluteTime *stime)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/*!
     @function
     @abstract Create a timestamp unsigned attribute with a TimeStampToken.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
OSStatus
SecCmsSignerInfoAddTimeStamp(SecCmsSignerInfoRef signerinfo, CSSM_DATA *tstoken)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);
#pragma clang diagnostic pop
#endif // TARGET_OS_OSX

__END_DECLS

#endif /* _SECURITY_SECCMSSIGNERINFO_H_ */
