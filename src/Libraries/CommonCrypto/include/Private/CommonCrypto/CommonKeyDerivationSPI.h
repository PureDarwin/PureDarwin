// CommonKeyDerivationSPI.h - private CommonCrypto key derivation interfaces
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _CC_KEY_DERIVATION_SPI_H_
#define _CC_KEY_DERIVATION_SPI_H_

#include <stdint.h>
#include <sys/types.h>
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonKeyDerivation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t CCKDFAlgorithm;

enum {
    kCCKDFAlgorithmPBKDF2_HMAC = 0,
    kCCKDFAlgorithmCTR_HMAC,
    kCCKDFAlgorithmCTR_HMAC_FIXED,
    kCCKDFAlgorithmFB_HMAC,
    kCCKDFAlgorithmFB_HMAC_FIXED,
    kCCKDFAlgorithmDPIPE_HMAC,
    kCCKDFAlgorithmHKDF,
    kCCKDFAlgorithmAnsiX963,
};

// one-shot HMAC-based derivation, unused inputs may be NULL with length 0
CCStatus CCKeyDerivationHMac(CCKDFAlgorithm algorithm, CCDigestAlgorithm digest, unsigned rounds,
                             const void *keyDerivationKey, size_t keyDerivationKeyLen,
                             const void *label, size_t labelLen,
                             const void *context, size_t contextLen,
                             const void *iv, size_t ivLen,
                             const void *salt, size_t saltLen,
                             void *derivedKey, size_t derivedKeyLen);

typedef struct CCKDFParameters *CCKDFParametersRef;

CCStatus CCKDFParametersCreatePbkdf2(CCKDFParametersRef *params, unsigned rounds,
                                     const void *salt, size_t saltLen);
CCStatus CCKDFParametersCreateCtrHmac(CCKDFParametersRef *params, const void *label, size_t labelLen,
                                      const void *context, size_t contextLen);
CCStatus CCKDFParametersCreateCtrHmacFixed(CCKDFParametersRef *params, const void *context, size_t contextLen);
CCStatus CCKDFParametersCreateHkdf(CCKDFParametersRef *params, const void *salt, size_t saltLen,
                                   const void *context, size_t contextLen);
CCStatus CCKDFParametersCreateAnsiX963(CCKDFParametersRef *params, const void *sharedinfo, size_t sharedinfoLen);
void CCKDFParametersDestroy(CCKDFParametersRef params);

// derives with the algorithm and inputs held in params
CCStatus CCDeriveKey(const CCKDFParametersRef params, CCDigestAlgorithm digest,
                     const void *keyDerivationKey, size_t keyDerivationKeyLen,
                     void *derivedKey, size_t derivedKeyLen);

// the two HKDF halves, params must come from CCKDFParametersCreateHkdf
CCStatus CCHKDFExtract(const CCKDFParametersRef params, CCDigestAlgorithm digest,
                       const void *keyDerivationKey, size_t keyDerivationKeyLen,
                       void *prk, size_t prkLen);
CCStatus CCHKDFExpand(const CCKDFParametersRef params, CCDigestAlgorithm digest,
                      const void *prk, size_t prkLen,
                      void *derivedKey, size_t derivedKeyLen);

#ifdef __cplusplus
}
#endif

#endif
