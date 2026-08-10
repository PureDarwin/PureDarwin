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
 */
/*
 * osKeyTemplate.h -  ASN1 templates for openssl asymmetric keys
 */

#include "osKeyTemplates.h"
#include <stddef.h>

/**** 
 **** DSA support 
 ****/

/* X509 style DSA algorithm parameters */
const SecAsn1Template kSecAsn1DSAAlgParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgParams) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParams,g) },
	{ 0, }
};

/* BSAFE style DSA algorithm parameters */
const SecAsn1Template kSecAsn1DSAAlgParamsBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgParamsBSAFE) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,keySizeInBits) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAAlgParamsBSAFE,g) },
	{ 0, }
};

/* DSA X509-style AlgorithmID */
const SecAsn1Template kSecAsn1DSAAlgorithmIdX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgorithmIdX509) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DSAAlgorithmIdX509, algorithm) },
	/* per CMS, this is optional */
    { SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_DSAAlgorithmIdX509,params),
	  kSecAsn1DSAAlgParamsTemplate },
	{ 0, }
};

/* DSA BSAFE-style AlgorithmID */
const SecAsn1Template kSecAsn1DSAAlgorithmIdBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAAlgorithmIdBSAFE) },
	{ SEC_ASN1_OBJECT_ID, offsetof(NSS_DSAAlgorithmIdBSAFE, algorithm) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAAlgorithmIdBSAFE,params),
	  kSecAsn1DSAAlgParamsBSAFETemplate },
	{ 0, }
};

/**** 
 **** DSA public keys 
 ****/

/* DSA public key, openssl/X509 format */
const SecAsn1Template kSecAsn1DSAPublicKeyX509Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPublicKeyX509) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPublicKeyX509, dsaAlg),
	  kSecAsn1DSAAlgorithmIdX509Template },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_DSAPublicKeyX509, publicKey), },
	{ 0, }
};

/* DSA public key, BSAFE/FIPS186 format */
const SecAsn1Template kSecAsn1DSAPublicKeyBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPublicKeyBSAFE) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPublicKeyBSAFE, dsaAlg),
	  kSecAsn1DSAAlgorithmIdBSAFETemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_DSAPublicKeyBSAFE, publicKey), },
	{ 0, }
};

/**** 
 **** DSA private keys 
 ****/
 
/* DSA Private key, openssl custom format */
const SecAsn1Template kSecAsn1DSAPrivateKeyOpensslTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyOpenssl) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,version) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,p) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,q) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,g) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,pub) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOpenssl,priv) },
	{ 0, }
};

/*
 * DSA private key, BSAFE/FIPS186 style.
 * This is basically a DSA-specific NSS_PrivateKeyInfo.
 *
 * NSS_DSAPrivateKeyBSAFE.privateKey is an octet string containing
 * the DER encoding of this.
 */
const SecAsn1Template kSecAsn1DSAPrivateKeyOctsTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyOcts) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyOcts,privateKey) },
	{ 0, }
};

const SecAsn1Template kSecAsn1DSAPrivateKeyBSAFETemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyBSAFE) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyBSAFE,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPrivateKeyBSAFE, dsaAlg),
	  kSecAsn1DSAAlgorithmIdBSAFETemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DSAPrivateKeyBSAFE,privateKey) },
	{ 0, }
};

/*
 * DSA Private Key, PKCS8/SMIME style.
 */
const SecAsn1Template kSecAsn1DSAPrivateKeyPKCS8Template[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSAPrivateKeyPKCS8) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSAPrivateKeyPKCS8,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_DSAPrivateKeyPKCS8, dsaAlg),
	  kSecAsn1DSAAlgorithmIdX509Template },
    { SEC_ASN1_OCTET_STRING, offsetof(NSS_DSAPrivateKeyPKCS8,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(NSS_DSAPrivateKeyPKCS8,attributes),
        kSecAsn1SetOfAttributeTemplate },
	{ 0, }
};

const SecAsn1Template kSecAsn1DSASignatureTemplate[] = {
	{ SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_DSASignature) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSASignature,r) },
    { SEC_ASN1_INTEGER, offsetof(NSS_DSASignature,s) },
	{ 0, }
};


