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
 * csrTemplates.h - ASN1 templates Cert Signing Requests (per PKCS10).
 */

#ifndef	_NSS_CSR_TEMPLATES_H_
#define _NSS_CSR_TEMPLATES_H_

#include <Security/X509Templates.h>
#include <Security/keyTemplates.h>	/* for NSS_Attribute */

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * ASN class : CertificationRequestInfo
 * C struct  : NSSCertRequestInfo
 */
typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER {
	SecAsn1Item							version;
    NSS_Name 							subject;
    SecAsn1PubKeyInfo 	subjectPublicKeyInfo;
	NSS_Attribute						**attributes;
} NSSCertRequestInfo DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

extern const SecAsn1Template kSecAsn1CertRequestInfoTemplate[];

/* 
 * ASN class : CertificationRequest
 * C struct  : NSSCertRequest
 */
typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER {
	NSSCertRequestInfo				reqInfo;
    SecAsn1AlgId 	signatureAlgorithm;
    SecAsn1Item 						signature;// BIT STRING, length in bits	
} NSSCertRequest DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

extern const SecAsn1Template kSecAsn1CertRequestTemplate[];

/*
 * This is what we use use to avoid unnecessary setup and teardown of 
 * a full NSSCertRequest when signing and verifying.
 */
typedef struct DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER {
	SecAsn1Item						certRequestBlob;	// encoded, ASN_ANY
    SecAsn1AlgId 	signatureAlgorithm;
    SecAsn1Item 						signature;// BIT STRING, length in bits	
} NSS_SignedCertRequest DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

extern const SecAsn1Template kSecAsn1SignedCertRequestTemplate[];

#ifdef  __cplusplus
}
#endif

#endif	/* _NSS_CSR_TEMPLATES_H_ */
