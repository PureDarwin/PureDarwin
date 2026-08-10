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


/*
 * SecItemBackup.c -  Client side backup interfaces and support code
 */

#include <Security/SecItemBackup.h>

#include <Security/SecItemPriv.h>
#include <Security/SecuritydXPC.h>
#include <Security/SecFramework.h>
#include "keychain/securityd/SecItemServer.h"
#include <ipc/securityd_client.h>
#include "keychain/SecureObjectSync/SOSBackupEvent.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <corecrypto/ccsha1.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFCCWrappers.h>
#include <utilities/array_size.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <AssertMacros.h>
#include <os/activity.h>
#include <notify.h>

#include <sys/stat.h>

static CFDataRef client_data_data_bool_to_data_error_request(enum SecXPCOperation op, SecurityClient *client, CFDataRef keybag, CFDataRef passcode, bool emcs, CFErrorRef *error) {
    __block CFDataRef result = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetDataOptional(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error)
        && SecXPCDictionarySetBool(message, kSecXPCKeyEMCSBackup, emcs, NULL);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return (result = SecXPCDictionaryCopyData(response, kSecXPCKeyResult, error));
    });
    return result;
}

static bool data_client_data_data_to_error_request(enum SecXPCOperation op, CFDataRef backup, SecurityClient *client, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetData(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    } , NULL);
}

static bool dict_data_data_to_error_request(enum SecXPCOperation op, CFDictionaryRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    } , NULL);
}

static CFDictionaryRef data_data_dict_to_dict_error_request(enum SecXPCOperation op, CFDictionaryRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    __block CFDictionaryRef dict = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPListOptional(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return (dict = SecXPCDictionaryCopyDictionary(response, kSecXPCKeyResult, error));
    });
    return dict;
}

static int string_to_fd_error_request(enum SecXPCOperation op, CFStringRef backupName, CFErrorRef *error) {
    __block int fd = -1;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyBackup, backupName, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        fd = SecXPCDictionaryDupFileDescriptor(response, kSecXPCKeyResult, error);
        return true;
    });
    return fd;
}

static bool string_data_data_to_bool_error_request(enum SecXPCOperation op, CFStringRef backupName, CFDataRef keybagDigest, CFDataRef manifest, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyBackup, backupName, error) &&
                SecXPCDictionarySetDataOptional(message, kSecXPCKeyKeybag, keybagDigest, error) &&
                SecXPCDictionarySetDataOptional(message, kSecXPCData, manifest, error);
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        return xpc_dictionary_get_bool(response, kSecXPCKeyResult);
    });
}

static bool string_string_data_data_data_to_bool_error_request(enum SecXPCOperation op, CFStringRef backupName, CFStringRef peerID, CFDataRef keybag, CFDataRef secret, CFDataRef backup, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyBackup, backupName, error) &&
        SecXPCDictionarySetStringOptional(message, kSecXPCKeyDigest, peerID, error) &&
        SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error) &&
        SecXPCDictionarySetData(message, kSecXPCKeyUserPassword, secret, error) &&
        SecXPCDictionarySetData(message, kSecXPCData, backup, error);
    }, ^bool(xpc_object_t response, __unused CFErrorRef *error) {
        return xpc_dictionary_get_bool(response, kSecXPCKeyResult);
    });
}

static CFStringRef string_to_string_error_request(enum SecXPCOperation op, CFStringRef viewName, CFErrorRef *error)
{
    __block CFStringRef result = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyString, viewName, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return result = SecXPCDictionaryCopyString(response, kSecXPCKeyResult, error);
    });
    return result;
}

static CFArrayRef to_array_error_request(enum SecXPCOperation op, CFErrorRef *error)
{
    __block CFArrayRef result = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return result = SecXPCDictionaryCopyArray(response, kSecXPCKeyResult, error);
    });
    return result;
}

// XPC calls

static int SecItemBackupHandoffFD(CFStringRef backupName, CFErrorRef *error) {
    __block int fileDesc = -1;
    os_activity_initiate("SecItemBackupHandoffFD", OS_ACTIVITY_FLAG_DEFAULT, ^{
        fileDesc = SECURITYD_XPC(sec_item_backup_handoff_fd, string_to_fd_error_request, backupName, error);
    });
    return fileDesc;
}

CFDataRef _SecKeychainCopyOTABackup(void) {
    __block CFDataRef result;
    os_activity_initiate("_SecKeychainCopyOTABackup", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_keychain_backup, client_data_data_bool_to_data_error_request, SecSecurityClientGet(), NULL, NULL, false, NULL);
    });
    return result;
}

CFDataRef _SecKeychainCopyBackup(CFDataRef backupKeybag, CFDataRef password) {
    __block CFDataRef result;
    os_activity_initiate("_SecKeychainCopyBackup", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_keychain_backup, client_data_data_bool_to_data_error_request, SecSecurityClientGet(), backupKeybag, password, false, NULL);
    });
    return result;
}

CFDataRef _SecKeychainCopyEMCSBackup(CFDataRef backupKeybag) {
    __block CFDataRef result;
    os_activity_initiate("_SecKeychainCopyEMCSBackup", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_keychain_backup, client_data_data_bool_to_data_error_request, SecSecurityClientGet(), backupKeybag, NULL, true, NULL);
    });
    return result;
}

bool _SecKeychainWriteBackupToFileDescriptor(CFDataRef backupKeybag, CFDataRef password, int fd, CFErrorRef *error) {
    __block bool result = false;
    os_activity_initiate("_SecKeychainWriteBackupToFile", OS_ACTIVITY_FLAG_DEFAULT, ^{

        securityd_send_sync_and_do(sec_keychain_backup_id, error, ^bool(xpc_object_t message, CFErrorRef *error) {
            return SecXPCDictionarySetDataOptional(message, kSecXPCKeyKeybag, backupKeybag, error)
                && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, password, error)
                && SecXPCDictionarySetFileDescriptor(message, kSecXPCKeyFileDescriptor, fd, error);
        }, ^bool(xpc_object_t response, CFErrorRef *error) {
            return (result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, error));
        });
    });
    return result;
}

bool
_SecKeychainRestoreBackupFromFileDescriptor(int fd, CFDataRef backupKeybag, CFDataRef password, CFErrorRef *error)
{
    __block bool result;
    os_activity_initiate("_SecKeychainRestoreBackup", OS_ACTIVITY_FLAG_DEFAULT, ^{
        securityd_send_sync_and_do(sec_keychain_restore_id, error, ^bool(xpc_object_t message, CFErrorRef *error) {
            return SecXPCDictionarySetFileDescriptor(message, kSecXPCKeyFileDescriptor, fd, error)
                && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, backupKeybag, error)
                && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, password, error);
        }, ^bool(xpc_object_t response, CFErrorRef *error) {
            return (result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, error));
        });
    });
    return result;
}

/*
 * Current promise is that this is low memory usage, so in the current format, ask securityd
 * to resolve the item for us.
 */

CFStringRef
_SecKeychainCopyKeybagUUIDFromFileDescriptor(int fd, CFErrorRef *error)
{
    __block CFStringRef result = NULL;
    os_activity_initiate("_SecKeychainCopyKeybagUUID", OS_ACTIVITY_FLAG_DEFAULT, ^{
        securityd_send_sync_and_do(sec_keychain_backup_keybag_uuid_id, error, ^bool(xpc_object_t message, CFErrorRef *error) {
            return SecXPCDictionarySetFileDescriptor(message, kSecXPCKeyFileDescriptor, fd, error);
        }, ^bool(xpc_object_t response, CFErrorRef *error) {
            return (result = SecXPCDictionaryCopyString(response, kSecXPCKeyResult, error));
        });
    });
    return result;
}


OSStatus _SecKeychainRestoreBackup(CFDataRef backup, CFDataRef backupKeybag,
                                   CFDataRef password) {
    __block OSStatus result;
    os_activity_initiate("_SecKeychainRestoreBackup", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SecOSStatusWith(^bool (CFErrorRef *error) {
            return SECURITYD_XPC(sec_keychain_restore, data_client_data_data_to_error_request, backup, SecSecurityClientGet(), backupKeybag, password, error);
        });
    });
    return result;
}


static int compareDigests(const void *l, const void *r) {
    return memcmp(l, r, CCSHA1_OUTPUT_SIZE);
}

CFDataRef SecItemBackupCreateManifest(CFDictionaryRef backup, CFErrorRef *error)
{
    CFIndex count = backup ? CFDictionaryGetCount(backup) : 0;
    CFMutableDataRef manifest = CFDataCreateMutable(kCFAllocatorDefault, CCSHA1_OUTPUT_SIZE * count);
    if (backup) {
        CFDictionaryForEach(backup, ^void (const void *key, const void *value) {
            if (isDictionary(value)) {
                /* converting key back to binary blob is horrible */
                CFDataRef sha1 = CFDictionaryGetValue(value, kSecItemBackupHashKey);
                if (isData(sha1) && CFDataGetLength(sha1) == CCSHA1_OUTPUT_SIZE) {
                    CFDataAppend(manifest, sha1);
                } else {
                    CFStringRef sha1Hex = CFDataCopyHexString(sha1);
                    secerror("bad hash %@ in backup", sha1Hex);
                    CFReleaseSafe(sha1Hex);
                    // TODO: Drop this key from dictionary (outside the loop)
                }
            }
        });
        qsort(CFDataGetMutableBytePtr(manifest), CFDataGetLength(manifest) / CCSHA1_OUTPUT_SIZE, CCSHA1_OUTPUT_SIZE, compareDigests);
    }
    return manifest;
}

OSStatus _SecKeychainBackupSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFDictionaryRef *backup_out)
{
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        *backup_out = SECURITYD_XPC(sec_keychain_backup_syncable, data_data_dict_to_dict_error_request, backup_in, keybag, password, error);
        return *backup_out != NULL;
    });
}

OSStatus _SecKeychainRestoreSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in)
{
    __block OSStatus result;
    os_activity_initiate("_SecKeychainRestoreSyncable", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SecOSStatusWith(^bool (CFErrorRef *error) {
            return SECURITYD_XPC(sec_keychain_restore_syncable, dict_data_data_to_error_request, backup_in, keybag, password, error);
        });
    });
    return result;
}

// Client code

static bool SecKeychainWithBackupFile(CFStringRef backupName, CFErrorRef *error, void(^with)(FILE *bufile)) {
    int fd = SecItemBackupHandoffFD(backupName, error);
    if (fd < 0) {
        secdebug("backup", "SecItemBackupHandoffFD returned %d", fd);
        return false;
    }

    // Rewind file to start
    if (lseek(fd, 0, SEEK_SET)) {
        secdebug("backup", "Could not seek in fd %d for %@", fd, backupName);
        return SecCheckErrno(true, error, CFSTR("lseek"));
    }

    FILE *backup = fdopen(fd, "r");
    if (!backup) {
        secdebug("backup", "Receiving file for %@ failed, %d", backupName, errno);
        SecCheckErrno(!backup, error, CFSTR("fdopen"));
        if (close(fd)) {
            secdebug("backup", "Encountered error closing file %@: %d", backupName, errno);
            SecCheckErrno(true, error, CFSTR("close"));
        }
        return false;
    } else {
        struct stat sb;
        if (fstat(fd, &sb)) {
            secdebug("backup", "Unable to get file metadata for %@, fd %d", backupName, fd);
            SecCheckErrno(true, error, CFSTR("fstat"));
            if (fclose(backup)) {
                secdebug("backup", "Encountered error closing file %@: %d", backupName, errno);
                SecCheckErrno(true, error, CFSTR("fclose"));
            }
            return false;
        }
        secdebug("backup", "Receiving file for %@ with fd %d of size %llu", backupName, fd, sb.st_size);
    }

    with(backup);
    if (fclose(backup)) {
        secdebug("backup", "Encountered error %d closing file %@ after backup handler", errno, backupName);
        SecCheckErrno(true, error, CFSTR("fclose"));
        // read only file and block has its own error handling for IO failure, no need to return false
    }
    return true;
}

static CFArrayRef SecItemBackupCopyNames(CFErrorRef *error)
{
    __block CFArrayRef result;
    os_activity_initiate("SecItemBackupCopyNames", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_item_backup_copy_names, to_array_error_request, error);
    });
    return result;
}

bool SecItemBackupWithRegisteredBackups(CFErrorRef *error, void(^backup)(CFStringRef backupName)) {
    CFArrayRef backupNames = SecItemBackupCopyNames(error);
    if (!backupNames) return false;
    CFStringRef name;
    CFArrayForEachC(backupNames, name) {
        backup(name);
    }
    CFRelease(backupNames);
    return true;
}

static CFStringRef SecItemBackupViewAndCopyBackupPeerID(CFStringRef viewName, CFErrorRef *error)
{
    __block CFStringRef result;
    os_activity_initiate("SecItemBackupViewAndCopyBackupPeerID", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_item_backup_ensure_copy_view, string_to_string_error_request, viewName, error);
    });
    return result;
}

bool SecItemBackupWithRegisteredViewBackup(CFStringRef viewName, CFErrorRef *error) {
    CFStringRef backupName = SecItemBackupViewAndCopyBackupPeerID(viewName, error);
    if(backupName == NULL) {
        return false;
    }
    CFReleaseNull(backupName);
    return true;
}


static bool SecItemBackupDoResetEventBody(const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    size_t sequence_len;
    const uint8_t *sequence_body = ccder_decode_len(&sequence_len, der, der_end);
    bool ok = sequence_body;
    if (ok && sequence_body + sequence_len != der_end) {
        // Can't ever happen!
        SecError(errSecDecode, error, CFSTR("trailing junk after reset"));
        ok = false;
    }
    if (ok) {
        CFDataRef keybag = NULL;
        if (sequence_body != der_end) {
            size_t keybag_len = 0;
            const uint8_t *keybag_start = ccder_decode_tl(CCDER_OCTET_STRING, &keybag_len, sequence_body, der_end);
            if (!keybag_start) {
                ok = SecError(errSecDecode, error, CFSTR("failed to decode keybag"));
            } else if (keybag_start + keybag_len != der_end) {
                ok = SecError(errSecDecode, error, CFSTR("trailing junk after keybag"));
            } else {
                keybag = CFDataCreate(kCFAllocatorDefault, keybag_start, keybag_len);
            }
        }
        handleEvent(kSecBackupEventReset, keybag, NULL);
        CFReleaseSafe(keybag);
    }

    return ok;
}

static bool SecItemBackupDoAddEvent(const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    CFDictionaryRef eventDict = NULL;
    const uint8_t *der_end_of_dict = der_decode_dictionary(kCFAllocatorDefault, &eventDict, error, der, der_end);
    if (der_end_of_dict && der_end_of_dict != der_end) {
        // Can't ever happen!
        SecError(errSecDecode, error, CFSTR("trailing junk after add"));
        der_end_of_dict = NULL;
    }
    if (der_end_of_dict) {
        CFDataRef hash = CFDictionaryGetValue(eventDict, kSecItemBackupHashKey);
        handleEvent(kSecBackupEventAdd, hash, eventDict);
    }

    CFReleaseSafe(eventDict);
    return der_end_of_dict;
}

static bool SecItemBackupDoCompleteEvent(const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    uint64_t event_num = 0;
    const uint8_t *der_end_of_num = ccder_decode_uint64(&event_num, der, der_end);
    if (der_end_of_num && der_end_of_num != der_end) {
        // Can't ever happen!
        SecError(errSecDecode, error, CFSTR("trailing junk after complete"));
        der_end_of_num = NULL;
    }
    if (der_end_of_num) {
        handleEvent(kSecBackupEventComplete, NULL, NULL);
    }
    return der_end_of_num;
}

static bool SecItemBackupDoDeleteEventBody(const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    size_t digest_len = 0;
    const uint8_t *digest_start = ccder_decode_len(&digest_len, der, der_end);
    if (digest_start && digest_start + digest_len != der_end) {
        // Can't ever happen!
        SecError(errSecDecode, error, CFSTR("trailing junk after delete"));
        digest_start = NULL;
    }
    if (digest_start) {
        CFDataRef hash = CFDataCreate(kCFAllocatorDefault, digest_start, digest_len);
        handleEvent(kSecBackupEventRemove, hash, NULL);
        CFRelease(hash);
    }

    return digest_start;
}

static void debugDisplayBackupEventTag(ccder_tag tag) {
#if !defined(NDEBUG)
    const char *eventDesc;
    switch (tag) {
        case CCDER_CONSTRUCTED_SEQUENCE:    eventDesc = "ResetEvent"; break;
        case CCDER_CONSTRUCTED_SET:         eventDesc = "AddEvent"; break;
        case CCDER_INTEGER:                 eventDesc = "ResetEvent"; break;
        case CCDER_OCTET_STRING:            eventDesc = "DeleteEvent"; break;
        default:                            eventDesc = "UnknownEvent"; break;
    }
    secdebug("backup", "processing event %s (tag %08lX)", eventDesc, tag);
#endif
}

static bool SecItemBackupDoEvent(const uint8_t *der, const uint8_t *der_end, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    ccder_tag tag;
    const uint8_t *der_start_of_len = ccder_decode_tag(&tag, der, der_end);
    debugDisplayBackupEventTag(tag);
    switch (tag) {
        case CCDER_CONSTRUCTED_SEQUENCE:
            return SecItemBackupDoResetEventBody(der_start_of_len, der_end, error, handleEvent);
        case CCDER_CONSTRUCTED_SET:
            return SecItemBackupDoAddEvent(der, der_end, error, handleEvent);
        case CCDER_INTEGER:
            return SecItemBackupDoCompleteEvent(der, der_end, error, handleEvent);
        case CCDER_OCTET_STRING:
            return SecItemBackupDoDeleteEventBody(der_start_of_len, der_end, error, handleEvent);
        default:
            return SecError(errSecDecode, error, CFSTR("unsupported event tag: %lu"), tag);
    }
}

// TODO: Move to ccder and give better name.
static const uint8_t *ccder_decode_len_unchecked(size_t *lenp, const uint8_t *der, const uint8_t *der_end) {
    if (der && der < der_end) {
        size_t len = *der++;
        if (len < 0x80) {
        } else if (len == 0x81) {
            if (der_end - der < 1) goto errOut;
            len = *der++;
        } else if (len == 0x82) {
            if (der_end - der < 2) goto errOut;
            len = *(der++) << 8;
            len += *der++;
        } else if (len == 0x83) {
            if (der_end - der < 3) goto errOut;
            len = *(der++) << 16;
            len += *(der++) << 8;
            len += *(der++);
        } else {
            goto errOut;
        }
        *lenp = len;
        return der;
    }
errOut:
    return NULL;
}

static bool SecKeychainWithBackupFileParse(FILE *backup, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    __block bool ok = true;
    size_t buf_remaining = 0;
    const size_t read_ahead = 16;
    size_t buf_len = read_ahead;
    uint8_t *buf = malloc(buf_len);
    for (;;) {
        const size_t bytes_read = fread(buf + buf_remaining, 1, read_ahead - buf_remaining, backup);
        if (bytes_read <= 0) {
            if (!feof(backup))
                ok = SecCheckErrno(true, error, CFSTR("read backup event header"));
            else if (!buf_remaining) {
                // Nothing read, nothing in buffer, clean eof.
            }
            break;
        }
        const size_t buf_avail = bytes_read + buf_remaining;

        const uint8_t *der = buf;
        const uint8_t *der_end = der + buf_avail;
        ccder_tag tag;
        size_t body_len;
        der = ccder_decode_tag(&tag, der, der_end);
        der = ccder_decode_len_unchecked(&body_len, der, der_end);

        if (!der) {
            ok = SecError(errSecDecode, error, CFSTR("failed to decode backup event header"));
            break;
        }

        const size_t decoded_len = der - buf;
        size_t event_len = decoded_len + body_len;
        if (event_len > buf_avail) {
            // We need to read the rest of this event, first
            // ensure we have enough space.
            if (buf_len < event_len) {
                // TODO: Think about a max for buf_len here to prevent attacks.
                uint8_t *new_buf = realloc(buf, event_len);
                if (!new_buf) {
                    ok = SecError(errSecAllocate, error, CFSTR("realloc buf failed"));
                    break;
                }
                buf = new_buf;
                buf_len = event_len;
            }

            // Read tail of current event.
            const size_t tail_len = fread(buf + buf_avail, 1, event_len - buf_avail, backup);
            if (tail_len < event_len - buf_avail) {
                if (!feof(backup)) {
                    ok = SecCheckErrno(true, error, CFSTR("failed to read event body"));
                } else {
                    ok = SecError(errSecDecode, error, CFSTR("unexpected end of event file %zu of %zu bytes read"), tail_len, event_len - buf_avail);
                }
                break;
            }
        }

        // Adjust der_end to the end of the event.
        der_end = buf + event_len;

        ok &= SecItemBackupDoEvent(buf, der_end, error, handleEvent);

        if (event_len < buf_avail) {
            // Shift remaining bytes to start of buffer.
            buf_remaining = buf_avail - event_len;
            memmove(buf, der_end, buf_remaining);
        } else {
            buf_remaining = 0;
        }
    }
    free(buf);
    return ok;
}

bool SecItemBackupWithChanges(CFStringRef backupName, CFErrorRef *error, void (^handleEvent)(SecBackupEventType et, CFTypeRef key, CFTypeRef item)) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    ok &= SecKeychainWithBackupFile(backupName, &localError, ^(FILE *backup) {
        ok &= SecKeychainWithBackupFileParse(backup, &localError, handleEvent);
    });
    if (!ok) {   // TODO: remove this logging
        secdebug("backup", "SecItemBackupWithChanges failed: %@", localError);
        handleEvent(kSecBackupEventComplete, NULL, NULL);
        CFErrorPropagate(localError, error);
    }

    return ok;
}

bool SecItemBackupSetConfirmedManifest(CFStringRef backupName, CFDataRef keybagDigest, CFDataRef manifest, CFErrorRef *error) {
    __block bool result;
    os_activity_initiate("SecItemBackupSetConfirmedManifest", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SECURITYD_XPC(sec_item_backup_set_confirmed_manifest, string_data_data_to_bool_error_request, backupName, keybagDigest, manifest, error);
    });
    return result;
}

void SecItemBackupRestore(CFStringRef backupName, CFStringRef peerID, CFDataRef keybag, CFDataRef secret, CFTypeRef backup, void (^completion)(CFErrorRef error)) {
    __block CFErrorRef localError = NULL;
    os_activity_initiate("SecItemBackupRestore", OS_ACTIVITY_FLAG_DEFAULT, ^{
        SECURITYD_XPC(sec_item_backup_restore, string_string_data_data_data_to_bool_error_request, backupName, peerID, keybag, secret, backup, &localError);
    });
    completion(localError);
    CFReleaseSafe(localError);
}

bool SecBackupKeybagAdd(CFDataRef passcode, CFDataRef *identifier, CFURLRef *pathinfo, CFErrorRef *error) {
    __block bool result = false;
    os_activity_initiate("_SecServerBackupKeybagAdd", OS_ACTIVITY_FLAG_DEFAULT, ^{
        securityd_send_sync_and_do(kSecXPCOpBackupKeybagAdd, error, ^bool(xpc_object_t message, CFErrorRef *error) {
            return SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
        }, ^bool(xpc_object_t response, CFErrorRef *error) {
            result = SecXPCDictionaryCopyDataOptional(response, kSecXPCKeyBackupKeybagIdentifier, identifier, error) &&
            SecXPCDictionaryCopyURLOptional(response, kSecXPCKeyBackupKeybagPath, pathinfo, error) &&
            SecXPCDictionaryGetBool(response, kSecXPCKeyResult, error);
            return result;
        });
    });
    return result;
}

bool SecBackupKeybagDelete(CFDictionaryRef query, CFErrorRef *error) {
    __block bool result = false;
    os_activity_initiate("_SecBackupKeybagDelete", OS_ACTIVITY_FLAG_DEFAULT, ^{
        securityd_send_sync_and_do(kSecXPCOpBackupKeybagDelete, error, ^bool(xpc_object_t message, CFErrorRef *error) {
            return SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error);
        }, ^bool(xpc_object_t response, CFErrorRef *error) {
            result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, error);
            return result;
        });
    });
    return result;
}


