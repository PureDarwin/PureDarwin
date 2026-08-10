/*
 * Copyright (c) 2006-2017 Apple Inc. All Rights Reserved.
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

/*
 * SecBase.c
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include <Availability.h>
#include "SecFramework.h"
#include <dispatch/dispatch.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURLAccess.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecBase.h>
#include <inttypes.h>

#if !TARGET_OS_OSX

static CFStringRef copyErrorMessageFromBundle(OSStatus status, CFStringRef tableName);

// caller MUST release the string, since it is gotten with "CFCopyLocalizedStringFromTableInBundle"
// intended use of reserved param is to pass in CFStringRef with name of the Table for lookup
// Will look by default in "SecErrorMessages.strings" in the resources of Security.framework.

CFStringRef
SecCopyErrorMessageString(OSStatus status, void *reserved)
{
    CFStringRef result = copyErrorMessageFromBundle(status, CFSTR("SecErrorMessages"));
    if (!result)
        result = copyErrorMessageFromBundle(status, CFSTR("SecDebugErrorMessages"));

    if (!result)
    {
        // no error message found, so format a faked-up error message from the status
        result = CFStringCreateWithFormat(NULL, NULL, CFSTR("OSStatus %d"), (int)status);
    }

    return result;
}

CFStringRef
copyErrorMessageFromBundle(OSStatus status,CFStringRef tableName)
{

    CFStringRef errorString = nil;
    CFStringRef keyString = nil;
    CFBundleRef secBundle = NULL;

    // Make a bundle instance using the URLRef.
    secBundle = CFBundleGetBundleWithIdentifier(kSecFrameworkBundleID);
    if (!secBundle)
        goto exit;

    // Convert status to Int32 string representation, e.g. "-25924"
    keyString = CFStringCreateWithFormat (kCFAllocatorDefault, NULL, CFSTR("%d"), (int)status);
    if (!keyString)
        goto exit;

    errorString = CFCopyLocalizedStringFromTableInBundle(keyString, tableName, secBundle, NULL);
    if (CFStringCompare(errorString, keyString, 0) == kCFCompareEqualTo)    // no real error message
    {
        if (errorString)
            CFRelease(errorString);
        errorString = nil;
    }
exit:
    if (keyString)
        CFRelease(keyString);

    return errorString;
}

const SecRandomRef kSecRandomDefault = NULL;

int SecRandomCopyBytes(__unused SecRandomRef rnd, size_t count, void *bytes) {
    return CCRandomCopyBytes(kCCRandomDefault, bytes, count);
}

#endif // TARGET_OS_OSX
