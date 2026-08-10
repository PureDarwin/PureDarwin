/*
 * Copyright (c) 2007-2017 Apple Inc.  All Rights Reserved.
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

#if defined(TARGET_DARWINOS) && TARGET_DARWINOS
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#include <os/transaction_private.h>
#include <os/variant_private.h>

#include <corecrypto/ccec.h>
#include "keychain/SecureObjectSync/SOSPeerInfoDER.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include "keychain/SecureObjectSync/SOSControlServer.h"
#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecEntitlements.h>
#include <Security/SecInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTask.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecuritydXPC.h>
#include "trust/trustd/OTATrustUtilities.h"
#include "keychain/securityd/SOSCloudCircleServer.h"
#include "keychain/securityd/SecItemBackupServer.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecLogSettingsServer.h"
#include "keychain/securityd/SecOTRRemote.h"
#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "keychain/securityd/iCloudTrace.h"
#include "keychain/securityd/spi.h"
#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCoreAnalytics.h>
#include <utilities/SecDb.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecXPCError.h>
#include <utilities/debugging.h>
#include <utilities/SecInternalReleasePriv.h>
#include <utilities/der_plist_internal.h>
#include <utilities/der_plist.h>
#include "trust/trustd/personalization.h"
#include "trust/trustd/SecPinningDb.h"
#include "keychain/securityd/SFKeychainControlManager.h"

#include <keychain/ckks/CKKS.h>
#include <keychain/ckks/CKKSControlServer.h>
#include "keychain/ot/OctagonControlServer.h"

#include "keychain/securityd/SFKeychainServer.h"
#if !TARGET_OS_BRIDGE
#include "keychain/securityd/PolicyReporter.h"
#endif

#include <AssertMacros.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <CoreFoundation/CoreFoundation.h>
// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp

#if TARGET_OS_OSX
#include <Security/SecTaskPriv.h>
#include <login/SessionAgentStatusCom.h>
#endif
#include <asl.h>
#include <bsm/libbsm.h>
#include <ipc/securityd_client.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <notify.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <syslog.h>
#include <xpc/private.h>
#include <xpc/xpc.h>

#include <ipc/server_security_helpers.h>
#include <ipc/server_entitlement_helpers.h>

#include "keychain/ot/OT.h"
#include "keychain/escrowrequest/EscrowRequestXPCServer.h"
#include "keychain/escrowrequest/EscrowRequestServerHelpers.h"

#if TARGET_OS_OSX
#include <sandbox.h>
#include <pwd.h>
#include <err.h>
#endif

#include "util.h"

static void refresh_prng(void)
{
    aks_ref_key_t ref = NULL;

    int fd = open("/dev/random", O_WRONLY);
    if (fd == -1) {
        secerror("failed to open /dev/random (%d)", errno);
        goto out;
    }

    /* create class F ref-key, and use its public key as an entropy source */
    int err = aks_ref_key_create(bad_keybag_handle, key_class_f, key_type_asym_ec_p256, NULL, 0, &ref);
    if (err != kAKSReturnSuccess) {
        secerror("failed to create refkey (%d)", err);
        goto out;
    }

    size_t pub_key_len = 0;
    const uint8_t *pub_key = aks_ref_key_get_public_key(ref, &pub_key_len);
    if (pub_key_len > ccec_export_pub_size_cp(ccec_cp_256())) {
        secerror("invalid pub key (%zu)", pub_key_len);
        goto out;
    }

    while (pub_key_len > 0) {
        ssize_t n = write(fd, pub_key, pub_key_len);
        if (n == -1) {
            secerror("failed to write /dev/random (%d)", errno);
            goto out;
        }

        pub_key += n;
        pub_key_len -= n;
    }

 out:
    if (ref) {
        aks_ref_key_free(&ref);
    }

    if (fd >= 0) {
        close(fd);
    }
}

#if SECUREOBJECTSYNC

CF_RETURNS_RETAINED
static CFStringRef
_xpc_dictionary_copy_CFString(xpc_object_t xdict, const char *key)
{
    CFStringRef result = NULL;
    const char *str = xpc_dictionary_get_string(xdict, key);
    if (str != NULL) {
        result = CFStringCreateWithCString(kCFAllocatorDefault, str, kCFStringEncodingUTF8);
    }
    return result;
}

CF_RETURNS_RETAINED
static CFDataRef
_xpc_dictionary_copy_CFDataNoCopy(xpc_object_t xdict, const char *key)
{
    CFDataRef result = NULL;
    size_t len = 0;
    const void *ptr = xpc_dictionary_get_data(xdict, key, &len);
    if (ptr != NULL) {
        result = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, ptr, len, kCFAllocatorNull);
    }
    return result;
}

static void with_label_and_password(xpc_object_t message, void (^action)(CFStringRef label, CFDataRef password)) {
    CFStringRef user_label = _xpc_dictionary_copy_CFString(message, kSecXPCKeyUserLabel);
    CFDataRef user_password = _xpc_dictionary_copy_CFDataNoCopy(message, kSecXPCKeyUserPassword);

    if (user_label != NULL && user_password != NULL) {
        action(user_label, user_password);
    }

    CFReleaseNull(user_label);
    CFReleaseNull(user_password);
}

static void with_label_and_password_and_dsid(xpc_object_t message, void (^action)(CFStringRef label, CFDataRef password, CFStringRef dsid)) {
    CFStringRef user_label = _xpc_dictionary_copy_CFString(message, kSecXPCKeyUserLabel);
    CFDataRef user_password = _xpc_dictionary_copy_CFDataNoCopy(message, kSecXPCKeyUserPassword);
    CFStringRef dsid = _xpc_dictionary_copy_CFString(message, kSecXPCKeyDSID);

    /* dsid is optional */
    if (user_label != NULL && user_password != NULL) {
        action(user_label, user_password, dsid);
    }

    CFReleaseNull(user_label);
    CFReleaseNull(user_password);
    CFReleaseNull(dsid);
}

static void with_view_and_action(xpc_object_t message, void (^action)(CFStringRef view_name, uint64_t view_action_code)) {
    CFStringRef view = _xpc_dictionary_copy_CFString(message, kSecXPCKeyViewName);
    const int64_t number = xpc_dictionary_get_int64(message, kSecXPCKeyViewActionCode);

    if (view != NULL) {
        action(view, number);
    }

    CFReleaseNull(view);
}

static CFArrayRef SecXPCDictionaryCopyPeerInfoArray(xpc_object_t dictionary, const char *key, CFErrorRef *error) {
    return CreateArrayOfPeerInfoWithXPCObject(xpc_dictionary_get_value(dictionary, key), error);
}

static CFDataRef SecXPCDictionaryCopyCFDataRef(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFDataRef retval = NULL;
    const uint8_t *bytes = NULL;
    size_t len = 0;

    bytes = xpc_dictionary_get_data(message, key, &len);
    require_action_quiet(bytes, errOut, SOSCreateError(kSOSErrorBadKey, CFSTR("missing CFDataRef info"), NULL, error));
    retval = CFDataCreate(NULL, bytes, len);
    require_action_quiet(retval, errOut, SOSCreateError(kSOSErrorBadKey, CFSTR("could not allocate CFDataRef info"), NULL, error));
errOut:
    return retval;
}

static CFSetRef CreateCFSetRefFromXPCObject(xpc_object_t xpcSetDER, CFErrorRef* error) {
    CFSetRef retval = NULL;
    require_action_quiet(xpcSetDER, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedNull, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Unexpected Null Set to decode")));

    require_action_quiet(xpc_get_type(xpcSetDER) == XPC_TYPE_DATA, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("xpcSetDER not data, got %@"), xpcSetDER));

    const uint8_t* der = xpc_data_get_bytes_ptr(xpcSetDER);
    const uint8_t* der_end = der + xpc_data_get_length(xpcSetDER);
    der = der_decode_set(kCFAllocatorDefault, &retval, error, der, der_end);
    if (der != der_end) {
        SecError(errSecDecode, error, CFSTR("trailing garbage at end of SecAccessControl data"));
        goto errOut;
    }
    return retval;
errOut:
    CFReleaseNull(retval);
    return NULL;
}

static SOSPeerInfoRef SecXPCDictionaryCopyPeerInfo(xpc_object_t message, const char *key, CFErrorRef *error) {
    size_t length = 0;
    const uint8_t *der = xpc_dictionary_get_data(message, key, &length);

    return SecRequirementError(der != NULL, error, CFSTR("No data for key %s"), key) ? SOSPeerInfoCreateFromDER(kCFAllocatorDefault, error, &der, der + length) : NULL;
}

static CFSetRef SecXPCSetCreateFromXPCDictionaryElement(xpc_object_t event, const char *key) {
    CFErrorRef error = NULL;
    xpc_object_t object = xpc_dictionary_get_value(event, key);
    CFSetRef retval = NULL;
    if(object) retval = CreateCFSetRefFromXPCObject(object, &error);
    CFReleaseNull(error);
    return retval;
}

static inline
void xpc_dictionary_set_and_consume_CFArray(xpc_object_t xdict, const char *key, CF_CONSUMED CFArrayRef cf_array) {
    if (cf_array) {
        xpc_object_t xpc_array = _CFXPCCreateXPCObjectFromCFObject(cf_array);
        xpc_dictionary_set_value(xdict, key, xpc_array);
        xpc_release(xpc_array);
    }
    CFReleaseNull(cf_array);
}

static inline
bool xpc_dictionary_set_and_consume_PeerInfoArray(xpc_object_t xdict, const char *key, CF_CONSUMED CFArrayRef cf_array, CFErrorRef *error) {
    bool success = true;
    if (cf_array) {
        xpc_object_t xpc_array = CreateXPCObjectWithArrayOfPeerInfo(cf_array, error);
        if (xpc_array) {
            xpc_dictionary_set_value(xdict, key, xpc_array);
            xpc_release(xpc_array);
        } else {
            success = false;
        }
    }
    CFReleaseNull(cf_array);
    return success;
}

#endif /* SECUREOBJECTSYNC */

static CFDataRef
SecDataCopyMmapFileDescriptor(int fd, void **mem, size_t *size, CFErrorRef *error)
{
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        return NULL;
    }

    *size = (size_t)sb.st_size;
    if ((off_t)*size != sb.st_size) {
        return NULL;
    }

    *mem = mmap(NULL, *size, PROT_READ, MAP_SHARED, fd, 0);
    if (*mem == MAP_FAILED) {
        return NULL;
    }

    return CFDataCreateWithBytesNoCopy(NULL, *mem, *size, kCFAllocatorNull);
}

static bool
SecDataWriteFileDescriptor(int fd, CFDataRef data)
{
    CFIndex count = CFDataGetLength(data);
    const uint8_t *ptr = CFDataGetBytePtr(data);
    bool writeResult = false;

    while (count) {
        ssize_t ret = write(fd, ptr, count);
        if (ret <= 0)
            break;
        count -= ret;
        ptr += ret;
    }
    if (count == 0)
        writeResult = true;

    return writeResult;
}

// Returns error if entitlement isn't present.
static bool
EntitlementPresentAndTrue(uint64_t op, SecTaskRef clientTask, CFStringRef entitlement, CFErrorRef *error)
{
    if (!SecTaskGetBooleanValueForEntitlement(clientTask, entitlement)) {
        SecError(errSecMissingEntitlement, error, CFSTR("%@: %@ lacks entitlement %@"), SOSCCGetOperationDescription((enum SecXPCOperation)op), clientTask, entitlement);
        return false;
    }
    return true;
}

static bool
EntitlementAbsentOrFalse(uint64_t op, SecTaskRef clientTask, CFStringRef entitlement, CFErrorRef *error)
{
    if (SecTaskGetBooleanValueForEntitlement(clientTask, entitlement)) {
        SecError(errSecNotAvailable, error, CFSTR("%@: %@ has entitlement %@"), SOSCCGetOperationDescription((enum SecXPCOperation) op), clientTask, entitlement);
        return false;
    }
    return true;
}

static void securityd_xpc_dictionary_handler(const xpc_connection_t connection, xpc_object_t event) {
    xpc_type_t type = xpc_get_type(event);
    __block CFErrorRef error = NULL;
    xpc_object_t xpcError = NULL;
    xpc_object_t replyMessage = NULL;
    CFDataRef  clientAuditToken = NULL;
    CFArrayRef domains = NULL;
    SecurityClient client = {
        .task = NULL,
        .accessGroups = NULL,
        .musr = NULL,
        .uid = xpc_connection_get_euid(connection),
        .allowSystemKeychain = false,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
        .canAccessNetworkExtensionAccessGroups = false,
        .applicationIdentifier = NULL,
        .isAppClip = false,
    };

    secdebug("serverxpc", "entering");
    if (type == XPC_TYPE_DICTIONARY) {
        // TODO: Find out what we're dispatching.
        replyMessage = xpc_dictionary_create_reply(event);

        uint64_t operation = xpc_dictionary_get_uint64(event, kSecXPCKeyOperation);

        audit_token_t auditToken = {};
        xpc_connection_get_audit_token(connection, &auditToken);
        clientAuditToken = CFDataCreate(kCFAllocatorDefault, (const UInt8*)&auditToken, sizeof(auditToken));

        if (!fill_security_client(&client, xpc_connection_get_euid(connection), auditToken)) {
            CFReleaseNull(clientAuditToken);
            xpc_connection_send_message(connection, replyMessage);
            xpc_release(replyMessage);
            return;
        }

#if TARGET_OS_IOS
        if (operation == sec_add_shared_web_credential_id || operation == sec_copy_shared_web_credential_id) {
            domains = SecTaskCopySharedWebCredentialDomains(client.task);
        }
#endif
        secinfo("serverxpc", "XPC [%@] operation: %@ (%" PRIu64 ")", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), operation);

        switch (operation)
            {
            case sec_item_add_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        // Check for any entitlement-required attributes
                        bool entitlementsCorrect = true;
                        if(CFDictionaryGetValue(query, kSecAttrDeriveSyncIDFromItemAttributes) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextServiceIdentifier) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextPublicKey) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextPublicIdentity)) {
                            entitlementsCorrect = EntitlementPresentAndTrue(sec_item_add_id, client.task, kSecEntitlementPrivateCKKSPlaintextFields, &error);
                        }
                        if(entitlementsCorrect &&
                           (CFDictionaryGetValue(query, kSecDataInetExtraNotes) ||
                            CFDictionaryGetValue(query, kSecDataInetExtraHistory) ||
                            CFDictionaryGetValue(query, kSecDataInetExtraClientDefined0) ||
                            CFDictionaryGetValue(query, kSecDataInetExtraClientDefined1) ||
                            CFDictionaryGetValue(query, kSecDataInetExtraClientDefined2) ||
                            CFDictionaryGetValue(query, kSecDataInetExtraClientDefined3))) {
                            entitlementsCorrect = EntitlementPresentAndTrue(sec_item_add_id, client.task, kSecEntitlementPrivateInetExpansionFields, &error);
                        }

                        if (entitlementsCorrect && CFDictionaryGetValue(query, kSecAttrSysBound)) {
                            entitlementsCorrect = EntitlementPresentAndTrue(sec_item_add_id, client.task, kSecEntitlementPrivateSysBound, &error);
                        }

                        CFTypeRef result = NULL;
                        if(entitlementsCorrect) {
                            if (_SecItemAdd(query, &client, &result, &error) && result) {
                                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                                CFReleaseNull(result);
                            }
                        }
                        CFReleaseNull(query);
                    }
                    break;
                }
            }
            case sec_item_copy_matching_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        CFTypeRef result = NULL;
                        if (_SecItemCopyMatching(query, &client, &result, &error) && result) {
                            SecXPCDictionarySetPListWithRepair(replyMessage, kSecXPCKeyResult, result, true, &error);
                            CFReleaseNull(result);
                        }
                        CFReleaseNull(query);
                    }
                    break;
                }
            }
            case sec_item_update_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_update_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        CFDictionaryRef attributesToUpdate = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyAttributesToUpdate, &error);
                        if (attributesToUpdate) {
                            // Check for any entitlement-required attributes
                            bool entitlementsCorrect = true;
                            if(CFDictionaryGetValue(query, kSecAttrDeriveSyncIDFromItemAttributes) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextServiceIdentifier) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextPublicKey) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextPublicIdentity)) {
                                entitlementsCorrect = EntitlementPresentAndTrue(sec_item_update_id, client.task, kSecEntitlementPrivateCKKSPlaintextFields, &error);
                            }
                            if(entitlementsCorrect &&
                               (CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraNotes) ||
                                CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraHistory) ||
                                CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraClientDefined0) ||
                                CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraClientDefined1) ||
                                CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraClientDefined2) ||
                                CFDictionaryGetValue(attributesToUpdate, kSecDataInetExtraClientDefined3))) {
                                entitlementsCorrect = EntitlementPresentAndTrue(sec_item_update_id, client.task, kSecEntitlementPrivateInetExpansionFields, &error);
                            }
                            if (entitlementsCorrect && CFDictionaryGetValue(query, kSecAttrSysBound)) {
                                entitlementsCorrect = EntitlementPresentAndTrue(sec_item_update_id, client.task, kSecEntitlementPrivateSysBound, &error);
                            }

                            if(entitlementsCorrect) {
                                bool result = _SecItemUpdate(query, attributesToUpdate, &client, &error);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            }
                            CFReleaseNull(attributesToUpdate);
                        }
                        CFReleaseNull(query);
                    }
                }
                break;
            }
            case sec_item_delete_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        bool result = _SecItemDelete(query, &client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                        CFReleaseNull(query);
                    }
                }
                break;
            }
            case sec_item_update_token_items_for_access_groups_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_update_token_items_for_access_groups_id, client.task, kSecEntitlementKeychainDeny, &error) &&
                    EntitlementPresentAndTrue(sec_item_update_token_items_for_access_groups_id, client.task, kSecEntitlementUpdateTokenItems, &error)) {
                    CFStringRef tokenID =  SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyArray, &error);
                    CFArrayRef tokenItems = SecXPCDictionaryCopyArray(event, kSecXPCKeyQuery, &error);
                    if (tokenID) {
                        bool result = _SecItemUpdateTokenItemsForAccessGroups(tokenID, accessGroups, tokenItems, &client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                    }
                    CFReleaseNull(tokenID);
                    CFReleaseNull(accessGroups);
                    CFReleaseNull(tokenItems);
                }
                break;
            }
            case sec_delete_all_id:
            {
                bool retval = false;
#if TARGET_OS_IPHONE
                /* buddy is temporary allowed to do this */
                CFStringRef applicationIdentifier = SecTaskCopyApplicationIdentifier(client.task);
                bool isBuddy = applicationIdentifier &&
                    CFEqual(applicationIdentifier, CFSTR("com.apple.purplebuddy"));
                CFReleaseNull(applicationIdentifier);

                if (isBuddy || EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateDeleteAll, &error))
                {
                    retval = _SecItemDeleteAll(&error);
                }
#endif
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                break;
            }
            case sec_keychain_backup_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef keybag = NULL, passcode = NULL;
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyKeybag, &keybag, &error)) {
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                            bool emcs = SecXPCDictionaryGetBool(event, kSecXPCKeyEMCSBackup, NULL);
                            CFDataRef backup = _SecServerKeychainCreateBackup(&client, keybag, passcode, emcs, &error);
                            if (backup) {
                                int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                                if (fd < 0) {
                                    SecXPCDictionarySetData(replyMessage, kSecXPCKeyResult, backup, &error);
                                } else {
                                    bool writeResult = SecDataWriteFileDescriptor(fd, backup);
                                    if (close(fd) != 0)
                                        writeResult = false;
                                    if (!writeResult)
                                        SecError(errSecIO, &error, CFSTR("Failed to write backup file: %d"), errno);
                                    SecXPCDictionarySetBool(replyMessage, kSecXPCKeyResult, writeResult, NULL);
                                }
                                CFRelease(backup);
                            }
                            CFReleaseSafe(passcode);
                        }
                        CFReleaseSafe(keybag);
                    }
                }
                break;
            }
            case sec_keychain_restore_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef backup = NULL;
                    void *mem = NULL;
                    size_t size = 0;

                    int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                    if (fd != -1) {
                        backup = SecDataCopyMmapFileDescriptor(fd, &mem, &size, &error);
                    } else {
                        backup = SecXPCDictionaryCopyData(event, kSecXPCKeyBackup, &error);
                    }
                    if (backup) {
                        CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                        if (keybag) {
                            CFDataRef passcode = NULL;
                            if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                                bool result = _SecServerKeychainRestore(backup, &client, keybag, passcode, &error);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                                CFReleaseSafe(passcode);
                            }
                        }
                        CFReleaseNull(keybag);
                    }
                    CFReleaseNull(backup);
                    if (fd != -1)
                        close(fd);
                    if (mem) {
                        munmap(mem, size);
                    }
                }
                break;
            }
            case sec_keychain_backup_keybag_uuid_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef backup = NULL;
                    CFStringRef uuid = NULL;
                    void *mem = NULL;
                    size_t size = 0;

                    int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                    if (fd != -1) {
                        backup = SecDataCopyMmapFileDescriptor(fd, &mem, &size, &error);
                        if (backup)
                            uuid = _SecServerBackupCopyUUID(backup, &error);
                    }
                    if (uuid)
                        SecXPCDictionarySetString(replyMessage, kSecXPCKeyResult, uuid, &error);

                    CFReleaseNull(backup);
                    if (fd != -1)
                        close(fd);
                    if (mem) {
                        munmap(mem, size);
                    }
                    CFReleaseNull(uuid);
                }
                break;
            }
#if SECUREOBJECTSYNC
            case sec_keychain_sync_update_message_id:
            {
                CFDictionaryRef updates = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (updates) {
                    CFArrayRef result = _SecServerKeychainSyncUpdateMessage(updates, &error);
                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                    CFReleaseNull(result);
                }
                CFReleaseNull(updates);
                break;
            }
            case sec_keychain_backup_syncable_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDictionaryRef oldbackup = NULL;
                    if (SecXPCDictionaryCopyDictionaryOptional(event, kSecXPCKeyBackup, &oldbackup, &error)) {
                        CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                        if (keybag) {
                            CFDataRef passcode = NULL;
                            if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                                CFDictionaryRef newbackup = _SecServerBackupSyncable(oldbackup, keybag, passcode, &error);
                                if (newbackup) {
                                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, newbackup, &error);
                                    CFRelease(newbackup);
                                }
                                CFReleaseSafe(passcode);
                            }
                            CFReleaseNull(keybag);
                        }
                        CFReleaseSafe(oldbackup);
                    }
                }
                break;
            }
            case sec_keychain_restore_syncable_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDictionaryRef backup = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyBackup, &error);
                    if (backup) {
                        CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                        if (keybag) {
                            CFDataRef passcode = NULL;
                            if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                                bool result = _SecServerRestoreSyncable(backup, keybag, passcode, &error);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                                CFReleaseSafe(passcode);
                            }
                            CFReleaseNull(keybag);
                        }
                        CFReleaseNull(backup);
                    }
                }
                break;
            }
            case sec_item_backup_copy_names_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFArrayRef names = SecServerItemBackupCopyNames(&error);
                    SecXPCDictionarySetPListOptional(replyMessage, kSecXPCKeyResult, names, &error);
                    CFReleaseSafe(names);
                }
                break;
            }
            case sec_item_backup_ensure_copy_view_id:
            {
                if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFStringRef viewName = NULL;
                    if (SecXPCDictionaryCopyStringOptional(event, kSecXPCKeyString, &viewName, &error)) {
                        CFStringRef name = SecServerItemBackupEnsureCopyView(viewName, &error);
                        SecXPCDictionarySetString(replyMessage, kSecXPCKeyResult, name, &error);
                        CFReleaseNull(name);
                    }
                }
                break;
            }
            case sec_item_backup_handoff_fd_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                    int fd = -1;
                    if (backupName) {
                        fd = SecServerItemBackupHandoffFD(backupName, &error);
                        CFRelease(backupName);
                    }
                    SecXPCDictionarySetFileDescriptor(replyMessage, kSecXPCKeyResult, fd, &error);
                    if (fd != -1)
                        close(fd);
                }
                break;
            }
            case sec_item_backup_set_confirmed_manifest_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef keybagDigest = NULL;
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyKeybag, &keybagDigest, &error)) {
                        CFDataRef manifest = NULL;
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCData, &manifest, &error)) {
                            CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                            if (backupName) {
                                bool result = SecServerItemBackupSetConfirmedManifest(backupName, keybagDigest, manifest, &error);
                                CFRelease(backupName);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            }
                            CFReleaseSafe(manifest);
                        }
                    }
                    CFReleaseNull(keybagDigest);
                }
                break;
            }
            case sec_item_backup_restore_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    bool result = false;
                    CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                    if (backupName) {
                        CFStringRef peerID = NULL;
                        if (SecXPCDictionaryCopyStringOptional(event, kSecXPCKeyDigest, &peerID, &error)) {
                            CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                            if (keybag) {
                                CFDataRef secret = SecXPCDictionaryCopyData(event, kSecXPCKeyUserPassword, &error);
                                if (secret) {
                                    CFDataRef backup = SecXPCDictionaryCopyData(event, kSecXPCData, &error);
                                    if (backup) {
                                        result = SecServerItemBackupRestore(backupName, peerID, keybag, secret, backup, &error);
                                        CFRelease(backup);
                                    }
                                    CFRelease(secret);
                                }
                                CFRelease(keybag);
                            }
                            CFReleaseSafe(peerID);
                        }
                        CFRelease(backupName);
                    }
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                }
                break;
            }
            case sec_add_shared_web_credential_id:
            {
#if SHAREDWEBCREDENTIALS
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;

                    CFStringRef appID = (client.task) ? SecTaskCopyApplicationIdentifier(client.task) : NULL;
                    if (_SecAddSharedWebCredential(query, &client, &auditToken, appID, domains, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFReleaseNull(result);
                    }
                    CFReleaseSafe(appID);
                    CFReleaseNull(query);
                }
#else
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, kCFBooleanFalse, &error);
#endif
                break;
            }
            case sec_copy_shared_web_credential_id:
            {
#if SHAREDWEBCREDENTIALS
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;
                    CFStringRef appID = (client.task) ? SecTaskCopyApplicationIdentifier(client.task) : NULL;
                    if (_SecCopySharedWebCredential(query, &client, &auditToken, appID, domains, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFReleaseNull(result);
                    }
                    CFReleaseSafe(appID);
                    CFReleaseNull(query);
                }
#else
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, kCFBooleanFalse, &error);
#endif
                break;
            }
            case sec_get_log_settings_id:
            {
                CFPropertyListRef currentList = SecCopyLogSettings_Server(&error);
                if (currentList) {
                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, currentList, &error);
                }
                CFReleaseSafe(currentList);
                break;
            }
            case sec_set_xpc_log_settings_id:
            {
                CFPropertyListRef newSettings = SecXPCDictionaryCopyPList(event, kSecXPCKeyQuery, &error);
                if (newSettings) {
                    SecSetXPCLogSettings_Server(newSettings, &error);
                }
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                CFReleaseNull(newSettings);
                break;
            }
            case sec_set_circle_log_settings_id:
            {
                CFPropertyListRef newSettings = SecXPCDictionaryCopyPList(event, kSecXPCKeyQuery, &error);
                if (newSettings) {
                    SecSetCircleLogSettings_Server(newSettings, &error);
                }
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                CFReleaseNull(newSettings);
                break;
            }
            case sec_otr_session_create_remote_id:
            {
                CFDataRef publicPeerId = NULL;
                if (SecXPCDictionaryCopyDataOptional(event, kSecXPCPublicPeerId, &publicPeerId, &error)) {
                    CFDataRef otrSession = _SecOTRSessionCreateRemote(publicPeerId, &error);
                    if (otrSession) {
                        SecXPCDictionarySetData(replyMessage, kSecXPCKeyResult, otrSession, &error);
                        CFReleaseNull(otrSession);
                    }
                    CFReleaseSafe(publicPeerId);
                }
                break;
            }
            case sec_otr_session_process_packet_remote_id:
            {
                CFDataRef sessionData = NULL, inputPacket = NULL, outputSessionData = NULL, outputPacket = NULL;
                bool readyForMessages = false;
                if (SecXPCDictionaryCopyDataOptional(event, kSecXPCOTRSession, &sessionData, &error)) {
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCData, &inputPacket, &error)) {
                        bool result = _SecOTRSessionProcessPacketRemote(sessionData, inputPacket, &outputSessionData, &outputPacket, &readyForMessages, &error);
                        if (result) {
                            SecXPCDictionarySetData(replyMessage, kSecXPCOTRSession, outputSessionData, &error);
                            SecXPCDictionarySetData(replyMessage, kSecXPCData, outputPacket, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCOTRReady, readyForMessages);
                            CFReleaseNull(outputSessionData);
                            CFReleaseNull(outputPacket);
                        }
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);

                        CFReleaseSafe(inputPacket);
                    }
                    CFReleaseSafe(sessionData);
                }
                break;
            }
            case kSecXPCOpTryUserCredentials:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password_and_dsid(event, ^(CFStringRef label, CFDataRef password, CFStringRef dsid) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCTryUserCredentials_Server(label, password, dsid, &error));
                    });
                }
                break;
            case kSecXPCOpSetUserCredentials:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password(event, ^(CFStringRef label, CFDataRef password) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSetUserCredentials_Server(label, password, &error));
                    });
                }
                break;
            case kSecXPCOpSetUserCredentialsAndDSID:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password_and_dsid(event, ^(CFStringRef label, CFDataRef password, CFStringRef dsid) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSetUserCredentialsAndDSID_Server(label, password, dsid, &error));
                    });
                }
                break;
            case kSecXPCOpView:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_view_and_action(event, ^(CFStringRef view, uint64_t actionCode) {
                        xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                                 SOSCCView_Server(view, (SOSViewActionCode)actionCode, &error));
                    });
                }
                break;
            case kSecXPCOpViewSet:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        CFSetRef enabledViews = SecXPCSetCreateFromXPCDictionaryElement(event, kSecXPCKeyEnabledViewsKey);
                        CFSetRef disabledViews = SecXPCSetCreateFromXPCDictionaryElement(event, kSecXPCKeyDisabledViewsKey);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCViewSet_Server(enabledViews, disabledViews));
                        CFReleaseNull(enabledViews);
                        CFReleaseNull(disabledViews);
                    }
                break;
            case kSecXPCOpCanAuthenticate:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCCanAuthenticate_Server(&error));
                }
                break;
            case kSecXPCOpPurgeUserCredentials:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCPurgeUserCredentials_Server(&error));
                }
                break;
            case kSecXPCOpDeviceInCircle:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCThisDeviceIsInCircle_Server(&error));
                }
                break;
            case kSecXPCOpRequestToJoin:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRequestToJoinCircle_Server(&error));
                }
                break;
            case kSecXPCOpAccountHasPublicKey:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCAccountHasPublicKey_Server(&error));
                }
                break;

            case kSecXPCOpRequestToJoinAfterRestore:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRequestToJoinCircleAfterRestore_Server(&error));
                }
                break;
            case kSecXPCOpRequestDeviceID:
            case kSecXPCOpSetDeviceID:
            case kSecXPCOpHandleIDSMessage:
            case kSecXPCOpSyncWithIDSPeer:
            case kSecXPCOpSendIDSMessage:
            case kSecXPCOpPingTest:
            case kSecXPCOpIDSDeviceID:
            case kSecXPCOpSyncWithKVSPeerIDOnly:{
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyError, errSecUnimplemented);
                }
                break;
            case kSecXPCOpAccountSetToNew:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCAccountSetToNew_Server(&error));
                }
                break;
            case kSecXPCOpResetToOffering:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCResetToOffering_Server(&error));
                }
                break;
            case kSecXPCOpResetToEmpty:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCResetToEmpty_Server(&error));
                }
                break;
            case kSecXPCOpRemoveThisDeviceFromCircle:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRemoveThisDeviceFromCircle_Server(&error));
                }
                break;
            case kSecXPCOpRemovePeersFromCircle:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFArrayRef applicants = SecXPCDictionaryCopyPeerInfoArray(event, kSecXPCKeyPeerInfoArray, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRemovePeersFromCircle_Server(applicants, &error));
                    CFReleaseNull(applicants);
                }
                break;
                case kSecXPCOpLoggedIntoAccount:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        SOSCCNotifyLoggedIntoAccount_Server();
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                true);
                    }
                    break;
            case kSecXPCOpLoggedOutOfAccount:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCLoggedOutOfAccount_Server(&error));
                }
                break;
            case kSecXPCOpAcceptApplicants:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfoArray);
                    // CreateArrayOfPeerInfoWithXPCObject enforces that xapplicants is a non-NULL xpc data object
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCAcceptApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpRejectApplicants:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfoArray);
                    // CreateArrayOfPeerInfoWithXPCObject enforces that xapplicants is a non-NULL xpc data object
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCRejectApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpSetNewPublicBackupKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef publicBackupKey = SecXPCDictionaryCopyData(event, kSecXPCKeyNewPublicBackupKey, &error);
                        if (publicBackupKey != NULL) {
                            SOSPeerInfoRef peerInfo = SOSCCSetNewPublicBackupKey_Server(publicBackupKey, &error);
                            CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                            CFReleaseNull(peerInfo);
                            if (peerInfoData) {
                                xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                                xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                                xpc_release(xpc_object);
                            }
                            CFReleaseNull(peerInfoData);
                            CFReleaseSafe(publicBackupKey);
                        }
                    }
                }
                break;
            case kSecXPCOpRegisterRecoveryPublicKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef recovery_key = SecXPCDictionaryCopyData(event, kSecXPCKeyRecoveryPublicKey, &error);
                        if (recovery_key != NULL) {
                            uint8_t zero = 0;
                            CFDataRef nullData = CFDataCreate(kCFAllocatorDefault, &zero, 1); // token we send if we really wanted to send NULL
                            if(CFEqual(recovery_key, nullData)) {
                                CFReleaseNull(recovery_key);
                            }
                            CFReleaseNull(nullData);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCRegisterRecoveryPublicKey_Server(recovery_key, &error));
                            CFReleaseNull(recovery_key);
                        }
                    }
                }
                break;
            case kSecXPCOpGetRecoveryPublicKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        xpc_object_t xpc_recovery_object = NULL;
                        CFDataRef recovery = SOSCCCopyRecoveryPublicKey(&error);
                        if(recovery)
                            xpc_recovery_object = _CFXPCCreateXPCObjectFromCFObject(recovery);

                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_recovery_object);
                        CFReleaseNull(recovery);
                    }
                }
                break;
            case kSecXPCOpSetBagForAllSlices:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef backupSlice = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error); // NULL checked below
                        bool includeV0 = xpc_dictionary_get_bool(event, kSecXPCKeyIncludeV0); // false is ok, so it's safe for this paramter to be unset or incorrect type
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, backupSlice && SOSCCRegisterSingleRecoverySecret_Server(backupSlice, includeV0, &error));
                        CFReleaseSafe(backupSlice);
                    }
                }
                break;
            case kSecXPCOpCopyApplicantPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyApplicantPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyValidPeerPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyValidPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpValidateUserPublic:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    bool trusted = SOSCCValidateUserPublic_Server(&error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, trusted);
                }
                break;
            case kSecXPCOpCopyNotValidPeerPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyNotValidPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyGenerationPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_CFArray(replyMessage, kSecXPCKeyResult,
                                                           SOSCCCopyGenerationPeerInfo_Server(&error));
                }
                break;
            case kSecXPCOpCopyRetirementPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyRetirementPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyViewUnawarePeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyViewUnawarePeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyEngineState:
                {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFArrayRef array = SOSCCCopyEngineState_Server(&error);
                    CFDataRef derData = NULL;

                    require_quiet(array, done);
                    derData = CFPropertyListCreateDERData(kCFAllocatorDefault, array, &error);

                    require_quiet(derData, done);
                    xpc_dictionary_set_data(replyMessage, kSecXPCKeyResult, CFDataGetBytePtr(derData),CFDataGetLength(derData));
                    done:
                        CFReleaseNull(derData);
                        CFReleaseNull(array);
                    }
                }
                break;
            case kSecXPCOpCopyPeerPeerInfo:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                     SOSCCCopyPeerPeerInfo_Server(&error),
                                                                     &error);
                }
                break;
            case kSecXPCOpCopyConcurringPeerPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyConcurringPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyMyPeerInfo:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerInfo_Server(&error);
                    CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                    CFReleaseNull(peerInfo);
                    if (peerInfoData) {
                        xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                        xpc_release(xpc_object);
                    }
                    CFReleaseNull(peerInfoData);
                }
                break;
            case kSecXPCOpGetLastDepartureReason:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCGetLastDepartureReason_Server(&error));
                }
                break;
            case kSecXPCOpSetLastDepartureReason:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    // 0 is a legitimate reason (kSOSDepartureReasonError), so it's safe for this parameter to be unset or incorrect type
                    int32_t reason = (int32_t) xpc_dictionary_get_int64(event, kSecXPCKeyReason);
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCSetLastDepartureReason_Server(reason, &error));
                }
                break;
            case kSecXPCOpProcessSyncWithPeers:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    CFSetRef peers = SecXPCDictionaryCopySet(event, kSecXPCKeySet, &error);
                    CFSetRef backupPeers = SecXPCDictionaryCopySet(event, kSecXPCKeySet2, &error);
                    if (peers && backupPeers) {
                        CFSetRef result = SOSCCProcessSyncWithPeers_Server(peers, backupPeers, &error);
                        if (result) {
                            SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        }
                        CFReleaseNull(result);
                    }
                    CFReleaseNull(peers);
                    CFReleaseNull(backupPeers);
                }
                break;
            case kSecXPCOpProcessSyncWithAllPeers:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCProcessSyncWithAllPeers_Server(&error));
                }
                break;
            case soscc_EnsurePeerRegistration_id:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCProcessEnsurePeerRegistration_Server(&error));
                }
                break;
            case kSecXPCOpRollKeys:
                {
                    // false is valid, so it's safe for this parameter to be unset or incorrect type
                    bool force = xpc_dictionary_get_bool(event, "force");
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                 _SecServerRollKeys(force, &client, &error));
                }
                break;
            case kSecXPCOpWaitForInitialSync:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCWaitForInitialSync_Server(&error));
                }
                break;
            case kSecXPCOpPeersHaveViewsEnabled:
                {
                    CFArrayRef viewSet = SecXPCDictionaryCopyArray(event, kSecXPCKeyArray, &error);
                    if (viewSet) {
                        CFBooleanRef result = SOSCCPeersHaveViewsEnabled_Server(viewSet, &error);
                        if (result != NULL) {
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result != kCFBooleanFalse);
                        }
                    }
                    CFReleaseNull(viewSet);
                }
                break;

            case kSecXPCOpWhoAmI:
                {
                    if (client.musr)
                        xpc_dictionary_set_data(replyMessage, "musr", CFDataGetBytePtr(client.musr), CFDataGetLength(client.musr));
                    xpc_dictionary_set_bool(replyMessage, "system-keychain", client.allowSystemKeychain);
                    xpc_dictionary_set_bool(replyMessage, "syncbubble-keychain", client.allowSyncBubbleKeychain);
                    xpc_dictionary_set_bool(replyMessage, "network-extension", client.isNetworkExtension);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                }
                break;
            case kSecXPCOpTransmogrifyToSyncBubble:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainSyncBubble, &error)) {
#if TARGET_OS_IOS
                        uid_t uid = (uid_t)xpc_dictionary_get_int64(event, "uid");
                        CFArrayRef services = SecXPCDictionaryCopyArray(event, "services", &error);
                        bool res = false;
                        if (uid && services) {
                            res = _SecServerTransmogrifyToSyncBubble(services, uid, &client, &error);
                        }
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);
                        CFReleaseNull(services);
#else
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
#endif
                    }
                }
                break;
            case kSecXPCOpTransmogrifyToSystemKeychain:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainMigrateSystemKeychain, &error)) {
#if TARGET_OS_IOS
                        bool res = _SecServerTransmogrifyToSystemKeychain(&client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);
#else
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
#endif

                    }
                }
                break;
            case kSecXPCOpDeleteUserView:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainMigrateSystemKeychain, &error)) {
                        bool res = false;
#if TARGET_OS_IOS
                        uid_t uid = (uid_t)xpc_dictionary_get_int64(event, "uid");
                        if (uid) {
                            res = _SecServerDeleteMUSERViews(&client, uid, &error);
                        }
#endif
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);

                    }
                }
                break;
            case kSecXPCOpCopyApplication:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        SOSPeerInfoRef peerInfo = SOSCCCopyApplication_Server(&error);
                        CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                        CFReleaseNull(peerInfo);
                        if (peerInfoData) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(peerInfoData);
                    }
                break;
            case kSecXPCOpCopyCircleJoiningBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        CFDataRef appBlob = SecXPCDictionaryCopyCFDataRef(event, kSecXPCData, &error);
                        if (appBlob != NULL) {
                            SOSPeerInfoRef applicant = SOSPeerInfoCreateFromData(kCFAllocatorDefault, &error, appBlob);
                            if (applicant != NULL) {
                                CFDataRef pbblob = SOSCCCopyCircleJoiningBlob_Server(applicant, &error);
                                if (pbblob) {
                                    xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(pbblob);
                                    xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                                    xpc_release(xpc_object);
                                }
                                CFReleaseNull(pbblob);
                                CFReleaseNull(applicant);
                            }
                            CFReleaseNull(appBlob);
                        }
                    }
                break;
            case kSecXPCOpCopyInitialSyncBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        uint64_t flags = xpc_dictionary_get_uint64(event, kSecXPCKeyFlags); // 0 is a valid flags, so no error checking
                        CFDataRef initialblob = SOSCCCopyInitialSyncData_Server((uint32_t)flags, &error);
                        if (initialblob) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(initialblob);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(initialblob);
                    }
                    break;
            case kSecXPCOpJoinWithCircleJoiningBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        CFDataRef joiningBlob = SecXPCDictionaryCopyCFDataRef(event, kSecXPCData, &error); // NULL checked below
                        uint64_t version = xpc_dictionary_get_uint64(event, kSecXPCVersion); // 0 is valid, so this parameter can be unset or incorrect type
                        if (joiningBlob != NULL) {
                            bool retval = SOSCCJoinWithCircleJoiningBlob_Server(joiningBlob, (PiggyBackProtocolVersion) version, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                            CFReleaseNull(joiningBlob);
                        }
                    }
                    break;
            case kSecXPCOpKVSKeyCleanup:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        bool retval = SOSCCCleanupKVSKeys_Server(&error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                    }
                    break;
            case kSecXPCOpMessageFromPeerIsPending:
                {
                    SOSPeerInfoRef peer = SecXPCDictionaryCopyPeerInfo(event, kSecXPCKeyPeerInfo, &error);
                    if (peer) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCMessageFromPeerIsPending_Server(peer, &error));
                    }
                    CFReleaseNull(peer);
                    break;
                }
            case kSecXPCOpSendToPeerIsPending:
                {
                    SOSPeerInfoRef peer = SecXPCDictionaryCopyPeerInfo(event, kSecXPCKeyPeerInfo, &error);
                    if (peer) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSendToPeerIsPending(peer, &error));
                    }
                    CFReleaseNull(peer);
                    break;
                }
#endif /* !SECUREOBJECTSYNC */
            case sec_delete_items_with_access_groups_id:
                {
                    bool retval = false;
#if TARGET_OS_IPHONE
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateUninstallDeletion, &error)) {
                        CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);

                        if (accessGroups) {
                            retval = _SecItemServerDeleteAllWithAccessGroups(accessGroups, &client, &error);
                        }
                        CFReleaseNull(accessGroups);
                    }
#endif
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                }
                break;
            case kSecXPCOpBackupKeybagAdd: {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementBackupTableOperations, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
                }
                break;
            }
            case kSecXPCOpBackupKeybagDelete: {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementBackupTableOperations, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
                }
                break;
            }
            case kSecXPCOpKeychainControlEndpoint: {
                if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainControl, &error)) {
                    xpc_endpoint_t endpoint = SecServerCreateKeychainControlEndpoint();
                    if (endpoint) {
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyEndpoint, endpoint);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                        xpc_release(endpoint);
                    } else {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
                    }
                }
                break;
            }
            case sec_item_copy_parent_certificates_id:  {
                CFArrayRef results = NULL;
                if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateCertificateAllAccess, &error)) {
                    CFDataRef issuer = SecXPCDictionaryCopyData(event, kSecXPCKeyNormalizedIssuer, &error);
                    CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);
                    if (issuer && accessGroups) {
                        results = _SecItemCopyParentCertificates(issuer, accessGroups, &error);
                    }
                    CFReleaseNull(issuer);
                    CFReleaseNull(accessGroups);
                }
                SecXPCDictionarySetPListOptional(replyMessage, kSecXPCKeyResult, results, &error);
                CFReleaseNull(results);
                break;
            }
            case sec_item_certificate_exists_id: {
                bool result = false;
                if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateCertificateAllAccess, &error)) {
                    CFDataRef issuer = SecXPCDictionaryCopyData(event, kSecXPCKeyNormalizedIssuer, &error);
                    CFDataRef serialNum = SecXPCDictionaryCopyData(event, kSecXPCKeySerialNumber, &error);
                    CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);
                    if (issuer && serialNum && accessGroups) {
                        result = _SecItemCertificateExists(issuer, serialNum, accessGroups, &error);
                    }
                    CFReleaseNull(issuer);
                    CFReleaseNull(serialNum);
                    CFReleaseNull(accessGroups);
                }
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                break;
            }
            default:
                break;
            }

        if (error)
        {
            if(SecErrorGetOSStatus(error) == errSecItemNotFound || isSOSErrorCoded(error, kSOSErrorPublicKeyAbsent))
                secdebug("ipc", "%@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);
            else if (SecErrorGetOSStatus(error) == errSecAuthNeeded)
                secwarning("Authentication is needed %@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);
            else
                secerror("%@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);

            xpcError = SecCreateXPCObjectWithCFError(error);
            if (replyMessage) {
                xpc_dictionary_set_value(replyMessage, kSecXPCKeyError, xpcError);
            }
        } else if (replyMessage) {
            secdebug("ipc", "%@ %@ responding %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), replyMessage);
        }
    } else {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &error, 0, CFSTR("Messages expect to be xpc dictionary, got: %@"), event);
        secerror("%@: returning error: %@", client.task, error);
        xpcError = SecCreateXPCObjectWithCFError(error);
        replyMessage = xpc_create_reply_with_format(event, "{%string: %value}", kSecXPCKeyError, xpcError);
    }

    if (replyMessage) {
        xpc_connection_send_message(connection, replyMessage);
        xpc_release(replyMessage);
    }
    if (xpcError) {
        xpc_release(xpcError);
    }
    CFReleaseSafe(error);
    CFReleaseSafe(client.accessGroups);
    CFReleaseSafe(client.musr);
    CFReleaseSafe(client.task);
    CFReleaseNull(client.applicationIdentifier);
    CFReleaseSafe(domains);
    CFReleaseSafe(clientAuditToken);
}

static void securityd_xpc_init(const char *service_name)
{
    secdebug("serverxpc", "start");
    xpc_connection_t listener = xpc_connection_create_mach_service(service_name, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        seccritical("security failed to register xpc listener for %s, exiting", service_name);
        abort();
    }

    xpc_connection_set_event_handler(listener, ^(xpc_object_t connection) {
        if (xpc_get_type(connection) == XPC_TYPE_CONNECTION) {
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
                    // Synchronous. The client has a connection pool so they can be somewhat re-entrant if they need.
                    securityd_xpc_dictionary_handler(connection, event);
                }
            });
            xpc_connection_resume(connection);
        }
    });
    xpc_connection_resume(listener);

#if OCTAGON
    xpc_activity_register("com.apple.securityd.daily", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        xpc_activity_state_t activityState = xpc_activity_get_state(activity);
        if (activityState == XPC_ACTIVITY_STATE_RUN) {
            SecCKKS24hrNotification();
            SecOctagon24hrNotification();
        }
    });
#endif

    xpc_activity_register("com.apple.securityd.entropyhealth", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        xpc_activity_state_t activityState = xpc_activity_get_state(activity);
        if (activityState == XPC_ACTIVITY_STATE_RUN) {
            SecCoreAnalyticsSendKernEntropyHealth();
        }
    });

    xpc_activity_register("com.apple.securityd.prng", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        xpc_activity_state_t state = xpc_activity_get_state(activity);
        if (state == XPC_ACTIVITY_STATE_RUN) {
            refresh_prng();
        }
    });

#if OCTAGON && !TARGET_OS_BRIDGE
    // Kick off reporting tasks.
    if (os_variant_has_internal_diagnostics("com.apple.security") && !os_variant_is_recovery("securityd")) {
        InitPolicyReporter();
    }
#endif
}


// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
static void securityd_soscc_lock_hack() {
	dispatch_queue_t		soscc_lock_queue = dispatch_queue_create("soscc_lock_queue", DISPATCH_QUEUE_PRIORITY_DEFAULT);
	int 					soscc_tok;

	// <rdar://problem/22500239> Prevent securityd from quitting while holding a keychain assertion
	// FIXME: securityd isn't currently registering for any other notifyd events. If/when it does,
	//        this code will need to be generalized / migrated away from just this specific purpose.
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(xpc_object_t object) {
		char *event_description = xpc_copy_description(object);
		secnotice("events", "%s", event_description);
		free(event_description);
	});

    secnotice("lockassertion", "notify_register_dispatch(kSOSCCHoldLockForInitialSync)");
    notify_register_dispatch(kSOSCCHoldLockForInitialSync, &soscc_tok, soscc_lock_queue, ^(int token __unused) {
        secnotice("lockassertion", "kSOSCCHoldLockForInitialSync: grabbing the lock");
        CFErrorRef error = NULL;

        uint64_t one_minute = 60ull;
        if(SecAKSUserKeybagHoldLockAssertion(one_minute, &error)){
            // <rdar://problem/22500239> Prevent securityd from quitting while holding a keychain assertion
            os_transaction_t transaction = os_transaction_create("securityd-LockAssertedingHolder");

            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, one_minute*NSEC_PER_SEC), soscc_lock_queue, ^{
                CFErrorRef localError = NULL;
                if(!SecAKSUserKeybagDropLockAssertion(&localError))
                    secerror("failed to unlock: %@", localError);
                CFReleaseNull(localError);
                os_release(transaction);
            });
		} else {
			secerror("Failed to take device lock assertion: %@", error);
		}
		CFReleaseNull(error);
		secnotice("lockassertion", "kSOSCCHoldLockForInitialSync => done");
	});
}
#endif

#if TARGET_OS_OSX

static char *
homedirPath(void)
{
    static char homeDir[PATH_MAX] = {};

    if (homeDir[0] == '\0') {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd == NULL)
            return NULL;

        if (realpath(pwd->pw_dir, homeDir) == NULL) {
            strlcpy(homeDir, pwd->pw_dir, sizeof(homeDir));
        }
    }
    return homeDir;
}
#endif


int main(int argc, char *argv[])
{
    DisableLocalization();

    char *wait4debugger = getenv("WAIT4DEBUGGER");
    if (wait4debugger && !strcasecmp("YES", wait4debugger)) {
		seccritical("SIGSTOPing self, awaiting debugger");
		kill(getpid(), SIGSTOP);
		seccritical("Again, for good luck (or bad debuggers)");
		kill(getpid(), SIGSTOP);
	}

/* <rdar://problem/15792007> Users with network home folders are unable to use/save password for Mail/Cal/Contacts/websites
 Secd doesn't realize DB connections get invalidated when network home directory users logout
 and their home gets unmounted. Exit secd, start fresh when user logs back in.
*/
#if TARGET_OS_OSX
    int sessionstatechanged_tok;
    notify_register_dispatch(kSA_SessionStateChangedNotification, &sessionstatechanged_tok, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(int token __unused) {
        // we could be a process running as root.
        // However, since root never logs out this isn't an issue.
        if (SASSessionStateForUser(getuid()) == kSA_state_loggingout_pointofnoreturn) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3ull*NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                xpc_transaction_exit_clean();
            });
        }
    });
#endif

    signal(SIGTERM, SIG_IGN);
    static dispatch_source_t termSource;
    termSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
    dispatch_source_set_event_handler(termSource, ^{
        secnotice("signal", "SIGTERM, exiting when clean ✌️");
        xpc_transaction_exit_clean();
    });
    dispatch_activate(termSource);

#if TARGET_OS_OSX
#define SECD_PROFILE_NAME "com.apple.secd"
    const char *homedir = homedirPath();
    if (homedir == NULL) {
        errx(1, "failed to get home directory for secd");
    }

    char *errorbuf = NULL;
    const char	*sandbox_params[] = {"_HOME", homedir, NULL};
    int32_t rc = sandbox_init_with_parameters(SECD_PROFILE_NAME, SANDBOX_NAMED, sandbox_params, &errorbuf);
    if (rc) {
        errx(1, "Failed to instantiate sandbox: %d %s", rc, errorbuf);
        /* errx will quit the process */
    }
#endif /* TARGET_OS_OSX */

    const char *serviceName = kSecuritydXPCServiceName;

// Mark our interest in running some features (before we bring the DB layer up)
#if OCTAGON
    EscrowRequestServerSetEnabled(true);
    OctagonSetShouldPerformInitialization(true);
    SecCKKSEnable();
#endif

    /* setup SQDLite before some other component have a chance to create a database connection */
    _SecDbServerSetup();

    securityd_init_server();
    securityd_xpc_init(serviceName);
    SecCreateSecuritydXPCServer();

#if SECUREOBJECTSYNC
    SOSControlServerInitialize();
#endif

#if OCTAGON
    CKKSControlServerInitialize();
    OctagonControlServerInitialize();
    EscrowRequestXPCServerInitialize();
#endif

	// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
	securityd_soscc_lock_hack();
#endif

    CFRunLoopRun();
}

/* vi:set ts=4 sw=4 et: */
