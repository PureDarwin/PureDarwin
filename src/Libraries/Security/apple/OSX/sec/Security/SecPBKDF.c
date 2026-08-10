/*
 *  SecPBKDF.c
 *
 *  Copyright (c) 2010,2012 Apple Inc. All Rights Reserved.
 *
 */

#include "Security/SecPBKDF.h"
#include "Security/pbkdf2.h"

#include <CommonCrypto/CommonHMAC.h>
#include "Security/SecBase.h"

#include <stdlib.h>
#include <string.h>

/* CC Based HMAC PRF functions */
void hmac_sha1_PRF(const uint8_t *key,
                   size_t key_len,
                   const uint8_t *text,
                   size_t text_len,
                   uint8_t digest[CC_SHA1_DIGEST_LENGTH])
{
    CCHmacContext hmac_sha1_context;
    
    CCHmacInit(&hmac_sha1_context, kCCHmacAlgSHA1, key, key_len);
    CCHmacUpdate(&hmac_sha1_context, text, text_len);
    CCHmacFinal(&hmac_sha1_context, digest);
}

void hmac_sha256_PRF(const uint8_t *key,
                   size_t key_len,
                   const uint8_t *text,
                   size_t text_len,
                   uint8_t digest[CC_SHA256_DIGEST_LENGTH])
{
    CCHmacContext hmac_sha256_context;

    CCHmacInit(&hmac_sha256_context, kCCHmacAlgSHA256, key, key_len);
    CCHmacUpdate(&hmac_sha256_context, text, text_len);
    CCHmacFinal(&hmac_sha256_context, digest);
}


/* This implements the HMAC SHA-1 version of pbkdf2 and allocates a local buffer for the HMAC */
OSStatus pbkdf2_hmac_sha1(const uint8_t *passwordPtr, size_t passwordLen,
                      const uint8_t *saltPtr, size_t saltLen,
                      uint32_t iterationCount,
                      void *dkPtr, size_t dkLen)
{
    // MAX(salt_length + 4, 20 /* SHA1 Digest size */) + 2 * 20;
    // salt_length + HASH_SIZE is bigger than either salt + 4 and digestSize.
    size_t kBigEnoughSize = (saltLen + CC_SHA1_DIGEST_LENGTH) + 2 * CC_SHA1_DIGEST_LENGTH;
    uint8_t *temp_data = malloc(kBigEnoughSize);
    
    if (temp_data == NULL) {
        return errSecMemoryError;
    }

    pbkdf2(hmac_sha1_PRF, CC_SHA1_DIGEST_LENGTH,
           passwordPtr, passwordLen,
           saltPtr, saltLen,
           iterationCount,
           dkPtr, dkLen,
           temp_data);
                   
    bzero(temp_data, kBigEnoughSize);
    
    return errSecSuccess;
}

/* This implements the HMAC SHA-256 version of pbkdf2 and allocates a local buffer for the HMAC */
OSStatus pbkdf2_hmac_sha256(const uint8_t *passwordPtr, size_t passwordLen,
                      const uint8_t *saltPtr, size_t saltLen,
                      uint32_t iterationCount,
                      void *dkPtr, size_t dkLen)
{
    // MAX(salt_length + 4, 32 /* SHA1 Digest size */) + 2 * 32;
    // salt_length + HASH_SIZE is bigger than either salt + 4 and digestSize.
    size_t kBigEnoughSize = (saltLen + CC_SHA256_DIGEST_LENGTH) + 2 * CC_SHA256_DIGEST_LENGTH;
    uint8_t *temp_data = malloc(kBigEnoughSize);
    
    if (temp_data == NULL) {
        return errSecMemoryError;
    }

    pbkdf2(hmac_sha256_PRF, CC_SHA256_DIGEST_LENGTH,
           passwordPtr, passwordLen,
           saltPtr, saltLen,
           iterationCount,
           dkPtr, dkLen,
           temp_data);

    bzero(temp_data, kBigEnoughSize);
    
    return errSecSuccess;
}

OSStatus SecKeyFromPassphraseDataHMACSHA1(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey)
{
    return pbkdf2_hmac_sha1(CFDataGetBytePtr(password), CFDataGetLength(password),
                     CFDataGetBytePtr(salt), CFDataGetLength(salt),
                     interationCount,
                     CFDataGetMutableBytePtr(derivedKey), CFDataGetLength(derivedKey));

}

OSStatus SecKeyFromPassphraseDataHMACSHA256(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey)
{
    return pbkdf2_hmac_sha256(CFDataGetBytePtr(password), CFDataGetLength(password),
                       CFDataGetBytePtr(salt), CFDataGetLength(salt),
                       interationCount,
                       CFDataGetMutableBytePtr(derivedKey), CFDataGetLength(derivedKey));

}
