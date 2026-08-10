/*
 * DER_CertCrl.h - X.509 certificate structures.
 *
 * A note on the context-specific tags: RFC 5280 marks several fields [n]. Where
 * the field is EXPLICIT the tag is constructed; where it is IMPLICIT over a
 * primitive type it is not. Getting that bit wrong makes the field silently
 * fail to match, so each one is called out in the spec tables.
 */
#ifndef _DER_CERT_CRL_H_
#define _DER_CERT_CRL_H_

#include <libDER/DER_Decode.h>

__BEGIN_DECLS

/* Certificate ::= SEQUENCE {
 *     tbsCertificate      TBSCertificate,
 *     signatureAlgorithm  AlgorithmIdentifier,
 *     signatureValue      BIT STRING }
 *
 * tbs is kept as its full TLV because it is what the signature is computed over.
 */
typedef struct {
    DERItem tbs;
    DERItem sigAlg;
    DERItem sig;
} DERSignedCertCrl;

extern const DERItemSpec DERSignedCertCrlItemSpecs[];
extern const DERShort    DERNumSignedCertCrlItemSpecs;

/* TBSCertificate ::= SEQUENCE {
 *     version         [0] EXPLICIT Version DEFAULT v1,
 *     serialNumber        CertificateSerialNumber,
 *     signature           AlgorithmIdentifier,
 *     issuer              Name,
 *     validity            Validity,
 *     subject             Name,
 *     subjectPublicKeyInfo SubjectPublicKeyInfo,
 *     issuerUniqueID  [1] IMPLICIT UniqueIdentifier OPTIONAL,
 *     subjectUniqueID [2] IMPLICIT UniqueIdentifier OPTIONAL,
 *     extensions      [3] EXPLICIT Extensions OPTIONAL }
 */
typedef struct {
    DERItem version;
    DERItem serialNum;
    DERItem tbsSigAlg;
    DERItem issuer;
    DERItem validity;
    DERItem subject;
    DERItem subjectPubKey;
    DERItem issuerID;
    DERItem subjectID;
    DERItem extensions;
} DERTBSCert;

extern const DERItemSpec DERTBSCertItemSpecs[];
extern const DERShort    DERNumTBSCertItemSpecs;

/* Validity ::= SEQUENCE { notBefore Time, notAfter Time }
 * Time is a CHOICE of UTCTime and GeneralizedTime, so both fields take any tag. */
typedef struct {
    DERItem notBefore;
    DERItem notAfter;
} DERValidity;

extern const DERItemSpec DERValidityItemSpecs[];
extern const DERShort    DERNumValidityItemSpecs;

/* AttributeTypeAndValue ::= SEQUENCE { type AttributeType, value AttributeValue }
 * The value is a DirectoryString CHOICE, so it takes any tag. */
typedef struct {
    DERItem type;
    DERItem value;
} DERAttributeTypeAndValue;

extern const DERItemSpec DERAttributeTypeAndValueItemSpecs[];
extern const DERShort    DERNumAttributeTypeAndValueItemSpecs;

/* Extension ::= SEQUENCE {
 *     extnID     OBJECT IDENTIFIER,
 *     critical   BOOLEAN DEFAULT FALSE,
 *     extnValue  OCTET STRING } */
typedef struct {
    DERItem extnID;
    DERItem critical;
    DERItem extnValue;
} DERExtension;

extern const DERItemSpec DERExtensionItemSpecs[];
extern const DERShort    DERNumExtensionItemSpecs;

/* BasicConstraints ::= SEQUENCE {
 *     cA                 BOOLEAN DEFAULT FALSE,
 *     pathLenConstraint  INTEGER (0..MAX) OPTIONAL } */
typedef struct {
    DERItem cA;
    DERItem pathLenConstraint;
} DERBasicConstraints;

extern const DERItemSpec DERBasicConstraintsItemSpecs[];
extern const DERShort    DERNumBasicConstraintsItemSpecs;

/* NameConstraints ::= SEQUENCE {
 *     permittedSubtrees [0] GeneralSubtrees OPTIONAL,
 *     excludedSubtrees  [1] GeneralSubtrees OPTIONAL } */
typedef struct {
    DERItem permittedSubtrees;
    DERItem excludedSubtrees;
} DERNameConstraints;

extern const DERItemSpec DERNameConstraintsItemSpecs[];
extern const DERShort    DERNumNameConstraintsItemSpecs;

/* GeneralSubtree ::= SEQUENCE {
 *     base     GeneralName,
 *     minimum  [0] BaseDistance DEFAULT 0,
 *     maximum  [1] BaseDistance OPTIONAL }
 * `base` is a GeneralName CHOICE, so it takes any tag. */
typedef struct {
    DERItem generalName;
    DERItem minimum;
    DERItem maximum;
} DERGeneralSubtree;

extern const DERItemSpec DERGeneralSubtreeItemSpecs[];
extern const DERShort    DERNumGeneralSubtreeItemSpecs;

/* PolicyInformation ::= SEQUENCE {
 *     policyIdentifier  CertPolicyId,
 *     policyQualifiers  SEQUENCE OF PolicyQualifierInfo OPTIONAL } */
typedef struct {
    DERItem policyIdentifier;
    DERItem policyQualifiers;
} DERPolicyInformation;

extern const DERItemSpec DERPolicyInformationItemSpecs[];
extern const DERShort    DERNumPolicyInformationItemSpecs;

/* PolicyQualifierInfo ::= SEQUENCE {
 *     policyQualifierId  OBJECT IDENTIFIER,
 *     qualifier          ANY DEFINED BY policyQualifierId } */
typedef struct {
    DERItem policyQualifierID;
    DERItem qualifier;
} DERPolicyQualifierInfo;

extern const DERItemSpec DERPolicyQualifierInfoItemSpecs[];
extern const DERShort    DERNumPolicyQualifierInfoItemSpecs;

/* UserNotice ::= SEQUENCE {
 *     noticeRef     NoticeReference OPTIONAL,
 *     explicitText  DisplayText OPTIONAL } */
typedef struct {
    DERItem noticeRef;
    DERItem explicitText;
} DERUserNotice;

extern const DERItemSpec DERUserNoticeItemSpecs[];
extern const DERShort    DERNumUserNoticeItemSpecs;

/* NoticeReference ::= SEQUENCE {
 *     organization   DisplayText,
 *     noticeNumbers  SEQUENCE OF INTEGER } */
typedef struct {
    DERItem organization;
    DERItem noticeNumbers;
} DERNoticeReference;

extern const DERItemSpec DERNoticeReferenceItemSpecs[];
extern const DERShort    DERNumNoticeReferenceItemSpecs;

/* PolicyMapping ::= SEQUENCE {
 *     issuerDomainPolicy   CertPolicyId,
 *     subjectDomainPolicy  CertPolicyId } */
typedef struct {
    DERItem issuerDomainPolicy;
    DERItem subjectDomainPolicy;
} DERPolicyMapping;

extern const DERItemSpec DERPolicyMappingItemSpecs[];
extern const DERShort    DERNumPolicyMappingItemSpecs;

/* PolicyConstraints ::= SEQUENCE {
 *     requireExplicitPolicy [0] SkipCerts OPTIONAL,
 *     inhibitPolicyMapping  [1] SkipCerts OPTIONAL }
 * Both are IMPLICIT over INTEGER, so the tags are primitive. */
typedef struct {
    DERItem requireExplicitPolicy;
    DERItem inhibitPolicyMapping;
} DERPolicyConstraints;

extern const DERItemSpec DERPolicyConstraintsItemSpecs[];
extern const DERShort    DERNumPolicyConstraintsItemSpecs;

/* DistributionPoint ::= SEQUENCE {
 *     distributionPoint [0] DistributionPointName OPTIONAL,
 *     reasons           [1] ReasonFlags OPTIONAL,
 *     cRLIssuer         [2] GeneralNames OPTIONAL } */
typedef struct {
    DERItem distributionPoint;
    DERItem reasons;
    DERItem cRLIssuer;
} DERDistributionPoint;

extern const DERItemSpec DERDistributionPointItemSpecs[];
extern const DERShort    DERNumDistributionPointItemSpecs;

/* AccessDescription ::= SEQUENCE {
 *     accessMethod    OBJECT IDENTIFIER,
 *     accessLocation  GeneralName }
 * accessLocation is a CHOICE, so it takes any tag. */
typedef struct {
    DERItem accessMethod;
    DERItem accessLocation;
} DERAccessDescription;

extern const DERItemSpec DERAccessDescriptionItemSpecs[];
extern const DERShort    DERNumAccessDescriptionItemSpecs;

/* AuthorityKeyIdentifier ::= SEQUENCE {
 *     keyIdentifier             [0] KeyIdentifier OPTIONAL,
 *     authorityCertIssuer       [1] GeneralNames OPTIONAL,
 *     authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL }
 * [0] and [2] are IMPLICIT over primitives; [1] wraps a SEQUENCE. */
typedef struct {
    DERItem keyIdentifier;
    DERItem authorityCertIssuer;
    DERItem authorityCertSerialNumber;
} DERAuthorityKeyIdentifier;

extern const DERItemSpec DERAuthorityKeyIdentifierItemSpecs[];
extern const DERShort    DERNumAuthorityKeyIdentifierItemSpecs;

/* PrivateKeyUsagePeriod ::= SEQUENCE {
 *     notBefore [0] GeneralizedTime OPTIONAL,
 *     notAfter  [1] GeneralizedTime OPTIONAL }  -- both IMPLICIT primitives */
typedef struct {
    DERItem notBefore;
    DERItem notAfter;
} DERPrivateKeyUsagePeriod;

extern const DERItemSpec DERPrivateKeyUsagePeriodItemSpecs[];
extern const DERShort    DERNumPrivateKeyUsagePeriodItemSpecs;

/* OtherName ::= SEQUENCE {
 *     type-id  OBJECT IDENTIFIER,
 *     value    [0] EXPLICIT ANY DEFINED BY type-id } */
typedef struct {
    DERItem typeIdentifier;
    DERItem value;
} DEROtherName;

extern const DERItemSpec DEROtherNameItemSpecs[];
extern const DERShort    DERNumOtherNameItemSpecs;

__END_DECLS

#endif /* _DER_CERT_CRL_H_ */
