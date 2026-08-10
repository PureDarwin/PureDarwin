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
 * pkcs12Templates.cpp
 */
 
#include <stddef.h>
#include "pkcs12Templates.h"
#include "nssUtils.h"
#include "SecAsn1Templates.h"
#include "oidsattr.h"

const SecAsn1Template NSS_P12_MacDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_MacData) },
	{ SEC_ASN1_INLINE,
	  offsetof(NSS_P12_MacData,mac),
	  NSS_P7_DigestInfoTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSS_P12_MacData,macSalt) },
	/* iterations is unsigned - right? */
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_P12_MacData,iterations) },
    { 0, }
};

const SecAsn1Template pointerToMacDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSS_P12_MacDataTemplate }
};

/* raw PFX with unprocessed authSafe */
const SecAsn1Template NSS_P12_RawPFXTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_RawPFX) },
	{ SEC_ASN1_INTEGER,
	  offsetof(NSS_P12_RawPFX,version) },
	{ SEC_ASN1_INLINE,
	  offsetof(NSS_P12_RawPFX, authSafe),
	  NSS_P7_RawContentInfoTemplate },
	{ SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_P12_RawPFX, macData),
	  NSS_P12_MacDataTemplate },
    { 0, }
};

/* PFX with decoded authSafe */
const SecAsn1Template NSS_P12_DecodedPFXTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_DecodedPFX) },
	{ SEC_ASN1_INTEGER,
	  offsetof(NSS_P12_DecodedPFX,version) },
	{ SEC_ASN1_INLINE,
	  offsetof(NSS_P12_DecodedPFX, authSafe),
	  NSS_P7_DecodedContentInfoTemplate },
	{ SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_P12_DecodedPFX, macData),
	  NSS_P12_MacDataTemplate },
    { 0, }
};

/* AuthenticatedSafe */
const SecAsn1Template NSS_P12_AuthenticatedSafeTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_P12_AuthenticatedSafe, info),
	  NSS_P7_DecodedContentInfoTemplate,
	  sizeof(NSS_P12_AuthenticatedSafe) }
};

/*
 * Individual SafeBag type-specific templates here when we write 'em
 */
const SecAsn1Template NSS_P12_PtrToShroudedKeyBagTemplate[] = {
	{ SEC_ASN1_POINTER, 0, kSecAsn1EncryptedPrivateKeyInfoTemplate }
};

/*
 * CertBag via SEC_ASN1_DYNAMIC
 */
static const SecAsn1Template * NSS_P12_CertBagChooser(
	void *arg, 			// --> NSS_P12_CertBag
	Boolean enc,
	const char *buf,	// on decode, tag byte and length
	size_t len,
	void *dest)			// --> NSS_P12_CertBag.bagValue
{
	NSS_P12_CertBag *bag = (NSS_P12_CertBag *)arg;
	const SecAsn1Template *templ = NULL;
	NSS_P12_CertBagType type = CT_Unknown;
	SecAsn1Oid *oid = &bag->bagType;
	
	if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS9_X509Certificate)) {
		templ = kSecAsn1OctetStringTemplate;
		type = CT_X509;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS9_SdsiCertificate)) {
		templ = kSecAsn1IA5StringTemplate;
		type = CT_SDSI;
	}
	else {
		/* punt */
		templ = kSecAsn1AnyTemplate;
	}
	if(!enc) {
		bag->type = type;
	}
	return templ;
}

static const SecAsn1TemplateChooserPtr NSS_P12_CertBagChooserPtr = 
	NSS_P12_CertBagChooser;

const SecAsn1Template NSS_P12_CertBagTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_CertBag) },
	{ SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P12_CertBag,bagType) },
	  /* these come in with a tag of 0xA0, context/constructed, 
	   * though I don't know why they are flagged as constructed */
	{ SEC_ASN1_DYNAMIC | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 0,
	  offsetof(NSS_P12_CertBag, certValue),
	  &NSS_P12_CertBagChooserPtr },
    { 0, }
};

const SecAsn1Template NSS_P12_PtrToCertBagTemplate[] = {
	{ SEC_ASN1_POINTER, 0, NSS_P12_CertBagTemplate }
};

/*
 * CrlBag via SEC_ASN1_DYNAMIC
 */
static const SecAsn1Template * NSS_P12_CrlBagChooser(
	void *arg, 			// --> NSS_P12_CrlBag
	Boolean enc,
	const char *buf,	// on decode, tag byte and length
	size_t len,
	void *dest)			// --> NSS_P12_CertBag.bagValue
{
	NSS_P12_CrlBag *bag = (NSS_P12_CrlBag *)arg;
	const SecAsn1Template *templ = NULL;
	NSS_P12_CrlBagType type = CRT_Unknown;
	SecAsn1Oid *oid = &bag->bagType;
	
	if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS9_X509Crl)) {
		templ = kSecAsn1OctetStringTemplate;
		type = CRT_X509;
	}
	else {
		/* punt */
		templ = kSecAsn1AnyTemplate;
	}
	if(!enc) {
		bag->type = type;
	}
	return templ;
}

static const SecAsn1TemplateChooserPtr NSS_P12_CrlBagChooserPtr = 
	NSS_P12_CrlBagChooser;

const SecAsn1Template NSS_P12_CrlBagTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_CrlBag) },
	{ SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P12_CrlBag,bagType) },
	  /* these come in with a tag of 0xA0, context/constructed, 
	   * though I don't know why they are flagged as constructed */
	{ SEC_ASN1_DYNAMIC | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 0,
	  offsetof(NSS_P12_CrlBag, crlValue),
	  &NSS_P12_CrlBagChooserPtr },
    { 0, }
};

const SecAsn1Template NSS_P12_PtrToCrlBagTemplate[] = {
	{ SEC_ASN1_POINTER, 0, NSS_P12_CrlBagTemplate }
};


/* the stub templates for unimplemented BagTypes */
#define NSS_P12_PtrToKeyBagTemplate				kSecAsn1PointerToAnyTemplate
#define NSS_P12_PtrToSecretBagTemplate			kSecAsn1PointerToAnyTemplate
#define NSS_P12_PtrToSafeContentsBagTemplate	kSecAsn1PointerToAnyTemplate

 
/*
 * SafeBag via SEC_ASN1_DYNAMIC
 */
static const SecAsn1Template * NSS_P12_SafeBagChooser(
	void *arg, 			// --> NSS_P12_SafeBag
	Boolean enc,
	const char *buf,	// on decode, tag byte and len
	size_t len,
	void *dest)			// --> NSS_P12_SafeBag.bagValue
{
	NSS_P12_SafeBag *bag = (NSS_P12_SafeBag *)arg;
	const SecAsn1Template *templ = NULL;
	NSS_P12_SB_Type type = BT_None;
	SecAsn1Oid *oid = &bag->bagId;
	
	if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_keyBag)) {
		templ = NSS_P12_PtrToKeyBagTemplate;
		type = BT_KeyBag;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_shroudedKeyBag)) {
		templ = NSS_P12_PtrToShroudedKeyBagTemplate;
		type = BT_ShroudedKeyBag;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_certBag)) {
		templ = NSS_P12_PtrToCertBagTemplate;
		type = BT_CertBag;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_crlBag)) {
		templ = NSS_P12_PtrToCrlBagTemplate;
		type = BT_CrlBag;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_secretBag)) {
		templ = NSS_P12_PtrToSecretBagTemplate;
		type = BT_SecretBag;
	}
	else if(nssCompareSecAsn1Items(oid, &CSSMOID_PKCS12_safeContentsBag)) {
		templ = NSS_P12_PtrToSafeContentsBagTemplate;
		type = BT_SafeContentsBag;
	}
	/* add more here when we implement them */
	else {
		templ = kSecAsn1PointerToAnyTemplate;
	}
	if(!enc) {
		bag->type = type;
	}
	return templ;
}

static const SecAsn1TemplateChooserPtr NSS_P12_SafeBagChooserPtr = 
	NSS_P12_SafeBagChooser;

const SecAsn1Template NSS_P12_SafeBagTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_SafeBag) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_P12_SafeBag,bagId) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_CONSTRUCTED |
		SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSS_P12_SafeBag,bagValue),
	  &NSS_P12_SafeBagChooserPtr },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SET_OF,
        offsetof(NSS_P12_SafeBag,bagAttrs),
        kSecAsn1AttributeTemplate },
   { 0 }
};

const SecAsn1Template NSS_P12_SafeContentsTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_P12_SafeContents, bags),
	  NSS_P12_SafeBagTemplate,
	  sizeof(NSS_P12_SafeContents) }
};

const SecAsn1Template NSS_P12_PBE_ParamsTemplate[] = {
	{ SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_P12_PBE_Params) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSS_P12_PBE_Params,salt) },
	/* iterations is unsigned - right? */
	{ SEC_ASN1_INTEGER,
	  offsetof(NSS_P12_PBE_Params,iterations) },
	{ 0 }
};


