/*
 *  Copyright (c) 2004-2018 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecCmsEnvelopedData.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSENCRYPTEDDATA_H_
#define _SECURITY_SECCMSENCRYPTEDDATA_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*!
    @function
    @abstract Create an empty EncryptedData object.
    @param algorithm Specifies the bulk encryption algorithm to use.
    @param keysize is the key size.
    @discussion An error results in a return value of NULL and an error set.
                (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
extern SecCmsEncryptedDataRef
SecCmsEncryptedDataCreate(SecCmsMessageRef cmsg, SECOidTag algorithm, int keysize);

/*!
    @function
    @abstract Destroy an encryptedData object.
 */
extern void
SecCmsEncryptedDataDestroy(SecCmsEncryptedDataRef encd);

/*!
    @function
    @abstract Return pointer to an EncryptedData object's contentInfo.
 */
extern SecCmsContentInfoRef
SecCmsEncryptedDataGetContentInfo(SecCmsEncryptedDataRef encd);

__END_DECLS

#endif /* _SECURITY_SECCMSENCRYPTEDDATA_H_ */
