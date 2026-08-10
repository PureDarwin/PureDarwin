/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * SOSPersist.h -- Utility routines for get/set in CFDictionary
 */

#ifndef _SOSPERSIST_H_
#define _SOSPERSIST_H_

__BEGIN_DECLS

#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>

#include <AssertMacros.h>


static inline bool SOSPeerGetPersistedBoolean(CFDictionaryRef persisted, CFStringRef key) {
    CFBooleanRef boolean = CFDictionaryGetValue(persisted, key);
    return boolean && CFBooleanGetValue(boolean);
}

static inline CFDataRef SOSPeerGetPersistedData(CFDictionaryRef persisted, CFStringRef key) {
    return asData(CFDictionaryGetValue(persisted, key), NULL);
}

static inline int64_t SOSPeerGetPersistedInt64(CFDictionaryRef persisted, CFStringRef key) {
    int64_t integer = 0;
    CFNumberRef number = CFDictionaryGetValue(persisted, key);
    if (number) {
        CFNumberGetValue(number, kCFNumberSInt64Type, &integer);
    }
    return integer;
}

static inline bool SOSPeerGetOptionalPersistedCFIndex(CFDictionaryRef persisted, CFStringRef key, CFIndex *value) {
    bool exists = false;
    CFNumberRef number = CFDictionaryGetValue(persisted, key);
    if (number) {
        exists = true;
        CFNumberGetValue(number, kCFNumberCFIndexType, value);
    }
    return exists;
}

static inline void SOSPersistBool(CFMutableDictionaryRef persist, CFStringRef key, bool value) {
    CFDictionarySetValue(persist, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

static inline void SOSPersistInt64(CFMutableDictionaryRef persist, CFStringRef key, int64_t value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
    CFDictionarySetValue(persist, key, number);
    CFReleaseSafe(number);
}

static inline void SOSPersistCFIndex(CFMutableDictionaryRef persist, CFStringRef key, CFIndex value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &value);
    CFDictionarySetValue(persist, key, number);
    CFReleaseSafe(number);
}

static inline void SOSPersistOptionalValue(CFMutableDictionaryRef persist, CFStringRef key, CFTypeRef value) {
    if (value)
        CFDictionarySetValue(persist, key, value);
}

__END_DECLS

#endif /* !_SOSPERSIST_H_ */
