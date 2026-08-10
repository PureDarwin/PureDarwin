/*
 * oids.c - object identifier constants.
 *
 * Generated from dotted OID forms and verified against OpenSSL; see oids.h.
 */

#include <libDER/oids.h>

#include <stddef.h>

/* 1.2.840.113549.1.1.1  PKCS#1 rsaEncryption */
static const DERByte oidRsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };
const DERItem oidRsa = { (DERByte *)oidRsa_bytes, sizeof(oidRsa_bytes) };

/* 1.2.840.113549.1.1.2  md2WithRSAEncryption */
static const DERByte oidMd2Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x02 };
const DERItem oidMd2Rsa = { (DERByte *)oidMd2Rsa_bytes, sizeof(oidMd2Rsa_bytes) };

/* 1.2.840.113549.1.1.3  md4WithRSAEncryption */
static const DERByte oidMd4Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x03 };
const DERItem oidMd4Rsa = { (DERByte *)oidMd4Rsa_bytes, sizeof(oidMd4Rsa_bytes) };

/* 1.2.840.113549.1.1.4  md5WithRSAEncryption */
static const DERByte oidMd5Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04 };
const DERItem oidMd5Rsa = { (DERByte *)oidMd5Rsa_bytes, sizeof(oidMd5Rsa_bytes) };

/* 1.2.840.113549.1.1.5  sha1WithRSAEncryption */
static const DERByte oidSha1Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05 };
const DERItem oidSha1Rsa = { (DERByte *)oidSha1Rsa_bytes, sizeof(oidSha1Rsa_bytes) };

/* 1.2.840.113549.1.1.11  sha256WithRSAEncryption */
static const DERByte oidSha256Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b };
const DERItem oidSha256Rsa = { (DERByte *)oidSha256Rsa_bytes, sizeof(oidSha256Rsa_bytes) };

/* 1.2.840.113549.1.1.12  sha384WithRSAEncryption */
static const DERByte oidSha384Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c };
const DERItem oidSha384Rsa = { (DERByte *)oidSha384Rsa_bytes, sizeof(oidSha384Rsa_bytes) };

/* 1.2.840.113549.1.1.13  sha512WithRSAEncryption */
static const DERByte oidSha512Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d };
const DERItem oidSha512Rsa = { (DERByte *)oidSha512Rsa_bytes, sizeof(oidSha512Rsa_bytes) };

/* 1.2.840.113549.1.1.14  sha224WithRSAEncryption */
static const DERByte oidSha224Rsa_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0e };
const DERItem oidSha224Rsa = { (DERByte *)oidSha224Rsa_bytes, sizeof(oidSha224Rsa_bytes) };

/* 1.2.840.10045.2.1  id-ecPublicKey */
static const DERByte oidEcPubKey_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
const DERItem oidEcPubKey = { (DERByte *)oidEcPubKey_bytes, sizeof(oidEcPubKey_bytes) };

/* 1.2.840.10045.3.1.7  prime256v1 / secp256r1 */
static const DERByte oidEcPrime256v1_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 };
const DERItem oidEcPrime256v1 = { (DERByte *)oidEcPrime256v1_bytes, sizeof(oidEcPrime256v1_bytes) };

/* 1.3.132.0.34  secp384r1 */
static const DERByte oidAnsip384r1_bytes[] = { 0x2b, 0x81, 0x04, 0x00, 0x22 };
const DERItem oidAnsip384r1 = { (DERByte *)oidAnsip384r1_bytes, sizeof(oidAnsip384r1_bytes) };

/* 1.3.132.0.35  secp521r1 */
static const DERByte oidAnsip521r1_bytes[] = { 0x2b, 0x81, 0x04, 0x00, 0x23 };
const DERItem oidAnsip521r1 = { (DERByte *)oidAnsip521r1_bytes, sizeof(oidAnsip521r1_bytes) };

/* 1.2.840.10045.4.1  ecdsa-with-SHA1 */
static const DERByte oidSha1Ecdsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01 };
const DERItem oidSha1Ecdsa = { (DERByte *)oidSha1Ecdsa_bytes, sizeof(oidSha1Ecdsa_bytes) };

/* 1.2.840.10045.4.3.1  ecdsa-with-SHA224 */
static const DERByte oidSha224Ecdsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x01 };
const DERItem oidSha224Ecdsa = { (DERByte *)oidSha224Ecdsa_bytes, sizeof(oidSha224Ecdsa_bytes) };

/* 1.2.840.10045.4.3.2  ecdsa-with-SHA256 */
static const DERByte oidSha256Ecdsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02 };
const DERItem oidSha256Ecdsa = { (DERByte *)oidSha256Ecdsa_bytes, sizeof(oidSha256Ecdsa_bytes) };

/* 1.2.840.10045.4.3.3  ecdsa-with-SHA384 */
static const DERByte oidSha384Ecdsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03 };
const DERItem oidSha384Ecdsa = { (DERByte *)oidSha384Ecdsa_bytes, sizeof(oidSha384Ecdsa_bytes) };

/* 1.2.840.10045.4.3.4  ecdsa-with-SHA512 */
static const DERByte oidSha512Ecdsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04 };
const DERItem oidSha512Ecdsa = { (DERByte *)oidSha512Ecdsa_bytes, sizeof(oidSha512Ecdsa_bytes) };

/* 1.2.840.10040.4.3  dsa-with-sha1 */
static const DERByte oidSha1Dsa_bytes[] = { 0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x03 };
const DERItem oidSha1Dsa = { (DERByte *)oidSha1Dsa_bytes, sizeof(oidSha1Dsa_bytes) };

/* 1.3.14.3.2.27  dsaWithSHA1 (OIW) */
static const DERByte oidSha1DsaOIW_bytes[] = { 0x2b, 0x0e, 0x03, 0x02, 0x1b };
const DERItem oidSha1DsaOIW = { (DERByte *)oidSha1DsaOIW_bytes, sizeof(oidSha1DsaOIW_bytes) };

/* 1.3.14.3.2.13  dsaCommonWithSHA1 (OIW) */
static const DERByte oidSha1DsaCommonOIW_bytes[] = { 0x2b, 0x0e, 0x03, 0x02, 0x0d };
const DERItem oidSha1DsaCommonOIW = { (DERByte *)oidSha1DsaCommonOIW_bytes, sizeof(oidSha1DsaCommonOIW_bytes) };

/* 1.3.14.3.2.29  sha1WithRSASignature (OIW) */
static const DERByte oidSha1RsaOIW_bytes[] = { 0x2b, 0x0e, 0x03, 0x02, 0x1d };
const DERItem oidSha1RsaOIW = { (DERByte *)oidSha1RsaOIW_bytes, sizeof(oidSha1RsaOIW_bytes) };

/* 1.2.840.113549.2.2  md2 digest */
static const DERByte oidMd2_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x02 };
const DERItem oidMd2 = { (DERByte *)oidMd2_bytes, sizeof(oidMd2_bytes) };

/* 1.2.840.113549.2.4  md4 digest */
static const DERByte oidMd4_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x04 };
const DERItem oidMd4 = { (DERByte *)oidMd4_bytes, sizeof(oidMd4_bytes) };

/* 1.2.840.113549.2.5  md5 digest */
static const DERByte oidMd5_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05 };
const DERItem oidMd5 = { (DERByte *)oidMd5_bytes, sizeof(oidMd5_bytes) };

/* 1.3.14.3.2.26  sha1 digest */
static const DERByte oidSha1_bytes[] = { 0x2b, 0x0e, 0x03, 0x02, 0x1a };
const DERItem oidSha1 = { (DERByte *)oidSha1_bytes, sizeof(oidSha1_bytes) };

/* 2.16.840.1.101.3.4.2.1  sha256 digest */
static const DERByte oidSha256_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01 };
const DERItem oidSha256 = { (DERByte *)oidSha256_bytes, sizeof(oidSha256_bytes) };

/* 2.16.840.1.101.3.4.2.2  sha384 digest */
static const DERByte oidSha384_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02 };
const DERItem oidSha384 = { (DERByte *)oidSha384_bytes, sizeof(oidSha384_bytes) };

/* 2.16.840.1.101.3.4.2.3  sha512 digest */
static const DERByte oidSha512_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03 };
const DERItem oidSha512 = { (DERByte *)oidSha512_bytes, sizeof(oidSha512_bytes) };

/* 2.16.840.1.101.3.4.2.4  sha224 digest */
static const DERByte oidSha224_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04 };
const DERItem oidSha224 = { (DERByte *)oidSha224_bytes, sizeof(oidSha224_bytes) };

/* 2.5.4.3  X.520 id-at-commonName */
static const DERByte oidCommonName_bytes[] = { 0x55, 0x04, 0x03 };
const DERItem oidCommonName = { (DERByte *)oidCommonName_bytes, sizeof(oidCommonName_bytes) };

/* 2.5.4.6  X.520 id-at-countryName */
static const DERByte oidCountryName_bytes[] = { 0x55, 0x04, 0x06 };
const DERItem oidCountryName = { (DERByte *)oidCountryName_bytes, sizeof(oidCountryName_bytes) };

/* 2.5.4.7  X.520 id-at-localityName */
static const DERByte oidLocalityName_bytes[] = { 0x55, 0x04, 0x07 };
const DERItem oidLocalityName = { (DERByte *)oidLocalityName_bytes, sizeof(oidLocalityName_bytes) };

/* 2.5.4.8  X.520 id-at-stateOrProvinceName */
static const DERByte oidStateOrProvinceName_bytes[] = { 0x55, 0x04, 0x08 };
const DERItem oidStateOrProvinceName = { (DERByte *)oidStateOrProvinceName_bytes, sizeof(oidStateOrProvinceName_bytes) };

/* 2.5.4.9  X.520 id-at-streetAddress */
static const DERByte oidStreetAddress_bytes[] = { 0x55, 0x04, 0x09 };
const DERItem oidStreetAddress = { (DERByte *)oidStreetAddress_bytes, sizeof(oidStreetAddress_bytes) };

/* 2.5.4.10  X.520 id-at-organizationName */
static const DERByte oidOrganizationName_bytes[] = { 0x55, 0x04, 0x0a };
const DERItem oidOrganizationName = { (DERByte *)oidOrganizationName_bytes, sizeof(oidOrganizationName_bytes) };

/* 2.5.4.11  X.520 id-at-organizationalUnitName */
static const DERByte oidOrganizationalUnitName_bytes[] = { 0x55, 0x04, 0x0b };
const DERItem oidOrganizationalUnitName = { (DERByte *)oidOrganizationalUnitName_bytes, sizeof(oidOrganizationalUnitName_bytes) };

/* 2.5.4.13  X.520 id-at-description */
static const DERByte oidDescription_bytes[] = { 0x55, 0x04, 0x0d };
const DERItem oidDescription = { (DERByte *)oidDescription_bytes, sizeof(oidDescription_bytes) };

/* 1.2.840.113549.1.9.1  PKCS#9 emailAddress */
static const DERByte oidEmailAddress_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01 };
const DERItem oidEmailAddress = { (DERByte *)oidEmailAddress_bytes, sizeof(oidEmailAddress_bytes) };

/* 1.2.840.113549.1.9.20  PKCS#9 friendlyName */
static const DERByte oidFriendlyName_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14 };
const DERItem oidFriendlyName = { (DERByte *)oidFriendlyName_bytes, sizeof(oidFriendlyName_bytes) };

/* 1.2.840.113549.1.9.21  PKCS#9 localKeyId */
static const DERByte oidLocalKeyId_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x15 };
const DERItem oidLocalKeyId = { (DERByte *)oidLocalKeyId_bytes, sizeof(oidLocalKeyId_bytes) };

/* 0.9.2342.19200300.100.1.1  RFC 4519 uid */
static const DERByte oidUserID_bytes[] = { 0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x01 };
const DERItem oidUserID = { (DERByte *)oidUserID_bytes, sizeof(oidUserID_bytes) };

/* 0.9.2342.19200300.100.1.25  RFC 4519 dc */
static const DERByte oidDomainComponent_bytes[] = { 0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x19 };
const DERItem oidDomainComponent = { (DERByte *)oidDomainComponent_bytes, sizeof(oidDomainComponent_bytes) };

/* 2.5.29.14  id-ce-subjectKeyIdentifier */
static const DERByte oidSubjectKeyIdentifier_bytes[] = { 0x55, 0x1d, 0x0e };
const DERItem oidSubjectKeyIdentifier = { (DERByte *)oidSubjectKeyIdentifier_bytes, sizeof(oidSubjectKeyIdentifier_bytes) };

/* 2.5.29.15  id-ce-keyUsage */
static const DERByte oidKeyUsage_bytes[] = { 0x55, 0x1d, 0x0f };
const DERItem oidKeyUsage = { (DERByte *)oidKeyUsage_bytes, sizeof(oidKeyUsage_bytes) };

/* 2.5.29.16  id-ce-privateKeyUsagePeriod */
static const DERByte oidPrivateKeyUsagePeriod_bytes[] = { 0x55, 0x1d, 0x10 };
const DERItem oidPrivateKeyUsagePeriod = { (DERByte *)oidPrivateKeyUsagePeriod_bytes, sizeof(oidPrivateKeyUsagePeriod_bytes) };

/* 2.5.29.17  id-ce-subjectAltName */
static const DERByte oidSubjectAltName_bytes[] = { 0x55, 0x1d, 0x11 };
const DERItem oidSubjectAltName = { (DERByte *)oidSubjectAltName_bytes, sizeof(oidSubjectAltName_bytes) };

/* 2.5.29.18  id-ce-issuerAltName */
static const DERByte oidIssuerAltName_bytes[] = { 0x55, 0x1d, 0x12 };
const DERItem oidIssuerAltName = { (DERByte *)oidIssuerAltName_bytes, sizeof(oidIssuerAltName_bytes) };

/* 2.5.29.19  id-ce-basicConstraints */
static const DERByte oidBasicConstraints_bytes[] = { 0x55, 0x1d, 0x13 };
const DERItem oidBasicConstraints = { (DERByte *)oidBasicConstraints_bytes, sizeof(oidBasicConstraints_bytes) };

/* 2.5.29.30  id-ce-nameConstraints */
static const DERByte oidNameConstraints_bytes[] = { 0x55, 0x1d, 0x1e };
const DERItem oidNameConstraints = { (DERByte *)oidNameConstraints_bytes, sizeof(oidNameConstraints_bytes) };

/* 2.5.29.31  id-ce-cRLDistributionPoints */
static const DERByte oidCrlDistributionPoints_bytes[] = { 0x55, 0x1d, 0x1f };
const DERItem oidCrlDistributionPoints = { (DERByte *)oidCrlDistributionPoints_bytes, sizeof(oidCrlDistributionPoints_bytes) };

/* 2.5.29.32  id-ce-certificatePolicies */
static const DERByte oidCertificatePolicies_bytes[] = { 0x55, 0x1d, 0x20 };
const DERItem oidCertificatePolicies = { (DERByte *)oidCertificatePolicies_bytes, sizeof(oidCertificatePolicies_bytes) };

/* 2.5.29.32.0  anyPolicy */
static const DERByte oidAnyPolicy_bytes[] = { 0x55, 0x1d, 0x20, 0x00 };
const DERItem oidAnyPolicy = { (DERByte *)oidAnyPolicy_bytes, sizeof(oidAnyPolicy_bytes) };

/* 2.5.29.33  id-ce-policyMappings */
static const DERByte oidPolicyMappings_bytes[] = { 0x55, 0x1d, 0x21 };
const DERItem oidPolicyMappings = { (DERByte *)oidPolicyMappings_bytes, sizeof(oidPolicyMappings_bytes) };

/* 2.5.29.35  id-ce-authorityKeyIdentifier */
static const DERByte oidAuthorityKeyIdentifier_bytes[] = { 0x55, 0x1d, 0x23 };
const DERItem oidAuthorityKeyIdentifier = { (DERByte *)oidAuthorityKeyIdentifier_bytes, sizeof(oidAuthorityKeyIdentifier_bytes) };

/* 2.5.29.36  id-ce-policyConstraints */
static const DERByte oidPolicyConstraints_bytes[] = { 0x55, 0x1d, 0x24 };
const DERItem oidPolicyConstraints = { (DERByte *)oidPolicyConstraints_bytes, sizeof(oidPolicyConstraints_bytes) };

/* 2.5.29.37  id-ce-extKeyUsage */
static const DERByte oidExtendedKeyUsage_bytes[] = { 0x55, 0x1d, 0x25 };
const DERItem oidExtendedKeyUsage = { (DERByte *)oidExtendedKeyUsage_bytes, sizeof(oidExtendedKeyUsage_bytes) };

/* 2.5.29.37.0  anyExtendedKeyUsage */
static const DERByte oidAnyExtendedKeyUsage_bytes[] = { 0x55, 0x1d, 0x25, 0x00 };
const DERItem oidAnyExtendedKeyUsage = { (DERByte *)oidAnyExtendedKeyUsage_bytes, sizeof(oidAnyExtendedKeyUsage_bytes) };

/* 2.5.29.54  id-ce-inhibitAnyPolicy */
static const DERByte oidInhibitAnyPolicy_bytes[] = { 0x55, 0x1d, 0x36 };
const DERItem oidInhibitAnyPolicy = { (DERByte *)oidInhibitAnyPolicy_bytes, sizeof(oidInhibitAnyPolicy_bytes) };

/* 1.3.6.1.5.5.7.1.1  id-pe-authorityInfoAccess */
static const DERByte oidAuthorityInfoAccess_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01 };
const DERItem oidAuthorityInfoAccess = { (DERByte *)oidAuthorityInfoAccess_bytes, sizeof(oidAuthorityInfoAccess_bytes) };

/* 1.3.6.1.5.5.7.1.11  id-pe-subjectInfoAccess */
static const DERByte oidSubjectInfoAccess_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x0b };
const DERItem oidSubjectInfoAccess = { (DERByte *)oidSubjectInfoAccess_bytes, sizeof(oidSubjectInfoAccess_bytes) };

/* 1.3.6.1.5.5.7.48.1  id-ad-ocsp */
static const DERByte oidAdOCSP_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01 };
const DERItem oidAdOCSP = { (DERByte *)oidAdOCSP_bytes, sizeof(oidAdOCSP_bytes) };

/* 1.3.6.1.5.5.7.48.2  id-ad-caIssuers */
static const DERByte oidAdCAIssuer_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02 };
const DERItem oidAdCAIssuer = { (DERByte *)oidAdCAIssuer_bytes, sizeof(oidAdCAIssuer_bytes) };

/* 1.3.6.1.5.5.7.48.1.5  id-pkix-ocsp-nocheck */
static const DERByte oidOCSPNoCheck_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x05 };
const DERItem oidOCSPNoCheck = { (DERByte *)oidOCSPNoCheck_bytes, sizeof(oidOCSPNoCheck_bytes) };

/* 1.3.6.1.5.5.7.2.1  id-qt-cps */
static const DERByte oidQtCps_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x01 };
const DERItem oidQtCps = { (DERByte *)oidQtCps_bytes, sizeof(oidQtCps_bytes) };

/* 1.3.6.1.5.5.7.2.2  id-qt-unotice */
static const DERByte oidQtUNotice_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02 };
const DERItem oidQtUNotice = { (DERByte *)oidQtUNotice_bytes, sizeof(oidQtUNotice_bytes) };

/* 1.3.6.1.5.5.7.3.1  id-kp-serverAuth */
static const DERByte oidExtendedKeyUsageServerAuth_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01 };
const DERItem oidExtendedKeyUsageServerAuth = { (DERByte *)oidExtendedKeyUsageServerAuth_bytes, sizeof(oidExtendedKeyUsageServerAuth_bytes) };

/* 1.3.6.1.5.5.7.3.2  id-kp-clientAuth */
static const DERByte oidExtendedKeyUsageClientAuth_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02 };
const DERItem oidExtendedKeyUsageClientAuth = { (DERByte *)oidExtendedKeyUsageClientAuth_bytes, sizeof(oidExtendedKeyUsageClientAuth_bytes) };

/* 1.3.6.1.5.5.7.3.3  id-kp-codeSigning */
static const DERByte oidExtendedKeyUsageCodeSigning_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03 };
const DERItem oidExtendedKeyUsageCodeSigning = { (DERByte *)oidExtendedKeyUsageCodeSigning_bytes, sizeof(oidExtendedKeyUsageCodeSigning_bytes) };

/* 1.3.6.1.5.5.7.3.4  id-kp-emailProtection */
static const DERByte oidExtendedKeyUsageEmailProtection_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04 };
const DERItem oidExtendedKeyUsageEmailProtection = { (DERByte *)oidExtendedKeyUsageEmailProtection_bytes, sizeof(oidExtendedKeyUsageEmailProtection_bytes) };

/* 1.3.6.1.5.5.7.3.5  id-kp-ipsecEndSystem */
static const DERByte oidExtendedKeyUsageIPSec_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x05 };
const DERItem oidExtendedKeyUsageIPSec = { (DERByte *)oidExtendedKeyUsageIPSec_bytes, sizeof(oidExtendedKeyUsageIPSec_bytes) };

/* 1.3.6.1.5.5.7.3.8  id-kp-timeStamping */
static const DERByte oidExtendedKeyUsageTimeStamping_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x08 };
const DERItem oidExtendedKeyUsageTimeStamping = { (DERByte *)oidExtendedKeyUsageTimeStamping_bytes, sizeof(oidExtendedKeyUsageTimeStamping_bytes) };

/* 1.3.6.1.5.5.7.3.9  id-kp-OCSPSigning */
static const DERByte oidExtendedKeyUsageOCSPSigning_bytes[] = { 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x09 };
const DERItem oidExtendedKeyUsageOCSPSigning = { (DERByte *)oidExtendedKeyUsageOCSPSigning_bytes, sizeof(oidExtendedKeyUsageOCSPSigning_bytes) };

/* 1.3.6.1.4.1.311.10.3.3  Microsoft server-gated crypto */
static const DERByte oidExtendedKeyUsageMicrosoftSGC_bytes[] = { 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0a, 0x03, 0x03 };
const DERItem oidExtendedKeyUsageMicrosoftSGC = { (DERByte *)oidExtendedKeyUsageMicrosoftSGC_bytes, sizeof(oidExtendedKeyUsageMicrosoftSGC_bytes) };

/* 2.16.840.1.113730.4.1  Netscape server-gated crypto */
static const DERByte oidExtendedKeyUsageNetscapeSGC_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x04, 0x01 };
const DERItem oidExtendedKeyUsageNetscapeSGC = { (DERByte *)oidExtendedKeyUsageNetscapeSGC_bytes, sizeof(oidExtendedKeyUsageNetscapeSGC_bytes) };

/* 1.3.6.1.4.1.311.20.2.3  Microsoft userPrincipalName */
static const DERByte oidMSNTPrincipalName_bytes[] = { 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x03 };
const DERItem oidMSNTPrincipalName = { (DERByte *)oidMSNTPrincipalName_bytes, sizeof(oidMSNTPrincipalName_bytes) };

/* 2.16.840.1.113730.1.1  Netscape cert-type */
static const DERByte oidNetscapeCertType_bytes[] = { 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x01, 0x01 };
const DERItem oidNetscapeCertType = { (DERByte *)oidNetscapeCertType_bytes, sizeof(oidNetscapeCertType_bytes) };

/* 1.2.840.113533.7.65.0  Entrust version extension */
static const DERByte oidEntrustVersInfo_bytes[] = { 0x2a, 0x86, 0x48, 0x86, 0xf6, 0x7d, 0x07, 0x41, 0x00 };
const DERItem oidEntrustVersInfo = { (DERByte *)oidEntrustVersInfo_bytes, sizeof(oidEntrustVersInfo_bytes) };

/* 1.3.6.1.4.1.11129.2.4.2  CT SCT list */
static const DERByte oidGoogleEmbeddedSignedCertificateTimestamp_bytes[] = { 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x02 };
const DERItem oidGoogleEmbeddedSignedCertificateTimestamp = { (DERByte *)oidGoogleEmbeddedSignedCertificateTimestamp_bytes, sizeof(oidGoogleEmbeddedSignedCertificateTimestamp_bytes) };

/* Apple FEE (Fast Elliptic Encryption) with MD5 - legacy, value unverified.
 * Deliberately empty: an empty DERItem can never compare equal to a real
 * OID, so this reads as "not present" rather than matching the wrong thing. */
const DERItem oidMd5Fee = { NULL, 0 };

/* Apple FEE with SHA1 - legacy, value unverified.
 * Deliberately empty: an empty DERItem can never compare equal to a real
 * OID, so this reads as "not present" rather than matching the wrong thing. */
const DERItem oidSha1Fee = { NULL, 0 };

/* Apple escrow service policy - value unverified.
 * Deliberately empty: an empty DERItem can never compare equal to a real
 * OID, so this reads as "not present" rather than matching the wrong thing. */
const DERItem oidApplePolicyEscrowService = { NULL, 0 };
