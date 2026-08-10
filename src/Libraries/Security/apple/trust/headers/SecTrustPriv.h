/*
 * Copyright (c) 2003-2018 Apple Inc. All Rights Reserved.
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
     @header SecTrustPriv
     The functions and data types in SecTrustPriv implement trust computation
     and allow the user to apply trust decisions to the trust configuration.
 */

#ifndef _SECURITY_SECTRUSTPRIV_H_
#define _SECURITY_SECTRUSTPRIV_H_

#include <Security/SecTrust.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

#define TRUST_TIME_LEEWAY (4500.0) // 1h15m: 1 hour to address DST/time zone issues, 15 min for clock skew

/* Constants used as keys in property lists.  See
 SecTrustCopySummaryPropertiesAtIndex for more information. */
extern const CFStringRef kSecPropertyKeyType;
extern const CFStringRef kSecPropertyKeyLabel;
extern const CFStringRef kSecPropertyKeyLocalizedLabel;
extern const CFStringRef kSecPropertyKeyValue;

extern const CFStringRef kSecPropertyTypeWarning;
extern const CFStringRef kSecPropertyTypeSuccess;
extern const CFStringRef kSecPropertyTypeSection;
extern const CFStringRef kSecPropertyTypeData;
extern const CFStringRef kSecPropertyTypeString;
extern const CFStringRef kSecPropertyTypeURL;
extern const CFStringRef kSecPropertyTypeDate;
extern const CFStringRef kSecPropertyTypeArray;
extern const CFStringRef kSecPropertyTypeNumber;

/* Constants used as keys in the dictionary returned by SecTrustCopyInfo. */
extern const CFStringRef kSecTrustInfoExtendedValidationKey;
extern const CFStringRef kSecTrustInfoCompanyNameKey;
extern const CFStringRef kSecTrustInfoRevocationKey;
extern const CFStringRef kSecTrustInfoRevocationValidUntilKey;
extern const CFStringRef kSecTrustInfoCertificateTransparencyKey;
extern const CFStringRef kSecTrustInfoResultNotBefore;
extern const CFStringRef kSecTrustInfoResultNotAfter;

/* Constants used as keys in the certificate details dictionary.
 An array of per-certificate details is returned by SecTrustCopyResult
 as the value of the kSecTrustResultDetails key.
*/
extern const CFStringRef kSecCertificateDetailStatusCodes;
    /*__OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_11_0);*/

/*!
 @enum Trust Result Constants
 @discussion Predefined key constants used to obtain values in a
 dictionary of trust evaluation results for a certificate chain,
 as retrieved from a call to SecTrustCopyResult.

 @constant kSecTrustResultDetails
 This key will be present if a trust evaluation has been performed.
 Its value is a CFArrayRef of CFDictionaryRef representing detailed
 status info for each certificate in the completed chain.
 @constant kSecTrustRevocationReason
 This key will be present iff this chain had its revocation checked,
 and a "revoked" response was received. The value of this key will
 be a CFNumberRef indicating the reason for revocation. The possible
 reason code values are described in RFC 5280, section 5.3.1.
 */
extern const CFStringRef kSecTrustResultDetails;
    /*__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_9_0);*/
extern const CFStringRef kSecTrustRevocationReason;
    /*__OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);*/

/*!
     @function SecTrustCopySummaryPropertiesAtIndex
     @abstract Return a property array for the certificate.
     @param trust A reference to the trust object to evaluate.
     @param ix The index of the requested certificate.  Indices run from 0
     (leaf) to the anchor (or last certificate found if no anchor was found).
     @result A property array. It is the caller's responsibility to CFRelease
     the returned array when it is no longer needed. This function returns a
     short summary description of the certificate in question.  The property
     at index 0 of the array might also include general information about the
     entire chain's validity in the context of this trust evaluation.

     @discussion Returns a property array for this trust certificate. A property
     array is an array of CFDictionaryRefs.  Each dictionary (we call it a
     property for short) has the following keys:

        kSecPropertyKeyType This key's value determines how this property
            should be displayed. Its associated value is one of the
            following:
            kSecPropertyTypeWarning
                The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
                set.  The kSecPropertyKeyValue is a CFStringRef which should
                be displayed in yellow with a warning triangle.
            kSecPropertyTypeError
                The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
                set.  The kSecPropertyKeyValue is a CFStringRef which should
                be displayed in red with an error X.
            kSecPropertyTypeSuccess
                The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
                set.  The kSecPropertyKeyValue is a CFStringRef which should
                be displayed in green with a checkmark in front of it.
            kSecPropertyTypeTitle
                The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
                set.  The kSecPropertyKeyValue is a CFStringRef which should
                be displayed in a larger bold font.
            kSecPropertyTypeSection
                The optional kSecPropertyKeyLocalizedLabel is a CFStringRef with the name
                of the next section to display.  The value of the
                kSecPropertyKeyValue key is a CFArrayRef which is a property
                array as defined here.
            kSecPropertyTypeData
                The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
                the localized label for the value for the kSecPropertyKeyValue.
                The type of this value is a CFDataRef.  Its contents should be
                displayed as: "bytes length_of_data : hexdump_of_data". Ideally
                the UI will only show one line of hex dump data and have a
                disclosure arrow to see the remainder.
            kSecPropertyTypeString
                The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
                the localized label for the value for the kSecPropertyKeyValue.
                The type of this value is a CFStringRef.  It's contents should be
                displayed in the normal font.
            kSecPropertyTypeURL
                The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
                the localized label for the value for the kSecPropertyKeyValue.
                The type of this value is a CFURLRef.  It's contents should be
                displayed as a hyperlink.
            kSecPropertyTypeDate
                The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
                the localized label for the value for the kSecPropertyKeyValue.
                The type of this value is a CFDateRef.  It's contents should be
                displayed in human readable form (probably in the current
                timezone).
        kSecPropertyKeyLocalizedLabel
            Human readable localized label for a given property.
        kSecPropertyKeyValue
            See description of kSecPropertyKeyType to determine what the value
            for this key is.
        kSecPropertyKeyLabel
            Non localized key (label) for this value.  This is only
            present for properties with fixed label names.
     @result A property array. It is the caller's responsability to CFRelease
     the returned array when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
CFArrayRef SecTrustCopySummaryPropertiesAtIndex(SecTrustRef trust, CFIndex ix);

/*!
     @function SecTrustCopyDetailedPropertiesAtIndex
     @abstract Return a property array for the certificate.
     @param trust A reference to the trust object to evaluate.
     @param ix The index of the requested certificate.  Indices run from 0
     (leaf) to the anchor (or last certificate found if no anchor was found).
     @result A property array. It is the caller's responsibility to CFRelease
     the returned array when it is no longer needed.
     See SecTrustCopySummaryPropertiesAtIndex on how to intepret this array.
     Unlike that function call this function returns a detailed description
    of the certificate in question.
 */
__nullable CF_RETURNS_RETAINED
CFArrayRef SecTrustCopyDetailedPropertiesAtIndex(SecTrustRef trust, CFIndex ix);

/*!
     @function SecTrustCopyInfo
     @abstract Return a dictionary with additional information about the
     evaluated certificate chain for use by clients.
     @param trust A reference to an evaluated trust object.
     @discussion Returns a dictionary for this trust evaluation. This
     dictionary may have the following keys:

        kSecTrustInfoExtendedValidationKey this key will be present and have
            a value of kCFBooleanTrue if this chain was validated for EV.
        kSecTrustInfoCompanyNameKey Company name field of subject of leaf
            certificate, this field is meant to be displayed to the user
            if the kSecTrustInfoExtendedValidationKey is present.
        kSecTrustInfoRevocationKey this key will be present iff this chain
            had its revocation checked. The value will be a kCFBooleanTrue
            if revocation checking was successful and none of the
            certificates in the chain were revoked.
            The value will be kCFBooleanFalse if no current revocation status
            could be obtained for one or more certificates in the chain due
            to connection problems or timeouts etc.  This is a hint to a
            client to retry revocation checking at a later time.
        kSecTrustInfoRevocationValidUntilKey this key will be present iff
        kSecTrustInfoRevocationKey has a value of kCFBooleanTrue.
            The value will be a CFDateRef representing the earliest date at
            which the revocation info for one of the certificates in this chain
            might change.

     @result A dictionary with various fields that can be displayed to the user,
     or NULL if no additional info is available or the trust has not yet been
     validated.  The caller is responsible for calling CFRelease on the value
     returned when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust);

/* For debugging purposes. */
__nullable
CFArrayRef SecTrustGetDetails(SecTrustRef trust);

__nullable CF_RETURNS_RETAINED
CFArrayRef SecTrustCopyFilteredDetails(SecTrustRef trust);

/*!
 @function SecTrustIsExpiredOnly
 @abstract Determine whether expiration is the only problem with a certificate chain.
 @param trust A reference to a trust object.
 @result A boolean value indicating whether expiration is the only problem found
 with the certificate chain in the given trust reference.
 @discussion Returns true if one or more certificates in the chain have expired,
 expiration is an error (i.e. it is not being ignored by existing trust settings),
 and it is the only error encountered. Returns false if the certificate(s) have not
 expired, or are expired but have trust settings to override their expiration,
 or if the trust chain has other errors beside expiration. Your code should call
 this function after SecTrustEvaluate has returned a recoverable trust failure,
 so you can distinguish this case from other possible errors.
 */
Boolean SecTrustIsExpiredOnly(SecTrustRef trust)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/* For debugging purposes. */
__nullable CF_RETURNS_RETAINED
CFStringRef SecTrustCopyFailureDescription(SecTrustRef trust);

/*
 @function SecTrustGetTrustStoreVersionNumber
 @abstract Ask trustd what trust store version it is using.
 @param error A returned error if trustd failed to answer.
 @result The current version of the trust store. 0 upon failure.
 */
uint64_t SecTrustGetTrustStoreVersionNumber(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*
 @function SecTrustGetAssetVersionNumber
 @abstract Ask trustd what asset version it is using.
 @param error A returned error if trustd failed to answer.
 @result The current version of the asset. 0 upon failure.
 */
uint64_t SecTrustGetAssetVersionNumber(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*
 @function SecTrustOTAPKIGetUpdatedAsset
 @abstract Trigger trustd to fetch a new trust supplementals asset right now.
 @param error A returned error if trustd failed to update the asset.
 @result The current version of the update, regardless of the success of the update.
 @discussion This function blocks up to 1 minute until trustd has finished with the
 asset download and update. You should use the error parameter to determine whether
 the update was was successful. The current asset version is always returned.
 */
uint64_t SecTrustOTAPKIGetUpdatedAsset(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*
 @function SecTrustOTASecExperimentGetUpdatedAsset
 @abstract Trigger trustd to fetch a new SecExperiment asset right now.
 @param error A returned error if trustd failed to update the asset.
 @result The current version of the update, regardless of the success of the update.
 @discussion This function blocks up to 1 minute until trustd has finished with the
 asset download and update. You should use the error parameter to determine whether
 the update was was successful. The current asset version is always returned.
 */
uint64_t SecTrustOTASecExperimentGetUpdatedAsset(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*
 @function SecTrustOTASecExperimentCopyAsset
 @abstract Get current asset from trustd
 @param error A returned error if trustd fails to return asset
 @result Dictionary of asset
 @discussion If the error parameter is supplied, and the function returns false,
 the caller is subsequently responsible for releasing the returned CFErrorRef.
 */
CFDictionaryRef SecTrustOTASecExperimentCopyAsset(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*
 @function SecTrustTriggerValidUpdate
 @abstract Trigger trustd to fetch a valid update.
 @param error A returned error if trustd failed to trigger the update.
 @result True if the update was triggered, false if not.
 */
bool SecTrustTriggerValidUpdate(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error);

/*!
 @function SecTrustFlushResponseCache
 @abstract Removes all OCSP responses from the per-user response cache.
 @param error An optional pointer to an error object
 @result A boolean value indicating whether the operation was successful.
 @discussion If the error parameter is supplied, and the function returns false,
 the caller is subsequently responsible for releasing the returned CFErrorRef.
 */
Boolean SecTrustFlushResponseCache(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error)
    __OSX_AVAILABLE(10.13.4) __IOS_AVAILABLE(11.3) __TVOS_AVAILABLE(11.3) __WATCHOS_AVAILABLE(4.3);

/*!
 @function SecTrustSetTrustedLogs
 @abstract Sets the trusted CT logs for a given trust.
 @param trust A reference to a trust object.
 @param trustedLogs An array of trusted logs.
 @result A result code.  See "Security Error Codes" (SecBase.h).
 @discussion trustedLog is a CFArray of CFData containing the DER-encode SubjectPublicKeyInfo
 of the trusted CT logs.
 */
OSStatus SecTrustSetTrustedLogs(SecTrustRef trust, CFArrayRef trustedLogs);

/* Keychain searches are allowed by default. Use this to turn off seaching of
    -keychain search list (i.e. login.keychain, system.keychain)
    -Local Items/iCloud Keychain
    -user- and admin-trusted roots
    -network-fetched issuers
 User must provide all necessary certificates in the input certificates and/or anchors. */
OSStatus SecTrustSetKeychainsAllowed(SecTrustRef trust, Boolean allowed)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/* Get the keychain search policy for the trust object. */
OSStatus SecTrustGetKeychainsAllowed(SecTrustRef trust, Boolean * __nonnull allowed)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecTrustEvaluateLeafOnly
 @abstract Evaluates the leaf of the trust reference synchronously.
 @param trust A reference to the trust object to evaluate.
 @param result A pointer to a result type.
 @result A result code. See "Security Error Codes" (SecBase.h).
 @discussion This function will only evaluate the trust of the leaf certificate.
 No chain will be built and only those aspects of the SecPolicyRef that address
 the expected contents of the leaf will be checked. This function does not honor
 any set exceptions or usage constraints.
 */
OSStatus SecTrustEvaluateLeafOnly(SecTrustRef trust, SecTrustResultType * __nonnull result)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecTrustSerialize
 @abstract Creates a serialized version of the trust object
 @param trust A reference to the trust object to serialize.
 @param error A pointer to an error.
 @result The serialized trust object.
 @discussion This function is intended to be used to share SecTrustRefs between
 processes. Saving the results to disk or sending them over network channels
 may cause unexpected behavior.
 */
__nullable CF_RETURNS_RETAINED
CFDataRef SecTrustSerialize(SecTrustRef trust, CFErrorRef *error)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecTrustDeserialize
 @abstract Creates a trust object from the serialized data
 @param serializedTrust A reference to the serialized trust object
 @param error A pointer to an error.
 @result A trust object
 @discussion This function is intended to be used to share SecTrustRefs between
 processes. Saving the results to disk or sending them over network channels
 may cause unexpected behavior.
 */
__nullable CF_RETURNS_RETAINED
SecTrustRef SecTrustDeserialize(CFDataRef serializedTrust, CFErrorRef *error)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecTrustGetTrustExceptionsArray
 @abstract Return the exceptions array currently set in the trust object
 @param trust A reference to the trust object
 @result The array of exceptions.
 @discussion This function returns an array of exceptions that was previously set
 using SecTrustSetExceptions, unlike SecTrustCopyExceptions which returns the
 exceptions which could be set using SecTrustSetExceptions.
 */
__nullable CFArrayRef SecTrustGetTrustExceptionsArray(SecTrustRef trust)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecTrustCopyInputCertificates
 @abstract Return the array of certificates currently set in the trust object
 @param trust A reference to the trust object
 @param certificates On return, an array of the certificates used by this trust.
 Call the CFRelease function to release this reference.
 @result A result code. See "Security Error Codes" (SecBase.h)
*/
OSStatus SecTrustCopyInputCertificates(SecTrustRef trust, CFArrayRef * _Nonnull CF_RETURNS_RETAINED certificates)
__OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);

/*!
 @function SecTrustAddToInputCertificates
 @abstract Add certificate(s) to the currently set certificates in the trust object
 @param trust A reference to the trust object
 @param certificates The group of certificates to add. This can either be a CFArrayRef
 of SecCertificateRef objects or a single SecCertificateRef.
 @result A result code. See "Security Error Codes" (SecBase.h)
 */
OSStatus SecTrustAddToInputCertificates(SecTrustRef trust, CFTypeRef _Nonnull certificates)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);

/*!
 @function SecTrustSetPinningPolicyName
 @abstract Set the policy name to be used during the trust evaluation.
 @param trust A reference to the trust object
 @param policyName A string representing the name of the pinning policy to be used.
 @result A result code. See "Security Error Codes" (SecBase.h)
 @discussion This function permits the caller to enable the dynamic lookup of the
 pinning policy using a built-in database as an alternative to using a SecPolicyCreate function
 with the pinning rules and calling SecTrustCreateWithCertificates or SecTrustSetPolicies.
 */
OSStatus SecTrustSetPinningPolicyName(SecTrustRef trust, CFStringRef policyName)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
 @function SecTrustSetPinningException
 @abstract Remove pinning requirement from this trust evaluation
 @param trust A reference to the trust object
 @result A result code. See "Security Error Codes" (SecBase.h)
 @discussion This function provides an exception for this particular trust for a bundle that
 otherwise requires pinning for all connections. Bundles use the SecTrustPinningRequired key
 with boolean value of true in their info plist to indicate that all SSL connections from the
 bundle must be pinned.
 */
OSStatus SecTrustSetPinningException(SecTrustRef trust)
    __OSX_AVAILABLE(10.13) __IOS_AVAILABLE(11.0) __TVOS_AVAILABLE(11.0) __WATCHOS_AVAILABLE(4.0);

/*!
  @function SecTrustGetExceptionResetCount
  @abstract Returns the current epoch of trusted exceptions.
  @param error A pointer to an error.
  @result An unsigned 64-bit integer representing the current epoch.
  @discussion Exceptions tagged with an older epoch are not trusted.
  */
uint64_t SecTrustGetExceptionResetCount(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error)
    API_AVAILABLE(macos(11.3), ios(12.0), tvos(12.0), watchos(5.0));

/*!
  @function SecTrustIncrementExceptionResetCount
  @abstract Increases the current epoch of trusted exceptions by 1.
  @param error A pointer to an error.
  @result A result code. See "Security Error Codes" (SecBase.h)
  @discussion By increasing the current epoch any existing exceptions, tagged with the old epoch, become distrusted.
  */
OSStatus SecTrustIncrementExceptionResetCount(CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error)
    API_AVAILABLE(macos(11.3), ios(12.0), tvos(12.0), watchos(5.0));

#ifdef __BLOCKS__
/*!
 @function SecTrustEvaluateFastAsync
 @abstract Evaluates a trust reference asynchronously.
 @param trust A reference to the trust object to evaluate.
 @param queue A dispatch queue on which the result callback will be
 executed. Note that this function MUST be called from that queue.
 @param result A SecTrustCallback block which will be executed when the
 trust evaluation is complete. The block is guaranteed to be called exactly once
 when the result code is errSecSuccess, and not called otherwise. Note that this
 block may be called synchronously inline if no asynchronous operations are required.
 @result A result code. See "Security Error Codes" (SecBase.h).
 */
OSStatus SecTrustEvaluateFastAsync(SecTrustRef trust, dispatch_queue_t queue, SecTrustCallback result)
    __API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0));
#endif

/*!
 @function SecTrustReportTLSAnalytics
 @discussion This function MUST NOT be called outside of the TLS stack.
*/
bool SecTrustReportTLSAnalytics(CFStringRef eventName, xpc_object_t eventAttributes, CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error)
    __API_AVAILABLE(macos(10.13.4), ios(11.3), tvos(11.3), watchos(4.3));

/*!
 @function SecTrustReportNetworkingAnalytics
 @discussion This function MUST NOT be called outside of the networking stack.
*/
bool SecTrustReportNetworkingAnalytics(const char *eventName, xpc_object_t eventAttributes)
    __API_AVAILABLE(macos(10.15), ios(13), tvos(13), watchos(5));

/*!
 @function SecTrustSetNeedsEvaluation
 @abstract Reset the evaluation state of the trust object
 @param trust Trust object to reset
 @discussion Calling this will reset the trust object so that the next time SecTrustEvaluate*
 is called, a new trust evaluation is performed. SecTrustSet* interfaces implicitly call this,
 so this function is only necessary if you've made system configuration changes (like trust
 settings) that don't impact the trust object itself.
 */
void SecTrustSetNeedsEvaluation(SecTrustRef trust);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

/*
 *  Legacy functions (OS X only)
 */
#if TARGET_OS_OSX

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

#if SEC_OS_IPHONE
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfour-char-constants"
#endif /* SEC_OS_IPHONE */
/*
     unique keychain item attributes for user trust records.
 */
enum {
    kSecTrustCertAttr                    = 'tcrt',
    kSecTrustPolicyAttr                  = 'tpol',
    /* Leopard and later */
    kSecTrustPubKeyAttr                  = 'tpbk',
    kSecTrustSignatureAttr               = 'tsig'
};

#if SEC_OS_IPHONE
#pragma clang diagnostic pop
#endif /* SEC_OS_IPHONE */

/*!
     @function SecTrustGetUserTrust
     @abstract Gets the user-specified trust settings of a certificate and policy.
     @param certificate A reference to a certificate.
     @param policy A reference to a policy.
     @param trustSetting On return, a pointer to the user specified trust settings.
     @result A result code. See "Security Error Codes" (SecBase.h).
     @availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
 */
OSStatus SecTrustGetUserTrust(SecCertificateRef __nullable certificate, SecPolicyRef __nullable policy, SecTrustUserSetting * __nullable trustSetting)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_5, __IPHONE_NA, __IPHONE_NA);

/*!
     @function SecTrustSetUserTrust
     @abstract Sets the user-specified trust settings of a certificate and policy.
     @param certificate A reference to a certificate.
     @param policy A reference to a policy.
     @param trustSetting The user-specified trust settings.
     @result A result code. See "Security Error Codes" (SecBase.h).
     @availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
     @discussion as of Mac OS version 10.5, this will result in a call to
     SecTrustSettingsSetTrustSettings().
 */
OSStatus SecTrustSetUserTrust(SecCertificateRef __nullable certificate, SecPolicyRef __nullable policy, SecTrustUserSetting trustSetting)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_5, __IPHONE_NA, __IPHONE_NA);

/*!
     @function SecTrustSetUserTrustLegacy
     @abstract Sets the user-specified trust settings of a certificate and policy.
     @param certificate A reference to a certificate.
     @param policy A reference to a policy.
     @param trustSetting The user-specified trust settings.
     @result A result code.  See "Security Error Codes" (SecBase.h).

     @This is the private version of what used to be SecTrustSetUserTrust(); it operates
     on UserTrust entries as that function used to. The current SecTrustSetUserTrust()
     function operated on Trust Settings.
 */
OSStatus SecTrustSetUserTrustLegacy(SecCertificateRef __nullable certificate, SecPolicyRef __nullable policy, SecTrustUserSetting trustSetting)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_5, __MAC_10_12, __IPHONE_NA, __IPHONE_NA);

/*!
     @function SecTrustGetCSSMAnchorCertificates
     @abstract Retrieves the CSSM anchor certificates.
     @param cssmAnchors A pointer to an array of anchor certificates.
     @param cssmAnchorCount A pointer to the number of certificates in anchors.
     @result A result code. See "Security Error Codes" (SecBase.h).
     @availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
 */
OSStatus SecTrustGetCSSMAnchorCertificates(const CSSM_DATA * __nullable * __nullable cssmAnchors, uint32 *cssmAnchorCount)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_4, __MAC_10_5, __IPHONE_NA, __IPHONE_NA);

/*!
     @function SecTrustCopyExtendedResult
     @abstract Gets the extended trust result after an evaluation has been performed.
     @param trust A trust reference.
     @param result On return, result points to a CFDictionaryRef containing extended trust results (if no error occurred).
     The caller is responsible for releasing this dictionary with CFRelease when finished with it.
     @result A result code. See "Security Error Codes" (SecBase.h).
     @discussion This function may only be used after SecTrustEvaluate has been called for the trust reference, otherwise
     errSecTrustNotAvailable is returned. If the certificate is not an extended validation certificate, there is
     no extended result data and errSecDataNotAvailable is returned. Currently, only one dictionary key is defined
     (kSecEVOrganizationName).

     Note: this function will be deprecated in a future release of OS X. Your
     code should use SecTrustCopyResult to obtain the trust results dictionary.
 */
OSStatus SecTrustCopyExtendedResult(SecTrustRef trust, CFDictionaryRef * __nonnull CF_RETURNS_RETAINED result)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_5, __MAC_10_12, __IPHONE_NA, __IPHONE_NA);

/*
 * Preference-related strings for Revocation policies.
 */

/*
 * Preference domain, i.e., the name of a plist in ~/Library/Preferences or in
 * /Library/Preferences
 */
#define kSecRevocationDomain                "com.apple.security.revocation"

/* OCSP and CRL style keys, followed by values used for both of them */
#define kSecRevocationOcspStyle             CFSTR("OCSPStyle")
#define kSecRevocationCrlStyle              CFSTR("CRLStyle")
#define kSecRevocationOff                   CFSTR("None")
#define kSecRevocationBestAttempt           CFSTR("BestAttempt")
#define kSecRevocationRequireIfPresent      CFSTR("RequireIfPresent")
#define kSecRevocationRequireForAll         CFSTR("RequireForAll")

/* Which first if both enabled? */
#define kSecRevocationWhichFirst            CFSTR("RevocationFirst")
#define kSecRevocationOcspFirst             CFSTR("OCSP")
#define kSecRevocationCrlFirst              CFSTR("CRL")

/* boolean: A "this policy is sufficient per cert" for each */
#define kSecRevocationOCSPSufficientPerCert CFSTR("OCSPSufficientPerCert")
#define kSecRevocationCRLSufficientPerCert  CFSTR("CRLSufficientPerCert")

/* local OCSP responder URI, value arbitrary string value */
#define kSecOCSPLocalResponder              CFSTR("OCSPLocalResponder")

/* Extended trust result keys (now in public API) */
#define kSecEVOrganizationName              kSecTrustOrganizationName
#define kSecTrustExpirationDate             kSecTrustRevocationValidUntilDate

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

#endif /* TARGET_OS_MAC && !TARGET_OS_IPHONE */

__END_DECLS

#endif /* !_SECURITY_SECTRUSTPRIV_H_ */
