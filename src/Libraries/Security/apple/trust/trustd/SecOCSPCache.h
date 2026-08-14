/*
 * Copyright (c) 2009-2010,2012-2017 Apple Inc. All Rights Reserved.
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
 *
 */

/*!
    @header SecOCSPCache
    The functions provided in SecOCSPCache.h provide an interface to
    an OCSP caching module.
*/

#ifndef _SECURITY_SECOCSPCACHE_H_
#define _SECURITY_SECOCSPCACHE_H_

#include "trust/trustd/SecOCSPRequest.h"
#include "trust/trustd/SecOCSPResponse.h"
#include <CoreFoundation/CFURL.h>

__BEGIN_DECLS

void SecOCSPCacheReplaceResponse(SecOCSPResponseRef old_response,
    SecOCSPResponseRef response, CFURLRef localResponderURI, CFAbsoluteTime verifyTime);

SecOCSPResponseRef SecOCSPCacheCopyMatching(SecOCSPRequestRef request,
    CFURLRef localResponderURI /* may be NULL */);

SecOCSPResponseRef SecOCSPCacheCopyMatchingWithMinInsertTime(SecOCSPRequestRef request,
    CFURLRef localResponderURI, CFAbsoluteTime minInsertTime);

bool SecOCSPCacheFlush(CFErrorRef *error);

/* for testing purposes only */
bool SecOCSPCacheDeleteContent(CFErrorRef *error);
void SecOCSPCacheDeleteCache(void);
void SecOCSPCacheCloseDB(void);
CFStringRef SecOCSPCacheCopyPath(void);

__END_DECLS

#endif /* _SECURITY_SECOCSPCACHE_H_ */
