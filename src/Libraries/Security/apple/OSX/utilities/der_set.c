/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
#include "der_set.h"

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"
#include "utilities/SecCFWrappers.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>

const uint8_t* der_decode_set(CFAllocatorRef allocator,
                                     CFSetRef* set, CFErrorRef *error,
                                     const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorNullInput, CFSTR("null input"), NULL, error);
        return NULL;
    }
    
    const uint8_t *payload_end = 0;
    const uint8_t *payload = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_CFSET, &payload_end, der, der_end);
    
    if (NULL == payload) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding, expected CCDER_CONSTRUCTED_CFSET"), NULL, error);
        return NULL;
    }
    
    CFMutableSetRef theSet = (set && *set) ? CFSetCreateMutableCopy(allocator, 0, *set)
                                           : CFSetCreateMutable(allocator, 0, &kCFTypeSetCallBacks);
    
    if (NULL == theSet) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Failed to create set"), NULL, error);
        payload = NULL;
        goto exit;
    }
    
    while (payload != NULL && payload < payload_end) {
        CFTypeRef value = NULL;
        
        payload = der_decode_plist(allocator, &value, error, payload, payload_end);
        
        if (payload) {
            CFSetAddValue(theSet, value);
        }
        CFReleaseNull(value);
    }
    
    
exit:
    if (payload == payload_end && set) {
        CFTransferRetained(*set, theSet);
    }
    
    CFReleaseNull(theSet);
    
    return payload;
}

struct size_context {
    bool   success;
    size_t size;
    CFErrorRef *error;
};

static void add_value_size(const void *value_void, void *context_void)
{
    CFTypeRef value = (CFTypeRef) value_void;
    struct size_context *context = (struct size_context*) context_void;
    
    if (!context->success)
        return;
    
    size_t kv_size = der_sizeof_plist(value, context->error);
    if (kv_size == 0) {
        context->success = false;
        return;
    }
    
    context->size += kv_size;
}

size_t der_sizeof_set(CFSetRef dict, CFErrorRef *error)
{
    struct size_context context = { .success = true, .size = 0, .error = error };
    
    CFSetApplyFunction(dict, add_value_size, &context);
    
    if (!context.success)
        return 0;
    
    return ccder_sizeof(CCDER_CONSTRUCTED_CFSET, context.size);
}

struct encode_context {
    bool         success;
    bool         repair_contents;
    CFErrorRef * error;
    CFMutableArrayRef list;
    CFAllocatorRef allocator;
};

static void add_sequence_to_array(const void *value_void, void *context_void)
{
    struct encode_context *context = (struct encode_context *) context_void;
    if (context->success) {
        size_t der_size = der_sizeof_plist(value_void, context->error);
        if (der_size == 0) {
            context-> success = false;
        } else {
            CFMutableDataRef value = CFDataCreateMutable(context->allocator, der_size);
            CFDataSetLength(value, der_size);
            
            uint8_t* const encode_begin = CFDataGetMutableBytePtr(value);
            uint8_t *encode_end = encode_begin + der_size;
            
            encode_end = der_encode_plist_repair(value_void, context->error, context->repair_contents, encode_begin, encode_end);
            
            if (encode_end != NULL) {
                CFDataDeleteBytes(value, CFRangeMake(0, (encode_end - encode_begin)));
                CFArrayAppendValue(context->list, value);
            } else {
                context-> success = false;
            }
            CFReleaseNull(value);
        }
    }
}

static CFComparisonResult cfdata_compare_der_contents(const void *val1, const void *val2, void *context __unused)
{
    return CFDataCompareDERData((CFDataRef) val1, (CFDataRef) val2);
}


uint8_t* der_encode_set(CFSetRef set, CFErrorRef *error,
                               const uint8_t *der, uint8_t *der_end)
{
    return der_encode_set_repair(set, error, false, der, der_end);
}

uint8_t* der_encode_set_repair(CFSetRef set, CFErrorRef *error,
                               bool repair, const uint8_t *der, uint8_t *der_end)
{
    CFMutableArrayRef elements = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    struct encode_context context = { .success = true, .error = error, .list = elements, .repair_contents = repair };
    CFSetApplyFunction(set, add_sequence_to_array, &context);
    
    if (!context.success) {
        CFReleaseNull(elements);
        return NULL;
    }
    
    CFRange allOfThem = CFRangeMake(0, CFArrayGetCount(elements));
    
    CFArraySortValues(elements, allOfThem, cfdata_compare_der_contents, NULL);

    uint8_t* original_der_end = der_end;
    
    for(CFIndex position = CFArrayGetCount(elements); position > 0;) {
        --position;
        CFDataRef data = CFArrayGetValueAtIndex(elements, position);
        der_end = ccder_encode_body(CFDataGetLength(data), CFDataGetBytePtr(data), der, der_end);
    }
    
    CFReleaseNull(elements);
    
    return SecCCDEREncodeHandleResult(ccder_encode_constructed_tl(CCDER_CONSTRUCTED_CFSET, original_der_end, der, der_end),
                                      error);
    
}
