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

#import "ipc/SecdWatchdog.h"
#include "utilities/debugging.h"
#include <xpc/private.h>
#import <xpc/private.h>
#import <libproc.h>
#import <os/log.h>
#import <mach/mach_time.h>
#import <mach/message.h>
#import <os/assumes.h>

#if !TARGET_OS_MAC
#import <CrashReporterSupport/CrashReporterSupport.h>
#endif

#define CPU_RUNTIME_SECONDS_BEFORE_WATCHDOG (60 * 20)
#define WATCHDOG_RESET_PERIOD (60 * 60 * 24)
#define WATCHDOG_CHECK_PERIOD (60 * 60)
#define WATCHDOG_CHECK_PERIOD_LEEWAY (60 * 10)
#define WATCHDOG_GRACEFUL_EXIT_LEEWAY (60 * 5)
#define WATCHDOG_DISKUSAGE_LIMIT (1000 * 1024 * 1024) // (1GiBi)

NSString* const SecdWatchdogAllowedRuntime = @"allowed-runtime";
NSString* const SecdWatchdogResetPeriod = @"reset-period";
NSString* const SecdWatchdogCheckPeriod = @"check-period";
NSString* const SecdWatchdogGracefulExitTime = @"graceful-exit-time";

void SecdLoadWatchDog()
{
    (void)[SecdWatchdog watchdog];
}

@implementation SecdWatchdog {
    uint64_t _rusageBaseline;
    CFTimeInterval _lastCheckTime;
    dispatch_source_t _timer;

    uint64_t _runtimeSecondsBeforeWatchdog;
    long _resetPeriod;
    long _checkPeriod;
    long _checkPeriodLeeway;
    long _gracefulExitLeeway;
    uint64_t _diskUsageBaseLine;
    uint64_t _diskUsageLimit;

    bool _diskUsageHigh;
}

@synthesize diskUsageHigh = _diskUsageHigh;

+ (instancetype)watchdog
{
    static SecdWatchdog* watchdog = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        watchdog = [[self alloc] init];
    });

    return watchdog;
}

- (instancetype)init
{
    if (self = [super init]) {
        _runtimeSecondsBeforeWatchdog = CPU_RUNTIME_SECONDS_BEFORE_WATCHDOG;
        _resetPeriod = WATCHDOG_RESET_PERIOD;
        _checkPeriod = WATCHDOG_CHECK_PERIOD;
        _checkPeriodLeeway = WATCHDOG_CHECK_PERIOD_LEEWAY;
        _gracefulExitLeeway = WATCHDOG_GRACEFUL_EXIT_LEEWAY;
        _diskUsageLimit = WATCHDOG_DISKUSAGE_LIMIT;
        _diskUsageHigh = false;

        [self activateTimer];
    }

    return self;
}

- (uint64_t)secondsFromMachTime:(uint64_t)machTime
{
    static dispatch_once_t once;
    static uint64_t ratio;
    dispatch_once(&once, ^{
        mach_timebase_info_data_t tbi;
        if (os_assumes_zero(mach_timebase_info(&tbi)) == KERN_SUCCESS) {
            ratio = tbi.numer / tbi.denom;
        } else {
            ratio = 1;
        }
    });

    return (machTime * ratio)/NSEC_PER_SEC;
}

+ (bool)watchdogrusage:(rusage_info_current *)rusage
{
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_CURRENT, (rusage_info_t *)rusage) != 0) {
        return false;
    }
    return true;
}

+ (bool)triggerOSFaults
{
    return true;
}

- (void)runWatchdog
{
    rusage_info_current currentRusage;

    if (![[self class] watchdogrusage:&currentRusage]) {
        return;
    }

    @synchronized (self) {
        uint64_t spentUserTime = [self secondsFromMachTime:currentRusage.ri_user_time];
        if (spentUserTime > _rusageBaseline + _runtimeSecondsBeforeWatchdog) {
            seccritical("SecWatchdog: watchdog has detected securityd/secd is using too much CPU - attempting to exit gracefully");
#if !TARGET_OS_MAC
            WriteStackshotReport(@"securityd watchdog triggered", __sec_exception_code_Watchdog);
#endif
            xpc_transaction_exit_clean(); // we've  used too much CPU - try to exit gracefully

            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(_gracefulExitLeeway * NSEC_PER_SEC)), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                // if we still haven't exited gracefully after 5 minutes, time to die unceremoniously
                seccritical("SecWatchdog: watchdog has failed to exit securityd/secd gracefully - exiting ungracefully");
                exit(EXIT_FAILURE);
            });

            return;
        }

        if (_diskUsageHigh == false &&
            (currentRusage.ri_logical_writes > _diskUsageBaseLine + _diskUsageLimit))
        {
            if ([[self class] triggerOSFaults]) {
                os_log_fault(OS_LOG_DEFAULT, "securityd have written more then %llu",
                             (unsigned long long)_diskUsageLimit);
            }
            _diskUsageHigh = true;
        }

        CFTimeInterval currentTime = CFAbsoluteTimeGetCurrent();
        if (currentTime > _lastCheckTime + _resetPeriod) {
            // we made it through a 24 hour period - reset our timeout to 24 hours from now, and our cpu usage threshold to another 20 minutes
            secinfo("SecWatchdog", "resetting watchdog monitoring interval ahead another 24 hours");
            _lastCheckTime = currentTime;
            _rusageBaseline = spentUserTime;
            _diskUsageHigh = false;
            _diskUsageBaseLine = currentRusage.ri_logical_writes;
        }
    }

}

- (void)activateTimer
{
    @synchronized (self) {

        rusage_info_current initialRusage;
        [[self class] watchdogrusage:&initialRusage];

        _rusageBaseline = [self secondsFromMachTime:initialRusage.ri_user_time];
        _lastCheckTime = CFAbsoluteTimeGetCurrent();

        __weak __typeof(self) weakSelf = self;
        _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
        dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, _checkPeriod * NSEC_PER_SEC, _checkPeriodLeeway * NSEC_PER_SEC); // run once every hour, but give the timer lots of leeway to be power friendly
        dispatch_source_set_event_handler(_timer, ^{
            __strong __typeof(self) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }
            [strongSelf runWatchdog];
        });
        dispatch_resume(_timer);
    }
}

- (NSDictionary*)watchdogParameters
{
    @synchronized (self) {
        return @{ SecdWatchdogAllowedRuntime : @(_runtimeSecondsBeforeWatchdog),
                  SecdWatchdogResetPeriod : @(_resetPeriod),
                  SecdWatchdogCheckPeriod : @(_checkPeriod),
                  SecdWatchdogGracefulExitTime : @(_gracefulExitLeeway) };
    }
}

- (BOOL)setWatchdogParameters:(NSDictionary*)parameters error:(NSError**)error
{
    NSMutableArray* failedParameters = [NSMutableArray array];
    @synchronized (self) {
        __weak __typeof(self) weakSelf = self;
        [parameters enumerateKeysAndObjectsUsingBlock:^(NSString* parameter, NSNumber* value, BOOL* stop) {
            __strong __typeof(self) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            if ([parameter isEqualToString:SecdWatchdogAllowedRuntime] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_runtimeSecondsBeforeWatchdog = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogResetPeriod] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_resetPeriod = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogCheckPeriod] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_checkPeriod = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogGracefulExitTime] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_gracefulExitLeeway = value.longValue;
            }
            else {
                [failedParameters addObject:parameter];
            }
        }];

        dispatch_source_cancel(_timer);
        _timer = NULL;
    }

    [self activateTimer];

    if (failedParameters.count > 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.apple.securityd.watchdog" code:0 userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"failed to set parameters: %@", failedParameters]}];
        }

        return NO;
    }
    else {
        return YES;
    }
}

@end
