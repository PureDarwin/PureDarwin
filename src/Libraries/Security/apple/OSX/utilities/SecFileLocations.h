/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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

//
//  SecFileLocations.h
//  utilities
//


#ifndef _SECFILELOCATIONS_H_
#define _SECFILELOCATIONS_H_

#include <CoreFoundation/CFURL.h>

__BEGIN_DECLS

CFURLRef SecCopyURLForFileInBaseDirectory(bool system, CFStringRef directoryPath, CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInKeychainDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInSystemKeychainDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInUserCacheDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInPreferencesDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInManagedPreferencesDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;
CFURLRef SecCopyURLForFileInProtectedDirectory(CFStringRef fileName) CF_RETURNS_RETAINED;

void WithPathInDirectory(CFURLRef fileURL, void(^operation)(const char *utf8String));
void WithPathInKeychainDirectory(CFStringRef fileName, void(^operation)(const char *utf8String));
void WithPathInUserCacheDirectory(CFStringRef fileName, void(^operation)(const char *utf8String));
void WithPathInProtectedDirectory(CFStringRef fileName, void(^operation)(const char *utf8String));

void SetCustomHomePath(const char* path);
void SecSetCustomHomeURLString(CFStringRef path);
void SecSetCustomHomeURL(CFURLRef url);

CFURLRef SecCopyHomeURL(void) CF_RETURNS_RETAINED;

__END_DECLS

#endif
