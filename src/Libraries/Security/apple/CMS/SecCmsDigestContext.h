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
    @header SecCmsDigestContext.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions calculating digests.
 */

#ifndef _SECURITY_SECCMSDIGESTCONTEXT_H_
#define _SECURITY_SECCMSDIGESTCONTEXT_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*!
    @function
    @abstract Start digest calculation using all the digest algorithms in "digestalgs" in parallel.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern SecCmsDigestContextRef
SecCmsDigestContextStartMultiple(SECAlgorithmID **digestalgs)
    API_AVAILABLE(macos(10.4), ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop

/*!
    @function
    @abstract Feed more data into the digest machine.
 */
extern void
SecCmsDigestContextUpdate(SecCmsDigestContextRef cmsdigcx, const unsigned char *data, size_t len);

/*!
    @function
    @abstract Cancel digesting operation in progress and destroy it.
    @discussion Cancel a DigestContext created with @link SecCmsDigestContextStartMultiple SecCmsDigestContextStartMultiple function@/link.
 */
extern void
SecCmsDigestContextCancel(SecCmsDigestContextRef cmsdigcx);

#if TARGET_OS_IPHONE
/*!
    @function
    @abstract Destroy a SecCmsDigestContextRef.
    @discussion Cancel a DigestContext created with @link SecCmsDigestContextStartMultiple SecCmsDigestContextStartMultiple function@/link after it has been used in a @link SecCmsSignedDataSetDigestContext SecCmsSignedDataSetDigestContext function@/link.
 */
extern void
SecCmsDigestContextDestroy(SecCmsDigestContextRef cmsdigcx)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // TARGET_OS_IPHONE

#if TARGET_OS_OSX
/*!
 @function
 @abstract Finish the digests and put them into an array of CSSM_DATAs (allocated on arena)
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern OSStatus
SecCmsDigestContextFinishMultiple(SecCmsDigestContextRef cmsdigcx, SecArenaPoolRef arena,
                                  CSSM_DATA_PTR **digestsp)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#endif // TARGET_OS_OSX

__END_DECLS

#endif /* _SECURITY_SECCMSDIGESTCONTEXT_H_ */
