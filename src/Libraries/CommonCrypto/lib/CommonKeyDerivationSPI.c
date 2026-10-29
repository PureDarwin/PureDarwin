// CommonKeyDerivationSPI.c - HMAC key derivation functions over corecrypto
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CommonCrypto/CommonKeyDerivationSPI.h>
#include <corecrypto/ccansikdf.h>
#include <corecrypto/cchkdf.h>
#include <corecrypto/ccnistkdf.h>
#include <corecrypto/ccpbkdf2.h>
#include <stdlib.h>
#include <string.h>
#include "CommonDigestPriv.h"

struct CCKDFParameters {
    CCKDFAlgorithm algorithm;
    unsigned rounds;
    size_t saltLen;
    uint8_t *salt;
    size_t labelLen;
    uint8_t *label;
    size_t contextLen;
    uint8_t *context;
};

static CCStatus
kdf_status(int rc)
{
    return rc == 0 ? kCCSuccess : kCCParamError;
}

static const struct ccdigest_info *
kdf_digest(CCDigestAlgorithm digest)
{
    if (digest == kCCDigestNone || digest >= kCCDigestMax) return NULL;
    return CCDigestGetDigestInfo(digest);
}

static CCStatus
kdf_copy(uint8_t **dst, size_t *dstLen, const void *src, size_t len)
{
    *dst = NULL;
    *dstLen = 0;
    if (len == 0) return kCCSuccess;
    if (src == NULL) return kCCParamError;

    *dst = malloc(len);
    if (*dst == NULL) return kCCMemoryFailure;

    memcpy(*dst, src, len);
    *dstLen = len;
    return kCCSuccess;
}

static CCStatus
kdf_create(CCKDFParametersRef *params, CCKDFAlgorithm algorithm, unsigned rounds,
           const void *salt, size_t saltLen, const void *label, size_t labelLen,
           const void *context, size_t contextLen)
{
    CCKDFParametersRef p;
    CCStatus st;

    if (params == NULL) return kCCParamError;
    *params = NULL;

    p = calloc(1, sizeof(*p));
    if (p == NULL) return kCCMemoryFailure;

    p->algorithm = algorithm;
    p->rounds = rounds;
    st = kdf_copy(&p->salt, &p->saltLen, salt, saltLen);
    if (st == kCCSuccess) st = kdf_copy(&p->label, &p->labelLen, label, labelLen);
    if (st == kCCSuccess) st = kdf_copy(&p->context, &p->contextLen, context, contextLen);
    if (st != kCCSuccess) {
        CCKDFParametersDestroy(p);
        return st;
    }

    *params = p;
    return kCCSuccess;
}

CCStatus
CCKDFParametersCreatePbkdf2(CCKDFParametersRef *params, unsigned rounds, const void *salt, size_t saltLen)
{
    if (rounds == 0) return kCCParamError;
    return kdf_create(params, kCCKDFAlgorithmPBKDF2_HMAC, rounds, salt, saltLen, NULL, 0, NULL, 0);
}

CCStatus
CCKDFParametersCreateCtrHmac(CCKDFParametersRef *params, const void *label, size_t labelLen,
                             const void *context, size_t contextLen)
{
    return kdf_create(params, kCCKDFAlgorithmCTR_HMAC, 0, NULL, 0, label, labelLen, context, contextLen);
}

CCStatus
CCKDFParametersCreateCtrHmacFixed(CCKDFParametersRef *params, const void *context, size_t contextLen)
{
    return kdf_create(params, kCCKDFAlgorithmCTR_HMAC_FIXED, 0, NULL, 0, NULL, 0, context, contextLen);
}

CCStatus
CCKDFParametersCreateHkdf(CCKDFParametersRef *params, const void *salt, size_t saltLen,
                          const void *context, size_t contextLen)
{
    return kdf_create(params, kCCKDFAlgorithmHKDF, 0, salt, saltLen, NULL, 0, context, contextLen);
}

CCStatus
CCKDFParametersCreateAnsiX963(CCKDFParametersRef *params, const void *sharedinfo, size_t sharedinfoLen)
{
    return kdf_create(params, kCCKDFAlgorithmAnsiX963, 0, NULL, 0, NULL, 0, sharedinfo, sharedinfoLen);
}

void
CCKDFParametersDestroy(CCKDFParametersRef params)
{
    if (params == NULL) return;
    free(params->salt);
    free(params->label);
    free(params->context);
    free(params);
}

static CCStatus
kdf_derive(CCKDFAlgorithm algorithm, const struct ccdigest_info *di, unsigned rounds,
           const void *kdk, size_t kdkLen, const void *label, size_t labelLen,
           const void *context, size_t contextLen, const void *salt, size_t saltLen,
           void *dk, size_t dkLen)
{
    switch (algorithm) {
    case kCCKDFAlgorithmPBKDF2_HMAC:
        // RFC 8018 section 5.2
        if (rounds == 0) return kCCParamError;
        return kdf_status(ccpbkdf2_hmac(di, kdkLen, kdk, saltLen, salt, rounds, dkLen, dk));
    case kCCKDFAlgorithmCTR_HMAC:
        // SP 800-108 section 4.1, fixed input label || 0x00 || context || [L]32
        return kdf_status(ccnistkdf_ctr_hmac(di, kdkLen, kdk, labelLen, label, contextLen, context, dkLen, dk));
    case kCCKDFAlgorithmCTR_HMAC_FIXED:
        // SP 800-108 section 4.1 with the caller's own fixed input data
        return kdf_status(ccnistkdf_ctr_hmac_fixed(di, kdkLen, kdk, contextLen, context, dkLen, dk));
    case kCCKDFAlgorithmHKDF:
        // RFC 5869 section 2, context is the info string
        return kdf_status(cchkdf(di, kdkLen, kdk, saltLen, salt, contextLen, context, dkLen, dk));
    case kCCKDFAlgorithmAnsiX963:
        // ANSI X9.63 section 5.6.3, also SEC 1 v2 section 3.6.1, context is SharedInfo
        return kdf_status(ccansikdf_x963(di, kdkLen, kdk, contextLen, context, dkLen, dk));
    case kCCKDFAlgorithmFB_HMAC:
    case kCCKDFAlgorithmFB_HMAC_FIXED:
    case kCCKDFAlgorithmDPIPE_HMAC:
        // feedback and pipeline modes have no corecrypto backend yet
        return kCCUnimplemented;
    default:
        return kCCParamError;
    }
}

CCStatus
CCKeyDerivationHMac(CCKDFAlgorithm algorithm, CCDigestAlgorithm digest, unsigned rounds,
                    const void *keyDerivationKey, size_t keyDerivationKeyLen,
                    const void *label, size_t labelLen,
                    const void *context, size_t contextLen,
                    const void *iv, size_t ivLen,
                    const void *salt, size_t saltLen,
                    void *derivedKey, size_t derivedKeyLen)
{
    const struct ccdigest_info *di = kdf_digest(digest);

    (void)iv;
    (void)ivLen;
    if (di == NULL || derivedKey == NULL || derivedKeyLen == 0) return kCCParamError;
    if (keyDerivationKey == NULL && keyDerivationKeyLen != 0) return kCCParamError;

    return kdf_derive(algorithm, di, rounds, keyDerivationKey, keyDerivationKeyLen,
                      label, labelLen, context, contextLen, salt, saltLen, derivedKey, derivedKeyLen);
}

CCStatus
CCDeriveKey(const CCKDFParametersRef params, CCDigestAlgorithm digest,
            const void *keyDerivationKey, size_t keyDerivationKeyLen,
            void *derivedKey, size_t derivedKeyLen)
{
    const struct ccdigest_info *di = kdf_digest(digest);

    if (params == NULL || di == NULL || derivedKey == NULL || derivedKeyLen == 0) return kCCParamError;
    if (keyDerivationKey == NULL && keyDerivationKeyLen != 0) return kCCParamError;

    return kdf_derive(params->algorithm, di, params->rounds, keyDerivationKey, keyDerivationKeyLen,
                      params->label, params->labelLen, params->context, params->contextLen,
                      params->salt, params->saltLen, derivedKey, derivedKeyLen);
}

CCStatus
CCHKDFExtract(const CCKDFParametersRef params, CCDigestAlgorithm digest,
              const void *keyDerivationKey, size_t keyDerivationKeyLen,
              void *prk, size_t prkLen)
{
    const struct ccdigest_info *di = kdf_digest(digest);

    if (params == NULL || params->algorithm != kCCKDFAlgorithmHKDF || di == NULL || prk == NULL) return kCCParamError;
    if (keyDerivationKey == NULL && keyDerivationKeyLen != 0) return kCCParamError;
    if (prkLen < di->output_size) return kCCBufferTooSmall;

    // RFC 5869 section 2.2
    return kdf_status(cchkdf_extract(di, params->saltLen, params->salt, keyDerivationKeyLen, keyDerivationKey, prk));
}

CCStatus
CCHKDFExpand(const CCKDFParametersRef params, CCDigestAlgorithm digest,
             const void *prk, size_t prkLen,
             void *derivedKey, size_t derivedKeyLen)
{
    const struct ccdigest_info *di = kdf_digest(digest);

    if (params == NULL || params->algorithm != kCCKDFAlgorithmHKDF || di == NULL) return kCCParamError;
    if (prk == NULL || derivedKey == NULL || derivedKeyLen == 0) return kCCParamError;

    // RFC 5869 section 2.3
    return kdf_status(cchkdf_expand(di, prkLen, prk, params->contextLen, params->context, derivedKeyLen, derivedKey));
}
