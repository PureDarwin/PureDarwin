/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>


const uint8_t* der_decode_null(CFAllocatorRef allocator,
                                  CFNullRef* nul, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }
	
    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_NULL, &payload_size, der, der_end);
	
	if (NULL == payload || payload_size != 0) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown null encoding"), NULL, error);
        return NULL;
    }
	
    *nul = kCFNull;
	
    return payload + payload_size;
}


size_t der_sizeof_null(CFNullRef data __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_NULL, 0);
}


uint8_t* der_encode_null(CFNullRef boolean __unused, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end)
{
	return SecCCDEREncodeHandleResult(ccder_encode_tl(CCDER_NULL, 0, der, der_end),
                                      error);
}
