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
#include <Foundation/Foundation.h>
#include "SecPLWrappers.h"

#if !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
#include <PowerLog/PowerLog.h>

static typeof(PLShouldLogRegisteredEvent) *soft_PLShouldLogRegisteredEvent = NULL;
static typeof(PLLogRegisteredEvent) *soft_PLLogRegisteredEvent = NULL;
static typeof(PLLogTimeSensitiveRegisteredEvent) *soft_PLLogTimeSensitiveRegisteredEvent = NULL;

static bool
setup(void)
{
    static dispatch_once_t onceToken;
    static CFBundleRef bundle = NULL;
    dispatch_once(&onceToken, ^{

        CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/PowerLog.framework"), kCFURLPOSIXPathStyle, true);
        if (url == NULL)
            return;

        bundle = CFBundleCreate(kCFAllocatorDefault, url);
        CFRelease(url);
        if (bundle == NULL)
            return;

        soft_PLShouldLogRegisteredEvent = CFBundleGetFunctionPointerForName(bundle, CFSTR("PLShouldLogRegisteredEvent"));
        soft_PLLogRegisteredEvent = CFBundleGetFunctionPointerForName(bundle, CFSTR("PLLogRegisteredEvent"));
        soft_PLLogTimeSensitiveRegisteredEvent = CFBundleGetFunctionPointerForName(bundle, CFSTR("PLLogTimeSensitiveRegisteredEvent"));

        if (soft_PLShouldLogRegisteredEvent == NULL ||
            soft_PLLogRegisteredEvent == NULL ||
            soft_PLLogTimeSensitiveRegisteredEvent == NULL)
        {
            CFRelease(bundle);
            bundle = NULL;
        }
    });
    return bundle != NULL;
}

#endif

bool SecPLShouldLogRegisteredEvent(NSString *event)
{
#if !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
    if (setup())
        return soft_PLShouldLogRegisteredEvent(PLClientIDSecurity, (__bridge CFStringRef)event);
#endif
    return false;
}

void SecPLLogRegisteredEvent(NSString *eventName, NSDictionary *eventDictionary)
{
#if !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
    if (setup())
        soft_PLLogRegisteredEvent(PLClientIDSecurity,
                                  (__bridge CFStringRef)eventName,
                                  (__bridge CFDictionaryRef)eventDictionary,
                                  NULL);
#endif
}

void SecPLLogTimeSensitiveRegisteredEvent(NSString *eventName, NSDictionary *eventDictionary)
{
#if !TARGET_OS_SIMULATOR && !TARGET_OS_BRIDGE
    if (setup())
        soft_PLLogTimeSensitiveRegisteredEvent(PLClientIDSecurity,
                                               (__bridge CFStringRef)eventName,
                                               (__bridge CFDictionaryRef)eventDictionary,
                                               NULL);
#endif
}

