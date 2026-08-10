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
 * X509Templates.c - Common ASN1 templates for use with libNSSDer.
 */

#include "SecAsn1Templates.h"
#include "X509Templates.h"
#include "keyTemplates.h"
#include <stddef.h>

/* 
 * Validity
 */
/*
 * NSS_Time Template chooser.
 */
static const NSS_TagChoice timeChoices[] = {
	{ SEC_ASN1_GENERALIZED_TIME, kSecAsn1GeneralizedTimeTemplate} ,
	{ SEC_ASN1_UTC_TIME, kSecAsn1UTCTimeTemplate },
	{ 0, NULL}
};

static const SecAsn1Template * NSS_TimeChooser(
	void *arg, 
	Boolean enc,
	const char *buf,
	size_t len,
	void *dest)
{
	return SecAsn1TaggedTemplateChooser(arg, enc, buf, len, dest, timeChoices);
}

static const SecAsn1TemplateChooserPtr NSS_TimeChooserPtr = NSS_TimeChooser;

const SecAsn1Template kSecAsn1ValidityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Validity) },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_Validity,notBefore.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_Validity,notAfter.item),
	  &NSS_TimeChooserPtr },
    { 0 }
};

/* X509 cert extension */
const SecAsn1Template kSecAsn1CertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_CertExtension) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSS_CertExtension,extnId) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,		/* XXX DER_DEFAULT */
	  offsetof(NSS_CertExtension,critical) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSS_CertExtension,value) },
    { 0, }
};

const SecAsn1Template kSecAsn1SequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1CertExtensionTemplate }
};

/* TBS Cert */
const SecAsn1Template kSecAsn1TBSCertificateTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_TBSCertificate) },
	/* optional version, explicit tag 0, default 0 */
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 		/* XXX DER_DEFAULT */ 
	  offsetof(NSS_TBSCertificate,version),
	  kSecAsn1IntegerTemplate },
	/* serial number is SIGNED integer */
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(NSS_TBSCertificate,serialNumber) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,signature),
	  kSecAsn1AlgorithmIDTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCertificate,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,issuer),
	  kSecAsn1NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,validity),
	  kSecAsn1ValidityTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCertificate,derSubject) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,subject),
	  kSecAsn1NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCertificate,subjectPublicKeyInfo),
	  kSecAsn1SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSS_TBSCertificate,issuerID),
	  kSecAsn1BitStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSS_TBSCertificate,subjectID),
	  kSecAsn1BitStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 3,
	  offsetof(NSS_TBSCertificate,extensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * For signing and verifying only, treating the TBS portion as an
 * opaque ASN_ANY blob.
 */
const SecAsn1Template kSecAsn1SignedCertOrCRLTemplate[] =
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_SignedCertOrCRL) },
    { SEC_ASN1_ANY, 
	  offsetof(NSS_SignedCertOrCRL,tbsBlob) },
    { SEC_ASN1_ANY,
	  offsetof(NSS_SignedCertOrCRL,signatureAlgorithm) },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_SignedCertOrCRL,signature) },
    { 0 }
};

/* Fully specified signed certificate */
const SecAsn1Template kSecAsn1SignedCertTemplate[] = 
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Certificate) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_Certificate,tbs),
	  kSecAsn1TBSCertificateTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_Certificate,signatureAlgorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_Certificate,signature) },
    { 0 }
};

/* Entry in CRL.revokedCerts */
const SecAsn1Template kSecAsn1RevokedCertTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_RevokedCert) },
	  /* serial number - signed itneger, just like in the actual cert */
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(NSS_RevokedCert,userCertificate) },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_RevokedCert,revocationDate.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_RevokedCert,extensions),
	  kSecAsn1CertExtensionTemplate },
    { 0, }
};

const SecAsn1Template kSecAsn1SequenceOfRevokedCertTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1RevokedCertTemplate }
};

/* NSS_TBSCrl (unsigned CRL) */
const SecAsn1Template kSecAsn1TBSCrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(NSS_TBSCrl) },
	/* optional version, default 0 */
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof (NSS_TBSCrl, version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCrl,signature),
	  kSecAsn1AlgorithmIDTemplate },
	{ SEC_ASN1_SAVE, offsetof(NSS_TBSCrl,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_TBSCrl,issuer),
	  kSecAsn1NameTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC,
	  offsetof(NSS_TBSCrl,thisUpdate.item),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_INLINE | SEC_ASN1_DYNAMIC | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_TBSCrl,nextUpdate),
	  &NSS_TimeChooserPtr },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_TBSCrl,revokedCerts),
	  kSecAsn1RevokedCertTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_TBSCrl,extensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0, }
};

/* Fully specified signed CRL */
const SecAsn1Template kSecAsn1SignedCrlTemplate[] = 
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_Crl) },
    { SEC_ASN1_INLINE, 
	  offsetof(NSS_Crl,tbs),
	  kSecAsn1TBSCrlTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_Crl,signatureAlgorithm),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(NSS_Crl,signature) },
    { 0 }
};
