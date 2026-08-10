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

#ifndef _SECURITY_VMDH_H_
#define _SECURITY_VMDH_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmdh *vmdh_t;

/* Return a newly allocated vmdh object given g, p and the recip of p recip.
   The recip and recip_len parameters are constant for a given p.  They are
   optional although providing them improves performance.
   The caller should call vmdh_destroy once the returned handle is no longer
   needed. */
vmdh_t vmdh_create(uint32_t g, const uint8_t *p, size_t p_len,
    const uint8_t *recip, size_t recip_len);

/* Generate a dh private/public keypair and return the public key in pub_key.
   on input *pub_key_len is the number of bytes available in pub_key, on output
   pub_key_len is the number of bytes actually in pub_key.  Returns true on
   success and false on failure. */
bool vmdh_generate_key(vmdh_t vmdh, uint8_t *pub_key, size_t *pub_key_len);

/* Given the length of a password return the size of the encrypted password. */
#define vmdh_encpw_len(PWLEN) (((PWLEN) & ~0xf) + 16)

/* Given a vmdh handle and the other parties public key pub_key (of
   pub_key_len bytes long), encrypt the password given by pw of pw_len bytes
   long and return it in encpw.  On input *enc_pw contains the number of bytes
   available in encpw, on output *encpw will contain the actual length of
   encpw. */ 
bool vmdh_encrypt_password(vmdh_t vmdh,
	const uint8_t *pub_key, size_t pub_key_len,
    const uint8_t *pw, size_t pw_len, uint8_t *encpw, size_t *encpw_len);

/* Destroy a vmdh object created with vmdh_create(). */
void vmdh_destroy(vmdh_t vmdh);

#ifdef __cplusplus
}
#endif

#endif /* _SECURITY_VMDH_H_ */
