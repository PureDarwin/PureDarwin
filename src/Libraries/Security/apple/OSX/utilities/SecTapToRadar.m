/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "SecTapToRadar.h"
#import "utilities/debugging.h"

#if TARGET_OS_IOS

#import <MobileCoreServices/LSApplicationWorkspace.h>
#import <CoreFoundation/CFUserNotification.h>
#import <SoftLinking/SoftLinking.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
SOFT_LINK_FRAMEWORK(Frameworks, MobileCoreServices)
SOFT_LINK_CLASS(MobileCoreServices, LSApplicationWorkspace);
#pragma clang diagnostic pop

#endif

#include "utilities/SecInternalReleasePriv.h"

static NSString* kSecNextTTRDate = @"NextTTRDate";
static NSString* kSecPreferenceDomain = @"com.apple.security";

@interface SecTapToRadar ()
@property (readwrite) NSString *alert;
@property (readwrite) NSString *radarDescription;
@property (readwrite) NSString *radarnumber;
@property (readwrite) dispatch_queue_t queue;

@end

static BOOL SecTTRDisabled = true;

@implementation SecTapToRadar

- (instancetype)initTapToRadar:(NSString *)alert
                   description:(NSString *)radarDescription
                          radar:(NSString *)radarnumber
{
    if ((self = [super init]) == nil) {
        return nil;
    }

    _alert = alert;
    _radarDescription = radarDescription;
    _radarnumber = radarnumber;
    _queue = dispatch_queue_create("com.apple.security.diagnostic-queue", 0);
    dispatch_set_target_queue(_queue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));

    _componentName = @"Security";
    _componentVersion = @"all";
    _componentID = @"606179";

    return self;
}

+ (NSString *)keyname:(SecTapToRadar *)ttrRequest
{
    return [NSString stringWithFormat:@"%@-%@", kSecNextTTRDate, ttrRequest.radarnumber];
}

+ (BOOL)isRateLimited:(SecTapToRadar *)ttrRequest
{
    return SecTTRDisabled || [ttrRequest isRateLimited];
}

- (BOOL)isRateLimited
{
    NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:kSecPreferenceDomain];

    NSString *key = [[self class] keyname: self];
    NSDate *val = [defaults valueForKey:key];
    if (![val isKindOfClass:[NSDate class]]) {
        [defaults removeObjectForKey:key];
        return NO;
    }

    if ([val compare:[NSDate date]] == NSOrderedAscending) {
        return NO;
    }

    return YES;
}

+ (void)disableTTRsEntirely {
    SecTTRDisabled = YES;
}


- (void)updateRetryTimestamp
{
    NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:kSecPreferenceDomain];
    [defaults setObject:[[NSDate date] dateByAddingTimeInterval:24*3600.0]
                 forKey:[[self class] keyname: self]];
}

- (void)clearRetryTimestamp
{
    NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:kSecPreferenceDomain];
    [defaults removeObjectForKey:[[self class] keyname: self]];
}


+ (void)triggerTapToRadar:(SecTapToRadar *)ttrRequest
{
    secnotice("secttr", "Triggering TTR: %@", ttrRequest.alert);
    dispatch_assert_queue(ttrRequest.queue);

#if TARGET_OS_IOS
    static dispatch_once_t onceToken;
    static NSMutableCharacterSet *queryComponent;
    dispatch_once(&onceToken, ^{
        queryComponent = [[NSMutableCharacterSet alloc] init];
        [queryComponent formUnionWithCharacterSet:[NSCharacterSet URLQueryAllowedCharacterSet]];
        [queryComponent removeCharactersInString:@"&"];
    });

    Class workspaceCls = getLSApplicationWorkspaceClass();
    if (workspaceCls == NULL) {
        return;
    }

    NSString *title = [NSString stringWithFormat:@"Triggered SecTTR: %@ - %@", ttrRequest.alert, ttrRequest.radarnumber];
    NSString *encodedTitle = [title stringByAddingPercentEncodingWithAllowedCharacters:queryComponent];

    NSString *desc = [NSString stringWithFormat:@"%@\nRelated radar: radr://%@", ttrRequest.radarDescription, ttrRequest.radarnumber];
    NSString *encodedDesc = [desc stringByAddingPercentEncodingWithAllowedCharacters:queryComponent];

    NSString *url = [NSString stringWithFormat:@"tap-to-radar://new?"
                     "Reproducibilty=Always&"
                     "Title=%@&"
                     "ComponentName=%@&"
                     "ComponentVersion=%@&"
                     "Reproducibility=Not%%20Applicable&"
                     "ComponentID=%@&"
                     "Classification=Crash/Hang/Data%%20Loss&"
                     "Description=%@",
                     encodedTitle,
                     [ttrRequest.componentName stringByAddingPercentEncodingWithAllowedCharacters:queryComponent],
                     [ttrRequest.componentVersion stringByAddingPercentEncodingWithAllowedCharacters:queryComponent],
                     [ttrRequest.componentID stringByAddingPercentEncodingWithAllowedCharacters:queryComponent],
                     encodedDesc];

    NSURL *tapToRadarURL = [NSURL URLWithString:url];
    [[workspaceCls defaultWorkspace] openURL:tapToRadarURL configuration:nil completionHandler:^(NSDictionary<NSString *,id> * __unused result, NSError * __unused error) {
    }];
#endif
}

+ (BOOL)askUserIfTTR:(SecTapToRadar *)ttrRequest
{
    BOOL result = NO;

#if TARGET_OS_IOS
    NSDictionary *alertOptions = @{
        (NSString *)kCFUserNotificationDefaultButtonTitleKey : @"Tap-To-Radar",
        (NSString *)kCFUserNotificationAlternateButtonTitleKey : @"Go away",
        (NSString *)kCFUserNotificationAlertMessageKey : ttrRequest.alert,
        (NSString *)kCFUserNotificationAlertHeaderKey : @"Security",
    };

    SInt32 error = 0;
    CFUserNotificationRef notification = CFUserNotificationCreate(NULL, 0, kCFUserNotificationPlainAlertLevel, &error, (__bridge CFDictionaryRef)alertOptions);
    if (notification != NULL) {
        CFOptionFlags responseFlags = 0;
        CFUserNotificationReceiveResponse(notification, 1.0 * 60 * 3, &responseFlags);
        switch (responseFlags & 0x03) {
        case kCFUserNotificationDefaultResponse:
            result = YES;
            break;
        default:
            break;
        }
        CFRelease(notification);
    } else {
        secnotice("SecTTR", "Failed to create notification error %@", @(error));
    }
#endif
    return result;
}

- (void)trigger
{
    dispatch_sync(self.queue, ^{
        @autoreleasepool {
            if (!SecIsInternalRelease()) {
                return;
            }

            if ([[self class] isRateLimited:self]) {
                secnotice("SecTTR", "Not showing ttr due to ratelimiting: %@", self.alert);
                return;
            }

            if ([[self class] askUserIfTTR:self]) {
                [[self class] triggerTapToRadar:self];
            }
            [self updateRetryTimestamp];
        }
    });
}

@end
