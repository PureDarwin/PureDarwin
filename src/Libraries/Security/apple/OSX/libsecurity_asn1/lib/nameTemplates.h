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
 * nameTemplates.h - ASN1 templates for X509 Name, GeneralName, etc.
 */

#ifndef	_NSS_NAME_TEMPLATES_H_
#define _NSS_NAME_TEMPLATES_H_

#include <Security/SecAsn1Types.h>

#ifdef  __cplusplus
extern "C" {
#endif

// MARK: ----- Generalized NSS_TaggedItem template chooser support -----

/*
 * A tagged item for use with simple CHOICE types implemented
 * via SEC_ASN1_DYNAMIC.
 */
typedef struct {
	SecAsn1Item		item;
	unsigned char	tag;
} NSS_TaggedItem;

/*
 * Type/template-specific SecAsn1TemplateChooser passes
 * an array of these, terminated by a NSS_TagChoice with 
 * a NULL templ field, to SecAsn1TaggedTemplateChooser().
 */
typedef struct {
	unsigned char 			tag;
	const SecAsn1Template	*templ;
} NSS_TagChoice;

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
	const NSS_TagChoice *chooser);

// MARK: ----- X509 Name, RDN ------

/* 
 * ASN class : AttributeTypeAndValue
 * C struct  : NSS_ATV
 * Roughly corresponds to a CSSM_X509_TYPE_VALUE_PAIR and used 
 * in DirectoryString elements.
 */

/*
 * This type and template process, via SEC_ASN1_DYNAMIC. the following
 * tagged types:
 *
 *	SEC_ASN1_PRINTABLE_STRING
 *	SEC_ASN1_TELETEX_STRING
 *	SEC_ASN1_UNIVERSAL_STRING
 *	SEC_ASN1_UTF8_STRING
 *	SEC_ASN1_BMP_STRING
 *  SEC_ASN1_IA5_STRING
 *
 * Note that SEC_ASN1_IA5_STRING is not a legal part of a 
 * DirectoryString, but some certs (e.g. the Thawte serverbasic cert)
 * use this type.
 */
typedef struct {
    SecAsn1Oid 		type;
    NSS_TaggedItem 	value;	
} NSS_ATV;

/*
 * ASN class : RelativeDistinguishedName
 * C struct  : NSS_RDN
 *
 * Corresponds to CSSM_X509_RDN. 
 */
typedef struct  {
    NSS_ATV 	**atvs;
} NSS_RDN;

/*
 * ASN class : Name
 * C struct  : NSS_Name
 *
 * Corresponds to CSSM_X509_NAME.
 */
typedef struct {
    NSS_RDN 	**rdns;
} NSS_Name;

extern const SecAsn1Template kSecAsn1ATVTemplate[];
extern const SecAsn1Template kSecAsn1RDNTemplate[];
extern const SecAsn1Template kSecAsn1NameTemplate[];

// MARK: ----- OtherName, GeneralizedName -----

/* 
 * ASN Class : OtherName
 * C struct  : CE_OtherName
 *
 * CE_OtherName.value expressed as ASN_ANY, not en/decoded.
 */
extern const SecAsn1Template kSecAsn1OtherNameTemplate[];

/* 
 * For decoding an OtherName when it's a context-specific CHOICE
 * of a GeneralName.
 */
extern const SecAsn1Template kSecAsn1GenNameOtherNameTemplate[];

/*
 * ASN Class : GeneralName
 * C struct  : NSS_GeneralName, typedefd to an NSS_TaggedItem
 *
 * This roughly maps to a CE_GeneralName (from certextensions.h). 
 * The NSS_TaggedItem mechanism is used to resolve choices down
 * to the SecAsn1Item level - i.e., at this level (prior to encoding
 * or after decoding), NSS_GeneralName.item either contains a simple
 * atomic type (IA5String, Octet string) or is raw, un{de,en}coded
 * ASN_ANY.
 */
typedef NSS_TaggedItem NSS_GeneralName;

/*
 * These context-specific tag definitions, for use in 
 * NSS_GeneralName.tag, are from the ASN definition and map to
 * CE_GeneralNameType values from certextensions.h. The values
 * happen to be equivalent but apps should not count on that -
 * these NSS_GeneralNameTag values are explicitly assigned per
 * the ASN spec of a GeneralName.
 *
 * Shown with each tag is the simple type the tag maps to.
 */
typedef enum {
	NGT_OtherName = 0,			// ASN_ANY
	NGT_RFC822Name = 1,			// IA5String
	NGT_DNSName = 2,			// IA5String
	NGT_X400Address = 3,		// ASY_ANY
	NGT_DirectoryName = 4,		// ASN_ANY
	NGT_EdiPartyName = 5,		// ASN_ANY
	NGT_URI = 6,				// IA5String
	NGT_IPAddress = 7,			// OCTET_STRING
	NGT_RegisteredID = 8		// OID
} NSS_GeneralNameTag;

extern const SecAsn1Template kSecAsn1GeneralNameTemplate[];

/*
 * ASN Class : GeneralNames
 * C struct  : NSS_GeneralNames
 *
 * Since the SEC_ANY_DYNAMIC mechanism doesn't work with POINTERs
 * or GROUPs (e.g., a sequence of NSS_GeneralName elements), decoding
 * an NSS_GeneralNames first requires a decode to an array of 
 * ANY_ANY blobs as shown here. Use SEC_SequenceOfAnyTemplate for
 * that step. Each of the resulting elements is individually 
 * decoded into an NSS_GeneralName.
 */
typedef struct {
	SecAsn1Item **names;		/* sequence */
} NSS_GeneralNames;

#define kSecAsn1GeneralNamesTemplate kSecAsn1SequenceOfAnyTemplate 

#ifdef  __cplusplus
}
#endif

#endif	/* _NSS_NAME_TEMPLATES_H_ */
