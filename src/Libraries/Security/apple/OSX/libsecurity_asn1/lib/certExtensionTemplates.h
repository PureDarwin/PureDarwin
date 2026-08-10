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
 * certExtensionTemplates.h - libnssasn1 structs and templates for cert and 
 *                            CRL extensions
 *
 */
 
#ifndef	_CERT_EXTENSION_TEMPLATES_H_
#define _CERT_EXTENSION_TEMPLATES_H_

#include <Security/X509Templates.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Basic Constraints
 * NSS struct  : NSS_BasicConstraints
 * CDSA struct : CE_BasicConstraints
 */
typedef struct {
	SecAsn1Item		cA;					// BOOL
	SecAsn1Item		pathLenConstraint;	// INTEGER optional
} NSS_BasicConstraints;

extern const SecAsn1Template kSecAsn1BasicConstraintsTemplate[];

/* 
 * Key Usage
 * NSS struct  : SecAsn1Item, BIT STRING - length in bits
 * CDSA struct : CE_KeyUsage
 */
#define kSecAsn1KeyUsageTemplate		kSecAsn1BitStringTemplate

/*
 * Extended Key Usage
 * NSS struct  : NSS_ExtKeyUsage
 * CDSA struct : CE_ExtendedKeyUsage
 */
typedef struct {
	SecAsn1Oid	**purposes;
} NSS_ExtKeyUsage;
#define kSecAsn1ExtKeyUsageTemplate		kSecAsn1SequenceOfObjectIDTemplate

/*
 * Subject Key Identifier
 * NSS struct  : SecAsn1Item
 * CDSA struct : CE_SubjectKeyID, typedef'd to a SecAsn1Item
 */
#define kSecAsn1SubjectKeyIdTemplate	kSecAsn1OctetStringTemplate

/*
 * Authority Key Identifier
 * NSS struct  : NSS_AuthorityKeyId
 * CDSA struct : CE_AuthorityKeyID
 *
 * All fields are optional.
 * NOTE: due to an anomaly in the encoding module, if the first field
 * of a sequence is optional, it has to be a POINTER type. 
 */
typedef struct {
	SecAsn1Item			*keyIdentifier;		// octet string
	NSS_GeneralNames	genNames;	
	SecAsn1Item			serialNumber;		// integer
} NSS_AuthorityKeyId;

extern const SecAsn1Template kSecAsn1AuthorityKeyIdTemplate[];

/*
 * Certificate policies. 
 * NSS struct  : NSS_CertPolicies
 * CDSA struct : CE_CertPolicies
 */
typedef struct {
	SecAsn1Oid		policyQualifierId;	// CSSMOID_QT_CPS, CSSMOID_QT_UNOTICE
	SecAsn1Item		qualifier;			// ASN_ANY, not interpreted here
} NSS_PolicyQualifierInfo;

extern const SecAsn1Template kSecAsn1PolicyQualifierTemplate[];

typedef struct {
	SecAsn1Oid				certPolicyId;
	NSS_PolicyQualifierInfo	**policyQualifiers;	// SEQUENCE OF
} NSS_PolicyInformation;

extern const SecAsn1Template kSecAsn1PolicyInformationTemplate[];

typedef struct {
	NSS_PolicyInformation	**policies;			// SEQUENCE OF
} NSS_CertPolicies;

extern const SecAsn1Template kSecAsn1CertPoliciesTemplate[];

/* 
 * netscape-cert-type
 * NSS struct  : SecAsn1Item, BIT STRING - length in bits
 * CDSA struct : CE_NetscapeCertType (a uint16)
 */
#define kSecAsn1NetscapeCertTypeTemplate		kSecAsn1BitStringTemplate

/*
 * CRL Distribution Points. 
 * NSS struct  : NSS_DistributionPoint, NSS_DistributionPoints
 * CDSA struct : CE_CRLDistributionPoint, CE_CRLDistributionPointSyntax
 */

typedef struct {
	SecAsn1Item			*distPointName;		// ASN_ANY, optional
	SecAsn1Item			reasons;			// BIT_STRING, optional
	NSS_GeneralNames	crlIssuer;			// optional
} NSS_DistributionPoint;

typedef struct {
	NSS_DistributionPoint	**distPoints;	// SEQUENCE OF
} NSS_CRLDistributionPoints;

extern const SecAsn1Template kSecAsn1DistributionPointTemplate[];
extern const SecAsn1Template kSecAsn1CRLDistributionPointsTemplate[];

/*
 * Resolving the NSS_DistributionPoint.distributionPoint option
 * involves inspecting the tag of the ASN_ANY and using one of
 * these templates. One the CDSA side the corresponding struct is
 * a CE_DistributionPointName.
 *
 * This one resolves to an NSS_GeneralNames:
 */
#define NSS_DIST_POINT_FULL_NAME_TAG	0
extern const SecAsn1Template kSecAsn1DistPointFullNameTemplate[];
 
/*
 * This one resolves to an NSS_RDN.
 */
#define NSS_DIST_POINT_RDN_TAG			1
extern const SecAsn1Template kSecAsn1DistPointRDNTemplate[];

/*
 * Issuing distribution point.
 *
 * NSS Struct  : NSS_IssuingDistributionPoint
 * CDSA struct : CE_IssuingDistributionPoint
 *
 * All fields optional; default for ASN_BOOLs is false.
 */
typedef struct {
	/* manually decode to a CE_DistributionPointName */
	SecAsn1Item			*distPointName;		// ASN_ANY, optional
	
	SecAsn1Item			*onlyUserCerts;		// ASN_BOOL
	SecAsn1Item			*onlyCACerts;		// ASN_BOOL
	SecAsn1Item			*onlySomeReasons;	// BIT STRING
	SecAsn1Item			*indirectCRL;		// ASN_BOOL
} NSS_IssuingDistributionPoint;

extern const SecAsn1Template kSecAsn1IssuingDistributionPointTemplate[];

/* 
 * Authority Information Access, Subject Information Access.
 *
 * NSS Struct  : NSS_AuthorityInfoAccess
 * CDSA struct : CE_AuthorityInfoAccess
 */
typedef struct {
	SecAsn1Item				accessMethod;
	
	/* NSS encoder just can't handle direct inline of an NSS_GeneralName here.
	 * After decode and prior to encode this is an encoded GeneralName. 
	 */
	SecAsn1Item				encodedAccessLocation;
} NSS_AccessDescription;

typedef struct {
	NSS_AccessDescription	**accessDescriptions;
} NSS_AuthorityInfoAccess;

extern const SecAsn1Template kSecAsn1AccessDescriptionTemplate[];
extern const SecAsn1Template kSecAsn1AuthorityInfoAccessTemplate[];

/*
 * Qualified Certificate Statements support
 */
typedef struct {
	SecAsn1Oid				*semanticsIdentifier;			/* optional */
	NSS_GeneralNames		*nameRegistrationAuthorities;	/* optional */
} NSS_SemanticsInformation;

typedef struct {
	SecAsn1Oid				statementId;
	SecAsn1Item				info;		/* optional, ANY */
} NSS_QC_Statement;

typedef struct {
	NSS_QC_Statement		**qcStatements;
} NSS_QC_Statements; 

extern const SecAsn1Template kSecAsn1SemanticsInformationTemplate[];
extern const SecAsn1Template kSecAsn1QC_StatementTemplate[];
extern const SecAsn1Template kSecAsn1QC_StatementsTemplate[];

/*
 * NameConstraints support
 */
typedef struct {
	NSS_GeneralNames		base;
	SecAsn1Item				minimum;	// INTEGER default=0
	SecAsn1Item				maximum;	// INTEGER optional
} NSS_GeneralSubtree;

typedef struct {
	NSS_GeneralSubtree		**subtrees; // SEQUENCE OF
} NSS_GeneralSubtrees; 

typedef struct {
	NSS_GeneralSubtrees		*permittedSubtrees; // optional
	NSS_GeneralSubtrees		*excludedSubtrees;  // optional
} NSS_NameConstraints; 

extern const SecAsn1Template kSecAsn1NameConstraintsTemplate[];

/*
 * PolicyMappings support
 */
typedef struct {
	SecAsn1Oid				issuerDomainPolicy;
	SecAsn1Oid				subjectDomainPolicy;
} NSS_PolicyMapping;

typedef struct {
	NSS_PolicyMapping		**policyMappings; // SEQUENCE OF
} NSS_PolicyMappings; 

extern const SecAsn1Template kSecAsn1PolicyMappingsTemplate[];

/*
 * PolicyConstraints support
 */
typedef struct {
	SecAsn1Item				requireExplicitPolicy;	// INTEGER optional
	SecAsn1Item				inhibitPolicyMapping;	// INTEGER optional
} NSS_PolicyConstraints;

extern const SecAsn1Template kSecAsn1PolicyConstraintsTemplate[];

/*
 * InhibitAnyPolicy support
 */
#define kSecAsn1InhibitAnyPolicyTemplate	kSecAsn1IntegerTemplate;

#ifdef	__cplusplus
}
#endif

#endif	/* _CERT_EXTENSION_TEMPLATES_H_ */
