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
 @header SecKeybagSupport.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECKEYBAGSUPPORT_H_
#define _SECURITYD_SECKEYBAGSUPPORT_H_

#include <CoreFoundation/CoreFoundation.h>
#include "utilities/SecAKSWrappers.h"
#include <libaks_acl_cf_keys.h>
#include <TargetConditionals.h>

#ifndef USE_KEYSTORE
// Use keystore (real or mock) on all platforms, except bridge
#define USE_KEYSTORE !TARGET_OS_BRIDGE
#endif

#if __has_include(<Kernel/IOKit/crypto/AppleKeyStoreDefs.h>)
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif

#if USE_KEYSTORE
#include <Security/SecAccessControlPriv.h>
#endif /* USE_KEYSTORE */

__BEGIN_DECLS


/* KEYBAG_NONE is private to security and have special meaning.
 They should not collide with AppleKeyStore constants, but are only referenced
 in here.
 */
#define KEYBAG_NONE (-1)   /* Set q_keybag to KEYBAG_NONE to obtain cleartext data. */
#define KEYBAG_DEVICE (g_keychain_keybag) /* actual keybag used to encrypt items */
extern keybag_handle_t g_keychain_keybag;

bool use_hwaes(void);

bool ks_crypt(CFTypeRef operation, keybag_handle_t keybag,
              keyclass_t keyclass, uint32_t textLength, const uint8_t *source, keyclass_t *actual_class,
              CFMutableDataRef dest, CFErrorRef *error);
#if USE_KEYSTORE
bool ks_encrypt_acl(keybag_handle_t keybag, keyclass_t keyclass, uint32_t textLength, const uint8_t *source,
                  CFMutableDataRef dest, CFDataRef auth_data, CFDataRef acm_context,
                  SecAccessControlRef access_control, CFErrorRef *error);
bool ks_decrypt_acl(aks_ref_key_t ref_key, CFDataRef encrypted_data, CFMutableDataRef dest,
                    CFDataRef acm_context, CFDataRef caller_access_groups,
                    SecAccessControlRef access_control, CFErrorRef *error);
bool ks_delete_acl(aks_ref_key_t ref_key, CFDataRef encrypted_data,
                   CFDataRef acm_context, CFDataRef caller_access_groups,
                   SecAccessControlRef access_control, CFErrorRef *error);
const void* ks_ref_key_get_external_data(keybag_handle_t keybag, CFDataRef key_data,
                                         aks_ref_key_t *ref_key, size_t *external_data_len, CFErrorRef *error);
bool ks_separate_data_and_key(CFDictionaryRef blob_dict, CFDataRef *ed_data, CFDataRef *key_data);

bool ks_access_control_needed_error(CFErrorRef *error, CFDataRef access_control_data, CFTypeRef operation);
bool create_cferror_from_aks(int aks_return, CFTypeRef operation, keybag_handle_t keybag, keyclass_t keyclass, CFDataRef access_control_data, CFDataRef acm_context_data, CFErrorRef *error);
#endif
bool ks_open_keybag(CFDataRef keybag, CFDataRef password, keybag_handle_t *handle, CFErrorRef *error);
bool ks_close_keybag(keybag_handle_t keybag, CFErrorRef *error);

__END_DECLS

#endif /* _SECURITYD_SECKEYBAGSUPPORT_H_ */
