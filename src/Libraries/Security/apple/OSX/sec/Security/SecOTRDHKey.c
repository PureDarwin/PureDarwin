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

#include "SecOTRDHKey.h"
#include <utilities/SecCFWrappers.h>

#include "SecOTRMath.h"
#include "SecOTRPacketData.h"

#include <corecrypto/ccn.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccec_priv.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccrng.h>

#define kECKeySize 256

static OSStatus GenerateHashForKey(ccec_pub_ctx_t public_key, void *output)
{
    size_t size = ccec_export_pub_size(public_key);

    uint8_t *pub_key_bytes_buffer = malloc(size);
    if (pub_key_bytes_buffer == NULL) {
        return errSecMemoryError;
    }

    ccec_export_pub(public_key, pub_key_bytes_buffer);

    ccdigest(ccsha1_di(), size, pub_key_bytes_buffer, output);

    free(pub_key_bytes_buffer);

    return errSecSuccess;
}


struct _SecOTRFullDHKey {
    CFRuntimeBase _base;

    ccec_full_ctx_decl(ccn_sizeof(kECKeySize), _key);
    uint8_t keyHash[CCSHA1_OUTPUT_SIZE];

};

CFGiblisWithCompareFor(SecOTRFullDHKey);

static size_t AppendECPublicKeyAsDATA(CFMutableDataRef data, ccec_pub_ctx_t public_key)
{
    size_t size = ccec_export_pub_size(public_key);

    AppendLong(data, (uint32_t)size); /* cast: no overflow, pub size always fit in 32 bits */
    ccec_export_pub(public_key, CFDataIncreaseLengthAndGetMutableBytes(data, (CFIndex)size));

    return size;
}

static size_t AppendECCompactPublicKey(CFMutableDataRef data, ccec_pub_ctx_t public_key)
{
    size_t size = ccec_compact_export_size(false, public_key);

    ccec_compact_export(false, CFDataIncreaseLengthAndGetMutableBytes(data, (CFIndex)size), (ccec_full_ctx_t)public_key);

    return size;
}

static CFStringRef CCNCopyAsHex(size_t n, cc_unit *value){
    size_t bytes = ccn_write_uint_size(n, value);
    uint8_t byte_array [bytes];
    ccn_write_uint(n, value, bytes, byte_array);

    __block CFStringRef description = NULL;

    BufferPerformWithHexString(byte_array, sizeof(byte_array), ^(CFStringRef dataString) {
        description = CFRetainSafe(dataString);
    });
    return description;
}
static void withXandY(ccec_pub_ctx_t pubKey, void (^action)(CFStringRef x, CFStringRef y)){
    CFStringRef xString = NULL;
    CFStringRef yString = NULL;
    xString = CCNCopyAsHex(ccec_ctx_n(pubKey), ccec_ctx_x(pubKey));
    yString = CCNCopyAsHex(ccec_ctx_n(pubKey), ccec_ctx_y(pubKey));

    action(xString, yString);
    CFReleaseNull(xString);
    CFReleaseNull(yString);
}
static CFStringRef SecOTRFullDHKeyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions)
{
    SecOTRFullDHKeyRef fullDHKey = (SecOTRFullDHKeyRef)cf;
    __block CFStringRef description = NULL;

    withXandY(ccec_ctx_pub(fullDHKey->_key), ^(CFStringRef x, CFStringRef y) {
        BufferPerformWithHexString(fullDHKey->keyHash, sizeof(fullDHKey->keyHash), ^(CFStringRef dataString) {
            description = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRFullDHKeyRef@%p: x: %@ y: %@ [%@]>"), fullDHKey, x, y, dataString);
        });
    });

    return description;
}

static Boolean SecOTRFullDHKeyCompare(CFTypeRef leftCF, CFTypeRef rightCF)
{
    SecOTRFullDHKeyRef left = (SecOTRFullDHKeyRef)leftCF;
    SecOTRFullDHKeyRef right = (SecOTRFullDHKeyRef)rightCF;

    return 0 == memcmp(left->keyHash, right->keyHash, sizeof(left->keyHash));
}

static void SecOTRFullDHKeyDestroy(CFTypeRef cf)
{
    SecOTRFullDHKeyRef fullKey = (SecOTRFullDHKeyRef)cf;
    
    bzero(fullKey->_key, sizeof(fullKey->_key));
}

static inline OSStatus SecOTRFDHKUpdateHash(SecOTRFullDHKeyRef fullKey)
{
    return GenerateHashForKey(ccec_ctx_pub(fullKey->_key), fullKey->keyHash);
}

SecOTRFullDHKeyRef SecOTRFullDHKCreate(CFAllocatorRef allocator)
{
    SecOTRFullDHKeyRef newFDHK = CFTypeAllocate(SecOTRFullDHKey, struct _SecOTRFullDHKey, allocator);

    SecFDHKNewKey(newFDHK);
    
    return newFDHK;
}

SecOTRFullDHKeyRef SecOTRFullDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size)
{
    SecOTRFullDHKeyRef newFDHK = CFTypeAllocate(SecOTRFullDHKey, struct _SecOTRFullDHKey, allocator);

    ccec_ctx_init(ccec_cp_256(), newFDHK->_key);

    uint32_t publicKeySize;
    require_noerr(ReadLong(bytes, size, &publicKeySize), fail);

    require(publicKeySize <= *size, fail);
    require_noerr(ccec_import_pub(ccec_cp_256(), publicKeySize, *bytes, ccec_ctx_pub(newFDHK->_key)), fail);
    
    *size -= publicKeySize;
    *bytes += publicKeySize;

    require_noerr(ReadMPI(bytes, size, ccec_ctx_n(newFDHK->_key), ccec_ctx_k(newFDHK->_key)), fail);

    require_noerr(SecOTRFDHKUpdateHash(newFDHK), fail);

    return newFDHK;

fail:
    CFReleaseNull(newFDHK);
    return NULL;
}

OSStatus SecFDHKNewKey(SecOTRFullDHKeyRef fullKey)
{
    struct ccrng_state *rng=ccrng(NULL);

    // We need compact keys, maybe we should be using
    // ccecdh_generate_key or ccechd_generate_compact_key, but for now ecdh are fine for compact use IFF we don't
    // use the non-compact pub part.

    // ccec_cmp() assumes public keys are 65 bytes or shorter.
    // If we ever generate different DHKeys, we will need to make a change there too.

    ccec_compact_generate_key(ccec_cp_256(), rng, fullKey->_key);

    return SecOTRFDHKUpdateHash(fullKey);
}

void SecFDHKAppendSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo)
{
    AppendECPublicKeyAsDATA(appendTo, ccec_ctx_pub(fullKey->_key));
    AppendMPI(appendTo, ccec_ctx_n(fullKey->_key), ccec_ctx_k(fullKey->_key));
}

void SecFDHKAppendPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo)
{
    if(ccec_ctx_bitlen(fullKey->_key) != kECKeySize) return;
    AppendECPublicKeyAsDATA(appendTo, ccec_ctx_pub(fullKey->_key));
}

void SecFDHKAppendCompactPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo)
{
    if(ccec_ctx_bitlen(fullKey->_key) != kECKeySize) return;
    AppendECCompactPublicKey(appendTo, ccec_ctx_pub(fullKey->_key));
}


uint8_t* SecFDHKGetHash(SecOTRFullDHKeyRef fullKey)
{
    return fullKey->keyHash;
}




//
//
//
struct _SecOTRPublicDHKey {
    CFRuntimeBase _base;

    ccec_pub_ctx_decl(ccn_sizeof(kECKeySize), _key);
    uint8_t keyHash[CCSHA1_OUTPUT_SIZE];
};

CFGiblisWithCompareFor(SecOTRPublicDHKey);

static CFStringRef SecOTRPublicDHKeyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecOTRPublicDHKeyRef publicDHKey = (SecOTRPublicDHKeyRef)cf;

    __block CFStringRef description = NULL;
    withXandY(publicDHKey->_key, ^(CFStringRef x, CFStringRef y) {
        BufferPerformWithHexString(publicDHKey->keyHash, sizeof(publicDHKey->keyHash), ^(CFStringRef dataString) {
            description = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRPublicDHKeyRef@%p: x: %@ y: %@ [%@]>"), publicDHKey, x, y, dataString);
        });

    });
        return description;
}

static Boolean SecOTRPublicDHKeyCompare(CFTypeRef leftCF, CFTypeRef rightCF)
{
    SecOTRPublicDHKeyRef left = (SecOTRPublicDHKeyRef)leftCF;
    SecOTRPublicDHKeyRef right = (SecOTRPublicDHKeyRef)rightCF;

    return 0 == memcmp(left->keyHash, right->keyHash, sizeof(left->keyHash));
}

static void SecOTRPublicDHKeyDestroy(CFTypeRef cf) {
    SecOTRPublicDHKeyRef pubKey = (SecOTRPublicDHKeyRef)cf;
    (void) pubKey;
}

static inline OSStatus SecOTRPDHKUpdateHash(SecOTRPublicDHKeyRef pubKey)
{
    return GenerateHashForKey(pubKey->_key, pubKey->keyHash);
}

static void ccec_copy_public(ccec_pub_ctx_t source, ccec_pub_ctx_t dest)
{
    cc_size sourceKeyN = ccec_ctx_n(source);
    memcpy(dest, source, ccec_pub_ctx_size(ccn_sizeof_n(sourceKeyN)));
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromFullKey(CFAllocatorRef allocator, SecOTRFullDHKeyRef full)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);
    
    ccec_copy_public(ccec_ctx_pub(full->_key), newPDHK->_key);
    memcpy(newPDHK->keyHash, full->keyHash, CCSHA1_OUTPUT_SIZE);

    return newPDHK;
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromSerialization(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size)
{
    size_t publicKeySize;
    {
        uint32_t readSize = 0;
        require_noerr(ReadLong(bytes, size, &readSize), fail);
        publicKeySize = readSize;
    }

    require(publicKeySize <= *size, fail);

    *size -= publicKeySize;

    return SecOTRPublicDHKCreateFromBytes(allocator, bytes, &publicKeySize);
fail:
    return NULL;
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromCompactSerialization(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);

    size_t publicKeySize = ccec_cp_prime_size(ccec_cp_256());

    require_quiet(publicKeySize <= *size, fail);

    require_noerr_quiet(ccec_compact_import_pub(ccec_cp_256(), publicKeySize, *bytes, newPDHK->_key), fail);

    *size -= publicKeySize;
    *bytes += publicKeySize;

    require_noerr(SecOTRPDHKUpdateHash(newPDHK), fail);

    return newPDHK;
fail:
    CFReleaseNull(newPDHK);
    return NULL;
}


SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);

    require_noerr(ccec_import_pub(ccec_cp_256(), *size, *bytes, newPDHK->_key), fail);

    *bytes += *size;
    *size = 0;

    require_noerr(SecOTRPDHKUpdateHash(newPDHK), fail);

    return newPDHK;
fail:
    CFReleaseNull(newPDHK);
    return NULL;
}


void SecPDHKAppendSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo)
{
    AppendECPublicKeyAsDATA(appendTo, pubKey->_key);
}

void SecPDHKAppendCompactSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo)
{
    AppendECPublicKeyAsDATA(appendTo, pubKey->_key);
}


uint8_t* SecPDHKGetHash(SecOTRPublicDHKeyRef pubKey)
{
    return pubKey->keyHash;
}


void SecPDHKeyGenerateS(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey, cc_unit* s)
{
    ccn_zero(kExponentiationUnits, s);

    size_t keyLen = ccn_sizeof_n(kExponentiationUnits);
    ccec_compute_key(myKey->_key, theirKey->_key, &keyLen, (uint8_t*)s);
}

static int ccec_cmp(ccec_pub_ctx_t l, ccec_pub_ctx_t r)
{
    size_t lsize = ccec_export_pub_size(l);
    size_t rsize = ccec_export_pub_size(r);
    
    int result = 0;
    
    if (lsize == rsize) {
        // Keys should never be larger than 256.
        // But if they are, we want to draw attention to it.
        if (lsize > 65) {
            secerror("The size of an SecOTRDHKey is larger than 65 bytes. \
                     This is not supported in SecOTR and will result in malformed ciphertexts.");
            return false;
        }

        uint8_t lpub[65];
        uint8_t rpub[65];
        
        ccec_export_pub(l, lpub);
        ccec_export_pub(r, rpub);

        result = memcmp(lpub, rpub, lsize);
    } else {
        result = rsize < lsize ? -1 : 1;
    }
    
    return result;
}

bool SecDHKIsGreater(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey)
{
    return ccec_cmp(ccec_ctx_pub(myKey->_key), theirKey->_key) > 0;
}

static void DeriveKeys(CFDataRef dataToHash,
                       uint8_t* messageKey,
                       uint8_t* macKey)
{
    if (messageKey == NULL && macKey == NULL)
        return;

    uint8_t hashedSharedKey[CCSHA1_OUTPUT_SIZE];

    ccdigest(ccsha1_di(), CFDataGetLength(dataToHash), CFDataGetBytePtr(dataToHash), hashedSharedKey);

    if (messageKey)
        memcpy(messageKey, hashedSharedKey, kOTRMessageKeyBytes);
    
    if (macKey) {
        ccdigest(ccsha1_di(), kOTRMessageKeyBytes, messageKey, macKey);
    }
    
    bzero(hashedSharedKey, sizeof(hashedSharedKey));
}

void SecOTRDHKGenerateOTRKeys(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey,
                           uint8_t* sendMessageKey, uint8_t* sendMacKey,
                           uint8_t* receiveMessageKey, uint8_t* receiveMacKey)
{
    CFMutableDataRef dataToHash = CFDataCreateMutable(kCFAllocatorDefault, 0);

    {
        cc_unit s[kExponentiationUnits];

        SecPDHKeyGenerateS(myKey, theirKey, s);
        AppendByte(dataToHash, SecDHKIsGreater(myKey, theirKey) ? 0x01 : 0x02);
        AppendMPI(dataToHash, kExponentiationUnits, s);

        ccn_zero(kExponentiationUnits, s);
    }

    DeriveKeys(dataToHash, receiveMessageKey, receiveMacKey);

    uint8_t *messageTypeByte = CFDataGetMutableBytePtr(dataToHash);

    *messageTypeByte ^= 0x03; // Invert the bits since it's either 1 or 2.

    DeriveKeys(dataToHash, sendMessageKey, sendMacKey);

    CFReleaseNull(dataToHash);
}
