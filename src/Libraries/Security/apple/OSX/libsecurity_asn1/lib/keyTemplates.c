/*
 * Copyright (c) 2003-2006,2008,2010-2012 Apple Inc. All Rights Reserved.
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
 *
 * keyTemplate.cpp -  ASN1 templates for asymmetric keys and related
 * structs.
 */

#include "SecAsn1Templates.h"
#include <stddef.h>
#include "keyTemplates.h"

/* AlgorithmIdentifier : SecAsn1AlgId */
const SecAsn1Template kSecAsn1AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1AlgId) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SecAsn1AlgId,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(SecAsn1AlgId,parameters), },
    { 0, }
};

/* SubjectPublicKeyInfo : SecAsn1PubKeyInfo */
const SecAsn1Template kSecAsn1SubjectPublicKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1PubKeyInfo) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1PubKeyInfo,algorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(SecAsn1PubKeyInfo,subjectPublicKey), },
    { 0, }
};

/* Attribute : NSS_Attribute */
const SecAsn1Template kSecAsn1AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(NSS_Attribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(NSS_Attribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(NSS_Attribute, attrValue),
        kSecAsn1AnyTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1AttributeTemplate },
};

/* PKCS8 PrivateKeyInfo : NSS_PrivateKeyInfo */
const SecAsn1Template kSecAsn1PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_PrivateKeyInfo) },
    { SEC_ASN1_INTEGER, offsetof(NSS_PrivateKeyInfo,version) },
    { SEC_ASN1_INLINE, offsetof(NSS_PrivateKeyInfo,algorithm),
        kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_PrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_PrivateKeyInfo,attributes),
        kSecAsn1SetOfAttributeTemplate },
    { 0 }
};

/* NSS_EncryptedPrivateKeyInfo */
const SecAsn1Template kSecAsn1EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_EncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_EncryptedPrivateKeyInfo,algorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, 
	  offsetof(NSS_EncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

/* DigestInfo: NSS_DigestInfo */
const SecAsn1Template kSecAsn1DigestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DigestInfo) },
    { SEC_ASN1_INLINE, offsetof(NSS_DigestInfo,digestAlgorithm),
        kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DigestInfo,digest) },
    { 0 }
};

// MARK: -
// MARK: *** RSA ***

/*** RSA public key, PKCS1 format : NSS_RSAPublicKeyPKCS1 ***/
const SecAsn1Template kSecAsn1RSAPublicKeyPKCS1Template[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_RSAPublicKeyPKCS1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPublicKeyPKCS1,modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPublicKeyPKCS1,publicExponent) },
    { 0, }
};

/*** RSA private key key, PKCS1 format : NSS_RSAPrivateKeyPKCS1 ***/
const SecAsn1Template kSecAsn1RSAPrivateKeyPKCS1Template[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_RSAPrivateKeyPKCS1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,version) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,prime1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,prime2) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,exponent1) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,exponent2) },
    { SEC_ASN1_INTEGER, offsetof(NSS_RSAPrivateKeyPKCS1,coefficient) },
    { 0, }
};

// MARK: -
// MARK: *** Diffie-Hellman ***

/****
 **** Diffie-Hellman, from PKCS3.
 ****/
const SecAsn1Template kSecAsn1DHParameterTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHParameter) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHParameter,prime) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHParameter,base) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof(NSS_DHParameter,privateValueLength) },
	{ 0, }
};

const SecAsn1Template kSecAsn1DHParameterBlockTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHParameterBlock) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHParameterBlock, oid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHParameterBlock, params),
	  kSecAsn1DHParameterTemplate },
	{ 0, }
};

const SecAsn1Template kSecAsn1DHPrivateKeyTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPrivateKey) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHPrivateKey, dhOid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHPrivateKey, params),
	  kSecAsn1DHParameterTemplate },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHPrivateKey,secretPart) },
	{ 0, }
};

/*
 * Diffie-Hellman, X9.42 style.
 */
const SecAsn1Template kSecAsn1DHValidationParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHValidationParams) },
	{ SEC_ASN1_BIT_STRING, offsetof(NSS_DHValidationParams, seed) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHValidationParams, pGenCounter) },
	{ 0, }
};

const SecAsn1Template kSecAsn1DHDomainParamsX942Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHDomainParamsX942) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, p) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, g) },
	{ SEC_ASN1_INTEGER, offsetof(NSS_DHDomainParamsX942, q) },
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_DHDomainParamsX942, j) },
	{ SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_DHDomainParamsX942, valParams),
	  kSecAsn1DHValidationParamsTemplate },
	{ 0, }
};

const SecAsn1Template kSecAsn1DHAlgorithmIdentifierX942Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHAlgorithmIdentifierX942) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DHAlgorithmIdentifierX942, oid) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DHAlgorithmIdentifierX942, params),
	  kSecAsn1DHDomainParamsX942Template },
	{ 0, }
};

const SecAsn1Template kSecAsn1DHPrivateKeyPKCS8Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPrivateKeyPKCS8) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DHPrivateKeyPKCS8,version) },
    { SEC_ASN1_INLINE, offsetof(NSS_DHPrivateKeyPKCS8,algorithm),
        kSecAsn1DHAlgorithmIdentifierX942Template },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DHPrivateKeyPKCS8,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_DHPrivateKeyPKCS8,attributes),
        kSecAsn1SetOfAttributeTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1DHPublicKeyX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DHPublicKeyX509) },
    { SEC_ASN1_INLINE, offsetof(NSS_DHPublicKeyX509,algorithm),
        kSecAsn1DHAlgorithmIdentifierX942Template },
	{ SEC_ASN1_BIT_STRING, offsetof(NSS_DHPublicKeyX509, publicKey) },
    { 0 }
};

/* ECDSA Private key */
const SecAsn1Template kSecAsn1ECDSAPrivateKeyInfoTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_ECDSA_PrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSS_ECDSA_PrivateKey,version) },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_ECDSA_PrivateKey,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(NSS_ECDSA_PrivateKey,params), kSecAsn1AnyTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 1, 
	  offsetof(NSS_ECDSA_PrivateKey,pubKey), kSecAsn1BitStringTemplate },
    { 0, }
};

