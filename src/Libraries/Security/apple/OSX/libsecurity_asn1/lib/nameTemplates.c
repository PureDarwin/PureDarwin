/*
 * Copyright (c) 2003-2006,2008,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * nameTemplates.c - ASN1 templates for X509 Name, GeneralName, etc.
 */

#include "SecAsn1Templates.h"
#include "nameTemplates.h"
#include "X509Templates.h"
#include "keyTemplates.h"
#include <stddef.h>
#include <security_utilities/simulatecrash_assert.h>

typedef struct {
    SecAsn1Oid typeId;
    SecAsn1Item value; // unparsed, BER-encoded
} CE_OtherName;

// MARK: ----- Generalized NSS_TaggedItem template chooser support -----

/*
 * Generalized Template chooser.
 */
const SecAsn1Template * SecAsn1TaggedTemplateChooser(
	/* Five args passed to specific SecAsn1TemplateChooser */
	void *arg, 				// currently not used
	Boolean enc,			
	const char *buf,
	size_t len,
	void *dest,
	/* array of tag/template pairs */
	const NSS_TagChoice *chooser)
{
	unsigned char tag = 0;
	const SecAsn1Template *templ = NULL;
	NSS_TaggedItem *item = (NSS_TaggedItem *)dest;
	
	assert(item != NULL);
	assert((chooser != NULL) && (chooser->templ != NULL));

	if(enc) {
		/* encoding: tag from an NSS_TaggedItem at *dest */
		tag = item->tag;
	}
	else if (len > 0) {
		/* decoding: tag from raw bytes being decoded */
		tag = buf[0] & SEC_ASN1_TAGNUM_MASK;
		/* and tell caller what's coming */
		item->tag = tag;
	}
	/*
	 * If buffer length is 0, leave tag = 0. No choice will have this
	 * the invalid tag.
	 */
	
	/* infer template from tag */
	const NSS_TagChoice *thisChoice;
	for(thisChoice=chooser; thisChoice->templ != NULL; thisChoice++) {
		if(tag == thisChoice->tag) {
			templ = thisChoice->templ;
			break;
		}
	}
	if(templ == NULL) {
		/* 
		 * Tag not found. On decoding, this is the caller's fault
		 * and they'll have to deal with it. 
		 * On decode, pick a template guaranteed to cause a decoding
		 * failure - the template from the first array of 
		 * NSS_TagChoices should do the trick since its tag didn't match. 
		 */
		templ = chooser[0].templ;
	}
	return templ;
}

// MARK: ----- X509 Name, RDN ------

/* AttributeTypeAndValue */

/*
 * NSS_ATV Template chooser.
 */
static const NSS_TagChoice atvChoices[] = {
	{ SEC_ASN1_PRINTABLE_STRING, kSecAsn1PrintableStringTemplate} ,
	{ SEC_ASN1_TELETEX_STRING, kSecAsn1TeletexStringTemplate },
	{ SEC_ASN1_UNIVERSAL_STRING, kSecAsn1UniversalStringTemplate },
	{ SEC_ASN1_UTF8_STRING, kSecAsn1UTF8StringTemplate },
	{ SEC_ASN1_BMP_STRING, kSecAsn1BMPStringTemplate },
	{ SEC_ASN1_IA5_STRING, kSecAsn1IA5StringTemplate },
	{ 0, NULL}
};

static const SecAsn1Template * NSS_ATVChooser(
	void *arg, 
	Boolean enc,
	const char *buf,
	size_t len,
	void *dest)
{
	return SecAsn1TaggedTemplateChooser(arg, enc, buf, len, dest, atvChoices);
}

static const SecAsn1TemplateChooserPtr NSS_ATVChooserPtr = NSS_ATVChooser;

const SecAsn1Template kSecAsn1ATVTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_ATV) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_ATV,type), },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_ATV,value),
	  &NSS_ATVChooserPtr },
    { 0, }
};

/* RelativeDistinguishedName */
const SecAsn1Template kSecAsn1RDNTemplate[] = {
    { SEC_ASN1_SET_OF,
	  offsetof(NSS_RDN,atvs), kSecAsn1ATVTemplate, sizeof(NSS_RDN) }
};

/* X509 Name */
const SecAsn1Template kSecAsn1NameTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(NSS_Name,rdns), kSecAsn1RDNTemplate, sizeof(NSS_Name) }
};

// MARK: ----- OtherName, GeneralizedName -----

/*
 * CE_OtherName.value expressed as ASN_ANY, not en/decoded.
 */
const SecAsn1Template NSS_OtherNameTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CE_OtherName) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CE_OtherName,typeId), },
    { SEC_ASN1_ANY,
	  offsetof(CE_OtherName,value), },
    { 0, }
};

/* 
 * For decoding an OtherName when it's a context-specific CHOICE
 * of a GeneralName.
 */
const SecAsn1Template kSecAsn1GenNameOtherNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_OtherName,
	  0, NSS_OtherNameTemplate, sizeof(CE_OtherName) }
};

/* 
 * NSS_GeneralName template chooser.
 * First, a crufty set of templates specific to this context.
 * All offsets are zero (the fundamental type is a NSS_TaggedItem).
 *
 * NOTE WELL: RFC2459 says that all of the choices within a 
 * GeneralName (which these templates implement) have implicit
 * context-specific tags. 
 * HOWEVER: RFC2538 and the real world indicate that the directoryName
 * choice is EXPLICITLY tagged. This causes an extra layer of DER - 
 * the "thing" is wrapped in a header consisting of the tag byte 
 * (SEC_ASN1_CONTEXT_SPECIFIC plus context tag plus SEC_ASN1_CONSTRUCTED)
 * and the length field. 
 *
 * To actually implement this in the current pile-of-cruft context,
 * the directoryName and otherName choices are processed here with 
 * NSS_InnerAnyTemplate which strips off the explicit tag layer, leaving
 * further processing to the app. 
 * 
 * I sure hope we don't find certs that actually conform to RFC2459 on 
 * this. We might have to handle both. Be forewarned.
 */
 
/* inner contents of an ASN_ANY */

#define NSS_GEN_NAME_OFFSET	(offsetof(NSS_GeneralName,item))
#define NSS_GEN_NAME_SIZE	(sizeof(NSS_GeneralName))

const SecAsn1Template kSecAsn1OtherNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | NGT_OtherName,
      NSS_GEN_NAME_OFFSET, kSecAsn1AnyTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1RFC822NameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_RFC822Name,
      NSS_GEN_NAME_OFFSET, kSecAsn1IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1DNSNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_DNSName,
      NSS_GEN_NAME_OFFSET, kSecAsn1IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1X400AddressTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC |  SEC_ASN1_CONSTRUCTED | NGT_X400Address,
      NSS_GEN_NAME_OFFSET, kSecAsn1AnyTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1DirectoryNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_EXPLICIT | NGT_DirectoryName,
      NSS_GEN_NAME_OFFSET, kSecAsn1AnyTemplate, NSS_GEN_NAME_SIZE }
	};
const SecAsn1Template kSecAsn1EdiPartyNameTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC |  SEC_ASN1_CONSTRUCTED | NGT_EdiPartyName,
      NSS_GEN_NAME_OFFSET, kSecAsn1AnyTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1URITemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_URI,
      NSS_GEN_NAME_OFFSET, kSecAsn1IA5StringTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1IPAddressTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_IPAddress,
      NSS_GEN_NAME_OFFSET, kSecAsn1OctetStringTemplate, NSS_GEN_NAME_SIZE }
};
const SecAsn1Template kSecAsn1RegisteredIDTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | NGT_RegisteredID,
      NSS_GEN_NAME_OFFSET, kSecAsn1ObjectIDTemplate, NSS_GEN_NAME_SIZE }
};

static const NSS_TagChoice genNameChoices[] = {
	{ NGT_OtherName, kSecAsn1OtherNameTemplate} ,
	{ NGT_RFC822Name, kSecAsn1RFC822NameTemplate },
	{ NGT_DNSName, kSecAsn1DNSNameTemplate },
	{ NGT_X400Address, kSecAsn1X400AddressTemplate },
	{ NGT_DirectoryName, kSecAsn1DirectoryNameTemplate },
	{ NGT_EdiPartyName, kSecAsn1EdiPartyNameTemplate },
	{ NGT_URI, kSecAsn1URITemplate },
	{ NGT_IPAddress, kSecAsn1IPAddressTemplate },
	{ NGT_RegisteredID, kSecAsn1RegisteredIDTemplate },
	{ 0, NULL}
};

static const SecAsn1Template * NSS_genNameChooser(  
	void *arg, 
	Boolean enc,
	const char *buf,
	size_t len,
	void *dest) 
{
	return SecAsn1TaggedTemplateChooser(arg, enc, buf, len, dest, genNameChoices);
}

static const SecAsn1TemplateChooserPtr NSS_genNameChooserPtr =
	NSS_genNameChooser;

const SecAsn1Template kSecAsn1GeneralNameTemplate[] = {
    { SEC_ASN1_DYNAMIC | SEC_ASN1_CONTEXT_SPECIFIC,
	  offsetof(NSS_GeneralName,item),		// Needed?
	  &NSS_genNameChooserPtr },
    { 0, }									// Needed?
};									
