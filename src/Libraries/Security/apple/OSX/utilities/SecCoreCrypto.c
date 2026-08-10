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


#include "SecCoreCrypto.h"
#include <corecrypto/ccder.h>


#ifndef CCDER_BOOL_SUPPORT

//
// bool encoding/decoding
//


const uint8_t* ccder_decode_bool(bool* boolean, const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;
    
    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_BOOLEAN, &payload_size, der, der_end);
    
    if (NULL == payload || (der_end - payload) < 1 || payload_size != 1) {
        return NULL;
    }
    
    if (boolean)
        *boolean = (*payload != 0);
    
    return payload + payload_size;
}


size_t ccder_sizeof_bool(bool value __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_BOOLEAN, 1);
}


uint8_t* ccder_encode_bool(bool value, const uint8_t *der, uint8_t *der_end)
{
    uint8_t value_byte = value;
    
    return ccder_encode_tl(CCDER_BOOLEAN, 1, der,
                           ccder_encode_body(1, &value_byte, der, der_end));
}
#endif
