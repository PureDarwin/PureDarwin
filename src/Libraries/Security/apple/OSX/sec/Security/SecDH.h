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

/*!
	@header SecDH
	The functions provided in SecDH.h implement the crypto required
    for a Diffie-Hellman key exchange.
*/

#ifndef _SECURITY_SECDH_H_
#define _SECURITY_SECDH_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueSecDHContext *SecDHContext;

/*!
	@function SecDHCreate
	@abstract Return a newly allocated SecDHContext object.
	@param g generator (2 or 5)
	@param p prime as a big-endian unsigned byte array
	@param p_len length of p, in bytes
	@param l (optional) minimum length of private key in bits, or 0 for default 
	@param recip (optional) reciprocal of p as a big-endian unsigned byte array
	@param recip_len length of recip, in bytes
	@param dh (output) pointer to a SecDHContext
	@discussion The recip and recip_len parameters are constant for a given p.
	They are optional, although providing them improves performance.
    @result On success, a newly allocated SecDHContext is returned in dh and
	errSecSuccess is returned.  On failure, NULL is returned in dh and an OSStatus error
	code is returned.
    The caller should call SecDHDestroy once the returned context is no longer
    needed.
 */
OSStatus SecDHCreate(uint32_t g, const uint8_t *p, size_t p_len, uint32_t l,
	const uint8_t *recip, size_t recip_len, SecDHContext *dh);

/*!
	@function SecDHCreateFromParameters
	@param params A DER-encoded ASN.1 parameter object, as per PKCS3, containing
	Diffie-Hellman key parameters
	@param params_len Length of params, in bytes
	@param dh (output) A pointer to a SecDHContext
    @result On success, a newly allocated SecDHContext is returned in dh and
	errSecSuccess is returned.  On failure, NULL is returned in dh and an OSStatus error
	code is returned.
    The caller should call SecDHDestroy once the returned context is no longer
    needed.
 */
OSStatus SecDHCreateFromParameters(const uint8_t *params, size_t params_len,
	SecDHContext *dh);

/*!
	@function SecDHCreateFromAlgorithmId
	@param alg A DER-encoded ASN.1 Algorithm Identifier object, as per PKCS1,
	containing DH parameters.
	@param alg_len Length of alg, in bytes
	@param dh (output) A pointer to a SecDHContext
    @result On success, a newly allocated SecDHContext is returned in dh and
	errSecSuccess is returned.  On failure, NULL is returned in dh and an OSStatus error
	code is returned.
    The caller should call SecDHDestroy once the returned context is no longer
    needed.
 */
OSStatus SecDHCreateFromAlgorithmId(const uint8_t *alg, size_t alg_len,
	SecDHContext *dh);

/*!
	@function SecDHGetMaxKeyLength
	@abstract Return the maximum length in bytes of the pub_key returned by
	SecDHGenerateKeypair().  
	@param dh A context created by one of the SecDHCreate functions
	@discussion The value returned by this function is also the largest number
	of bytes returned by SecDHComputeKey().  If a caller used the
	SecDHCreate() function to create the SecDHContext passed to this function,
	the value returned will be less than or equal to the p_len parameter
	passed to SecDHCreate().
    @result Return maximum length, in bytes, of keys returned by the passed-in
	SecDHContext.
 */
size_t SecDHGetMaxKeyLength(SecDHContext dh);

/*!
	@function SecDHGenerateKeypair
	@abstract Generate a Diffie-Hellman private/public key pair and return
	the public key as an unsigned big-endian byte array.
	@param dh A context created by one of the SecDHCreate functions
	@param pub_key On return, the public key to be shared with the other party.
	@params pub_key_len On input, the number of bytes available in pub_key;
	on output, the number of bytes actually in pub_key.  
	@discussion Reusing a SecDHContext for multiple SecDHGenerateKeypair()
	invocations is permitted.
    @result errSecSuccess on success, or an OSStatus error code on failure.
 */
OSStatus SecDHGenerateKeypair(SecDHContext dh, uint8_t *pub_key,
	size_t *pub_key_len);

/*!
	@function SecDHComputeKey
	@abstract Given a SecDHContext and the other party's public key, 
	compute the shared secret.  
	@param dh A context created by one of the SecDHCreate functions, on which
	SecDHGenerateKeypair() has been invoked first.
	@param pub_key The other party's public key, as an unsigned big-endian byte
	array.  
	@params pub_key_len The length of pub_key, in bytes
    @param computed_key A pointer to a byte array in which the computed key
	is returned.
	@param computed_key_len On input, contains the number of
	bytes requested to be returned in computed_key; on output, contains 
	the number of bytes returned in computed_key.
	This will only be less than the requested number of bytes if the number
	of bytes requested is larger than the number of bytes output by the
	compute-key operation.
	@discussion If *computed_key_len is less than the size of the actual
	computed key, only the first *computed_key_len bytes will be returned.
	No leading zero bytes will be returned, and the computed_key is returned
	as an unsigned big-endian byte array.
    @result errSecSuccess on success, or an OSStatus error code on failure.
 */
OSStatus SecDHComputeKey(SecDHContext dh,
	const uint8_t *pub_key, size_t pub_key_len,
    uint8_t *computed_key, size_t *computed_key_len);

/*!
	@function SecDHDestroy
	@abstract Destroy a SecDHContext created with one of the SecDHCreate functions. 
	@param dh A context created by one of the SecDHCreate functions
 */
void SecDHDestroy(SecDHContext dh);


/*!
    @function SecDHEncodeParams
    @abstract Encode parameters in a PKCS#3 blob
 */
OSStatus SecDHEncodeParams(CFDataRef g, CFDataRef p,
                           CFDataRef l, CFDataRef recip,
                           CFDataRef *params);

/*!
     @function SecDHDecodeParams
     @abstract Decode parameters in a PKCS#3 blob
 */
OSStatus SecDHDecodeParams(CFDataRef *g, CFDataRef *p,
                           CFDataRef *l, CFDataRef *r,
                           CFDataRef params);

#ifdef __cplusplus
}
#endif

#endif /* _SECURITY_SECDH_H_ */
