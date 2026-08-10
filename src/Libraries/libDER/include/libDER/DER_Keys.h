/*
 * DER_Keys.h - key and algorithm structures.
 */
#ifndef _DER_KEYS_H_
#define _DER_KEYS_H_

#include <libDER/DER_Decode.h>

__BEGIN_DECLS

/* AlgorithmIdentifier ::= SEQUENCE {
 *     algorithm   OBJECT IDENTIFIER,
 *     parameters  ANY DEFINED BY algorithm OPTIONAL }
 */
typedef struct {
    DERItem oid;
    DERItem params;
} DERAlgorithmId;

extern const DERItemSpec DERAlgorithmIdItemSpecs[];
extern const DERShort    DERNumAlgorithmIdItemSpecs;

/* SubjectPublicKeyInfo ::= SEQUENCE {
 *     algorithm         AlgorithmIdentifier,
 *     subjectPublicKey  BIT STRING }
 */
typedef struct {
    DERItem algId;
    DERItem pubKey;
} DERSubjPubKeyInfo;

extern const DERItemSpec DERSubjPubKeyInfoItemSpecs[];
extern const DERShort    DERNumSubjPubKeyInfoItemSpecs;

/* RSAPublicKey ::= SEQUENCE {
 *     modulus         INTEGER,
 *     publicExponent  INTEGER }      -- PKCS#1
 */
typedef struct {
    DERItem modulus;
    DERItem pubExponent;
} DERRSAPubKeyPKCS1;

extern const DERItemSpec DERRSAPubKeyPKCS1ItemSpecs[];
extern const DERShort    DERNumRSAPubKeyPKCS1ItemSpecs;

/* Apple's RSA public key form carries a precomputed reciprocal between the
 * modulus and the exponent. Callers may ignore the reciprocal, but it has to be
 * in the spec table or the surrounding fields decode into the wrong slots. */
typedef struct {
    DERItem modulus;
    DERItem reciprocal;
    DERItem pubExponent;
} DERRSAPubKeyApple;

extern const DERItemSpec DERRSAPubKeyAppleItemSpecs[];
extern const DERShort    DERNumRSAPubKeyAppleItemSpecs;

/* RSAPrivateKey ::= SEQUENCE {
 *     version, modulus, publicExponent, privateExponent,
 *     prime1, prime2, exponent1, exponent2, coefficient }   -- PKCS#1
 */
typedef struct {
    DERItem version;
    DERItem modulus;
    DERItem pubExponent;
    DERItem privExponent;
    DERItem prime1;
    DERItem prime2;
    DERItem exponent1;
    DERItem exponent2;
    DERItem coefficient;
} DERRSAPrivKeyCRT;

extern const DERItemSpec DERRSAPrivKeyCRTItemSpecs[];
extern const DERShort    DERNumRSAPrivKeyCRTItemSpecs;

__END_DECLS

#endif /* _DER_KEYS_H_ */
