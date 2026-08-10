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
 @header SOSDigestVector.h
 The functions provided in SOSDigestVector.h provide an interface doing
 cheap appending, lazy sorting and delta math on sorted arrays of same
 sized objects.
 */

#ifndef _SEC_SOSDIGESTVECTOR_H_
#define _SEC_SOSDIGESTVECTOR_H_

#include <corecrypto/ccsha1.h>
#include <CoreFoundation/CFError.h>
#include <sys/types.h>

__BEGIN_DECLS

enum {
    kSOSDigestVectorRemovalsLeftError = 1,
    kSOSDigestVectorUnorderedAddError = 2,
};

extern CFStringRef kSOSDigestVectorErrorDomain;

#define SOSDigestSize ((size_t)CCSHA1_OUTPUT_SIZE)

#define SOSDigestVectorInit { .digest = NULL, .count = 0, .capacity = 0, .unsorted = false }

typedef uint8_t (*SOSDigestVectorDigestPtr)[SOSDigestSize];

struct SOSDigestVector {
    uint8_t (*digest)[SOSDigestSize];
    size_t count;
    size_t capacity;
    bool unsorted;
};

typedef void (^SOSDigestVectorApplyBlock)(const uint8_t digest[SOSDigestSize], bool *stop);

/* SOSDigestVector. */
void SOSDigestVectorAppend(struct SOSDigestVector *dv, const uint8_t *digest);
void SOSDigestVectorAppendMultipleOrdered(struct SOSDigestVector *dv, size_t count,
                                   const uint8_t *digests);
void SOSDigestVectorSort(struct SOSDigestVector *dv);
void SOSDigestVectorSwap(struct SOSDigestVector *dva, struct SOSDigestVector *dvb);
size_t SOSDigestVectorIndexOf(struct SOSDigestVector *dv, const uint8_t *digest);
size_t SOSDigestVectorIndexOfSorted(const struct SOSDigestVector *dv, const uint8_t *digest);
bool SOSDigestVectorContains(struct SOSDigestVector *dv, const uint8_t *digest);
bool SOSDigestVectorContainsSorted(const struct SOSDigestVector *dv, const uint8_t *digest);
void SOSDigestVectorFree(struct SOSDigestVector *dv);

void SOSDigestVectorApply(struct SOSDigestVector *dv, SOSDigestVectorApplyBlock with);
void SOSDigestVectorApplySorted(const struct SOSDigestVector *dv, SOSDigestVectorApplyBlock with);
void SOSDigestVectorIntersectSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                                    struct SOSDigestVector *dvintersect);
void SOSDigestVectorUnionSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                                struct SOSDigestVector *dvunion);
void SOSDigestVectorUniqueSorted(struct SOSDigestVector *dv);

void SOSDigestVectorDiffSorted(const struct SOSDigestVector *dv1, const struct SOSDigestVector *dv2,
                               struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1);
void SOSDigestVectorDiff(struct SOSDigestVector *dv1, struct SOSDigestVector *dv2,
                         struct SOSDigestVector *dv1_2, struct SOSDigestVector *dv2_1);
void SOSDigestVectorComplementSorted(const struct SOSDigestVector *dvA, const struct SOSDigestVector *dvB,
                                     struct SOSDigestVector *dvcomplement);
bool SOSDigestVectorPatchSorted(const struct SOSDigestVector *base, const struct SOSDigestVector *removals,
                                const struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                                CFErrorRef *error);
bool SOSDigestVectorPatch(struct SOSDigestVector *base, struct SOSDigestVector *removals,
                          struct SOSDigestVector *additions, struct SOSDigestVector *dv,
                          CFErrorRef *error);

__END_DECLS

#endif /* !_SEC_SOSDIGESTVECTOR_H_ */
