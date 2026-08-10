/*
 * Copyright (c) 2007,2012-2013 Apple Inc. All Rights Reserved.
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
	@header SecRSAKeyPriv
	The functions provided in SecRSAKeyPriv.h implement and manage a rsa
    public or private key.
*/

#ifndef _SECURITY_SECRSAKEYPRIV_H_
#define _SECURITY_SECRSAKEYPRIV_H_

#include <Security/SecRSAKey.h>

__BEGIN_DECLS

OSStatus SecRSAKeyGeneratePair(CFDictionaryRef parameters,
    SecKeyRef *rsaPublicKey, SecKeyRef *rsaPrivateKey);

__END_DECLS

#endif /* !_SECURITY_SECRSAKEY_H_ */
