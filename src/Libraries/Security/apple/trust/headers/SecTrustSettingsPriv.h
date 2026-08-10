/*
 * Copyright (c) 2002-2020 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECTRUSTSETTINGSPRIV_H_
#define _SECURITY_SECTRUSTSETTINGSPRIV_H_

#include <Security/SecBase.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrustSettings.h>
#if SEC_OS_OSX
#include <Security/cssmtype.h>
#endif

__BEGIN_DECLS

/*
 * Private Keys in the Usage Contraints dictionary.
 * kSecTrustSettingsPolicyName      Specifies a cert verification policy, e.g.,
 *                                  sslServer, eapClient, etc, using policy names.
 *                                  This entry can be used to restrict the policy where
 *                                  the same Policy Constant is used for multiple policyNames.
 * kSectrustSettingsPolicyOptions   Specifies a dictionary of policy options (from
 *                                  SecPolicyInternal.h). This entry can be used to require
 *                                  a particular SecPolicyCheck whenever this certificate is
 *                                  encountered during trust evaluation.
 */
#define kSecTrustSettingsPolicyName               CFSTR("kSecTrustSettingsPolicyName")
#define kSecTrustSettingsPolicyOptions            CFSTR("kSecTrustSettingsPolicyOptions")

extern const CFStringRef kSecTrustStoreSPKIHashKey;
extern const CFStringRef kSecTrustStoreHashAlgorithmKey;

extern const CFStringRef kSecCTExceptionsCAsKey;
extern const CFStringRef kSecCTExceptionsDomainsKey;
extern const CFStringRef kSecCTExceptionsHashAlgorithmKey;
extern const CFStringRef kSecCTExceptionsSPKIHashKey;

/*
 @function SecTrustStoreSetCTExceptions
 @abstract Set the certificate transparency enforcement exceptions
 @param applicationIdentifier Identifier for the caller. If null, the application-identifier will be read from the caller's entitlements.
 @param exceptions Dictionary of exceptions to set for this application. These exceptions replace existing exceptions for the keys in the dictionary. Exceptions for omitted keys are not affected. Null removes all exceptions for this application. See the discussion sections below for a complete overview of options.
 @param error On failure, describes the cause of the failure; otherwise, null.
 @result boolean indicating success of the operation. If false, error will be filled in with a description of the error.
 @discussions An exceptions dictionary has two optional keys:
 kSecCTExceptionsDomainsKey takes an array of strings. These strings are the domains that are excluded from enforcing CT. A leading "." is supported to signify subdomains. Wildcard domains are not supported.
 kSecCTExceptionsCAsKey takes an array of dictionaries. Each dictionary has two required keys:
    kSecTrustStoreHashAlgorithmKey takes a string indicating the hash algorithm. Currently only "sha256" is supported.
    kSecTrustStoreSPKIHashKey takes a data containing hash of a certificate's SubjectPublicKeyInfo.
 */
bool SecTrustStoreSetCTExceptions(CFStringRef applicationIdentifier, CFDictionaryRef exceptions, CFErrorRef *error);

/*
 @function SecTrustStoreCopyCTExceptions
 @abstract Return the certificate transparency enforcement exceptions
 @param applicationIdentifier Identifier for the caller's exceptions to fetch. If null, all set exceptions will be returned (regardless of which caller set them).
 @param error On failure, describes the cause of the failure; otherwise, null.
 @result The dictionary of currently set exceptions. Null if none exist or upon failure.
 @discussion The returned exceptions dictionary has the same options as input exceptions. See the discussion of SecTrustStoreSetCTExceptions.
 */
CF_RETURNS_RETAINED CFDictionaryRef SecTrustStoreCopyCTExceptions(CFStringRef applicationIdentifier, CFErrorRef *error);


extern const CFStringRef kSecCARevocationAdditionsKey;
extern const CFStringRef kSecCARevocationHashAlgorithmKey;
extern const CFStringRef kSecCARevocationSPKIHashKey;

/*
 @function SecTrustStoreSetCARevocationAdditions
 @abstract Set a list of certificate authorities (specified by subject public key info hash) for which revocation should be explicitly checked.
 @param applicationIdentifier Identifier for the caller. If null, the application-identifier will be read from the caller's entitlements.
 @param additions Dictionary of SPKI hashes for which revocation should be explicitly checked. Existing entries for the keys in the dictionary will be replaced. Null removes all CA revocation additions for this application. See the discussion sections below for a complete overview of options.
 @param error On failure, describes the cause of the failure; otherwise, null.
 @result boolean indicating success of the operation. If false, error will be filled in with a description of the error.
 @discussions An additions dictionary currently has one defined key:
 kSecCARevocationAdditionsKey takes an array of dictionaries. Each dictionary has two required keys:
    kSecTrustStoreHashAlgorithmKey takes a string indicating the hash algorithm. Currently only "sha256" is supported.
    kSecTrustStoreSPKIHashKey takes a data containing hash of a certificate's SubjectPublicKeyInfo.
 */
bool SecTrustStoreSetCARevocationAdditions(CFStringRef applicationIdentifier, CFDictionaryRef additions, CFErrorRef *error);

/*
 @function SecTrustStoreCopyCARevocationAdditions
 @abstract Return the certificate authority SPKI hashes for which revocation should be explicitly checked.
 @param applicationIdentifier Identifier for the caller's additions to fetch. If null, all set exceptions will be returned (regardless of which caller set them).
 @param error On failure, describes cause of the failure; otherwise, null.
 @result The dictionary of currently set CA revocation additions. Null if none exist or upon failure.
 @discussion The returned additions dictionary has the same options as input additions. See the discussion of SecTrustStoreSetCARevocationAdditions.
 */
CF_RETURNS_RETAINED CFDictionaryRef SecTrustStoreCopyCARevocationAdditions(CFStringRef applicationIdentifier, CFErrorRef *error);

/*
 @function SecTrustStoreSetTransparentConnectionPins
 @abstract Set a list of certificate authorities (specified by subject public key info hash) to which Transparent Connections should be pinned.
 @param applicationIdentifier Identifier for the caller. If null, the application-identifier will be read from the caller's entitlements.
 @param pins Array of dictionaries containing SPKI hashes to which Transparent Connections should be pinned. Existing entries for the keys in the dictionary will be replaced. Null removes all pins for this application. See the discussion sections below for a complete overview of options.
 @param error On failure, describes the cause of the failure; otherwise, null.
 @result boolean indicating success of the operation. If false, error will be filled in with a description of the error.
 @discussion The pins dictionaries should each have the following keys and values
    kSecTrustStoreHashAlgorithmKey takes a string indicating the hash algorithm. Currently only "sha256" is supported.
    kSecTrustStoreSPKIHashKey takes a data containing hash of a certificate's SubjectPublicKeyInfo.
 The device must be in HRN mode to honor these pins.
 */
bool SecTrustStoreSetTransparentConnectionPins(CFStringRef applicationIdentifier, CFArrayRef pins, CFErrorRef *error);

/*
 @function SecTrustStoreCopyTransparentConnectionPins
 @abstract Return the certificate authority SPKI hashes  to which Transparent Connections should be pinned.
 @param applicationIdentifier Identifier for the caller's additions to fetch. If null, all set exceptions will be returned (regardless of which caller set them).
 @param error On failure, describes cause of the failure; otherwise, null.
 @result The array of currently set CA pins. Null if none exist or upon failure.
 @discussion The returned pins array has the same options as input pins. See the discussion of SecTrustStoreSetTransparentConnectionPins.
 */
CF_RETURNS_RETAINED CFArrayRef SecTrustStoreCopyTransparentConnectionPins(CFStringRef applicationIdentifier, CFErrorRef *error);

#if SEC_OS_OSX

/*
 * Fundamental routine used by TP to ascertain status of one cert.
 *
 * Returns true in *foundMatchingEntry if a trust setting matching
 * specific constraints was found for the cert. Returns true in
 * *foundAnyEntry if any entry was found for the cert, even if it
 * did not match the specified constraints. The TP uses this to
 * optimize for the case where a cert is being evaluated for
 * one type of usage, and then later for another type. If
 * foundAnyEntry is false, the second evaluation need not occur.
 *
 * Returns the domain in which a setting was found in *foundDomain.
 *
 * Allowed errors applying to the specified cert evaluation
 * are returned in a mallocd array in *allowedErrors and must
 * be freed by caller.
 */
OSStatus SecTrustSettingsEvaluateCert(
     CFStringRef                         certHashStr,
     /* parameters describing the current cert evalaution */
     const CSSM_OID                      *policyOID,
     const char                          *policyString,        /* optional */
     uint32                              policyStringLen,
     SecTrustSettingsKeyUsage            keyUsage,             /* optional */
     bool                                isRootCert,           /* for checking default setting */
     /* RETURNED values */
     SecTrustSettingsDomain              *foundDomain,
     CSSM_RETURN                         **allowedErrors,      /* mallocd and RETURNED */
     uint32                              *numAllowedErrors,    /* RETURNED */
     SecTrustSettingsResult              *resultType,          /* RETURNED */
     bool                                *foundMatchingEntry,  /* RETURNED */
     bool                                *foundAnyEntry)       /* RETURNED */
DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/*
 * Obtain trusted certs which match specified usage.
 * Only certs with a SecTrustSettingsResult of
 * kSecTrustSettingsResultTrustRoot or
 * or kSecTrustSettingsResultTrustAsRoot will be returned.
 *
 * To be used by SecureTransport for its (hopefully soon-to-be-
 * deprecated) SSLSetTrustedRoots() call; I hope nothing else has
 * to use this...
 *
 * Caller must CFRelease the returned CFArrayRef.
 */
OSStatus SecTrustSettingsCopyQualifiedCerts(
     const CSSM_OID                      *policyOID,
     const char                          *policyString,        /* optional */
     uint32                              policyStringLen,
     SecTrustSettingsKeyUsage            keyUsage,             /* optional */
     CFArrayRef                          *certArray)           /* RETURNED */
DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/*
 * Obtain unrestricted root certificates from the specified domain(s).
 * Only returns root certificates with no usage constraints.
 * Caller must CFRelease the returned CFArrayRef.
 */
OSStatus SecTrustSettingsCopyUnrestrictedRoots(
     Boolean                         userDomain,
     Boolean                         adminDomain,
     Boolean                         systemDomain,
     CFArrayRef                      *certArray);          /* RETURNED */

/*
 * Obtain a string representing a cert's SHA1 digest. This string is
 * the key used to look up per-cert trust settings in a TrustSettings record.
 */
CFStringRef CF_RETURNS_RETAINED SecTrustSettingsCertHashStrFromCert(
     SecCertificateRef certRef);

CFStringRef CF_RETURNS_RETAINED SecTrustSettingsCertHashStrFromData(
     const void *cert,
     size_t certLen);

/*
 * Add a cert's TrustSettings to a non-persistent TrustSettings record.
 * Primarily intended for use in creating a system TrustSettings record
 * (which is itself immutable via this module).
 *
 * The settingsIn argument is an external representation of a TrustSettings
 * record, obtained from this function or from
 * SecTrustSettingsCreateExternalRepresentation().
 * If settingsIn is NULL, a new (empty) TrustSettings will be created.
 *
 * The certRef and trustSettingsDictOrArray arguments are as in
 * SecTrustSettingsSetTrustSettings(). May be NULL, when e.g. creating
 * a new and empty TrustSettings record.
 *
 * The external representation is written to the settingOut argument,
 * which must eventually be CFReleased by the caller.
 */
OSStatus SecTrustSettingsSetTrustSettingsExternal(
     CFDataRef               settingsIn,                   /* optional */
     SecCertificateRef       certRef,                      /* optional */
     CFTypeRef               trustSettingsDictOrArray,     /* optional */
     CFDataRef               *settingsOut);                /* RETURNED */

/*
 * Add user trust settings for a SSL certificate and a given hostname.
 * This is a wrapper around the SecTrustSettingsSetTrustSettings API
 * and should be functionally equivalent to "Always trust" in the UI.
 *
 * When this function is called, the user will be prompted to authorize
 * the trust settings change. After they successfully authenticate, or
 * cancel the dialog, the result block will be called to indicate the
 * current trust status. If an error occurred (such as errUserCanceled),
 * the error reference provided to the block will be non-NULL.
 */
void SecTrustSettingsSetTrustedCertificateForSSLHost(
    SecCertificateRef certificate,
    CFStringRef hostname,
    void (^result)(SecTrustSettingsResult trustResult, CFErrorRef error))
    __OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_NA);
#endif // SEC_OS_OSX

#if SEC_OS_OSX_INCLUDES
/*
 * Purge the cache of User and Admin Certs
 */
void SecTrustSettingsPurgeUserAdminCertsCache(void);

/*
 * A wrapper around SecTrustSettingsCopyCertificates that combines user and admin
 * domain outputs.
 */
OSStatus SecTrustSettingsCopyCertificatesForUserAdminDomains(
    CFArrayRef CF_RETURNS_RETAINED *certArray);

/* Just like the API version (SecTrustSettingsCopyTrustSettings) but
 * uses the cached version of trust settings to avoid disk reads. */
OSStatus SecTrustSettingsCopyTrustSettings_Cached(
    SecCertificateRef certRef,
    SecTrustSettingsDomain domain,
    CFArrayRef CF_RETURNS_RETAINED *trustSettings);

/* Purge the trust settings cache (used by the above) */
void SecTrustSettingsPurgeCache(void);

/* Determines if the given cert has any trust settings in the admin or user domains */
bool SecTrustSettingsUserAdminDomainsContain(SecCertificateRef certRef);
#endif /* SEC_OS_OSX_INCLUDES */

__END_DECLS

#endif // _SECURITY_SECTRUSTSETTINGSPRIV_H_
