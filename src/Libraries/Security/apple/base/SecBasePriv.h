/*
 * Copyright (c) 2008-2009,2011-2014,2016 Apple Inc. All Rights Reserved.
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
 @header SecBasePriv
	SecBasePriv contains private error codes from the Security framework.
*/

#ifndef _SECURITY_SECBASEPRIV_H_
#define _SECURITY_SECBASEPRIV_H_

#include <Security/SecBase.h>
#include <AvailabilityMacros.h>
#include <Availability.h>


// Macros for allowing use of availability for internal functions without digging for when
// they first existed.
// When publishing any API publicly, don't use these.
#define __SEC_MAC_ONLY_UNKNOWN __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);
#define __SEC_IOS_ONLY_UNKNOWN __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_10_0);
#define __SEC_MAC_AND_IOS_UNKNOWN __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

#if SEC_OS_OSX
#include <Security/cssmtype.h>
#endif /* SEC_OS_OSX */

__BEGIN_DECLS

/*******************************************************
 *** Private OSStatus values unique to Security APIs ***
 *******************************************************/

/*
    Note: the comments that appear after these errors are used to create SecErrorMessages.strings.
    The comments must not be multi-line, and should be in a form meaningful to an end user. If
    a different or additional comment is needed, it can be put in the header doc format, or on a
    line that does not start with errZZZ.
*/

enum
{
    errSecInvalidCertificate     = errSecDecode, // -26265,  /* This certificate could not be decoded. */
    errSecPolicyDenied			 = -26270,  /* The certificate chain was not trusted due to a policy not accepting it. */
    errSecInvalidKey             = errSecDecode, // -26274,  /* The provided key material was not valid. */
    errSecInternal               = -26276,  /* An internal error occured in the Security framework. */
    errSecUnsupportedAlgorithm   = errSecUnimplemented, // -26268,  /* An unsupported algorithm was encountered. */
    errSecUnsupportedOperation   = errSecUnimplemented, // -26271,  /* The operation you requested is not supported by this key. */
    errSecUnsupportedPadding     = errSecParam, // -26273,  /* The padding you requested is not supported. */
    errSecItemInvalidKey         = errSecParam, // -34000,  /* A string key in dictionary is not one of the supported keys. */
    errSecItemInvalidKeyType     = errSecParam, // -34001,  /* A key in a dictionary is neither a CFStringRef nor a CFNumberRef. */
    errSecItemInvalidValue       = errSecParam, // -34002,  /* A value in a dictionary is an invalid (or unsupported) CF type. */
    errSecItemClassMissing       = errSecParam, // -34003,  /* No kSecItemClass key was specified in a dictionary. */
    errSecItemMatchUnsupported   = errSecParam, // -34004,  /* The caller passed one or more kSecMatch keys to a function which does not support matches. */
    errSecUseItemListUnsupported = errSecParam, // -34005,  /* The caller passed in a kSecUseItemList key to a function which does not support it. */
    errSecUseKeychainUnsupported = errSecParam, // -34006,  /* The caller passed in a kSecUseKeychain key to a function which does not support it. */
    errSecUseKeychainListUnsupported = errSecParam, // -34007,  /* The caller passed in a kSecUseKeychainList key to a function which does not support it. */
    errSecReturnDataUnsupported  = errSecParam, // -34008,  /* The caller passed in a kSecReturnData key to a function which does not support it. */
    errSecReturnAttributesUnsupported = errSecParam, // -34009,  /* The caller passed in a kSecReturnAttributes key to a function which does not support it. */
    errSecReturnRefUnsupported   = errSecParam, // -34010,  /* The caller passed in a kSecReturnRef key to a function which does not support it. */
    errSecReturnPersistentRefUnsupported   = errSecParam, // -34010,  /* The caller passed in a kSecReturnPersistentRef key to a function which does not support it. */
    errSecValueRefUnsupported    = errSecParam, // -34012,  /* The caller passed in a kSecValueRef key to a function which does not support it. */
    errSecValuePersistentRefUnsupported = errSecParam, // -34013,  /* The caller passed in a kSecValuePersistentRef key to a function which does not support it. */
    errSecReturnMissingPointer   = errSecParam, // -34014,  /* The caller passed asked for something to be returned but did not pass in a result pointer. */
	errSecMatchLimitUnsupported  = errSecParam, // -34015,  /* The caller passed in a kSecMatchLimit key to a call which does not support limits. */
	errSecItemIllegalQuery       = errSecParam, // -34016,  /* The caller passed in a query which contained too many keys. */
	errSecWaitForCallback        = -34017,  /* This operation is incomplete, until the callback is invoked (not an error). */
    errSecUpgradePending         = -34019,  /* Error returned if keychain database needs a schema migration but the device is locked, clients should wait for a device unlock notification and retry the command. */

    errSecMPSignatureInvalid     = -25327,  /* Signature invalid on MP message */
    errSecOTRTooOld              = -25328,  /* Message is too old to use */
    errSecOTRIDTooNew            = -25329,  /* Key ID is too new to use! Message from the future? */
    errSecOTRNotReady            = -25331,  /* Can't process packets because the session hasn't finished negotiating */

    errSecAuthNeeded             = -25330,  /* Auth is needed before the requested action can be performed.  An array of
                                               constraints to be fulfilled is passed inside error.userInfo's 'cons' key. */

    errSecPeersNotAvailable      = -25336,  /* No peers in the circle are available/online. */
    errSecErrorStringNotAvailable= -25337,  /* Unable to load error string for error */

    /* UNUSED enums */
    errSecDeviceIDNeeded         = -25332,  /* Cannot send IDS messages without having our own IDS ID. */
    errSecIDSNotRegistered       = -25333,  /* IDS is not set up or devices are not registered/available within an IDS account. */
    errSecFailedToSendIDSMessage = -25334,  /* Failed to send IDS message. */
    errSecDeviceIDNoMatch        = -25335,  /* The provided device ID does not match any device IDs in the ids account. */
    errSecTimedOut               = -25336,  /* Timed out waiting for task */
};

// Guard for CFNetwork
#define SECURITY_PROVIDES_INVALIDTRUSTSETTINGS

#if SEC_OS_OSX
const char *cssmErrorString(CSSM_RETURN error)
    __SEC_MAC_ONLY_UNKNOWN;
#endif

OSStatus SecKeychainErrFromOSStatus(OSStatus osStatus)
    API_AVAILABLE(macos(10.4), ios(NA), bridgeos(NA));

/*
 * For used when running in root session as a agent/daemon and want to redirect to
 * a background user session. This call must be called before any Sec calls are done,
 * so very early in main().
 *
 * This only apply to MacOS where background session exists.
 */
void _SecSetSecuritydTargetUID(uid_t uid)
    API_AVAILABLE(macos(10.13.5)) API_UNAVAILABLE(ios, macCatalyst, watchos, tvos, bridgeos);



__END_DECLS

#endif /* !_SECURITY_SECBASEPRIV_H_ */
