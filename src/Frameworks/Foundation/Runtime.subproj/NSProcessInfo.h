/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSProcessInfo_h
#define NSProcessInfo_h

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSObjCRuntime.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNotification.h>

typedef NS_ENUM(NSInteger, NSProcessInfoThermalState) {
    NSProcessInfoThermalStateNominal,
    NSProcessInfoThermalStateFair,
    NSProcessInfoThermalStateSerious,
    NSProcessInfoThermalStateCritical
};

FOUNDATION_EXPORT NSNotificationName const NSProcessInfoThermalStateDidChangeNotification;
FOUNDATION_EXPORT NSNotificationName const NSProcessInfoPowerStateDidChangeNotification;

// what an activity asks the system not to do while it runs
typedef NS_OPTIONS(uint64_t, NSActivityOptions) {
    NSActivityIdleDisplaySleepDisabled = (1ULL << 40),
    NSActivityIdleSystemSleepDisabled = (1ULL << 20),
    NSActivitySuddenTerminationDisabled = (1ULL << 14),
    NSActivityAutomaticTerminationDisabled = (1ULL << 15),
    NSActivityAnimationTrackingEnabled = (1ULL << 45),
    NSActivityTrackingEnabled = (1ULL << 46),
    NSActivityUserInitiated = (0x00FFFFFFULL | NSActivityIdleSystemSleepDisabled),
    NSActivityUserInitiatedAllowingIdleSystemSleep = (NSActivityUserInitiated & ~NSActivityIdleSystemSleepDisabled),
    NSActivityBackground = 0x000000FFULL,
    NSActivityLatencyCritical = 0xFF00000000ULL,
    NSActivityUserInteractive = (NSActivityUserInitiated | NSActivityLatencyCritical),
};

typedef struct {
    NSInteger majorVersion;
    NSInteger minorVersion;
    NSInteger patchVersion;
} NSOperatingSystemVersion;

@interface NSProcessInfo : NSObject

enum {
    NSWindowsNTOperatingSystem = 1,
    NSWindows95OperatingSystem,
    NSSolarisOperatingSystem,
    NSHPUXOperatingSystem,
    NSMACHOperatingSystem,
    NSSunOSOperatingSystem,
    NSOSF1OperatingSystem
};

+ (NSProcessInfo *)processInfo;

- (NSArray<NSString *> *)arguments;
- (NSDictionary<NSString *, NSString *> *)environment;
- (NSString *)processName;
- (NSString *)globallyUniqueString;
- (int)processIdentifier;
- (NSString *)hostName;
- (NSTimeInterval)systemUptime;
- (NSUInteger)operatingSystem;
- (NSString *)operatingSystemName;
- (NSOperatingSystemVersion)operatingSystemVersion;
- (NSString *)operatingSystemVersionString;
- (BOOL)isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion)version;
- (NSUInteger)processorCount;
- (unsigned long long)physicalMemory;
- (NSUInteger)activeProcessorCount;
- (NSProcessInfoThermalState)thermalState;
- (BOOL)isLowPowerModeEnabled;

- (id<NSObject>)beginActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason;
- (void)endActivity:(id<NSObject>)activity;
- (void)performActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason usingBlock:(void (^)(void))block;
- (void)disableSuddenTermination;
- (void)enableSuddenTermination;
- (void)disableAutomaticTermination:(NSString *)reason;
- (void)enableAutomaticTermination:(NSString *)reason;
- (BOOL)automaticTerminationSupportEnabled;
- (void)setAutomaticTerminationSupportEnabled:(BOOL)enabled;

@end

/* Cocotron's entry point for handing Foundation the process arguments. Here
 * NSProcessInfo reads them from the crt (_NSGetArgc/_NSGetArgv), so there is
 * nothing to stash - it exists so NSApplicationMain and friends compile. */
FOUNDATION_EXPORT void __NSInitializeProcess(int argc, const char *argv[]);

#endif /* NSProcessInfo_h */
