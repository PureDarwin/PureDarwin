/*
 * Copyright (c) 2000-2001,2012,2014 Apple Inc. All Rights Reserved.
 *
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

/*
	File:		pbkdf2.h
	Contains:	Apple Data Security Services PKCS #5 PBKDF2 function declaration.
	Copyright (c) 1999,2012,2014 Apple Inc. All Rights Reserved.
*/

#ifndef __PBKDF2__
#define __PBKDF2__

__BEGIN_DECLS

/* This function should generate a pseudo random octect stream
   of hLen bytes long (The value hLen is specified as an argument to pbkdf2
   and should be constant for any given prf function.) which is output in the buffer
   pointed to by randomPtr (the caller of this function is responsible for allocation
   of the buffer).
   The inputs to the pseudo random function are the first keyLen octets pointed
   to by keyPtr and the first textLen octets pointed to by textPtr.
   Both keyLen and textLen can have any nonzero value.
   A good prf would be a HMAC-SHA-1 algorithm where the keyPtr octets serve as
   HMAC's "key" and the textPtr octets serve as HMAC's "text".  */
typedef void (*PRF)(const uint8_t *keyPtr, size_t keyLen,
					const uint8_t *textPtr, size_t textLen,
					uint8_t *randomPtr);

/* This function implements the PBKDF2 key derrivation algorithm described in
   http://www.rsa.com/rsalabs/pubs/PKCS/html/pkcs-5.html
   The output is a derived key of dkLen bytes which is written to the buffer
   pointed to by dkPtr.
   The caller should ensure dkPtr is at least dkLen bytes long.
   The Key is derived from passwordPtr (which is passwordLen bytes long) and from
   saltPtr (which is saltLen bytes long).  The algorithm used is desacribed in
   PKCS #5 version 2.0 and iterationCount iterations are performed.
   The argument prf is a pointer to a psuedo random number generator declared above.
   It should write exactly hLen bytes into its output buffer each time it is called.
   The argument tempBuffer should point to a buffer MAX (hLen, saltLen + 4) + 2 * hLen
   bytes long.  This buffer is used during the calculation for intermediate results.
   Security Considerations:
   The argument saltPtr should be a pointer to a buffer of at least 8 random bytes
   (64 bits).  Thus saltLen should be >= 8.
   For each session a new salt should be generated.
   The value of iterationCount should be at least 1000 (one thousand).
   A good prf would be a HMAC-SHA-1 algorithm where the password serves as
   HMAC's "key" and the data serves as HMAC's "text".  */
void pbkdf2 (PRF prf, size_t hLen,
			 const void *passwordPtr, size_t passwordLen,
			 const void *saltPtr, size_t saltLen,
			 size_t iterationCount,
			 void *dkPtr, size_t dkLen,
			 void *tempBuffer);


#ifdef	__cplusplus
}
#endif

#endif /* __PBKDF2__ */
