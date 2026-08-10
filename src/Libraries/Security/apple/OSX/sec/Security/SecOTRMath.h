/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECOTRMATH_H_
#define _SECOTRMATH_H_

#include <CoreFoundation/CFBase.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>

#define kOTRAuthKeyBytes 16
#define kOTRAuthMACKeyBytes 32

#define kOTRMessageKeyBytes 16
#define kOTRMessageMacKeyBytes 20

#define kExponentiationBits 1536
#define kExponentiationUnits ccn_nof(kExponentiationBits)
#define kExponentiationBytes ((kExponentiationBits+7)/8)

#define kSHA256HMAC160Bits  160
#define kSHA256HMAC160Bytes (kSHA256HMAC160Bits/8)

typedef enum {
    kSSID = 0x00,
    kCs = 0x01,
    kM1 = 0x02,
    kM2 = 0x03,
    kM1Prime = 0x04,
    kM2Prime = 0x05
} OTRKeyType;


void DeriveOTR256BitsFromS(OTRKeyType whichKey, size_t sSize, const cc_unit* s, size_t keySize, uint8_t* key);
void DeriveOTR128BitPairFromS(OTRKeyType whichHalf, size_t sSize, const cc_unit* s,
                              size_t firstKeySize, uint8_t* firstKey,
                              size_t secondKeySize, uint8_t* secondKey);
void DeriveOTR64BitsFromS(OTRKeyType whichKey, size_t sSize, const cc_unit* s,
                          size_t firstKeySize, uint8_t* firstKey);


void AES_CTR_HighHalf_Transform(size_t keySize, const uint8_t* key,
                                uint64_t highHalf,
                                size_t howMuch, const uint8_t* from,
                                uint8_t* to);

void AES_CTR_IV0_Transform(size_t keySize, const uint8_t* key,
                           size_t howMuch, const uint8_t* from,
                           uint8_t* to);

#endif
