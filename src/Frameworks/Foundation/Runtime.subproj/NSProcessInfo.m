/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSProcessInfo.h>
#include <sys/time.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <crt_externs.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>

NSNotificationName const NSProcessInfoThermalStateDidChangeNotification = @"NSProcessInfoThermalStateDidChangeNotification";
NSNotificationName const NSProcessInfoPowerStateDidChangeNotification = @"NSProcessInfoPowerStateDidChangeNotification";

@implementation NSProcessInfo

+ (NSProcessInfo *)processInfo {
    static NSProcessInfo *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

static NSString *_string(const char *cString) {
    return (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                 kCFStringEncodingUTF8);
}

- (NSArray<NSString *> *)arguments {
    int argc = *_NSGetArgc();
    char **argv = *_NSGetArgv();
    NSMutableArray<NSString *> *args = [NSMutableArray arrayWithCapacity:(NSUInteger)argc];

    for (int i = 0; i < argc; i++) {
        [args addObject:_string(argv[i])];
    }
    return args;
}

- (NSDictionary<NSString *, NSString *> *)environment {
    NSMutableDictionary *env = [NSMutableDictionary dictionaryWithCapacity:0];

    for (char **entry = *_NSGetEnviron(); entry != NULL && *entry != NULL; entry++) {
        char *equals = strchr(*entry, '=');
        if (equals == NULL) {
            continue;
        }

        CFStringRef key = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                  (const UInt8 *)*entry,
                                                  (CFIndex)(equals - *entry),
                                                  kCFStringEncodingUTF8, false);
        [env setObject:_string(equals + 1) forKey:(__bridge NSString *)key];
        CFRelease(key);
    }
    return env;
}

- (NSString *)processName {
    const char *argv0 = (*_NSGetArgv())[0];
    if (argv0 == NULL) {
        return _string("");
    }

    const char *slash = strrchr(argv0, '/');
    return _string(slash != NULL ? slash + 1 : argv0);
}

/* Unique across processes and calls: pid plus a counter and the clock, which
 * is what callers use it for (temporary file and connection names). */
- (NSString *)globallyUniqueString {
    static unsigned long counter = 0;
    struct timeval now;

    gettimeofday(&now, NULL);

    return [NSString stringWithFormat:@"%@-%d-%lx-%lx-%lx",
                     [self processName], getpid(), (unsigned long)(++counter),
                     (unsigned long)now.tv_sec, (unsigned long)now.tv_usec];
}

- (NSString *)globallyUniqueID {
    return [self globallyUniqueString];
}

- (int)processIdentifier {
    return getpid();
}

- (NSString *)hostName {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        return _string("localhost");
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return _string(buffer);
}

/* Darwin is the MACH flavour in this (OpenStep-era) enumeration. */
/* Seconds since boot, from the kernel's boot time. */
- (NSTimeInterval)systemUptime {
    struct timeval boot;
    size_t size = sizeof(boot);
    int name[2] = { CTL_KERN, KERN_BOOTTIME };

    if (sysctl(name, 2, &boot, &size, NULL, 0) != 0) {
        return 0.0;
    }

    struct timeval now;

    gettimeofday(&now, NULL);
    return (NSTimeInterval)(now.tv_sec - boot.tv_sec) +
           (NSTimeInterval)(now.tv_usec - boot.tv_usec) / 1000000.0;
}

- (NSUInteger)operatingSystem {
    return NSMACHOperatingSystem;
}

- (NSString *)operatingSystemName {
    return @"NSMACHOperatingSystem";
}

// SystemVersion.plist is where the release lives (ProductVersion, ProductBuildVersion)
static NSDictionary *systemVersionPlist(void) {
    static NSDictionary *plist;

    if (plist == nil) {
        plist = [[NSDictionary alloc] initWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    }
    return plist;
}

- (NSOperatingSystemVersion)operatingSystemVersion {
    NSOperatingSystemVersion v = { 0, 0, 0 };
    NSArray *parts = [[systemVersionPlist() objectForKey:@"ProductVersion"] componentsSeparatedByString:@"."];

    if ([parts count] > 0) {
        v.majorVersion = [[parts objectAtIndex:0] integerValue];
    }
    if ([parts count] > 1) {
        v.minorVersion = [[parts objectAtIndex:1] integerValue];
    }
    if ([parts count] > 2) {
        v.patchVersion = [[parts objectAtIndex:2] integerValue];
    }
    return v;
}

// the same shape as Apple's: "Version 26.5 (Build 25F74)"
- (NSString *)operatingSystemVersionString {
    NSString *version = [systemVersionPlist() objectForKey:@"ProductVersion"];
    NSString *build = [systemVersionPlist() objectForKey:@"ProductBuildVersion"];

    if (version == nil) {
        return @"Version unknown";
    }
    if (build == nil) {
        return [NSString stringWithFormat:@"Version %@", version];
    }
    return [NSString stringWithFormat:@"Version %@ (Build %@)", version, build];
}

- (BOOL)isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion)version {
    NSOperatingSystemVersion v = [self operatingSystemVersion];

    if (v.majorVersion != version.majorVersion) {
        return v.majorVersion > version.majorVersion;
    }
    if (v.minorVersion != version.minorVersion) {
        return v.minorVersion > version.minorVersion;
    }
    return v.patchVersion >= version.patchVersion;
}

// every processor here is online and schedulable
- (NSUInteger)activeProcessorCount {
    return [self processorCount];
}

- (NSUInteger)processorCount {
    int count = 0;
    size_t length = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &length, NULL, 0) != 0 || count < 1) {
        return 1;
    }
    return (NSUInteger)count;
}

// no thermal or battery sensors are read here, the machine always reports nominal and mains power
- (NSProcessInfoThermalState)thermalState {
    return NSProcessInfoThermalStateNominal;
}

- (BOOL)isLowPowerModeEnabled {
    return NO;
}

// PD has no idle sleep or sudden termination to hold off, so an activity is only a token that keeps its reason
- (id<NSObject>)beginActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason {
    return [[[NSString alloc] initWithFormat:@"<NSActivity 0x%llx %@>", (unsigned long long)options, reason] autorelease];
}

- (void)endActivity:(id<NSObject>)activity {
}

- (void)performActivityWithOptions:(NSActivityOptions)options reason:(NSString *)reason usingBlock:(void (^)(void))block {
    id<NSObject> activity = [[self beginActivityWithOptions:options reason:reason] retain];

    block();
    [self endActivity:activity];
    [activity release];
}

- (void)disableSuddenTermination {
}

- (void)enableSuddenTermination {
}

- (void)disableAutomaticTermination:(NSString *)reason {
}

- (void)enableAutomaticTermination:(NSString *)reason {
}

- (BOOL)automaticTerminationSupportEnabled {
    return NO;
}

- (void)setAutomaticTerminationSupportEnabled:(BOOL)enabled {
}

- (unsigned long long)physicalMemory {
    uint64_t bytes = 0;
    size_t length = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &length, NULL, 0) != 0) {
        return 0;
    }
    return bytes;
}

@end

void __NSInitializeProcess(int argc, const char *argv[]) {
}
