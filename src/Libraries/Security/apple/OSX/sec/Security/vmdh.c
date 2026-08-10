/*
 * Copyright (c) 2006-2007,2012,2014 Apple Inc. All Rights Reserved.
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
	@header vmdh
	The functions provided in vmdh.h implement the crypto exchange required
    for a Diffie-Hellman voicemail exchange.
*/


#include "vmdh.h"
#include <CommonCrypto/CommonCryptor.h>
#include <utilities/debugging.h>
#include <string.h>
#include <Security/SecInternal.h>
#include <Security/SecDH.h>

vmdh_t vmdh_create(uint32_t g, const uint8_t *p, size_t p_len,
    const uint8_t *recip, size_t recip_len) {
	SecDHContext dh;
	if (SecDHCreate(g, p, p_len, 0/*l*/, recip, recip_len, &dh))
		return NULL;
	return (vmdh_t)dh;
}

bool vmdh_generate_key(vmdh_t vmdh, uint8_t *pub_key, size_t *pub_key_len) {
	return !SecDHGenerateKeypair((SecDHContext)vmdh, pub_key, pub_key_len);
}

bool vmdh_encrypt_password(vmdh_t vmdh,
	const uint8_t *pub_key, size_t pub_key_len,
    const uint8_t *pw, size_t pw_len, uint8_t *encpw, size_t *encpw_len) {
	uint8_t aes_key[kCCKeySizeAES128];
	size_t aes_key_len = kCCKeySizeAES128;

	if (SecDHComputeKey((SecDHContext)vmdh, pub_key, pub_key_len,
		aes_key, &aes_key_len)) {
		return false;
	}

    /* Use the first 16 bytes in aes_key as an AES key. */
	if (CCCrypt(kCCEncrypt, kCCAlgorithmAES128,
		kCCOptionPKCS7Padding, aes_key, kCCKeySizeAES128, NULL,
		pw, pw_len, encpw, *encpw_len, encpw_len)) {
		return false;
	}

	/* Zero out key material. */
	bzero(aes_key, kCCKeySizeAES128);

    return true;
}

void vmdh_destroy(vmdh_t vmdh) {
    return SecDHDestroy((SecDHContext)vmdh);
}
