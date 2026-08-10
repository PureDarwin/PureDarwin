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
 * ocspTemplates.h -  ASN1 templates OCSP requests and responses.
 */

#ifndef	_OCSP_TEMPLATES_H_
#define _OCSP_TEMPLATES_H_

#include <Security/X509Templates.h> /* NSS_CertExtension */
#include <Security/nameTemplates.h> /* NSS_GeneralName and support */

#ifdef  __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// MARK: ----- OCSP Request -----

/*
 * CertID          ::=     SEQUENCE {
 *		hashAlgorithm		AlgorithmIdentifier,
 *		issuerNameHash		OCTET STRING, -- Hash of Issuer's DN
 *		issuerKeyHash		OCTET STRING, -- Hash of Issuers public key
 *		serialNumber		CertificateSerialNumber }   -- i.e., INTEGER
 */
typedef struct {
	SecAsn1AlgId		algId;
	SecAsn1Item							issuerNameHash;
	SecAsn1Item							issuerPubKeyHash;
	SecAsn1Item							serialNumber;
} SecAsn1OCSPCertID;

extern const SecAsn1Template kSecAsn1OCSPCertIDTemplate[];
 
/*
 * Request         ::=     SEQUENCE {
 *		reqCert                     CertID,
 *		singleRequestExtensions     [0] EXPLICIT Extensions OPTIONAL }  
 */
typedef struct {
	SecAsn1OCSPCertID					reqCert;
    NSS_CertExtension 					**extensions;		// optional	
} SecAsn1OCSPRequest;

extern const SecAsn1Template kSecAsn1OCSPRequestTemplate[];

/*
 * Signature       ::=     SEQUENCE {
 *		signatureAlgorithm      AlgorithmIdentifier,
 *		signature               BIT STRING,
 *		certs               [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL}
 *
 * Since we wish to avoid knowing anything about the details of the certs, 
 * we declare them here as ASN_ANY, get/set as raw data, and leave it to 
 * the CL to parse them.
 */
typedef struct {
	SecAsn1AlgId		algId;
	SecAsn1Item							sig;		// length in BITS
	SecAsn1Item							**certs;	// OPTIONAL
} SecAsn1OCSPSignature;

extern const SecAsn1Template kSecAsn1OCSPSignatureTemplate[];

/* 
 * TBSRequest      ::=     SEQUENCE {
 *		version             [0]     EXPLICIT Version DEFAULT v1,
 *		requestorName       [1]     EXPLICIT GeneralName OPTIONAL,
 *		requestList                 SEQUENCE OF Request,
 *		requestExtensions   [2]     EXPLICIT Extensions OPTIONAL } 
 */
typedef struct {
	SecAsn1Item							*version;				// OPTIONAL
	NSS_GeneralName						*requestorName;			// OPTIONAL
	SecAsn1OCSPRequest					**requestList;
    NSS_CertExtension 					**requestExtensions;	// OPTIONAL
} SecAsn1OCSPTbsRequest;

extern const SecAsn1Template kSecAsn1OCSPTbsRequestTemplate[];

/* 
 * OCSPRequest     ::=     SEQUENCE {
 *		tbsRequest                  TBSRequest,
 *		optionalSignature   [0]     EXPLICIT Signature OPTIONAL }
 */
typedef struct {
	SecAsn1OCSPTbsRequest				tbsRequest;
	SecAsn1OCSPSignature				*signature;			// OPTIONAL
} SecAsn1OCSPSignedRequest;

extern const SecAsn1Template kSecAsn1OCSPSignedRequestTemplate[];

// MARK: ----- OCSP Response -----

/*
 * CertStatus ::= CHOICE {
 *		good        [0]     IMPLICIT NULL,
 *		revoked     [1]     IMPLICIT RevokedInfo,
 *		unknown     [2]     IMPLICIT UnknownInfo }
 *
 * RevokedInfo ::= SEQUENCE {
 *		revocationTime              GeneralizedTime,
 *		revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }
 *
 * UnknownInfo ::= NULL -- this can be replaced with an enumeration
 *
 * See <Security/certextensions.h> for enum values of CE_CrlReason.
 */
typedef struct {
	SecAsn1Item					revocationTime;
	SecAsn1Item					*revocationReason;		// OPTIONAL, CE_CrlReason
} SecAsn1OCSPRevokedInfo;

typedef union {
	SecAsn1OCSPRevokedInfo		*revokedInfo;
	SecAsn1Item					*nullData;
} SecAsn1OCSPCertStatus;

typedef enum {
	CS_Good = 0,
	CS_Revoked = 1,
	CS_Unknown = 2,
	CS_NotParsed = 0xff		/* Not in protocol: means value not parsed or seen */
} SecAsn1OCSPCertStatusTag;

extern const SecAsn1Template kSecAsn1OCSPRevokedInfoTemplate[];

/* 
 * Encode/decode CertStatus separately using one of these  hree templates. 
 * The result goes into SecAsn1OCSPSingleResponse.certStatus on encode. 
 */
extern const SecAsn1Template kSecAsn1OCSPCertStatusGoodTemplate[];
extern const SecAsn1Template kSecAsn1OCSPCertStatusRevokedTemplate[];
extern const SecAsn1Template kSecAsn1OCSPCertStatusUnknownTemplate[];

/* 
 * SingleResponse ::= SEQUENCE {
 *		certID                       CertID,
 *		certStatus                   CertStatus,
 *		thisUpdate                   GeneralizedTime,
 *		nextUpdate         [0]       EXPLICIT GeneralizedTime OPTIONAL,
 *		singleExtensions   [1]       EXPLICIT Extensions OPTIONAL }
 */
typedef struct {
	SecAsn1OCSPCertID			certID;
	SecAsn1Item					certStatus;				// ASN_ANY here
	SecAsn1Item					thisUpdate;				// GeneralizedTime
	SecAsn1Item					*nextUpdate;			// GeneralizedTime, OPTIONAL
    NSS_CertExtension 			**singleExtensions;		// OPTIONAL
} SecAsn1OCSPSingleResponse;

extern const SecAsn1Template kSecAsn1OCSPSingleResponseTemplate[];

/*
 * ResponderID ::= CHOICE {
 *     byName               EXPLICIT [1] Name,
 *     byKey                EXPLICIT [2] KeyHash }
 *
 * Since our ASN.1 encoder/decoder can't handle CHOICEs very well, we encode
 * this separately using one of the following two templates. On encode the
 * result if this step of the encode goes into SecAsn1OCSPResponseData.responderID,
 * where it's treated as an ANY_ANY when encoding that struct. The reverse happens
 * on decode. 
 */
typedef union {
	SecAsn1Item					byName;
	SecAsn1Item					byKey;		// key hash in OCTET STRING
} SecAsn1OCSPResponderID;

typedef enum {
	RIT_Name	= 1,
	RIT_Key		= 2
} SecAsn1OCSPResponderIDTag;

extern const SecAsn1Template kSecAsn1OCSPResponderIDAsNameTemplate[];
extern const SecAsn1Template kSecAsn1OCSPResponderIDAsKeyTemplate[];

/* 
 * ResponseData ::= SEQUENCE {
 *		version              [0] EXPLICIT Version DEFAULT v1,
 *		responderID              ResponderID,
 *		producedAt               GeneralizedTime,
 *		responses                SEQUENCE OF SingleResponse,
 *		responseExtensions   [1] EXPLICIT Extensions OPTIONAL }
 */
typedef struct {
	SecAsn1Item					*version;		// OPTIONAL
	SecAsn1Item					responderID;	// ASN_ANY here, decode/encode separately
	SecAsn1Item					producedAt;		// GeneralizedTime
	SecAsn1OCSPSingleResponse   **responses;
    NSS_CertExtension 			**responseExtensions;	// OPTIONAL
} SecAsn1OCSPResponseData;

extern const SecAsn1Template kSecAsn1OCSPResponseDataTemplate[];

/* 
 * BasicOCSPResponse       ::= SEQUENCE {
 *		tbsResponseData      ResponseData,
 *		signatureAlgorithm   AlgorithmIdentifier,
 *		signature            BIT STRING,
 *		certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
 *
 * Since we ALWAYS encode the tbsResponseData in preparation for signing,
 * we declare it as a raw ASN_ANY in the BasicOCSPResponse.
 *
 * Certs are likewise ASN_ANY since we use the CL to parse and create them. 
 */
typedef struct {
	SecAsn1Item						tbsResponseData;
	SecAsn1AlgId	algId;
	SecAsn1Item						sig;		// length in BITS
	SecAsn1Item						**certs;	// optional
} SecAsn1OCSPBasicResponse;

extern const SecAsn1Template kSecAsn1OCSPBasicResponseTemplate[];

/*
 * ResponseBytes ::=       SEQUENCE {
 *		responseType   OBJECT IDENTIFIER,
 *		response       OCTET STRING }
 *
 * The contents of response are actually an encoded SecAsn1OCSPBasicResponse (at 
 * least until another response type is defined). 
 */
typedef struct {
	SecAsn1Oid					responseType;
	SecAsn1Item					response;
} SecAsn1OCSPResponseBytes;

extern const SecAsn1Template kSecAsn1OCSPResponseBytesTemplate[];

/* 
 * OCSPResponse ::= SEQUENCE {
 *		responseStatus         OCSPResponseStatus,		-- an ENUM
 *		responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
 */
typedef struct {
	SecAsn1Item					responseStatus;		// see enum below
	SecAsn1OCSPResponseBytes	*responseBytes;		// optional
} SecAsn1OCSPResponse;

extern const SecAsn1Template kSecAsn1OCSPResponseTemplate[];

typedef enum {
	RS_Success = 0,
	RS_MalformedRequest = 1,
	RS_InternalError = 2,
	RS_TryLater = 3,
	RS_Unused = 4,
	RS_SigRequired = 5,
	RS_Unauthorized = 6
} SecAsn1OCSPResponseStatus;

/* 
 * This is not part of the OCSP protocol; it's used in the communication between
 * the Apple X.509 TP module and the ocspd server.
 *
 * OCSPDRequest ::= SEQUENCE {
 *		cacheWriteDisable :: = EXPLICIT [0] BOOL OPTIONAL;	-- cache write disable
 *															--  default FALSE
 *		cacheWriteDisable :: = EXPLICIT [1] BOOL OPTIONAL;	-- cache read disable
 *															--  default FALSE
 *		certID		::= OCTET STRING;						-- for cache lookup
 *		ocspReq		::= EXPLICIT [2] OCTET STRING OPTIONAL;	-- for net fetch
 *		localResp	::= EXPLICIT [3] IA5String OPTIONAL;	-- for local responder
 *		urls		::= EXPLICIT [4] SEQUENCE of IA5String OPTIONAL;
 *															-- for normal net fetch
 * };
 */

#define OCSPD_REQUEST_VERS	0

typedef struct {
	SecAsn1Item			*cacheWriteDisable;
	SecAsn1Item			*cacheReadDisable;
	SecAsn1Item			certID;				// DER encoded SecAsn1OCSPCertID
	SecAsn1Item			*ocspReq;			// DER encoded SecAsn1OCSPSignedRequest
	SecAsn1Item			*localRespURI;		// local responder URI
	SecAsn1Item			**urls;				// normal URIs
	
} SecAsn1OCSPDRequest;

/* 
 * And this is a sequence of them, packaged up and sent to ocspd in one RPC.
 */
typedef struct {
	SecAsn1Item			version;			// OCSPD_REQUEST_VERS
	SecAsn1OCSPDRequest	**requests;
} SecAsn1OCSPDRequests;

extern const SecAsn1Template kSecAsn1OCSPDRequestTemplate[];
extern const SecAsn1Template kSecAsn1OCSPDRequestsTemplate[];

/* 
 * Unordered set of replies from ocsdp; they map back to individual
 * SecAsn1OCSPDRequests by the encoded certID (which is obtained from the 
 * SecAsn1OCSPDRequest, NOT from the OCSP response).
 */
typedef struct {
	SecAsn1Item			certID;			// DER encoded SecAsn1OCSPCertID
	SecAsn1Item			ocspResp;		// DER encoded SecAsn1OCSPResponse
} SecAsn1OCSPDReply;

#define OCSPD_REPLY_VERS	0

typedef struct {
	SecAsn1Item			version;			// OCSPD_REPLY_VERS
	SecAsn1OCSPDReply	**replies; 
} SecAsn1OCSPReplies;

extern const SecAsn1Template kSecAsn1OCSPDReplyTemplate[];
extern const SecAsn1Template kSecAsn1OCSPDRepliesTemplate[];

#pragma clang diagnostic pop

#ifdef  __cplusplus
}
#endif

#endif	/* _OCSP_TEMPLATES_H_ */
