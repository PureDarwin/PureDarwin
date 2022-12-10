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

#include "ccdebug.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

#include <corecrypto/cc.h>
#include <corecrypto/cc_priv.h>
#include <corecrypto/ccchacha20poly1305.h>
#include <corecrypto/ccchacha20poly1305_priv.h>

static inline CCCryptorStatus translate_corecrypto_error_code(int error)
{
    switch (error) {
        case CCERR_OK:
            return kCCSuccess;
        case CCERR_PARAMETER:
            return kCCParamError;
        default:
            return kCCUnspecifiedError;
    }
}

static inline CCCryptorStatus validate_parameters(size_t keyLength, size_t nonceLength)
{
    if (keyLength != CCCHACHA20_KEY_NBYTES) {
        return kCCKeySizeError;
    }
    if (nonceLength != CCCHACHA20_NONCE_NBYTES) {
        return kCCParamError;
    }

    return kCCSuccess;
}

CCCryptorStatus CCCryptorChaCha20(const void *key, size_t keyLength,
                                  const void *nonce, size_t nonceLength,
                                  uint32_t counter,
                                  const void *dataIn, size_t dataInLength, void *dataOut)
{
    CCCryptorStatus status = validate_parameters(keyLength, nonceLength);
    if (status != kCCSuccess) {
        return status;
    }

    int result = ccchacha20(key, nonce, counter, dataInLength, dataIn, dataOut);
    return translate_corecrypto_error_code(result);
}
