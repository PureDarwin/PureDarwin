/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#include "capabilities.h"
#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <Security/Security.h>
#include "CCCryptorTestFuncs.h"
#include "testbyteBuffer.h"
#include "testmore.h"
#include "CommonCryptorPriv.h"

#if (CCSYMCHACHA20 == 0)
entryPoint(CommonCryptoSymChaCha20, "CommonCrypto Symmetric ChaCha20 Testing")
#else

static void testChaCha20InvalidParameters(void)
{
    uint8_t valid_key[32] = {};
    uint8_t valid_nonce[12] = {};

    uint8_t plaintext[1024];
    (void)CCRandomGenerateBytes(plaintext, sizeof(plaintext));
    uint8_t ciphertext[sizeof(plaintext) + 16];

    // Invalid key length
    CCCryptorStatus status = CCCryptorChaCha20(valid_key, sizeof(valid_key) - 1, valid_nonce, sizeof(valid_nonce), 0, plaintext, sizeof(plaintext), ciphertext);
    ok(status == kCCKeySizeError, "Expected %d, got %d", kCCKeySizeError, status);

    // Invalid IV length
    status = CCCryptorChaCha20(valid_key, sizeof(valid_key), valid_nonce, sizeof(valid_nonce) - 1, 0, plaintext, sizeof(plaintext), ciphertext);
    ok(status == kCCParamError, "Expected %d, got %d", kCCParamError, status);
}

#define CRYPTO_MAX_PLAINTEXT_LENGTH 256

static void testChaCha20RoundTrip(void)
{
    uint8_t valid_key[32] = {};
    uint8_t valid_nonce[12] = {};

    for (size_t plaintext_length = 1; plaintext_length < CRYPTO_MAX_PLAINTEXT_LENGTH; plaintext_length++) {
        uint8_t plaintext[CRYPTO_MAX_PLAINTEXT_LENGTH];
        uint8_t ciphertext[CRYPTO_MAX_PLAINTEXT_LENGTH];
        uint8_t recovered_plaintext[CRYPTO_MAX_PLAINTEXT_LENGTH];

        (void)CCRandomGenerateBytes(valid_key, sizeof(valid_key));
        (void)CCRandomGenerateBytes(valid_nonce, sizeof(valid_nonce));
        (void)CCRandomGenerateBytes(plaintext, sizeof(plaintext));
        uint32_t counter = arc4random();

        CCCryptorStatus status = CCCryptorChaCha20(valid_key, sizeof(valid_key), valid_nonce, sizeof(valid_nonce), counter, plaintext, plaintext_length, ciphertext);
        ok(status == kCCSuccess, "Encryption failed. Expected %d, got %d", kCCSuccess, status);

        status = CCCryptorChaCha20(valid_key, sizeof(valid_key), valid_nonce, sizeof(valid_nonce), counter, ciphertext, plaintext_length, recovered_plaintext);
        ok(status == kCCSuccess, "Decryption failed. Expected %d, got %d", kCCSuccess, status);
        ok(0 == memcmp(plaintext, recovered_plaintext, plaintext_length), "Invalid plaintext");
    }
}

static int kTestTestCount = 767;

int
CommonCryptoSymChaCha20(int __unused argc, char *const * __unused argv)
{
    plan_tests(kTestTestCount);
    testChaCha20InvalidParameters();
    testChaCha20RoundTrip();
    return 0;
}

#endif
