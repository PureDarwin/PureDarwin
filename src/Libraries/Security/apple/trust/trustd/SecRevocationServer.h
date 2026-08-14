/*
 * Copyright (c) 2017-2018 Apple Inc. All Rights Reserved.
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

/*!
 @header SecRevocationServer
 The functions provided in SecRevocationServer.h provide an interface to
 the trust evaluation engine for dealing with certificate revocation.
 */

#ifndef _SECURITY_SECREVOCATIONSERVER_H_
#define _SECURITY_SECREVOCATIONSERVER_H_

#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecRevocationDb.h"
#include "trust/trustd/SecOCSPRequest.h"
#include "trust/trustd/SecOCSPResponse.h"

typedef struct OpaqueSecORVC *SecORVCRef;

/* Revocation verification context. */
struct OpaqueSecRVC {
    /* Pointer to the builder for this revocation check */
    SecPathBuilderRef   builder;

    /* Index of cert in pvc that this RVC is for 0 = leaf, etc. */
    CFIndex             certIX;

    /* The OCSP Revocation verification context */
    SecORVCRef          orvc;

    /* Valid database info for this revocation check */
    SecValidInfoRef     valid_info;

    bool                done;

    bool                revocation_checked;
};
typedef struct OpaqueSecRVC *SecRVCRef;

/* OCSP Revocation verification context. */
struct OpaqueSecORVC {
    /* Pointer to the builder for this revocation check. */
    SecPathBuilderRef builder;

    /* Pointer to the generic rvc for this revocation check */
    SecRVCRef rvc;

    /* The ocsp request we send to each responder. */
    SecOCSPRequestRef ocspRequest;

    /* The freshest response we received so far, from stapling or cache or responder. */
    SecOCSPResponseRef ocspResponse;

    /* The best validated candidate single response we received so far, from stapling or cache or responder. */
    SecOCSPSingleResponseRef ocspSingleResponse;

    /* Index of cert in builder that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* Validity period for which this revocation status. */
    CFAbsoluteTime thisUpdate;
    CFAbsoluteTime nextUpdate;

    /* URL of current responder. For logging purposes. */
    CFURLRef responder;

    bool done;
};

bool SecPathBuilderCheckRevocation(SecPathBuilderRef builder);
void SecPathBuilderCheckKnownIntermediateConstraints(SecPathBuilderRef builder);
CFAbsoluteTime SecRVCGetEarliestNextUpdate(SecRVCRef rvc);
CFAbsoluteTime SecRVCGetLatestThisUpdate(SecRVCRef rvc);
void SecRVCDelete(SecRVCRef rvc);
bool SecRVCHasDefinitiveValidInfo(SecRVCRef rvc);
bool SecRVCHasRevokedValidInfo(SecRVCRef rvc);
void SecRVCSetValidDeterminedErrorResult(SecRVCRef rvc);
bool SecRVCRevocationChecked(SecRVCRef rvc);

/* OCSP verification callbacks */
void SecORVCConsumeOCSPResponse(SecORVCRef rvc, SecOCSPResponseRef ocspResponse /*CF_CONSUMED*/,
                                CFTimeInterval maxAge, bool updateCache, bool fromCache);
void SecORVCUpdatePVC(SecORVCRef rvc);


#endif /* _SECURITY_SECREVOCATIONSERVER_H_ */
