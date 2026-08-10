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
 * SecFramework.c - generic non API class specific functions
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include "SecFramework.h"
#include <CoreFoundation/CFBundle.h>
#include <utilities/SecCFWrappers.h>

/* Security.framework's bundle id. */
#if TARGET_OS_IPHONE
CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.Security");
#else
CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.security");
#endif

CFGiblisGetSingleton(CFBundleRef, SecFrameworkGetBundle, bundle,  ^{
    *bundle = CFRetainSafe(CFBundleGetBundleWithIdentifier(kSecFrameworkBundleID));
})

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName) {
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle)
        return CFBundleCopyLocalizedString(bundle, key, key, tableName);

    return CFRetainSafe(key);
}
