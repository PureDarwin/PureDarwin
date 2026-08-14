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

#include <TargetConditionals.h>
#if TARGET_OS_IOS
#include <MobileGestalt.h>
#endif

#import <os/feature_private.h>

#import "keychain/ot/OTConstants.h"
#import "utilities/debugging.h"

NSErrorDomain const OctagonErrorDomain = @"com.apple.security.octagon";

NSString* OTDefaultContext = @"defaultContext";
NSString* OTDefaultsDomain = @"com.apple.security.octagon";
NSString* OTDefaultsOctagonEnable = @"enable";

NSString* OTProtocolPairing = @"OctagonPairing";
NSString* OTProtocolPiggybacking = @"OctagonPiggybacking";

const char * OTTrustStatusChangeNotification = "com.apple.security.octagon.trust-status-change";

NSString* const CuttlefishErrorDomain = @"CuttlefishError";
NSString* const CuttlefishErrorRetryAfterKey = @"retryafter";

NSString* OTEscrowRecordPrefix = @"com.apple.icdp.record.";

// I don't recommend using this command, but it does describe the plist that will enable this feature:
//
//  defaults write /System/Library/FeatureFlags/Domain/Security octagon -dict Enabled -bool YES
//
static bool OctagonEnabledOverrideSet = false;
static bool OctagonEnabledOverride = false;

static bool OctagonRecoveryKeyEnabledOverrideSet = false;
static bool OctagonRecoveryKeyEnabledOverride = false;

static bool OctagonAuthoritativeTrustEnabledOverrideSet = false;
static bool OctagonAuthoritativeTrustEnabledOverride = false;

static bool OctagonSOSFeatureIsEnabledOverrideSet = false;
static bool OctagonSOSFeatureIsEnabledOverride = false;

static bool OctagonOptimizationIsEnabledOverrideSet = false;
static bool OctagonOptimizationIsEnabledOverride = false;

static bool OctagonEscrowRecordFetchIsEnabledOverrideSet = false;
static bool OctagonEscrowRecordFetchIsEnabledOverride = false;

static bool SecKVSOnCloudKitIsEnabledOverrideSet = false;
static bool SecKVSOnCloudKitIsEnabledOverride = false;

static bool SecErrorNestedErrorCappingIsEnabledOverrideSet = false;
static bool SecErrorNestedErrorCappingIsEnabledOverride = false;

bool OctagonIsEnabled(void)
{
    if(OctagonEnabledOverrideSet) {
        secnotice("octagon", "Octagon is %@ (overridden)", OctagonEnabledOverride ? @"enabled" : @"disabled");
        return OctagonEnabledOverride;
    }

    static bool octagonEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        octagonEnabled = os_feature_enabled(Security, octagon);
        secnotice("octagon", "Octagon is %@ (via feature flags)", octagonEnabled ? @"enabled" : @"disabled");
    });

    return octagonEnabled;
}

void OctagonSetIsEnabled(BOOL value)
{
    OctagonEnabledOverrideSet = true;
    OctagonEnabledOverride = value;
}

static bool OctagonOverridePlatformSOS = false;
static bool OctagonPlatformSOSOverrideValue = false;
static bool OctagonPlatformSOSUpgrade = false;

BOOL OctagonPlatformSupportsSOS(void)
{
    if(OctagonOverridePlatformSOS) {
        return OctagonPlatformSOSOverrideValue ? YES : NO;
    }
    
#if TARGET_OS_OSX
    return YES;
#elif TARGET_OS_IOS
    static bool isSOSCapable = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Only iPhones, iPads, and iPods support SOS.
        CFStringRef deviceClass = MGCopyAnswer(kMGQDeviceClass, NULL);

        isSOSCapable = deviceClass && (CFEqual(deviceClass, kMGDeviceClassiPhone) ||
                                       CFEqual(deviceClass, kMGDeviceClassiPad) ||
                                       CFEqual(deviceClass, kMGDeviceClassiPod));

        if(deviceClass) {
            CFRelease(deviceClass);
        } else {
            secerror("octagon: Unable to determine device class. Guessing SOS status as Not Supported");
            isSOSCapable = false;
        }

        secnotice("octagon", "SOS is %@ on this platform" , isSOSCapable ? @"supported" : @"not supported");
    });

    return isSOSCapable ? YES : NO;
#else
    return NO;
#endif
}

void OctagonSetPlatformSupportsSOS(BOOL value)
{
    OctagonPlatformSOSOverrideValue = value;
    OctagonOverridePlatformSOS = YES;
}

void OctagonSetSOSUpgrade(BOOL value)
{
    OctagonPlatformSOSUpgrade = value;
}

BOOL OctagonPerformSOSUpgrade()
{
    if(OctagonPlatformSOSUpgrade){
        return OctagonPlatformSOSUpgrade;
    }
    return os_feature_enabled(Security, octagonSOSupgrade);
}

BOOL OctagonRecoveryKeyIsEnabled(void)
{
    if(OctagonRecoveryKeyEnabledOverrideSet) {
        secnotice("octagon", "Octagon RecoveryKey is %@ (overridden)", OctagonRecoveryKeyEnabledOverride ? @"enabled" : @"disabled");
        return OctagonRecoveryKeyEnabledOverride;
    }

    static bool octagonRecoveryKeyEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        octagonRecoveryKeyEnabled = os_feature_enabled(Security, recoverykey);
        secnotice("octagon", "Octagon is %@ (via feature flags)", octagonRecoveryKeyEnabled ? @"enabled" : @"disabled");
    });

    return octagonRecoveryKeyEnabled;
}

void OctagonRecoveryKeySetIsEnabled(BOOL value)
{
    OctagonRecoveryKeyEnabledOverrideSet = true;
    OctagonRecoveryKeyEnabledOverride = value;
}


BOOL OctagonAuthoritativeTrustIsEnabled(void)
{
    if(OctagonAuthoritativeTrustEnabledOverrideSet) {
        secnotice("octagon", "Authoritative Octagon Trust is %@ (overridden)", OctagonAuthoritativeTrustEnabledOverride ? @"enabled" : @"disabled");
        return OctagonAuthoritativeTrustEnabledOverride;
    }

    static bool octagonAuthoritativeTrustEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        octagonAuthoritativeTrustEnabled = os_feature_enabled(Security, octagonTrust);
        secnotice("octagon", "Authoritative Octagon Trust is %@ (via feature flags)", octagonAuthoritativeTrustEnabled ? @"enabled" : @"disabled");
    });

    return octagonAuthoritativeTrustEnabled;
}

void OctagonAuthoritativeTrustSetIsEnabled(BOOL value)
{
    OctagonAuthoritativeTrustEnabledOverrideSet = true;
    OctagonAuthoritativeTrustEnabledOverride = value;
}

BOOL OctagonIsSOSFeatureEnabled(void)
{
    if(OctagonSOSFeatureIsEnabledOverrideSet) {
        secnotice("octagon", "SOS Feature is %@ (overridden)", OctagonSOSFeatureIsEnabledOverride ? @"enabled" : @"disabled");
        return OctagonSOSFeatureIsEnabledOverride;
    }

    static bool sosEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sosEnabled = os_feature_enabled(Security, EnableSecureObjectSync);
        secnotice("octagon", "SOS Feature is %@ (via feature flags)", sosEnabled ? @"enabled" : @"disabled");
    });

    return sosEnabled;
}

void OctagonSetSOSFeatureEnabled(BOOL value)
{
    OctagonSOSFeatureIsEnabledOverrideSet = true;
    OctagonSOSFeatureIsEnabledOverride = value;
}

//feature flag for enabling/disabling performance enhancements
BOOL OctagonIsOptimizationEnabled(void)
{
    if(OctagonOptimizationIsEnabledOverrideSet) {
        secnotice("octagon", "Octagon Optimization is %@ (overridden)", OctagonOptimizationIsEnabledOverride ? @"enabled" : @"disabled");
        return OctagonOptimizationIsEnabledOverride;
    }

    static bool optimizationEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        optimizationEnabled = os_feature_enabled(Security, OctagonOptimization);
        secnotice("octagon", "Octagon Optimization is %@ (via feature flags)", optimizationEnabled ? @"enabled" : @"disabled");
    });

    return optimizationEnabled;
}

void OctagonSetOptimizationEnabled(BOOL value)
{
    OctagonOptimizationIsEnabledOverrideSet = true;
    OctagonOptimizationIsEnabledOverride = value;
}


//feature flag for checking if escrow record fetching is enabled
BOOL OctagonIsEscrowRecordFetchEnabled(void)
{
    if(OctagonEscrowRecordFetchIsEnabledOverrideSet) {
        secnotice("octagon", "Octagon Escrow Record Fetching is %@ (overridden)", OctagonEscrowRecordFetchIsEnabledOverride ? @"enabled" : @"disabled");
        return OctagonEscrowRecordFetchIsEnabledOverride;
    }

    static bool escrowRecordFetchingEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        escrowRecordFetchingEnabled = os_feature_enabled(Security, OctagonEscrowRecordFetch);
        secnotice("octagon", "Octagon Escrow Record Fetching is %@ (via feature flags)", escrowRecordFetchingEnabled ? @"enabled" : @"disabled");
    });

    return escrowRecordFetchingEnabled;
}

void OctagonSetEscrowRecordFetchEnabled(BOOL value)
{
    OctagonEscrowRecordFetchIsEnabledOverrideSet = true;
    OctagonEscrowRecordFetchIsEnabledOverride = value;
}

//feature flag for checking kvs on cloudkit enablement
BOOL SecKVSOnCloudKitIsEnabled(void)
{
    if(SecKVSOnCloudKitIsEnabledOverrideSet) {
        secnotice("octagon", "KVS on CloudKit is %@ (overridden)", SecKVSOnCloudKitIsEnabledOverride ? @"enabled" : @"disabled");
        return SecKVSOnCloudKitIsEnabledOverride;
    }

    static bool kvsOnCloudKitEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        kvsOnCloudKitEnabled = os_feature_enabled(KVS, KVSOnCloudKitForAll);
        secnotice("octagon", "KVS on CloudKit is %@ (via feature flags)", kvsOnCloudKitEnabled ? @"enabled" : @"disabled");
    });

    return kvsOnCloudKitEnabled;
}

void SecKVSOnCloudKitSetOverrideIsEnabled(BOOL value)
{
    SecKVSOnCloudKitIsEnabledOverrideSet = true;
    SecKVSOnCloudKitIsEnabledOverride = value;
}

//feature flag for checking whether or not we should cap the number of nested errors
bool SecErrorIsNestedErrorCappingEnabled(void)
{
    if(SecErrorNestedErrorCappingIsEnabledOverrideSet) {
        secnotice("octagon", "SecError Nested Error Capping is %@ (overridden)", SecErrorNestedErrorCappingIsEnabledOverride ? @"enabled" : @"disabled");
        return SecErrorNestedErrorCappingIsEnabledOverride;
    }

    static bool errorCappingEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorCappingEnabled = os_feature_enabled(Security, SecErrorNestedErrorCapping);
        secnotice("octagon", "SecError Nested Error Capping is %@ (via feature flags)", errorCappingEnabled ? @"enabled" : @"disabled");
    });

    return errorCappingEnabled;
}

void SecErrorSetOverrideNestedErrorCappingIsEnabled(BOOL value)
{
    SecErrorNestedErrorCappingIsEnabledOverrideSet = true;
    SecErrorNestedErrorCappingIsEnabledOverride = value;
}
