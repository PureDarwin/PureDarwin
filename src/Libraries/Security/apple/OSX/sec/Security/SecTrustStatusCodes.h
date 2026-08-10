/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
    @header SecTrustStatusCodes
*/

#ifndef _SECURITY_SECTRUSTSTATUSCODES_H_
#define _SECURITY_SECTRUSTSTATUSCODES_H_

#include <Security/SecTrust.h>

__BEGIN_DECLS

/*!
 @function SecTrustCopyStatusCodes
 @abstract Returns a malloced array of SInt32 values, with the length in numStatusCodes,
 for the certificate specified by chain index in the given SecTrustRef.
 @param trust A reference to a trust object.
 @param index The index of the certificate whose status codes should be returned.
 @param numStatusCodes On return, the number of status codes allocated, or 0 if none.
 @result A pointer to an array of status codes, or NULL if no status codes exist.
 If the result is non-NULL, the caller must free() this pointer.
 @discussion This function returns an array of evaluation status codes for a certificate
 specified by its chain index in a trust reference. If NULL is returned, the certificate
 has no status codes.
 */
SInt32 *SecTrustCopyStatusCodes(SecTrustRef trust,
                                CFIndex index,
                                CFIndex *numStatusCodes);
__END_DECLS

#endif /* !_SECURITY_SECTRUSTSTATUSCODES_H_ */
