/*
 * Copyright (c) 2003-2017 Apple Inc. All Rights Reserved.
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
 @header SecPolicyPriv
 The functions provided in SecPolicyPriv provide an interface to various
 X.509 certificate trust policies.
*/

#ifndef _SECURITY_SECPOLICYPRIV_H_
#define _SECURITY_SECPOLICYPRIV_H_

#include <Security/SecBase.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <Availability.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

/*!
	@enum Policy Constants (Private)
	@discussion Predefined constants used to specify a policy.
 */
extern const CFStringRef kSecPolicyAppleMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleTestMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleQAProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleServerAuthentication
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
extern const CFStringRef kSecPolicyAppleIDValidationRecordSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSMPEncryption
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);
extern const CFStringRef kSecPolicyAppleTestSMPEncryption
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);
extern const CFStringRef kSecPolicyApplePCSEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_7_0);
extern const CFStringRef kSecPolicyApplePPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleTestPPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleSWUpdateSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyApplePackageSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleOSXProvisioningProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleATVVPNProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleAST2DiagnosticsServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);
extern const CFStringRef kSecPolicyAppleEscrowProxyServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleFMiPServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMMCService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGSService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyApplePPQService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleHomeKitServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneActivation
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneDeviceCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleFactoryDeviceCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiAP
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiTunesStoreURLBag
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneProfileApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneProvisioningProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleLockdownPairing
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleURLBag
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleOTATasking
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMobileAsset
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDAuthority
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGenericApplePinned
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGenericAppleSSLPinned
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSoftwareSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleExternalDeveloper
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleOCSPSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDSService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDSServiceContext
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyApplePushService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleLegacyPushService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleTVOSApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleUniqueDeviceIdentifierCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleEscrowProxyCompatibilityServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMMCSCompatibilityServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSecureIOStaticAsset
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);
extern const CFStringRef kSecPolicyAppleWarsaw
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);
extern const CFStringRef kSecPolicyAppleiCloudSetupServerAuth
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);
extern const CFStringRef kSecPolicyAppleiCloudSetupCompatibilityServerAuth
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);
extern const CFStringRef kSecPolicyAppleAppTransportSecurity
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleMobileSoftwareUpdate
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleMobileAssetDevelopment
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleMacOSProfileApplicationSigning
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleBasicAttestationSystem
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleBasicAttestationUser
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleiPhoneVPNApplicationSigning
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyAppleiAPSWAuth
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));
extern const CFStringRef kSecPolicyAppleDemoDigitalCatalog
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));
extern const CFStringRef kSecPolicyAppleAssetReceipt
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));
extern const CFStringRef kSecPolicyAppleDeveloperIDPlusTicket
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));
extern const CFStringRef kSecPolicyAppleComponentCertificate
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));
extern const CFStringRef kSecPolicyAppleKeyTransparency
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));
extern const CFStringRef kSecPolicyAppleLegacySSL
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));
extern const CFStringRef kSecPolicyAppleAlisha
    API_AVAILABLE(macos(10.15.4), ios(13.4), watchos(6.2), tvos(13.4));
extern const CFStringRef kSecPolicyAppleMeasuredBootPolicySigning
    API_AVAILABLE(macos(10.15.4), ios(13.4), watchos(6.2), tvos(13.4));
extern const CFStringRef kSecPolicyApplePayQRCodeEncryption
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));
extern const CFStringRef kSecPolicyApplePayQRCodeSigning
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));
extern const CFStringRef kSecPolicyAppleAccessoryUpdateSigning
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));
extern const CFStringRef kSecPolicyAppleEscrowServiceIdKeySigning
    API_AVAILABLE(macos(10.15.6), ios(13.5.5));
extern const CFStringRef kSecPolicyApplePCSEscrowServiceIdKeySigning
    API_AVAILABLE(macos(10.15.6), ios(13.5.5));
extern const CFStringRef kSecPolicyAppleAggregateMetricTransparency
    API_AVAILABLE(macos(10.15.6), ios(13.6), watchos(6.2), tvos(13.4));
extern const CFStringRef kSecPolicyAppleAggregateMetricEncryption
    API_AVAILABLE(macos(11.1), ios(14.3), watchos(7.2), tvos(14.3));
extern const CFStringRef kSecPolicyApplePayModelSigning
    API_AVAILABLE(macos(11.3), ios(14.5), watchos(7.4), tvos(14.5));


/*!
	@enum Policy Name Constants (Private)
	@discussion Predefined constants used to specify a SSL Pinning policy.
    To be used with SecTrustSetPolicyName.
    @constant kSecPolicyNameAppleAST2Service
    @constant kSecPolicyNameAppleEscrowProxyService
    @constant kSecPolicyNameAppleFMiPService
    @constant kSecPolicyNameAppleGSService
    @constant kSecPolicyNameAppleHomeKitService
    @constant kSecPolicyNameAppleiCloudSetupService
    @constant kSecPolicyNameAppleIDSService
    @constant kSecPolicyNameAppleMMCSService
    @constant kSecPolicyNameApplePPQService
    @constant kSecPolicyNameApplePushService
    @constant kSecPolicyNameAppleAIDCService
    @constant kSecPolicyNameAppleMapsService
    @constant kSecPolicyNameAppleHealthProviderService
    @constant kSecPolicyNameAppleParsecService
    @constant kSecPolicyNameAppleAMPService
    @constant kSecPolicyNameAppleSiriService
    @constant kSecPolicyNameAppleHomeAppClipUploadService
    @constant kSecPolicyNameAppleUpdatesService
    @constant kSecPolicyNameApplePushCertPortal
 */
extern const CFStringRef kSecPolicyNameAppleAST2Service
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleEscrowProxyService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleFMiPService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleGSService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleHomeKitService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleiCloudSetupService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleIDSService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleMMCSService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameApplePPQService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameApplePushService
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);
extern const CFStringRef kSecPolicyNameAppleAIDCService
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);
extern const CFStringRef kSecPolicyNameAppleMapsService
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);
extern const CFStringRef kSecPolicyNameAppleHealthProviderService
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);
extern const CFStringRef kSecPolicyNameAppleParsecService
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);
extern const CFStringRef kSecPolicyNameAppleAMPService
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));
extern const CFStringRef kSecPolicyNameAppleSiriService
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));
extern const CFStringRef kSecPolicyNameAppleHomeAppClipUploadService
    API_AVAILABLE(macos(10.15.1), ios(13.2), watchos(6.1), tvos(13.1));
extern const CFStringRef kSecPolicyNameAppleUpdatesService
    API_AVAILABLE(macos(10.15.4), ios(13.4), watchos(6.2), tvos(13.4));
extern const CFStringRef kSecPolicyNameApplePushCertPortal
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/*!
 @enum Policy Value Constants
 @abstract Predefined property key constants used to get or set values in
 a dictionary for a policy instance.
 @discussion
    All policies will have the following read-only value:
        kSecPolicyOid       (the policy object identifier)

    Additional policy values which your code can optionally set:
        kSecPolicyName              (name which must be matched)
        kSecPolicyClient            (evaluate for client, rather than server)
        kSecPolicyRevocationFlags   (only valid for a revocation policy)
        kSecPolicyRevocationFlags   (only valid for a revocation policy)
        kSecPolicyTeamIdentifier    (only valid for a Passbook signing policy)
        kSecPolicyContext           (valid for policies below that take a context parameter)
        kSecPolicyPolicyName        (only valid for GenericApplePinned or
                                    GenericAppleSSLPinned policies)
        kSecPolicyIntermediateMarkerOid (only valid for GenericApplePinned or
                                        GenericAppleSSLPinned policies)
        kSecPolicyLeafMarkerOid     (only valid for GenericApplePinned or
                                    GenericAppleSSLPinned policies)
        kSecPolicyRootDigest        (only valid for the UniqueDeviceCertificate policy)

 @constant kSecPolicyContext Specifies a CFDictionaryRef with keys and values
    specified by the particular SecPolicyCreate function.
 @constant kSecPolicyPolicyName Specifies a CFStringRef of the name of the
    desired policy result.
 @constant kSecPolicyIntermediateMarkerOid Specifies a CFStringRef of the
    marker OID (in decimal format) required in the intermediate certificate.
 @constant kSecPolicyLeafMarkerOid Specifies a CFStringRef of the
    marker OID (in decimal format) required in the leaf certificate.
 @constant kSecPolicyRootDigest Specifies a CFDataRef of digest required to
    match the SHA-256 of the root certificate.
 */
extern const CFStringRef kSecPolicyContext
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyPolicyName
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyIntermediateMarkerOid
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyLeafMarkerOid
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyRootDigest
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @enum Revocation Policy Constants
 @abstract Predefined constants which allow you to specify how revocation
 checking will be performed for a trust evaluation.
 @constant kSecRevocationOnlineCheck If this flag is set, perform an online
 revocation check, ignoring cached revocation results. This flag will not force
 an online check if an online check was done within the last 5 minutes. Online
 checks are only applicable to OCSP; this constant will not force a fresh
 CRL download.
 @constant kSecRevocationCheckIfTrusted If this flag is set, perform network-based
 revocation checks only if the chain has no other validation errors. This flag
 overrides SecTrustSetNetworkFetchAllowed and kSecRevocationNetworkAccessDisabled
 for revocation checking (but not for intermediate fetching).
 Note that this flag's behavior is not default because revoked certs produce Fatal
 trust results, whereas most checks produce Recoverable trust results. If we skip
 revocation checks on untrusted chains, the user may be able to ignore the failures
 of a revoked cert.
 */
CF_ENUM(CFOptionFlags) {
    kSecRevocationOnlineCheck = (1 << 5),
    kSecRevocationCheckIfTrusted = (1 << 6),
};

/*!
 @function SecPolicyCreateApplePinned
 @abstract Returns a policy object for verifying Apple certificates.
 @param policyName A string that identifies the policy name.
 @param intermediateMarkerOID A string containing the decimal representation of the
 extension OID in the intermediate certificate.
 @param leafMarkerOID A string containing the decimal representation of the extension OID
 in the leaf certificate.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching the intermediateMarkerOID
    parameter.
    * The leaf has a marker extension with OID matching the leafMarkerOID parameter.
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePinned(CFStringRef policyName,
                                        CFStringRef intermediateMarkerOID, CFStringRef leafMarkerOID)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleSSLPinned
 @abstract Returns a policy object for verifying Apple SSL certificates.
 @param policyName A string that identifies the service/policy name.
 @param hostname hostname to verify the certificate name against.
 @param intermediateMarkerOID A string containing the decimal representation of the
 extension OID in the intermediate certificate. If NULL is passed, the default OID of
 1.2.840.113635.100.6.2.12 is checked.
 @param leafMarkerOID A string containing the decimal representation of the extension OID
 in the leaf certificate.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching the intermediateMarkerOID
    parameter, or 1.2.840.113635.100.6.2.12 if NULL is passed.
    * The leaf has a marker extension with OID matching the leafMarkerOID parameter.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSSLPinned(CFStringRef policyName, CFStringRef hostname,
                                           CFStringRef __nullable intermediateMarkerOID, CFStringRef leafMarkerOID)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateiPhoneActivation
 @abstract Returns a policy object for verifying iPhone Activation
 certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "iPhone Activation".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneActivation(void);

/*!
 @function SecPolicyCreateiPhoneDeviceCertificate
 @abstract Returns a policy object for verifying iPhone Device certificate
 chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs
     * There are exactly 4 certs in chain.
     * The first intermediate has Common Name "Apple iPhone Device CA".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneDeviceCertificate(void);

/*!
 @function SecPolicyCreateFactoryDeviceCertificate
 @abstract Returns a policy object for verifying Factory Device certificate
 chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to the Factory Device CA.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateFactoryDeviceCertificate(void);

/*!
 @function SecPolicyCreateiAP
 @abstract Returns a policy object for verifying iAP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The leaf has notBefore date after 5/31/2006 midnight GMT.
     * The leaf has Common Name beginning with "IPA_".
 The intended use of this policy is that the caller pass in the
 intermediates for iAP1 and iAP2 to SecTrustSetAnchorCertificates().
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiAP(void);

/*!
 @function SecPolicyCreateiTunesStoreURLBag
 @abstract Returns a policy object for verifying iTunes Store URL bag
 certificates.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to the iTMS CA.
     * There are exactly 2 certs in the chain.
     * The leaf has Organization "Apple Inc.".
     * The leaf has Common Name "iTunes Store URL Bag".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiTunesStoreURLBag(void);

/*!
 @function SecPolicyCreateEAP
 @abstract Returns a policy object for verifying for 802.1x/EAP certificates.
 @param server Passing true for this parameter create a policy for EAP
 server certificates.
 @param trustedServerNames Optional; if present, the hostname in the leaf
 certificate must be in the trustedServerNames list.  Note that contrary
 to all other policies the trustedServerNames list entries can have wildcards
 whilst the certificate cannot.  This matches the existing deployments.
 @discussion This policy uses the Basic X.509 policy with validity check but
 disallowing network fetching. If trustedServerNames param is non-null, the
 ExtendedKeyUsage extension, if present, of the leaf certificate is verified
 to contain either the ServerAuth OID, if the server param is true or
 ClientAuth OID, otherwise.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateEAP(Boolean server, CFArrayRef __nullable trustedServerNames);

/*!
 @function SecPolicyCreateIPSec
 @abstract Returns a policy object for evaluating IPSec certificate chains.
 @param server Passing true for this parameter create a policy for IPSec
 server certificates.
 @param hostname Optional; if present, the policy will require the specified
 hostname or ip address to match the hostname in the leaf certificate.
 @discussion This policy uses the Basic X.509 policy with validity check.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateIPSec(Boolean server, CFStringRef __nullable  hostname);

/*!
 @function SecPolicyCreateAppleSWUpdateSigning
 @abstract Returns a policy object for evaluating SW update signing certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate ExtendedKeyUsage Extension contains 1.2.840.113635.100.4.1.
     * The leaf ExtendedKeyUsage extension contains 1.2.840.113635.100.4.1.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSWUpdateSigning(void);

/*!
 @function SecPolicyCreateApplePackageSigning
 @abstract Returns a policy object for evaluating installer package signing certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The leaf KeyUsage extension has the digital signature bit set.
     * The leaf ExtendedKeyUsage extension has the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePackageSigning(void);

/*!
 @function SecPolicyCreateiPhoneApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures.  This is for apps signed directly by the app store.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has Common Name "Apple iPhone Certification Authority".
     * The leaf has Common Name "Apple iPhone OS Application Signing".
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.3 or OID
     1.2.840.113635.100.6.1.6.
     * The leaf has ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID
       or the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneApplicationSigning(void);

/*!
 @function SecPolicyCreateiPhoneVPNApplicationSigning
 @abstract Returns a policy object for evaluating signed VPN application
 signatures.  This is for VPN plugins signed directly by the VPN team.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has Common Name "Apple iPhone Certification Authority".
     * The leaf has Common Name "Apple iPhone OS Application Signing".
     * The leaf has a marker extension with 1.2.840.113635.100.6.1.6.
     * The leaf has ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID
     or the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneVPNApplicationSigning(void)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateiPhoneProfileApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures. This policy is for certificates inside a UPP or regular
 profile.
 @discussion This policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching 1.2.840.113635.100.6.2.1 (WWDR CA).
    * The leaf has a marker extension with OID matching one of the following:
            * 1.2.840.113635.100.6.1.2  ("iPhone Developer" leaf)
            * 1.2.840.113635.100.6.1.4  ("iPhone Distribution" leaf)
            * 1.2.840.113635.100.6.1.25.1 ("TestFlight" leaf)
            * On internal releases, 1.2.840.113635.100.6.1.25.2
    * The leaf has an ExtendedKeyUsage OID matching 1.3.6.1.5.5.7.3.3 (CodeSigning EKU).
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneProfileApplicationSigning(void);

/*!
 @function SecPolicyCreateMacOSProfileApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures. This policy is for certificates inside a UPP or regular
 profile.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The leaf has a marker extension with OID matching one of the following:
        * 1.2.840.113635.100.6.1.7  ("3rd Party Mac Developer Application" leaf)
        * 1.2.840.113635.100.6.1.12 ("Mac Developer" leaf)
        * 1.2.840.113635.100.6.1.13 ("Developer ID Application" leaf)
        * 1.2.840.113635.100.6.22   ("Software Signing" leaf
    * The leaf has an ExtendedKeyUsage OID matching 1.3.6.1.5.5.7.3.3 (CodeSigning EKU).
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMacOSProfileApplicationSigning(void)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateiPhoneProvisioningProfileSigning
 @abstract Returns a policy object for evaluating provisioning profile signatures.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has Common Name "Apple iPhone Certification Authority".
     * The leaf has Common Name "Apple iPhone OS Provisioning Profile Signing".
     * If the device is not a production device and is running an internal
       release, the leaf may have the Common Name "TEST Apple iPhone OS
       Provisioning Profile Signing TEST".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneProvisioningProfileSigning(void);

/*!
 @function SecPolicyCreateAppleTVOSApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures.  This is for apps signed directly by the Apple TV app store,
 and allows for both the prod and the dev/test certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
       Test roots are never permitted.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
     * The leaf has ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID or
       the CodeSigning OID.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.24 or OID
       1.2.840.113635.100.6.1.24.1.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleTVOSApplicationSigning(void);

/*!
 @function SecPolicyCreateOCSPSigner
 @abstract Returns a policy object for evaluating ocsp response signers.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have an ExtendedKeyUsage of OCSPSigning.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOCSPSigner(void);


enum {
    kSecSignSMIMEUsage = (1 << 0),
    kSecKeyEncryptSMIMEUsage = (1 << 1),
    kSecDataEncryptSMIMEUsage = (1 << 2),
    kSecKeyExchangeDecryptSMIMEUsage = (1 << 3),
    kSecKeyExchangeEncryptSMIMEUsage = (1 << 4),
    kSecKeyExchangeBothSMIMEUsage = (1 << 5),
    kSecAnyEncryptSMIME = kSecKeyEncryptSMIMEUsage | kSecDataEncryptSMIMEUsage |
        kSecKeyExchangeDecryptSMIMEUsage | kSecKeyExchangeEncryptSMIMEUsage,
    kSecIgnoreExpirationSMIMEUsage = (1 << 6)
};

/*!
 @function SecPolicyCreateSMIME
 @abstract Returns a policy object for evaluating S/MIME certificate chains.
 @param smimeUsage Pass the bitwise or of one or more kSecXXXSMIMEUsage
 flags, to indicate the intended usage of this certificate.
 @param email Optional; if present, the policy will require the specified
 email to match the email in the leaf certificate.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have
     * a KeyUsage matching the smimeUsage,
     * an ExtendedKeyUsage, if any, with the EmailProtection OID, and
     * if the email param is specified, the email address in the RFC822Name in the
       SubjectAlternativeName extension or in the Email Address field of the
       Subject Name.
 Note that temporal validity checking can be disabled with kSecIgnoreExpirationSMIMEUsage
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef __nullable email);

/*!
 @function SecPolicyCreateCodeSigning
 @abstract Returns a policy object for evaluating code signing certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have
     * a KeyUsage with both the DigitalSignature and NonRepudiation bits set, and
     * an ExtendedKeyUsage with the AnyExtendedKeyUsage OID or the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateCodeSigning(void);

/*!
 @function SecPolicyCreateLockdownPairing
 @abstract basic x509 policy for checking lockdown pairing certificate chains.
 @discussion This policy checks some of the Basic X.509 policy options with no
 validity check. It explicitly allows for empty subjects.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateLockdownPairing(void);

/*!
 @function SecPolicyCreateURLBag
 @abstract Returns a policy object for evaluating certificate chains for signing URL bags.
 @discussion This policy uses the Basic X.509 policy with no validity check and requires
 that the leaf has ExtendedKeyUsage extension with the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateURLBag(void);

/*!
 @function SecPolicyCreateOTATasking
 @abstract  Returns a policy object for evaluating certificate chains for signing OTA Tasking.
 @discussion This policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "OTA Task Signing".
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOTATasking(void);

/*!
 @function SecPolicyCreateMobileAsset
 @abstract  Returns a policy object for evaluating certificate chains for signing Mobile Assets.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "Asset Manifest Signing".
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileAsset(void);

/*!
 @function SecPolicyCreateMobileAssetDevelopment
 @abstract  Returns a policy object for evaluating certificate chains for signing development
 Mobile Assets.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.18.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.55.1.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileAssetDevelopment(void)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateAppleIDAuthorityPolicy
 @abstract Returns a policy object for evaluating certificate chains for Apple ID Authority.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate(s) has(have) a marker extension with OID 1.2.840.113635.100.6.2.3
      or OID 1.2.840.113635.100.6.2.7.
    * The leaf has a marker extension with OID 1.2.840.113635.100.4.7.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDAuthorityPolicy(void);

/*!
 @function SecPolicyCreateMacAppStoreReceipt
 @abstract Returns a policy object for evaluating certificate chains for signing
 Mac App Store Receipts.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.6.1.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.11.1.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMacAppStoreReceipt(void);

/*!
 @function SecPolicyCreatePassbookCardSigner
 @abstract Returns a policy object for evaluating certificate chains for signing Passbook cards.
 @param cardIssuer Required; must match name in marker extension.
 @param teamIdentifier Optional; if present, the policy will require the specified
 team ID to match the organizationalUnit field in the leaf certificate's subject.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.16 and containing the
      cardIssuer.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.14.
    * The leaf has a Organizational Unit matching the TeamID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer,
	CFStringRef __nullable teamIdentifier);

/*!
 @function SecPolicyCreateMobileStoreSigner
 @abstract Returns a policy object for evaluating Mobile Store certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification Authority".
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.12.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileStoreSigner(void);

/*!
 @function SecPolicyCreateTestMobileStoreSigner
 @abstract  Returns a policy object for evaluating Test Mobile Store certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification Authority".
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.12.1.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestMobileStoreSigner(void);

/*!
 @function SecPolicyCreateEscrowServiceSigner
 @abstract Returns a policy object for evaluating Escrow Service certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateEscrowServiceSigner(void);

/*!
 @function SecPolicyCreatePCSEscrowServiceSigner
 @abstract Returns a policy object for evaluating PCS Escrow Service certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreatePCSEscrowServiceSigner(void);

/*!
 @function SecPolicyCreateOSXProvisioningProfileSigning
 @abstract  Returns a policy object for evaluating certificate chains for signing OS X
 Provisioning Profiles.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.4.11.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void);

/*!
 @function SecPolicyCreateConfigurationProfileSigner
 @abstract Returns a policy object for evaluating certificate chains for signing
 Configuration Profiles.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.3.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.16, or on non-GM
     builds only, OID 1.2.840.113635.100.4.17.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateConfigurationProfileSigner(void);

/*!
 @function SecPolicyCreateQAConfigurationProfileSigner
 @abstract Returns a policy object for evaluating certificate chains for signing
 QA Configuration Profiles. On GM builds, this function returns the same
 policy as SecPolicyCreateConfigurationProfileSigner.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.3.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.17.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateQAConfigurationProfileSigner(void);

/*!
 @function SecPolicyCreateAppleIDValidationRecordSigningPolicy
 @abstract Returns a policy object for evaluating certificate chains for signing
 Apple ID Validation Records.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate(s) has(have) a marker extension with OID 1.2.840.113635.100.6.2.3
      or OID 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.25.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDValidationRecordSigningPolicy(void);

/*!
 @function SecPolicyCreateAppleSMPEncryption
 @abstract Returns a policy object for evaluating SMP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.13.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.30.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSMPEncryption(void);

/*!
 @function SecPolicyCreateTestAppleSMPEncryption
 @abstract Returns a policy object for evaluating Test SMP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to a Test Apple Root with ECC public key certificate.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Test Apple System Integration CA - ECC".
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestAppleSMPEncryption(void);

/*!
 @function SecPolicyCreateApplePPQSigning
 @abstract Returns a policy object for verifying production PPQ Signing certificates.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification
      Authority".
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.38.2.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePPQSigning(void);

/*!
 @function SecPolicyCreateTestApplePPQSigning
 @abstract Returns a policy object for verifying test PPQ Signing certificates. On
 customer builds, this function returns the same policy as SecPolicyCreateApplePPQSigning.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification
      Authority".
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.38.1.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestApplePPQSigning(void);

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.4.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.4.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef __nullable hostname);

/*!
 @function SecPolicyCreateAppleIDSServiceContext
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATIDS" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.4.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.4.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Apple Push service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATAPN" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.5.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.5.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateApplePushServiceLegacy
 @abstract Ensure we're appropriately pinned to the Push service (via Entrust)
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to an Entrust Intermediate.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the MMCS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATMMCS" with value
 Boolean true will allow Test Apple roots and test OIDs on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.11.2 or, if
    enabled, OID 1.2.840.113635.100.6.27.11.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateAppleCompatibilityMMCSService
 @abstract Ensure we're appropriately pinned to the MMCS service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.11.2 or
    OID 1.2.840.113635.100.6.27.11.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityMMCSService(CFStringRef hostname)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleGSService
 @abstract Ensure we're appropriately pinned to the GS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATGS" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.2.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateApplePPQService
 @abstract Ensure we're appropriately pinned to the PPQ service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATPPQ" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.3.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.3.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleAST2Service
 @abstract Ensure we're appropriately pinned to the AST2 Diagnostic service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATAST2" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.8.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.8.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleAST2Service(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);

/*!
 @function SecPolicyCreateAppleEscrowProxyService
 @abstract Ensure we're appropriately pinned to the iCloud Escrow Proxy service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATEscrow" with value
Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.7.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.7.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleEscrowProxyService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleCompatibilityEscrowProxyService
 @abstract Ensure we're appropriately pinned to the iCloud Escrow Proxy service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.7.2 or,
    if UAT is enabled with a defaults write (internal devices only),
    OID 1.2.840.113635.100.6.27.7.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityEscrowProxyService(CFStringRef hostname)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleFMiPService
 @abstract Ensure we're appropriately pinned to the Find My iPhone service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATFMiP" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.6.2 or,
    if Test Roots are allowed, OID 1.2.840.113635.100.6.27.6.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleFMiPService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 @param hostname Optional; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.1
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage, if any, with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef __nullable hostname);

/*!
 @function SecPolicyCreateAppleTimeStamping
 @abstract Returns a policy object for evaluating time stamping certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and requires the leaf has ExtendedKeyUsage with the TimeStamping OID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleTimeStamping(void);

/*!
 @function SecPolicyCreateApplePayIssuerEncryption
 @abstract  Returns a policy object for evaluating Apple Pay Issuer Encryption certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple Worldwide Developer Relations CA - G2".
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.39.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleATVVPNProfileSigning
 @abstract  Returns a policy object for evaluating Apple TV VPN Profile certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.43.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleATVVPNProfileSigning(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleHomeKitServerAuth
 @abstract Ensure we're appropriately pinned to the HomeKit service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.16
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.9.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleHomeKitServerAuth(CFStringRef hostname)
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);

/*!
 @function SecPolicyCreateAppleExternalDeveloper
 @abstract Returns a policy object for verifying Apple-issued external developer
 certificates.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching 1.2.840.113635.100.6.2.1
    (WWDR CA) or 1.2.840.113635.100.6.2.6 (Developer ID CA).
    * The leaf has a marker extension with OID matching one of the following:
        * 1.2.840.113635.100.6.1.2  ("iPhone Developer" leaf)
        * 1.2.840.113635.100.6.1.4  ("iPhone Distribution" leaf)
        * 1.2.840.113635.100.6.1.5  ("Safari Developer" leaf)
        * 1.2.840.113635.100.6.1.7  ("3rd Party Mac Developer Application" leaf)
        * 1.2.840.113635.100.6.1.8  ("3rd Party Mac Developer Installer" leaf)
        * 1.2.840.113635.100.6.1.12 ("Mac Developer" leaf)
        * 1.2.840.113635.100.6.1.13 ("Developer ID Application" leaf)
        * 1.2.840.113635.100.6.1.14 ("Developer ID Installer" leaf)
    * The leaf has an ExtendedKeyUsage OID matching one of the following:
        * 1.3.6.1.5.5.7.3.3         (CodeSigning EKU)
        * 1.2.840.113635.100.4.8    ("Safari Developer" EKU)
        * 1.2.840.113635.100.4.9    ("3rd Party Mac Developer Installer" EKU)
        * 1.2.840.113635.100.4.13   ("Developer ID Installer" EKU)
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleExternalDeveloper(void)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleSoftwareSigning
 @abstract Returns a policy object for verifying the Apple Software Signing certificate.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has the Common Name "Apple Code Signing Certification Authority".
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.22.
    * The leaf has an ExtendedKeyUsage OID matching 1.3.6.1.5.5.7.3.3 (Code Signing).
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSoftwareSigning(void)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyGetName
 @abstract Returns a policy's name.
 @param policy A policy reference.
 @result A policy name.
 */
__nullable CFStringRef SecPolicyGetName(SecPolicyRef policy)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyGetOidString
 @abstract Returns a policy's oid in string decimal format.
 @param policy A policy reference.
 @result A policy oid.
 */
CFStringRef SecPolicyGetOidString(SecPolicyRef policy)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleUniqueDeviceCertificate
 @abstract Returns a policy object for verifying Unique Device Identifier Certificates.
 @param testRootHash Optional; The SHA-256 fingerprint of a test root for pinning.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to the SEP Root CA. Internal releases allow the chain to be
    anchored to the testRootHash input if the value true is set for the key
    "ApplePinningAllowTestCertsUCRT" in the com.apple.security preferences for the user
    of the calling application.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.44 and value
    of "ucrt".
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.10.1.
    * RSA key sizes are disallowed. EC key sizes are P-256 or larger.
@result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleUniqueDeviceCertificate(CFDataRef __nullable testRootHash)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleWarsaw
 @abstract Returns a policy object for verifying signed Warsaw assets.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.2.14.
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.29.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleWarsaw(void)
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);

/*!
 @function SecPolicyCreateAppleSecureIOStaticAsset
 @abstract Returns a policy object for verifying signed static assets for Secure IO.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.50.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSecureIOStaticAsset(void)
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);

/*!
 @function SecPolicyCreateAppleiCloudSetupService
 @abstract Ensure we're appropriately pinned to the iCloud Setup service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATiCloudSetup" with value
 Boolean true will allow Test Apple roots and test OIDs on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.15.2 or, if
    enabled, OID 1.2.840.113635.100.6.27.15.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleiCloudSetupService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);

/*!
 @function SecPolicyCreateAppleCompatibilityiCloudSetupService
 @abstract Ensure we're appropriately pinned to the iCloud Setup service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.15.2 or
    OID 1.2.840.113635.100.6.27.15.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName extension.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityiCloudSetupService(CFStringRef hostname)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);

/*!
 @function SecPolicyCreateAppleAppTransportSecurity
 @abstract Ensure all certs in the evaluation meet ATS minimums
 @discussion This policy is meant to be used alongside an SSL policy in order to enforce App Transport Security certificate rules:
    * All certificates use either RSA key sizes of 2048-bits or larger or EC key sizes of 256-bits or larger.
    * All certificates use SHA-256 or better for signature hash algorithms.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleAppTransportSecurity(void)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateMobileSoftwareUpdate
 @abstract  Returns a policy object for evaluating certificate chains for signing Mobile Software Updates.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the Apple Root CAs.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.18.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.57.2, or on internal releases,
    1.2.840.113635.100.6.57.1.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileSoftwareUpdate(void)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateAppleBasicAttestationSystem
 @abstract Returns a policy object for verifying Basic Attestation Authority SCRT-attested certs
 @param testRootHash Optional; The SHA-256 fingerprint of a test root for pinning.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The chain is anchored to the Basic Attestation System Root CA.
     * There are exactly 3 certs in the chain.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleBasicAttestationSystem(CFDataRef __nullable testRootHash)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateAppleBasicAttestationUser
 @abstract Returns a policy object for verifying Basic Attestation Authority UCRT-attested certs
 @param testRootHash Optional; The SHA-256 fingerprint of a test root for pinning.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The chain is anchored to the Basic Attestation User Root CA.
     * There are exactly 3 certs in the chain.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleBasicAttestationUser(CFDataRef __nullable testRootHash)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecPolicyCreateiAPSWAuth
 @abstract Returns a policy object for verifying iAP Software Auth certificates
 @discussion The resulting policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * There are exactly 2 certs in the chain.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.59.1
 The intended use of this policy is that the caller pass in the
 SW Auth root to SecTrustSetAnchorCertificates().
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiAPSWAuth(void)
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);

/*!
 @function SecPolicyCreateDemoDigitalCatalog
 @abstract  Returns a policy object for evaluating certificate chains for signing Digital
 Catalog manifests for Demo units.
 @discussion This policy uses the Basic X.509 policy with validity check and
 pinning options:
    * There are exactly 3 certs in the chain.
    * The intermediate has common name "DemoUnit CA"
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.60
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateDemoDigitalCatalogSigning(void)
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);

/*!
 @function SecPolicyCreateAppleAssetReceipt
 @abstract  Returns a policy object for evaluating certificate chains for signing Asset Receipts
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.61.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleAssetReceipt(void)
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));

/*!
 @function SecPolicyCreateAppleDeveloperIDPlustTicket
 @abstract  Returns a policy object for evaluating certificate chains for signing Developer ID+ Tickets
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.17.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.30.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleDeveloperIDPlusTicket(void)
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));

/*!
 @function SecPolicyCreateiAPSWAuthWithExpiration
 @abstract Returns a policy object for verifying iAP Software Auth certificates
 @param checkExpiration Determines whether the policy checks expiration on the certificates
 @discussion The resulting policy uses the Basic X.509 policy and pinning options:
    * There are exactly 2 certs in the chain.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.59.1
 The intended use of this policy is that the caller pass in the
 SW Auth root to SecTrustSetAnchorCertificates().
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiAPSWAuthWithExpiration(bool checkExpiration)
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));

/*!
 @function SecPolicyCreateAppleFDRProvisioning
 @abstract Returns a policy object for verifying FDR Provisioning certificates
 @discussion The resulting policy uses the Basic X.509 policy with no validity check.
 The intended use of this policy is that the caller pass in the FDR root to SecTrustSetAnchorCertificates().
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleFDRProvisioning(void)
    API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));

/*!
 @function SecPolicyCreateAppleComponentCertificate
 @abstract Returns a policy object for verifying Component certs
 @param testRootHash Optional; The SHA-256 fingerprint of a test root for pinning.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to the Component Root CA.
    * There are exactly 3 certs in the chain.
    * The leaf and intermediate each have a marker extension with OID matching 1.2.840.113635.100.11.1
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleComponentCertificate(CFDataRef __nullable testRootHash)
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!
 @function SecPolicyCreateAppleKeyTransparency
 @abstract Returns a policy object for verifying Apple certificates.
 @param applicationId A string that identifies the applicationId.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.3".
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.69.1 and value
        matching the applicationId.
     * Revocation is checked via any available method.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleKeyTransparency(CFStringRef applicationId)
    API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!
 @function SecPolicyCreateLegacySSL
 @abstract Returns a policy object for evaluating legacy SSL certificate chains that don't meet
 SecPolicyCreateSSL.
 @param server Passing true for this parameter creates a policy for SSL
 server certificates.
 @param hostname (Optional) If present, the policy will require the specified
 hostname to match the hostname in the leaf certificate.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 @discussion Use of this policy will be audited. Passing false for the server parameter will
 result in a SecPolicy object with the same requirements as SecPolicyCreateSSL with a false
 server parameter (i.e. the client authentication verification performed by this policy is
 identical to the client authentication verification performed by SecPolicyCreateSSL).
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateLegacySSL(Boolean server, CFStringRef __nullable hostname)
    SPI_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0));

/*!
 @function SecPolicyCreateAlisha
 @abstract Returns a policy object for verifying Alisha certificates.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
     * EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAlisha(void)
    API_AVAILABLE(macos(10.15.4), ios(13.4), watchos(6.2), tvos(13.4));
extern const CFStringRef kSecPolicyAppleEscrowServiceIdKeySigning
    API_AVAILABLE(macos(10.15.6), ios(13.5.5));
extern const CFStringRef kSecPolicyApplePCSEscrowServiceIdKeySigning
    API_AVAILABLE(macos(10.15.6), ios(13.5.5));

/*!
 @function SecPolicyCreateMeasuredBootPolicySigning
 @abstract Returns a policy object for verifying Measured Boot Policy Signing certificates.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.24.17.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.26.6.1
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 Because this policy does not pin the anchors, the caller must use SecTrustSetAnchorCertificates with
 the expected roots.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMeasuredBootPolicySigning(void)
    API_AVAILABLE(macos(10.15.4), ios(13.4), watchos(6.2), tvos(13.4));

/*!
 @function SecPolicyCreateApplePayQRCodeEncryption
 @abstract Returns a policy object for verifying ApplePay QRCode Encryption certificates
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The root matches the "Apple External EC Root", or on internal builds, "Test Apple External EC Root"
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.22.
     * The leaf has a marker extension with OID 1.2.840.113635.100.13.3
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
     * Revocation is checked via any available method
 Because the "Apple External" roots are not trusted by default, the caller must use
 SecTrustSetAnchorCertificates with the expected roots.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePayQRCodeEncryption(void)
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/*!
 @function SecPolicyCreateApplePayQRCodeSigning
 @abstract Returns a policy object for verifying ApplePay QRCode Signing certificates
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The root matches the "Apple External EC Root", or on internal builds, "Test Apple External EC Root"
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.22.
     * The leaf has a marker extension with OID 1.2.840.113635.100.12.12
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
     * Revocation is checked via any available method
 Because the "Apple External" roots are not trusted by default, the caller must use
 SecTrustSetAnchorCertificates with the expected roots.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePayQRCodeSigning(void)
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/*!
 @function SecPolicyCreateAppleAccessoryUpdateSigning
 @abstract Returns a policy object for verifying Accessory Firmware Update Signing certificates
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.17.
     * The leaf has a marker extension with OID 1.2.840.113635.100.12.9, or, if
        "AllowAccessoryUpdateSigningBeta" is set to true in the com.apple.security
        preference/defaults domain, OID 1.2.840.113635.100.12.10
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
     * Revocation is checked via any available method
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleAccessoryUpdateSigning(void)
    API_AVAILABLE(macos(10.16), ios(14.0), watchos(7.0), tvos(14.0));

/*!
 @function SecPolicyCreateAggregateMetricTransparency
 @abstract Returns a policy object for verifying Aggregate Metric Transparency certificates
 @param facilitator A boolean to indicate whether the facilitator or partner transparency
 certificate is being checked.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.26.
     * The leaf has a marker extension with OID 1.2.840.113635.100.12.17 if facilitator is true or
      1.2.840.113635.100.12.18 if facilitator is false. The contents of this marker extension
      are not checked.
     * Revocation is checked via any available method.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
     * Require a positive CT verification result.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAggregateMetricTransparency(bool facilitator)
    API_AVAILABLE(macos(10.15.6), ios(13.6), watchos(6.2), tvos(13.4));

/*!
 @function SecPolicyCreateAggregateMetricEncryption
 @abstract Returns a policy object for verifying Aggregate Metric Encryption certificates
 @param facilitator A boolean to indicate whether the facilitator or partner encryption
 certificate is being checked.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.26.
     * The leaf has a marker extension with OID 1.2.840.113635.100.15.2 if facilitator is true or
      1.2.840.113635.100.15.3 if facilitator is false.
     * Revocation is checked via any available method.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
     * Require a positive CT verification result using the non-TLS CT log list
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAggregateMetricEncryption(bool facilitator)
    API_AVAILABLE(macos(11.1), ios(14.3), watchos(7.2), tvos(14.3));

/*!
 @function SecPolicyCreateSSLWithKeyUsage
 @abstract Returns a policy object for evaluating SSL certificate chains (with key usage enforcement)
 @param server Passing true for this parameter creates a policy for SSL
 server certificates.
 @param hostname (Optional) If present, the policy will require the specified
 hostname to match the hostname in the leaf certificate.
 @param keyUsage SecKeyUsage flags (see SecCertificatePriv.h) that the server certificate must have specified.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateSSLWithKeyUsage(Boolean server, CFStringRef __nullable hostname, uint32_t keyUsage)
    API_AVAILABLE(macos(11.3), ios(14.5), watchos(7.4), tvos(14.5));

/*!
 @function SecPolicySetSHA256Pins
 @abstract Sets the SubjectPublicKeyInfo (SPKI) pins.
 @param policy The policy to modify.
 @param leafSPKISHA256 An array of SHA256 hashes of permitted leaf SPKIs. Passing NULL will remove any previous pins.
 @param caSPKISHA256 An array of SHA256 hashes of CA SPKIs. Passing NULL will remove any previous pins.
 @discussion Pins replace any existing pins set on the policy.
 Setting leaf pins will require that the SPKI in the leaf certificate validated must match at least one of the array of leafSPKISHA256 pins.
 Setting CA pins will require that at an SPKI in at least one CA certificate in the built chain from the leaf certificate to a trusted anchor certificate
 matches at least one of the array of caSPKISHA256 pins.
 */
void SecPolicySetSHA256Pins(SecPolicyRef policy, CFArrayRef _Nullable leafSPKISHA256, CFArrayRef _Nullable caSPKISHA256)
    API_AVAILABLE(macos(11.3), ios(14.5), watchos(7.4), tvos(14.5));

/*!
 @function SecPolicyCreateApplayPayModelSigning
 @abstract Returns a policy object for verifying Aggregate Metric Encryption certificates
 @param checkExpiration A boolean to indicate whether the policy should check for expiration.
 @discussion The resulting policy uses the Basic X.509 policy with optional validity check and
 pinning options:
     * The chain is anchored to any of the Apple Root CAs.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.17.
     * The leaf has a marker extension with OID 1.2.840.113635.100.12.20
     * Revocation is checked via any available method.
     * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePayModelSigning(bool checkExpiration)
    API_AVAILABLE(macos(11.3), ios(14.5), watchos(7.4), tvos(14.5));

/*
 *  Legacy functions (OS X only)
 */
#if TARGET_OS_OSX

/*!
     @function SecPolicyCopy
     @abstract Returns a copy of a policy reference based on certificate type and OID.
     @param certificateType A certificate type.
     @param policyOID The OID of the policy you want to find. This is a required parameter. See oidsalg.h to see a list of policy OIDs.
     @param policy The returned policy reference. This is a required parameter.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion This function is deprecated in Mac OS X 10.7 and later;
     to obtain a policy reference, use one of the SecPolicyCreate* functions in SecPolicy.h.
 */
OSStatus SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef * __nonnull CF_RETURNS_RETAINED policy)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);

/* Given a unified SecPolicyRef, return a copy with a legacy
 C++ ItemImpl-based Policy instance. Only for internal use;
 legacy references cannot be used by SecPolicy API functions. */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateItemImplInstance(SecPolicyRef policy);

/* Given a CSSM_OID pointer, return a string which can be passed
 to SecPolicyCreateWithProperties. The return value can be NULL
 if no supported policy was found for the OID argument. */
__nullable
CFStringRef SecPolicyGetStringForOID(CSSM_OID* oid)
    API_DEPRECATED("No longer supported", macos(10.5,10.14));

/*!
 @function SecPolicyCreateAppleTimeStampingAndRevocationPolicies
 @abstract Create timeStamping policy array from a given set of policies by applying identical revocation behavior
 @param policyOrArray can be a SecPolicyRef or a CFArray of SecPolicyRef
 @discussion This function is deprecated in macOS 10.13 and later. Your code should call SecPolicyCreateAppleTimeStamping
 and SecPolicyCreateRevocation instead to obtain these policies, then insert them into an array as needed.
 */
__nullable CF_RETURNS_RETAINED
CFArrayRef SecPolicyCreateAppleTimeStampingAndRevocationPolicies(CFTypeRef policyOrArray)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_10, __MAC_10_13, __IPHONE_NA, __IPHONE_NA);

#endif /* TARGET_OS_MAC && !TARGET_OS_IPHONE */

/* MARK: WARNING: The following constants and functions are for project use
 * within the Security project and are subject to change without warning */

/*!
 @enum Policy Check Keys
 @discussion Keys that represent various checks that can be done in a trust
 policy. Use outside of the Security project at your own peril.
 */
extern const CFStringRef kSecPolicyCheckAnchorApple;
extern const CFStringRef kSecPolicyCheckAnchorSHA256;
extern const CFStringRef kSecPolicyCheckAnchorTrusted;
extern const CFStringRef kSecPolicyCheckBasicCertificateProcessing;
extern const CFStringRef kSecPolicyCheckBasicConstraints;
extern const CFStringRef kSecPolicyCheckBasicConstraintsCA;
extern const CFStringRef kSecPolicyCheckBasicConstraintsPathLen;
extern const CFStringRef kSecPolicyCheckBlackListedKey;
extern const CFStringRef kSecPolicyCheckBlackListedLeaf;
extern const CFStringRef kSecPolicyCheckCertificatePolicy;
extern const CFStringRef kSecPolicyCheckChainLength;
extern const CFStringRef kSecPolicyCheckCriticalExtensions;
extern const CFStringRef kSecPolicyCheckCTRequired;
extern const CFStringRef kSecPolicyCheckEAPTrustedServerNames;
extern const CFStringRef kSecPolicyCheckEmail;
extern const CFStringRef kSecPolicyCheckExtendedKeyUsage;
extern const CFStringRef kSecPolicyCheckExtendedValidation;
extern const CFStringRef kSecPolicyCheckGrayListedKey;
extern const CFStringRef kSecPolicyCheckGrayListedLeaf;
extern const CFStringRef kSecPolicyCheckLeafSPKISHA256;
extern const CFStringRef kSecPolicyCheckIdLinkage;
extern const CFStringRef kSecPolicyCheckIntermediateCountry;
extern const CFStringRef kSecPolicyCheckIntermediateEKU;
extern const CFStringRef kSecPolicyCheckIntermediateMarkerOid;
extern const CFStringRef kSecPolicyCheckIntermediateMarkerOidWithoutValueCheck;
extern const CFStringRef kSecPolicyCheckIntermediateOrganization;
extern const CFStringRef kSecPolicyCheckIntermediateSPKISHA256;
extern const CFStringRef kSecPolicyCheckCAspkiSHA256;
extern const CFStringRef kSecPolicyCheckIssuerCommonName;
extern const CFStringRef kSecPolicyCheckIssuerPolicyConstraints;
extern const CFStringRef kSecPolicyCheckIssuerNameConstraints;
extern const CFStringRef kSecPolicyCheckKeySize;
extern const CFStringRef kSecPolicyCheckKeyUsage;
extern const CFStringRef kSecPolicyCheckKeyUsageReportOnly;
extern const CFStringRef kSecPolicyCheckLeafMarkerOid;
extern const CFStringRef kSecPolicyCheckLeafMarkerOidWithoutValueCheck;
extern const CFStringRef kSecPolicyCheckLeafMarkersProdAndQA;
extern const CFStringRef kSecPolicyCheckMissingIntermediate;
extern const CFStringRef kSecPolicyCheckNameConstraints;
extern const CFStringRef kSecPolicyCheckNoNetworkAccess;
extern const CFStringRef kSecPolicyCheckNonEmptySubject;
extern const CFStringRef kSecPolicyCheckNonTlsCTRequired;
extern const CFStringRef kSecPolicyCheckNotCA;
extern const CFStringRef kSecPolicyCheckNotValidBefore;
extern const CFStringRef kSecPolicyCheckPinningRequired;
extern const CFStringRef kSecPolicyCheckPolicyConstraints;
extern const CFStringRef kSecPolicyCheckRevocation;
extern const CFStringRef kSecPolicyCheckRevocationIfTrusted;
extern const CFStringRef kSecPolicyCheckRevocationOnline;
extern const CFStringRef kSecPolicyCheckRevocationResponseRequired;
extern const CFStringRef kSecPolicyCheckSSLHostname;
extern const CFStringRef kSecPolicyCheckServerAuthEKU;
extern const CFStringRef kSecPolicyCheckSignatureHashAlgorithms;
extern const CFStringRef kSecPolicyCheckSubjectCommonName;
extern const CFStringRef kSecPolicyCheckSubjectCommonNamePrefix;
extern const CFStringRef kSecPolicyCheckSubjectCommonNameTEST;
extern const CFStringRef kSecPolicyCheckSubjectOrganization;
extern const CFStringRef kSecPolicyCheckSubjectOrganizationalUnit;
extern const CFStringRef kSecPolicyCheckSystemTrustedCTRequired;
extern const CFStringRef kSecPolicyCheckSystemTrustedWeakHash;
extern const CFStringRef kSecPolicyCheckSystemTrustedWeakKey;
extern const CFStringRef kSecPolicyCheckTemporalValidity;
extern const CFStringRef kSecPolicyCheckUnparseableExtension;
extern const CFStringRef kSecPolicyCheckUsageConstraints;
extern const CFStringRef kSecPolicyCheckValidityPeriodMaximums;
extern const CFStringRef kSecPolicyCheckValidRoot;
extern const CFStringRef kSecPolicyCheckWeakKeySize;
extern const CFStringRef kSecPolicyCheckWeakSignature;

/*  Special option for checking Apple Anchors */
extern const CFStringRef kSecPolicyAppleAnchorIncludeTestRoots;

/* Special option for checking Prod and QA Markers */
extern const CFStringRef kSecPolicyLeafMarkerProd;
extern const CFStringRef kSecPolicyLeafMarkerQA;

/* Special option for checking Revocation */
extern const CFStringRef kSecPolicyCheckRevocationOCSP;
extern const CFStringRef kSecPolicyCheckRevocationCRL;
extern const CFStringRef kSecPolicyCheckRevocationAny;

/* Policy Names */
extern const CFStringRef kSecPolicyNameX509Basic;
extern const CFStringRef kSecPolicyNameSSLServer;
extern const CFStringRef kSecPolicyNameSSLClient;
extern const CFStringRef kSecPolicyNameEAPServer;
extern const CFStringRef kSecPolicyNameEAPClient;
extern const CFStringRef kSecPolicyNameIPSecServer;
extern const CFStringRef kSecPolicyNameIPSecClient;
extern const CFStringRef kSecPolicyNameSMIME;
extern const CFStringRef kSecPolicyNameCodeSigning;
extern const CFStringRef kSecPolicyNameTimeStamping;
extern const CFStringRef kSecPolicyNameOCSPSigner;

/*!
 @function SecPolicyCreateEscrowServiceIdKeySigning
 @abstract Returns a policy object for verifying Escrow Service ID keys.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to the current Escrow Roots in the OTAPKI asset.
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * CN matching the name generated by escrow service.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateEscrowServiceIdKeySigning(void)
    API_AVAILABLE(macos(10.15.6), ios(13.6));

/*!
 @function SecPolicyCreatePCSEscrowServiceIdKeySigning
 @abstract Returns a policy object for verifying PCS Escrow Service ID keys.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to the current Escrow Roots in the OTAPKI asset.
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * CN matching the name generated by escrow service.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreatePCSEscrowServiceIdKeySigning(void)
    API_AVAILABLE(macos(10.15.6), ios(13.6));

/*
 * MARK: SecPolicyCheckCert functions
 */
bool SecPolicyCheckCertSSLHostname(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertEmail(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertTemporalValidity(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertWeakKeySize(SecCertificateRef cert, CFTypeRef __nullable pvcValue);
bool SecPolicyCheckCertKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertExtendedKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonName(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonNamePrefix(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonNameTEST(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectOrganization(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectOrganizationalUnit(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertNotValidBefore(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertEAPTrustedServerNames(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertLeafMarkerOid(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertLeafMarkersProdAndQA(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertNonEmptySubject(SecCertificateRef cert, CFTypeRef __nullable pvcValue);
bool SecPolicyCheckCertKeySize(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertWeakSignature(SecCertificateRef cert, CFTypeRef __nullable pvcValue);
bool SecPolicyCheckCertSignatureHashAlgorithms(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertCertificatePolicy(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertCriticalExtensions(SecCertificateRef cert, CFTypeRef __nullable pvcValue);
bool SecPolicyCheckCertSubjectCountry(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertUnparseableExtension(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertNotCA(SecCertificateRef cert, CFTypeRef pvcValue);

void SecPolicySetName(SecPolicyRef policy, CFStringRef policyName);
__nullable CFArrayRef SecPolicyXPCArrayCopyArray(xpc_object_t xpc_policies, CFErrorRef *error);

void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value);

bool SecDNSIsTLD(CFStringRef reference);

CFDataRef CreateCFDataFromBase64CFString(CFStringRef base64string);
CFArrayRef parseNSPinnedDomains(CFDictionaryRef nsPinnedDomainsDict, CFStringRef hostName, CFStringRef nsPinnedIdentityType);
void SecPolicyReconcilePinningRequiredIfInfoSpecified(CFMutableDictionaryRef options);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */
