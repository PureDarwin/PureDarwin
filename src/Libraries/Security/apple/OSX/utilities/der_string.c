/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


const uint8_t* der_decode_string(CFAllocatorRef allocator,
                                 CFStringRef* string, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_UTF8_STRING, &payload_size, der, der_end);

    if (NULL == payload || (ssize_t) (der_end - payload) < (ssize_t) payload_size){
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown string encoding"), NULL, error);
        return NULL;
    }

    *string = CFStringCreateWithBytes(allocator, payload, payload_size, kCFStringEncodingUTF8, false);

    if (NULL == *string) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("String allocation failed"), NULL, error);
        return NULL;
    }

    return payload + payload_size;
}


size_t der_sizeof_string(CFStringRef str, CFErrorRef *error)
{
    const CFIndex str_length    = CFStringGetLength(str);
    const CFIndex maximum       = CFStringGetMaximumSizeForEncoding(str_length, kCFStringEncodingUTF8);

    CFIndex encodedLen = 0;
    CFIndex converted = CFStringGetBytes(str, CFRangeMake(0, str_length), kCFStringEncodingUTF8, 0, false, NULL, maximum, &encodedLen);

    return ccder_sizeof(CCDER_UTF8_STRING, (converted == str_length) ? encodedLen : 0);
}


uint8_t* der_encode_string(CFStringRef string, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end)
{
    if (NULL == der_end) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }

    const CFIndex str_length = CFStringGetLength(string);

    ptrdiff_t der_space = der_end - der;
    CFIndex bytes_used = 0;
    uint8_t *buffer = der_end - der_space;
    CFIndex converted = CFStringGetBytes(string, CFRangeMake(0, str_length), kCFStringEncodingUTF8, 0, false, buffer, der_space, &bytes_used);
    if (converted != str_length){
        SecCFDERCreateError(kSecDERErrorUnsupportedCFObject, CFSTR("String extraction failed"), NULL, error);
        return NULL;
    }

    return SecCCDEREncodeHandleResult(ccder_encode_tl(CCDER_UTF8_STRING, bytes_used, der,
                                                      ccder_encode_body(bytes_used, buffer, der, der_end)),
                                      error);

}
