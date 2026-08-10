/*
 * DER_CertCrl.c - X.509 spec tables.
 *
 * Context tag conventions used below:
 *   CTX_C(n)  constructed [n] - an EXPLICIT wrapper, or an IMPLICIT one over a
 *             constructed type such as SEQUENCE
 *   CTX_P(n)  primitive [n]   - IMPLICIT over a primitive type
 * Choosing the wrong one makes the field silently fail to match, which for an
 * OPTIONAL field means it reads as absent rather than as an error.
 */

#include <libDER/DER_CertCrl.h>

#define CTX_C(n) (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | (n))
#define CTX_P(n) (ASN1_CONTEXT_SPECIFIC | (n))

/* Certificate */
const DERItemSpec DERSignedCertCrlItemSpecs[] = {
    /* The signature is computed over the encoded TBSCertificate, so keep the
     * whole TLV rather than just its content. */
    { DER_OFFSET(DERSignedCertCrl, tbs),    ASN1_CONSTR_SEQUENCE, DER_DEC_SAVE_DER },
    { DER_OFFSET(DERSignedCertCrl, sigAlg), ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERSignedCertCrl, sig),    ASN1_BIT_STRING,      DER_DEC_NO_OPTS  },
};
const DERShort DERNumSignedCertCrlItemSpecs =
    sizeof(DERSignedCertCrlItemSpecs) / sizeof(DERItemSpec);

/* TBSCertificate */
const DERItemSpec DERTBSCertItemSpecs[] = {
    { DER_OFFSET(DERTBSCert, version),       CTX_C(0),             DER_DEC_OPTIONAL },
    { DER_OFFSET(DERTBSCert, serialNum),     ASN1_INTEGER,         DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERTBSCert, tbsSigAlg),     ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    /* issuer, subject, validity and subjectPubKey are handed to callers as
     * CONTENT, not whole TLVs: Security feeds each of them straight to
     * DERParseSequenceContent, and its own comments describe issuer/subject as
     * "a sequence without the tag". Only tbs above is kept in full DER form,
     * because the signature is computed over the encoded TBSCertificate. */
    { DER_OFFSET(DERTBSCert, issuer),        ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERTBSCert, validity),      ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERTBSCert, subject),       ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERTBSCert, subjectPubKey), ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS  },
    /* IMPLICIT BIT STRINGs - primitive tags. */
    { DER_OFFSET(DERTBSCert, issuerID),      CTX_P(1),             DER_DEC_OPTIONAL },
    { DER_OFFSET(DERTBSCert, subjectID),     CTX_P(2),             DER_DEC_OPTIONAL },
    /* EXPLICIT wrapper around the Extensions SEQUENCE. */
    { DER_OFFSET(DERTBSCert, extensions),    CTX_C(3),             DER_DEC_OPTIONAL },
};
const DERShort DERNumTBSCertItemSpecs =
    sizeof(DERTBSCertItemSpecs) / sizeof(DERItemSpec);

/* Validity - Time is a CHOICE of UTCTime and GeneralizedTime. */
const DERItemSpec DERValidityItemSpecs[] = {
    { DER_OFFSET(DERValidity, notBefore), 0, DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
    { DER_OFFSET(DERValidity, notAfter),  0, DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
};
const DERShort DERNumValidityItemSpecs =
    sizeof(DERValidityItemSpecs) / sizeof(DERItemSpec);

/* AttributeTypeAndValue - value is a DirectoryString CHOICE. */
const DERItemSpec DERAttributeTypeAndValueItemSpecs[] = {
    { DER_OFFSET(DERAttributeTypeAndValue, type),  ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERAttributeTypeAndValue, value), 0,
      DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
};
const DERShort DERNumAttributeTypeAndValueItemSpecs =
    sizeof(DERAttributeTypeAndValueItemSpecs) / sizeof(DERItemSpec);

/* Extension */
const DERItemSpec DERExtensionItemSpecs[] = {
    { DER_OFFSET(DERExtension, extnID),    ASN1_OBJECT_ID,    DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERExtension, critical),  ASN1_BOOLEAN,      DER_DEC_OPTIONAL },
    { DER_OFFSET(DERExtension, extnValue), ASN1_OCTET_STRING, DER_DEC_NO_OPTS  },
};
const DERShort DERNumExtensionItemSpecs =
    sizeof(DERExtensionItemSpecs) / sizeof(DERItemSpec);

/* BasicConstraints */
const DERItemSpec DERBasicConstraintsItemSpecs[] = {
    { DER_OFFSET(DERBasicConstraints, cA),                ASN1_BOOLEAN, DER_DEC_OPTIONAL },
    { DER_OFFSET(DERBasicConstraints, pathLenConstraint), ASN1_INTEGER, DER_DEC_OPTIONAL },
};
const DERShort DERNumBasicConstraintsItemSpecs =
    sizeof(DERBasicConstraintsItemSpecs) / sizeof(DERItemSpec);

/* NameConstraints - both wrap a SEQUENCE OF, so constructed. */
const DERItemSpec DERNameConstraintsItemSpecs[] = {
    { DER_OFFSET(DERNameConstraints, permittedSubtrees), CTX_C(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERNameConstraints, excludedSubtrees),  CTX_C(1), DER_DEC_OPTIONAL },
};
const DERShort DERNumNameConstraintsItemSpecs =
    sizeof(DERNameConstraintsItemSpecs) / sizeof(DERItemSpec);

/* GeneralSubtree - base is a GeneralName CHOICE; the distances are IMPLICIT
 * INTEGERs and so primitive. */
const DERItemSpec DERGeneralSubtreeItemSpecs[] = {
    { DER_OFFSET(DERGeneralSubtree, generalName), 0,
      DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
    { DER_OFFSET(DERGeneralSubtree, minimum),     CTX_P(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERGeneralSubtree, maximum),     CTX_P(1), DER_DEC_OPTIONAL },
};
const DERShort DERNumGeneralSubtreeItemSpecs =
    sizeof(DERGeneralSubtreeItemSpecs) / sizeof(DERItemSpec);

/* PolicyInformation */
const DERItemSpec DERPolicyInformationItemSpecs[] = {
    { DER_OFFSET(DERPolicyInformation, policyIdentifier), ASN1_OBJECT_ID,       DER_DEC_NO_OPTS  },
    { DER_OFFSET(DERPolicyInformation, policyQualifiers), ASN1_CONSTR_SEQUENCE, DER_DEC_OPTIONAL },
};
const DERShort DERNumPolicyInformationItemSpecs =
    sizeof(DERPolicyInformationItemSpecs) / sizeof(DERItemSpec);

/* PolicyQualifierInfo - qualifier is ANY. */
const DERItemSpec DERPolicyQualifierInfoItemSpecs[] = {
    { DER_OFFSET(DERPolicyQualifierInfo, policyQualifierID), ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERPolicyQualifierInfo, qualifier),         0,
      DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
};
const DERShort DERNumPolicyQualifierInfoItemSpecs =
    sizeof(DERPolicyQualifierInfoItemSpecs) / sizeof(DERItemSpec);

/* UserNotice - both members optional; explicitText is a DisplayText CHOICE. */
const DERItemSpec DERUserNoticeItemSpecs[] = {
    { DER_OFFSET(DERUserNotice, noticeRef),    ASN1_CONSTR_SEQUENCE, DER_DEC_OPTIONAL },
    { DER_OFFSET(DERUserNotice, explicitText), 0,
      DER_DEC_ASN_ANY | DER_DEC_OPTIONAL | DER_DEC_SAVE_DER },
};
const DERShort DERNumUserNoticeItemSpecs =
    sizeof(DERUserNoticeItemSpecs) / sizeof(DERItemSpec);

/* NoticeReference */
const DERItemSpec DERNoticeReferenceItemSpecs[] = {
    { DER_OFFSET(DERNoticeReference, organization),  0,
      DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
    { DER_OFFSET(DERNoticeReference, noticeNumbers), ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS },
};
const DERShort DERNumNoticeReferenceItemSpecs =
    sizeof(DERNoticeReferenceItemSpecs) / sizeof(DERItemSpec);

/* PolicyMapping */
const DERItemSpec DERPolicyMappingItemSpecs[] = {
    { DER_OFFSET(DERPolicyMapping, issuerDomainPolicy),  ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERPolicyMapping, subjectDomainPolicy), ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
};
const DERShort DERNumPolicyMappingItemSpecs =
    sizeof(DERPolicyMappingItemSpecs) / sizeof(DERItemSpec);

/* PolicyConstraints - IMPLICIT INTEGERs. */
const DERItemSpec DERPolicyConstraintsItemSpecs[] = {
    { DER_OFFSET(DERPolicyConstraints, requireExplicitPolicy), CTX_P(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERPolicyConstraints, inhibitPolicyMapping),  CTX_P(1), DER_DEC_OPTIONAL },
};
const DERShort DERNumPolicyConstraintsItemSpecs =
    sizeof(DERPolicyConstraintsItemSpecs) / sizeof(DERItemSpec);

/* DistributionPoint - [0] wraps DistributionPointName (a CHOICE, constructed),
 * [1] is an IMPLICIT BIT STRING (primitive), [2] wraps GeneralNames. */
const DERItemSpec DERDistributionPointItemSpecs[] = {
    { DER_OFFSET(DERDistributionPoint, distributionPoint), CTX_C(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERDistributionPoint, reasons),           CTX_P(1), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERDistributionPoint, cRLIssuer),         CTX_C(2), DER_DEC_OPTIONAL },
};
const DERShort DERNumDistributionPointItemSpecs =
    sizeof(DERDistributionPointItemSpecs) / sizeof(DERItemSpec);

/* AccessDescription - accessLocation is a GeneralName CHOICE. */
const DERItemSpec DERAccessDescriptionItemSpecs[] = {
    { DER_OFFSET(DERAccessDescription, accessMethod),   ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DERAccessDescription, accessLocation), 0,
      DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
};
const DERShort DERNumAccessDescriptionItemSpecs =
    sizeof(DERAccessDescriptionItemSpecs) / sizeof(DERItemSpec);

/* AuthorityKeyIdentifier - [0] IMPLICIT OCTET STRING and [2] IMPLICIT INTEGER
 * are primitive; [1] wraps GeneralNames. */
const DERItemSpec DERAuthorityKeyIdentifierItemSpecs[] = {
    { DER_OFFSET(DERAuthorityKeyIdentifier, keyIdentifier),             CTX_P(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERAuthorityKeyIdentifier, authorityCertIssuer),       CTX_C(1), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERAuthorityKeyIdentifier, authorityCertSerialNumber), CTX_P(2), DER_DEC_OPTIONAL },
};
const DERShort DERNumAuthorityKeyIdentifierItemSpecs =
    sizeof(DERAuthorityKeyIdentifierItemSpecs) / sizeof(DERItemSpec);

/* PrivateKeyUsagePeriod - IMPLICIT GeneralizedTimes, primitive. */
const DERItemSpec DERPrivateKeyUsagePeriodItemSpecs[] = {
    { DER_OFFSET(DERPrivateKeyUsagePeriod, notBefore), CTX_P(0), DER_DEC_OPTIONAL },
    { DER_OFFSET(DERPrivateKeyUsagePeriod, notAfter),  CTX_P(1), DER_DEC_OPTIONAL },
};
const DERShort DERNumPrivateKeyUsagePeriodItemSpecs =
    sizeof(DERPrivateKeyUsagePeriodItemSpecs) / sizeof(DERItemSpec);

/* OtherName - value is an EXPLICIT [0] wrapper. */
const DERItemSpec DEROtherNameItemSpecs[] = {
    { DER_OFFSET(DEROtherName, typeIdentifier), ASN1_OBJECT_ID, DER_DEC_NO_OPTS },
    { DER_OFFSET(DEROtherName, value),          CTX_C(0),       DER_DEC_NO_OPTS },
};
const DERShort DERNumOtherNameItemSpecs =
    sizeof(DEROtherNameItemSpecs) / sizeof(DERItemSpec);
