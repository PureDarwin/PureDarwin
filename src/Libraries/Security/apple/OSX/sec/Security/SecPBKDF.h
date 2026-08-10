/*
 *  SecPBKDF.h
 *
 *  Copyright (c) 2010,2012 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CFData.h>

#include <CommonCrypto/CommonHMAC.h>

/* CC Based HMAC PRF functions */
void hmac_sha1_PRF(const uint8_t *key,
                   size_t key_len,
                   const uint8_t *text,
                   size_t text_len,
                   uint8_t digest[CC_SHA1_DIGEST_LENGTH]);

void hmac_sha256_PRF(const uint8_t *key,
                   size_t key_len,
                   const uint8_t *text,
                   size_t text_len,
                   uint8_t digest[CC_SHA256_DIGEST_LENGTH]);


/**
 PBKDF2 key derivation with HMAC-SHA1.

 @param passwordPtr The pointer to the passsword data
 @param passwordLen The password data length
 @param saltPtr The pointer to the salt
 @param saltLen The salt length
 @param iterationCount Number of PBKDF2 iterations
 @param dkPtr The pointer to the derived key
 @param dkLen The derived key length
 @return errSecMemoryError on a failure to allocate the buffer. errSecSuccess otherwise.
 */
OSStatus pbkdf2_hmac_sha1(const uint8_t *passwordPtr, size_t passwordLen,
                      const uint8_t *saltPtr, size_t saltLen,
                      uint32_t iterationCount,
                      void *dkPtr, size_t dkLen);

/**
 PBKDF2 key derivation with HMAC-SHA256.
 
 @param passwordPtr The pointer to the passsword data
 @param passwordLen The password data length
 @param saltPtr The pointer to the salt
 @param saltLen The salt length
 @param iterationCount Number of PBKDF2 iterations
 @param dkPtr The pointer to the derived key
 @param dkLen The derived key length
 @return errSecMemoryError on a failure to allocate the buffer. errSecSuccess otherwise.
 */
OSStatus pbkdf2_hmac_sha256(const uint8_t *passwordPtr, size_t passwordLen,
                      const uint8_t *saltPtr, size_t saltLen,
                      uint32_t iterationCount,
                      void *dkPtr, size_t dkLen);

/* Transformation conveninces from and to CFData where the password bytes used are the UTF-8 representation and 1000 iterations

   This routine promises not to make any copies of the password or salt that aren't
   eradicated before completion.
   
   The size of the result buffer is used to produce the derivedKey.
   
   Be careful when using CFTypes for secrets, they tend to copy data more than you'd like.
   If your password and or salt aren't already in CF types use the buffer versions above.
   
   If you already have the data in this form, the interface will unwrap and not copy the data anywhere extra for you.

   void SecKeyFromPassword_HMAC_sha1(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey)
   {
        pbkdf2_hmac_sha1(CFDataGetBytePtr(password), CFDataGetLength(password),
                         CFDataGetBytePtr(salt), CFDataGetLength(salt),
                         interationCount,
                         CFDataGetMutableBytePtr(derivedKey), CFDataGetLength(derivedKey));
   }
   
   Suggested way to transform strings into data:
   
    CFDataRef   *passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);

    ...

    CFReleaseSafe(passwordData);

*/

/**
 PBKDF2 key derivation with HMAC-SHA1.

 @param password Password data
 @param salt Salt data
 @param interationCount Number of PBKDF2 iterations
 @param derivedKey Mutable data reference to write the result of the key derivation
 @return errSecMemoryError on a failure to allocate the buffer. errSecSuccess otherwise.
 */
OSStatus SecKeyFromPassphraseDataHMACSHA1(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey);

/**
 PBKDF2 key derivation with HMAC-SHA256.
 
 @param password Password data
 @param salt Salt data
 @param interationCount Number of PBKDF2 iterations
 @param derivedKey Mutable data reference to write the result of the key derivation
 @return errSecMemoryError on a failure to allocate the buffer. errSecSuccess otherwise.
 */
OSStatus SecKeyFromPassphraseDataHMACSHA256(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey);
