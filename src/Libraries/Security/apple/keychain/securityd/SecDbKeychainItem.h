/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SecDbKeychainItem.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECKEYCHAINITEM_H_
#define _SECURITYD_SECKEYCHAINITEM_H_

#include "keychain/securityd/SecKeybagSupport.h"
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecDbQuery.h"

CF_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

bool ks_encrypt_data(keybag_handle_t keybag, SecAccessControlRef _Nullable access_control, CFDataRef _Nullable acm_context,
                     CFDictionaryRef secretData, CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef _Nullable *_Nonnull pBlob, bool useDefaultIV, CFErrorRef _Nullable *_Nullable error);
bool ks_encrypt_data_with_backupuuid(keybag_handle_t keybag, SecAccessControlRef _Nullable access_control, CFDataRef _Nullable acm_context,
                     CFDictionaryRef secretData, CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef _Nullable *_Nonnull pBlob, CFDataRef _Nullable *_Nullable bkUUID, bool useDefaultIV, CFErrorRef _Nullable *_Nullable error);
bool ks_encrypt_data_legacy(keybag_handle_t keybag, SecAccessControlRef _Nullable access_control, CFDataRef _Nullable acm_context,
                            CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef _Nullable *_Nonnull pBlob, bool useDefaultIV, CFErrorRef _Nullable *_Nullable error); // used for backup
bool ks_decrypt_data(keybag_handle_t keybag, CFTypeRef cryptoOp, SecAccessControlRef _Nullable *_Nullable paccess_control, CFDataRef acm_context,
                     CFDataRef blob, const SecDbClass *db_class, CFArrayRef caller_access_groups,
                     CFMutableDictionaryRef _Nullable *_Nullable attributes_p, uint32_t *_Nullable version_p, bool decryptSecretData, keyclass_t*_Nullable outKeyclass, CFErrorRef _Nullable *_Nullable error);
bool s3dl_item_from_data(CFDataRef edata, Query *q, CFArrayRef accessGroups,
                         CFMutableDictionaryRef _Nonnull *_Nonnull item, SecAccessControlRef _Nullable *_Nullable access_control, keyclass_t *_Nullable keyclass, CFErrorRef _Nullable *_Nullable error);
SecDbItemRef _Nullable SecDbItemCreateWithBackupDictionary(const SecDbClass *dbclass, CFDictionaryRef dict, keybag_handle_t src_keybag, keybag_handle_t dst_keybag, CFErrorRef _Nullable *_Nullable error);
bool SecDbItemExtractRowIdFromBackupDictionary(SecDbItemRef item, CFDictionaryRef dict, CFErrorRef _Nullable *_Nullable error);
bool SecDbItemInferSyncable(SecDbItemRef item, CFErrorRef _Nullable *_Nullable error);

CFTypeRef _Nullable SecDbKeychainItemCopyPrimaryKey(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef _Nullable *_Nullable error);
CFTypeRef _Nullable SecDbKeychainItemCopySHA256PrimaryKey(SecDbItemRef item, CFErrorRef _Nullable *_Nullable error);
CFTypeRef _Nullable SecDbKeychainItemCopySHA1(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef _Nullable *_Nullable error);
CFTypeRef _Nullable SecDbKeychainItemCopyCurrentDate(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef _Nullable *_Nullable error);
CFTypeRef _Nullable SecDbKeychainItemCopyEncryptedData(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef _Nullable *_Nullable error);

SecAccessControlRef _Nullable SecDbItemCopyAccessControl(SecDbItemRef item, CFErrorRef _Nullable *_Nullable error);
bool SecDbItemSetAccessControl(SecDbItemRef item, SecAccessControlRef access_control, CFErrorRef _Nullable *_Nullable error);

void SecDbResetMetadataKeys(void);

__END_DECLS

CF_ASSUME_NONNULL_END

#endif /* _SECURITYD_SECKEYCHAINITEM_H_ */
