/*
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
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


#ifndef SecAppleAnchor_c
#define SecAppleAnchor_c

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef CF_OPTIONS(uint32_t, SecAppleTrustAnchorFlags) {
    kSecAppleTrustAnchorFlagsIncludeTestAnchors    = 1 << 0,
    kSecAppleTrustAnchorFlagsAllowNonProduction    = 1 << 1,
};

/*
 * Return true if the certificate is an the Apple Trust anchor.
 */
bool
SecIsAppleTrustAnchor(SecCertificateRef cert,
                      SecAppleTrustAnchorFlags flags);

bool
SecIsAppleTrustAnchorData(CFDataRef cert,
			  SecAppleTrustAnchorFlags flags);

CFArrayRef SecGetAppleTrustAnchors(bool allowNonProduction);

__END_DECLS


#endif /* SecAppleAnchor */
