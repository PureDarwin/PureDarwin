/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

// #define COMMON_DH_FUNCTIONS
#include <AssertMacros.h>
#include <CommonCrypto/CommonDH.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include "ccDispatch.h"
#include <corecrypto/ccn.h>
#include <corecrypto/cc_priv.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccdh_gp.h>
#include "ccErrors.h"
#include "ccGlobals.h"
#include "ccdebug.h"

typedef ccdh_full_ctx_t CCDH;

CCDHRef
CCDHCreate(CCDHParameters dhParameter)
{
    ccdh_const_gp_t gp;
    CCDH retval = NULL;

    CC_DEBUG_LOG("Entering\n");
    if (dhParameter != kCCDHRFC3526Group5) {
        return NULL;
    }

    gp = ccdh_gp_rfc3526group05();
    size_t retSize = ccdh_full_ctx_size(ccdh_ccn_size(gp));
    require((retval = malloc(retSize)) != NULL, error);

    ccdh_ctx_init(gp, ccdh_ctx_public(retval));
    return (CCDHRef)retval;

error:
    free(retval);
    return NULL;
}

void
CCDHRelease(CCDHRef ref)
{
    CC_DEBUG_LOG("Entering\n");
    free((CCDH)ref);
}

int
CCDHGenerateKey(CCDHRef ref, void *output, size_t *outputLength)
{
    CC_DEBUG_LOG("Entering\n");
    CC_NONULLPARM(ref);
    CC_NONULLPARM(output);
    CC_NONULLPARM(outputLength);

    CCDH keyref = (CCDH) ref;

    if (ccdh_generate_key(ccdh_ctx_gp(keyref), ccDRBGGetRngState(), keyref)) {
        return -1;
    }

    size_t size_needed = ccdh_export_pub_size(ccdh_ctx_public(keyref));
    if (size_needed > *outputLength) {
        *outputLength = size_needed;
        return -1;
    }

    *outputLength = size_needed;
    ccdh_export_pub(ccdh_ctx_public(keyref), output);
    return 0;
}


int
CCDHComputeKey(unsigned char *sharedKey, size_t *sharedKeyLen, const void *peerPubKey, size_t peerPubKeyLen, CCDHRef ref)
{
    CC_DEBUG_LOG("Entering\n");
    CC_NONULLPARM(sharedKey);
    CC_NONULLPARM(sharedKeyLen);
    CC_NONULLPARM(peerPubKey);
    CC_NONULLPARM(ref);

    CCDH keyref = (CCDH) ref;
    ccdh_pub_ctx_decl_gp(ccdh_ctx_gp(keyref), peer_pub);

    // Return the expected value in case of error
    size_t size_needed = CC_BITLEN_TO_BYTELEN(ccdh_gp_prime_bitlen(ccdh_ctx_gp(keyref)));
    if (size_needed > *sharedKeyLen) {
        *sharedKeyLen = size_needed;
        return -1;
    }

    // Import key
    if (ccdh_import_pub(ccdh_ctx_gp(keyref), peerPubKeyLen, peerPubKey, peer_pub)) {
        *sharedKeyLen = size_needed;
        return -2;
    }

    // Export secret with no leading zero
    if (ccdh_compute_shared_secret(keyref, peer_pub, sharedKeyLen, sharedKey, ccrng(NULL))) {
        return -3;
    }

    return 0;
}
