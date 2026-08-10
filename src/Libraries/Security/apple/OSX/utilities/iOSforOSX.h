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


#ifndef utilities_iOSforOSX_h
#define utilities_iOSforOSX_h

#include <TargetConditionals.h>
#if TARGET_OS_OSX

extern CFURLRef SecCopyKeychainDirectoryFile(CFStringRef file);

CFURLRef PortableCFCopyHomeDirectoryURL(void);

#ifndef _SECURITY_SECRANDOM_H_
extern const void *kSecRandomDefault;
#endif

#ifndef _SECURITY_SECBASE_H_
typedef struct __SecKey *SecKeyRef;
#endif
OSStatus SecKeyCopyPersistentRef(SecKeyRef item, CFDataRef *newpersistentRef);
OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef *key);


#endif

CFURLRef PortableCFCopyHomeDirectoryURL(void) asm("_CFCopyHomeDirectoryURL");

#endif
