/*
 * Copyright (c) 2002-2011,2012-2013,2016 Apple Inc. All Rights Reserved.
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
    @header SecIdentityPriv
    The functions provided in SecIdentityPriv.h implement a convenient way to
    match private keys with certificates.
*/

#ifndef _SECURITY_SECIDENTITYPRIV_H_
#define _SECURITY_SECIDENTITYPRIV_H_

#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <CoreFoundation/CFBase.h>

__BEGIN_DECLS

/*! @function SecIdentityCreate
    @abstract create a new identity object from the provided certificate and its associated private key.
    @param allocator CFAllocator to allocate the identity object. Pass NULL to use the default allocator.
    @param certificate A certificate reference.
    @param privateKey A private key reference.
    @result An identity reference.
*/
SecIdentityRef SecIdentityCreate(
     CFAllocatorRef allocator,
     SecCertificateRef certificate,
     SecKeyRef privateKey)
    __SEC_MAC_AND_IOS_UNKNOWN;
    //__OSX_AVAILABLE_STARTING(__MAC_10_3, __SEC_IPHONE_UNKNOWN);

#if SEC_OS_OSX
/*!
    @function ConvertArrayToKeyUsage
    @abstract Given an array of key usages defined in SecItem.h return the equivalent CSSM_KEYUSE
    @param usage An CFArrayRef containing CFTypeRefs defined in SecItem.h
          kSecAttrCanEncrypt,
          kSecAttrCanDecrypt,
          kSecAttrCanDerive,
          kSecAttrCanSign,
          kSecAttrCanVerify,
          kSecAttrCanWrap,
          kSecAttrCanUnwrap
          If the CFArrayRef is NULL then the CSSM_KEYUSAGE will be CSSM_KEYUSE_ANY
    @result A CSSM_KEYUSE.  Derived from the passed in Array
*/
CSSM_KEYUSE ConvertArrayToKeyUsage(CFArrayRef usage)
  __SEC_MAC_ONLY_UNKNOWN;

/*!
    @function SecIdentityDeleteApplicationPreferenceItems
    @abstract Delete identity preference items created by the calling application.
    @result errSecSuccess on successful deletion, or errSecItemNotFound if no items
    were found to be deleted. Other keychain error results may be possible (SecBase.h).
    @discussion This function deletes all identity preference items which match the
    application identifier of the caller. This implies that items to be deleted were
    created with SecIdentitySetPreferred on a version of macOS where this function
    is implemented, since older versions of macOS did not add application identifier
    information. Note: currently, deletion is also limited to preference items whose
    name is in URI format.
*/
OSStatus SecIdentityDeleteApplicationPreferenceItems(void);

#endif // SEC_OS_OSX

__END_DECLS

#endif /* _SECURITY_SECIDENTITYPRIV_H_ */
