/*
 * Copyright (c) 2015-2020 Apple Inc. All Rights Reserved.
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
    @header SecTrustInternal
    This header provides the interface to internal functions used by SecTrust.
*/

#ifndef _SECURITY_SECTRUSTINTERNAL_H_
#define _SECURITY_SECTRUSTINTERNAL_H_

#include <Security/SecTrust.h>

__BEGIN_DECLS

/* args_in keys. */
#define kSecTrustCertificatesKey "certificates"
#define kSecTrustAnchorsKey "anchors"
#define kSecTrustAnchorsOnlyKey "anchorsOnly"
#define kSecTrustKeychainsAllowedKey "keychainsAllowed"
#define kSecTrustPoliciesKey "policies"
#define kSecTrustResponsesKey "responses"
#define kSecTrustSCTsKey "scts"
#define kSecTrustTrustedLogsKey "trustedLogs"
#define kSecTrustVerifyDateKey "verifyDate"
#define kSecTrustExceptionsKey "exceptions"
#define kSecTrustRevocationAdditionsKey "revocationCheck"

/* args_out keys. */
#define kSecTrustDetailsKey "details"
#define kSecTrustChainKey "chain"
#define kSecTrustResultKey "result"
#define kSecTrustInfoKey "info"

extern const CFStringRef kSecCertificateDetailSHA1Digest;

#if TARGET_OS_OSX
SecKeyRef SecTrustCopyPublicKey_ios(SecTrustRef trust);
CFArrayRef SecTrustCopyProperties_ios(SecTrustRef trust);
#endif

#define kSecTrustEventNameKey "eventName"
#define kSecTrustEventAttributesKey "eventAttributes"
#define kSecTrustEventApplicationID "appID"

typedef enum {
    kSecTrustErrorSubTypeBlocked,
    kSecTrustErrorSubTypeRevoked,
    kSecTrustErrorSubTypeKeySize,
    kSecTrustErrorSubTypeWeakHash,
    kSecTrustErrorSubTypeDenied,
    kSecTrustErrorSubTypeCompliance,
    kSecTrustErrorSubTypePinning,
    kSecTrustErrorSubTypeTrust,
    kSecTrustErrorSubTypeUsage,
    kSecTrustErrorSubTypeName,
    kSecTrustErrorSubTypeExpired,
    kSecTrustErrorSubTypeInvalid,
} SecTrustErrorSubType;

#define __PC_SUBTYPE_   kSecTrustErrorSubTypeInvalid
#define __PC_SUBTYPE_N  kSecTrustErrorSubTypeName
#define __PC_SUBTYPE_E  kSecTrustErrorSubTypeExpired
#define __PC_SUBTYPE_S  kSecTrustErrorSubTypeKeySize
#define __PC_SUBTYPE_H  kSecTrustErrorSubTypeWeakHash
#define __PC_SUBTYPE_U  kSecTrustErrorSubTypeUsage
#define __PC_SUBTYPE_P  kSecTrustErrorSubTypePinning
#define __PC_SUBTYPE_V  kSecTrustErrorSubTypeRevoked
#define __PC_SUBTYPE_T  kSecTrustErrorSubTypeTrust
#define __PC_SUBTYPE_C  kSecTrustErrorSubTypeCompliance
#define __PC_SUBTYPE_D  kSecTrustErrorSubTypeDenied
#define __PC_SUBTYPE_B  kSecTrustErrorSubTypeBlocked

__END_DECLS

#endif /* !_SECURITY_SECTRUSTINTERNAL_H_ */
