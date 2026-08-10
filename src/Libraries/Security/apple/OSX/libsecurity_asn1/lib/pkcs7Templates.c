/*
 * Copyright (c) 2003-2004,2008,2010,2012 Apple Inc. All Rights Reserved.
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
 * pkcs7Templates.cpp
 */

#include <stddef.h>
#include "pkcs7Templates.h"
#include "keyTemplates.h"	/* NSS_AlgorithmIDTemplate */
#include "SecAsn1Templates.h"
#include "nssUtils.h"
#include "oidsattr.h"

const SecAsn1Template NSS_P7_DigestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P7_DigestInfo) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_P7_DigestInfo,digestAlgorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSS_P7_DigestInfo,digest) },
    { 0 }
};

/*
 * Uninterpreted ContentInfo, with content stripped from its
 * EXPLICIT CONTEXT_SPECIFIC wrapper
 */
const SecAsn1Template NSS_P7_RawContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P7_RawContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P7_RawContentInfo,contentType) },
	{ SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_EXPLICIT | 
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL | 0,
	  offsetof(NSS_P7_RawContentInfo,content),
	  kSecAsn1AnyTemplate },
    { 0 }
};

/*
 * Individual ContentInfo.content templates
 */
const SecAsn1Template NSS_P7_EncrContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P7_EncrContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P7_EncrContentInfo,contentType) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_P7_EncrContentInfo,encrAlg),
	  kSecAsn1AlgorithmIDTemplate },
	{ SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL | 0,
	  offsetof(NSS_P7_EncrContentInfo,encrContent),
	  kSecAsn1OctetStringTemplate },
    { 0 }
};

const SecAsn1Template NSS_P7_EncryptedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P7_EncryptedData) },
	{ SEC_ASN1_INTEGER,
	  offsetof(NSS_P7_EncryptedData,version) },
	{ SEC_ASN1_INLINE,
	  offsetof(NSS_P7_EncryptedData,contentInfo),
	  NSS_P7_EncrContentInfoTemplate },
   { 0 }
};

const SecAsn1Template NSS_P7_PtrToEncryptedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSS_P7_EncryptedDataTemplate }
};

/*
 * Decoded ContentInfo via SEC_ASN1_DYNAMIC
 */
 
static const SecAsn1Template * NSS_P7_ContentInfoChooser(
	void *arg, 			// --> NSS_P7_DecodedContentInfo
	Boolean enc,
	const char *buf,	// on decode, tag byte and length
	size_t len,
	void *dest)			// --> NSS_P7_DecodedContentInfo.content
{
	NSS_P7_DecodedContentInfo *dci = 
		(NSS_P7_DecodedContentInfo *)arg;
	const SecAsn1Template *templ = NULL;
	NSS_P7_CI_Type type = CT_None;
	
	if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_Data)) {
		templ = kSecAsn1PointerToOctetStringTemplate;
		type = CT_Data;
	}
	else if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_EncryptedData)) {
		templ = NSS_P7_PtrToEncryptedDataTemplate;
		type = CT_EncryptedData;
	}
	else if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_SignedData)) {
		templ = NSS_P7_PtrToSignedDataTemplate;
		type = CT_SignedData;
	}
	else if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_EnvelopedData)) {
		templ = NSS_P7_PtrToEnvelDataTemplate;
		type = CT_EnvData;
	}
	else if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_SignedAndEnvelopedData)) {
		templ = NSS_P7_PtrToSignEnvelDataTemplate;
		type = CT_SignedEnvData;
	}
	else if(nssCompareSecAsn1Items(&dci->contentType,
			&CSSMOID_PKCS7_DigestedData)) {
		templ = NSS_P7_PtrToDigestedDataTemplate;
		type = CT_DigestData;
	}
	/* add more here when we implement them */
	else {
		return kSecAsn1PointerToAnyTemplate;
	}
	if(!enc) {
		dci->type = type;
	}
	return templ;
}

static const SecAsn1TemplateChooserPtr NSS_P7_ContentInfoChooserPtr = 
	NSS_P7_ContentInfoChooser;

const SecAsn1Template NSS_P7_DecodedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P7_DecodedContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P7_DecodedContentInfo,contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_DYNAMIC | 
			SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 
			SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSS_P7_DecodedContentInfo,content),
	  &NSS_P7_ContentInfoChooserPtr },
   { 0 }
};
