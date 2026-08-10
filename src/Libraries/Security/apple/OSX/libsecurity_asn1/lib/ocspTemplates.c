/*
 * Copyright (c) 2003-2006,2008-2012 Apple Inc. All Rights Reserved.
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
 * ocspTemplates.cpp -  ASN1 templates OCSP requests and responses.
 */

#include "ocspTemplates.h"
#include "keyTemplates.h"		/* for kSecAsn1AlgorithmIDTemplate */
#include "SecAsn1Templates.h"
#include <stddef.h>

// MARK: ----- OCSP Request -----

const SecAsn1Template kSecAsn1OCSPCertIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPCertID) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPCertID, algId),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING, offsetof(SecAsn1OCSPCertID, issuerNameHash) },
    { SEC_ASN1_OCTET_STRING, offsetof(SecAsn1OCSPCertID, issuerPubKeyHash) },
	/* serial number is SIGNED integer */
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
	  offsetof(SecAsn1OCSPCertID, serialNumber) },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPRequest) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPRequest, reqCert),
	  kSecAsn1OCSPCertIDTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPRequest, extensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPSignatureTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPSignature) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPSignature, algId),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(SecAsn1OCSPSignature, sig) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPSignature, certs),
	  kSecAsn1SequenceOfAnyTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPTbsRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPTbsRequest) },
	/* optional version, explicit tag 0, default 0 */
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(SecAsn1OCSPTbsRequest, version),
	  kSecAsn1PointerToIntegerTemplate },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 1, 
	  offsetof(SecAsn1OCSPTbsRequest, requestorName),
	  kSecAsn1GeneralNameTemplate },
	{ SEC_ASN1_SEQUENCE_OF, 
	  offsetof(SecAsn1OCSPTbsRequest, requestList),
	  kSecAsn1OCSPRequestTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 2,
	  offsetof(SecAsn1OCSPTbsRequest, requestExtensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPSignedRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPSignedRequest) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPSignedRequest, tbsRequest),
	  kSecAsn1OCSPTbsRequestTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
			SEC_ASN1_POINTER | SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPSignedRequest, signature),
	  kSecAsn1OCSPSignatureTemplate },
    { 0 }
};

// MARK: ----- OCSP Response -----

const SecAsn1Template kSecAsn1OCSPRevokedInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPRevokedInfo) },
	{ SEC_ASN1_GENERALIZED_TIME, offsetof(SecAsn1OCSPRevokedInfo, revocationTime) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(SecAsn1OCSPRevokedInfo, revocationReason) ,
	  kSecAsn1PointerToEnumeratedTemplate },
    { 0 }
};

/* three context-specific templates, app picks one of these */

/* 
 * Encode/decode CertStatus separately using one of these †hree templates. 
 * The result goes into SecAsn1OCSPSingleResponse.certStatus on encode. 
 */
const SecAsn1Template kSecAsn1OCSPCertStatusGoodTemplate[] = {
	{ SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SecAsn1OCSPCertStatus, nullData),
	  kSecAsn1NullTemplate }
};
	
const SecAsn1Template kSecAsn1OCSPCertStatusRevokedTemplate[] = {
	{ SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 1, 
	  offsetof(SecAsn1OCSPCertStatus, revokedInfo) ,
	  kSecAsn1OCSPRevokedInfoTemplate }
};

const SecAsn1Template kSecAsn1OCSPCertStatusUnknownTemplate[] = {
	{ SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(SecAsn1OCSPCertStatus, nullData),
	  kSecAsn1NullTemplate }
};

const SecAsn1Template kSecAsn1OCSPSingleResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPSingleResponse) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPSingleResponse, certID),
	  kSecAsn1OCSPCertIDTemplate },
    { SEC_ASN1_ANY,
	  offsetof(SecAsn1OCSPSingleResponse, certStatus),
	  kSecAsn1AnyTemplate },
	{ SEC_ASN1_GENERALIZED_TIME, offsetof(SecAsn1OCSPSingleResponse, thisUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPSingleResponse, nextUpdate),
	  kSecAsn1PointerToGeneralizedTimeTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
	  SEC_ASN1_EXPLICIT | 1,
	  offsetof(SecAsn1OCSPSingleResponse, singleExtensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * support for ResponderID CHOICE
 */
const SecAsn1Template kSecAsn1OCSPResponderIDAsNameTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	offsetof(SecAsn1OCSPResponderID, byName),
	kSecAsn1AnyTemplate }
};

const SecAsn1Template kSecAsn1OCSPResponderIDAsKeyTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	offsetof(SecAsn1OCSPResponderID, byKey),
	kSecAsn1OctetStringTemplate }
};

const SecAsn1Template kSecAsn1OCSPResponseDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPResponseData) },
	/* optional version, explicit tag 0, default 0 */
    { SEC_ASN1_EXPLICIT | SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	  SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(SecAsn1OCSPResponseData, version),
	  kSecAsn1PointerToIntegerTemplate },
	{ SEC_ASN1_ANY, 
	  offsetof(SecAsn1OCSPResponseData, responderID),
	  kSecAsn1AnyTemplate },
	{ SEC_ASN1_GENERALIZED_TIME, offsetof(SecAsn1OCSPResponseData, producedAt) },
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(SecAsn1OCSPResponseData, responses), 
	  kSecAsn1OCSPSingleResponseTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 1,
	  offsetof(SecAsn1OCSPResponseData, responseExtensions),
	  kSecAsn1SequenceOfCertExtensionTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPBasicResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPBasicResponse) },
    { SEC_ASN1_ANY,  offsetof(SecAsn1OCSPBasicResponse, tbsResponseData) },
    { SEC_ASN1_INLINE,
	  offsetof(SecAsn1OCSPBasicResponse, algId),
	  kSecAsn1AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(SecAsn1OCSPBasicResponse, sig) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPBasicResponse, certs),
	  kSecAsn1SequenceOfAnyTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPResponseBytesTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPResponseBytes) },
	{ SEC_ASN1_OBJECT_ID, offsetof(SecAsn1OCSPResponseBytes, responseType) },
	{ SEC_ASN1_OCTET_STRING, offsetof(SecAsn1OCSPResponseBytes, response) },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPPtrToResponseBytesTemplate[] = {
	{ SEC_ASN1_POINTER, 0, kSecAsn1OCSPResponseBytesTemplate }
};

const SecAsn1Template kSecAsn1OCSPResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPResponse) },
	{ SEC_ASN1_ENUMERATED, offsetof(SecAsn1OCSPResponse, responseStatus) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPResponse, responseBytes),
	  kSecAsn1OCSPPtrToResponseBytesTemplate },
    { 0 }
};

// MARK: ---- OCSPD RPC ----

const SecAsn1Template kSecAsn1OCSPDRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPDRequest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 0,
	  offsetof(SecAsn1OCSPDRequest, cacheWriteDisable),
	  kSecAsn1PointerToBooleanTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 1,
	  offsetof(SecAsn1OCSPDRequest, cacheReadDisable),
	  kSecAsn1PointerToBooleanTemplate },
    { SEC_ASN1_OCTET_STRING,  offsetof(SecAsn1OCSPDRequest, certID) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 2,
	  offsetof(SecAsn1OCSPDRequest, ocspReq),
	  kSecAsn1PointerToOctetStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 3,
	  offsetof(SecAsn1OCSPDRequest, localRespURI),
	  kSecAsn1PointerToIA5StringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 
			SEC_ASN1_EXPLICIT | 4,
	  offsetof(SecAsn1OCSPDRequest, urls),
	  kSecAsn1SequenceOfIA5StringTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPDRequestsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPDRequests) },
	{ SEC_ASN1_INTEGER, offsetof(SecAsn1OCSPDRequests, version) },
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(SecAsn1OCSPDRequests, requests), 
	  kSecAsn1OCSPDRequestTemplate },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPDReplyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPDReply) },
	{ SEC_ASN1_ANY, offsetof(SecAsn1OCSPDReply, certID) },
	{ SEC_ASN1_ANY, offsetof(SecAsn1OCSPDReply, ocspResp) },
    { 0 }
};

const SecAsn1Template kSecAsn1OCSPDRepliesTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SecAsn1OCSPReplies) },
	{ SEC_ASN1_INTEGER, offsetof(SecAsn1OCSPReplies, version) },
    { SEC_ASN1_SEQUENCE_OF, 
	  offsetof(SecAsn1OCSPReplies, replies), 
	  kSecAsn1OCSPDReplyTemplate },
    { 0 }
};
