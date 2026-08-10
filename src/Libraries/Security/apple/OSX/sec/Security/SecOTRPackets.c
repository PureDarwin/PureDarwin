/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include "SecOTRSessionPriv.h"
#include "SecOTRPackets.h"

#include "SecOTR.h"
#include "SecOTRIdentityPriv.h"

//*****************************************#include "SecCFWrappers.h"
#include "SecOTRPacketData.h"
#include "SecOTRDHKey.h"

#include <utilities/SecCFWrappers.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccdigest.h>

#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccmode_factory.h>
#include <corecrypto/cchmac.h>
#include <corecrypto/ccsha2.h>

//
// Crypto functions
//

static inline void AppendSHA256HMAC(CFMutableDataRef appendTo,
                                    size_t keybytes,
                                    const uint8_t* key,
                                    size_t howMuch,
                                    const uint8_t* from)
{
    uint8_t *to = CFDataIncreaseLengthAndGetMutableBytes(appendTo, CCSHA256_OUTPUT_SIZE);
    
    cchmac(ccsha256_di(), keybytes, key, howMuch, from, to);
}

// First 160 bits of the HMAC
static inline void AppendSHA256HMAC_160(CFMutableDataRef appendTo,
                                    size_t keySize,
                                    const uint8_t* key,
                                    size_t howMuch,
                                    const uint8_t* from)
{
    AppendSHA256HMAC(appendTo, keySize, key, howMuch, from);
    const CFIndex bytesToRemove = CCSHA256_OUTPUT_SIZE - kSHA256HMAC160Bytes;
    const CFRange rangeToDelete = CFRangeMake(CFDataGetLength(appendTo) - bytesToRemove, bytesToRemove);
    
    CFDataDeleteBytes(appendTo, rangeToDelete);
}

static inline void DeriveAndAppendSHA256HMAC(CFMutableDataRef appendTo,
                                             cc_size sN,
                                             const cc_unit* s,
                                             OTRKeyType whichKey,
                                             size_t howMuch,
                                             const uint8_t* from)
{
    size_t localKeySize = CCSHA256_OUTPUT_SIZE;
    uint8_t localKey[localKeySize];
    
    DeriveOTR256BitsFromS(whichKey, sN, s, localKeySize, localKey);
    
    AppendSHA256HMAC(appendTo, localKeySize, localKey, howMuch, from);
    
    bzero(localKey, localKeySize);
}

static inline void DeriveAndAppendSHA256HMAC_160(CFMutableDataRef appendTo,
                                                 cc_size sN,
                                                 const cc_unit* s,
                                                 OTRKeyType whichKey,
                                                 size_t howMuch,
                                                 const uint8_t* from)
{
    size_t localKeySize = CCSHA256_OUTPUT_SIZE;
    uint8_t localKey[localKeySize];
    
    DeriveOTR256BitsFromS(whichKey, sN, s, localKeySize, localKey);
    
    AppendSHA256HMAC_160(appendTo, localKeySize, localKey, howMuch, from);
    
    bzero(localKey, sizeof(localKey));
}

//
// Message creators
//

void SecOTRAppendDHMessage(SecOTRSessionRef session,
                           CFMutableDataRef appendTo)
{
    //
    // Message Type: kDHMessage (0x02)
    // AES_CTR(r, 0) of G^X MPI
    // SHA256(gxmpi)
    //
    
    if(!session) return;
    CFMutableDataRef gxmpi = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if(!gxmpi) return;

    AppendHeader(appendTo, kDHMessage);

    SecFDHKAppendPublicSerialization(session->_myKey, gxmpi);

    size_t gxmpiSize = (size_t)CFDataGetLength(gxmpi);
    if(gxmpiSize == 0) {
        CFReleaseNull(gxmpi);
        return;
    }
    const uint8_t* gxmpiLocation = CFDataGetBytePtr(gxmpi);

    /* 64 bits cast: gxmpiSize is the size of the EC public key, which is hardcoded and never more than 2^32 bytes. */
    assert(gxmpiSize<UINT32_MAX); /* debug check only */
    AppendLong(appendTo, (uint32_t)gxmpiSize);
    assert(gxmpiSize<INT32_MAX);
    uint8_t* encGxmpiLocation = CFDataIncreaseLengthAndGetMutableBytes(appendTo, (CFIndex)gxmpiSize);
    AES_CTR_IV0_Transform(sizeof(session->_r), session->_r, gxmpiSize, gxmpiLocation, encGxmpiLocation);
    
    AppendLong(appendTo, CCSHA256_OUTPUT_SIZE);
    uint8_t* hashLocation = CFDataIncreaseLengthAndGetMutableBytes(appendTo, CCSHA256_OUTPUT_SIZE);

    ccdigest(ccsha256_di(), gxmpiSize, gxmpiLocation, hashLocation);

    CFReleaseNull(gxmpi);
}

void SecOTRAppendDHKeyMessage(SecOTRSessionRef session,
                              CFMutableDataRef appendTo)
{
    //
    // Message Type: kDHKeyMessage (0x0A)
    // G^X Data MPI
    //
    
    AppendHeader(appendTo, kDHKeyMessage);
    SecFDHKAppendPublicSerialization(session->_myKey, appendTo);
}

static uint8_t* AppendEncryptedSignature(SecOTRSessionRef session,
                                         const cc_unit* s,
                                         bool usePrime,
                                         CFMutableDataRef appendTo)
{
    CFMutableDataRef signature = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFMutableDataRef mbData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFMutableDataRef mb = CFDataCreateMutable(kCFAllocatorDefault, 0);

    SecFDHKAppendPublicSerialization(session->_myKey, mbData);
    SecPDHKAppendSerialization(session->_theirKey, mbData);
    
    CFIndex publicKeyOffset = CFDataGetLength(mbData);

    SecOTRPublicIdentityRef myPublic = SecOTRPublicIdentityCopyFromPrivate(kCFAllocatorDefault, session->_me, NULL);
    AppendPublicKey(mbData, myPublic);
    CFReleaseNull(myPublic);

    AppendLong(mbData, session->_keyID);
    
    DeriveAndAppendSHA256HMAC(mb,
                              kExponentiationUnits, s,
                              usePrime ? kM1Prime : kM1,
                              (size_t)CFDataGetLength(mbData), CFDataGetBytePtr(mbData));
    
    CFDataDeleteBytes(mbData, CFRangeMake(0, publicKeyOffset));

    CFMutableDataRef xb = mbData; mbData = NULL;
    SecOTRFIAppendSignature(session->_me, mb, signature, NULL);
    CFReleaseNull(mb);

    AppendCFDataAsDATA(xb, signature);
    CFReleaseNull(signature);

    CFIndex dataLength = CFDataGetLength(xb);

    CFIndex signatureStartIndex = CFDataGetLength(appendTo);
    /* 64 bits cast: We are appending the signature we just generated, which is never bigger than 2^32 bytes. */
    assert(((unsigned long)dataLength)<=UINT32_MAX); /* debug check, correct as long as CFIndex is a signed long */
    AppendLong(appendTo, (uint32_t)dataLength);
    uint8_t *destination = CFDataIncreaseLengthAndGetMutableBytes(appendTo, dataLength);

    uint8_t c[kOTRAuthKeyBytes];
    DeriveOTR128BitPairFromS(kCs, kExponentiationUnits, s,
                             sizeof(c), usePrime ? NULL : c,
                             sizeof(c), usePrime ? c : NULL);

    AES_CTR_IV0_Transform(sizeof(c), c,
                          (size_t)dataLength, CFDataGetBytePtr(xb),
                          destination);
    bzero(c, sizeof(c));
    CFReleaseNull(xb);
    
    return CFDataGetMutableBytePtr(appendTo) + signatureStartIndex;
}


static void AppendMACedEncryptedSignature(SecOTRSessionRef session,
                                          bool usePrime,
                                          CFMutableDataRef appendTo)
{
    
    cc_unit s[kExponentiationUnits];
    
    SecPDHKeyGenerateS(session->_myKey, session->_theirKey, s);

    CFIndex signatureStartOffset = CFDataGetLength(appendTo);
    const uint8_t *signatureStart = AppendEncryptedSignature(session, s, usePrime, appendTo);
    size_t signatureSize = (size_t)CFDataGetLength(appendTo) - (size_t)signatureStartOffset;
    
    
    DeriveAndAppendSHA256HMAC_160(appendTo,
                                  kExponentiationUnits, s,
                                  usePrime ? kM2Prime : kM2,
                                  signatureSize, signatureStart);
    bzero(s, sizeof(s));
}


void SecOTRAppendRevealSignatureMessage(SecOTRSessionRef session,
                                        CFMutableDataRef appendTo)
{
    //
    // Message Type: kRevealSignatureMessage (0x11)
    // G^X Data MPI
    //
    
    AppendHeader(appendTo, kRevealSignatureMessage);
    
    AppendLong(appendTo, kOTRAuthKeyBytes);
    uint8_t* keyPosition = CFDataIncreaseLengthAndGetMutableBytes(appendTo, kOTRAuthKeyBytes);
    memcpy(keyPosition, session->_r, kOTRAuthKeyBytes);
    
    AppendMACedEncryptedSignature(session, false, appendTo);
}

void SecOTRAppendSignatureMessage(SecOTRSessionRef session,
                                        CFMutableDataRef appendTo)
{
    //
    // Message Type: kSignatureMessage (0x12)
    // G^X Data MPI
    //
    
    AppendHeader(appendTo, kSignatureMessage);
    AppendMACedEncryptedSignature(session, true, appendTo);
}

