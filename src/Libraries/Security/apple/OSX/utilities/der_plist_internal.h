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


#ifndef _DER_PLIST_INTERNAL_H_
#define _DER_PLIST_INTERNAL_H_

#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFError.h>

// Always returns false, to satisfy static analysis
#define SecCFDERCreateError(errorCode, descriptionString, previousError, newError) \
    SecCFCreateErrorWithFormat(errorCode, sSecDERErrorDomain, previousError, newError, NULL, descriptionString)

uint8_t * SecCCDEREncodeHandleResult(uint8_t *der, CFErrorRef *newError);


// CFArray <-> DER
size_t der_sizeof_array(CFArrayRef array, CFErrorRef *error);

uint8_t* der_encode_array(CFArrayRef array, CFErrorRef *error,
                          const uint8_t *der, uint8_t *der_end);
uint8_t* der_encode_array_repair(CFArrayRef array, CFErrorRef *error,
                                 bool repair,
                                 const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_array(CFAllocatorRef allocator,
                                CFArrayRef* array, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end);

// CFNull <-> DER
size_t der_sizeof_null(CFNullRef	nul, CFErrorRef *error);

uint8_t* der_encode_null(CFNullRef	nul, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_null(CFAllocatorRef allocator,
                                  CFNullRef	*nul, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end);


// CFBoolean <-> DER
size_t der_sizeof_boolean(CFBooleanRef boolean, CFErrorRef *error);

uint8_t* der_encode_boolean(CFBooleanRef boolean, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_boolean(CFAllocatorRef allocator,
                                  CFBooleanRef* boolean, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end);

// CFData <-> DER
size_t der_sizeof_data(CFDataRef data, CFErrorRef *error);

uint8_t* der_encode_data(CFDataRef data, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_data(CFAllocatorRef allocator,
                               CFDataRef* data, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end);

// CFDate <-> DER
size_t der_sizeof_date(CFDateRef date, CFErrorRef *error);

uint8_t* der_encode_date(CFDateRef date, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end);

uint8_t* der_encode_date_repair(CFDateRef date, CFErrorRef *error,
                                bool repair, const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_date(CFAllocatorRef allocator,
                               CFDateRef* date, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end);


// CFDictionary <-> DER
size_t der_sizeof_dictionary(CFDictionaryRef dictionary, CFErrorRef *error);

uint8_t* der_encode_dictionary(CFDictionaryRef dictionary, CFErrorRef *error,
                               const uint8_t *der, uint8_t *der_end);
uint8_t* der_encode_dictionary_repair(CFDictionaryRef dictionary, CFErrorRef *error,
                                      bool repair, const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_dictionary(CFAllocatorRef allocator,
                                     CFDictionaryRef* dictionary, CFErrorRef *error,
                                     const uint8_t* der, const uint8_t *der_end);

// CFNumber <-> DER
// Currently only supports signed 64 bit values. No floating point.
size_t der_sizeof_number(CFNumberRef number, CFErrorRef *error);

uint8_t* der_encode_number(CFNumberRef number, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_number(CFAllocatorRef allocator,
                                 CFNumberRef* number, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end);

// CFString <-> DER
size_t der_sizeof_string(CFStringRef string, CFErrorRef *error);

uint8_t* der_encode_string(CFStringRef string, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_string(CFAllocatorRef allocator,
                                 CFStringRef* string, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end);

// CFSet <-> DER
size_t der_sizeof_set(CFSetRef dict, CFErrorRef *error);

uint8_t* der_encode_set(CFSetRef set, CFErrorRef *error,
                        const uint8_t *der, uint8_t *der_end);
uint8_t* der_encode_set_repair(CFSetRef set, CFErrorRef *error,
                               bool repair, const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_set(CFAllocatorRef allocator,
                              CFSetRef* set, CFErrorRef *error,
                              const uint8_t* der, const uint8_t *der_end);

#include <corecrypto/ccder.h>
enum {
    CCDER_CONSTRUCTED_CFSET = CCDER_PRIVATE | CCDER_SET,
};

#endif
