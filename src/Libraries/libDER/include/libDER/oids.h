/*
 * oids.h - object identifier constants.
 */
#ifndef _LIB_DER_OIDS_H_
#define _LIB_DER_OIDS_H_

#include <libDER/DERItem.h>
/* Callers that name an OID almost always go on to compare against it. */
#include <libDER/DER_Decode.h>

__BEGIN_DECLS

extern const DERItem oidRsa;    /* 1.2.840.113549.1.1.1  PKCS#1 rsaEncryption */
extern const DERItem oidMd2Rsa;    /* 1.2.840.113549.1.1.2  md2WithRSAEncryption */
extern const DERItem oidMd4Rsa;    /* 1.2.840.113549.1.1.3  md4WithRSAEncryption */
extern const DERItem oidMd5Rsa;    /* 1.2.840.113549.1.1.4  md5WithRSAEncryption */
extern const DERItem oidSha1Rsa;    /* 1.2.840.113549.1.1.5  sha1WithRSAEncryption */
extern const DERItem oidSha256Rsa;    /* 1.2.840.113549.1.1.11  sha256WithRSAEncryption */
extern const DERItem oidSha384Rsa;    /* 1.2.840.113549.1.1.12  sha384WithRSAEncryption */
extern const DERItem oidSha512Rsa;    /* 1.2.840.113549.1.1.13  sha512WithRSAEncryption */
extern const DERItem oidSha224Rsa;    /* 1.2.840.113549.1.1.14  sha224WithRSAEncryption */
extern const DERItem oidEcPubKey;    /* 1.2.840.10045.2.1  id-ecPublicKey */
extern const DERItem oidEcPrime256v1;    /* 1.2.840.10045.3.1.7  prime256v1 / secp256r1 */
extern const DERItem oidAnsip384r1;    /* 1.3.132.0.34  secp384r1 */
extern const DERItem oidAnsip521r1;    /* 1.3.132.0.35  secp521r1 */
extern const DERItem oidSha1Ecdsa;    /* 1.2.840.10045.4.1  ecdsa-with-SHA1 */
extern const DERItem oidSha224Ecdsa;    /* 1.2.840.10045.4.3.1  ecdsa-with-SHA224 */
extern const DERItem oidSha256Ecdsa;    /* 1.2.840.10045.4.3.2  ecdsa-with-SHA256 */
extern const DERItem oidSha384Ecdsa;    /* 1.2.840.10045.4.3.3  ecdsa-with-SHA384 */
extern const DERItem oidSha512Ecdsa;    /* 1.2.840.10045.4.3.4  ecdsa-with-SHA512 */
extern const DERItem oidSha1Dsa;    /* 1.2.840.10040.4.3  dsa-with-sha1 */
extern const DERItem oidSha1DsaOIW;    /* 1.3.14.3.2.27  dsaWithSHA1 (OIW) */
extern const DERItem oidSha1DsaCommonOIW;    /* 1.3.14.3.2.13  dsaCommonWithSHA1 (OIW) */
extern const DERItem oidSha1RsaOIW;    /* 1.3.14.3.2.29  sha1WithRSASignature (OIW) */
extern const DERItem oidMd2;    /* 1.2.840.113549.2.2  md2 digest */
extern const DERItem oidMd4;    /* 1.2.840.113549.2.4  md4 digest */
extern const DERItem oidMd5;    /* 1.2.840.113549.2.5  md5 digest */
extern const DERItem oidSha1;    /* 1.3.14.3.2.26  sha1 digest */
extern const DERItem oidSha256;    /* 2.16.840.1.101.3.4.2.1  sha256 digest */
extern const DERItem oidSha384;    /* 2.16.840.1.101.3.4.2.2  sha384 digest */
extern const DERItem oidSha512;    /* 2.16.840.1.101.3.4.2.3  sha512 digest */
extern const DERItem oidSha224;    /* 2.16.840.1.101.3.4.2.4  sha224 digest */
extern const DERItem oidCommonName;    /* 2.5.4.3  X.520 id-at-commonName */
extern const DERItem oidCountryName;    /* 2.5.4.6  X.520 id-at-countryName */
extern const DERItem oidLocalityName;    /* 2.5.4.7  X.520 id-at-localityName */
extern const DERItem oidStateOrProvinceName;    /* 2.5.4.8  X.520 id-at-stateOrProvinceName */
extern const DERItem oidStreetAddress;    /* 2.5.4.9  X.520 id-at-streetAddress */
extern const DERItem oidOrganizationName;    /* 2.5.4.10  X.520 id-at-organizationName */
extern const DERItem oidOrganizationalUnitName;    /* 2.5.4.11  X.520 id-at-organizationalUnitName */
extern const DERItem oidDescription;    /* 2.5.4.13  X.520 id-at-description */
extern const DERItem oidEmailAddress;    /* 1.2.840.113549.1.9.1  PKCS#9 emailAddress */
extern const DERItem oidFriendlyName;    /* 1.2.840.113549.1.9.20  PKCS#9 friendlyName */
extern const DERItem oidLocalKeyId;    /* 1.2.840.113549.1.9.21  PKCS#9 localKeyId */
extern const DERItem oidUserID;    /* 0.9.2342.19200300.100.1.1  RFC 4519 uid */
extern const DERItem oidDomainComponent;    /* 0.9.2342.19200300.100.1.25  RFC 4519 dc */
extern const DERItem oidSubjectKeyIdentifier;    /* 2.5.29.14  id-ce-subjectKeyIdentifier */
extern const DERItem oidKeyUsage;    /* 2.5.29.15  id-ce-keyUsage */
extern const DERItem oidPrivateKeyUsagePeriod;    /* 2.5.29.16  id-ce-privateKeyUsagePeriod */
extern const DERItem oidSubjectAltName;    /* 2.5.29.17  id-ce-subjectAltName */
extern const DERItem oidIssuerAltName;    /* 2.5.29.18  id-ce-issuerAltName */
extern const DERItem oidBasicConstraints;    /* 2.5.29.19  id-ce-basicConstraints */
extern const DERItem oidNameConstraints;    /* 2.5.29.30  id-ce-nameConstraints */
extern const DERItem oidCrlDistributionPoints;    /* 2.5.29.31  id-ce-cRLDistributionPoints */
extern const DERItem oidCertificatePolicies;    /* 2.5.29.32  id-ce-certificatePolicies */
extern const DERItem oidAnyPolicy;    /* 2.5.29.32.0  anyPolicy */
extern const DERItem oidPolicyMappings;    /* 2.5.29.33  id-ce-policyMappings */
extern const DERItem oidAuthorityKeyIdentifier;    /* 2.5.29.35  id-ce-authorityKeyIdentifier */
extern const DERItem oidPolicyConstraints;    /* 2.5.29.36  id-ce-policyConstraints */
extern const DERItem oidExtendedKeyUsage;    /* 2.5.29.37  id-ce-extKeyUsage */
extern const DERItem oidAnyExtendedKeyUsage;    /* 2.5.29.37.0  anyExtendedKeyUsage */
extern const DERItem oidInhibitAnyPolicy;    /* 2.5.29.54  id-ce-inhibitAnyPolicy */
extern const DERItem oidAuthorityInfoAccess;    /* 1.3.6.1.5.5.7.1.1  id-pe-authorityInfoAccess */
extern const DERItem oidSubjectInfoAccess;    /* 1.3.6.1.5.5.7.1.11  id-pe-subjectInfoAccess */
extern const DERItem oidAdOCSP;    /* 1.3.6.1.5.5.7.48.1  id-ad-ocsp */
extern const DERItem oidAdCAIssuer;    /* 1.3.6.1.5.5.7.48.2  id-ad-caIssuers */
extern const DERItem oidOCSPNoCheck;    /* 1.3.6.1.5.5.7.48.1.5  id-pkix-ocsp-nocheck */
extern const DERItem oidQtCps;    /* 1.3.6.1.5.5.7.2.1  id-qt-cps */
extern const DERItem oidQtUNotice;    /* 1.3.6.1.5.5.7.2.2  id-qt-unotice */
extern const DERItem oidExtendedKeyUsageServerAuth;    /* 1.3.6.1.5.5.7.3.1  id-kp-serverAuth */
extern const DERItem oidExtendedKeyUsageClientAuth;    /* 1.3.6.1.5.5.7.3.2  id-kp-clientAuth */
extern const DERItem oidExtendedKeyUsageCodeSigning;    /* 1.3.6.1.5.5.7.3.3  id-kp-codeSigning */
extern const DERItem oidExtendedKeyUsageEmailProtection;    /* 1.3.6.1.5.5.7.3.4  id-kp-emailProtection */
extern const DERItem oidExtendedKeyUsageIPSec;    /* 1.3.6.1.5.5.7.3.5  id-kp-ipsecEndSystem */
extern const DERItem oidExtendedKeyUsageTimeStamping;    /* 1.3.6.1.5.5.7.3.8  id-kp-timeStamping */
extern const DERItem oidExtendedKeyUsageOCSPSigning;    /* 1.3.6.1.5.5.7.3.9  id-kp-OCSPSigning */
extern const DERItem oidExtendedKeyUsageMicrosoftSGC;    /* 1.3.6.1.4.1.311.10.3.3  Microsoft server-gated crypto */
extern const DERItem oidExtendedKeyUsageNetscapeSGC;    /* 2.16.840.1.113730.4.1  Netscape server-gated crypto */
extern const DERItem oidMSNTPrincipalName;    /* 1.3.6.1.4.1.311.20.2.3  Microsoft userPrincipalName */
extern const DERItem oidNetscapeCertType;    /* 2.16.840.1.113730.1.1  Netscape cert-type */
extern const DERItem oidEntrustVersInfo;    /* 1.2.840.113533.7.65.0  Entrust version extension */
extern const DERItem oidGoogleEmbeddedSignedCertificateTimestamp;    /* 1.3.6.1.4.1.11129.2.4.2  CT SCT list */
extern const DERItem oidMd5Fee;    /* UNVERIFIED - never matches; see oids.c */
extern const DERItem oidSha1Fee;    /* UNVERIFIED - never matches; see oids.c */
extern const DERItem oidApplePolicyEscrowService;    /* UNVERIFIED - never matches; see oids.c */

/* Apple's own arcs under appleDataSecurity (1.2.840.113635.100). */
extern const DERItem oidAppleIntmMarkerAppleWWDR;    /* 1.2.840.113635.100.6.2.1  Apple WWDR intermediate marker */
extern const DERItem oidAppleIntmMarkerAppleID;    /* 1.2.840.113635.100.6.2.3  Apple Application Integration intermediate marker */
extern const DERItem oidAppleIntmMarkerAppleID2;    /* 1.2.840.113635.100.6.2.7  Apple Apple ID intermediate marker (second subCA) */
extern const DERItem oidAppleIntmMarkerAppleSystemIntg2;    /* 1.2.840.113635.100.6.2.10  Apple System Integration 2 intermediate marker */
extern const DERItem oidAppleIntmMarkerAppleServerAuthentication;    /* 1.2.840.113635.100.6.2.12  Apple Server Authentication intermediate marker */
extern const DERItem oidAppleIntmMarkerAppleSystemIntgG3;    /* 1.2.840.113635.100.6.2.13  Apple System Integration G3 intermediate marker */
extern const DERItem oidAppleIntmMarkerAppleHomeKitServerCA;    /* 1.2.840.113635.100.6.2.16  Apple HomeKit server CA intermediate marker */
extern const DERItem oidAppleExtendedKeyUsageCodeSigning;    /* 1.2.840.113635.100.4.1  Apple Apple code signing EKU */
extern const DERItem oidAppleExtendedKeyUsageCodeSigningDev;    /* UNVERIFIED - never matches; see oids.c */
extern const DERItem oidAppleExtendedKeyUsageAppleID;    /* 1.2.840.113635.100.4.7  Apple Apple ID sharing EKU */
extern const DERItem oidAppleExtendedKeyUsagePassbook;    /* 1.2.840.113635.100.4.14  Apple Passbook signing EKU */
extern const DERItem oidAppleExtendedKeyUsageProfileSigning;    /* 1.2.840.113635.100.4.16  Apple configuration profile signing EKU */
extern const DERItem oidAppleExtendedKeyUsageQAProfileSigning;    /* 1.2.840.113635.100.4.17  Apple configuration profile signing EKU, QA */
extern const DERItem oidAppleCertExtOSXProvisioningProfileSigning;    /* 1.2.840.113635.100.4.11  Apple macOS provisioning profile signing */
extern const DERItem oidApplePolicyMobileStore;    /* 1.2.840.113635.100.5.12  Apple mobile store signing certificate policy */
extern const DERItem oidApplePolicyMobileStoreProdQA;    /* 1.2.840.113635.100.5.12.1  Apple mobile store signing certificate policy, QA */
extern const DERItem oidAppleInstallerPackagingSigningExternal;    /* 1.2.840.113635.100.6.1.16  Apple Passbook card issuer leaf marker */
extern const DERItem oidAppleTVOSApplicationSigningProd;    /* 1.2.840.113635.100.6.1.24  Apple tvOS application signing */
extern const DERItem oidAppleTVOSApplicationSigningProdQA;    /* 1.2.840.113635.100.6.1.24.1  Apple tvOS application signing, QA */
extern const DERItem oidAppleCertExtensionAppleIDRecordValidationSigning;    /* 1.2.840.113635.100.6.25  Apple Apple ID validation record signing */
extern const DERItem oidAppleCertExtAppleServerAuthentication;    /* 1.2.840.113635.100.6.27.1  Apple Apple SSL server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationGS;    /* 1.2.840.113635.100.6.27.2  Apple GS server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationPPQProd;    /* 1.2.840.113635.100.6.27.3.2  Apple PPQ server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationPPQProdQA;    /* 1.2.840.113635.100.6.27.3.1  Apple PPQ server authentication, QA */
extern const DERItem oidAppleCertExtAppleServerAuthenticationIDSProd;    /* 1.2.840.113635.100.6.27.4.2  Apple IDS server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationIDSProdQA;    /* 1.2.840.113635.100.6.27.4.1  Apple IDS server authentication, QA */
extern const DERItem oidAppleCertExtAppleServerAuthenticationAPNProd;    /* 1.2.840.113635.100.6.27.5.2  Apple APN server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationAPNProdQA;    /* 1.2.840.113635.100.6.27.5.1  Apple APN server authentication, QA */
extern const DERItem oidAppleCertExtFMiPServerAuthProd;    /* 1.2.840.113635.100.6.27.6.2  Apple Find My iPhone server authentication */
extern const DERItem oidAppleCertExtFMiPServerAuthProdQA;    /* 1.2.840.113635.100.6.27.6.1  Apple Find My iPhone server authentication, QA */
extern const DERItem oidAppleCertExtEscrowProxyServerAuthProd;    /* 1.2.840.113635.100.6.27.7.2  Apple escrow proxy server authentication */
extern const DERItem oidAppleCertExtEscrowProxyServerAuthProdQA;    /* 1.2.840.113635.100.6.27.7.1  Apple escrow proxy server authentication, QA */
extern const DERItem oidAppleCertExtAST2DiagnosticsServerAuthProd;    /* 1.2.840.113635.100.6.27.8.2  Apple AST2 diagnostics server authentication */
extern const DERItem oidAppleCertExtAST2DiagnosticsServerAuthProdQA;    /* 1.2.840.113635.100.6.27.8.1  Apple AST2 diagnostics server authentication, QA */
extern const DERItem oidAppleCertExtHomeKitServerAuth;    /* 1.2.840.113635.100.6.27.9  Apple HomeKit server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationMMCSProd;    /* 1.2.840.113635.100.6.27.11.2  Apple MMCS server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationMMCSProdQA;    /* 1.2.840.113635.100.6.27.11.1  Apple MMCS server authentication, QA */
extern const DERItem oidAppleCertExtAppleServerAuthenticationiCloudSetupProd;    /* 1.2.840.113635.100.6.27.15.2  Apple iCloud setup server authentication */
extern const DERItem oidAppleCertExtAppleServerAuthenticationiCloudSetupProdQA;    /* 1.2.840.113635.100.6.27.15.1  Apple iCloud setup server authentication, QA */
extern const DERItem oidAppleCertExtAppleSMPEncryption;    /* 1.2.840.113635.100.6.30  Apple SMP encryption */
extern const DERItem oidAppleCertExtApplePPQSigningProdQA;    /* 1.2.840.113635.100.6.38.1  Apple PPQ signing, QA */
extern const DERItem oidAppleCertExtApplePPQSigningProd;    /* 1.2.840.113635.100.6.38.2  Apple PPQ signing */
extern const DERItem oidAppleCertExtCryptoServicesExtEncryption;    /* 1.2.840.113635.100.6.39  Apple Apple Pay issuer encryption */
extern const DERItem oidAppleCertExtATVVPNProfileSigning;    /* 1.2.840.113635.100.6.43  Apple Apple TV VPN profile signing */

__END_DECLS

#endif /* _LIB_DER_OIDS_H_ */
