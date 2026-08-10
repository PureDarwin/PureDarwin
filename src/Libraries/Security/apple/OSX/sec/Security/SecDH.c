/*
 * Copyright (c) 2007-2008,2010,2012-2013 Apple Inc. All Rights Reserved.
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
 * SecDH.c - Implement the crypto required for a Diffie-Hellman key exchange.
 */

#include "SecDH.h"
#include <libDER/DER_Keys.h>
#include <corecrypto/ccdh.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>
#include <libkern/OSByteOrder.h>
#include "utilities/debugging.h"
#include <Security/SecInternal.h>
#include <Security/SecRandom.h>
#include <stdlib.h>
#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>

#ifdef DEBUG
#define DH_DEBUG  1
#endif

/* SecDHContext memory layout
+-----------------+
| ccdh_gp         |
+-----------------+
| ccdh_full_ctx   |
+-----------------+
*/

static inline ccdh_gp_t SecDH_gp(SecDHContext dh)
{
    return (ccdh_gp_t)dh;
}

static inline ccdh_full_ctx_t SecDH_priv(SecDHContext dh)
{
    ccdh_gp_t gp = SecDH_gp(dh);
    cc_size s = ccn_sizeof_n(ccdh_gp_n(gp));
    return (ccdh_full_ctx_t)cc_pad_align((uintptr_t)dh + ccdh_gp_size(s));
}

size_t SecDHGetMaxKeyLength(SecDHContext dh) {

    ccdh_gp_t gp = SecDH_gp(dh);
    return ccn_sizeof_n(ccdh_gp_n(gp));
}

static inline size_t SecDH_context_size(size_t p_len)
{
    cc_size real_p_len = ccn_sizeof_size(p_len);

    // Add padding to allow proper alignment of the ccdh_full_ctx.
    return ccdh_gp_size(real_p_len) + (CC_MAX_ALIGNMENT - 1) + ccdh_full_ctx_size(real_p_len);
}

/* Shared static functions. */

static OSStatus
der2OSStatus(DERReturn derReturn)
{
	switch(derReturn)
	{
	case DR_Success:				return errSecSuccess;
	case DR_EndOfSequence:			return errSecDecode; 
	case DR_UnexpectedTag:			return errSecDecode;
	case DR_DecodeError:			return errSecDecode;
	case DR_Unimplemented:			return errSecUnimplemented;
	case DR_IncompleteSeq:			return errSecDecode;
	case DR_ParamErr:				return errSecParam;
	case DR_BufOverflow:			return errSecBufferTooSmall;
	default:						return errSecInternal;
	}
}

static int dhRngCallback(struct ccrng_state *rng, unsigned long outlen, void *out)
{
    return SecRandomCopyBytes(kSecRandomDefault, outlen, out);
}

static struct ccrng_state dhrng = {
    .generate = dhRngCallback
};

OSStatus SecDHCreate(uint32_t g, const uint8_t *p, size_t p_len,
	uint32_t l, const uint8_t *recip, size_t recip_len, SecDHContext *pdh)
{
    cc_size n = ccn_nof_size(p_len);
    size_t context_size = SecDH_context_size(p_len);
    void *context = malloc(context_size);
    cc_clear(context_size, context);

    ccdh_gp_t gp;
    gp  = context;

    CCDH_GP_N(gp) = n;
    CCDH_GP_L(gp) = l;

    if(ccn_read_uint(n, CCDH_GP_PRIME(gp), p_len, p))
        goto errOut;
    if(recip) {
        if(ccn_read_uint(n+1, CCDH_GP_RECIP(gp), recip_len, recip))
            goto errOut;
        cczp_init_with_recip(CCDH_GP_ZP(gp), CCDH_GP_RECIP(gp));
    } else if (cczp_init(CCDH_GP_ZP(gp))) {
        goto errOut;
    }
    ccn_seti(n, CCDH_GP_G(gp), g);

    *pdh = (SecDHContext) context;

    return errSecSuccess;

errOut:
    SecDHDestroy(context);
    *pdh = NULL;
    return errSecInternal;

}

/* this used to be in libgDH */
/*
 * Support for encoding and decoding DH parameter blocks.
 * Apple form encodes the reciprocal of the prime p.
 */
/* PKCS3, Openssl compatible */
typedef struct {
	DERItem				p;
	DERItem				g;
	DERItem				l;
	DERItem				recip; /* Only used in Apple Custom blocks. */
} DER_DHParams;

static const DERItemSpec DER_DHParamsItemSpecs[] =
{
	{ DER_OFFSET(DER_DHParams, p),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, g),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, l),
        ASN1_INTEGER,
        DER_DEC_OPTIONAL | DER_ENC_SIGNED_INT },
    /* Not part of PKCS3 per-se, but we add it on just for kicks.  Since
     it's optional we will automatically decode any apple specific
     params, but we won't add this section unless the caller asks
     us to.  */
	{ DER_OFFSET(DER_DHParams, recip),
        ASN1_PRIVATE | ASN1_PRIMITIVE | 0,
        DER_DEC_OPTIONAL | DER_ENC_SIGNED_INT },
};
static const DERSize DER_NumDHParamsItemSpecs =
sizeof(DER_DHParamsItemSpecs) / sizeof(DERItemSpec);


OSStatus SecDHCreateFromParameters(const uint8_t *params,
	size_t params_len, SecDHContext *pdh)
{
    // We support DomainParameters as specified in PKCS#3
    // (http://www.emc.com/emc-plus/rsa-labs/standards-initiatives/pkcs-3-diffie-hellman-key-agreement-standar.htm)
    // DHParameter ::= SEQUENCE {
    //   prime INTEGER, -- p
    //   base INTEGER, -- g
    //   privateValueLength INTEGER OPTIONAL }

    DERReturn drtn;
	DERItem paramItem = {(DERByte *)params, params_len};
	DER_DHParams decodedParams;
    uint32_t l = 0;

    drtn = DERParseSequence(&paramItem,
                            DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                            &decodedParams, sizeof(decodedParams));
    if(drtn)
        return drtn;

    if (decodedParams.l.length > 0) {
        drtn = DERParseInteger(&decodedParams.l, &l);
        if(drtn)
            return drtn;
    }
    cc_size n = ccn_nof_size(decodedParams.p.length);
    cc_size p_len = ccn_sizeof_n(n);
    size_t context_size = SecDH_context_size(p_len);
    void *context = malloc(context_size);
    if(context==NULL)
        return errSecAllocate;

    bzero(context, context_size);

    ccdh_gp_t gp = context;

    CCDH_GP_N(gp) = n;
    CCDH_GP_L(gp) = l;

    if(ccn_read_uint(n, CCDH_GP_PRIME(gp), decodedParams.p.length, decodedParams.p.data))
        goto errOut;
    if(decodedParams.recip.length) {
        if(ccn_read_uint(n+1, CCDH_GP_RECIP(gp), decodedParams.recip.length, decodedParams.recip.data))
            goto errOut;
        cczp_init_with_recip(CCDH_GP_ZP(gp), CCDH_GP_RECIP(gp));
    } else if (cczp_init(CCDH_GP_ZP(gp))) {
        goto errOut;
    }

    if(ccn_read_uint(n, CCDH_GP_G(gp), decodedParams.g.length, decodedParams.g.data))
        goto errOut;

    *pdh = (SecDHContext) context;
    return errSecSuccess;

errOut:
    SecDHDestroy(context);
    *pdh = NULL;
    return errSecInvalidKey;
}

OSStatus SecDHCreateFromAlgorithmId(const uint8_t *alg, size_t alg_len,
	SecDHContext *pdh) {
	DERAlgorithmId algorithmId;
	DERItem algId;

	algId.data = (uint8_t *)alg;
	algId.length = alg_len;

	DERReturn drtn = DERParseSequence(&algId,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&algorithmId, sizeof(algorithmId));
	if (drtn != DR_Success)
		return der2OSStatus(drtn);

    return SecDHCreateFromParameters(algorithmId.params.data,
		algorithmId.params.length, pdh);
}

OSStatus SecDHGenerateKeypair(SecDHContext dh, uint8_t *pub_key,
	size_t *pub_key_len)
{
    int result;
    ccdh_gp_t gp = SecDH_gp(dh);
    ccdh_full_ctx_t priv = SecDH_priv(dh);

    if((result = ccdh_generate_key(gp, &dhrng, priv)))
        return result;

    /* output y as a big endian byte buffer */
    size_t ylen = ccn_write_uint_size(ccdh_gp_n(gp), ccdh_ctx_y(priv));
    if(*pub_key_len < ylen)
       return errSecBufferTooSmall;
    ccn_write_uint(ccdh_gp_n(gp),ccdh_ctx_y(priv), ylen, pub_key);
    *pub_key_len = ylen;

    return errSecSuccess;
}

OSStatus SecDHComputeKey(SecDHContext dh,
	const uint8_t *pub_key, size_t pub_key_len,
    uint8_t *computed_key, size_t *computed_key_len)
{
    ccdh_gp_t gp = SecDH_gp(dh);
    ccdh_full_ctx_t priv = SecDH_priv(dh);
    ccdh_pub_ctx_decl_gp(gp, pub);
    cc_size n = ccdh_gp_n(gp);
    cc_unit r[n];

    if(ccdh_import_pub(gp, pub_key_len, pub_key, pub))
        return errSecInvalidKey;

    //ccdh_compute_shared_secret() cannot be used directly, because it doesn't allow truncated output. Buffering is needed.
    if(ccdh_compute_key(priv, pub, r))
        return errSecInvalidKey;

    ccn_write_uint(n, r, *computed_key_len, computed_key);
    size_t out_size = ccn_write_uint_size(n, r);
    if(out_size < *computed_key_len)
        *computed_key_len=out_size;

    return errSecSuccess;
}

void SecDHDestroy(SecDHContext dh) {
	/* Zero out key material. */
    ccdh_gp_t gp = SecDH_gp(dh);
    cc_size p_len = ccn_sizeof_n(ccdh_gp_n(gp));
    size_t context_size = SecDH_context_size(p_len);

    cc_clear(context_size, dh);
    free(dh);
}

/* Max encoded size for standard (PKCS3) parameters */
#define DH_ENCODED_PARAM_SIZE(primeSizeInBytes)					\
DER_MAX_ENCODED_SIZE(										\
DER_MAX_ENCODED_SIZE(primeSizeInBytes) +		/* g */		\
DER_MAX_ENCODED_SIZE(primeSizeInBytes) +		/* p */     \
DER_MAX_ENCODED_SIZE(4))                        /* l */


OSStatus SecDHEncodeParams(CFDataRef g, CFDataRef p,
                           CFDataRef l, CFDataRef recip,
                           CFDataRef *params)
{
    OSStatus ortn;
    DER_DHParams derParams =
    {
        .p = {
            .length = CFDataGetLength(p),
            .data = (DERByte *)CFDataGetBytePtr(p),
        },
        .g = {
            .length = CFDataGetLength(g),
            .data = (DERByte *)CFDataGetBytePtr(g),
        },
        .l = {
            .length = l?CFDataGetLength(l):0,
            .data = (DERByte *)(l?CFDataGetBytePtr(l):NULL),
        },
        .recip = {
            .length = recip?CFDataGetLength(recip):0,
            .data = (DERByte *)(recip?CFDataGetBytePtr(recip):NULL),
        },
    };

    DERSize ioLen = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE,
                                               &derParams,
                                               DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs);

    DERByte *der = malloc(ioLen);
    // FIXME: What if this fails - we should probably not have a malloc here ?
    assert(der);
    ortn = (int)DEREncodeSequence(ASN1_CONSTR_SEQUENCE,
                                  &derParams,
                                  DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                                  der,
                                  &ioLen);

    if(!ortn && params)
        *params=CFDataCreate(kCFAllocatorDefault, der, ioLen);

    // FIXME: we should just allocate the CFDataRef

    free(der);
    return ortn;
}


OSStatus SecDHDecodeParams(CFDataRef *g, CFDataRef *p,
                           CFDataRef *l, CFDataRef *r,
                           CFDataRef params)
{
    DERReturn drtn;
	DERItem paramItem = {(DERByte *)CFDataGetBytePtr(params), CFDataGetLength(params)};
	DER_DHParams decodedParams;

    drtn = DERParseSequence(&paramItem,
                            DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                            &decodedParams, sizeof(decodedParams));
    if(drtn)
        return drtn;

    if(g) *g=CFDataCreate(kCFAllocatorDefault, decodedParams.g.data, decodedParams.g.length);
    if(p) *p=CFDataCreate(kCFAllocatorDefault, decodedParams.p.data, decodedParams.p.length);
    if(l) *l=CFDataCreate(kCFAllocatorDefault, decodedParams.l.data, decodedParams.l.length);
    if(r) *r=CFDataCreate(kCFAllocatorDefault, decodedParams.recip.data, decodedParams.recip.length);

    return errSecSuccess;
}
