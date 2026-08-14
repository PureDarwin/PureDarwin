/*
 * Copyright (c) 2017-2020 Apple Inc.  All Rights Reserved.
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

#include <AssertMacros.h>
#include <sandbox.h>
#include <dirhelper_priv.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <notify.h>
#include <xpc/private.h>
#include <xpc/xpc.h>
#include <CoreFoundation/CFStream.h>
#include <os/assumes.h>

#include <Security/SecuritydXPC.h>
#include <Security/SecTrustStore.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecEntitlements.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#include <ipc/securityd_client.h>
#include <ipc/server_entitlement_helpers.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>
#include <utilities/debugging.h>
#include <utilities/SecXPCError.h>
#include "trust/trustd/SecOCSPCache.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "trust/trustd/SecPinningDb.h"
#include "trust/trustd/SecPolicyServer.h"
#include "trust/trustd/SecRevocationDb.h"
#include "trust/trustd/SecTrustServer.h"
#include "keychain/securityd/spi.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/SecTrustExceptionResetCount.h"
#include "trust/trustd/trustdFileLocations.h"

#if TARGET_OS_OSX
#include <Security/SecTaskPriv.h>
#include <login/SessionAgentStatusCom.h>
#include "trust/trustd/macOS/SecTrustOSXEntryPoints.h"
#endif

#include "OTATrustUtilities.h"
#include "trustd_spi.h"

static bool SecXPCDictionarySetChainOptional(xpc_object_t message, const char *key, CFArrayRef path, CFErrorRef *error) {
    if (!path)
        return true;
    __block xpc_object_t xpc_chain = NULL;
    require_action_quiet(xpc_chain = xpc_array_create(NULL, 0), exit, SecError(errSecParam, error, CFSTR("xpc_array_create failed")));
    CFArrayForEach(path, ^(const void *value) {
        SecCertificateRef cert = (SecCertificateRef)value;
        if (xpc_chain && !SecCertificateAppendToXPCArray(cert, xpc_chain, error)) {
            xpc_release(xpc_chain);
            xpc_chain = NULL;
        }
    });

exit:
    if (!xpc_chain)
        return false;

    xpc_dictionary_set_value(message, key, xpc_chain);
    xpc_release(xpc_chain);
    return true;
}

static SecCertificateRef SecXPCDictionaryCopyCertificate(xpc_object_t message, const char *key, CFErrorRef *error) {
    size_t length = 0;
    const void *bytes = xpc_dictionary_get_data(message, key, &length);
    if (bytes) {
        SecCertificateRef certificate = SecCertificateCreateWithBytes(kCFAllocatorDefault, bytes, length);
        if (certificate)
            return certificate;
        SecError(errSecDecode, error, CFSTR("object for key %s failed to create certificate from data"), key);
    } else {
        SecError(errSecParam, error, CFSTR("object for key %s missing"), key);
    }
    return NULL;
}

static bool SecXPCDictionaryCopyCertificates(xpc_object_t message, const char *key, CFArrayRef *certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates = xpc_dictionary_get_value(message, key);
    if (!xpc_certificates)
        return SecError(errSecAllocate, error, CFSTR("no certs for key %s"), key);
    *certificates = SecCertificateXPCArrayCopyArray(xpc_certificates, error);
    return *certificates;
}

static bool SecXPCDictionaryCopyCertificatesOptional(xpc_object_t message, const char *key, CFArrayRef *certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates = xpc_dictionary_get_value(message, key);
    if (!xpc_certificates) {
        *certificates = NULL;
        return true;
    }
    *certificates = SecCertificateXPCArrayCopyArray(xpc_certificates, error);
    return *certificates;
}

static bool SecXPCDictionaryCopyPoliciesOptional(xpc_object_t message, const char *key, CFArrayRef *policies, CFErrorRef *error) {
    xpc_object_t xpc_policies = xpc_dictionary_get_value(message, key);
    if (!xpc_policies) {
        if (policies)
            *policies = NULL;
        return true;
    }
    *policies = SecPolicyXPCArrayCopyArray(xpc_policies, error);
    return *policies != NULL;
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

static SecTrustStoreRef SecXPCDictionaryGetTrustStore(xpc_object_t message, const char *key, CFErrorRef *error) {
    SecTrustStoreRef ts = NULL;
    CFStringRef domain = SecXPCDictionaryCopyString(message, key, error);
    if (domain) {
        ts = SecTrustStoreForDomainName(domain, error);
        CFRelease(domain);
    }
    return ts;
}

static bool SecXPCTrustStoreContains(SecurityClient * __unused client, xpc_object_t event,
                                     xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, error);
    if (ts) {
        SecCertificateRef cert = SecXPCDictionaryCopyCertificate(event, kSecXPCKeyCertificate, error);
        if (cert) {
            bool contains;
            if (_SecTrustStoreContainsCertificate(ts, cert, &contains, error)) {
                xpc_dictionary_set_bool(reply, kSecXPCKeyResult, contains);
                result = true;
            }
            CFReleaseNull(cert);
        }
    }
    return result;
}

static bool SecXPCTrustStoreSetTrustSettings(SecurityClient * __unused client, xpc_object_t event,
                                             xpc_object_t reply, CFErrorRef *error) {
    bool noError = false;
    SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, error);
    if (ts) {
        SecCertificateRef certificate = SecXPCDictionaryCopyCertificate(event, kSecXPCKeyCertificate, error);
        if (certificate) {
            CFTypeRef trustSettingsDictOrArray = NULL;
            if (SecXPCDictionaryCopyPListOptional(event, kSecXPCKeySettings, &trustSettingsDictOrArray, error)) {
                bool result = _SecTrustStoreSetTrustSettings(ts, certificate, trustSettingsDictOrArray, error);
                xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
                noError = true;
                CFReleaseSafe(trustSettingsDictOrArray);
            }
            CFReleaseNull(certificate);
        }
    }
    return noError;
}

static bool SecXPCTrustStoreRemoveCertificate(SecurityClient * __unused client, xpc_object_t event,
                                              xpc_object_t reply, CFErrorRef *error) {
    SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, error);
    if (ts) {
        SecCertificateRef cert = SecXPCDictionaryCopyCertificate(event, kSecXPCKeyCertificate, error);
        if (cert) {
            bool result = _SecTrustStoreRemoveCertificate(ts, cert, error);
            xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
            CFReleaseNull(cert);
            return true;
        }
    }
    return false;
}

static bool SecXPCTrustStoreCopyAll(SecurityClient * __unused client, xpc_object_t event,
                                    xpc_object_t reply, CFErrorRef *error) {
    SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, error);
    if (ts) {
        CFArrayRef trustStoreContents = NULL;
        if(_SecTrustStoreCopyAll(ts, &trustStoreContents, error) && trustStoreContents) {
            SecXPCDictionarySetPList(reply, kSecXPCKeyResult, trustStoreContents, error);
            CFReleaseNull(trustStoreContents);
            return true;
        }
    }
    return false;
}

static bool SecXPCTrustStoreCopyUsageConstraints(SecurityClient * __unused client, xpc_object_t event,
                                                 xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, error);
    if (ts) {
        SecCertificateRef cert = SecXPCDictionaryCopyCertificate(event, kSecXPCKeyCertificate, error);
        if (cert) {
            CFArrayRef usageConstraints = NULL;
            if(_SecTrustStoreCopyUsageConstraints(ts, cert, &usageConstraints, error) && usageConstraints) {
                SecXPCDictionarySetPList(reply, kSecXPCKeyResult, usageConstraints, error);
                CFReleaseNull(usageConstraints);
                result = true;
            }
            CFReleaseNull(cert);
        }
    }
    return result;
}

static bool SecXPC_OCSPCacheFlush(SecurityClient * __unused client, xpc_object_t __unused event,
                                  xpc_object_t __unused reply, CFErrorRef *error) {
    if(SecOCSPCacheFlush(error)) {
        return true;
    }
    return false;
}

static bool SecXPC_OTAPKI_GetCurrentTrustStoreVersion(SecurityClient * __unused client, xpc_object_t __unused event,
                                          xpc_object_t reply, CFErrorRef *error) {
    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, SecOTAPKIGetCurrentTrustStoreVersion(error));
    return true;
}

static bool SecXPC_OTAPKI_GetCurrentAssetVersion(SecurityClient * __unused client, xpc_object_t __unused event,
                                                 xpc_object_t reply, CFErrorRef *error) {
    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, SecOTAPKIGetCurrentAssetVersion(error));
    return true;
}

static bool SecXPC_OTAPKI_GetEscrowCertificates(SecurityClient * __unused client, xpc_object_t event,
                                                xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    uint32_t escrowRootType = (uint32_t)xpc_dictionary_get_uint64(event, "escrowType");
    CFArrayRef array = SecOTAPKICopyCurrentEscrowCertificates(escrowRootType, error);
    if (array) {
        xpc_object_t xpc_array = _CFXPCCreateXPCObjectFromCFObject(array);
        xpc_dictionary_set_value(reply, kSecXPCKeyResult, xpc_array);
        xpc_release(xpc_array);
        result = true;
    }
    CFReleaseNull(array);
    return result;
}

static bool SecXPC_OTAPKI_GetNewAsset(SecurityClient * __unused client, xpc_object_t __unused event,
                                      xpc_object_t reply, CFErrorRef *error) {
    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, SecOTAPKISignalNewAsset(error));
    return true;
}

static bool SecXPC_OTASecExperiment_GetNewAsset(SecurityClient * __unused client, xpc_object_t __unused event,
                                      xpc_object_t reply, CFErrorRef *error) {
    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, SecOTASecExperimentGetNewAsset(error));
    return true;
}

static bool SecXPC_OTASecExperiment_GetAsset(SecurityClient * __unused client, xpc_object_t __unused event,
                                      xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    CFDictionaryRef asset = SecOTASecExperimentCopyAsset(error);
    if (asset) {
        xpc_object_t xpc_dict = _CFXPCCreateXPCObjectFromCFObject(asset);
        if (xpc_dict) {
            xpc_dictionary_set_value(reply, kSecXPCKeyResult, xpc_dict);
            xpc_release(xpc_dict);
            result = true;
        }
    }
    CFReleaseNull(asset);
    return result;
}

static bool SecXPC_OTAPKI_CopyTrustedCTLogs(SecurityClient * __unused client, xpc_object_t __unused event,
                                            xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    CFDictionaryRef trustedLogs = SecOTAPKICopyCurrentTrustedCTLogs(error);
    if (trustedLogs) {
        xpc_object_t xpc_dictionary = _CFXPCCreateXPCObjectFromCFObject(trustedLogs);
        if (xpc_dictionary) {
            xpc_dictionary_set_value(reply, kSecXPCKeyResult, xpc_dictionary);
            xpc_release(xpc_dictionary);
            result = true;
        }
    }
    CFReleaseNull(trustedLogs);
    return result;
}

static bool SecXPC_OTAPKI_CopyCTLogForKeyID(SecurityClient * __unused client, xpc_object_t event,
                                            xpc_object_t reply, CFErrorRef *error) {
    bool result = false;
    size_t length = 0;
    const void *bytes = xpc_dictionary_get_data(event, kSecXPCData, &length);
    CFDataRef keyID = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes, length, kCFAllocatorNull);
    if (keyID) {
        CFDictionaryRef logDict = SecOTAPKICopyCTLogForKeyID(keyID, error);
        if (logDict) {
            xpc_object_t xpc_dictionary = _CFXPCCreateXPCObjectFromCFObject(logDict);
            xpc_dictionary_set_value(reply, kSecXPCKeyResult, xpc_dictionary);
            xpc_release(xpc_dictionary);
            CFReleaseNull(logDict);
            result = true;
        }
        CFReleaseNull(keyID);
    }
    return result;
}

static bool SecXPC_Networking_AnalyticsReport(SecurityClient * __unused client, xpc_object_t event,
                                       xpc_object_t reply, CFErrorRef *error) {
    xpc_object_t attributes = xpc_dictionary_get_dictionary(event, kSecTrustEventAttributesKey);
    CFStringRef eventName = SecXPCDictionaryCopyString(event, kSecTrustEventNameKey, error);
    bool result = false;
    if (attributes && eventName) {
        result = SecNetworkingAnalyticsReport(eventName, attributes, error);
    }
    xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
    CFReleaseNull(eventName);
    return result;
}

static bool SecXPCTrustStoreSetCTExceptions(SecurityClient *client, xpc_object_t event,
                                            xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    CFDictionaryRef exceptions = NULL;
    if (!SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error) || !appID) {
        /* We always want to set the app ID with the exceptions */
        appID = SecTaskCopyApplicationIdentifier(client->task);
    }
    (void)SecXPCDictionaryCopyDictionaryOptional(event, kSecTrustExceptionsKey, &exceptions, error);
    bool result = _SecTrustStoreSetCTExceptions(appID, exceptions, error);
    xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
    CFReleaseNull(exceptions);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustStoreCopyCTExceptions(SecurityClient * __unused client, xpc_object_t event,
                                             xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    (void)SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error);
    CFDictionaryRef exceptions = _SecTrustStoreCopyCTExceptions(appID, error);
    SecXPCDictionarySetPListOptional(reply, kSecTrustExceptionsKey, exceptions, error);
    CFReleaseNull(exceptions);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustStoreSetCARevocationAdditions(SecurityClient *client, xpc_object_t event,
                                                     xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    CFDictionaryRef additions = NULL;
    if (!SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error) || !appID) {
        /* We always want to set the app ID with the additions */
        appID = SecTaskCopyApplicationIdentifier(client->task);
    }
    (void)SecXPCDictionaryCopyDictionaryOptional(event, kSecTrustRevocationAdditionsKey, &additions, error);
    bool result = _SecTrustStoreSetCARevocationAdditions(appID, additions, error);
    xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
    CFReleaseNull(additions);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustStoreCopyCARevocationAdditions(SecurityClient * __unused client, xpc_object_t event,
                                                      xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    (void)SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error);
    CFDictionaryRef additions = _SecTrustStoreCopyCARevocationAdditions(appID, error);
    SecXPCDictionarySetPListOptional(reply, kSecTrustRevocationAdditionsKey, additions, error);
    CFReleaseNull(additions);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustStoreSetTransparentConnectionPins(SecurityClient *client, xpc_object_t event,
                                                         xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    CFArrayRef pins = NULL;
    if (!SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error) || !appID) {
        /* We always want to set the app ID with the additions */
        appID = SecTaskCopyApplicationIdentifier(client->task);
    }
    (void)SecXPCDictionaryCopyArrayOptional(event, kSecTrustAnchorsKey, &pins, error);
    bool result = _SecTrustStoreSetTransparentConnectionPins(appID, pins, error);
    xpc_dictionary_set_bool(reply, kSecXPCKeyResult, result);
    CFReleaseNull(pins);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustStoreCopyTransparentConnectionPins(SecurityClient * __unused client, xpc_object_t event,
                                                          xpc_object_t reply, CFErrorRef *error) {
    CFStringRef appID = NULL;
    (void)SecXPCDictionaryCopyStringOptional(event, kSecTrustEventApplicationID, &appID, error);
    CFArrayRef pins = _SecTrustStoreCopyTransparentConnectionPins(appID, error);
    SecXPCDictionarySetPListOptional(reply, kSecTrustAnchorsKey, pins, error);
    CFReleaseNull(pins);
    CFReleaseNull(appID);
    return false;
}

static bool SecXPCTrustGetExceptionResetCount(SecurityClient * __unused client, xpc_object_t event, xpc_object_t reply, CFErrorRef *error) {
    uint64_t exceptionResetCount = SecTrustServerGetExceptionResetCount(error);
    if (error && *error) {
        return false;
    }

    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, exceptionResetCount);
    return true;
}

static bool SecXPCTrustIncrementExceptionResetCount(SecurityClient * __unused client, xpc_object_t event, xpc_object_t reply, CFErrorRef *error) {
    OSStatus status = errSecInternal;
    bool result = SecTrustServerIncrementExceptionResetCount(error);
    if (result && (!error || (error && !*error))) {
        status = errSecSuccess;
    }

    xpc_dictionary_set_bool(reply, kSecXPCKeyResult, status);
    return result;
}

static bool SecXPC_Valid_Update(SecurityClient * __unused client, xpc_object_t __unused event,
                                xpc_object_t reply, CFErrorRef *error) {
    xpc_dictionary_set_uint64(reply, kSecXPCKeyResult, SecRevocationDbUpdate(error));
    return true;
}

typedef bool(*SecXPCOperationHandler)(SecurityClient *client, xpc_object_t event, xpc_object_t reply, CFErrorRef *error);

typedef struct {
    CFStringRef entitlement;
    SecXPCOperationHandler handler;
} SecXPCServerOperation;

struct trustd_operations {
    SecXPCServerOperation trust_store_contains;
    SecXPCServerOperation trust_store_set_trust_settings;
    SecXPCServerOperation trust_store_remove_certificate;
    SecXPCServerOperation trust_store_copy_all;
    SecXPCServerOperation trust_store_copy_usage_constraints;
    SecXPCServerOperation ocsp_cache_flush;
    SecXPCServerOperation ota_pki_trust_store_version;
    SecXPCServerOperation ota_pki_asset_version;
    SecXPCServerOperation ota_pki_get_escrow_certs;
    SecXPCServerOperation ota_pki_get_new_asset;
    SecXPCServerOperation ota_pki_copy_trusted_ct_logs;
    SecXPCServerOperation ota_pki_copy_ct_log_for_keyid;
    SecXPCServerOperation networking_analytics_report;
    SecXPCServerOperation trust_store_set_ct_exceptions;
    SecXPCServerOperation trust_store_copy_ct_exceptions;
    SecXPCServerOperation ota_secexperiment_get_asset;
    SecXPCServerOperation ota_secexperiment_get_new_asset;
    SecXPCServerOperation trust_get_exception_reset_count;
    SecXPCServerOperation trust_increment_exception_reset_count;
    SecXPCServerOperation trust_store_set_ca_revocation_additions;
    SecXPCServerOperation trust_store_copy_ca_revocation_additions;
    SecXPCServerOperation valid_update;
    SecXPCServerOperation trust_store_set_transparent_connection_pins;
    SecXPCServerOperation trust_store_copy_transparent_connection_pins;
};

static struct trustd_operations trustd_ops = {
    .trust_store_contains = { NULL, SecXPCTrustStoreContains },
    .trust_store_set_trust_settings = { kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreSetTrustSettings },
    .trust_store_remove_certificate = { kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreRemoveCertificate },
    .trust_store_copy_all = { kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreCopyAll },
    .trust_store_copy_usage_constraints = { kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreCopyUsageConstraints },
    .ocsp_cache_flush = { NULL, SecXPC_OCSPCacheFlush },
    .ota_pki_trust_store_version = { NULL, SecXPC_OTAPKI_GetCurrentTrustStoreVersion },
    .ota_pki_asset_version = { NULL, SecXPC_OTAPKI_GetCurrentAssetVersion },
    .ota_pki_get_escrow_certs = { NULL, SecXPC_OTAPKI_GetEscrowCertificates },
    .ota_pki_get_new_asset = { NULL, SecXPC_OTAPKI_GetNewAsset },
    .ota_secexperiment_get_new_asset = { NULL, SecXPC_OTASecExperiment_GetNewAsset },
    .ota_secexperiment_get_asset = { NULL, SecXPC_OTASecExperiment_GetAsset },
    .ota_pki_copy_trusted_ct_logs = { NULL, SecXPC_OTAPKI_CopyTrustedCTLogs },
    .ota_pki_copy_ct_log_for_keyid = { NULL, SecXPC_OTAPKI_CopyCTLogForKeyID },
    .networking_analytics_report = { NULL, SecXPC_Networking_AnalyticsReport },
    .trust_store_set_ct_exceptions = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreSetCTExceptions },
    .trust_store_copy_ct_exceptions = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreCopyCTExceptions },
    .trust_get_exception_reset_count = { NULL, SecXPCTrustGetExceptionResetCount },
    .trust_increment_exception_reset_count = { kSecEntitlementModifyAnchorCertificates, SecXPCTrustIncrementExceptionResetCount },
    .trust_store_set_ca_revocation_additions = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreSetCARevocationAdditions },
    .trust_store_copy_ca_revocation_additions = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreCopyCARevocationAdditions },
    .valid_update = { NULL, SecXPC_Valid_Update },
    .trust_store_set_transparent_connection_pins = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreSetTransparentConnectionPins },
    .trust_store_copy_transparent_connection_pins = {kSecEntitlementModifyAnchorCertificates, SecXPCTrustStoreCopyTransparentConnectionPins },
};

static void trustd_xpc_dictionary_handler(const xpc_connection_t connection, xpc_object_t event) {
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
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
        .canAccessNetworkExtensionAccessGroups = false,
#if TARGET_OS_IPHONE
        .inMultiUser = false,
#endif
    };

    secdebug("serverxpc", "entering");
    if (type == XPC_TYPE_DICTIONARY) {
        replyMessage = xpc_dictionary_create_reply(event);

        uint64_t operation = xpc_dictionary_get_uint64(event, kSecXPCKeyOperation);

        audit_token_t auditToken = {};
        xpc_connection_get_audit_token(connection, &auditToken);

        client.task = SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);
        clientAuditToken = CFDataCreate(kCFAllocatorDefault, (const UInt8*)&auditToken, sizeof(auditToken));
        client.accessGroups = SecTaskCopyAccessGroups(client.task);

        secinfo("serverxpc", "XPC [%@] operation: %@ (%" PRIu64 ")", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), operation);

        if (operation == sec_trust_evaluate_id) {
            CFArrayRef certificates = NULL, anchors = NULL, policies = NULL, responses = NULL, scts = NULL, trustedLogs = NULL, exceptions = NULL;
            bool anchorsOnly = xpc_dictionary_get_bool(event, kSecTrustAnchorsOnlyKey);
            bool keychainsAllowed = xpc_dictionary_get_bool(event, kSecTrustKeychainsAllowedKey);
            double verifyTime;
            if (SecXPCDictionaryCopyCertificates(event, kSecTrustCertificatesKey, &certificates, &error) &&
                SecXPCDictionaryCopyCertificatesOptional(event, kSecTrustAnchorsKey, &anchors, &error) &&
                SecXPCDictionaryCopyPoliciesOptional(event, kSecTrustPoliciesKey, &policies, &error) &&
                SecXPCDictionaryCopyCFDataArrayOptional(event, kSecTrustResponsesKey, &responses, &error) &&
                SecXPCDictionaryCopyCFDataArrayOptional(event, kSecTrustSCTsKey, &scts, &error) &&
                SecXPCDictionaryCopyArrayOptional(event, kSecTrustTrustedLogsKey, &trustedLogs, &error) &&
                SecXPCDictionaryGetDouble(event, kSecTrustVerifyDateKey, &verifyTime, &error) &&
                SecXPCDictionaryCopyArrayOptional(event, kSecTrustExceptionsKey, &exceptions, &error)) {
                // If we have no error yet, capture connection and reply in block and properly retain them.
                xpc_retain(connection);
                CFRetainSafe(client.task);
                CFRetainSafe(clientAuditToken);

                // Clear replyMessage so we don't send a synchronous reply.
                xpc_object_t asyncReply = replyMessage;
                replyMessage = NULL;

                SecTrustServerEvaluateBlock(SecTrustServerGetWorkloop(), clientAuditToken, certificates, anchors, anchorsOnly, keychainsAllowed, policies,
                                            responses, scts, trustedLogs, verifyTime, client.accessGroups, exceptions,
                                            ^(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, CFArrayRef chain,
                                              CFErrorRef replyError) {
                    // Send back reply now
                    if (replyError) {
                        CFRetain(replyError);
                    } else {
                        xpc_dictionary_set_int64(asyncReply, kSecTrustResultKey, tr);
                        SecXPCDictionarySetPListOptional(asyncReply, kSecTrustDetailsKey, details, &replyError) &&
                        SecXPCDictionarySetPListOptional(asyncReply, kSecTrustInfoKey, info, &replyError) &&
                        SecXPCDictionarySetChainOptional(asyncReply, kSecTrustChainKey, chain, &replyError);
                    }
                    if (replyError) {
                        secdebug("ipc", "%@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), replyError);
                        xpc_object_t xpcReplyError = SecCreateXPCObjectWithCFError(replyError);
                        if (xpcReplyError) {
                            xpc_dictionary_set_value(asyncReply, kSecXPCKeyError, xpcReplyError);
                            xpc_release(xpcReplyError);
                        }
                        CFReleaseNull(replyError);
                    } else {
                        secdebug("ipc", "%@ %@ responding %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), asyncReply);
                    }

                    xpc_connection_send_message(connection, asyncReply);
                    xpc_release(asyncReply);
                    xpc_release(connection);
                    CFReleaseSafe(client.task);
                    CFReleaseSafe(clientAuditToken);
                });
            }
            CFReleaseSafe(policies);
            CFReleaseSafe(anchors);
            CFReleaseSafe(certificates);
            CFReleaseSafe(responses);
            CFReleaseSafe(scts);
            CFReleaseSafe(trustedLogs);
            CFReleaseSafe(exceptions);
        } else {
            SecXPCServerOperation *server_op = NULL;
            switch (operation) {
                case sec_trust_store_contains_id:
                    server_op = &trustd_ops.trust_store_contains;
                    break;
                case sec_trust_store_set_trust_settings_id:
                    server_op = &trustd_ops.trust_store_set_trust_settings;
                    break;
                case sec_trust_store_remove_certificate_id:
                    server_op = &trustd_ops.trust_store_remove_certificate;
                    break;
                case sec_trust_store_copy_all_id:
                    server_op = &trustd_ops.trust_store_copy_all;
                    break;
                case sec_trust_store_copy_usage_constraints_id:
                    server_op = &trustd_ops.trust_store_copy_usage_constraints;
                    break;
                case sec_ocsp_cache_flush_id:
                    server_op = &trustd_ops.ocsp_cache_flush;
                    break;
                case sec_ota_pki_trust_store_version_id:
                    server_op = &trustd_ops.ota_pki_trust_store_version;
                    break;
                case sec_ota_pki_asset_version_id:
                    server_op = &trustd_ops.ota_pki_asset_version;
                    break;
                case kSecXPCOpOTAGetEscrowCertificates:
                    server_op = &trustd_ops.ota_pki_get_escrow_certs;
                    break;
                case kSecXPCOpOTAPKIGetNewAsset:
                    server_op = &trustd_ops.ota_pki_get_new_asset;
                    break;
                case kSecXPCOpOTASecExperimentGetNewAsset:
                    server_op = &trustd_ops.ota_secexperiment_get_new_asset;
                    break;
                case kSecXPCOpOTASecExperimentGetAsset:
                    server_op = &trustd_ops.ota_secexperiment_get_asset;
                    break;
                case kSecXPCOpOTAPKICopyTrustedCTLogs:
                    server_op = &trustd_ops.ota_pki_copy_trusted_ct_logs;
                    break;
                case kSecXPCOpOTAPKICopyCTLogForKeyID:
                    server_op = &trustd_ops.ota_pki_copy_ct_log_for_keyid;
                    break;
                case kSecXPCOpNetworkingAnalyticsReport:
                    server_op = &trustd_ops.networking_analytics_report;
                    break;
                case kSecXPCOpSetCTExceptions:
                    server_op = &trustd_ops.trust_store_set_ct_exceptions;
                    break;
                case kSecXPCOpCopyCTExceptions:
                    server_op = &trustd_ops.trust_store_copy_ct_exceptions;
                    break;
                case sec_trust_get_exception_reset_count_id:
                    server_op = &trustd_ops.trust_get_exception_reset_count;
                    break;
                case sec_trust_increment_exception_reset_count_id:
                    server_op = &trustd_ops.trust_increment_exception_reset_count;
                    break;
                case kSecXPCOpSetCARevocationAdditions:
                    server_op = &trustd_ops.trust_store_set_ca_revocation_additions;
                    break;
                case kSecXPCOpCopyCARevocationAdditions:
                    server_op = &trustd_ops.trust_store_copy_ca_revocation_additions;
                    break;
                case kSecXPCOpValidUpdate:
                    server_op = &trustd_ops.valid_update;
                    break;
                case kSecXPCOpSetTransparentConnectionPins:
                    server_op = &trustd_ops.trust_store_set_transparent_connection_pins;
                    break;
                case kSecXPCOpCopyTransparentConnectionPins:
                    server_op = &trustd_ops.trust_store_copy_transparent_connection_pins;
                    break;
                default:
                    break;
            }
            if (server_op && server_op->handler) {
                bool entitled = true;
                if (server_op->entitlement) {
                    entitled = EntitlementPresentAndTrue(operation, client.task, server_op->entitlement, &error);
                }
                if (entitled) {
                    (void)server_op->handler(&client, event, replyMessage, &error);
                }
            }
        }

        if (error)
        {
            if(SecErrorGetOSStatus(error) == errSecItemNotFound)
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
    if (xpcError)
        xpc_release(xpcError);
    CFReleaseSafe(error);
    CFReleaseSafe(client.accessGroups);
    CFReleaseSafe(client.musr);
    CFReleaseSafe(client.task);
    CFReleaseSafe(domains);
    CFReleaseSafe(clientAuditToken);
}

static void trustd_xpc_init(const char *service_name)
{
    secdebug("serverxpc", "start");
    xpc_connection_t listener = xpc_connection_create_mach_service(service_name, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        seccritical("security failed to register xpc listener for %s, exiting", service_name);
        abort();
    }

    xpc_connection_set_event_handler(listener, ^(xpc_object_t connection) {
        if (xpc_get_type(connection) == XPC_TYPE_CONNECTION) {
            xpc_connection_set_target_queue(connection, SecTrustServerGetWorkloop());
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
                    trustd_xpc_dictionary_handler(connection, event);
                }
            });
            xpc_connection_activate(connection);
        }
    });
    xpc_connection_activate(listener);
}

static void trustd_sandbox(void) {
#if TARGET_OS_OSX
    char buf[PATH_MAX] = "";

    if (!_set_user_dir_suffix("com.apple.trustd") ||
        confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *tempdir = realpath(buf, NULL);
    if (tempdir == NULL) {
        secerror("failed to resolve temporary directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (confstr(_CS_DARWIN_USER_CACHE_DIR, buf, sizeof(buf)) == 0 ||
        (mkdir(buf, 0700) && errno != EEXIST)) {
        secerror("failed to initialize cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *cachedir = realpath(buf, NULL);
    if (cachedir == NULL) {
        secerror("failed to resolve cache directory (%d): %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    const char *parameters[] = {
        "_TMPDIR", tempdir,
        "_DARWIN_CACHE_DIR", cachedir,
        NULL
    };

    char *sberror = NULL;
    if (sandbox_init_with_parameters("com.apple.trustd", SANDBOX_NAMED, parameters, &sberror) != 0) {
        secerror("Failed to enter trustd sandbox: %{public}s", sberror);
        exit(EXIT_FAILURE);
    }

    free(tempdir);
    free(cachedir);
#else // !TARGET_OS_OSX
    char buf[PATH_MAX] = "";
    _set_user_dir_suffix("com.apple.trustd");
    confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf));
#endif // !TARGET_OS_OSX
}

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

    trustd_sandbox();
    FixTrustdFilePermissions();
    /* set up SQLite before some other component has a chance to create a database connection */
    _SecDbServerSetup();

    const char *serviceName = kTrustdXPCServiceName;
    if (argc > 1 && (!strcmp(argv[1], "--agent"))) {
        serviceName = kTrustdAgentXPCServiceName;
    }

    /* migrate files and initialize static content */
    trustd_init_server();
    /* We're ready now. Go. */
    trustd_xpc_init(serviceName);
    dispatch_main();
}
