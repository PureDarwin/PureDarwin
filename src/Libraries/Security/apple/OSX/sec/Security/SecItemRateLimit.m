/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import "SecItemRateLimit.h"
#import "SecItemRateLimit_tests.h"

#import <utilities/debugging.h>
#import <utilities/SecInternalReleasePriv.h>
#import "ipc/securityd_client.h"

#import <sys/codesign.h>
#import <os/feature_private.h>

// Dressed-down version of RateLimiter which directly computes the rate of one bucket and resets when it runs out of tokens

// Broken out so the test-only reset method can reinit this
static SecItemRateLimit* ratelimit;

@implementation SecItemRateLimit {
    bool _forceEnabled;
    dispatch_queue_t _dataQueue;
}

+ (instancetype)instance {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ratelimit = [SecItemRateLimit new];
    });
    return ratelimit;
}

- (instancetype)init {
    if (self = [super init]) {
        _roCapacity = 25; 	// allow burst of this size
        _roRate = 3.0;      // allow sustained rate of this many per second
        _rwCapacity = 25;
        _rwRate = 1.0;
        _roBucket = nil;
        _rwBucket = nil;
        _forceEnabled = false;
        _limitMultiplier = 5.0;     // Multiply capacity and rate by this much after exceeding limit
        _dataQueue = dispatch_queue_create("com.apple.keychain.secitemratelimit.dataqueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    }

    return self;
}

- (bool)isEnabled {
    return _forceEnabled || [self shouldCountAPICalls];
}

- (void)forceEnabled:(bool)force {
    _forceEnabled = force;
    secnotice("secitemratelimit", "%sorcing SIRL to be enabled (effective: %i)", force ? "F" : "Not f", [self isEnabled]);
}

- (bool)isReadOnlyAPICallWithinLimits {
    if (![self consumeTokenFromBucket:false]) {
        secnotice("secitemratelimit", "Readonly API rate exceeded");
        return false;
    } else {
        return true;
    }
}

- (bool)isModifyingAPICallWithinLimits {
    if (![self consumeTokenFromBucket:true]) {
        secnotice("secitemratelimit", "Modifying API rate exceeded");
        return false;
    } else {
        return true;
    }
}

- (bool)consumeTokenFromBucket:(bool)readwrite {
    if (![self shouldCountAPICalls] && !_forceEnabled) {
        return true;
    }

    __block bool ok = false;
    dispatch_sync(_dataQueue, ^{
        int* capacity = readwrite ? &_rwCapacity : &_roCapacity;
        double* rate = readwrite ? &_rwRate : &_roRate;
        NSDate* __strong* bucket = readwrite ? &_rwBucket : &_roBucket;

        NSDate* now = [NSDate now];
        NSDate* fullBucket = [now dateByAddingTimeInterval:-(*capacity * (1.0 / *rate))];
        // bucket has more tokens than a 'full' bucket? This prevents occasional-but-bursty activity slipping through
        if (!*bucket || [*bucket timeIntervalSinceDate: fullBucket] < 0) {
            *bucket = fullBucket;
        }

        *bucket = [*bucket dateByAddingTimeInterval:1.0 / *rate];
        ok = [*bucket timeIntervalSinceDate:now] <= 0;

        // Get a new bucket next time so we only complain every now and then
        if (!ok) {
            *bucket = nil;
            *capacity *= _limitMultiplier;
            *rate *= _limitMultiplier;
        }
    });

    return ok;
}

- (bool)shouldCountAPICalls {
    static bool shouldCount = false;
    static dispatch_once_t shouldCountToken;
    dispatch_once(&shouldCountToken, ^{
        if (!SecIsInternalRelease()) {
            secnotice("secitemratelimit", "Not internal release, disabling SIRL");
            return;
        }

        // gSecurityd is the XPC elision mechanism for testing; don't want simcrashes during tests
        if (gSecurityd != nil) {
            secnotice("secitemratelimit", "gSecurityd non-nil, disabling SIRL for testing");
            return;
        }

        if (!os_feature_enabled(Security, SecItemRateLimiting)) {
            secnotice("secitemratelimit", "SIRL disabled via feature flag");
            return;
        }

        SecTaskRef task = SecTaskCreateFromSelf(NULL);
        NSMutableArray* exempt = [NSMutableArray arrayWithArray:@[@"com.apple.pcsstatus", @"com.apple.protectedcloudstorage.protectedcloudkeysyncing", @"com.apple.cloudd", @"com.apple.pcsctl"]];
        CFStringRef identifier = NULL;
        require_action_quiet(task, cleanup, secerror("secitemratelimit: unable to get task from self, disabling SIRL"));

        // If not a valid or debugged platform binary, don't count
        uint32_t flags = SecTaskGetCodeSignStatus(task);
        require_action_quiet((flags & (CS_VALID | CS_PLATFORM_BINARY | CS_PLATFORM_PATH)) == (CS_VALID | CS_PLATFORM_BINARY) ||
                             (flags & (CS_DEBUGGED | CS_PLATFORM_BINARY | CS_PLATFORM_PATH)) == (CS_DEBUGGED | CS_PLATFORM_BINARY),
                             cleanup, secnotice("secitemratelimit", "Not valid/debugged platform binary, disabling SIRL"));

        // Some processes have legitimate need to query or modify a large number of items
        identifier = SecTaskCopySigningIdentifier(task, NULL);
        require_action_quiet(identifier, cleanup, secerror("secitemratelimit: unable to get signing identifier, disabling SIRL"));
#if TARGET_OS_OSX
        [exempt addObjectsFromArray:@[@"com.apple.keychainaccess", @"com.apple.Safari"]];
#elif TARGET_OS_IOS
        [exempt addObjectsFromArray:@[@"com.apple.mobilesafari", @"com.apple.Preferences"]];
#endif
        if ([exempt containsObject:(__bridge NSString*)identifier]) {
            secnotice("secitemratelimit", "%@ exempt from SIRL", identifier);
            goto cleanup;
        }

        secnotice("secitemratelimit", "valid/debugged platform binary %@ on internal release, enabling SIRL", identifier);
        shouldCount = true;

cleanup:
        CFReleaseNull(task);
        CFReleaseNull(identifier);
    });
    return shouldCount;
}

// Testing
+ (instancetype)getStaticRateLimit {
    return [SecItemRateLimit instance];
}

// Testing and super thread-UNsafe. Caveat emptor.
+ (void)resetStaticRateLimit {
	ratelimit = [SecItemRateLimit new];
}

@end

bool isReadOnlyAPIRateWithinLimits(void) {
    return [[SecItemRateLimit instance] isReadOnlyAPICallWithinLimits];
}

bool isModifyingAPIRateWithinLimits(void) {
    return [[SecItemRateLimit instance] isModifyingAPICallWithinLimits];
}
