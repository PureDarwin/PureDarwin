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


#ifndef _SECCFERROR_H_
#define _SECCFERROR_H_

#include <CoreFoundation/CoreFoundation.h>
#include "utilities/SecCFRelease.h"

//
// Leaf error creation from other systems
//

// kern_return_t errors
#define kSecKernDomain  kCFErrorDomainMach
bool SecKernError(kern_return_t result, CFErrorRef *error, CFStringRef format, ...)
   CF_FORMAT_FUNCTION(3, 4);

// Unix errno errors
#define kSecErrnoDomain  kCFErrorDomainPOSIX
bool SecCheckErrno(int result, CFErrorRef *error, CFStringRef format, ...)
    CF_FORMAT_FUNCTION(3, 4);

// OSStatus errors
#define kSecErrorDomain  kCFErrorDomainOSStatus
bool SecError(OSStatus status, CFErrorRef *error, CFStringRef format, ...)
   CF_FORMAT_FUNCTION(3, 4);

// requirement error, typically parameters
bool SecRequirementError(bool requirement, CFErrorRef *error, CFStringRef format, ...)
    CF_FORMAT_FUNCTION(3, 4);

// Allocation failure
bool SecAllocationError(const void *allocated, CFErrorRef *error, CFStringRef format, ...)
    CF_FORMAT_FUNCTION(3, 4);

//
// Create and chain, all return false to make the analyzer happy.
//
#define SecCFCreateError(errorCode, domain, descriptionString, previousError, newError) \
    SecCFCreateErrorWithFormat(errorCode, domain, previousError, newError, NULL, descriptionString)

bool SecCFCreateErrorWithFormat(CFIndex errorCode, CFStringRef domain, CFErrorRef previousError, CFErrorRef *newError,
                                CFDictionaryRef formatoptions, CFStringRef descriptionString, ...)
                        CF_FORMAT_FUNCTION(6,7);


bool SecCFCreateErrorWithFormatAndArguments(CFIndex errorCode, CFStringRef domain,
                                            CFErrorRef previousError, CFErrorRef *newError,
                                            CFDictionaryRef formatoptions, CFStringRef descriptionString, va_list args)
                                CF_FORMAT_FUNCTION(6, 0);

//
// MARK: Optional value type casting
//

static inline bool asArrayOptional(CFTypeRef cfType, CFArrayRef *array, CFErrorRef *error) {
    if (!cfType || CFGetTypeID(cfType) == CFArrayGetTypeID()) {
        if (array) *array = (CFArrayRef)cfType;
        return true;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not an array"), cfType);
    }
    return false;
}

static inline bool asDataOptional(CFTypeRef cfType, CFDataRef *data, CFErrorRef *error) {
    if (!cfType || CFGetTypeID(cfType) == CFDataGetTypeID()) {
        if (data) *data = (CFDataRef)cfType;
        return true;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not an data"), cfType);
    }
    return false;
}

//
// MARK: Required value type casting
//

static inline CFDataRef copyIfData(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFDataGetTypeID()) {
        return (CFDataRef)CFRetainSafe(cfType);
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a data"), cfType);
    }
    return NULL;
}

static inline CFSetRef copyIfSet(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFSetGetTypeID()) {
        return (CFSetRef)CFRetainSafe(cfType);
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a set"), cfType);
    }
    return NULL;
}

//
// MARK: Analyzer confusing asXxx casting
//
static inline CFArrayRef asArray(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFArrayGetTypeID()) {
        return (CFArrayRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not an array"), cfType);
    }
    return NULL;
}

static inline CFBooleanRef asBoolean(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFBooleanGetTypeID()) {
        return (CFBooleanRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not an boolean"), cfType);
    }
    return NULL;
}

static inline CFDataRef asData(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFDataGetTypeID()) {
        return (CFDataRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a data"), cfType);
    }
    return NULL;
}

static inline CFDateRef asDate(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFDateGetTypeID()) {
        return (CFDateRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a date"), cfType);
    }
    return NULL;
}

static inline CFDictionaryRef asDictionary(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFDictionaryGetTypeID()) {
        return (CFDictionaryRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a dictionary"), cfType);
    }
    return NULL;
}

static inline CFSetRef asSet(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFSetGetTypeID()) {
        return (CFSetRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a set"), cfType);
    }
    return NULL;
}

static inline CFStringRef asString(CFTypeRef cfType, CFErrorRef *error) {
    if (cfType && CFGetTypeID(cfType) == CFStringGetTypeID()) {
        return (CFStringRef)cfType;
    }
    if(error) {
        SecError(-50, error, CFSTR("object %@ is not a string"), cfType);
    }
    return NULL;
}

#endif /* _SECCFERROR_H_ */
