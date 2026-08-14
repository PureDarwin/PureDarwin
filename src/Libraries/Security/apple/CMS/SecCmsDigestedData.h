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
    @header SecCmsDigestedData.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for creating
                and accessing the DigestData content type of a 
                Cryptographic Message Syntax (CMS) object
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSDIGESTEDDATA_H_
#define _SECURITY_SECCMSDIGESTEDDATA_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*!
    @function
    @abstract Create a digestedData object (presumably for encoding).
    @discussion Version will be set by SecCmsDigestedDataEncodeBeforeStart
                digestAlg is passed as parameter
                contentInfo must be filled by the user
                digest will be calculated while encoding
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern SecCmsDigestedDataRef
SecCmsDigestedDataCreate(SecCmsMessageRef cmsg, SECAlgorithmID *digestalg)
    API_AVAILABLE(macos(10.4), ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop

/*!
    @function
    @abstract Destroy a digestedData object.
 */
extern void
SecCmsDigestedDataDestroy(SecCmsDigestedDataRef digd);

/*!
    @function
    @abstract Return pointer to digestedData object's contentInfo.
 */
extern SecCmsContentInfoRef
SecCmsDigestedDataGetContentInfo(SecCmsDigestedDataRef digd);

__END_DECLS

#endif /* _SECURITY_SECCMSDIGESTEDDATA_H_ */
