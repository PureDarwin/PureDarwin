/*
 * Copyright (c) 2008-2009,2012-2018 Apple Inc. All Rights Reserved.
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
 * SecOCSPResponse.c - Wrapper to decode ocsp responses.
 */

#include "trust/trustd/SecOCSPResponse.h"

#include <asl.h>
#include <AssertMacros.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecFramework.h>
#include <Security/SecKeyPriv.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/SecAsn1Templates.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsocsp.h>
#include <stdlib.h>
#include "SecInternal.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecSCTUtils.h>


#define ocspdErrorLog(args, ...)     secerror(args, ## __VA_ARGS__)
#define ocspdHttpDebug(args...)     secdebug("ocspdHttp", ## args)
#define ocspdDebug(args...)     secdebug("ocsp", ## args)


/*
   OCSPResponse ::= SEQUENCE {
      responseStatus         OCSPResponseStatus,
      responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }

   OCSPResponseStatus ::= ENUMERATED {
       successful            (0),  --Response has valid confirmations
       malformedRequest      (1),  --Illegal confirmation request
       internalError         (2),  --Internal error in issuer
       tryLater              (3),  --Try again later
                                   --(4) is not used
       sigRequired           (5),  --Must sign the request
       unauthorized          (6)   --Request unauthorized
   }

   ResponseBytes ::=       SEQUENCE {
       responseType   OBJECT IDENTIFIER,
       response       OCTET STRING }

   id-pkix-ocsp           OBJECT IDENTIFIER ::= { id-ad-ocsp }
   id-pkix-ocsp-basic     OBJECT IDENTIFIER ::= { id-pkix-ocsp 1 }

   The value for response SHALL be the DER encoding of
   BasicOCSPResponse.

   BasicOCSPResponse       ::= SEQUENCE {
      tbsResponseData      ResponseData,
      signatureAlgorithm   AlgorithmIdentifier,
      signature            BIT STRING,
      certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }

   The value for signature SHALL be computed on the hash of the DER
   encoding ResponseData.

   ResponseData ::= SEQUENCE {
      version              [0] EXPLICIT Version DEFAULT v1,
      responderID              ResponderID,
      producedAt               GeneralizedTime,
      responses                SEQUENCE OF SingleResponse,
      responseExtensions   [1] EXPLICIT Extensions OPTIONAL }

   ResponderID ::= CHOICE {
      byName               [1] Name,
      byKey                [2] KeyHash }

   KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
   (excluding the tag and length fields)

   SingleResponse ::= SEQUENCE {
      certID                       CertID,
      certStatus                   CertStatus,
      thisUpdate                   GeneralizedTime,
      nextUpdate         [0]       EXPLICIT GeneralizedTime OPTIONAL,
      singleExtensions   [1]       EXPLICIT Extensions OPTIONAL }

   CertStatus ::= CHOICE {
       good        [0]     IMPLICIT NULL,
       revoked     [1]     IMPLICIT RevokedInfo,
       unknown     [2]     IMPLICIT UnknownInfo }

   RevokedInfo ::= SEQUENCE {
       revocationTime              GeneralizedTime,
       revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }

   UnknownInfo ::= NULL -- this can be replaced with an enumeration
*/

static CFAbsoluteTime genTimeToCFAbsTime(const SecAsn1Item *datetime)
{
    return SecAbsoluteTimeFromDateContent(SEC_ASN1_GENERALIZED_TIME,
        datetime->Data, datetime->Length);
}

void SecOCSPSingleResponseDestroy(SecOCSPSingleResponseRef this) {
    CFReleaseSafe(this->scts);
    free(this);
}

static SecOCSPSingleResponseRef SecOCSPSingleResponseCreate(
    SecAsn1OCSPSingleResponse *resp, SecAsn1CoderRef coder) {
	assert(resp != NULL);
    SecOCSPSingleResponseRef this;
    require(this = (SecOCSPSingleResponseRef)
        calloc(1, sizeof(struct __SecOCSPSingleResponse)), errOut);
    this->certStatus = CS_NotParsed;
	this->thisUpdate = NULL_TIME;
	this->nextUpdate = NULL_TIME;
	this->revokedTime = NULL_TIME;
	this->crlReason = kSecRevocationReasonUndetermined;
    this->scts = NULL;

	if ((resp->certStatus.Data == NULL) || (resp->certStatus.Length == 0)) {
		ocspdErrorLog("OCSPSingleResponse: bad certStatus");
        goto errOut;
	}
	this->certStatus = (SecAsn1OCSPCertStatusTag)(resp->certStatus.Data[0] & SEC_ASN1_TAGNUM_MASK);
	if (this->certStatus == CS_Revoked) {
		/* Decode further to get SecAsn1OCSPRevokedInfo */
		SecAsn1OCSPCertStatus certStatus;
		memset(&certStatus, 0, sizeof(certStatus));
		if (SecAsn1DecodeData(coder, &resp->certStatus,
				kSecAsn1OCSPCertStatusRevokedTemplate, &certStatus)) {
			ocspdErrorLog("OCSPSingleResponse: err decoding certStatus");
            goto errOut;
		}
		SecAsn1OCSPRevokedInfo *revokedInfo = certStatus.revokedInfo;
		if (revokedInfo != NULL) {
			/* Treat this as optional even for CS_Revoked */
			this->revokedTime = genTimeToCFAbsTime(&revokedInfo->revocationTime);
			const SecAsn1Item *revReason = revokedInfo->revocationReason;
			if((revReason != NULL) &&
			   (revReason->Data != NULL) &&
			   (revReason->Length != 0)) {
			   this->crlReason = revReason->Data[0];
			}
		}
	}
    this->thisUpdate = genTimeToCFAbsTime(&resp->thisUpdate);
    if (this->thisUpdate == NULL_TIME) {
		ocspdErrorLog("OCSPResponse: bad thisUpdate DER");
        goto errOut;
    }

	if (resp->nextUpdate != NULL) {
		this->nextUpdate = genTimeToCFAbsTime(resp->nextUpdate);
        if (this->nextUpdate == NULL_TIME) {
            ocspdErrorLog("OCSPResponse: bad nextUpdate DER");
            goto errOut;
        }
	}

    /* Lookup through extensions to find SCTs */
    if(resp->singleExtensions) {
        ocspdErrorLog("OCSPResponse: single response has extension(s).");
        int i = 0;
        NSS_CertExtension *extn;
        while ((extn = resp->singleExtensions[i])) {
            if(SecAsn1OidCompare(&extn->extnId, &OID_GOOGLE_OCSP_SCT )) {
                ocspdErrorLog("OCSPResponse: single response has an SCT extension.");
                SecAsn1Item sct_data = {0,};

                // Note: if there are more that one valid SCT extension, we just use the first one that successfully decoded
                if((this->scts == NULL) && (SecAsn1DecodeData(coder, &extn->value, kSecAsn1OctetStringTemplate, &sct_data) == 0)) {
                    this->scts = SecCreateSignedCertificateTimestampsArrayFromSerializedSCTList(sct_data.Data, sct_data.Length);
                    ocspdErrorLog("OCSPResponse: single response has an SCT extension, parsed = %p.", this->scts);
                }
            }
            i++;
        }
    }

	ocspdDebug("status %d reason %d", (int)this->certStatus,
		(int)this->crlReason);
    return this;
errOut:
    if (this)
        SecOCSPSingleResponseDestroy(this);
    return NULL;
}

/* Calculate temporal validity; set latestNextUpdate and expireTime.
   Returns true if valid, else returns false. */
bool SecOCSPResponseCalculateValidity(SecOCSPResponseRef this,
    CFTimeInterval maxAge, CFTimeInterval defaultTTL, CFAbsoluteTime verifyTime)
{
    bool ok = false;
	this->latestNextUpdate = NULL_TIME;

    if (this->producedAt > verifyTime + TRUST_TIME_LEEWAY) {
        secnotice("ocsp", "OCSPResponse: producedAt more than 1:15 from now");
        goto exit;
    }

    /* Make this->latestNextUpdate be the date farthest in the future
       of any of the singleResponses nextUpdate fields. */
    SecAsn1OCSPSingleResponse **responses;
    for (responses = this->responseData.responses; *responses; ++responses) {
		SecAsn1OCSPSingleResponse *resp = *responses;

		/* thisUpdate later than 'now' invalidates the whole response. */
		CFAbsoluteTime thisUpdate = genTimeToCFAbsTime(&resp->thisUpdate);
		if (thisUpdate > verifyTime + TRUST_TIME_LEEWAY) {
			secnotice("ocsp","OCSPResponse: thisUpdate more than 1:15 from now");
            goto exit;
		}

		/* Keep track of latest nextUpdate. */
		if (resp->nextUpdate != NULL) {
			CFAbsoluteTime nextUpdate = genTimeToCFAbsTime(resp->nextUpdate);
			if (nextUpdate > this->latestNextUpdate) {
				this->latestNextUpdate = nextUpdate;
			}
		}
        else {
            /* RFC 5019 section 2.2.4 states on nextUpdate:
                 Responders MUST always include this value to aid in
                 response caching.  See Section 6 for additional
                 information on caching.
            */
			secnotice("ocsp", "OCSPResponse: nextUpdate not present");
#ifdef STRICT_RFC5019
            goto exit;
#endif
        }
	}

    /* Now that we have this->latestNextUpdate, we figure out the latest
       date at which we will expire this response from our cache.  To comply
       with rfc5019s:

6.1.  Caching at the Client

   To minimize bandwidth usage, clients MUST locally cache authoritative
   OCSP responses (i.e., a response with a signature that has been
   successfully validated and that indicate an OCSPResponseStatus of
   'successful').

   Most OCSP clients will send OCSPRequests at or near the nextUpdate
   time (when a cached response expires).  To avoid large spikes in
   responder load that might occur when many clients refresh cached
   responses for a popular certificate, responders MAY indicate when the
   client should fetch an updated OCSP response by using the cache-
   control:max-age directive.  Clients SHOULD fetch the updated OCSP
   Response on or after the max-age time.  To ensure that clients
   receive an updated OCSP response, OCSP responders MUST refresh the
   OCSP response before the max-age time.

6.2 [...]

       we need to take the cache-control:max-age directive into account.

       The way the code below is written we ignore a max-age=0 in the
       http header.  Since a value of 0 (NULL_TIME) also means there
       was no max-age in the header. This seems ok since that would imply
       no-cache so we also ignore negative values for the same reason,
       instead we'll expire whenever this->latestNextUpdate tells us to,
       which is the signed value if max-age is too low, since we don't
       want to refetch multilple times for a single page load in a browser. */
	if (this->latestNextUpdate == NULL_TIME) {
        /* See comment above on RFC 5019 section 2.2.4. */
		/* Absolute expire time = current time plus defaultTTL */
		this->expireTime = verifyTime + defaultTTL;
	} else if (this->latestNextUpdate < verifyTime - TRUST_TIME_LEEWAY) {
        secnotice("ocsp", "OCSPResponse: latestNextUpdate more than 1:15 ago");
        goto exit;
    } else if (maxAge > 0) {
        /* Beware of double overflows such as:

               now + maxAge < this->latestNextUpdate

           in the math below since an attacker could create any positive
           value for maxAge. */
        if (maxAge < this->latestNextUpdate - verifyTime) {
            /* maxAge header wants us to expire the cache entry sooner than
               nextUpdate would allow, to balance server load. */
            this->expireTime = verifyTime + maxAge;
        } else {
            /* maxAge http header attempting to make us cache the response
               longer than it's valid for, bad http header! Ignoring you. */
#ifdef DEBUG
            CFStringRef hexResp = CFDataCopyHexString(this->data);
            ocspdDebug("OCSPResponse: now + maxAge > latestNextUpdate,"
                " using latestNextUpdate %@", hexResp);
            CFReleaseSafe(hexResp);
#endif
            this->expireTime = this->latestNextUpdate;
        }
	} else {
        /* No maxAge provided, just use latestNextUpdate. */
		this->expireTime = this->latestNextUpdate;
    }

    ok = true;
exit:
	return ok;
}

SecOCSPResponseRef SecOCSPResponseCreateWithID(CFDataRef ocspResponse, int64_t responseID) {
	SecAsn1OCSPResponse topResp = {};
    SecOCSPResponseRef this = NULL;

    require(ocspResponse, errOut);
    require(this = (SecOCSPResponseRef)calloc(1, sizeof(struct __SecOCSPResponse)),
        errOut);
    require_noerr(SecAsn1CoderCreate(&this->coder), errOut);

    this->data = ocspResponse;
    this->responseID = responseID;
    CFRetain(ocspResponse);

    SecAsn1Item resp;
    resp.Length = CFDataGetLength(ocspResponse);
    resp.Data = (uint8_t *)CFDataGetBytePtr(ocspResponse);
	if (SecAsn1DecodeData(this->coder, &resp, kSecAsn1OCSPResponseTemplate,
        &topResp)) {
		ocspdErrorLog("OCSPResponse: decode failure at top level");
	}
	/* remainder is valid only on RS_Success */
	if ((topResp.responseStatus.Data == NULL) ||
	   (topResp.responseStatus.Length == 0)) {
		ocspdErrorLog("OCSPResponse: no responseStatus");
        goto errOut;
	}
    this->responseStatus = topResp.responseStatus.Data[0];
	if (this->responseStatus != kSecOCSPSuccess) {
#ifdef DEBUG
        CFStringRef hexResp = CFDataCopyHexString(this->data);
        secdebug("ocsp", "OCSPResponse: status: %d %@", this->responseStatus, hexResp);
        CFReleaseNull(hexResp);
#endif
        /* not a failure of our constructor; this object is now useful, but
		 * only for this one byte of status info */
		goto fini;
	}
	if (topResp.responseBytes == NULL) {
		/* I don't see how this can be legal on RS_Success */
		ocspdErrorLog("OCSPResponse: empty responseBytes");
        goto errOut;
	}
    if (!SecAsn1OidCompare(&topResp.responseBytes->responseType,
			&OID_PKIX_OCSP_BASIC)) {
		ocspdErrorLog("OCSPResponse: unknown responseType");
        goto errOut;

	}

	/* decode the SecAsn1OCSPBasicResponse */
	if (SecAsn1DecodeData(this->coder, &topResp.responseBytes->response,
			kSecAsn1OCSPBasicResponseTemplate, &this->basicResponse)) {
		ocspdErrorLog("OCSPResponse: decode failure at SecAsn1OCSPBasicResponse");
        goto errOut;
	}

	/* signature and cert evaluation done externally */

	/* decode the SecAsn1OCSPResponseData */
	if (SecAsn1DecodeData(this->coder, &this->basicResponse.tbsResponseData,
			kSecAsn1OCSPResponseDataTemplate, &this->responseData)) {
        ocspdErrorLog("OCSPResponse: decode failure at SecAsn1OCSPResponseData");
        goto errOut;
	}
    this->producedAt = genTimeToCFAbsTime(&this->responseData.producedAt);
    if (this->producedAt == NULL_TIME) {
		ocspdErrorLog("OCSPResponse: bad producedAt");
        goto errOut;
    }

	if (this->responseData.responderID.Data == NULL) {
		ocspdErrorLog("OCSPResponse: bad responderID");
        goto errOut;
	}

	/* Choice processing for ResponderID */
    this->responderIdTag = (SecAsn1OCSPResponderIDTag)
		(this->responseData.responderID.Data[0] & SEC_ASN1_TAGNUM_MASK);
	const SecAsn1Template *templ;
	switch(this->responderIdTag) {
		case RIT_Name:
            /* @@@ Since we don't use the decoded byName value we could skip
               decoding it but we do it anyway for validation. */
			templ = kSecAsn1OCSPResponderIDAsNameTemplate;
			break;
		case RIT_Key:
			templ = kSecAsn1OCSPResponderIDAsKeyTemplate;
			break;
		default:
			ocspdErrorLog("OCSPResponse: bad responderID tag");
            goto errOut;
	}
	if (SecAsn1DecodeData(this->coder, &this->responseData.responderID, templ,
        &this->responderID)) {
		ocspdErrorLog("OCSPResponse: decode failure at responderID");
        goto errOut;
	}

fini:
    return this;
errOut:
#ifdef DEBUG
    {
        CFStringRef hexResp = (this) ? CFDataCopyHexString(this->data) : NULL;
        secdebug("ocsp", "bad ocsp response: %@", hexResp);
        CFReleaseSafe(hexResp);
    }
#endif
    if (this) {
        SecOCSPResponseFinalize(this);
    }
    return NULL;
}

SecOCSPResponseRef SecOCSPResponseCreate(CFDataRef this) {
    return SecOCSPResponseCreateWithID(this, -1);
}

int64_t SecOCSPResponseGetID(SecOCSPResponseRef this) {
    return this->responseID;
}

CFDataRef SecOCSPResponseGetData(SecOCSPResponseRef this) {
    return this->data;
}

SecOCSPResponseStatus SecOCSPGetResponseStatus(SecOCSPResponseRef this) {
    return this->responseStatus;
}

CFAbsoluteTime SecOCSPResponseGetExpirationTime(SecOCSPResponseRef this) {
    return this->expireTime;
}

CFDataRef SecOCSPResponseGetNonce(SecOCSPResponseRef this) {
    return this->nonce;
}

CFAbsoluteTime SecOCSPResponseProducedAt(SecOCSPResponseRef this) {
    return this->producedAt;
}

CFArrayRef SecOCSPResponseCopySigners(SecOCSPResponseRef this) {
    CFMutableArrayRef result = NULL;
    result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!result) {
        return NULL;
    }
    SecAsn1Item **certs;
    for (certs = this->basicResponse.certs; certs && *certs; ++certs) {
        SecCertificateRef cert = NULL;
        cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, (*certs)->Data, (*certs)->Length);
        if (cert) {
            CFArrayAppendValue(result, cert);
            CFReleaseNull(cert);
        }
    }

    return result;
}

void SecOCSPResponseFinalize(SecOCSPResponseRef this) {
    CFReleaseSafe(this->data);
    CFReleaseSafe(this->nonce);
    SecAsn1CoderRelease(this->coder);
    free(this);
}

static CFAbsoluteTime SecOCSPSingleResponseComputedNextUpdate(SecOCSPSingleResponseRef this, CFTimeInterval defaultTTL) {
    /* rfc2560 section 2.4 states: "If nextUpdate is not set, the
     responder is indicating that newer revocation information
     is available all the time".
     Let's ensure that thisUpdate isn't more than defaultTTL in
     the past then. */
    return this->nextUpdate == NULL_TIME ? this->thisUpdate + defaultTTL : this->nextUpdate;
}

bool SecOCSPSingleResponseCalculateValidity(SecOCSPSingleResponseRef this, CFTimeInterval defaultTTL, CFAbsoluteTime verifyTime) {
    if (this->thisUpdate > verifyTime + TRUST_TIME_LEEWAY) {
        ocspdErrorLog("OCSPSingleResponse: thisUpdate more than 1:15 from now");
        return false;
    }

    CFAbsoluteTime cnu = SecOCSPSingleResponseComputedNextUpdate(this, defaultTTL);
    if (verifyTime - TRUST_TIME_LEEWAY > cnu) {
        ocspdErrorLog("OCSPSingleResponse: %s %.2f days ago", this->nextUpdate ? "nextUpdate" : "thisUpdate + defaultTTL", (verifyTime - cnu) / 86400);
        return false;
    }

    return true;
}

CFArrayRef SecOCSPSingleResponseCopySCTs(SecOCSPSingleResponseRef this)
{
    return CFRetainSafe(this->scts);
}


SecOCSPSingleResponseRef SecOCSPResponseCopySingleResponse(
    SecOCSPResponseRef this, SecOCSPRequestRef request) {
    SecOCSPSingleResponseRef sr = NULL;

    if (!request) { return sr; }
    CFDataRef issuer = SecCertificateCopyIssuerSequence(request->certificate);
    const DERItem *publicKey = SecCertificateGetPublicKeyData(request->issuer);
    CFDataRef serial = SecCertificateCopySerialNumberData(request->certificate, NULL);
    CFDataRef issuerNameHash = NULL;
    CFDataRef issuerPubKeyHash = NULL;
    SecAsn1Oid *algorithm = NULL;
    SecAsn1Item *parameters = NULL;

    SecAsn1OCSPSingleResponse **responses;
    for (responses = this->responseData.responses; *responses; ++responses) {
        SecAsn1OCSPSingleResponse *resp = *responses;
        SecAsn1OCSPCertID *certId = &resp->certID;
        /* First check the easy part, serial number should match. */
        if (!serial || certId->serialNumber.Length != (size_t)CFDataGetLength(serial) ||
            memcmp(CFDataGetBytePtr(serial), certId->serialNumber.Data,
                certId->serialNumber.Length)) {
            /* Serial # mismatch, skip this singleResponse. */
            continue;
        }

        /* Calcluate the issuerKey and issuerName digests using the
           hashAlgorithm and parameters specified in the certId, if
           they differ from the ones we already computed. */
        if (!SecAsn1OidCompare(algorithm, &certId->algId.algorithm) ||
            !SecAsn1OidCompare(parameters, &certId->algId.parameters)) {
            algorithm = &certId->algId.algorithm;
            parameters = &certId->algId.parameters;
            CFReleaseSafe(issuerNameHash);
            CFReleaseSafe(issuerPubKeyHash);
            issuerNameHash = SecDigestCreate(kCFAllocatorDefault, algorithm,
                parameters, CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
            issuerPubKeyHash = SecDigestCreate(kCFAllocatorDefault, algorithm,
                parameters, publicKey->data, publicKey->length);
        }

        if (!issuerNameHash || !issuerPubKeyHash) {
            /* This can happen when the hash algorithm is not supported, should be really rare */
            /* See also: <rdar://problem/21908655> CrashTracer: securityd at securityd: SecOCSPResponseCopySingleResponse */
            ocspdErrorLog("Unknown hash algorithm in singleResponse");
            algorithm = NULL;
            parameters = NULL;
            continue;
        }

        if (certId->issuerNameHash.Length == (size_t)CFDataGetLength(issuerNameHash)
            && !memcmp(CFDataGetBytePtr(issuerNameHash),
                certId->issuerNameHash.Data, certId->issuerNameHash.Length)
            && certId->issuerPubKeyHash.Length == (size_t)CFDataGetLength(issuerPubKeyHash)
            && !memcmp(CFDataGetBytePtr(issuerPubKeyHash),
                certId->issuerPubKeyHash.Data, certId->issuerPubKeyHash.Length)) {

            /* resp matches the certificate in request, so let's use it. */
            sr = SecOCSPSingleResponseCreate(resp, this->coder);
            if (sr) {
                ocspdDebug("found matching singleResponse");
                break;
            }
        }

    }

    CFReleaseSafe(issuerPubKeyHash);
    CFReleaseSafe(issuerNameHash);
    CFReleaseSafe(serial);
    CFReleaseSafe(issuer);

    if (!sr) {
        ocspdDebug("certID not found");
    }

	return sr;
}

static bool SecOCSPResponseVerifySignature(SecOCSPResponseRef this,
    SecKeyRef key) {
	/* Beware this->basicResponse.sig: on decode, length is in BITS */
    return SecKeyDigestAndVerify(key, &this->basicResponse.algId,
        this->basicResponse.tbsResponseData.Data,
        this->basicResponse.tbsResponseData.Length,
        this->basicResponse.sig.Data,
        this->basicResponse.sig.Length / 8) == errSecSuccess;
}

static bool SecOCSPResponseIsIssuer(SecOCSPResponseRef this,
    SecCertificateRef issuer) {
    bool shouldBeSigner = false;
	if (this->responderIdTag == RIT_Name) {
		/* Name inside response must == signer's SubjectName. */
        CFDataRef subject = SecCertificateCopySubjectSequence(issuer);
        if (!subject) {
			ocspdDebug("error on SecCertificateCopySubjectSequence");
			return false;
		}
        if ((size_t)CFDataGetLength(subject) == this->responderID.byName.Length &&
            !memcmp(this->responderID.byName.Data, CFDataGetBytePtr(subject),
                this->responderID.byName.Length)) {
			ocspdDebug("good ResponderID.byName");
			shouldBeSigner = true;
        } else {
			ocspdDebug("BAD ResponderID.byName");
		}
        CFRelease(subject);
    } else /* if (this->responderIdTag == RIT_Key) */ {
		/* ResponderID.byKey must == SHA1(signer's public key) */
        CFDataRef pubKeyDigest = SecCertificateCopyPublicKeySHA1Digest(issuer);
        if ((size_t)CFDataGetLength(pubKeyDigest) == this->responderID.byKey.Length &&
            !memcmp(this->responderID.byKey.Data, CFDataGetBytePtr(pubKeyDigest),
                this->responderID.byKey.Length)) {
			ocspdDebug("good ResponderID.byKey");
			shouldBeSigner = true;
		} else {
			ocspdDebug("BAD ResponderID.byKey");
		}
        CFRelease(pubKeyDigest);
    }

    if (shouldBeSigner) {
        SecKeyRef key = SecCertificateCopyKey(issuer);
        if (key) {
            shouldBeSigner = SecOCSPResponseVerifySignature(this, key);
            ocspdDebug("ocsp response signature %sok", shouldBeSigner ? "" : "not ");
            CFRelease(key);
        } else {
			ocspdDebug("Failed to extract key from leaf certificate");
            shouldBeSigner = false;
        }
    }

    return shouldBeSigner;
}

/* Returns the SecCertificateRef of the cert that signed this ocspResponse if
 we can find one and NULL if we can't find a valid signer. */
SecCertificateRef SecOCSPResponseCopySigner(SecOCSPResponseRef this, SecCertificateRef issuer) {
    /* Look though any certs that came with the response to find
     * which one signed the response. */
    SecAsn1Item **certs;
    for (certs = this->basicResponse.certs; certs && *certs; ++certs) {
        SecCertificateRef cert = SecCertificateCreateWithBytes(
                                    kCFAllocatorDefault, (*certs)->Data, (*certs)->Length);
        if (cert) {
            if (SecOCSPResponseIsIssuer(this, cert)) {
                return cert;
            } else {
                CFRelease(cert);
            }
        } else {
            ocspdErrorLog("ocsp response cert failed to parse");
        }
    }
    ocspdDebug("ocsp response did not contain a signer cert.");

    /* If none of the returned certs work, try the issuer of the certificate
       being checked directly. */
    if (issuer && SecOCSPResponseIsIssuer(this, issuer)) {
        CFRetain(issuer);
        return issuer;
    }

    /* We couldn't find who signed this ocspResponse, give up. */
    return NULL;
}

bool SecOCSPResponseIsWeakHash(SecOCSPResponseRef response) {
    SecAsn1AlgId algId = response->basicResponse.algId;
    const DERItem algOid = {
        .data = algId.algorithm.Data,
        .length = algId.algorithm.Length,
    };
    SecSignatureHashAlgorithm algorithm = SecSignatureHashAlgorithmForAlgorithmOid(&algOid);
    if (algorithm == kSecSignatureHashAlgorithmUnknown ||
        algorithm == kSecSignatureHashAlgorithmMD2 ||
        algorithm == kSecSignatureHashAlgorithmMD4 ||
        algorithm == kSecSignatureHashAlgorithmMD5 ||
        algorithm == kSecSignatureHashAlgorithmSHA1) {
        return true;
    }
    return false;
}
