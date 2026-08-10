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


#include "SecOTRMath.h"

#include "SecOTRPacketData.h"

#include <utilities/SecCFWrappers.h>
#include <AssertMacros.h>

#include <pthread.h>

#include <Security/SecRandom.h>

#include <corecrypto/ccsha2.h>
#include <corecrypto/cczp.h>
#include <corecrypto/ccdh_gp.h>

#include <limits.h>

//
// Random Number Generation
//


static const uint8_t kIVZero[16] = { };

static void AES_CTR_Transform(size_t keySize, const uint8_t* key,
                       const uint8_t iv[16],
                       size_t howMuch, const uint8_t* from, uint8_t* to)
{
    const struct ccmode_ctr* ctr_encrypt = ccaes_ctr_crypt_mode();
    ccctr_ctx_decl(ctr_encrypt->size, ctr_ctx);
    ctr_encrypt->init(ctr_encrypt, ctr_ctx, keySize, key, iv);
    
    ctr_encrypt->ctr(ctr_ctx, howMuch, from, to);
}

void AES_CTR_HighHalf_Transform(size_t keySize, const uint8_t* key,
                                uint64_t highHalf,
                                size_t howMuch, const uint8_t* from, uint8_t* to)
{
    uint8_t iv[16] = { highHalf >> 56, highHalf >> 48, highHalf >> 40, highHalf >> 32,
                       highHalf >> 24, highHalf >> 16, highHalf >> 8 , highHalf >> 0,
                                    0,              0,             0,              0,
                                    0,              0,             0,              0 };
    AES_CTR_Transform(keySize, key, iv, howMuch, from, to);
}

void AES_CTR_IV0_Transform(size_t keySize, const uint8_t* key,
                           size_t howMuch, const uint8_t* from, uint8_t* to)
{
    AES_CTR_Transform(keySize, key, kIVZero, howMuch, from, to);
}


//
// Key Derivation
//

static void HashMPIWithPrefix(uint8_t byte, cc_size sN, const cc_unit* s, uint8_t* buffer)
{
    CFMutableDataRef dataToHash = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataAppendBytes(dataToHash, &byte, 1);
    
    AppendMPI(dataToHash, sN, s);
    
    uint8_t *bytesToHash = CFDataGetMutableBytePtr(dataToHash);
    CFIndex  amountToHash = CFDataGetLength(dataToHash);
    
    /* 64 bits cast: amountToHash is the size of an identity +1 , which is currently hardcoded and never more than 2^32 bytes */
    assert((unsigned long)amountToHash<UINT32_MAX); /* Debug check, Correct as long as CFIndex is a signed long and CC_LONG is a uint32_t */

    (void) CC_SHA256(bytesToHash, (CC_LONG)amountToHash, buffer);
    
    bzero(bytesToHash, (size_t)amountToHash);
    CFReleaseNull(dataToHash);
}

void DeriveOTR256BitsFromS(OTRKeyType whichKey, cc_size sN, const cc_unit* s, size_t keySize, uint8_t* key)
{
    HashMPIWithPrefix(whichKey, sN, s, key);
}

void DeriveOTR128BitPairFromS(OTRKeyType whichKey, size_t sSize, const cc_unit* s,
                              size_t firstKeySize, uint8_t* firstKey,
                              size_t secondKeySize, uint8_t* secondKey)
{
    uint8_t hashBuffer[CCSHA256_OUTPUT_SIZE];
    
    HashMPIWithPrefix(whichKey, sSize, s, hashBuffer);
    
    if (firstKey) {
        firstKeySize = firstKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : firstKeySize;
        memcpy(firstKey, hashBuffer, firstKeySize);
    }
    if (secondKey) {
        secondKeySize = secondKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : secondKeySize;
        memcpy(secondKey, hashBuffer, secondKeySize);
    }
    
    bzero(hashBuffer, CCSHA256_OUTPUT_SIZE);

}

void DeriveOTR64BitsFromS(OTRKeyType whichKey, size_t sn, const cc_unit* s,
                          size_t topKeySize, uint8_t* topKey)
{
    uint8_t hashBuffer[CCSHA256_OUTPUT_SIZE];
    
    HashMPIWithPrefix(whichKey, sn, s, hashBuffer);
    
    topKeySize = topKeySize > CCSHA256_OUTPUT_SIZE/2 ? CCSHA256_OUTPUT_SIZE/2 : topKeySize;
    memcpy(topKey, hashBuffer, topKeySize);
    
    bzero(hashBuffer, CCSHA256_OUTPUT_SIZE);
}

