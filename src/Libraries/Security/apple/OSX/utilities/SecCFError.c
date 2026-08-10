/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>
#include <notify.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <Security/OTConstants.h>

//
// OSStatus values we magically know
//
enum {
    parameterError                = -50,     /* One or more parameters passed to a function were not valid. */
    allocationError               = -108,    /* Failed to allocate memory. */
};

bool SecKernError(kern_return_t result, CFErrorRef *error, CFStringRef format, ...) {
    if (!result) return true;
    if (error) {
        va_list args;
        CFIndex code = result;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(code, kSecKernDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

bool SecCheckErrno(int result, CFErrorRef *error, CFStringRef format, ...) {
    if (result == 0) return true;
    if (error) {
        va_list args;
        int errnum = errno;
        CFIndex code = errnum;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);
        SecCFCreateErrorWithFormat(code, kSecErrnoDomain, previousError, error, NULL, CFSTR("%@: [%d] %s"), message, errnum, strerror(errnum));
        CFReleaseSafe(message);
    }
    return false;
}

bool SecError(OSStatus status, CFErrorRef *error, CFStringRef format, ...) {
    if (status == 0) {
        return true;
    }

    CFErrorRef localError = NULL;
    va_list args;
    CFIndex code = status;
    va_start(args, format);
    SecCFCreateErrorWithFormatAndArguments(code, kSecErrorDomain, error ? *error : NULL, &localError, NULL, format, args);
    va_end(args);

    if (error) {
        *error = localError; // Existing *error is consumed by SecCFCreateErrorWithFormatAndArguments
    } else {
        // This happens a bunch in our codebase, so this log can only really exist in debug builds
        secdebug("secerror", "Error, but no out-parameter for error: %@", localError);
        CFReleaseNull(localError);
    }

    return false;
}

// Parameter error
bool SecRequirementError(bool requirement, CFErrorRef *error, CFStringRef format, ...) {
    if (requirement) return true;
    if (error) {
        va_list args;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(parameterError, kSecErrorDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

// Allocation failure
bool SecAllocationError(const void *allocated, CFErrorRef *error, CFStringRef format, ...) {
    if (allocated) return true;
    if (error) {
        va_list args;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(allocationError, kSecErrorDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

bool SecCFCreateErrorWithFormat(CFIndex errorCode, CFStringRef domain, CFErrorRef previousError, CFErrorRef *newError,
                                CFDictionaryRef formatoptions, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);

    bool result = SecCFCreateErrorWithFormatAndArguments(errorCode, domain, previousError, newError, formatoptions, format, args);

    va_end(args);

    return result;
}

static bool SecCFErrorIsEqual(CFIndex errorCode, CFStringRef domain, CFStringRef description, CFErrorRef previousError)
{
    bool isEqual = false;
    bool equalDescriptions = false;

    if (previousError == NULL) {
        return false;
    }

    CFDictionaryRef previousUserInfo = CFErrorCopyUserInfo(previousError);
    CFStringRef previousDescription = CFDictionaryGetValue(previousUserInfo, kCFErrorDescriptionKey);
    if (previousDescription) {
        equalDescriptions = CFStringCompare(description, previousDescription, 0) == kCFCompareEqualTo ? true : false;
    }

    CFReleaseNull(previousUserInfo);
    bool equalCodes = errorCode == CFErrorGetCode(previousError);

    CFErrorDomain previousDomain = CFErrorGetDomain(previousError);
    bool equalDomains = CFStringCompare(domain, previousDomain, 0) == kCFCompareEqualTo ? true : false;

    isEqual = equalCodes && equalDomains && equalDescriptions;
    return isEqual;
}

#define CAP_LIMIT   200
static bool SecCFErrorShouldCapNestedError(CFErrorRef previousError, long *newCount)
{
    bool shouldCap = false;

    if (previousError) {
        CFDictionaryRef userInfo = CFErrorCopyUserInfo(previousError);
        if (userInfo && CFDictionaryContainsKey(userInfo, kSOSCountKey) == true) {
            CFNumberRef previousCount = CFDictionaryGetValue(userInfo, kSOSCountKey);
            if (previousCount) {
                long previousLong = 0;
                CFNumberGetValue(previousCount, kCFNumberLongType, &previousLong);
                if (SecErrorIsNestedErrorCappingEnabled() && previousLong >= CAP_LIMIT) {
                    shouldCap = true;
                } else {
                    *newCount = previousLong+1;
                }
            }
        }
        CFReleaseNull(userInfo);
    } else {
        *newCount = 0;
    }
    return shouldCap;
}

// Also consumes whatever newError points to
bool SecCFCreateErrorWithFormatAndArguments(CFIndex errorCode, CFStringRef domain,
                                            CF_CONSUMED CFErrorRef previousError, CFErrorRef *newError,
                                            CFDictionaryRef formatoptions, CFStringRef format, va_list args)
{
    if (newError && !(*newError)) {
        CFStringRef formattedString = CFStringCreateWithFormatAndArguments(NULL, formatoptions, format, args);

        long newDepthCount = 0;
        CFNumberRef newCount = NULL;

        if (SecCFErrorIsEqual(errorCode, domain, formattedString, previousError) == true) {
            secdebug("error_thee_well", "SecCFCreateErrorWithFormatAndArguments previous Error: %@ is equal to the new incoming error: domain: %@, error code: %ld, description: %@", previousError, domain, (long)errorCode, formattedString);
            *newError = CFRetainSafe(previousError);
            CFReleaseNull(previousError);
            CFReleaseNull(formattedString);
            return false;
        } else if (SecCFErrorShouldCapNestedError(previousError, &newDepthCount) == true) {
            secdebug("error_thee_well", "SecCFCreateErrorWithFormatAndArguments reached nested error limit, returning previous error: %@", previousError);
            *newError = CFRetainSafe(previousError);
            CFReleaseNull(previousError);
            CFReleaseNull(formattedString);
            return false;
        } else {
            newCount = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &newDepthCount);
        }

        CFMutableDictionaryRef newUserInfo = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

        if (previousError) {
            CFDictionaryAddValue(newUserInfo, kCFErrorUnderlyingErrorKey, previousError);
        }
        if (newCount) {
            CFDictionaryAddValue(newUserInfo, kSOSCountKey, newCount);
        }
        if (formattedString) {
            CFDictionaryAddValue(newUserInfo, kCFErrorDescriptionKey, formattedString);
        }

        *newError = CFErrorCreate(kCFAllocatorDefault, domain, errorCode, newUserInfo);

        if (previousError) {
            secdebug("error_thee_well", "encapsulated %@ with new error: %@", previousError, *newError);
        }
        CFReleaseNull(newCount);
        CFReleaseNull(formattedString);
        CFReleaseNull(newUserInfo);
        CFReleaseNull(previousError);
    } else {
        if (previousError && newError && (previousError != *newError)) {
            secdebug("error_thee_well", "dropping %@", previousError);
            CFReleaseNull(previousError);
        }
    }

    if (newError) {
        secdebug("error_thee_well", "SecError: %@", *newError);
    }

    return false;
}
