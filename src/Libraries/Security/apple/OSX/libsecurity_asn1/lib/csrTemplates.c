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
 * csrTemplates.cpp - ASN1 templates Cert Signing Requests (per PKCS10).
 */

#include "SecAsn1Templates.h"
#include <stddef.h>
#include "csrTemplates.h"
#include "keyTemplates.h"

const SecAsn1Template kSecAsn1CertRequestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSCertRequestInfo) },
    { SEC_ASN1_INTEGER,  offsetof(NSSCertRequestInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequestInfo,subject),
	  kSecAsn1NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequestInfo,subjectPublicKeyInfo),
	  kSecAsn1SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCertRequestInfo,attributes),
	  kSecAsn1SetOfAttributeTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1CertRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSCertRequest) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequest,reqInfo),
	  kSecAsn1CertRequestInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequest,signatureAlgorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(NSSCertRequest,signature) },
	{ 0 }
};

const SecAsn1Template kSecAsn1SignedCertRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_SignedCertRequest) },
    { SEC_ASN1_ANY,
	  offsetof(NSS_SignedCertRequest,certRequestBlob),
	  kSecAsn1CertRequestInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_SignedCertRequest,signatureAlgorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(NSS_SignedCertRequest,signature) },
	{ 0 }
};

