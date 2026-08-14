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
    @header SecSMIME.h

    @availability 10.4 and later
    @abstract S/MIME Specific routines.
    @discussion Header file for routines specific to S/MIME.  Keep
		things that are pure pkcs7 out of here; this is for
		S/MIME policy, S/MIME interoperability, etc.
*/

#ifndef _SECURITY_SECSMIME_H_
#define _SECURITY_SECSMIME_H_ 1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*!
    @function
    @abstract Find bulk algorithm suitable for all recipients.
 */
extern OSStatus
SecSMIMEFindBulkAlgForRecipients(SecCertificateRef *rcerts, SECOidTag *bulkalgtag, int *keysize);

__END_DECLS

#endif /* _SECURITY_SECSMIME_H_ */
