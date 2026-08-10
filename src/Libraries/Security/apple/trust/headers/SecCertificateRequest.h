/*
 * Copyright (c) 2002-2017 Apple Inc. All Rights Reserved.
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
     @header SecCertificateRequest
     SecCertificateRequest implements a way to issue a certificate request to a
     certificate authority.
*/

#ifndef _SECURITY_SECCERTIFICATEREQUEST_H_
#define _SECURITY_SECCERTIFICATEREQUEST_H_

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCMS.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

extern const CFStringRef kSecOidCommonName;
extern const CFStringRef kSecOidCountryName;
extern const CFStringRef kSecOidStateProvinceName;
extern const CFStringRef kSecOidLocalityName;
extern const CFStringRef kSecOidOrganization;
extern const CFStringRef kSecOidOrganizationalUnit;

extern const unsigned char SecASN1PrintableString;
extern const unsigned char SecASN1UTF8String;

/*
        Parameter keys for certificate request generation:
        @param kSecCSRChallengePassword CFStringRef
            conversion to PrintableString or UTF8String needs to be possible.
        @param kSecCertificateKeyUsage  CFNumberRef
            with key usage mask using kSecKeyUsage constants.
        @param kSecSubjectAltName       CFDictionaryRef
            with keys defined below.
        @param kSecCSRBasicContraintsPathLen    CFNumberRef
            if set will include basic constraints and mark it as
            a CA cert.  If 0 <= number < 256, specifies path length, otherwise
            path length will be omitted.  Basic contraints will always be
            marked critical.
        @param kSecCertificateExtensions     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value in binary (CFDataRef) or
            appropriate string (CFStringRef) type (based on used character set).
        @param kSecCertificateExtensionsEncoded     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value.  It is assumed that the value
            is a CFDataRef and is already properly encoded.  This value will be
            placed straight into the extension value OCTET STRING.
         @param kSecCMSSignHashAlgorithm    CFStringRef
             (Declared in SecCMS.h)
             if set, determines the hash algorithm used to create the signing
             request or certificate. If this parameter is omitted, the default
             hash algorithm will be used (SHA1 for RSA and SHA256 for ECDSA).
             Supported digest algorithm strings are defined in
             SecCMS.h, e.g. kSecCMSHashingAlgorithmSHA256;.
*/
extern const CFStringRef kSecCSRChallengePassword;
extern const CFStringRef kSecSubjectAltName;
extern const CFStringRef kSecCertificateKeyUsage;
extern const CFStringRef kSecCSRBasicContraintsPathLen;
extern const CFStringRef kSecCertificateExtensions;
extern const CFStringRef kSecCertificateExtensionsEncoded;

/*
 Keys for kSecSubjectAltName dictionaries:
 @param kSecSubjectAltNameDNSName CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single DNS name,
     or a CFArrayRef of CFStringRefs, each containing a single DNS Name.
 @param kkSecSubjectAltNameEmailAddress CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single email
     address (RFC 822 Name), or a CFArrayRef of CFStringRefs, each containing a
     single email address.
 @param kSecSubjectAltNameURI CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single URI,
     or a CFArrayRef of CFStringRefs, each containing a single URI.
 @param kSecSubjectAltNameNTPrincipalName CFStringRef
     The value for this key is a CFStringRef containing the NTPrincipalName.
*/
extern const CFStringRef kSecSubjectAltNameDNSName;
extern const CFStringRef kSecSubjectAltNameEmailAddress;
extern const CFStringRef kSecSubjectAltNameURI;
extern const CFStringRef kSecSubjectAltNameNTPrincipalName;

typedef struct {
    CFTypeRef oid;    /* kSecOid constant or CFDataRef with oid */
    unsigned char type; /* currently only SecASN1PrintableString or SecASN1UTF8String */
    CFTypeRef value;    /* CFStringRef -> ASCII, UTF8, CFDataRef -> binary */
} SecATV;

typedef SecATV *SecRDN;

/*
    @function SecGenerateCertificateRequestWithParameters
    @abstract Return a newly generated CSR for subject and keypair.
    @param subject  RDNs in the subject
    @param paramters    Parameters for the CSR generation. See above.
    @param publicKey    Public key
    @param privateKey   Private key
    @result On success, a newly allocated CSR, otherwise NULL

Example for subject:
    SecATV cn[]   = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("test") }, {} };
    SecATV c[]    = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV o[]    = { { kSecOidOrganization, SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecRDN atvs[] = { cn, c, o, NULL };
*/
CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequestWithParameters(SecRDN _Nonnull * _Nonnull subject,
    CFDictionaryRef _Nullable parameters, SecKeyRef _Nullable publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

/*
 @function SecGenerateCertificateRequest
 @abstract Return a newly generated CSR for subject and keypair.
 @param subject  RDNs in the subject in array format
 @param paramters    Parameters for the CSR generation. See above.
 @param publicKey    Public key
 @param privateKey   Private key
 @result On success, a newly allocated CSR, otherwise NULL
 @discussion The subject array contains an array of the RDNS. Each RDN is
 itself an array of ATVs. Each ATV is an array of length two containing
 first the OID and then the value.

Example for subject (in Objective-C for ease of reading):
 NSArray *subject = @[
     @[@[(__bridge NSString*)kSecOidCommonName, @"test"]]
     @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
     @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
 ];
 */
CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequest(CFArrayRef subject,
    CFDictionaryRef _Nullable parameters, SecKeyRef _Nullable publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

/*
    @function SecVerifyCertificateRequest
    @abstract validate a CSR and return contained information to certify
    @param publicKey (optional/out) SecKeyRef public key to certify
    @param challenge (optional/out) CFStringRef enclosed challenge
    @param subject (optional/out) encoded subject RDNs
    @param extensions (optional/out) encoded extensions
*/
bool SecVerifyCertificateRequest(CFDataRef csr, SecKeyRef CF_RETURNS_RETAINED * _Nullable publicKey,
                                 CFStringRef CF_RETURNS_RETAINED _Nullable * _Nullable challenge,
                                 CFDataRef CF_RETURNS_RETAINED _Nullable * _Nullable subject,
                                 CFDataRef CF_RETURNS_RETAINED _Nullable * _Nullable extensions);


/*
 @function SecGenerateSelfSignedCertificate
 @abstract Return a newly generated certificate for subject and keypair.
 @param subject  RDNs in the subject in array format
 @param paramters    Parameters for the CSR generation. See above.
 @param publicKey    Public key (NOTE: This is unused)
 @param privateKey   Private key
 @result On success, a newly allocated certificate, otherwise NULL
 @discussion The subject array contains an array of the RDNS. Each RDN is
 itself an array of ATVs. Each ATV is an array of length two containing
 first the OID and then the value.

 Example for subject (in Objective-C for ease of reading):
 NSArray *subject = @[
     @[@[(__bridge NSString*)kSecOidCommonName, @"test"]]
     @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
     @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
 ];
 */
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters,
    SecKeyRef _Nullable publicKey, SecKeyRef privateKey);

/*
 @function SecIdentitySignCertificate
 @param issuer      issuer's identity (certificate/private key pair)
 @param serialno    serial number for the issued certificate
 @param publicKey   public key for the issued certificate
 @param subject     subject name for the issued certificate
 @param extensions  extensions for the issued certificate
 @param hashingAlgorithm hash algorithm to use for signature
 @result On success, a newly allocated certificate, otherwise NULL
 @discussion This call can be used in combination with SecVerifyCertificateRequest
 to generate a signed certifcate from a CSR after verifying it. The outputs
 of SecVerifyCertificateRequest may be passed as inputs to this function.

 The subject may be an array, as specified in SecCertificateGenerateRequest,
 or a data containing an encoded subject sequence, as specified by RFC 5280.

 Supported digest algorithm strings are defined in SecCMS.h, e.g.
 kSecCMSHashingAlgorithmSHA256.
 */
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecIdentitySignCertificate(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef _Nullable extensions);

CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecIdentitySignCertificateWithAlgorithm(SecIdentityRef issuer, CFDataRef serialno,
   SecKeyRef publicKey, CFTypeRef subject, CFTypeRef _Nullable extensions, CFStringRef _Nullable hashingAlgorithm);

/* PRIVATE */

CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequestSubject(SecCertificateRef ca_certificate, CFArrayRef subject);

CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* _SECURITY_SECCERTIFICATEREQUEST_H_ */
