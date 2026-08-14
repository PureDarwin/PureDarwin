/*
 * Copyright (c) 2018-2020 Apple Inc. All Rights Reserved.
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
#import <Foundation/Foundation.h>
#include <stdatomic.h>
#include <notify.h>
#include <sys/stat.h>
#include <Security/Security.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecPolicyPriv.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#import "OTATrustUtilities.h"
#include "trustdFileLocations.h"
#include "SecTrustStoreServer.h"

/*
 * Each config file is a dictionary with NSString keys corresponding to the appIDs.
 * The value for each appID is the config and is defined (and verified) by the config callbacks.
 */

//
// MARK: Shared Configuration helpers
//
typedef bool(*arrayValueChecker)(id _Nonnull obj);
typedef NSDictionary <NSString*, id>*(^ConfigDiskReader)(NSURL * fileURL, NSError **error);
typedef bool (^ConfigCheckerAndSetter)(id newConfig, id *existingMutableConfig, CFErrorRef *error);
typedef CFTypeRef (^CombineAndCopyAllConfig)(NSDictionary <NSString*,id> *allConfig, CFErrorRef *error);

static bool checkDomainsValuesCompliance(id _Nonnull obj) {
    if (![obj isKindOfClass:[NSString class]]) {
        return false;
    }
    if (SecDNSIsTLD((__bridge CFStringRef)obj)) {
        return false;
    }
    return true;
}

static bool checkCAsValuesCompliance(id _Nonnull obj) {
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return false;
    }
    if (2 != [(NSDictionary*)obj count]) {
        return false;
    }
    if (nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecTrustStoreHashAlgorithmKey] ||
        nil == ((NSDictionary*)obj)[(__bridge NSString*)kSecTrustStoreSPKIHashKey]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecTrustStoreHashAlgorithmKey] isKindOfClass:[NSString class]] ||
        ![((NSDictionary*)obj)[(__bridge NSString*)kSecTrustStoreSPKIHashKey] isKindOfClass:[NSData class]]) {
        return false;
    }
    if (![((NSDictionary*)obj)[(__bridge NSString*)kSecTrustStoreHashAlgorithmKey] isEqualToString:@"sha256"]) {
        return false;
    }
    return true;
}

static bool checkArrayValues(NSString *key, id value, arrayValueChecker checker, CFErrorRef *error) {
    if (![value isKindOfClass:[NSArray class]]) {
        return SecError(errSecParam, error, CFSTR("value for %@ is not an array in configuration"), key);
    }

    __block bool result = true;
    [(NSArray*)value enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (!checker(obj)) {
            result = SecError(errSecParam, error, CFSTR("value %lu for %@ is not the expected type"), (unsigned long)idx, key);
            *stop = true;
        }
    }];
    return result;
}

static bool _SecTrustStoreSetConfiguration(CFStringRef appID, CFTypeRef configuration, CFErrorRef *error,
                                           char *configurationType, NSURL *fileURL, _Atomic bool *cachedConfigExists,
                                           char *notification, ConfigDiskReader readConfigFromDisk,
                                           ConfigCheckerAndSetter checkAndSetConfig)
{
    if (!SecOTAPKIIsSystemTrustd()) {
        secerror("Unable to write %{public}s from user agent", configurationType);
        return SecError(errSecWrPerm, error, CFSTR("Unable to write %s from user agent"), configurationType);
    }

    if (!appID) {
        secerror("application-identifier required to set %{public}s", configurationType);
        return SecError(errSecParam, error, CFSTR("application-identifier required to set %s"), configurationType);
    }

    @autoreleasepool {
        NSError *nserror = nil;
        NSMutableDictionary *allConfig = [readConfigFromDisk(fileURL, &nserror) mutableCopy];
        id appConfig = NULL;
        if (allConfig && allConfig[(__bridge NSString*)appID]) {
            appConfig = [allConfig[(__bridge NSString*)appID] mutableCopy];
        } else if (!allConfig) {
            allConfig =  [NSMutableDictionary dictionary];
        }

        if (configuration) {
            id inConfig = (__bridge id)configuration;
            if (!checkAndSetConfig(inConfig, &appConfig, error)) {
                secerror("%{public}s have error: %@", configurationType, error ? *error : nil);
                return false;
            }
        }

        if (!configuration || [appConfig count] == 0) {
            [allConfig removeObjectForKey:(__bridge NSString*)appID];
        } else {
            allConfig[(__bridge NSString*)appID] = appConfig;
        }

        if (![allConfig writeToClassDURL:fileURL permissions:0644 error:&nserror]) {
            secerror("failed to write %{public}s: %@", configurationType, nserror);
            if (error) {
                *error = CFRetainSafe((__bridge CFErrorRef)nserror);
            }
            return false;
        }
        secnotice("config", "wrote %lu configs for %{public}s", (unsigned long)[allConfig count], configurationType);
        atomic_store(cachedConfigExists, [allConfig count] != 0);
        notify_post(notification);
        return true;
    }
}

static void _SecTrustStoreCreateEmptyConfigCache(char *configurationType, _Atomic bool *cachedConfigExists, char *notification, int *notify_token, NSURL *fileURL, ConfigDiskReader readConfigFromDisk)
{
    @autoreleasepool {
        NSError *read_error = nil;
        NSDictionary <NSString*,id> *allConfig = readConfigFromDisk(fileURL, &read_error);
        if (!allConfig|| [allConfig count] == 0) {
            secnotice("config", "skipping further reads. no %{public}s found: %@", configurationType, read_error);
            atomic_store(cachedConfigExists, false);
        } else {
            secnotice("config", "have %{public}s. will need to read.", configurationType);
            atomic_store(cachedConfigExists, true);
        }

        /* read-only trustds register for notfications from the read-write trustd */
        if (!SecOTAPKIIsSystemTrustd()) {
            uint32_t status = notify_register_check(notification, notify_token);
            if (status == NOTIFY_STATUS_OK) {
                int check = 0;
                status = notify_check(*notify_token, &check);
                (void)check; // notify_check errors if we don't pass a second parameter, but we don't need the value here
            }
            if (status != NOTIFY_STATUS_OK) {
                secerror("failed to establish notification for %{public}s: %u", configurationType, status);
                notify_cancel(*notify_token);
                *notify_token = 0;
            }
        }
    }
}

static CFTypeRef _SecTrustStoreCopyConfiguration(CFStringRef appID, CFErrorRef *error, char *configurationType,
                                                 _Atomic bool *cachedConfigExists, char *notification, int *notify_token,
                                                 NSURL *fileURL, ConfigDiskReader readConfigFromDisk, CombineAndCopyAllConfig combineAllConfig) {
    @autoreleasepool {
        /* Read the negative cached value as to whether there is config to read */
        if (!SecOTAPKIIsSystemTrustd()) {
            /* Check whether we got a notification. If we didn't, and there is no config set, return NULL.
             * Otherwise, we need to read from disk */
            int check = 0;
            uint32_t check_status = notify_check(*notify_token, &check);
            if (check_status == NOTIFY_STATUS_OK && check == 0 && !atomic_load(cachedConfigExists)) {
                return NULL;
            }
        } else if (!atomic_load(cachedConfigExists)) {
            return NULL;
        }

        /* We need to read the config from disk */
        NSError *read_error = nil;
        NSDictionary <NSString*,id> *allConfig = readConfigFromDisk(fileURL, &read_error);
        if (!allConfig || [allConfig count] == 0) {
            secnotice("config", "skipping further reads. no %{public}s found: %@", configurationType, read_error);
            atomic_store(cachedConfigExists, false);
            return NULL;
        }

        /* If the caller specified an appID, return only the config for that appID */
        if (appID) {
            return CFBridgingRetain(allConfig[(__bridge NSString*)appID]);
        }

        return combineAllConfig(allConfig, error);
    }
}

//
// MARK: CT Exceptions
//

ConfigCheckerAndSetter checkInputExceptionsAndSetAppExceptions = ^bool(id inConfig, id *appConfig, CFErrorRef *error) {
    __block bool result = true;
    if (![inConfig isKindOfClass:[NSDictionary class]]) {
        return SecError(errSecParam, error, CFSTR("value for CT Exceptions is not a dictionary in new configuration"));
    }

    if (!appConfig || (*appConfig && ![*appConfig isKindOfClass:[NSMutableDictionary class]])) {
        return SecError(errSecParam, error, CFSTR("value for CT Exceptions is not a dictionary in current configuration"));
    } else if (!*appConfig) {
        *appConfig = [NSMutableDictionary dictionary];
    }

    NSMutableDictionary *appExceptions = (NSMutableDictionary *)*appConfig;
    NSDictionary *inExceptions = (NSDictionary *)inConfig;
    if (inExceptions.count == 0) {
        return true;
    }
    [inExceptions enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if ([key isEqualToString:(__bridge NSString*)kSecCTExceptionsDomainsKey]) {
            if (!checkArrayValues(key, obj, checkDomainsValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else if ([key isEqualToString:(__bridge NSString*)kSecCTExceptionsCAsKey]) {
            if (!checkArrayValues(key, obj, checkCAsValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else {
            result = SecError(errSecParam, error, CFSTR("unknown key (%@) in configuration dictionary"), key);
            *stop = YES;
            result = false;
            return;
        }
        if ([(NSArray*)obj count] == 0) {
            [appExceptions removeObjectForKey:key];
        } else {
            appExceptions[key] = obj;
        }
    }];
    return result;
};

static _Atomic bool gHasCTExceptions = false;
#define kSecCTExceptionsChanged "com.apple.trustd.ct.exceptions-changed"

static NSURL *CTExceptionsOldFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("CTExceptions.plist")));
}

static NSURL *CTExceptionsFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInPrivateTrustdDirectory(CFSTR("CTExceptions.plist")));
}

ConfigDiskReader readExceptionsFromDisk = ^NSDictionary <NSString*,NSDictionary*> *(NSURL *fileUrl, NSError **error) {
    secdebug("ct", "reading CT exceptions from disk");
    NSDictionary <NSString*,NSDictionary*> *allExceptions = [NSDictionary dictionaryWithContentsOfURL:fileUrl
                                                                                                error:error];
    return allExceptions;
};

bool _SecTrustStoreSetCTExceptions(CFStringRef appID, CFDictionaryRef exceptions, CFErrorRef *error)  {
    return _SecTrustStoreSetConfiguration(appID, exceptions, error, "CT Exceptions", CTExceptionsFileURL(),
                                          &gHasCTExceptions, kSecCTExceptionsChanged, readExceptionsFromDisk,
                                          checkInputExceptionsAndSetAppExceptions);
}

CombineAndCopyAllConfig combineAllCTExceptions = ^CFTypeRef(NSDictionary <NSString*,id> *allExceptions, CFErrorRef *error) {
    NSMutableArray *domainExceptions = [NSMutableArray array];
    NSMutableArray *caExceptions = [NSMutableArray array];
    [allExceptions enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused key, id _Nonnull appConfig,
                                                       BOOL * _Nonnull __unused stop) {
        if (![appConfig isKindOfClass:[NSDictionary class]]) {
            return;
        }

        NSDictionary *appExceptions = (NSDictionary *)appConfig;
        if (appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey] &&
            checkArrayValues((__bridge NSString*)kSecCTExceptionsDomainsKey, appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey],
                                  checkDomainsValuesCompliance, error)) {
            [domainExceptions addObjectsFromArray:appExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey]];
        }
        if (appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey] &&
            checkArrayValues((__bridge NSString*)kSecCTExceptionsCAsKey, appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey],
                                  checkCAsValuesCompliance, error)) {
            [caExceptions addObjectsFromArray:appExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey]];
        }
    }];
    NSMutableDictionary *exceptions = [NSMutableDictionary dictionaryWithCapacity:2];
    if ([domainExceptions count] > 0) {
        exceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey] = domainExceptions;
    }
    if ([caExceptions count] > 0) {
        exceptions[(__bridge NSString*)kSecCTExceptionsCAsKey] = caExceptions;
    }
    if ([exceptions count] > 0) {
        secdebug("ct", "found %lu CT exceptions on disk", (unsigned long)[exceptions count]);
        atomic_store(&gHasCTExceptions, true);
        return CFBridgingRetain(exceptions);
    }
    return NULL;
};

CFDictionaryRef _SecTrustStoreCopyCTExceptions(CFStringRef appID, CFErrorRef *error) {
    static int notify_token = 0;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _SecTrustStoreCreateEmptyConfigCache("CT Exceptions",
                                             &gHasCTExceptions, kSecCTExceptionsChanged, &notify_token,
                                             CTExceptionsFileURL(), readExceptionsFromDisk);
    });
    return _SecTrustStoreCopyConfiguration(appID, error, "CT Exceptions",
                                           &gHasCTExceptions, kSecCTExceptionsChanged, &notify_token,
                                           CTExceptionsFileURL(), readExceptionsFromDisk, combineAllCTExceptions);
}

//
// MARK: CA Revocation Additions
//

ConfigCheckerAndSetter checkInputAdditionsAndSetAppAdditions = ^bool(id inConfig, id *appConfig, CFErrorRef *error) {
    __block bool result = true;
    if (![inConfig isKindOfClass:[NSDictionary class]]) {
        return SecError(errSecParam, error, CFSTR("value for CA revocation additions is not a dictionary in new configuration"));
    }

    if (!appConfig || (*appConfig && ![*appConfig isKindOfClass:[NSMutableDictionary class]])) {
        return SecError(errSecParam, error, CFSTR("value for CA revocation additions is not a dictionary in existing configuration"));
    } else if (!*appConfig) {
        *appConfig = [NSMutableDictionary dictionary];
    }

    NSMutableDictionary *appAdditions = (NSMutableDictionary *)*appConfig;
    NSDictionary *inAdditions = (NSDictionary *)inConfig;
    if (inAdditions.count == 0) {
        return true;
    }
    [inAdditions enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if ([key isEqualToString:(__bridge NSString*)kSecCARevocationAdditionsKey]) {
            if (!checkArrayValues(key, obj, checkCAsValuesCompliance, error)) {
                *stop = YES;
                result = false;
                return;
            }
        } else {
            result = SecError(errSecParam, error, CFSTR("unknown key (%@) in additions dictionary"), key);
            *stop = YES;
            result = false;
            return;
        }
        if ([(NSArray*)obj count] == 0) {
            [appAdditions removeObjectForKey:key];
        } else {
            appAdditions[key] = obj;
        }
    }];
    return result;
};

static _Atomic bool gHasCARevocationAdditions = false;
#define kSecCARevocationChanged "com.apple.trustd.ca.revocation-changed"

static NSURL *CARevocationOldFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("CARevocation.plist")));
}

static NSURL *CARevocationFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInPrivateTrustdDirectory(CFSTR("CARevocation.plist")));
}

ConfigDiskReader readRevocationAdditionsFromDisk = ^NSDictionary <NSString*,NSDictionary*> *(NSURL *fileUrl, NSError **error) {
    secdebug("ocsp", "reading CA revocation additions from disk");
    NSDictionary <NSString*,NSDictionary*> *allAdditions = [NSDictionary dictionaryWithContentsOfURL:fileUrl
                                                                                                error:error];
    return allAdditions;
};

bool _SecTrustStoreSetCARevocationAdditions(CFStringRef appID, CFDictionaryRef additions, CFErrorRef *error)  {
    return _SecTrustStoreSetConfiguration(appID, additions, error, "CA Revocation Additions", CARevocationFileURL(),
                                          &gHasCARevocationAdditions, kSecCARevocationChanged, readRevocationAdditionsFromDisk,
                                          checkInputAdditionsAndSetAppAdditions);
}

CombineAndCopyAllConfig combineAllCARevocationAdditions = ^CFTypeRef(NSDictionary <NSString*,id> *allAdditions, CFErrorRef *error) {
    NSMutableArray *caAdditions = [NSMutableArray array];
    [allAdditions enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused key, id _Nonnull appConfig,
                                                       BOOL * _Nonnull __unused stop) {
        if (![appConfig isKindOfClass:[NSDictionary class]]) {
            return;
        }

        NSDictionary *appAdditions = (NSDictionary *)appConfig;
        if (appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey] &&
            checkArrayValues((__bridge NSString*)kSecCARevocationAdditionsKey,
                                    appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey],
                                    checkCAsValuesCompliance, error)) {
            [caAdditions addObjectsFromArray:appAdditions[(__bridge NSString*)kSecCARevocationAdditionsKey]];
        }
    }];
    NSMutableDictionary *additions = [NSMutableDictionary dictionaryWithCapacity:1];
    if ([caAdditions count] > 0) {
        additions[(__bridge NSString*)kSecCARevocationAdditionsKey] = caAdditions;
    }
    if ([additions count] > 0) {
        secdebug("ocsp", "found %lu CA revocation additions on disk", (unsigned long)[additions count]);
        atomic_store(&gHasCARevocationAdditions, true);
        return CFBridgingRetain(additions);
    }
    return NULL;
};

CFDictionaryRef _SecTrustStoreCopyCARevocationAdditions(CFStringRef appID, CFErrorRef *error) {
    static int notify_token = 0;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _SecTrustStoreCreateEmptyConfigCache("CA Revocation Additions",
                                             &gHasCARevocationAdditions, kSecCARevocationChanged, &notify_token,
                                             CARevocationFileURL(), readRevocationAdditionsFromDisk);
    });
    return _SecTrustStoreCopyConfiguration(appID, error, "CA Revocation Additions",
                                           &gHasCARevocationAdditions, kSecCARevocationChanged, &notify_token,
                                           CARevocationFileURL(), readRevocationAdditionsFromDisk, combineAllCARevocationAdditions);
}

//
// MARK: Transparent Connection Pins
//
static _Atomic bool gHasTransparentConnectionPins = false;
#define kSecTransparentConnectionPinsChanged "com.apple.trustd.hrn.pins-changed"
const NSString *kSecCAPinsKey = @"CAPins";

static NSURL *TransparentConnectionPinsOldFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("TransparentConnectionPins.plist")));
}

static NSURL *TransparentConnectionPinsFileURL() {
    return CFBridgingRelease(SecCopyURLForFileInPrivateTrustdDirectory(CFSTR("TransparentConnectionPins.plist")));
}

ConfigDiskReader readPinsFromDisk = ^NSDictionary <NSString*,NSArray*> *(NSURL *fileUrl, NSError **error) {
    secdebug("config", "reading Pins from disk");
    NSDictionary <NSString*,NSArray*> *allPins = [NSDictionary dictionaryWithContentsOfURL:fileUrl
                                                                                     error:error];
    return allPins;
};

ConfigCheckerAndSetter checkInputPinsAndSetPins = ^bool(id inConfig, id *appConfig, CFErrorRef *error) {
    if (!appConfig || (*appConfig && ![*appConfig isKindOfClass:[NSMutableArray class]])) {
        return SecError(errSecParam, error, CFSTR("value for Transparent Connection pins is not an array in existing configuration"));
    } else if (!*appConfig) {
        *appConfig = [NSMutableArray array];
    }

    if(!checkArrayValues(@"TransparentConnectionPins", inConfig, checkCAsValuesCompliance, error)) {
        return false;
    }

    // Replace (null input) or remove config
    if (!inConfig) {
        [*appConfig removeAllObjects];
    } else if ([inConfig count] > 0) {
        *appConfig = [(NSArray*)inConfig mutableCopy];
    }
    return true;
};

CombineAndCopyAllConfig combineAllPins = ^CFTypeRef(NSDictionary <NSString*,id> *allConfig, CFErrorRef *error) {
    NSMutableArray *pins = [NSMutableArray  array];
    [allConfig enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull __unused key, id  _Nonnull obj, BOOL * _Nonnull __unused stop) {
        if (checkArrayValues(@"TransparentConnectionPins", obj, checkCAsValuesCompliance, error)) {
            [pins addObjectsFromArray:(NSArray *)obj];
        }
    }];
    if ([pins count] > 0) {
        secdebug("config", "found %lu Transparent Connection pins on disk", (unsigned long)[pins count]);
        atomic_store(&gHasTransparentConnectionPins, true);
        return CFBridgingRetain(pins);
    }
    return NULL;
};

bool _SecTrustStoreSetTransparentConnectionPins(CFStringRef appID, CFArrayRef pins, CFErrorRef *error)  {
    return _SecTrustStoreSetConfiguration(appID, pins, error, "Transparent Connection Pins", TransparentConnectionPinsFileURL(),
                                          &gHasTransparentConnectionPins, kSecTransparentConnectionPinsChanged,
                                          readPinsFromDisk, checkInputPinsAndSetPins);
}

CFArrayRef _SecTrustStoreCopyTransparentConnectionPins(CFStringRef appID, CFErrorRef *error) {
    static int notify_token = 0;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _SecTrustStoreCreateEmptyConfigCache("Transparent Connection Pins",
                                             &gHasTransparentConnectionPins, kSecTransparentConnectionPinsChanged, &notify_token,
                                             TransparentConnectionPinsFileURL(), readPinsFromDisk);
    });
    return _SecTrustStoreCopyConfiguration(appID, error, "Transparent Connection Pins",
                                           &gHasTransparentConnectionPins, kSecTransparentConnectionPinsChanged, &notify_token,
                                           TransparentConnectionPinsFileURL(), readPinsFromDisk, combineAllPins);
}

//
// MARK: One-time migration
//
static bool _SecTrustStoreMigrateConfiguration(NSURL *oldFileURL, NSURL *newFileURL, char *configurationType, ConfigDiskReader readConfigFromDisk)
{
    NSError *error;
    if (readConfigFromDisk(newFileURL, &error)) {
        secdebug("config", "already migrated %{public}s", configurationType);
        return true;
    }
    NSDictionary *config = readConfigFromDisk(oldFileURL, &error);
    if (!config) {
        // always write something to the new config so that we can use it as a migration indicator
        secdebug("config", "no existing %{public}s to migrate: %@", configurationType, error);
        config = [NSDictionary dictionary];
    }

    secdebug("config", "migrating %{public}s", configurationType);
    if (![config writeToClassDURL:newFileURL permissions:0644 error:&error]) {
        secerror("failed to write %{public}s: %@", configurationType, error);
        return false;
    }
    // Delete old file
    WithPathInDirectory(CFBridgingRetain(oldFileURL), ^(const char *utf8String) {
        remove(utf8String);
    });
    return true;

}

void _SecTrustStoreMigrateConfigurations(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (SecOTAPKIIsSystemTrustd()) {
            _SecTrustStoreMigrateConfiguration(CTExceptionsOldFileURL(), CTExceptionsFileURL(),
                                               "CT Exceptions", readExceptionsFromDisk);
            _SecTrustStoreMigrateConfiguration(CARevocationOldFileURL(), CARevocationFileURL(),
                                               "CA Revocation Additions", readRevocationAdditionsFromDisk);
            _SecTrustStoreMigrateConfiguration(TransparentConnectionPinsOldFileURL(), TransparentConnectionPinsFileURL(),
                                               "Transparent Connection Pins", readPinsFromDisk);
        }
    });
}
