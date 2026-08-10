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


#include "utilities/der_plist_internal.h"
#include "utilities/SecCFError.h"
#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include <CoreFoundation/CoreFoundation.h>

CFStringRef sSecDERErrorDomain = CFSTR("com.apple.security.cfder.error");

uint8_t * SecCCDEREncodeHandleResult(uint8_t *der, CFErrorRef *newError)
{
    if (!der) {
        SecCFDERCreateError(kSecDERErrorCCDEREncode, CFSTR("ccder failed to encode"), NULL, newError);
    }
    return der;
}
