/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * SecCertificateSource.h - certificate sources for trust evaluation engine
 *
 */

#ifndef _SECURITY_SECCERTIFICATESOURCE_H_
#define _SECURITY_SECCERTIFICATESOURCE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>

/********************************************************
 ************ SecCertificateSource object ***************
 ********************************************************/
typedef struct SecCertificateSource *SecCertificateSourceRef;

typedef void(*SecCertificateSourceParents)(void *, CFArrayRef);

typedef bool(*CopyParents)(SecCertificateSourceRef source,
                           SecCertificateRef certificate,
                           void *context, SecCertificateSourceParents);

typedef CFArrayRef(*CopyConstraints)(SecCertificateSourceRef source,
                                     SecCertificateRef certificate);

typedef bool(*Contains)(SecCertificateSourceRef source,
                        SecCertificateRef certificate);

struct SecCertificateSource {
    CopyParents		copyParents;
    CopyConstraints	copyUsageConstraints;
    Contains		contains;
};

bool SecCertificateSourceCopyParents(SecCertificateSourceRef source,
                                     SecCertificateRef certificate,
                                     void *context, SecCertificateSourceParents callback);

CFArrayRef SecCertificateSourceCopyUsageConstraints(SecCertificateSourceRef source,
                                                    SecCertificateRef certificate);

bool SecCertificateSourceContains(SecCertificateSourceRef source,
                                  SecCertificateRef certificate);

/********************************************************
 ********************** Sources *************************
 ********************************************************/

/* SecItemCertificateSource */
SecCertificateSourceRef SecItemCertificateSourceCreate(CFArrayRef accessGroups);
void SecItemCertificateSourceDestroy(SecCertificateSourceRef source);

/* SecMemoryCertificateSource*/
SecCertificateSourceRef SecMemoryCertificateSourceCreate(CFArrayRef certificates);
void SecMemoryCertificateSourceDestroy(SecCertificateSourceRef source);

/* SecSystemAnchorSource */
extern const SecCertificateSourceRef kSecSystemAnchorSource;

/* SecUserAnchorSource */
extern const SecCertificateSourceRef kSecUserAnchorSource;

/* SecCAIssuerCertificateSource */
extern const SecCertificateSourceRef kSecCAIssuerSource;

#if TARGET_OS_OSX
/* SecLegacyCertificateSource */
extern const SecCertificateSourceRef kSecLegacyCertificateSource;

/* SecLegacyAnchorSource */
extern const SecCertificateSourceRef kSecLegacyAnchorSource;
#endif

#endif /* _SECURITY_SECCERTIFICATESOURCE_H_ */
