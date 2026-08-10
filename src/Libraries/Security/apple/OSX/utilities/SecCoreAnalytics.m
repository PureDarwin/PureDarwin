/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "SecCoreAnalytics.h"
#import <CoreAnalytics/CoreAnalytics.h>
#import <SoftLinking/SoftLinking.h>
#import <Availability.h>
#import <sys/sysctl.h>

NSString* const SecCoreAnalyticsValue = @"value";


void SecCoreAnalyticsSendValue(CFStringRef _Nonnull eventName, int64_t value)
{
    [SecCoreAnalytics sendEvent:(__bridge NSString*)eventName
                          event:@{
                              SecCoreAnalyticsValue: [NSNumber numberWithLong:value],
                          }];
}

void SecCoreAnalyticsSendKernEntropyHealth()
{
    size_t sz_int = sizeof(int);
    size_t sz_uint = sizeof(unsigned int);
    size_t sz_tv = sizeof(struct timeval);

    int startup_done;
    unsigned int adaptive_proportion_failure_count = 0;
    unsigned int adaptive_proportion_max_observation_count = 0;
    unsigned int adaptive_proportion_reset_count = 0;
    unsigned int repetition_failure_count = 0;
    unsigned int repetition_max_observation_count = 0;
    unsigned int repetition_reset_count = 0;

    int rv = sysctlbyname("kern.entropy.health.startup_done", &startup_done, &sz_int, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.adaptive_proportion_test.failure_count", &adaptive_proportion_failure_count, &sz_uint, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.adaptive_proportion_test.max_observation_count", &adaptive_proportion_max_observation_count, &sz_uint, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.adaptive_proportion_test.reset_count", &adaptive_proportion_reset_count, &sz_uint, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.repetition_count_test.failure_count", &repetition_failure_count, &sz_uint, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.repetition_count_test.max_observation_count", &repetition_max_observation_count, &sz_uint, NULL, 0);
    rv |= sysctlbyname("kern.entropy.health.repetition_count_test.reset_count", &repetition_reset_count, &sz_uint, NULL, 0);

    // Round up to next power of two.
    if (adaptive_proportion_reset_count > 0) {
        adaptive_proportion_reset_count =
            1U << (sizeof(unsigned int) * 8 - __builtin_clz(adaptive_proportion_reset_count));
    }

    // Round up to next power of two.
    if (repetition_reset_count > 0) {
        repetition_reset_count =
            1U << (sizeof(unsigned int) * 8 - __builtin_clz(repetition_reset_count));
    }

    // Default to not submitting uptime, except on failure.
    int uptime = -1;

    if (adaptive_proportion_failure_count > 0 || repetition_failure_count > 0) {
        time_t now;
        time(&now);

        struct timeval boottime;
        int mib[2] = { CTL_KERN, KERN_BOOTTIME };
        rv |= sysctl(mib, 2, &boottime, &sz_tv, NULL, 0);

        // Submit uptime in minutes.
        uptime = (int)((now - boottime.tv_sec) / 60);
    }

    if (rv) {
        return;
    }

    [SecCoreAnalytics sendEventLazy:@"com.apple.kern.entropyHealth" builder:^NSDictionary<NSString *,NSObject *> * _Nonnull{
        return @{
            @"uptime" : @(uptime),
            @"startup_done" : @(startup_done),
            @"adaptive_proportion_failure_count" : @(adaptive_proportion_failure_count),
            @"adaptive_proportion_max_observation_count" : @(adaptive_proportion_max_observation_count),
            @"adaptive_proportion_reset_count" : @(adaptive_proportion_reset_count),
            @"repetition_failure_count" : @(repetition_failure_count),
            @"repetition_max_observation_count" : @(repetition_max_observation_count),
            @"repetition_reset_count" : @(repetition_reset_count)
        };
    }];
}

@implementation SecCoreAnalytics

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CoreAnalytics);

SOFT_LINK_FUNCTION(CoreAnalytics, AnalyticsSendEvent, soft_AnalyticsSendEvent, \
    void, (NSString* eventName, NSDictionary<NSString*,NSObject*>* eventPayload),(eventName, eventPayload));
SOFT_LINK_FUNCTION(CoreAnalytics, AnalyticsSendEventLazy, soft_AnalyticsSendEventLazy, \
    void, (NSString* eventName, NSDictionary<NSString*,NSObject*>* (^eventPayloadBuilder)(void)),(eventName, eventPayloadBuilder));

+ (void)sendEvent:(NSString*) eventName event:(NSDictionary<NSString*,NSObject*>*)event
{
    if (isCoreAnalyticsAvailable()) {
        soft_AnalyticsSendEvent(eventName, event);
    }
}

+ (void)sendEventLazy:(NSString*) eventName builder:(NSDictionary<NSString*,NSObject*>* (^)(void))builder
{
    if (isCoreAnalyticsAvailable()) {
        soft_AnalyticsSendEventLazy(eventName, builder);
    }
}

@end
