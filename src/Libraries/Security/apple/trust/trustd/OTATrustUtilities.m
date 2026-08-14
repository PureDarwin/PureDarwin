/*
 * Copyright (c) 2003-2018 Apple Inc. All Rights Reserved.
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
 *
 * OTATrustUtilities.m
 */

#import <Foundation/Foundation.h>
#include "OTATrustUtilities.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <copyfile.h>
#include <sys/syslimits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ftw.h>
#include "SecFramework.h"
#include <pthread.h>
#include <sys/param.h>
#include <stdlib.h>
#include <os/transaction_private.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecFramework.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <CommonCrypto/CommonDigest.h>
#include "trust/trustd/SecPinningDb.h"
#include "trust/trustd/trustdFileLocations.h"
#import <ipc/securityd_client.h>

#if !TARGET_OS_BRIDGE
#import <MobileAsset/MAAsset.h>
#import <MobileAsset/MAAssetQuery.h>
#include <notify.h>
#include <utilities/sec_action.h>
#include <utilities/SecFileLocations.h>
#import "trust/trustd/SecTrustLoggingServer.h"
#endif

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#import <MobileKeyBag/MobileKeyBag.h>
#include <System/sys/content_protection.h>
#endif

static inline bool isNSNumber(id nsType) {
    return nsType && [nsType isKindOfClass:[NSNumber class]];
}

static inline bool isNSDictionary(id nsType) {
    return nsType && [nsType isKindOfClass:[NSDictionary class]];
}

static inline bool isNSArray(id nsType) {
    return nsType && [nsType isKindOfClass:[NSArray class]];
}

static inline bool isNSDate(id nsType) {
    return nsType && [nsType isKindOfClass:[NSDate class]];
}

static inline bool isNSData(id nsType) {
    return nsType && [nsType isKindOfClass:[NSData class]];
}

dispatch_queue_t SecTrustServerGetWorkloop(void) {
    static dispatch_workloop_t workloop = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        workloop = dispatch_workloop_create("com.apple.trustd.evaluation");
    });
    return workloop;
}

/* MARK: - */
/* MARK: System Trust Store */
static CFStringRef kSecSystemTrustStoreBundlePath = CFSTR("/System/Library/Security/Certificates.bundle");

CFGiblisGetSingleton(CFBundleRef, SecSystemTrustStoreGetBundle, bundle,  ^{
    CFStringRef bundlePath = NULL;
#if TARGET_OS_SIMULATOR
    char *simulatorRoot = getenv("SIMULATOR_ROOT");
    if (simulatorRoot)
        bundlePath = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%@"), simulatorRoot, kSecSystemTrustStoreBundlePath);
#endif
    if (!bundlePath)
        bundlePath = CFRetainSafe(kSecSystemTrustStoreBundlePath);
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundlePath, kCFURLPOSIXPathStyle, true);
    *bundle = (url) ? CFBundleCreate(kCFAllocatorDefault, url) : NULL;
    CFReleaseSafe(url);
    CFReleaseSafe(bundlePath);
})

static CFURLRef SecSystemTrustStoreCopyResourceURL(CFStringRef resourceName,
                                                   CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = NULL;
    CFBundleRef bundle = SecSystemTrustStoreGetBundle();
    if (bundle) {
        url = CFBundleCopyResourceURL(bundle, resourceName,
                                      resourceType, subDirName);
    }
    if (!url) {
        secwarning("resource: %@.%@ in %@ not found", resourceName,
                   resourceType, subDirName);
    }
    return url;
}

static NSURL *SecSystemTrustStoreCopyResourceNSURL(NSString *resourceFileName) {
    CFBundleRef bundle = SecSystemTrustStoreGetBundle();
    if (!bundle) {
        return NULL;
    }
    NSURL *resourceDir = CFBridgingRelease(CFBundleCopyResourcesDirectoryURL(bundle));
    if (!resourceDir) {
        return NULL;
    }
    NSURL *fileURL = [NSURL URLWithString:resourceFileName
                            relativeToURL:resourceDir];
    if (!fileURL) {
        secwarning("resource: %@ not found", resourceFileName);
    }
    return fileURL;
}

static CFDataRef SecSystemTrustStoreCopyResourceContents(CFStringRef resourceName,
                                                         CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = SecSystemTrustStoreCopyResourceURL(resourceName, resourceType, subDirName);
    CFDataRef data = NULL;
    if (url) {
        SInt32 error;
        if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
                                                      url, &data, NULL, NULL, &error)) {
            secwarning("read: %ld", (long) error);
        }
        CFRelease(url);
    }
    return data;
}

/* MARK: - */
/* MARK: MobileAsset Updates */
// MARK: Forward Declarations
static uint64_t GetAssetVersion(CFErrorRef *error);
static uint64_t GetSystemVersion(CFStringRef key);
#if !TARGET_OS_BRIDGE
static BOOL UpdateFromAsset(NSURL *localURL, NSNumber *asset_version, NSError **error);
static NSNumber *SecExperimentUpdateAsset(MAAsset *asset, NSNumber *asset_version, NSError **error);
static void TriggerUnlockNotificationOTATrustAssetCheck(NSString* assetType, dispatch_queue_t queue);
#endif

/* This queue is for fetching changes to the OTAPKI reference or otherwise doing maintenance activities */
static dispatch_queue_t kOTABackgroundQueue = NULL;

// MARK: Constants
NSString *kOTATrustContentVersionKey = @"MobileAssetContentVersion";
NSString *kOTATrustLastCheckInKey = @"MobileAssetLastCheckIn";
NSString *kOTATrustContextFilename = @"OTAPKIContext.plist";
NSString *kOTATrustTrustedCTLogsFilename = @"TrustedCTLogs.plist";
NSString *kOTATrustTrustedCTLogsNonTLSFilename = @"TrustedCTLogs_nonTLS.plist";
NSString *kOTATrustAnalyticsSamplingRatesFilename = @"AnalyticsSamplingRates.plist";
NSString *kOTATrustAppleCertifcateAuthoritiesFilename = @"AppleCertificateAuthorities.plist";
NSString *kOTASecExperimentConfigFilename = @"SecExperimentAssets.plist";

/* A device will honor a kill switch until it gets a new asset xml that sets the kill switch value to 0/false
 * OR the asset is (completely) reset to shipping version. Such resets can happen if asset files cannot be
 * read properly or if the OS is updated and contains a newer asset version or pinning DB version. */
const CFStringRef kOTAPKIKillSwitchCT = CFSTR("CTKillSwitch");
const CFStringRef kOTAPKIKillSwitchNonTLSCT = CFSTR("CTKillSwitch_nonTLS");

#if !TARGET_OS_BRIDGE
NSString *OTATrustMobileAssetType = @"com.apple.MobileAsset.PKITrustSupplementals";
NSString *OTASecExperimentMobileAssetType = @"com.apple.MobileAsset.SecExperimentAssets";
#define kOTATrustMobileAssetNotification "com.apple.MobileAsset.PKITrustSupplementals.ma.cached-metadata-updated"
#define kOTATrustOnDiskAssetNotification "com.apple.trustd.asset-updated"
#define kOTATrustCheckInNotification "com.apple.trustd.asset-check-in"
#define kOTATrustKillSwitchNotification "com.apple.trustd.kill-switch"
#define kOTASecExperimentNewAssetNotification "com.apple.trustd.secexperiment.asset-updated"
const NSUInteger OTATrustMobileAssetCompatibilityVersion = 1;
const NSUInteger OTASecExperimentMobileAssetCompatibilityVersion = 1;
#define kOTATrustDefaultUpdatePeriod 60*60*12 // 12 hours
#define kOTATrustMinimumUpdatePeriod 60*5     // 5 min
#define kOTATrustDefaultWaitPeriod 60         // 1 min

#if TARGET_OS_OSX
const CFStringRef kSecSUPrefDomain = CFSTR("com.apple.SoftwareUpdate");
const CFStringRef kSecSUScanPrefConfigDataInstallKey = CFSTR("ConfigDataInstall");
#endif

// MARK: Helper functions
typedef enum {
    OTATrustLogLevelNone,
    OTATrustLogLevelDebug,
    OTATrustLogLevelInfo,
    OTATrustLogLevelNotice,
    OTATrustLogLevelError,
} OTATrustLogLevel;

static void MakeOTATrustError(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode, NSString *format,...) NS_FORMAT_FUNCTION(6,7);
static void MakeOTATrustErrorWithAttributes(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode,
                                            NSDictionary *attributes, NSString *format,...)
    NS_FORMAT_FUNCTION(7,8);

static void MakeOTATrustErrorArgs(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode,
                                  NSDictionary *attributes, NSString *format, va_list arguments)
    NS_FORMAT_FUNCTION(7,0);

static void LogLocally(OTATrustLogLevel level, NSString *errorString) {
    switch (level) {
        case OTATrustLogLevelNone:
            break;
        case OTATrustLogLevelDebug:
            secdebug("OTATrust", "%@", errorString);
            break;
        case OTATrustLogLevelInfo:
            secinfo("OTATrust", "%@", errorString);
            break;
        case OTATrustLogLevelNotice:
            secnotice("OTATrust", "%@", errorString);
            break;
        case OTATrustLogLevelError:
            secerror("OTATrust: %@", errorString);
            break;
    }
}

static void LogRemotelyWithAttributes(OTATrustLogLevel level, NSError **error, NSDictionary *attributes) {
#if ENABLE_TRUSTD_ANALYTICS
    /* only report errors and notices */
    if (error && level == OTATrustLogLevelError) {
        [[TrustAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventOTAPKIEvent hardFailure:YES result:*error withAttributes:attributes];
    } else if (error && level == OTATrustLogLevelNotice) {
        [[TrustAnalytics logger] logResultForEvent:TrustdHealthAnalyticsEventOTAPKIEvent hardFailure:NO result:*error withAttributes:attributes];
    }
#endif // ENABLE_TRUSTD_ANALYTICS
}

static void LogRemotely(OTATrustLogLevel level, NSError **error) {
    LogRemotelyWithAttributes(level, error, nil);
}

static void MakeOTATrustErrorArgs(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode,
                                  NSDictionary *attributes, NSString *format, va_list args) {
    NSString *formattedString = nil;
    if (format) {
        formattedString = [[NSString alloc] initWithFormat:format arguments:args];
    }

    NSError *localError = nil;
    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] init];
    if (format) {
        [userInfo setObject:formattedString forKey:NSLocalizedDescriptionKey];
    }
    if (error && *error) {
        userInfo[NSUnderlyingErrorKey] = *error;
    }
    localError = [NSError errorWithDomain:errDomain
                                     code:errCode
                                 userInfo:userInfo];

    LogLocally(level, formattedString);
    if ([assetType isEqualToString:OTATrustMobileAssetType]) {
        LogRemotelyWithAttributes(level, &localError, attributes);
    }
    if (error) { *error = localError; }
}

static void MakeOTATrustErrorWithAttributes(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode,
                                            NSDictionary *attributes, NSString *format,...) {
    va_list args;
    va_start(args, format);
    MakeOTATrustErrorArgs(assetType, error, level, errDomain, errCode, attributes, format, args);
    va_end(args);
}

static void MakeOTATrustError(NSString *assetType, NSError **error, OTATrustLogLevel level, NSErrorDomain errDomain, OSStatus errCode, NSString *format,...) {
    va_list args;
    va_start(args, format);
    MakeOTATrustErrorArgs(assetType, error, level, errDomain, errCode, nil, format, args);
    va_end(args);
}

static BOOL CanCheckMobileAsset(void) {
    BOOL result = YES;
#if TARGET_OS_OSX
    /* Check the user's SU preferences to determine if "Install system data files" is off */
    if (!CFPreferencesSynchronize(kSecSUPrefDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost)) {
        secerror("OTATrust: unable to synchronize SoftwareUpdate prefs");
        return NO;
    }

    id value = nil;
    if (CFPreferencesAppValueIsForced(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain)) {
        value = CFBridgingRelease(CFPreferencesCopyAppValue(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain));
    } else {
        value = CFBridgingRelease(CFPreferencesCopyValue(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain,
                                                         kCFPreferencesAnyUser, kCFPreferencesCurrentHost));
    }
    if (isNSNumber(value)) {
        result = [value boolValue];
    }

    if (!result) { secnotice("OTATrust", "User has disabled system data installation."); }

    /* MobileAsset.framework isn't mastered into the BaseSystem. Check that the MA classes are linked. */
    if (![ASAssetQuery class] || ![ASAsset class] || ![MAAssetQuery class] || ![MAAsset class]) {
        secnotice("OTATrust", "Weak-linked MobileAsset framework missing.");
        result = NO;
    }
#endif
    return result;
}

static BOOL ShouldUpdateWithAsset(NSString *assetType, NSNumber *asset_version) {
    if (![asset_version isKindOfClass:[NSNumber class]])  {
        return NO;
    }
    CFErrorRef error = nil;
    uint64_t current_version;
    if ([assetType isEqualToString:OTATrustMobileAssetType]) {
        current_version = SecOTAPKIGetCurrentAssetVersion(&error);
    } else if ([assetType isEqualToString:OTASecExperimentMobileAssetType]) {
        current_version = SecOTASecExperimentGetCurrentAssetVersion(&error);
    } else {
        return NO;
    }
    if (error) {
        CFReleaseNull(error);
        return NO;
    }
    if ([asset_version compare:[NSNumber numberWithUnsignedLongLong:current_version]] == NSOrderedDescending) {
        return YES;
    }
    return NO;
}

// MARK: File management functions
static bool verify_create_path(const char *path) {
    if (SecOTAPKIIsSystemTrustd()) {
        int ret = mkpath_np(path, 0755);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
            return false;
        }
        chmod(path, 0755);
    }
    return true;
}

static NSURL *GetAssetFileURL(NSString *filename) {
    NSURL *directory = CFBridgingRelease(SecCopyURLForFileInProtectedTrustdDirectory(CFSTR("SupplementalsAssets")));
    if (!verify_create_path([directory fileSystemRepresentation])) {
        return nil;
    }

    if (filename) {
        return [directory URLByAppendingPathComponent:filename];
    } else {
        return directory;
    }
}

static void DeleteFileWithName(NSString *filename) {
    NSURL *fileURL = GetAssetFileURL(filename);
    if (remove([fileURL fileSystemRepresentation]) == -1) {
        int error = errno;
        if (error != ENOENT) {
            secnotice("OTATrust", "failed to remove %@: %s", fileURL, strerror(error));
        }
    }
}

static BOOL UpdateOTAContextOnDisk(NSString *key, id value, NSError **error) {
    if (SecOTAPKIIsSystemTrustd()) {
        /* Get current context, if applicable, and update/add key/value */
        NSURL *otaContextFile = GetAssetFileURL(kOTATrustContextFilename);
        NSDictionary *currentContext = nil;
        if (otaContextFile) {
            currentContext = [NSDictionary dictionaryWithContentsOfURL:otaContextFile];
        } else {
            return NO;
        }
        NSMutableDictionary *newContext = nil;
        if (currentContext) {
            newContext = [currentContext mutableCopy];
        } else {
            newContext = [NSMutableDictionary dictionary];
        }
        newContext[key] = value;

        /* Write dictionary to disk */
        if (![newContext writeToClassDURL:otaContextFile permissions:0644 error:error]) {
            secerror("OTATrust: unable to write OTA Context to disk: %@", error ? *error : nil);
            LogRemotely(OTATrustLogLevelError, error);
            return NO;
        }
        return YES;
    }
    return NO;
}

static BOOL UpdateOTAContext(NSNumber *asset_version, NSError **error) {
    return UpdateOTAContextOnDisk(kOTATrustContentVersionKey, asset_version, error) && UpdateOTACheckInDate();
}

/* Delete only the asset data but not the check-in time. */
static void DeleteOldAssetData(void) {
    if (SecOTAPKIIsSystemTrustd()) {
        /* Delete the asset files, but keep the check-in time and version */
        DeleteFileWithName(kOTATrustTrustedCTLogsFilename);
        DeleteFileWithName(kOTATrustTrustedCTLogsNonTLSFilename);
        DeleteFileWithName(kOTATrustAnalyticsSamplingRatesFilename);
        DeleteFileWithName(kOTATrustAppleCertifcateAuthoritiesFilename);
    }
}

/* Delete all asset data, intended for error cases */
static BOOL DeleteAssetFromDisk(void) {
    if (SecOTAPKIIsSystemTrustd()) {
        DeleteOldAssetData();
        DeleteFileWithName(kOTATrustContextFilename);
        return YES;
    }
    return NO;
}

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static bool ChangeFileProtectionToClassD(NSURL *fileURL, NSError **error) {
    BOOL result = YES;
    int file_fd = open([fileURL fileSystemRepresentation], O_RDONLY);
    if (file_fd) {
        int retval = fcntl(file_fd, F_SETPROTECTIONCLASS, PROTECTION_CLASS_D);
        if (retval < 0) {
            MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSPOSIXErrorDomain, errno,
                              @"set proteciton class error for asset %d: %s", errno, strerror(errno));
            result = NO;
        }
        close(file_fd);
    } else {
        MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSPOSIXErrorDomain, errno,
                          @"open error for asset %d: %s", errno, strerror(errno));
        result = NO;
    }
    return result;
}
#endif

static bool ChangeFilePermissions(NSURL *fileURL, mode_t permissions, NSError **error) {
    const char *path = [fileURL fileSystemRepresentation];
    int ret = chmod(path, permissions);
    if (!(ret == 0)) {
        int localErrno = errno;
        secerror("failed to change permissions of %s: %s", path, strerror(localErrno));
        MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSPOSIXErrorDomain, errno,
                          @"failed to change permissions of %s: %s", path, strerror(localErrno));
        return NO;
    }
    return YES;
}

static BOOL CopyFileToDisk(NSString *filename, NSURL *localURL, NSError **error) {
    if (SecOTAPKIIsSystemTrustd()) {
        NSURL *toFileURL = GetAssetFileURL(filename);
        secdebug("OTATrust", "will copy asset file data from \"%@\"", localURL);
        copyfile_state_t state = copyfile_state_alloc();
        int retval = copyfile([localURL fileSystemRepresentation], [toFileURL fileSystemRepresentation],
                              state, COPYFILE_DATA);
        copyfile_state_free(state);
        if (retval < 0) {
            MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSPOSIXErrorDomain, errno,
                              @"copyfile error for asset %d: %s", errno, strerror(errno));
            return NO;
        } else {
            /* make sure all processes can read this file before first unlock */
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
            return ChangeFilePermissions(toFileURL, 0644, error) && ChangeFileProtectionToClassD(toFileURL, error);
#else
            return ChangeFilePermissions(toFileURL, 0644, error);
#endif
        }
    }
    return NO;
}

static void DisableKillSwitches() {
    UpdateOTAContextOnDisk((__bridge NSString*)kOTAPKIKillSwitchCT, @0, nil);
    UpdateOTAContextOnDisk((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT, @0, nil);
}

static void GetKillSwitchAttributes(NSDictionary *attributes) {
    bool killSwitchEnabled = false;

    // CT Kill Switch
    NSNumber *ctKillSwitch = [attributes objectForKey:(__bridge NSString*)kOTAPKIKillSwitchCT];
    if (isNSNumber(ctKillSwitch)) {
        NSError *error = nil;
        UpdateOTAContextOnDisk((__bridge NSString*)kOTAPKIKillSwitchCT, ctKillSwitch, &error);
        UpdateKillSwitch((__bridge NSString*)kOTAPKIKillSwitchCT, [ctKillSwitch boolValue]);
        secnotice("OTATrust", "got CT kill switch = %d", [ctKillSwitch boolValue]);
        killSwitchEnabled = true;
    }

    // Non-TLS CT Kill Switch
    ctKillSwitch = [attributes objectForKey:(__bridge NSString*)kOTAPKIKillSwitchNonTLSCT];
    if (isNSNumber(ctKillSwitch)) {
        NSError *error = nil;
        UpdateOTAContextOnDisk((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT, ctKillSwitch, &error);
        UpdateKillSwitch((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT, [ctKillSwitch boolValue]);
        secnotice("OTATrust", "got non-TLS CT kill switch = %d", [ctKillSwitch boolValue]);
        killSwitchEnabled = true;
    }

    /* Other kill switches TBD.
     * When adding one, make sure to add to the Analytics Samplers since these kill switches
     * are installed before the full asset is downloaded and installed. (A device can have the
     * kill switches without having the asset version that contained them.) */

    // notify the other trustds if any kill switch was read
    if (SecOTAPKIIsSystemTrustd() && killSwitchEnabled) {
        notify_post(kOTATrustKillSwitchNotification);
    }
}

// MARK: Fetch and Update Functions
static NSNumber *PKIUpdateAndPurgeAsset(MAAsset *asset, NSNumber *asset_version, NSError **error) {
    if (SecPinningDbUpdateFromURL([asset getLocalFileUrl], error) &&
        UpdateFromAsset([asset getLocalFileUrl], asset_version, error)) {
        secnotice("OTATrust", "finished update to version %@ from installed asset. purging asset.", asset_version);
#if ENABLE_TRUSTD_ANALYTICS
        [[TrustAnalytics logger] logSuccessForEventNamed:TrustdHealthAnalyticsEventOTAPKIEvent];
#endif // ENABLE_TRUSTD_ANALYTICS
        [asset purge:^(MAPurgeResult purge_result) {
            if (purge_result != MAPurgeSucceeded) {
                secerror("OTATrust: purge failed: %ld", (long)purge_result);
            }
         }];
        return asset_version;
    } else {
        MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecCallbackFailed,
                          @"Failed to install new asset version %@ from %@", asset_version, [asset getLocalFileUrl]);
        return nil;
    }
}

static NSNumber *UpdateAndPurgeAsset(NSString* assetType, MAAsset *asset, NSNumber *asset_version, NSError **error) {
    if ([assetType isEqualToString:OTATrustMobileAssetType]) {
        return PKIUpdateAndPurgeAsset(asset, asset_version, error);
    } else if ([assetType isEqualToString:OTASecExperimentMobileAssetType]) {
        return SecExperimentUpdateAsset(asset, asset_version, error);
    } else {
        return nil;
    }
}

static MADownloadOptions *GetMADownloadOptions(BOOL wait) {
    /* default behavior */
    MADownloadOptions *options = [[MADownloadOptions alloc] init];
    options.discretionary = YES;
    options.allowsCellularAccess = NO;

    /* If an XPC interface is waiting on this, all expenses allowed */
    if (wait) {
        options.discretionary = NO;
        options.allowsCellularAccess = YES;
        return options;
    }

    /* If last asset check-in was too long ago, use more expensive options */
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (!SecOTAPKIAssetStalenessLessThanSeconds(otapkiref, kSecOTAPKIAssetStalenessWarning)) {
        secnotice("OTATrust", "Asset staleness state: warning");
        options.allowsCellularAccess = YES;
        options.discretionary = NO;
    } else if (!SecOTAPKIAssetStalenessLessThanSeconds(otapkiref, kSecOTAPKIAssetStalenessAtRisk)) {
        secnotice("OTATrust", "Asset staleness state: at risk");
        options.discretionary = NO;
    }
    CFReleaseNull(otapkiref);
    return options;
}

static BOOL assetVersionCheck(NSString *assetType, MAAsset *asset) {
    NSUInteger compatVersion;
    NSError *ma_error = nil;
    if ([assetType isEqualToString:OTATrustMobileAssetType]) {
        compatVersion = OTATrustMobileAssetCompatibilityVersion;
    } else {
        compatVersion = OTASecExperimentMobileAssetCompatibilityVersion;
    }
    /* Check Compatibility Version against this software version */
    NSNumber *compatibilityVersion = [asset assetProperty:@"_CompatibilityVersion"];
    if (!isNSNumber(compatibilityVersion) ||
        [compatibilityVersion unsignedIntegerValue] != compatVersion) {
        MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecIncompatibleVersion,
                          @"skipping asset %@ because Compatibility Version doesn't match %@", assetType, compatibilityVersion);
        return NO;
    }
    /* Check Content Version against the current content version */
    NSNumber *asset_version = [asset assetProperty:@"_ContentVersion"];
    if (!ShouldUpdateWithAsset(assetType, asset_version)) {
        /* write the version and last (successful) check-in time */
        if ([assetType isEqualToString:OTATrustMobileAssetType]) {
            UpdateOTAContext(asset_version, &ma_error);
            NSDictionary *eventAttributes = @{
                                              @"assetVersion" : asset_version,
                                              @"systemVersion" : @(GetSystemVersion((__bridge CFStringRef)kOTATrustContentVersionKey)),
                                              @"installedVersion" : @(SecOTAPKIGetCurrentAssetVersion(nil)),
                                              };
            MakeOTATrustErrorWithAttributes(assetType, &ma_error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecDuplicateItem, eventAttributes,
                                            @"skipping asset %@ because we already have _ContentVersion %@ (or newer)", assetType, asset_version);
        } else {
            MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecDuplicateItem,
                              @"skipping asset %@ because we already have _ContentVersion %@ (or newer)", assetType, asset_version);
        }
        return NO;
    }
    return YES;
}

static int downloadWaitTime(void) {
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
    NSNumber *updateTimeout = [defaults valueForKey:@"TrustdAssetDownloadWaitTimeout"];
    int timeout = kOTATrustDefaultWaitPeriod;
    if (isNSNumber(updateTimeout)) {
        timeout = [updateTimeout intValue];
    }
    return timeout;
}

static BOOL DownloadOTATrustAsset(BOOL isLocalOnly, BOOL wait, NSString *assetType, NSError **error) {
    if (!CanCheckMobileAsset()) {
        MakeOTATrustError(assetType, error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecServiceNotAvailable,
                         @"MobileAsset disabled, skipping check.");
        return NO;
    }

    __block NSNumber *updated_version = nil;
    __block dispatch_semaphore_t done = wait ? dispatch_semaphore_create(0) : nil;
    __block NSError *ma_error = nil;

    secnotice("OTATrust", "begin MobileAsset query for catalog %@", assetType);
    [MAAsset startCatalogDownload:assetType options:GetMADownloadOptions(wait) then:^(MADownLoadResult result) {
        @autoreleasepool {
            os_transaction_t transaction = os_transaction_create("com.apple.trustd.asset.download");
            if (result != MADownloadSuccessful) {
                MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, @"MADownLoadResult", (OSStatus)result,
                                  @"failed to download catalog for asset %@: %ld", assetType, (long)result);
                if (result == MADownloadDaemonNotReady && ([assetType isEqualToString:OTATrustMobileAssetType])) {
                    /* mobileassetd has to wait for first unlock to download. trustd usually launches before first unlock. */
                    TriggerUnlockNotificationOTATrustAssetCheck(assetType, kOTABackgroundQueue);
                }
                return;
            }
            MAAssetQuery *query = [[MAAssetQuery alloc] initWithType:(NSString *)assetType];
            [query augmentResultsWithState:true];

            secnotice("OTATrust", "begin MobileAsset metadata sync request %{public}@", assetType);
            MAQueryResult queryResult = [query queryMetaDataSync];
            if (queryResult != MAQuerySuccessful) {
                MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, @"MAQueryResult", (OSStatus)queryResult,
                                  @"failed to query MobileAsset %@ metadata: %ld", assetType, (long)queryResult);
                return;
            }

            if (!query.results) {
                MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecInternal,
                                  @"no results in MobileAsset query for %@", assetType);
                return;
            }

            bool began_async_job = false;
            for (MAAsset *asset in query.results) {
                /* Check Compatibility Version against this software version */
                NSNumber *asset_version = [asset assetProperty:@"_ContentVersion"];
                if (!assetVersionCheck(assetType, asset)) {
                    continue;
                }

                if ([assetType isEqualToString:OTATrustMobileAssetType]) {
                    GetKillSwitchAttributes(asset.attributes);
                }

                switch (asset.state) {
                    default:
                        MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecInternal,
                                          @"unknown asset state %ld", (long)asset.state);
                        continue;
                    case MAInstalled:
                        /* The asset is already in the cache, get it from disk. */
                        secdebug("OTATrust", "OTATrust asset %{public}@ already installed", assetType);
                        updated_version = UpdateAndPurgeAsset(assetType, asset, asset_version, &ma_error);
                        break;
                    case MAUnknown:
                        MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, @"MAAssetState", (OSStatus)asset.state,
                                          @"asset %@ is unknown", assetType);
                        continue;
                    case MADownloading:
                        secnotice("OTATrust", "asset %{public}@ is downloading", assetType);
                        /* fall through */
                    case MANotPresent:
                        secnotice("OTATrust", "begin download of OTATrust asset");
                        began_async_job = true;
                        [asset startDownload:GetMADownloadOptions(wait) then:^(MADownLoadResult downloadResult) {
                            @autoreleasepool {
                                os_transaction_t inner_transaction = os_transaction_create("com.apple.trustd.asset.downloadAsset");
                                if (downloadResult != MADownloadSuccessful) {
                                    MakeOTATrustError(assetType, &ma_error, OTATrustLogLevelError, @"MADownLoadResult", (OSStatus)downloadResult,
                                                      @"failed to download asset %@: %ld", assetType, (long)downloadResult);
                                    return;
                                }
                                updated_version = UpdateAndPurgeAsset(assetType, asset, asset_version, &ma_error);
                                if (wait) {
                                    dispatch_semaphore_signal(done);
                                }
                                (void)inner_transaction; // dead store
                                inner_transaction = nil;
                            }
                        }];
                        break;
                } /* switch (asset.state) */
            } /* for (MAAsset.. */
            if (wait && !began_async_job) {
                dispatch_semaphore_signal(done);
            }
            /* Done with the transaction */
            (void)transaction; // dead store
            transaction = nil;
        } /* autoreleasepool */
     }]; /* [MAAsset startCatalogDownload: ] */

    /* If the caller is waiting for a response, wait up to one minute for the update to complete.
     * If the MAAsset callback does not complete in that time, report a timeout.
     * If the MAAsset callback completes and did not successfully update, it should report an error;
     * forward that error to the caller.
     * If the MAAsset callback completes and did not update and did not provide an error; report
     * an unknown error. */
    BOOL result = NO;
    if (wait) {
        if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * downloadWaitTime())) != 0) {
            MakeOTATrustError(assetType, error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecNetworkFailure,
                              @"Failed to get asset %@ metadata within %d seconds.", assetType, downloadWaitTime());
        } else {
            result = (updated_version != nil);
            if (error && ma_error) {
                *error = ma_error;
            } else if (!result) {
                MakeOTATrustError(assetType, error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecInternalComponent,
                                  @"Unknown error occurred.");
            }
        }
    }
    return result;
}

static void TriggerUnlockNotificationOTATrustAssetCheck(NSString* assetType, dispatch_queue_t queue) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    /* If the last check-in is recent enough, wait for our regularly scheduled check-in. */
    if (SecOTAPKIAssetStalenessLessThanSeconds(otapkiref, kSecOTAPKIAssetStalenessAtRisk)) {
        CFReleaseNull(otapkiref);
        return;
    }
    CFReleaseNull(otapkiref);
#if !TARGET_OS_SIMULATOR
    /* register for unlock notifications */
    int out_token = 0;
    notify_register_dispatch(kMobileKeyBagLockStatusNotificationID, &out_token, queue, ^(int token) {
        secnotice("OTATrust", "Got lock status notification for at-risk last check-in after MA daemon error");
        (void)DownloadOTATrustAsset(NO, NO, assetType, nil);
        notify_cancel(token);
    });
#endif
}

static bool InitializeKillSwitch(NSString *key) {
#if !TARGET_OS_BRIDGE
    NSError *error = nil;
    NSDictionary *OTAPKIContext = [NSDictionary dictionaryWithContentsOfURL:GetAssetFileURL(kOTATrustContextFilename) error:&error];
    if (isNSDictionary(OTAPKIContext)) {
        NSNumber *killSwitchValue = OTAPKIContext[key];
        if (isNSNumber(killSwitchValue)) {
            secinfo("OTATrust", "found on-disk kill switch %{public}@ with value %d", key, [killSwitchValue boolValue]);
            return [killSwitchValue boolValue];
        } else {
            MakeOTATrustError(OTATrustMobileAssetType, &error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecInvalidValue,
                              @"OTAContext.plist missing check-in");
        }
    } else {
        MakeOTATrustError(OTATrustMobileAssetType, &error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecMissingValue,
                          @"OTAContext.plist missing dictionary");
    }
#endif
    return false;
}

static void InitializeOTATrustAsset(dispatch_queue_t queue) {
    /* Only the "system" trustd does updates */
    if (SecOTAPKIIsSystemTrustd()) {
        /* Asynchronously ask MobileAsset for most recent asset. */
        dispatch_async(queue, ^{
            secnotice("OTATrust", "Initial check with MobileAsset for newer PKITrustSupplementals asset");
            (void)DownloadOTATrustAsset(NO, NO, OTATrustMobileAssetType, nil);
        });

        /* Register for changes in our asset */
        if (CanCheckMobileAsset()) {
            int out_token = 0;
            notify_register_dispatch(kOTATrustMobileAssetNotification, &out_token, queue, ^(int __unused token) {
                secnotice("OTATrust", "Got notification about a new PKITrustSupplementals asset from mobileassetd.");
                (void)DownloadOTATrustAsset(YES, NO, OTATrustMobileAssetType, nil);
            });
        }
    } else {
        /* Register for changes signaled by the system trustd */
        secnotice("OTATrust", "Initializing listener for PKI Asset changes from system trustd.");
        int out_token = 0;
        notify_register_dispatch(kOTATrustOnDiskAssetNotification, &out_token, queue, ^(int __unused token) {
            secnotice("OTATrust", "Got notification about a new PKITrustSupplementals asset from system trustd.");
            NSError *nserror = nil;
            CFErrorRef error = nil;
            NSNumber *asset_version = [NSNumber numberWithUnsignedLongLong:GetAssetVersion(&error)];
            if (error) {
                nserror = CFBridgingRelease(error);
            }
            if (!UpdateFromAsset(GetAssetFileURL(nil), asset_version, &nserror)) {
                secerror("OTATrust: failed to update from asset after notification: %@", nserror);
                /* Reset our last check-in time and reset the asset version to the system asset version -- even
                 * though we may be using something newer than that (but not as new as what's on disk). On re-launch
                 * (provided reading from disk still fails) we'd be using the system asset version anyway. */
                SecOTAPKIResetCurrentAssetVersion(&error);
            }
        });
        int out_token2 = 0;
        notify_register_dispatch(kOTATrustCheckInNotification, &out_token2, queue, ^(int __unused token) {
            secinfo("OTATrust", "Got notification about successful PKITrustSupplementals asset check-in");
            (void)UpdateOTACheckInDate();
        });
        int out_token3 = 0;
        notify_register_dispatch(kOTATrustKillSwitchNotification, &out_token3, queue, ^(int __unused token) {
            UpdateKillSwitch((__bridge NSString*)kOTAPKIKillSwitchCT, InitializeKillSwitch((__bridge NSString*)kOTAPKIKillSwitchCT));
            UpdateKillSwitch((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT, InitializeKillSwitch((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT));
        });
    }
}

static void InitializeOTASecExperimentAsset(dispatch_queue_t queue) {
    /* Only the "system" trustd does updates */
    if (SecOTAPKIIsSystemTrustd()) {
        /* Asynchronously ask MobileAsset for most recent asset. */
        dispatch_async(queue, ^{
            secnotice("OTATrust", "Initial check with MobileAsset for newer SecExperiment asset");
            (void)DownloadOTATrustAsset(NO, NO, OTASecExperimentMobileAssetType, nil);
        });
    } else {
        /* Register for changes signaled by the system trustd */
        secnotice("OTATrust", "Initializing listener for SecExperiment Asset changes from system trustd.");
        int out_token = 0;
        notify_register_dispatch(kOTASecExperimentNewAssetNotification, &out_token, queue, ^(int __unused token) {
            NSError *error = nil;
            secnotice("OTATrust", "Got notification about a new SecExperiment asset from system trustd.");
            MAAssetQuery *assetQuery = [[MAAssetQuery alloc] initWithType:OTASecExperimentMobileAssetType];
            [assetQuery returnTypes:MAInstalledOnly];
            MAQueryResult queryResult = [assetQuery queryMetaDataSync];

            if (queryResult != MAQuerySuccessful) {
                secerror("OTATrust: failed to update SecExperiment Asset after notification: %ld", (long)queryResult);
            } else {
                secnotice("OTATrust", "Updated SecExperiment asset successfully");
                for (MAAsset *asset in assetQuery.results) {
                    NSNumber *asset_version = [asset assetProperty:@"_ContentVersion"];
                    if (!assetVersionCheck(OTASecExperimentMobileAssetType, asset)) {
                        continue;
                    }
                    UpdateAndPurgeAsset(OTASecExperimentMobileAssetType, asset, asset_version, &error);
                }
            }
        });
    }
}

static void TriggerPeriodicOTATrustAssetChecks(dispatch_queue_t queue) {
    if (SecOTAPKIIsSystemTrustd()) {
        static sec_action_t action;
        static bool first_launch = true;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
            NSNumber *updateDeltas = [defaults valueForKey:@"PKITrustSupplementalsUpdatePeriod"];
            int delta = kOTATrustDefaultUpdatePeriod;
            if (isNSNumber(updateDeltas)) {
                delta = [updateDeltas intValue];
                if (delta < kOTATrustMinimumUpdatePeriod) {
                    delta = kOTATrustMinimumUpdatePeriod;
                }
            }
            secnotice("OTATrust", "Setting periodic update delta to %d seconds", delta);
            action = sec_action_create_with_queue(queue,"OTATrust", delta);
            sec_action_set_handler(action, ^{
                if (!first_launch) {
                    (void)DownloadOTATrustAsset(NO, NO, OTASecExperimentMobileAssetType, nil);
                    (void)DownloadOTATrustAsset(NO, NO, OTATrustMobileAssetType, nil);
                }
                first_launch = false;
            });
        });
        sec_action_perform(action);
    }
}
#endif /* !TARGET_OS_BRIDGE */

/* MARK: - */
/* MARK: Initialization functions */
static CFPropertyListRef CFPropertyListCopyFromSystem(CFStringRef asset) {
    CFPropertyListRef plist = NULL;
    CFDataRef xmlData = SecSystemTrustStoreCopyResourceContents(asset, CFSTR("plist"), NULL);

    if (xmlData) {
        plist = CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL, NULL);
        CFRelease(xmlData);
    }

    return plist;
}

static uint64_t GetSystemVersion(CFStringRef key) {
    uint64_t system_version = 0;
    int64_t asset_number = 0;

    CFDataRef assetVersionData = SecSystemTrustStoreCopyResourceContents(CFSTR("AssetVersion"), CFSTR("plist"), NULL);
    if (NULL != assetVersionData) {
        CFPropertyListFormat propFormat;
        CFDictionaryRef versionPlist =  CFPropertyListCreateWithData(kCFAllocatorDefault, assetVersionData, 0, &propFormat, NULL);
        if (NULL != versionPlist && CFDictionaryGetTypeID() == CFGetTypeID(versionPlist)) {
            CFNumberRef versionNumber = (CFNumberRef)CFDictionaryGetValue(versionPlist, (const void *)key);
            if (NULL != versionNumber){
                CFNumberGetValue(versionNumber, kCFNumberSInt64Type, &asset_number);
                if (asset_number < 0) { // Not valid
                    asset_number = 0;
                }
                system_version = (uint64_t)asset_number;
            }
        }
        CFReleaseSafe(versionPlist);
        CFReleaseSafe(assetVersionData);
    }

    return system_version;
}

static bool initialization_error_from_asset_data = false;

static bool ShouldInitializeWithAsset(void) {
    uint64_t system_version = GetSystemVersion((__bridge CFStringRef)kOTATrustContentVersionKey);
    uint64_t asset_version = GetAssetVersion(nil);

    if (asset_version > system_version) {
        secnotice("OTATrust", "Using asset v%llu instead of system v%llu", asset_version, system_version);
        return !initialization_error_from_asset_data;
    }
    return false;
}

static CFSetRef CFSetCreateFromPropertyList(CFPropertyListRef plist) {
    CFSetRef result = NULL;

    if (plist) {
        CFMutableSetRef tempSet = NULL;
        if (CFGetTypeID(plist) == CFArrayGetTypeID()) {
            tempSet = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
            if (NULL == tempSet) {
                return result;
            }
            CFArrayRef array = (CFArrayRef)plist;
            CFIndex num_keys = CFArrayGetCount(array);
            for (CFIndex idx = 0; idx < num_keys; idx++) {
                CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(array, idx);
                CFSetAddValue(tempSet, data);
            }
        } else {
            return result;
        }

        result = tempSet;
    }
    return result;
}

static CF_RETURNS_RETAINED CFSetRef InitializeBlackList() {
    CFPropertyListRef plist = CFPropertyListCopyFromSystem(CFSTR("Blocked"));
    CFSetRef result = CFSetCreateFromPropertyList(plist);
    CFReleaseSafe(plist);

    return result;
}

static CF_RETURNS_RETAINED CFSetRef InitializeGrayList() {
    CFPropertyListRef plist = CFPropertyListCopyFromSystem(CFSTR("GrayListedKeys"));
    CFSetRef result = CFSetCreateFromPropertyList(plist);
    CFReleaseSafe(plist);

    return result;
}

static CF_RETURNS_RETAINED CFURLRef InitializePinningList() {
    return SecSystemTrustStoreCopyResourceURL(CFSTR("CertificatePinning"), CFSTR("plist"), NULL);
}

static CF_RETURNS_RETAINED CFDictionaryRef InitializeAllowList() {
    CFPropertyListRef allowList = CFPropertyListCopyFromSystem(CFSTR("Allowed"));

    if (allowList && (CFGetTypeID(allowList) == CFDictionaryGetTypeID())) {
        return allowList;
    } else {
        CFReleaseNull(allowList);
        return NULL;
    }
}

static NSDictionary <NSData *, NSDictionary *> *ConvertTrustedCTLogsArrayToDictionary(NSArray *trustedLogsArray) {
    NSMutableDictionary <NSData *, NSDictionary *> *result = [NSMutableDictionary dictionaryWithCapacity:trustedLogsArray.count];
    [trustedLogsArray enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (!isNSDictionary(obj)) {
            secerror("OTATrust: failed to read entry from trusted CT logs array at index %lu", (unsigned long)idx);
            return;
        }
        NSDictionary *log_data = (NSDictionary *)obj;
        NSData *log_id = log_data[@"log_id"];
        if (!isNSData(log_id)) {
            secinfo("OTATrust", "failed to read log_id from trusted CT log array entry at index %lu, computing log_id", (unsigned long)idx);
            // We can make the log_id from the key
            NSData *key = log_data[@"key"];
            if (!isNSData(key)) {
                secerror("failed to read key from trusted CT log array entry at index %lu", (unsigned long)idx);
                return;
            }
            log_id = CFBridgingRelease(SecSHA256DigestCreateFromData(NULL, (__bridge CFDataRef)key));
        }
        [result setObject:log_data forKey:log_id];
    }];
    return result;
}

CFDictionaryRef SecOTAPKICreateTrustedCTLogsDictionaryFromArray(CFArrayRef trustedCTLogsArray)
{
    @autoreleasepool {
        return CFBridgingRetain(ConvertTrustedCTLogsArrayToDictionary((__bridge NSArray*)trustedCTLogsArray));
    }
}

static CF_RETURNS_RETAINED CFDictionaryRef InitializeTrustedCTLogs(NSString *filename) {
    @autoreleasepool {
        NSArray *trustedCTLogs = nil;
        NSError *error = nil;
#if !TARGET_OS_BRIDGE
        if (ShouldInitializeWithAsset()) {
            trustedCTLogs = [NSArray arrayWithContentsOfURL:GetAssetFileURL(filename) error:&error];
            if (!isNSArray(trustedCTLogs)) {
                secerror("OTATrust: failed to read CT list from asset data: %@", error);
                LogRemotely(OTATrustLogLevelError, &error);
                if (!DeleteAssetFromDisk()) {
                    initialization_error_from_asset_data = true;
                }
            }
        }
#endif
        if (!isNSArray(trustedCTLogs)) {
            trustedCTLogs = [NSArray arrayWithContentsOfURL:SecSystemTrustStoreCopyResourceNSURL(filename)];
        }
        if (isNSArray(trustedCTLogs)) {
            return CFBridgingRetain(ConvertTrustedCTLogsArrayToDictionary(trustedCTLogs));
        }
        return NULL;
    }
}

static CF_RETURNS_RETAINED CFDictionaryRef InitializeEVPolicyToAnchorDigestsTable() {
    CFDictionaryRef result = NULL;
    CFPropertyListRef evroots = CFPropertyListCopyFromSystem(CFSTR("EVRoots"));

    if (evroots) {
        if (CFGetTypeID(evroots) == CFDictionaryGetTypeID()) {
            /* @@@ Ensure that each dictionary key is a dotted list of digits,
             each value is an NSArrayRef and each element in the array is a
             20 byte digest. */
            result = (CFDictionaryRef)evroots;
        }
        else {
            secwarning("EVRoot.plist is wrong type.");
            CFRelease(evroots);
        }
    }

    return result;
}

static CFIndex InitializeValidSnapshotVersion(CFIndex *outFormat) {
    CFIndex validVersion = 0;
    CFIndex validFormat = 0;
    CFDataRef validVersionData = SecSystemTrustStoreCopyResourceContents(CFSTR("ValidUpdate"), CFSTR("plist"), NULL);
    if (NULL != validVersionData)
    {
        CFPropertyListFormat propFormat;
        CFDictionaryRef versionPlist =  CFPropertyListCreateWithData(kCFAllocatorDefault, validVersionData, 0, &propFormat, NULL);
        if (NULL != versionPlist && CFDictionaryGetTypeID() == CFGetTypeID(versionPlist))
        {
            CFNumberRef versionNumber = (CFNumberRef)CFDictionaryGetValue(versionPlist, (const void *)CFSTR("Version"));
            if (NULL != versionNumber)
            {
                CFNumberGetValue(versionNumber, kCFNumberCFIndexType, &validVersion);
            }
            CFNumberRef formatNumber = (CFNumberRef)CFDictionaryGetValue(versionPlist, (const void *)CFSTR("Format"));
            if (NULL != formatNumber)
            {
                CFNumberGetValue(formatNumber, kCFNumberCFIndexType, &validFormat);
            }
        }
        CFReleaseSafe(versionPlist);
        CFReleaseSafe(validVersionData);
    }
    if (outFormat) {
        *outFormat = validFormat;
    }
    return validVersion;
}

static Boolean PathExists(const char* path, size_t* pFileSize) {
    const char *checked_path = (path) ? path : "";
    Boolean result = false;
    struct stat         sb;

    if (NULL != pFileSize) {
        *pFileSize = 0;
    }

    int stat_result = stat(checked_path, &sb);
    result = (stat_result == 0);

    if (result && !S_ISDIR(sb.st_mode)) {
        // It is a file
        if (NULL != pFileSize) {
            *pFileSize = (size_t)sb.st_size;
        }
    }

    return result;
}

static const char* InitializeValidSnapshotData(CFStringRef filename_str) {
    char *result = NULL;
    const char *base_error_str = "could not get valid snapshot";

    CFURLRef valid_url = SecSystemTrustStoreCopyResourceURL(filename_str, CFSTR("sqlite3"), NULL);
    if (NULL == valid_url) {
        secerror("%s", base_error_str);
    } else {
        CFStringRef valid_str = CFURLCopyFileSystemPath(valid_url, kCFURLPOSIXPathStyle);
        char file_path_buffer[PATH_MAX];
        memset(file_path_buffer, 0, PATH_MAX);
        if (NULL == valid_str) {
            secerror("%s path", base_error_str);
        } else {
            const char *valid_cstr = CFStringGetCStringPtr(valid_str, kCFStringEncodingUTF8);
            if (NULL == valid_cstr) {
                if (CFStringGetCString(valid_str, file_path_buffer, PATH_MAX, kCFStringEncodingUTF8)) {
                    valid_cstr = file_path_buffer;
                }
            }
            if (NULL == valid_cstr) {
                secerror("%s path as UTF8 string", base_error_str);
            } else {
                asprintf(&result, "%s", valid_cstr);
            }
        }
        CFReleaseSafe(valid_str);
    }
    CFReleaseSafe(valid_url);
    if (result && !PathExists(result, NULL)) {
        free(result);
        result = NULL;
    }
    return (const char*)result;
}

static const char* InitializeValidDatabaseSnapshot() {
    return InitializeValidSnapshotData(CFSTR("valid"));
}

static const uint8_t* MapFile(const char* path, size_t* out_file_size) {
    int rtn, fd;
    const uint8_t *buf = NULL;
    struct stat    sb;
    size_t size = 0;

    if (NULL == path || NULL == out_file_size) {
        return NULL;
    }

    *out_file_size = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0) { return NULL; }
    rtn = fstat(fd, &sb);
    if (rtn || (sb.st_size > (off_t) ((UINT32_MAX >> 1)-1))) {
        close(fd);
        return NULL;
    }
    size = (size_t)sb.st_size;

    buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!buf || buf == MAP_FAILED) {
        secerror("unable to map %s (errno %d)", path, errno);
        close(fd);
        return NULL;
    }

    close(fd);
    *out_file_size = size;
    return buf;
}

static void UnMapFile(void* mapped_data, size_t data_size) {
    if (!mapped_data) {
        return;
    }
    int rtn = munmap(mapped_data, data_size);
    if (rtn != 0) {
        secerror("unable to unmap %ld bytes at %p (error %d)", data_size, mapped_data, rtn);
    }
}

struct index_record {
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    uint32_t offset;
};
typedef struct index_record index_record;

static bool InitializeAnchorTable(CFDictionaryRef* pLookupTable, const char** ppAnchorTable) {

    bool result = false;

    if (NULL == pLookupTable || NULL == ppAnchorTable) {
        return result;
    }

    *pLookupTable = NULL;
    *ppAnchorTable = NULL;;

    CFDataRef                cert_index_file_data = NULL;
    char                     file_path_buffer[PATH_MAX];
    CFURLRef                 table_data_url = NULL;
    CFStringRef                table_data_cstr_path = NULL;
    const char*                table_data_path = NULL;
    const index_record*     pIndex = NULL;
    size_t                  index_offset = 0;
    size_t                    index_data_size = 0;
    CFMutableDictionaryRef     anchorLookupTable = NULL;
    uint32_t                 offset_int_value = 0;
    CFNumberRef             index_offset_value = NULL;
    CFDataRef               index_hash = NULL;
    CFMutableArrayRef       offsets = NULL;
    Boolean                    release_offset = false;

    char* local_anchorTable = NULL;
    size_t local_anchorTableSize = 0;

    // local_anchorTable is still NULL so the asset in the system trust store bundle needs to be used.
    CFReleaseSafe(cert_index_file_data);
    cert_index_file_data = SecSystemTrustStoreCopyResourceContents(CFSTR("certsIndex"), CFSTR("data"), NULL);
    if (!cert_index_file_data) {
        secerror("could not find certsIndex");
    }
    table_data_url =  SecSystemTrustStoreCopyResourceURL(CFSTR("certsTable"), CFSTR("data"), NULL);
    if (!table_data_url) {
        secerror("could not find certsTable");
    }

    if (NULL != table_data_url) {
        table_data_cstr_path  = CFURLCopyFileSystemPath(table_data_url, kCFURLPOSIXPathStyle);
        if (NULL != table_data_cstr_path) {
            memset(file_path_buffer, 0, PATH_MAX);
            table_data_path = CFStringGetCStringPtr(table_data_cstr_path, kCFStringEncodingUTF8);
            if (NULL == table_data_path) {
                if (CFStringGetCString(table_data_cstr_path, file_path_buffer, PATH_MAX, kCFStringEncodingUTF8)) {
                    table_data_path = file_path_buffer;
                }
            }
            local_anchorTable  = (char *)MapFile(table_data_path, &local_anchorTableSize);
            CFReleaseSafe(table_data_cstr_path);
        }
    }
    CFReleaseSafe(table_data_url);

    if (NULL == local_anchorTable || NULL  == cert_index_file_data) {
        // we are in trouble
        if (NULL != local_anchorTable) {
            UnMapFile(local_anchorTable, local_anchorTableSize);
            local_anchorTable = NULL;
            local_anchorTableSize = 0;
        }
        CFReleaseSafe(cert_index_file_data);
        return result;
    }

    // ------------------------------------------------------------------------
    // Now that the locations of the files are known and the table file has
    // been mapped into memory, create a dictionary that maps the SHA1 hash of
    // normalized issuer to the offset in the mapped anchor table file which
    // contains a index_record to the correct certificate
    // ------------------------------------------------------------------------
    pIndex = (const index_record*)CFDataGetBytePtr(cert_index_file_data);
    index_data_size = CFDataGetLength(cert_index_file_data);

    anchorLookupTable = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);

    for (index_offset = index_data_size; index_offset > 0; index_offset -= sizeof(index_record), pIndex++) {
        offset_int_value = pIndex->offset;

        index_offset_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &offset_int_value);
        index_hash = CFDataCreate(kCFAllocatorDefault, pIndex->hash, CC_SHA1_DIGEST_LENGTH);

        // see if the dictionary already has this key
        release_offset = false;
        offsets = (CFMutableArrayRef)CFDictionaryGetValue(anchorLookupTable, index_hash);
        if (NULL == offsets) {
            offsets = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            release_offset = true;
        }

        // Add the offset
        CFArrayAppendValue(offsets, index_offset_value);

        // set the key value pair in the dictionary
        CFDictionarySetValue(anchorLookupTable, index_hash, offsets);

        CFRelease(index_offset_value);
        CFRelease(index_hash);
        if (release_offset) {
            CFRelease(offsets);
        }
    }

    CFRelease(cert_index_file_data);

    if (NULL != anchorLookupTable && NULL != local_anchorTable) {
        *pLookupTable = anchorLookupTable;
        *ppAnchorTable = local_anchorTable;
        result = true;
    } else {
        CFReleaseSafe(anchorLookupTable);
        if (NULL != local_anchorTable) {
            UnMapFile(local_anchorTable, local_anchorTableSize);
            local_anchorTable = NULL;
            local_anchorTableSize = 0;
        }
    }

    return result;
}

static void InitializeEscrowCertificates(CFArrayRef *escrowRoots, CFArrayRef *escrowPCSRoots) {
    CFDataRef file_data = SecSystemTrustStoreCopyResourceContents(CFSTR("AppleESCertificates"), CFSTR("plist"), NULL);

    if (NULL == file_data) {
        return;
    }

    CFPropertyListFormat propFormat;
    CFDictionaryRef certsDictionary =  CFPropertyListCreateWithData(kCFAllocatorDefault, file_data, 0, &propFormat, NULL);
    if (NULL != certsDictionary && CFDictionaryGetTypeID() == CFGetTypeID((CFTypeRef)certsDictionary)) {
        CFArrayRef certs = (CFArrayRef)CFDictionaryGetValue(certsDictionary, CFSTR("ProductionEscrowKey"));
        if (NULL != certs && CFArrayGetTypeID() == CFGetTypeID((CFTypeRef)certs) && CFArrayGetCount(certs) > 0) {
            *escrowRoots = CFArrayCreateCopy(kCFAllocatorDefault, certs);
        }
        CFArrayRef pcs_certs = (CFArrayRef)CFDictionaryGetValue(certsDictionary, CFSTR("ProductionPCSEscrowKey"));
        if (NULL != pcs_certs && CFArrayGetTypeID() == CFGetTypeID((CFTypeRef)pcs_certs) && CFArrayGetCount(pcs_certs) > 0) {
            *escrowPCSRoots = CFArrayCreateCopy(kCFAllocatorDefault, pcs_certs);
        }
    }
    CFReleaseSafe(certsDictionary);
    CFRelease(file_data);
}

static CF_RETURNS_RETAINED CFDictionaryRef InitializeEventSamplingRates() {
    NSDictionary *analyticsSamplingRates =  nil;
    NSDictionary *eventSamplingRates = nil;
    NSError *error = nil;
#if !TARGET_OS_BRIDGE
    if (ShouldInitializeWithAsset()) {
        analyticsSamplingRates = [NSDictionary dictionaryWithContentsOfURL:GetAssetFileURL(kOTATrustAnalyticsSamplingRatesFilename) error:&error];
        if (!isNSDictionary(analyticsSamplingRates)) {
            secerror("OTATrust: failed to read sampling rates from asset data: %@", error);
            LogRemotely(OTATrustLogLevelError, &error);
            if (!DeleteAssetFromDisk()) {
                initialization_error_from_asset_data = true;
            }
        }
        eventSamplingRates = analyticsSamplingRates[@"Events"];
    }
#endif
    if (!isNSDictionary(eventSamplingRates)) {
        analyticsSamplingRates = [NSDictionary dictionaryWithContentsOfURL:SecSystemTrustStoreCopyResourceNSURL(kOTATrustAnalyticsSamplingRatesFilename)];
    }
    if (isNSDictionary(analyticsSamplingRates)) {
        eventSamplingRates = analyticsSamplingRates[@"Events"];
        if (isNSDictionary(eventSamplingRates)) {
            return CFBridgingRetain(eventSamplingRates);
        }
    }
    return NULL;
}

static CF_RETURNS_RETAINED CFArrayRef InitializeAppleCertificateAuthorities() {
    NSArray *appleCAs = nil;
    NSError *error = nil;
#if !TARGET_OS_BRIDGE
    if (ShouldInitializeWithAsset()) {
        appleCAs = [NSArray arrayWithContentsOfURL:GetAssetFileURL(kOTATrustAppleCertifcateAuthoritiesFilename) error:&error];
        if (!isNSArray(appleCAs)) {
            secerror("OTATrust: failed to read Apple CAs list from asset data: %@", error);
            LogRemotely(OTATrustLogLevelError, &error);
            if (!DeleteAssetFromDisk()) {
                initialization_error_from_asset_data = true;
            }
        }
    }
#endif
    if (!isNSArray(appleCAs)) {
        appleCAs = [NSArray arrayWithContentsOfURL:SecSystemTrustStoreCopyResourceNSURL(kOTATrustAppleCertifcateAuthoritiesFilename)];
    }
    if (isNSArray(appleCAs)) {
        return CFBridgingRetain(appleCAs);
    }
    return NULL;
}

/* MARK: - */
/* MARK: SecOTA */

/* We keep track of one OTAPKI reference */
static SecOTAPKIRef kCurrentOTAPKIRef = NULL;
/* This queue is for making changes to the OTAPKI reference */
static dispatch_queue_t kOTAQueue = NULL;

struct _OpaqueSecOTAPKI {
    CFRuntimeBase       _base;
    CFSetRef            _blackListSet;
    CFSetRef            _grayListSet;
    CFDictionaryRef     _allowList;
    CFDictionaryRef     _trustedCTLogs;
    CFDictionaryRef     _nonTlsTrustedCTLogs;
    CFURLRef            _pinningList;
    CFArrayRef          _escrowCertificates;
    CFArrayRef          _escrowPCSCertificates;
    CFDictionaryRef     _evPolicyToAnchorMapping;
    CFDictionaryRef     _anchorLookupTable;
    const char*         _anchorTable;
    uint64_t            _trustStoreVersion;
    const char*         _validDatabaseSnapshot;
    CFIndex             _validSnapshotVersion;
    CFIndex             _validSnapshotFormat;
    uint64_t            _assetVersion;
    CFDateRef           _lastAssetCheckIn;
    CFDictionaryRef     _eventSamplingRates;
    CFArrayRef          _appleCAs;
    CFDictionaryRef     _secExperimentConfig;
    uint64_t            _secExperimentAssetVersion;
    bool                _ctKillSwitch;
    bool                _nonTlsCtKillSwitch;
};

CFGiblisFor(SecOTAPKI)

static CF_RETURNS_RETAINED CFStringRef SecOTAPKICopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecOTAPKIRef otapkiRef = (SecOTAPKIRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTAPKIRef: version %llu/%llu>"),
                                    otapkiRef->_trustStoreVersion, otapkiRef->_assetVersion);
}

static void SecOTAPKIDestroy(CFTypeRef cf) {
    SecOTAPKIRef otapkiref = (SecOTAPKIRef)cf;

    CFReleaseNull(otapkiref->_blackListSet);
    CFReleaseNull(otapkiref->_grayListSet);
    CFReleaseNull(otapkiref->_escrowCertificates);
    CFReleaseNull(otapkiref->_escrowPCSCertificates);

    CFReleaseNull(otapkiref->_evPolicyToAnchorMapping);
    CFReleaseNull(otapkiref->_anchorLookupTable);

    CFReleaseNull(otapkiref->_trustedCTLogs);
    CFReleaseNull(otapkiref->_nonTlsTrustedCTLogs);
    CFReleaseNull(otapkiref->_pinningList);
    CFReleaseNull(otapkiref->_eventSamplingRates);
    CFReleaseNull(otapkiref->_appleCAs);
    CFReleaseNull(otapkiref->_lastAssetCheckIn);
    CFReleaseNull(otapkiref->_secExperimentConfig);

    if (otapkiref->_anchorTable) {
        free((void *)otapkiref->_anchorTable);
        otapkiref->_anchorTable = NULL;
    }
    if (otapkiref->_validDatabaseSnapshot) {
        free((void *)otapkiref->_validDatabaseSnapshot);
        otapkiref->_validDatabaseSnapshot = NULL;
    }
}

static uint64_t GetSystemTrustStoreVersion(void) {
    return GetSystemVersion(CFSTR("VersionNumber"));
}

static uint64_t GetAssetVersion(CFErrorRef *error) {
    @autoreleasepool {
        /* Get system asset version */
        uint64_t version = GetSystemVersion((__bridge CFStringRef)kOTATrustContentVersionKey);

#if !TARGET_OS_BRIDGE
        uint64_t asset_version = 0;
        NSError *nserror = nil;
        NSDictionary *OTAPKIContext = [NSDictionary dictionaryWithContentsOfURL:GetAssetFileURL(kOTATrustContextFilename) error:&nserror];
        if (isNSDictionary(OTAPKIContext)) {
            NSNumber *tmpNumber = OTAPKIContext[kOTATrustContentVersionKey];
            if (isNSNumber(tmpNumber)) {
                asset_version = [tmpNumber unsignedLongLongValue];
            } else if (error) {
                MakeOTATrustError(OTATrustMobileAssetType, &nserror, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecInvalidValue,
                                  @"OTAContext.plist missing version");
            }
        } else if (error) {
            MakeOTATrustError(OTATrustMobileAssetType, &nserror, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecMissingValue,
                              @"OTAContext.plist missing dictionary");
        }

        if (asset_version > version) {
            return asset_version;
        } else {
            /* Don't delete the last check-in time so that we know we're up to date with the MobileAsset. */
            if (error) {
                /* only log this if we're tracking errors */
                secnotice("OTATrust", "asset (%llu) is not newer than the system version (%llu); deleting stale data", asset_version, version);
                *error = CFRetainSafe((__bridge CFErrorRef)nserror);
            }
            DeleteOldAssetData();
            DisableKillSwitches();
        }
#endif
        return version;
    }
}

static CF_RETURNS_RETAINED CFDateRef InitializeLastAssetCheckIn(void) {
#if !TARGET_OS_BRIDGE
    NSError *error = nil;
    NSDictionary *OTAPKIContext = [NSDictionary dictionaryWithContentsOfURL:GetAssetFileURL(kOTATrustContextFilename) error:&error];
    if (isNSDictionary(OTAPKIContext)) {
        NSDate *checkIn = OTAPKIContext[kOTATrustLastCheckInKey];
        if (isNSDate(checkIn)) {
            return CFRetainSafe((__bridge CFDateRef)checkIn);
        } else {
            MakeOTATrustError(OTATrustMobileAssetType, &error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecInvalidValue,
                              @"OTAContext.plist missing check-in");
        }
    } else {
        MakeOTATrustError(OTATrustMobileAssetType, &error, OTATrustLogLevelNotice, NSOSStatusErrorDomain, errSecMissingValue,
                          @"OTAContext.plist missing dictionary");
    }
#endif
    return NULL;
}

static SecOTAPKIRef SecOTACreate() {

    SecOTAPKIRef otapkiref = NULL;

    otapkiref = CFTypeAllocate(SecOTAPKI, struct _OpaqueSecOTAPKI , kCFAllocatorDefault);

    if (NULL == otapkiref) {
        return otapkiref;
    }

    // Make sure that if this routine has to bail that the clean up
    // will do the right thing
    memset(otapkiref, 0, sizeof(*otapkiref));

    // Start off by getting the trust store version
    otapkiref->_trustStoreVersion = GetSystemTrustStoreVersion();

    // Get the set of black listed keys
    CFSetRef blackKeysSet = InitializeBlackList();
    if (NULL == blackKeysSet) {
        CFReleaseNull(otapkiref);
        return otapkiref;
    }
    otapkiref->_blackListSet = blackKeysSet;

    // Get the set of gray listed keys
    CFSetRef grayKeysSet = InitializeGrayList();
    if (NULL == grayKeysSet) {
        CFReleaseNull(otapkiref);
        return otapkiref;
    }
    otapkiref->_grayListSet = grayKeysSet;

    // Get the allow list dictionary
    // (now loaded lazily in SecOTAPKICopyAllowList)

    // Get the trusted Certificate Transparency Logs
    otapkiref->_trustedCTLogs = InitializeTrustedCTLogs(kOTATrustTrustedCTLogsFilename);
    otapkiref->_nonTlsTrustedCTLogs = InitializeTrustedCTLogs(kOTATrustTrustedCTLogsNonTLSFilename);

    // Get the pinning list
    otapkiref->_pinningList = InitializePinningList();

    // Get the Event Sampling Rates
    otapkiref->_eventSamplingRates = InitializeEventSamplingRates();

    // Get the list of CAs used by Apple
    otapkiref->_appleCAs = InitializeAppleCertificateAuthorities();

    // Get the asset version (after possible reset due to missing asset data)
    if (!initialization_error_from_asset_data) {
        CFErrorRef error = nil;
        otapkiref->_assetVersion = GetAssetVersion(&error);
        otapkiref->_lastAssetCheckIn = InitializeLastAssetCheckIn();
        CFReleaseNull(error);
    } else {
        otapkiref->_assetVersion = GetSystemVersion((__bridge CFStringRef)kOTATrustContentVersionKey);
    }
    otapkiref->_secExperimentAssetVersion = 0;
    // Get the valid update snapshot version and format
    CFIndex update_format = 0;
    otapkiref->_validSnapshotVersion = InitializeValidSnapshotVersion(&update_format);
    otapkiref->_validSnapshotFormat = update_format;

    // Get the valid database snapshot path (if it exists, NULL otherwise)
    otapkiref->_validDatabaseSnapshot = InitializeValidDatabaseSnapshot();

    CFArrayRef escrowCerts = NULL;
    CFArrayRef escrowPCSCerts = NULL;
    InitializeEscrowCertificates(&escrowCerts, &escrowPCSCerts);
    if (NULL == escrowCerts || NULL == escrowPCSCerts) {
        CFReleaseNull(escrowCerts);
        CFReleaseNull(escrowPCSCerts);
        CFReleaseNull(otapkiref);
        return otapkiref;
    }
    otapkiref->_escrowCertificates = escrowCerts;
    otapkiref->_escrowPCSCertificates = escrowPCSCerts;

    // Get the mapping of EV Policy OIDs to Anchor digest
    CFDictionaryRef evOidToAnchorDigestMap = InitializeEVPolicyToAnchorDigestsTable();
    if (NULL == evOidToAnchorDigestMap) {
        CFReleaseNull(otapkiref);
        return otapkiref;
    }
    otapkiref->_evPolicyToAnchorMapping = evOidToAnchorDigestMap;

    CFDictionaryRef anchorLookupTable = NULL;
    const char* anchorTablePtr = NULL;

    if (!InitializeAnchorTable(&anchorLookupTable, &anchorTablePtr)) {
        CFReleaseSafe(anchorLookupTable);
        if (anchorTablePtr) {
            free((void *)anchorTablePtr);
        }
        CFReleaseNull(otapkiref);
        return otapkiref;
    }
    otapkiref->_anchorLookupTable = anchorLookupTable;
    otapkiref->_anchorTable = anchorTablePtr;

#if !TARGET_OS_BRIDGE
    /* Initialize our update handling */
    InitializeOTATrustAsset(kOTABackgroundQueue);
    otapkiref->_ctKillSwitch = InitializeKillSwitch((__bridge NSString*)kOTAPKIKillSwitchCT);
    otapkiref->_nonTlsCtKillSwitch = InitializeKillSwitch((__bridge NSString*)kOTAPKIKillSwitchNonTLSCT);
    InitializeOTASecExperimentAsset(kOTABackgroundQueue);
#else // TARGET_OS_BRIDGE
    otapkiref->_ctKillSwitch = true; // bridgeOS never enforces CT
    otapkiref->_nonTlsCtKillSwitch = true;
#endif // TARGET_OS_BRIDGE

    return otapkiref;
}

SecOTAPKIRef SecOTAPKICopyCurrentOTAPKIRef() {
    __block SecOTAPKIRef result = NULL;
    static dispatch_once_t kInitializeOTAPKI = 0;
    dispatch_once(&kInitializeOTAPKI, ^{
        @autoreleasepool {
            kOTAQueue = dispatch_queue_create("com.apple.security.OTAPKIQueue", NULL);
            dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                                                                 QOS_CLASS_BACKGROUND, 0);
            attr = dispatch_queue_attr_make_with_autorelease_frequency(attr, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM);
            kOTABackgroundQueue = dispatch_queue_create("com.apple.security.OTAPKIBackgroundQueue", attr);
            if (!kOTAQueue || !kOTABackgroundQueue) {
                secerror("Failed to create OTAPKI Queues. May crash later.");
            }
            dispatch_sync(kOTAQueue, ^{
                kCurrentOTAPKIRef = SecOTACreate();
            });
        }
    });

    dispatch_sync(kOTAQueue, ^{
        result = kCurrentOTAPKIRef;
        CFRetainSafe(result);
    });
    return result;
}

#if !TARGET_OS_BRIDGE
BOOL UpdateOTACheckInDate(void) {
    __block NSDate *checkIn = [NSDate date];
    dispatch_sync(kOTAQueue, ^{
        CFRetainAssign(kCurrentOTAPKIRef->_lastAssetCheckIn, (__bridge CFDateRef)checkIn);
    });

    if (SecOTAPKIIsSystemTrustd()) {
        /* Let the other trustds know we successfully checked in */
        notify_post(kOTATrustCheckInNotification);

        /* Update the on-disk check-in date, so when we re-launch we remember */
        NSError *error = nil;
        BOOL result = NO;
        if (!(result = UpdateOTAContextOnDisk(kOTATrustLastCheckInKey, checkIn, &error))) {
            secerror("OTATrust: failed to write last check-in time: %@", error);
        }
        return result;
    } else {
        return NO;
    }
}

void UpdateKillSwitch(NSString *key, bool value) {
    dispatch_sync(kOTAQueue, ^{
        if ([key isEqualToString:(__bridge NSString*)kOTAPKIKillSwitchCT]) {
            kCurrentOTAPKIRef->_ctKillSwitch = value;
        } else if ([key isEqualToString:(__bridge NSString*)kOTAPKIKillSwitchNonTLSCT]) {
            kCurrentOTAPKIRef->_nonTlsCtKillSwitch = value;
        }
    });
}

static BOOL UpdateFromAsset(NSURL *localURL, NSNumber *asset_version, NSError **error) {
    if (!localURL || !asset_version) {
        MakeOTATrustError(OTATrustMobileAssetType, error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecInternal,
                          @"missing url and version for downloaded asset");
        return NO;
    }
    __block NSArray *newTrustedCTLogs = NULL;
    __block NSArray *newNonTlsTrustedCTLogs = NULL;
    __block uint64_t version = [asset_version unsignedLongLongValue];
    __block NSDictionary *newAnalyticsSamplingRates = NULL;
    __block NSArray *newAppleCAs = NULL;

    NSURL *TrustedCTLogsFileLoc = [NSURL URLWithString:kOTATrustTrustedCTLogsFilename
                                         relativeToURL:localURL];
    newTrustedCTLogs = [NSArray arrayWithContentsOfURL:TrustedCTLogsFileLoc error:error];
    if (!newTrustedCTLogs) {
        secerror("OTATrust: unable to create TrustedCTLogs from asset file: %@", error ? *error: nil);
        LogRemotely(OTATrustLogLevelError, error);
        return NO;
    }

    NSURL *nonTLSTrustedCTLogsFileLoc = [NSURL URLWithString:kOTATrustTrustedCTLogsNonTLSFilename
                                         relativeToURL:localURL];
    newNonTlsTrustedCTLogs = [NSArray arrayWithContentsOfURL:nonTLSTrustedCTLogsFileLoc error:error];
    if (!newNonTlsTrustedCTLogs) {
        secerror("OTATrust: unable to create TrustedCTLogs_nonTLS from asset file: %@", error ? *error: nil);
        LogRemotely(OTATrustLogLevelError, error);
        return NO;
    }

    NSURL *AnalyticsSamplingRatesFileLoc = [NSURL URLWithString:kOTATrustAnalyticsSamplingRatesFilename
                                             relativeToURL:localURL];
    newAnalyticsSamplingRates = [NSDictionary dictionaryWithContentsOfURL:AnalyticsSamplingRatesFileLoc error:error];
    if (!newAnalyticsSamplingRates) {
        secerror("OTATrust: unable to create AnalyticsSamplingRates from asset file: %@", error ? *error: nil);
        LogRemotely(OTATrustLogLevelError, error);
        return NO;
    }

    NSURL *AppleCAsFileLoc = [NSURL URLWithString:kOTATrustAppleCertifcateAuthoritiesFilename
                                             relativeToURL:localURL];
    newAppleCAs = [NSArray arrayWithContentsOfURL:AppleCAsFileLoc error:error];
    if (!newAppleCAs) {
        secerror("OTATrust: unable to create AppleCAs from asset file: %@", error ? *error: nil);
        LogRemotely(OTATrustLogLevelError, error);
        return NO;
    }

    /* Update the Current OTAPKIRef with the new data */
    dispatch_sync(kOTAQueue, ^{
        secnotice("OTATrust", "updating asset version from %llu to %llu", kCurrentOTAPKIRef->_assetVersion, version);
        CFRetainAssign(kCurrentOTAPKIRef->_trustedCTLogs, (__bridge CFDictionaryRef)ConvertTrustedCTLogsArrayToDictionary(newTrustedCTLogs));
        CFRetainAssign(kCurrentOTAPKIRef->_nonTlsTrustedCTLogs, (__bridge CFDictionaryRef)ConvertTrustedCTLogsArrayToDictionary(newNonTlsTrustedCTLogs));
        CFRetainAssign(kCurrentOTAPKIRef->_eventSamplingRates, (__bridge CFDictionaryRef)newAnalyticsSamplingRates);
        CFRetainAssign(kCurrentOTAPKIRef->_appleCAs, (__bridge CFArrayRef)newAppleCAs);
        kCurrentOTAPKIRef->_assetVersion = version;
    });

    /* Reset the current files, version, and checkin so that in the case of write failures, we'll re-try
     * to update the data. We don't call DeleteAssetFromDisk() here to preserve any kill switches. */
    DeleteOldAssetData();
    UpdateOTAContext(@(0), nil);
    UpdateOTAContextOnDisk(kOTATrustLastCheckInKey, [NSDate dateWithTimeIntervalSince1970:0], nil);

    /* Write the data to disk (so that we don't have to re-download the asset on re-launch). */
    if (CopyFileToDisk(kOTATrustTrustedCTLogsFilename, TrustedCTLogsFileLoc, error) &&
        CopyFileToDisk(kOTATrustTrustedCTLogsNonTLSFilename, nonTLSTrustedCTLogsFileLoc, error) &&
        CopyFileToDisk(kOTATrustAnalyticsSamplingRatesFilename, AnalyticsSamplingRatesFileLoc, error) &&
        CopyFileToDisk(kOTATrustAppleCertifcateAuthoritiesFilename, AppleCAsFileLoc, error) &&
        UpdateOTAContext(asset_version, error)) { // Set version and check-in time last (after success)
        /* If we successfully updated the "asset" on disk, signal the other trustds to pick up the changes */
        notify_post(kOTATrustOnDiskAssetNotification);
    } else {
        return NO;
    }

    return YES;
}
#endif // !TARGET_OS_BRIDGE

#if !TARGET_OS_BRIDGE
static NSNumber *SecExperimentUpdateAsset(MAAsset *asset, NSNumber *asset_version, NSError **error) {
    NSURL *localURL = [asset getLocalFileUrl];
    if (!localURL || !asset_version) {
        MakeOTATrustError(OTASecExperimentMobileAssetType, error, OTATrustLogLevelError, NSOSStatusErrorDomain, errSecInternal,
                          @"missing url and version for downloaded SecExperiment asset");
        return nil;
    }
    NSDictionary *newSecExpConfig = NULL;
    uint64_t version = [asset_version unsignedLongLongValue];

    NSURL *secExpConfigFileLoc = [NSURL URLWithString:kOTASecExperimentConfigFilename
                                        relativeToURL:localURL];
    newSecExpConfig = [NSDictionary dictionaryWithContentsOfURL:secExpConfigFileLoc error:error];
    if (!newSecExpConfig) {
        secerror("OTATrust: unable to create SecExperiment from asset file: %@", error ? *error: nil);
        return nil;
    }
    /* Update the Current OTAPKIRef with the new data */
    dispatch_sync(kOTAQueue, ^{
        secnotice("OTATrust", "updating SecExperiment asset version from %llu to %llu", kCurrentOTAPKIRef->_secExperimentAssetVersion, version);
        CFRetainAssign(kCurrentOTAPKIRef->_secExperimentConfig, (__bridge CFDictionaryRef)newSecExpConfig);
        kCurrentOTAPKIRef->_secExperimentAssetVersion = version;
    });
    /* Signal the other trustds to pick up the changes */
    notify_post(kOTASecExperimentNewAssetNotification);
    return asset_version;
}
#endif // !TARGET_OS_BRIDGE

CFSetRef SecOTAPKICopyBlackListSet(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return CFRetainSafe(otapkiRef->_blackListSet);
}


CFSetRef SecOTAPKICopyGrayList(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return CFRetainSafe(otapkiRef->_grayListSet);
}

CFDictionaryRef SecOTAPKICopyAllowList(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    CFDictionaryRef result = otapkiRef->_allowList;
    if (!result) {
        result = InitializeAllowList();
        otapkiRef->_allowList = result;
    }

    return CFRetainSafe(result);
}

CFArrayRef SecOTAPKICopyAllowListForAuthKeyID(SecOTAPKIRef otapkiRef, CFStringRef authKeyID) {
    // %%% temporary performance optimization:
    // only load dictionary if we know an allow list exists for this key
    const CFStringRef keyIDs[3] = {
        CFSTR("7C724B39C7C0DB62A54F9BAA183492A2CA838259"),
        CFSTR("65F231AD2AF7F7DD52960AC702C10EEFA6D53B11"),
        CFSTR("D2A716207CAFD9959EEB430A19F2E0B9740EA8C7")
    };
    CFArrayRef result = NULL;
    bool hasAllowList = false;
    CFIndex count = (sizeof(keyIDs) / sizeof(keyIDs[0]));
    for (CFIndex ix=0; ix<count && authKeyID; ix++) {
        if (kCFCompareEqualTo == CFStringCompare(authKeyID, keyIDs[ix], 0)) {
            hasAllowList = true;
            break;
        }
    }
    if (!hasAllowList || !otapkiRef) {
        return result;
    }

    CFDictionaryRef allowListDict = SecOTAPKICopyAllowList(otapkiRef);
    if (!allowListDict) {
        return result;
    }

    // return a retained copy of the allow list array (or NULL)
    result = CFDictionaryGetValue(allowListDict, authKeyID);
    CFRetainSafe(result);
    CFReleaseSafe(allowListDict);
    return result;
}

CFDictionaryRef SecOTAPKICopyTrustedCTLogs(SecOTAPKIRef otapkiRef) {
    CFDictionaryRef result = NULL;
    if (NULL == otapkiRef) {
        return result;
    }

#if !TARGET_OS_BRIDGE
    /* Trigger periodic background MA checks in system trustd
     * We also check on trustd launch and listen for notifications. */
    TriggerPeriodicOTATrustAssetChecks(kOTABackgroundQueue);
#endif

    result = otapkiRef->_trustedCTLogs;
    CFRetainSafe(result);
    return result;
}

CFDictionaryRef SecOTAPKICopyNonTlsTrustedCTLogs(SecOTAPKIRef otapkiRef) {
    CFDictionaryRef result = NULL;
    if (NULL == otapkiRef) {
        return result;
    }

#if !TARGET_OS_BRIDGE
    /* Trigger periodic background MA checks in system trustd
     * We also check on trustd launch and listen for notifications. */
    TriggerPeriodicOTATrustAssetChecks(kOTABackgroundQueue);
#endif

    result = otapkiRef->_nonTlsTrustedCTLogs;
    CFRetainSafe(result);
    return result;
}

CFURLRef SecOTAPKICopyPinningList(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return CFRetainSafe(otapkiRef->_pinningList);
}


/* Returns an array of certificate data (CFDataRef) */
CFArrayRef SecOTAPKICopyEscrowCertificates(uint32_t escrowRootType, SecOTAPKIRef otapkiRef) {
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (NULL == otapkiRef) {
        return result;
    }

    switch (escrowRootType) {
        // Note: we shouldn't be getting called to return baseline roots,
        // since this function vends production roots by definition.
        case kSecCertificateBaselineEscrowRoot:
        case kSecCertificateProductionEscrowRoot:
        case kSecCertificateBaselineEscrowBackupRoot:
        case kSecCertificateProductionEscrowBackupRoot:
            if (otapkiRef->_escrowCertificates) {
                CFArrayRef escrowCerts = otapkiRef->_escrowCertificates;
                CFArrayAppendArray(result, escrowCerts, CFRangeMake(0, CFArrayGetCount(escrowCerts)));
            }
            break;
        case kSecCertificateBaselineEscrowEnrollmentRoot:
        case kSecCertificateProductionEscrowEnrollmentRoot:
            if (otapkiRef->_escrowCertificates) {
                // for enrollment purposes, exclude the v100 root
                static const unsigned char V100EscrowRoot[] = {
                    0x65,0x5C,0xB0,0x3C,0x39,0x3A,0x32,0xA6,0x0B,0x96,
                    0x40,0xC0,0xCA,0x73,0x41,0xFD,0xC3,0x9E,0x96,0xB3
                };
                CFArrayRef escrowCerts = otapkiRef->_escrowCertificates;
                CFIndex idx, count = CFArrayGetCount(escrowCerts);
                for (idx=0; idx < count; idx++) {
                    CFDataRef tmpData = (CFDataRef) CFArrayGetValueAtIndex(escrowCerts, idx);
                    SecCertificateRef tmpCert = (tmpData) ? SecCertificateCreateWithData(NULL, tmpData) : NULL;
                    CFDataRef sha1Hash = (tmpCert) ? SecCertificateGetSHA1Digest(tmpCert) : NULL;
                    const uint8_t *dp = (sha1Hash) ? CFDataGetBytePtr(sha1Hash) : NULL;
                    if (!(dp && !memcmp(V100EscrowRoot, dp, sizeof(V100EscrowRoot))) && tmpData) {
                        CFArrayAppendValue(result, tmpData);
                    }
                    CFReleaseSafe(tmpCert);
                }
            }
            break;
        case kSecCertificateBaselinePCSEscrowRoot:
        case kSecCertificateProductionPCSEscrowRoot:
            if (otapkiRef->_escrowPCSCertificates) {
                CFArrayRef escrowPCSCerts = otapkiRef->_escrowPCSCertificates;
                CFArrayAppendArray(result, escrowPCSCerts, CFRangeMake(0, CFArrayGetCount(escrowPCSCerts)));
            }
            break;
        default:
            break;
    }

    return result;
}


CFDictionaryRef SecOTAPKICopyEVPolicyToAnchorMapping(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return CFRetainSafe(otapkiRef->_evPolicyToAnchorMapping);
}


CFDictionaryRef SecOTAPKICopyAnchorLookupTable(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return  CFRetainSafe(otapkiRef->_anchorLookupTable);
}

const char* SecOTAPKIGetAnchorTable(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return otapkiRef->_anchorTable;
}

const char* SecOTAPKIGetValidDatabaseSnapshot(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

    return otapkiRef->_validDatabaseSnapshot;
}

CFIndex SecOTAPKIGetValidSnapshotVersion(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return 0;
    }

    return otapkiRef->_validSnapshotVersion;
}

CFIndex SecOTAPKIGetValidSnapshotFormat(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return 0;
    }

    return otapkiRef->_validSnapshotFormat;
}

uint64_t SecOTAPKIGetTrustStoreVersion(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return 0;
    }

    return otapkiRef->_trustStoreVersion;
}

uint64_t SecOTAPKIGetAssetVersion(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return 0;
    }

    return otapkiRef->_assetVersion;
}

CFDateRef SecOTAPKICopyLastAssetCheckInDate(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }
    return CFRetainSafe(otapkiRef->_lastAssetCheckIn);
}

bool SecOTAPKIAssetStalenessLessThanSeconds(SecOTAPKIRef otapkiRef, CFTimeInterval seconds) {
    if (NULL == otapkiRef) {
        return false;
    }

    bool result = false;
    CFDateRef lastCheckIn = CFRetainSafe(otapkiRef->_lastAssetCheckIn);
    if (isDate(lastCheckIn) && (fabs([(__bridge NSDate *)lastCheckIn timeIntervalSinceNow]) < seconds)) {
        result = true;
    }
    CFReleaseNull(lastCheckIn);
    return result;
}

NSNumber *SecOTAPKIGetSamplingRateForEvent(SecOTAPKIRef otapkiRef, NSString *eventName) {
    if (NULL == otapkiRef) {
        return nil;
    }

#if !TARGET_OS_BRIDGE
    /* Trigger periodic background MA checks in system trustd
     * We also check on trustd launch and listen for notifications. */
    TriggerPeriodicOTATrustAssetChecks(kOTABackgroundQueue);
#endif

    if (otapkiRef->_eventSamplingRates) {
        CFTypeRef value = CFDictionaryGetValue(otapkiRef->_eventSamplingRates, (__bridge CFStringRef)eventName);
        if (isNumberOfType(value, kCFNumberSInt64Type) || isNumberOfType(value, kCFNumberSInt32Type)) {
            return (__bridge NSNumber *)value;
        }
    }
    return nil;
}

CFArrayRef SecOTAPKICopyAppleCertificateAuthorities(SecOTAPKIRef otapkiRef) {
    if (NULL == otapkiRef) {
        return NULL;
    }

#if !TARGET_OS_BRIDGE
    /* Trigger periodic background MA checks in system trustd
     * We also check on trustd launch and listen for notifications. */
    TriggerPeriodicOTATrustAssetChecks(kOTABackgroundQueue);
#endif

    return CFRetainSafe(otapkiRef->_appleCAs);
}

bool SecOTAPKIKillSwitchEnabled(SecOTAPKIRef otapkiRef, CFStringRef key) {
    if (NULL == otapkiRef || NULL == key) {
        return false;
    }
    if (CFEqualSafe(key, kOTAPKIKillSwitchCT)) {
        return otapkiRef->_ctKillSwitch;
    } else if (CFEqualSafe(key, kOTAPKIKillSwitchNonTLSCT)) {
        return otapkiRef->_nonTlsCtKillSwitch;
    }
    return false;
}

/* Returns an array of certificate data (CFDataRef) */
CFArrayRef SecOTAPKICopyCurrentEscrowCertificates(uint32_t escrowRootType, CFErrorRef* error) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref) {
        SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
        return NULL;
    }

    CFArrayRef result = SecOTAPKICopyEscrowCertificates(escrowRootType, otapkiref);
    CFRelease(otapkiref);

    if (NULL == result) {
        SecError(errSecInternal, error, CFSTR("Could not get escrow certificates from the current OTAPKIRef"));
    }
    return result;
}

static CF_RETURNS_RETAINED CFDictionaryRef convertDataKeysToBase64Strings(CFDictionaryRef CF_CONSUMED dictionaryWithDataKeys) {
    @autoreleasepool {
        NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:((__bridge NSDictionary*)dictionaryWithDataKeys).count];
        NSDictionary <NSData *, id> *input = CFBridgingRelease(dictionaryWithDataKeys);
        for (NSData *key in input) {
            id obj = [input objectForKey:key];
            NSString *base64Key = [key base64EncodedStringWithOptions:0];
            result[base64Key] = obj;
        }
        return CFBridgingRetain(result);
    }
}

/* Returns an dictionary of dictionaries for currently trusted CT logs, indexed by the base64-encoded LogID */
CFDictionaryRef SecOTAPKICopyCurrentTrustedCTLogs(CFErrorRef* error) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref) {
        SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
        return NULL;
    }

    CFDictionaryRef result = convertDataKeysToBase64Strings(SecOTAPKICopyTrustedCTLogs(otapkiref));
    CFReleaseSafe(otapkiref);

    if (NULL == result) {
        SecError(errSecInternal, error, CFSTR("Could not get CT logs from the current OTAPKIRef"));
    }
    return result;
}

/* Returns a dictionary for the CT log matching specified LogID */
CFDictionaryRef SecOTAPKICopyCTLogForKeyID(CFDataRef keyID, CFErrorRef* error) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref) {
        SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
        return NULL;
    }

    /* Get the log lists */
    CFDictionaryRef trustedTlsLogs = SecOTAPKICopyTrustedCTLogs(otapkiref);
    CFDictionaryRef trustedNonTlsLogs = SecOTAPKICopyNonTlsTrustedCTLogs(otapkiref);
    CFReleaseNull(otapkiref);
    if (!trustedTlsLogs || !trustedNonTlsLogs) {
        CFReleaseNull(trustedTlsLogs);
        CFReleaseNull(trustedNonTlsLogs);
        return NULL;
    }

    /* Find the log */
    CFDictionaryRef logDict = CFDictionaryGetValue(trustedTlsLogs, keyID);
    if (!logDict) {
        logDict = CFDictionaryGetValue(trustedNonTlsLogs, keyID);
    }
    CFRetainSafe(logDict);
    CFReleaseNull(trustedTlsLogs);
    CFReleaseNull(trustedNonTlsLogs);
    return logDict;
}

uint64_t SecOTAPKIGetCurrentTrustStoreVersion(CFErrorRef* error){
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref) {
        SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
        return 0;
    }

    uint64_t result = otapkiref->_trustStoreVersion;
    CFReleaseNull(otapkiref);
    return result;
}

uint64_t SecOTAPKIGetCurrentAssetVersion(CFErrorRef* error) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiref) {
        SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
        return 0;
    }

    uint64_t result = otapkiref->_assetVersion;
    CFReleaseNull(otapkiref);
    return result;
}

uint64_t SecOTASecExperimentGetCurrentAssetVersion(CFErrorRef* error) {
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
     if (NULL == otapkiref) {
         SecError(errSecInternal, error, CFSTR("Unable to get the current OTAPKIRef"));
         return 0;
     }

     uint64_t result = otapkiref->_secExperimentAssetVersion;
     CFReleaseNull(otapkiref);
     return result;
 }

uint64_t SecOTAPKIResetCurrentAssetVersion(CFErrorRef* error) {
    uint64_t system_version = GetSystemVersion((__bridge CFStringRef)kOTATrustContentVersionKey);

    dispatch_sync(kOTAQueue, ^{
        kCurrentOTAPKIRef->_assetVersion = system_version;
        CFReleaseNull(kCurrentOTAPKIRef->_lastAssetCheckIn);
        kCurrentOTAPKIRef->_lastAssetCheckIn = NULL;
    });

#if !TARGET_OS_BRIDGE
    DeleteAssetFromDisk();
#endif
    return system_version;
}

uint64_t SecOTAPKISignalNewAsset(CFErrorRef* error) {
    NSError *nserror = nil;
    uint64_t version = 0;
#if !TARGET_OS_BRIDGE
    if (SecOTAPKIIsSystemTrustd()) {
        if (!DownloadOTATrustAsset(NO, YES, OTATrustMobileAssetType, &nserror) && error) {
            *error = CFRetainSafe((__bridge CFErrorRef)nserror);
        }
    } else {
        SecError(errSecServiceNotAvailable, error, CFSTR("This function may only be performed by the system trustd."));
    }
    version = GetAssetVersion(nil);
#else
    SecError(errSecUnsupportedService, error, CFSTR("This function is not available on this platform"));
    version = GetAssetVersion(error);
#endif
    return version;
}

uint64_t SecOTASecExperimentGetNewAsset(CFErrorRef* error) {
    NSError *nserror = nil;
#if !TARGET_OS_BRIDGE
    if (SecOTAPKIIsSystemTrustd()) {
        if (!DownloadOTATrustAsset(NO, YES, OTASecExperimentMobileAssetType, &nserror) && error) {
            *error = CFRetainSafe((__bridge CFErrorRef)nserror);
        }
    } else {
        SecError(errSecServiceNotAvailable, error, CFSTR("This function may only be performed by the system trustd."));
    }
#else
    SecError(errSecUnsupportedService, error, CFSTR("This function is not available on this platform"));
#endif
    return SecOTASecExperimentGetCurrentAssetVersion(error);
}

CFDictionaryRef SecOTASecExperimentCopyAsset(CFErrorRef* error) {
    SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiRef) {
        secnotice("OTATrust", "Null otapkiref");
        return NULL;
    }
    CFDictionaryRef asset = NULL;
    if (otapkiRef->_secExperimentConfig) {
        asset = CFRetainSafe(otapkiRef->_secExperimentConfig);
        secnotice("OTATrust", "asset found");
    } else {
        secnotice("OTATrust", "asset NULL");
    }
    CFReleaseNull(otapkiRef);
    return asset;
}
