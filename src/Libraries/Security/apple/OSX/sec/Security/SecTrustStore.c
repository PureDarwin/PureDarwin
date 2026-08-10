/*
 * Copyright (c) 2007-2020 Apple Inc. All Rights Reserved.
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
 * SecTrustStore.c - CertificateSource API to a system root certificate store
 */
#include <Security/SecTrustStore.h>

#include <Security/SecCertificateInternal.h>
#include <Security/SecInternal.h>
#include <Security/SecuritydXPC.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <CoreFoundation/CFString.h>
#include <AssertMacros.h>
#include <ipc/securityd_client.h>
#include "SecFramework.h"
#include <sys/stat.h>
#include <stdio.h>
#include <os/activity.h>
#include <dirent.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustSettingsPriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include "utilities/SecDb.h"
#include "SecTrustInternal.h"

static CFStringRef kSecTrustStoreUserName = CFSTR("user");
static CFStringRef kSecTrustStoreAdminName = CFSTR("admin");
static CFStringRef kSecTrustStoreSystemName = CFSTR("system");

SecTrustStoreRef SecTrustStoreForDomain(SecTrustStoreDomain domain) {
    CFStringRef domainName = NULL;
    switch (domain) {
        case kSecTrustStoreDomainUser:
            domainName = kSecTrustStoreUserName;
            break;
        case kSecTrustStoreDomainAdmin:
            domainName = kSecTrustStoreAdminName;
            break;
        case kSecTrustStoreDomainSystem:
            domainName = kSecTrustStoreSystemName;
            break;
        default:
            return NULL;
    }

    if (gTrustd) {
        return gTrustd->sec_trust_store_for_domain(domainName, NULL);
    } else {
        return (SecTrustStoreRef)domainName;
    }
}

static bool SecXPCDictionarySetCertificate(xpc_object_t message, const char *key, SecCertificateRef certificate, CFErrorRef *error) {
    if (certificate) {
        xpc_dictionary_set_data(message, key, SecCertificateGetBytePtr(certificate),
                                SecCertificateGetLength(certificate));
        return true;
    }
    return SecError(errSecParam, error, CFSTR("NULL certificate"));
}

static bool string_cert_to_bool_error(enum SecXPCOperation op, SecTrustStoreRef ts, SecCertificateRef cert, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, blockError) &&
                SecXPCDictionarySetCertificate(message, kSecXPCKeyCertificate, cert, blockError);
    }, NULL);
}

static bool string_cert_to_bool_bool_error(enum SecXPCOperation op, SecTrustStoreRef ts, SecCertificateRef cert, bool *result, CFErrorRef *error)
{
    os_activity_t activity = os_activity_create("SecTrustStoreContains", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    bool status = securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, blockError) &&
               SecXPCDictionarySetCertificate(message, kSecXPCKeyCertificate, cert, blockError);
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        if (result)
            *result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return true;
    });
    os_release(activity);
    return status;
}

Boolean SecTrustStoreContains(SecTrustStoreRef ts,
	SecCertificateRef certificate) {
    bool ok = false;
	__block bool contains = false;
    require(ts, errOut);
    ok = (SecOSStatusWith(^bool (CFErrorRef *error) {
        return TRUSTD_XPC(sec_trust_store_contains, string_cert_to_bool_bool_error, ts, certificate, &contains, error);
    }) == errSecSuccess);

errOut:
	return ok && contains;
}


static bool string_cert_cftype_to_error(enum SecXPCOperation op, SecTrustStoreRef ts, SecCertificateRef certificate, CFTypeRef trustSettingsDictOrArray, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        bool ok = false;
        ok = SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, blockError) &&
            SecXPCDictionarySetCertificate(message, kSecXPCKeyCertificate, certificate, blockError) &&
            (!trustSettingsDictOrArray || SecXPCDictionarySetPList(message, kSecXPCKeySettings, trustSettingsDictOrArray, blockError));
        return ok;
    }, NULL);
}

static OSStatus validateConstraint(Boolean isSelfSigned, CFMutableDictionaryRef trustSettingsDict) {
    OSStatus result = errSecSuccess;

    /* Check "TrustRoot"/"TrustAsRoot" */
    CFNumberRef resultNumber = NULL;
    resultNumber = (CFNumberRef)CFDictionaryGetValue(trustSettingsDict, kSecTrustSettingsResult);
    uint32_t resultValue = kSecTrustSettingsResultInvalid;
    if (!isNumber(resultNumber) && !isSelfSigned) {
        /* only self-signed certs get default of TrustAsRoot */
        return errSecParam;
    }
    if (isNumber(resultNumber) && CFNumberGetValue(resultNumber, kCFNumberSInt32Type, &resultValue)) {
        if (isSelfSigned && resultValue == kSecTrustSettingsResultTrustAsRoot) {
            return errSecParam;
        }
        if (!isSelfSigned && resultValue == kSecTrustSettingsResultTrustRoot) {
            return errSecParam;
        }
    }

    /* If there's a policy specified, change the contents */
    SecPolicyRef policy = NULL;
    policy = (SecPolicyRef)CFDictionaryGetValue(trustSettingsDict, kSecTrustSettingsPolicy);
    if (policy) {
        CFStringRef policyOid = NULL, policyName = NULL;
        policyOid = SecPolicyGetOidString(policy);
        policyName = SecPolicyGetName(policy);
        CFDictionarySetValue(trustSettingsDict, kSecTrustSettingsPolicy, policyOid);
        if (policyName) { CFDictionaryAddValue(trustSettingsDict, kSecTrustSettingsPolicyName, policyName); }
    }

    return result;
}

static OSStatus validateTrustSettings(Boolean isSelfSigned,
            CFTypeRef trustSettingsDictOrArray,
            CFTypeRef * CF_RETURNS_RETAINED modifiedTrustSettings) {
    OSStatus status = errSecParam;
    CFTypeRef result = NULL;

    /* NULL is a valid input */
    if (!trustSettingsDictOrArray && isSelfSigned) {
        return errSecSuccess;
    } else if (!trustSettingsDictOrArray && !isSelfSigned) {
        return errSecParam;
    }

    if (CFDictionaryGetTypeID() == CFGetTypeID(trustSettingsDictOrArray)) {
        result = CFDictionaryCreateMutableCopy(NULL, 0, trustSettingsDictOrArray);
        status = validateConstraint(isSelfSigned, (CFMutableDictionaryRef)result);
    } else if (CFArrayGetTypeID() == CFGetTypeID(trustSettingsDictOrArray)) {
        require_action_quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks),
                             out, status = errSecAllocate);
        CFIndex ix, count = CFArrayGetCount(trustSettingsDictOrArray);
        for (ix = 0; ix < count; ix++) {
            CFDictionaryRef constraint = CFArrayGetValueAtIndex(trustSettingsDictOrArray, ix);
            CFDictionaryRef modifiedConstraint = NULL;
            require_noerr_quiet(status = validateTrustSettings(isSelfSigned, constraint, (CFTypeRef *)&modifiedConstraint), out);
            CFArrayAppendValue((CFMutableArrayRef)result, modifiedConstraint);
            CFReleaseNull(modifiedConstraint); /* constraint now owned by array */
        }
    }

out:
    if (errSecSuccess == status && modifiedTrustSettings) {
        *modifiedTrustSettings = CFRetainSafe(result);
    }
    CFReleaseNull(result);
    return status;
}

OSStatus SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray) {
    __block OSStatus result;
    __block CFTypeRef validatedTrustSettings = NULL;

    Boolean isSelfSigned = false;
    require_noerr_quiet(result = SecCertificateIsSelfSigned(certificate, &isSelfSigned), out);
    require_noerr_quiet(result = validateTrustSettings(isSelfSigned, trustSettingsDictOrArray, &validatedTrustSettings), out);

    os_activity_initiate("SecTrustStoreSetTrustSettings", OS_ACTIVITY_FLAG_DEFAULT, ^{
        result = SecOSStatusWith(^bool (CFErrorRef *error) {
            return TRUSTD_XPC(sec_trust_store_set_trust_settings, string_cert_cftype_to_error, ts, certificate, validatedTrustSettings, error);
        });
    });

out:
    CFReleaseNull(validatedTrustSettings);
    return result;
}

OSStatus SecTrustStoreRemoveCertificate(SecTrustStoreRef ts,
    SecCertificateRef certificate)
{
    __block OSStatus status = errSecParam;

    os_activity_t activity = os_activity_create("SecTrustStoreRemoveCertificate", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    require(ts, errOut);

    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        return TRUSTD_XPC(sec_trust_store_remove_certificate, string_cert_to_bool_error, ts, certificate, error);
    });

errOut:
    os_release(activity);
	return status;
}

OSStatus SecTrustStoreGetSettingsVersionNumber(SecTrustSettingsVersionNumber* p_settings_version_number)
{
    if (NULL == p_settings_version_number) {
        return errSecParam;
    }

    OSStatus status = errSecSuccess;
    CFErrorRef error = nil;
    uint64_t versionNumber = SecTrustGetTrustStoreVersionNumber(&error);
    *p_settings_version_number = (SecTrustSettingsVersionNumber)versionNumber;

    if (error) {
        status = (OSStatus)CFErrorGetCode(error);
    }
    CFReleaseSafe(error);
    return status;
}

OSStatus SecTrustStoreGetSettingsAssetVersionNumber(SecTrustSettingsAssetVersionNumber* p_settings_asset_version_number)
{
    if (NULL == p_settings_asset_version_number) {
        return errSecParam;
    }

    OSStatus status = errSecSuccess;
    CFErrorRef error = nil;
    uint64_t versionNumber = SecTrustGetAssetVersionNumber(&error);
    *p_settings_asset_version_number = (SecTrustSettingsAssetVersionNumber)versionNumber;

    if (error) {
        status = (OSStatus)CFErrorGetCode(error);
    }
    CFReleaseSafe(error);
    return status;
}

static bool string_to_array_error(enum SecXPCOperation op, SecTrustStoreRef ts, CFArrayRef *trustStoreContents, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, blockError);
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        if (trustStoreContents) {
            *trustStoreContents = SecXPCDictionaryCopyArray(response, kSecXPCKeyResult, blockError);
            if (!*trustStoreContents) return false;
        }
        return true;
    });
}

OSStatus SecTrustStoreCopyAll(SecTrustStoreRef ts, CFArrayRef *trustStoreContents)
{
    __block CFArrayRef results = NULL;
    OSStatus status = errSecParam;

    os_activity_t activity = os_activity_create("SecTrustStoreCopyAll", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    require(ts, errOut);

    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        return TRUSTD_XPC(sec_trust_store_copy_all, string_to_array_error, ts, &results, error);
    });

    *trustStoreContents = results;

errOut:
    os_release(activity);
    return status;
}

static bool string_cert_to_array_error(enum SecXPCOperation op, SecTrustStoreRef ts, SecCertificateRef cert, CFArrayRef *usageConstraints, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *blockError) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, blockError) &&
                SecXPCDictionarySetCertificate(message, kSecXPCKeyCertificate, cert, blockError);
    }, ^bool(xpc_object_t response, CFErrorRef *blockError) {
        return SecXPCDictionaryCopyArrayOptional(response, kSecXPCKeyResult, usageConstraints, blockError);
    });
}

OSStatus SecTrustStoreCopyUsageConstraints(SecTrustStoreRef ts, SecCertificateRef certificate, CFArrayRef *usageConstraints)
{
    __block CFArrayRef results = NULL;
    OSStatus status = errSecParam;

    os_activity_t activity = os_activity_create("SecTrustStoreCopyUsageConstraints", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);
    require(ts, errOut);
    require(certificate, errOut);
    require(usageConstraints, errOut);

    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        return TRUSTD_XPC(sec_trust_store_copy_usage_constraints, string_cert_to_array_error, ts, certificate, &results, error);
    });

    *usageConstraints = results;

errOut:
    os_release(activity);
    return status;
}

#define do_if_registered(sdp, ...) if (gTrustd && gTrustd->sdp) { return gTrustd->sdp(__VA_ARGS__); }

const CFStringRef kSecTrustStoreSPKIHashKey = CFSTR("SubjectPublicKeyInfoHash");
const CFStringRef kSecTrustStoreHashAlgorithmKey = CFSTR("HashAlgorithm");

/* MARK: CT Enforcement Exceptions */

const CFStringRef kSecCTExceptionsCAsKey = CFSTR("DisabledForCAs");
const CFStringRef kSecCTExceptionsDomainsKey = CFSTR("DisabledForDomains");
const CFStringRef kSecCTExceptionsHashAlgorithmKey = kSecTrustStoreHashAlgorithmKey;
const CFStringRef kSecCTExceptionsSPKIHashKey = kSecTrustStoreSPKIHashKey;

bool SecTrustStoreSetCTExceptions(CFStringRef applicationIdentifier, CFDictionaryRef exceptions, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    if (applicationIdentifier && gTrustd && gTrustd->sec_trust_store_set_ct_exceptions) {
        return gTrustd->sec_trust_store_set_ct_exceptions(applicationIdentifier, exceptions, error);
    } else if (gTrustd && gTrustd->sec_trust_store_set_ct_exceptions) {
        /* When calling from the TrustTests, we need to pass the appID for the tests. Ordinarily,
         * this is done by trustd using the client's entitlements. */
        return gTrustd->sec_trust_store_set_ct_exceptions(CFSTR("com.apple.trusttests"), exceptions, error);
    }

    os_activity_t activity = os_activity_create("SecTrustStoreSetCTExceptions", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block bool result = false;
    securityd_send_sync_and_do(kSecXPCOpSetCTExceptions, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetPListOptional(message, kSecTrustExceptionsKey, exceptions, block_error);
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    return SecError(errSecReadOnly, error, CFSTR("SecTrustStoreSetCTExceptions not supported on bridgeOS"));
#endif // TARGET_OS_BRIDGE
}

CFDictionaryRef SecTrustStoreCopyCTExceptions(CFStringRef applicationIdentifier, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    do_if_registered(sec_trust_store_copy_ct_exceptions, applicationIdentifier, error);

    os_activity_t activity = os_activity_create("SecTrustStoreCopyCTExceptions", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block CFDictionaryRef result = NULL;
    securityd_send_sync_and_do(kSecXPCOpCopyCTExceptions, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        (void)SecXPCDictionaryCopyDictionaryOptional(response, kSecTrustExceptionsKey, &result, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    SecError(errSecReadOnly, error, CFSTR("SecTrustStoreCopyCTExceptions not supported on bridgeOS"));
    return NULL;
#endif // TARGET_OS_BRIDGE
}

/* MARK: CA Revocation Additions */

/* Specify explicit additions to the list of known CAs for which revocation will be checked.
 * Input: dictionary with following key and value:
 *   Key = kSecCARevocationAdditionsKey; Value = Array of dictionaries
 * For revocation checking to be enabled for certificates issued by a CA, the CA must be specified as a
 * dictionary entry containing the hash of the subjectPublicKeyInfo that appears in the CA certificate:
 *   Key = kSecCARevocationHashAlgorithmKey; Value = String. Currently, must be ”sha256”.
 *   Key = kSecCARevocationSPKIHashKey; Value = Data. Created by applying the specified hash algorithm
 *   to the DER encoding of the certificate's subjectPublicKeyInfo.
*/

const CFStringRef kSecCARevocationAdditionsKey = CFSTR("EnabledForCAs");
const CFStringRef kSecCARevocationHashAlgorithmKey = kSecTrustStoreHashAlgorithmKey;
const CFStringRef kSecCARevocationSPKIHashKey = kSecTrustStoreSPKIHashKey;

bool SecTrustStoreSetCARevocationAdditions(CFStringRef applicationIdentifier, CFDictionaryRef additions, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    if (applicationIdentifier && gTrustd && gTrustd->sec_trust_store_set_ca_revocation_additions) {
        return gTrustd->sec_trust_store_set_ca_revocation_additions(applicationIdentifier, additions, error);
    } else if (gTrustd && gTrustd->sec_trust_store_set_ca_revocation_additions) {
        /* When calling from the TrustTests, we need to pass the appID for the tests. Ordinarily,
         * this is done by trustd using the client's entitlements. */
        return gTrustd->sec_trust_store_set_ca_revocation_additions(CFSTR("com.apple.trusttests"), additions, error);
    }

    os_activity_t activity = os_activity_create("SecTrustStoreSetCARevocationAdditions", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block bool result = false;
    securityd_send_sync_and_do(kSecXPCOpSetCARevocationAdditions, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetPListOptional(message, kSecTrustRevocationAdditionsKey, additions, block_error);
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    return SecError(errSecReadOnly, error, CFSTR("SecTrustStoreSetCARevocationAdditions not supported on bridgeOS"));
#endif // TARGET_OS_BRIDGE
}

CFDictionaryRef SecTrustStoreCopyCARevocationAdditions(CFStringRef applicationIdentifier, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    do_if_registered(sec_trust_store_copy_ca_revocation_additions, applicationIdentifier, error);

    os_activity_t activity = os_activity_create("SecTrustStoreCopyCARevocationAdditions", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block CFDictionaryRef result = NULL;
    securityd_send_sync_and_do(kSecXPCOpCopyCARevocationAdditions, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        (void)SecXPCDictionaryCopyDictionaryOptional(response, kSecTrustRevocationAdditionsKey, &result, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    SecError(errSecReadOnly, error, CFSTR("SecTrustStoreCopyCARevocationAdditions not supported on bridgeOS"));
    return NULL;
#endif // TARGET_OS_BRIDGE
}

/* MARK: Transparent Connection Pins */

bool SecTrustStoreSetTransparentConnectionPins(CFStringRef applicationIdentifier, CFArrayRef pins, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    if (applicationIdentifier && gTrustd && gTrustd->sec_trust_store_set_transparent_connection_pins) {
        return gTrustd->sec_trust_store_set_transparent_connection_pins(applicationIdentifier, pins, error);
    } else if (gTrustd && gTrustd->sec_trust_store_set_transparent_connection_pins) {
        /* When calling from the TrustTests, we need to pass the appID for the tests. Ordinarily,
         * this is done by trustd using the client's entitlements. */
        return gTrustd->sec_trust_store_set_transparent_connection_pins(CFSTR("com.apple.trusttests"), pins, error);
    }

    os_activity_t activity = os_activity_create("SecTrustStoreSetTransparentConnectionPins", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block bool result = false;
    securityd_send_sync_and_do(kSecXPCOpSetTransparentConnectionPins, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetPListOptional(message, kSecTrustAnchorsKey, pins, block_error);
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        result = SecXPCDictionaryGetBool(response, kSecXPCKeyResult, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    return SecError(errSecReadOnly, error, CFSTR("SecTrustStoreSetTransparentConnectionPins not supported on bridgeOS"));
#endif // TARGET_OS_BRIDGE
}

CF_RETURNS_RETAINED CFArrayRef SecTrustStoreCopyTransparentConnectionPins(CFStringRef applicationIdentifier, CFErrorRef *error) {
#if !TARGET_OS_BRIDGE
    do_if_registered(sec_trust_store_copy_transparent_connection_pins, applicationIdentifier, error);

    os_activity_t activity = os_activity_create("SecTrustStoreCopyTransparentConnectionPins", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block CFArrayRef result = NULL;
    securityd_send_sync_and_do(kSecXPCOpCopyTransparentConnectionPins, error, ^bool(xpc_object_t message, CFErrorRef *block_error) {
        SecXPCDictionarySetStringOptional(message, kSecTrustEventApplicationID, applicationIdentifier, block_error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *block_error) {
        (void)SecXPCDictionaryCopyArrayOptional(response, kSecTrustAnchorsKey, &result, block_error);
        return true;
    });

    os_release(activity);
    return result;
#else // TARGET_OS_BRIDGE
    SecError(errSecReadOnly, error, CFSTR("SecTrustStoreCopyTransparentConnectionPins not supported on bridgeOS"));
    return NULL;
#endif // TARGET_OS_BRIDGE
}
