/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
 @header SecItemShim.h
 SecItemShim defines functions and macros for shimming iOS Security
 implementation to be used inside OSX.
 */

#ifndef _SECURITY_SECITEMSHIM_H_
#define _SECURITY_SECITEMSHIM_H_

#import <Availability.h>

#if TARGET_OS_OSX

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <Security/SecKey.h>

__BEGIN_DECLS

struct __SecKeyDescriptor;

OSStatus SecItemAdd_ios(CFDictionaryRef attributes, CFTypeRef *result);
OSStatus SecItemCopyMatching_ios(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecItemUpdate_ios(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete_ios(CFDictionaryRef query);

OSStatus SecKeyGeneratePair_ios(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey);
SecKeyRef SecKeyCreateRandomKey_ios(CFDictionaryRef parameters, CFErrorRef *error);

#if SECITEM_SHIM_OSX

#define SecItemAdd SecItemAdd_ios
#define SecItemCopyMatching SecItemCopyMatching_ios
#define SecItemUpdate SecItemUpdate_ios
#define SecItemDelete SecItemDelete_ios

#define SecKeyGeneratePair SecKeyGeneratePair_ios
#define SecKeyCreateRandomKey SecKeyCreateRandomKey_ios

#endif

__END_DECLS

#endif // TARGET_OS_OSX
#endif /* !_SECURITY_SECITEMSHIM_H_ */
