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
 @header SOSBackupSliceKeyBag.h - View Bags - backup bags for views
 */

#ifndef _sec_SOSBackupSliceKeyBag_
#define _sec_SOSBackupSliceKeyBag_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

extern CFStringRef bskbRkbgPrefix;

CFDataRef SOSRKNullKey(void);

// We don't have a portable header (particularly for the SIM) so for now we define the one type we need.
// This should be fixed when we get a portable AKS interface.
typedef int32_t bskb_keybag_handle_t;

typedef struct CF_BRIDGED_TYPE(id) __OpaqueSOSBackupSliceKeyBag *SOSBackupSliceKeyBagRef;

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreate(CFAllocatorRef allocator, CFSetRef peers, CFErrorRef* error);
SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateDirect(CFAllocatorRef allocator, CFDataRef aks_bag, CFErrorRef *error);

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateWithAdditionalKeys(CFAllocatorRef allocator,
                                                                     CFSetRef /*SOSPeerInfoRef*/ peers,
                                                                     CFDictionaryRef /*CFStringRef (prefix) CFDataRef (keydata) */ additionalKeys,
                                                                     CFErrorRef* error);

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error);

CFDataRef SOSBSKBCopyEncoded(SOSBackupSliceKeyBagRef BackupSliceKeyBag, CFErrorRef* error);

//
bool SOSBSKBIsDirect(SOSBackupSliceKeyBagRef backupSliceKeyBag);

CFSetRef SOSBSKBGetPeers(SOSBackupSliceKeyBagRef backupSliceKeyBag);

int SOSBSKBCountPeers(SOSBackupSliceKeyBagRef backupSliceKeyBag);

bool SOSBSKBPeerIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, SOSPeerInfoRef pi);
bool SOSBKSBKeyIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFDataRef publicKey);
bool SOSBKSBPeerBackupKeyIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, SOSPeerInfoRef pi);
bool SOSBSKBAllPeersBackupKeysAreInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFSetRef peers);
bool SOSBKSBPrefixedKeyIsInKeyBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFStringRef prefix, CFDataRef publicKey);

// Keybag fetching
CFDataRef SOSBSKBCopyAKSBag(SOSBackupSliceKeyBagRef backupSliceKeyBag, CFErrorRef* error);


// Der encoding
const uint8_t* der_decode_BackupSliceKeyBag(CFAllocatorRef allocator,
                                  SOSBackupSliceKeyBagRef* BackupSliceKeyBag, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end);

size_t der_sizeof_BackupSliceKeyBag(SOSBackupSliceKeyBagRef BackupSliceKeyBag, CFErrorRef *error);
uint8_t* der_encode_BackupSliceKeyBag(SOSBackupSliceKeyBagRef BackupSliceKeyBag, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end);

bskb_keybag_handle_t SOSBSKBLoadLocked(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                       CFErrorRef *error);

bskb_keybag_handle_t SOSBSKBLoadAndUnlockWithPeerIDAndSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                             CFStringRef peerID, CFDataRef peerSecret,
                                                             CFErrorRef *error);

bskb_keybag_handle_t SOSBSKBLoadAndUnlockWithPeerSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                        SOSPeerInfoRef peer, CFDataRef peerSecret,
                                                        CFErrorRef *error);

bskb_keybag_handle_t SOSBSKBLoadAndUnlockWithDirectSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                          CFDataRef directSecret,
                                                          CFErrorRef *error);

bskb_keybag_handle_t SOSBSKBLoadAndUnlockWithWrappingSecret(SOSBackupSliceKeyBagRef backupSliceKeyBag,
                                                            CFDataRef wrappingSecret,
                                                            CFErrorRef *error);

// Utilities for backup keys
bool SOSBSKBIsGoodBackupPublic(CFDataRef publicKey, CFErrorRef *error);

CFDataRef SOSBSKBCopyRecoveryKey(SOSBackupSliceKeyBagRef bskb);
bool SOSBSKBHasRecoveryKey(SOSBackupSliceKeyBagRef bskb);
bool SOSBSKBHasThisRecoveryKey(SOSBackupSliceKeyBagRef bskb, CFDataRef backupKey);

#endif /* defined(_sec_SOSBackupSliceKeyBag_) */
