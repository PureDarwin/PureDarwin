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


const uint8_t* der_decode_number(CFAllocatorRef allocator,
                                 CFNumberRef* number, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_INTEGER, &payload_size, der, der_end);

    if (NULL == payload || (ssize_t) (der_end - payload) < (ssize_t) payload_size) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown number encoding"), NULL, error);
        return NULL;
    }
    if (payload_size > sizeof(long long)) {
        SecCFDERCreateError(kSecDERErrorUnsupportedNumberType, CFSTR("Number too large"), NULL, error);
        return NULL;
    }

    long long value = 0;
    
    if (payload_size > 0) {
        if ((*payload & 0x80) == 0x80)
            value = -1; // Negative integers fill with 1s so we end up negative.
        
        const uint8_t* const payload_end = payload + payload_size;
        
        for (const uint8_t *payload_byte = payload;
             payload_byte < payload_end;
             ++payload_byte) {
            value <<= 8;
            value |= *payload_byte;
        }
    }

    *number = CFNumberCreate(allocator, kCFNumberLongLongType, &value);
    
    if (*number == NULL) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Number allocation failed"), NULL, error);
        return NULL;
    }
    
    return payload + payload_size;
}


static inline uint8_t byte_of(size_t byteNumber, long long value)
{
    return value >> (8 * (byteNumber - 1));
}

static inline size_t bytes_when_encoded(long long value)
{
    size_t bytes_encoded = sizeof(long long);
    
    uint8_t first_byte = byte_of(bytes_encoded, value);
    
    // Skip initial 0xFFs or 0x00
    if (first_byte == 0xFF || first_byte == 0x00) {
        do {
            --bytes_encoded;
        } while (bytes_encoded > 1 && (byte_of(bytes_encoded, value) == first_byte));
        
        if ((first_byte & 0x80) != (byte_of(bytes_encoded, value) & 0x80))
            bytes_encoded += 1;
    }
    
    return bytes_encoded;
}

size_t der_sizeof_number(CFNumberRef data, CFErrorRef *error)
{
    long long value;
    if (!CFNumberGetValue(data, kCFNumberLongLongType, &value)) {
        SecCFDERCreateError(kSecDERErrorUnsupportedNumberType, CFSTR("Unable to get number from data"), NULL, error);
        return 0;
    }
    
    return ccder_sizeof(CCDER_INTEGER, bytes_when_encoded(value));
}

uint8_t* der_encode_number(CFNumberRef number, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end)
{
    long long value;
    if (!CFNumberGetValue(number, kCFNumberLongLongType, &value)) {
        SecCFDERCreateError(kSecDERErrorUnsupportedNumberType, CFSTR("Unable to get number from data"), NULL, error);
        return NULL;
    }
    
    size_t first_byte_to_include = bytes_when_encoded(value);
    
    if (!der_end || (ssize_t) (der_end - der) < (ssize_t) first_byte_to_include) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Unknown size"), NULL, error);
        return NULL;
    }

    // Put the bytes we should include on the end.
    for(size_t bytes_included = 0; bytes_included < first_byte_to_include; ++bytes_included)
    {
        --der_end;
        *der_end = value & 0xFF;
        value >>= 8;
    }

    return SecCCDEREncodeHandleResult(ccder_encode_tl(CCDER_INTEGER, first_byte_to_include, der, der_end),
                                      error);

}
