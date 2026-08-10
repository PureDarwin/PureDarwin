/*
 * Copyright (c) 2003-2006,2008,2010-2012,2020 Apple Inc. All Rights Reserved.
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
 * certExtensionTemplates.cpp - libnssasn1 structs and templates for cert and 
 *                              CRL extensions
 *
 */
 
#include "certExtensionTemplates.h"
#include "SecAsn1Templates.h"
#include <stddef.h>

/* Basic Constraints */
const SecAsn1Template kSecAsn1BasicConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_BasicConstraints) },
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_BasicConstraints,cA) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_BasicConstraints, pathLenConstraint) },
    { 0, }
};

/* Authority Key Identifier */

/* signed integer - SEC_ASN1_SIGNED_INT state gets lost
 * in SEC_ASN1_CONTEXT_SPECIFIC processing */
const SecAsn1Template kSecAsn1SignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1AuthorityKeyIdTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_AuthorityKeyId) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
	  SEC_ASN1_POINTER | 0, 
	  offsetof(NSS_AuthorityKeyId,keyIdentifier),
	  kSecAsn1OctetStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	                      SEC_ASN1_CONTEXT_SPECIFIC | 1, 
	  offsetof(NSS_AuthorityKeyId,genNames), 
	  kSecAsn1GeneralNamesTemplate },
	/* serial number is SIGNED integer */
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSS_AuthorityKeyId,serialNumber), 
	  kSecAsn1SignedIntegerTemplate},
	{ 0 }
};

/* Certificate policies */
const SecAsn1Template kSecAsn1PolicyQualifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_PolicyQualifierInfo) },
    { SEC_ASN1_OBJECT_ID,  
	  offsetof(NSS_PolicyQualifierInfo,policyQualifierId) },
	{ SEC_ASN1_ANY, offsetof(NSS_PolicyQualifierInfo, qualifier) },
	{ 0 }
};

const SecAsn1Template kSecAsn1PolicyInformationTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_PolicyInformation) },
    { SEC_ASN1_OBJECT_ID,  
	  offsetof(NSS_PolicyInformation,certPolicyId) },
	{ SEC_ASN1_SEQUENCE_OF | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_PolicyInformation,policyQualifiers),
	  kSecAsn1PolicyQualifierTemplate },
	{ 0 }
};

const SecAsn1Template kSecAsn1CertPoliciesTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_CertPolicies,policies),
	  kSecAsn1PolicyInformationTemplate },
	{ 0 }
};

/* CRL Distribution Points */

/*
 * NOTE WELL: RFC2459, and all the documentation I can find, claims that
 * the tag for the DistributionPointName option (tag 0) of a 
 * DistributionPoint is IMPLICIT and context-specific. However this
 * is IMPOSSIBLE - since the underlying type (DistributionPointName)
 * also relies upon context-specific tags to resolve a CHOICE. 
 * The real world indicates that the tag for the DistributionPoint option
 * is indeed EXPLICIT. Examination of many certs' cRLDistributionPoints
 * extensions shows this, and the NSS reference code also specifies
 * an EXPLICIT tag for this field. 
 */
const SecAsn1Template kSecAsn1DistributionPointTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_DistributionPoint) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_DistributionPoint,distPointName), 
	  kSecAsn1PointerToAnyTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSS_DistributionPoint,reasons), kSecAsn1BitStringTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
						  SEC_ASN1_CONSTRUCTED | 2,
	  offsetof(NSS_DistributionPoint, crlIssuer), 
	  kSecAsn1GeneralNamesTemplate
	},
	{ 0 }
};

const SecAsn1Template kSecAsn1CRLDistributionPointsTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_CRLDistributionPoints,distPoints),
	  kSecAsn1DistributionPointTemplate },
	{ 0 }
};


/*
 * These are the context-specific targets of the DistributionPointName
 * option.
 */
const SecAsn1Template kSecAsn1DistPointFullNameTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 0,
	offsetof (NSS_GeneralNames,names), kSecAsn1GeneralNamesTemplate}
};

const SecAsn1Template kSecAsn1DistPointRDNTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 1, 
	offsetof (NSS_RDN,atvs), kSecAsn1RDNTemplate}
};
	 
/*
 * Issuing distribution points
 *
 * Although the spec says that the DistributionPointName element
 * is context-specific, it must be explicit because the underlying
 * type - a DistributionPointName - also relies on a context-specific
 * tags to resolve a CHOICE.
 */

/* kludge: ASN decoder doesn't handle 
 * SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER
 * very well... */
static const SecAsn1Template kSecAsn1OptBooleanTemplate[] = {
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL, 0, NULL, sizeof(SecAsn1Item) }
};

static const SecAsn1Template kSecAsn1OptBitStringTemplate[] = {
    { SEC_ASN1_BIT_STRING | SEC_ASN1_OPTIONAL, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1IssuingDistributionPointTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_IssuingDistributionPoint) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_IssuingDistributionPoint,distPointName), 
	  kSecAsn1PointerToAnyTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 1,
	  offsetof(NSS_IssuingDistributionPoint,onlyUserCerts), 
	  kSecAsn1OptBooleanTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 2,
	  offsetof(NSS_IssuingDistributionPoint,onlyCACerts), 
	  kSecAsn1OptBooleanTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 3,
	  offsetof(NSS_IssuingDistributionPoint,onlySomeReasons), 
	  kSecAsn1OptBitStringTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 4,
	  offsetof(NSS_IssuingDistributionPoint,indirectCRL), 
	  kSecAsn1OptBooleanTemplate},
	{ 0 }
};


/* 
 * Authority Information Access and Subject Information Access.
 */
const SecAsn1Template kSecAsn1AccessDescriptionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_AccessDescription) },
    { SEC_ASN1_OBJECT_ID,  
	  offsetof(NSS_AccessDescription,accessMethod) },
	/* 
	 * NSS encoder just can't handle direct inline of an NSS_GeneralName here.
	 */
    { SEC_ASN1_ANY,
      offsetof(NSS_AccessDescription, encodedAccessLocation) },
	{ 0 }
};

const SecAsn1Template kSecAsn1AuthorityInfoAccessTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(NSS_AuthorityInfoAccess,accessDescriptions), 
	  kSecAsn1AccessDescriptionTemplate, 
	  sizeof(NSS_AuthorityInfoAccess) }
};

/*
 * Qualified Certificate Statements templates.
 *
 * This is the NSS_QC_Statement.info when NSS_QC_Statement.statementId
 * is CSSMOID_OID_QCS_SYNTAX_V2.
 */
const SecAsn1Template kSecAsn1SemanticsInformationTemplate[] = {
    { SEC_ASN1_SEQUENCE,
		0, NULL, sizeof(NSS_SemanticsInformation) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER,
		offsetof(NSS_SemanticsInformation,semanticsIdentifier), 
		kSecAsn1ObjectIDTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER,
		offsetof(NSS_SemanticsInformation, nameRegistrationAuthorities), 
		kSecAsn1GeneralNamesTemplate },
	{ 0 }
};
		
const SecAsn1Template kSecAsn1QC_StatementTemplate[] = {
    { SEC_ASN1_SEQUENCE,
		0, NULL, sizeof(NSS_QC_Statement) },
    { SEC_ASN1_OBJECT_ID,  
		offsetof(NSS_QC_Statement,statementId) },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL,
		offsetof(NSS_QC_Statement, info) },
	{ 0 }
};

const SecAsn1Template kSecAsn1QC_StatementsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 
		offsetof(NSS_QC_Statements,qcStatements), 
		kSecAsn1QC_StatementTemplate, 
		sizeof(NSS_QC_Statements) }
};

/*
 * NameConstraints templates
 */
const SecAsn1Template kSecAsn1GeneralSubtreeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_GeneralSubtree) },
	{ SEC_ASN1_SEQUENCE,
	  offsetof(NSS_GeneralSubtree,base), 
	  kSecAsn1GeneralNamesTemplate },	
	{ SEC_ASN1_INTEGER,
	  offsetof(NSS_GeneralSubtree,minimum) },
	{ SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_GeneralSubtree,maximum) },
    { 0, }
};

const SecAsn1Template kSecAsn1GeneralSubtreesTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_GeneralSubtrees,subtrees),
	  kSecAsn1GeneralSubtreeTemplate },
	{ 0 }
};

const SecAsn1Template kSecAsn1NameConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_NameConstraints) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_NameConstraints,permittedSubtrees), 
	  kSecAsn1GeneralSubtreesTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 1,
	  offsetof(NSS_NameConstraints,excludedSubtrees), 
	  kSecAsn1GeneralSubtreesTemplate },
	{ 0 }
};

/*
 * PolicyMappings templates
 */
const SecAsn1Template kSecAsn1PolicyMappingTemplate[] = {
    { SEC_ASN1_SEQUENCE,
		0, NULL, sizeof(NSS_PolicyMapping) },
    { SEC_ASN1_OBJECT_ID,  
		offsetof(NSS_PolicyMapping,issuerDomainPolicy) },
    { SEC_ASN1_OBJECT_ID,  
		offsetof(NSS_PolicyMapping,subjectDomainPolicy) },
	{ 0 }
};

const SecAsn1Template kSecAsn1PolicyMappingsTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_PolicyMappings,policyMappings),
	  kSecAsn1PolicyMappingTemplate },
	{ 0 }
};

/*
 * PolicyConstraints templates
 */
const SecAsn1Template kSecAsn1PolicyConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_PolicyConstraints) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSS_PolicyConstraints,requireExplicitPolicy),
	  kSecAsn1IntegerTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSS_PolicyConstraints,inhibitPolicyMapping),
	  kSecAsn1IntegerTemplate },
	{ 0 }
};


