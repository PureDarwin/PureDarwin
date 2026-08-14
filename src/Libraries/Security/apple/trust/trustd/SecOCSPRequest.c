/*
 * Copyright (c) 2008-2009,2012,2014 Apple Inc. All Rights Reserved.
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
 * SecOCSPRequest.c - Trust policies dealing with certificate revocation.
 */

#include "trust/trustd/SecOCSPRequest.h"
#include <Security/SecCertificateInternal.h>
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsalg.h>
#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>
#include "SecInternal.h"

/*
   OCSPRequest     ::=     SEQUENCE {
       tbsRequest                  TBSRequest,
       optionalSignature   [0]     EXPLICIT Signature OPTIONAL }

   TBSRequest      ::=     SEQUENCE {
       version             [0]     EXPLICIT Version DEFAULT v1,
       requestorName       [1]     EXPLICIT GeneralName OPTIONAL,
       requestList                 SEQUENCE OF Request,
       requestExtensions   [2]     EXPLICIT Extensions OPTIONAL }

   Signature       ::=     SEQUENCE {
       signatureAlgorithm      AlgorithmIdentifier,
       signature               BIT STRING,
       certs               [0] EXPLICIT SEQUENCE OF Certificate
   OPTIONAL}

   Version         ::=             INTEGER  {  v1(0) }

   Request         ::=     SEQUENCE {
       reqCert                     CertID,
       singleRequestExtensions     [0] EXPLICIT Extensions OPTIONAL }

   CertID          ::=     SEQUENCE {
       hashAlgorithm       AlgorithmIdentifier,
       issuerNameHash      OCTET STRING, -- Hash of Issuer's DN
       issuerKeyHash       OCTET STRING, -- Hash of Issuers public key
       serialNumber        CertificateSerialNumber }
 */
static CFDataRef _SecOCSPRequestCopyDEREncoding(SecOCSPRequestRef this) {
	/* fields obtained from issuer */
	SecAsn1OCSPSignedRequest	signedReq = {};
	SecAsn1OCSPTbsRequest		*tbs = &signedReq.tbsRequest;
	SecAsn1OCSPRequest			singleReq = {};
	SecAsn1OCSPCertID			*certId = &singleReq.reqCert;
	SecAsn1OCSPRequest			*reqArray[2] = { &singleReq, NULL };
	uint8_t						version = 0;
	SecAsn1Item					vers = {1, &version};
    CFDataRef                   der = NULL;
    SecAsn1CoderRef             coder = NULL;
    CFDataRef                   issuerNameDigest;
    CFDataRef                   serial;
    CFDataRef                   issuerPubKeyDigest;


    /* algId refers to the hash we'll perform in issuer name and key */
    certId->algId.algorithm = CSSMOID_SHA1;
    /* preencoded DER NULL */
    static uint8_t nullParam[2] = {5, 0};
	certId->algId.parameters.Data = nullParam;
	certId->algId.parameters.Length = sizeof(nullParam);

    /* @@@ Change this from using SecCertificateCopyIssuerSHA1Digest() /
       SecCertificateCopyPublicKeySHA1Digest() to
       SecCertificateCopyIssuerSequence() / SecCertificateGetPublicKeyData()
       and call SecDigestCreate here instead. */
    issuerNameDigest = SecCertificateCopyIssuerSHA1Digest(this->certificate);
    issuerPubKeyDigest = SecCertificateCopyPublicKeySHA1Digest(this->issuer);
    serial = SecCertificateCopySerialNumberData(this->certificate, NULL);

	/* build the CertID from those components */
	certId->issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId->issuerNameHash.Data = (uint8_t *)CFDataGetBytePtr(issuerNameDigest);
	certId->issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId->issuerPubKeyHash.Data = (uint8_t *)CFDataGetBytePtr(issuerPubKeyDigest);
	certId->serialNumber.Length = CFDataGetLength(serial);
	certId->serialNumber.Data = (uint8_t *)CFDataGetBytePtr(serial);

	/* Build top level request with one entry in requestList, no signature,
       and no optional extensions. */
	tbs->version = &vers;
	tbs->requestList = reqArray;

	/* Encode the request. */
    require_noerr(SecAsn1CoderCreate(&coder), errOut);
    SecAsn1Item encoded;
	require_noerr(SecAsn1EncodeItem(coder, &signedReq,
        kSecAsn1OCSPSignedRequestTemplate, &encoded), errOut);
    der = CFDataCreate(kCFAllocatorDefault, encoded.Data,
        encoded.Length);

errOut:
    if (coder)
        SecAsn1CoderRelease(coder);
    CFReleaseSafe(issuerNameDigest);
    CFReleaseSafe(serial);
    CFReleaseSafe(issuerPubKeyDigest);

    return der;
}

SecOCSPRequestRef SecOCSPRequestCreate(SecCertificateRef certificate,
    SecCertificateRef issuer) {
    SecOCSPRequestRef this;
    require(this = (SecOCSPRequestRef)calloc(1, sizeof(struct __SecOCSPRequest)),
        errOut);
    this->certificate = certificate;
    this->issuer = issuer;

    return this;
errOut:
    if (this) {
        SecOCSPRequestFinalize(this);
    }
    return NULL;
}

CFDataRef SecOCSPRequestGetDER(SecOCSPRequestRef this) {
    if (!this) { return NULL; }
    CFDataRef der = this->der;
    if (!der) {
        this->der = der = _SecOCSPRequestCopyDEREncoding(this);
    }
    return der;
}

void SecOCSPRequestFinalize(SecOCSPRequestRef this) {
    CFReleaseSafe(this->der);
    free(this);
}

