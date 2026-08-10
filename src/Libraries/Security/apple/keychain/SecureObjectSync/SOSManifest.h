/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SOSManifest.h
 The functions provided in SOSTransport.h provide an interface to the
 secure object syncing transport
 */

#ifndef _SEC_SOSMANIFEST_H_
#define _SEC_SOSMANIFEST_H_

#include <corecrypto/ccsha1.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>

__BEGIN_DECLS

enum {
    kSOSManifestUnsortedError = 1,
    kSOSManifestCreateError = 2,
};

extern CFStringRef kSOSManifestErrorDomain;

/* SOSObject. */

/* Forward declarations of SOS types. */
typedef struct __OpaqueSOSManifest *SOSManifestRef;
struct SOSDigestVector;

/* SOSManifest. */
CFTypeID SOSManifestGetTypeID(void);

SOSManifestRef SOSManifestCreateWithBytes(const uint8_t *bytes, size_t len,
                                          CFErrorRef *error);
SOSManifestRef SOSManifestCreateWithDigestVector(struct SOSDigestVector *dv, CFErrorRef *error);
SOSManifestRef SOSManifestCreateWithData(CFDataRef data, CFErrorRef *error);

size_t SOSManifestGetSize(SOSManifestRef m);

size_t SOSManifestGetCount(SOSManifestRef m);

const uint8_t *SOSManifestGetBytePtr(SOSManifestRef m);

CFDataRef SOSManifestGetData(SOSManifestRef m);

const struct SOSDigestVector *SOSManifestGetDigestVector(SOSManifestRef manifest);

bool SOSManifestDiff(SOSManifestRef a, SOSManifestRef b,
                     SOSManifestRef *a_minus_b, SOSManifestRef *b_minus_a,
                     CFErrorRef *error);

SOSManifestRef SOSManifestCreateWithPatch(SOSManifestRef base,
                                          SOSManifestRef removals,
                                          SOSManifestRef additions,
                                          CFErrorRef *error);

// Returns the set of elements in B that are not in A.
// This is the relative complement of A in B (B\A), sometimes written B − A
SOSManifestRef SOSManifestCreateComplement(SOSManifestRef A,
                                           SOSManifestRef B,
                                           CFErrorRef *error);

SOSManifestRef SOSManifestCreateIntersection(SOSManifestRef m1,
                                             SOSManifestRef m2,
                                             CFErrorRef *error);


SOSManifestRef SOSManifestCreateUnion(SOSManifestRef m1,
                                      SOSManifestRef m2,
                                      CFErrorRef *error);

void SOSManifestForEach(SOSManifestRef m, void(^block)(CFDataRef e, bool *stop));

CFDataRef SOSManifestGetDigest(SOSManifestRef m, CFErrorRef *error);

__END_DECLS

#endif /* !_SEC_SOSMANIFEST_H_ */
