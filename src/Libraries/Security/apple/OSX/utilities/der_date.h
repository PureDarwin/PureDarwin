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


#ifndef _UTILITIES_DER_DATE_H_
#define _UTILITIES_DER_DATE_H_

#include <CoreFoundation/CoreFoundation.h>

const uint8_t* der_decode_generalizedtime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                               const uint8_t* der, const uint8_t *der_end);
const uint8_t* der_decode_universaltime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                             const uint8_t* der, const uint8_t *der_end);

size_t der_sizeof_generalizedtime(CFAbsoluteTime at, CFErrorRef *error);
uint8_t* der_encode_generalizedtime(CFAbsoluteTime at, CFErrorRef *error,
                                    const uint8_t *der, uint8_t *der_end);

size_t der_sizeof_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error);
uint8_t* der_encode_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error,
                                         const uint8_t *der, uint8_t *der_end);
uint8_t* der_encode_generalizedtime_body_repair(CFAbsoluteTime at, CFErrorRef *error, bool repair,
                                                const uint8_t *der, uint8_t *der_end);

#endif /* _UTILITIES_DER_DATE_H_ */
