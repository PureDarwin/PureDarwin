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
- (NSUInteger)processorCount;
- (unsigned long long)physicalMemory;

@end

/* Cocotron's entry point for handing Foundation the process arguments. Here
 * NSProcessInfo reads them from the crt (_NSGetArgc/_NSGetArgv), so there is
 * nothing to stash - it exists so NSApplicationMain and friends compile. */
FOUNDATION_EXPORT void __NSInitializeProcess(int argc, const char *argv[]);

#endif /* NSProcessInfo_h */
