/*
 * DER_Keys.c - key and algorithm spec tables.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

/*
 * Layouts follow RFC 5280 and PKCS#1.
 */

#include <libDER/DER_Keys.h>

/* AlgorithmIdentifier - parameters are ANY and optional (absent for ECDSA,
 * NULL for the RSA PKCS#1 algorithms). */
const DERItemSpec DERAlgorithmIdItemSpecs[] = {
    { DER_OFFSET(DERAlgorithmId, oid),    ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERAlgorithmId, params), 0,
      DER_DEC_ASN_ANY | DER_DEC_OPTIONAL | DER_DEC_SAVE_DER },
};
const DERShort DERNumAlgorithmIdItemSpecs =
    sizeof(DERAlgorithmIdItemSpecs) / sizeof(DERItemSpec);

/* SubjectPublicKeyInfo */
const DERItemSpec DERSubjPubKeyInfoItemSpecs[] = {
    { DER_OFFSET(DERSubjPubKeyInfo, algId),  ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERSubjPubKeyInfo, pubKey), ASN1_BIT_STRING,      DER_DEC_NO_OPTS },
};
const DERShort DERNumSubjPubKeyInfoItemSpecs =
    sizeof(DERSubjPubKeyInfoItemSpecs) / sizeof(DERItemSpec);

/* RSAPublicKey (PKCS#1). The values are unsigned magnitudes on the way in and
 * need the INTEGER sign byte on the way out. */
const DERItemSpec DERRSAPubKeyPKCS1ItemSpecs[] = {
    { DER_OFFSET(DERRSAPubKeyPKCS1, modulus),     ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPubKeyPKCS1, pubExponent), ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
};
const DERShort DERNumRSAPubKeyPKCS1ItemSpecs =
    sizeof(DERRSAPubKeyPKCS1ItemSpecs) / sizeof(DERItemSpec);

/* Apple's RSA public key form: the reciprocal sits between the modulus and the
 * exponent. Callers ignore it, but it must be decoded or the exponent lands in
 * the wrong field. */
const DERItemSpec DERRSAPubKeyAppleItemSpecs[] = {
    { DER_OFFSET(DERRSAPubKeyApple, modulus),     ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPubKeyApple, reciprocal),  ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPubKeyApple, pubExponent), ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
};
const DERShort DERNumRSAPubKeyAppleItemSpecs =
    sizeof(DERRSAPubKeyAppleItemSpecs) / sizeof(DERItemSpec);

/* RSAPrivateKey (PKCS#1, CRT form). */
const DERItemSpec DERRSAPrivKeyCRTItemSpecs[] = {
    { DER_OFFSET(DERRSAPrivKeyCRT, version),      ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, modulus),      ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, pubExponent),  ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, privExponent), ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, prime1),       ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, prime2),       ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, exponent1),    ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, exponent2),    ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
    { DER_OFFSET(DERRSAPrivKeyCRT, coefficient),  ASN1_INTEGER, DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
};
const DERShort DERNumRSAPrivKeyCRTItemSpecs =
    sizeof(DERRSAPrivKeyCRTItemSpecs) / sizeof(DERItemSpec);
