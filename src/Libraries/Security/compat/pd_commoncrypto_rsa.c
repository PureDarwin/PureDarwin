// pd_commoncrypto_rsa.c - the three CommonCrypto RSA calls configd links from Security.framework
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// CommonCrypto's RSA (CommonRSACryptor.c) needs corecrypto's private-key RSA, which the corecrypto fork does not
// have yet, so Security leaves it out. Key generation failed before too, its corecrypto was a stub: configd gets
// the same kCCUnimplemented as then, and no key is ever made to export or release
#include <stddef.h>
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonRSACryptor.h>

CCCryptorStatus CCRSACryptorGeneratePair(size_t keysize, uint32_t e, CCRSACryptorRef *publicKey,
    CCRSACryptorRef *privateKey)
{
    (void)keysize; (void)e;
    if (publicKey != NULL)
        *publicKey = NULL;
    if (privateKey != NULL)
        *privateKey = NULL;
    return kCCUnimplemented;
}

CCCryptorStatus CCRSACryptorExport(CCRSACryptorRef key, void *out, size_t *outLen)
{
    (void)key; (void)out;
    if (outLen != NULL)
        *outLen = 0;
    return kCCUnimplemented;
}

void CCRSACryptorRelease(CCRSACryptorRef key)
{
    (void)key;
}
