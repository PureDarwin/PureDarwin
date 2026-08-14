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

#ifndef _SECURITY_SECCMSENVELOPEDDATA_H_
#define _SECURITY_SECCMSENVELOPEDDATA_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*!
     @function
     @abstract Create an enveloped data message.
 */
extern SecCmsEnvelopedDataRef
SecCmsEnvelopedDataCreate(SecCmsMessageRef cmsg, SECOidTag algorithm, int keysize);

/*!
    @function
    @abstract Destroy an enveloped data message.
 */
extern void
SecCmsEnvelopedDataDestroy(SecCmsEnvelopedDataRef edp);

/*!
    @function
    @abstract Return pointer to this envelopedData's contentinfo.
 */
extern SecCmsContentInfoRef
SecCmsEnvelopedDataGetContentInfo(SecCmsEnvelopedDataRef envd);

#if TARGET_OS_OSX
/*!
 @function
 @abstract Add a recipientinfo to the enveloped data msg.
 @discussion Rip must be created on the same pool as edp - this is not enforced, though.
 */
extern OSStatus
SecCmsEnvelopedDataAddRecipient(SecCmsEnvelopedDataRef edp, SecCmsRecipientInfoRef rip);
#endif

__END_DECLS

#endif /* _SECURITY_SECCMSENVELOPEDDATA_H_ */
