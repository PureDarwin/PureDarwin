/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <pthread/pthread.h>

#include "server_entitlement_helpers.h"

#include <Security/SecTask.h>
#include <Security/SecTaskPriv.h>
#include "ipc/securityd_client.h"
#include <Security/SecEntitlements.h>
#include "sectask/SystemEntitlements.h"
#include <Security/SecItem.h>
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/debugging.h"
#include <os/feature_private.h>

CFStringRef SecTaskCopyStringForEntitlement(SecTaskRef task,
                                            CFStringRef entitlement)
{
    CFStringRef value = (CFStringRef)SecTaskCopyValueForEntitlement(task,
                                                                    entitlement, NULL);
    if (value && CFGetTypeID(value) != CFStringGetTypeID()) {
        CFReleaseNull(value);
    }

    return value;
}

CFArrayRef SecTaskCopyArrayOfStringsForEntitlement(SecTaskRef task,
                                                   CFStringRef entitlement)
{
    CFArrayRef value = (CFArrayRef)SecTaskCopyValueForEntitlement(task,
                                                                  entitlement, NULL);
    if (value) {
        if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex ix, count = CFArrayGetCount(value);
            for (ix = 0; ix < count; ++ix) {
                CFStringRef string = (CFStringRef)CFArrayGetValueAtIndex(value, ix);
                if (CFGetTypeID(string) != CFStringGetTypeID()) {
                    CFReleaseNull(value);
                    break;
                }
            }
        } else {
            CFReleaseNull(value);
        }
    }

    return value;
}

CFStringRef SecTaskCopyApplicationIdentifier(SecTaskRef task) {
    // Catalyst apps may have the iOS style application identifier.
    CFStringRef result = SecTaskCopyStringForEntitlement(task,
                                                         kSecEntitlementBasicApplicationIdentifier);
    if (!result) {
        result = SecTaskCopyStringForEntitlement(task,
                                                 kSecEntitlementAppleApplicationIdentifier);
    }
    return result;
}

#if TARGET_OS_IOS
CFArrayRef SecTaskCopySharedWebCredentialDomains(SecTaskRef task) {
    return SecTaskCopyArrayOfStringsForEntitlement(task,
                                                   kSecEntitlementAssociatedDomains);
}
#endif

CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task)
{
    CFMutableArrayRef groups = NULL;

    bool onDemandInstallable = SecTaskGetBooleanValueForEntitlement(task, kSystemEntitlementOnDemandInstallCapable);

    CFArrayRef keychainAccessGroups, appleSecurityApplicationGroups;
    CFStringRef appID;
    CFArrayRef associatedAppIDs;

    keychainAccessGroups = SecTaskCopyArrayOfStringsForEntitlement(task, kSecEntitlementKeychainAccessGroups);
    appleSecurityApplicationGroups = SecTaskCopyArrayOfStringsForEntitlement(task, kSecEntitlementAppleSecurityApplicationGroups);
    appID = SecTaskCopyApplicationIdentifier(task);
    // Catalyst apps (may?) have this entitlement.
    associatedAppIDs = SecTaskCopyArrayOfStringsForEntitlement(task, kSecEntitlementAssociatedApplicationIdentifier);

    groups = CFArrayCreateMutableForCFTypes(NULL);

    if (keychainAccessGroups) {
        CFArrayAppendArray(groups, keychainAccessGroups, CFRangeMake(0, CFArrayGetCount(keychainAccessGroups)));
    }

#if TARGET_OS_OSX
    const bool entitlementsValidated = SecTaskEntitlementsValidated(task);
#else
    const bool entitlementsValidated = true;
#endif

    if (entitlementsValidated) {
        // If app-id or k-a-g are present but binary is not validated, just honor k-a-g.
        // Because AMFI would not allow client to run if it would have k-a-g entitlement
        // and not being properly signed. Assoc-app-id behaves similarly.
        if (associatedAppIDs) {
            CFArrayAppendAll(groups, associatedAppIDs);
        }
        if (appID) {
            CFArrayAppendValue(groups, appID);
        }
        if (appleSecurityApplicationGroups) {
            if (onDemandInstallable) {
                // This is perfectly legal for other functionality but not for keychain use
                secnotice("entitlements", "Ignoring \"%@\" because client is API-restricted", kSecEntitlementAppleSecurityApplicationGroups);
            } else {
                CFArrayAppendArray(groups, appleSecurityApplicationGroups, CFRangeMake(0, CFArrayGetCount(appleSecurityApplicationGroups)));
            }
        }
    } else {
        // Try to provide some hopefully helpful diagnostics for common failure cases.
        if (CFArrayGetCount(groups) == 0) {
            if (appID) {
                secwarning("Entitlement %@=%@ is ignored because of invalid application signature or incorrect provisioning profile",
                           kSecEntitlementApplicationIdentifier, appID);
            }
            if (appleSecurityApplicationGroups) {
                secwarning("Entitlement %@=%@ is ignored because of invalid application signature or incorrect provisioning profile",
                           kSecEntitlementAppleSecurityApplicationGroups, appleSecurityApplicationGroups);
            }
        }
    }

    // Do not allow to explicitly specify com.apple.token if token support is not allowed by feature flags.
    CFIndex index = CFArrayGetFirstIndexOfValue(groups, CFRangeMake(0, CFArrayGetCount(groups)), kSecAttrAccessGroupToken);
    if (index != kCFNotFound) {
        if (os_feature_enabled(CryptoTokenKit, UseTokens)) {
            // Make sure that com.apple.token is last one. This is because it is always read-only group and therefore updating keychain
            // operations without explicitly set kSecAttrAccessGroup attribute would always fail.
            CFArrayRemoveValueAtIndex(groups, index);
            CFArrayAppendValue(groups, kSecAttrAccessGroupToken);
        } else {
            secwarning("Keychain access group com.apple.token ignored, feature not available");
            CFArrayRemoveValueAtIndex(groups, index);
        }
    }

#if TARGET_OS_OSX
    /*
     * We would like to add implicit token access group always, but we avoid doing that in case that application
     * clearly intended to use non-smartcard functionality of keychain but messed up signing or provisioning. In this case,
     * we want to return -34018 (errSecMissingEntitlements) to help diagnosing the issue for the app authors and adding
     * implicit token access group would instead result in errSecItemNotFound.
     */
    bool entitlementsFailure = (CFArrayGetCount(groups) == 0 && appID != NULL);
    if (!entitlementsFailure) {
        bool addTokenGroup = os_feature_enabled(CryptoTokenKit, UseTokens);
        if (addTokenGroup && !CFArrayContainsValue(groups, CFRangeMake(0, CFArrayGetCount(groups)), kSecAttrAccessGroupToken)) {
            CFArrayAppendValue(groups, kSecAttrAccessGroupToken);
        }
    }
#endif

    CFReleaseNull(associatedAppIDs);
    CFReleaseNull(appID);
    CFReleaseNull(keychainAccessGroups);
    CFReleaseNull(appleSecurityApplicationGroups);

    return groups;
}

bool
SecTaskGetBooleanValueForEntitlement(SecTaskRef task, CFStringRef entitlement)
{
    CFTypeRef value = SecTaskCopyValueForEntitlement(task, entitlement, NULL);
    bool ok = false;

    if (value && CFBooleanGetTypeID() == CFGetTypeID(value)) {
        ok = CFBooleanGetValue(value);
    }
    CFReleaseNull(value);
    return ok;
}
