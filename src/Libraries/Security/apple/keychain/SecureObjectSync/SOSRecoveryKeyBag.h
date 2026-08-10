/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

//
//  SOSRecoveryKeyBag.h
//  sec
//

#ifndef SOSRecoveryKeyBag_h
#define SOSRecoveryKeyBag_h

#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct __OpaqueSOSRecoveryKeyBag *SOSRecoveryKeyBagRef;

CFTypeRef SOSRecoveryKeyBageGetTypeID(void);
SOSRecoveryKeyBagRef SOSRecoveryKeyBagCreateForAccount(CFAllocatorRef allocator, CFTypeRef account, CFDataRef pubData, CFErrorRef *error);
CFDataRef SOSRecoveryKeyCopyKeyForAccount(CFAllocatorRef allocator, CFTypeRef account, SOSRecoveryKeyBagRef recoveryKeyBag, CFErrorRef *error);

CFDataRef SOSRecoveryKeyBagCopyEncoded(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef* error);
SOSRecoveryKeyBagRef SOSRecoveryKeyBagCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error);
CFDataRef SOSRecoveryKeyBagGetKeyData(SOSRecoveryKeyBagRef rkbg, CFErrorRef *error);

bool SOSRecoveryKeyBagDSIDIs(SOSRecoveryKeyBagRef rkbg, CFStringRef dsid);;

// Der encoding
const uint8_t* der_decode_RecoveryKeyBag(CFAllocatorRef allocator,
                                            SOSRecoveryKeyBagRef* RecoveryKeyBag, CFErrorRef *error,
                                            const uint8_t* der, const uint8_t *der_end);

size_t der_sizeof_RecoveryKeyBag(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef *error);
uint8_t* der_encode_RecoveryKeyBag(SOSRecoveryKeyBagRef RecoveryKeyBag, CFErrorRef *error,
                                      const uint8_t *der, uint8_t *der_end);
#endif /* SOSRecoveryKeyBag_h */
