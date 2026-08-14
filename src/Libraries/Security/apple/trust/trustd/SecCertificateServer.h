/*
 * Copyright (c) 2017-2020 Apple Inc. All Rights Reserved.
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

/*
 * SecCertificateServer.h - SecCertificate and SecCertificatePath types
 *  with additonal validation context.
 */


#ifndef _SECURITY_SECCERTIFICATESERVER_H_
#define _SECURITY_SECCERTIFICATESERVER_H_

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecCertificate.h>

#include "trust/trustd/policytree.h"


typedef struct SecCertificateVC *SecCertificateVCRef;

SecCertificateVCRef SecCertificateVCCreate(SecCertificateRef certificate, CFArrayRef usageContraints);

typedef struct SecCertificatePathVC *SecCertificatePathVCRef;

/* Create a new certificate path from an old one. */
SecCertificatePathVCRef SecCertificatePathVCCreate(SecCertificatePathVCRef path,
                                               SecCertificateRef certificate, CFArrayRef usageConstraints);

SecCertificatePathVCRef SecCertificatePathVCCopyAddingLeaf(SecCertificatePathVCRef path,
                                                       SecCertificateRef leaf);

/* Return a new certificate path without the first skipCount certificates. */
SecCertificatePathVCRef SecCertificatePathVCCopyFromParent(SecCertificatePathVCRef path, CFIndex skipCount);

/* Create an array of SecCertificateRefs from a certificate path. */
CFArrayRef SecCertificatePathVCCopyCertificates(SecCertificatePathVCRef path);

/* Create an array of CFDataRefs from a certificate path. */
CFArrayRef SecCertificatePathVCCreateSerialized(SecCertificatePathVCRef path);

/* Record the fact that we found our own root cert as our parent
 certificate. */
void SecCertificatePathVCSetSelfIssued(SecCertificatePathVCRef certificatePath);
bool SecCertificatePathVCIsCertificateAtIndexSelfIssued(SecCertificatePathVCRef path, CFIndex ix);

void SecCertificatePathVCSetIsAnchored(SecCertificatePathVCRef certificatePath);

/* Return the index of the first non anchor certificate in the chain that is
 self signed counting from the leaf up.  Return -1 if there is none. */
CFIndex SecCertificatePathVCSelfSignedIndex(SecCertificatePathVCRef certificatePath);

Boolean SecCertificatePathVCIsAnchored(SecCertificatePathVCRef certificatePath);

void SecCertificatePathVCSetNextSourceIndex(SecCertificatePathVCRef certificatePath, CFIndex sourceIndex);

CFIndex SecCertificatePathVCGetNextSourceIndex(SecCertificatePathVCRef certificatePath);

CFIndex SecCertificatePathVCGetCount(SecCertificatePathVCRef certificatePath);

SecCertificateRef SecCertificatePathVCGetCertificateAtIndex(SecCertificatePathVCRef certificatePath, CFIndex ix);

void SecCertificatePathVCForEachCertificate(SecCertificatePathVCRef path, void(^operation)(SecCertificateRef certificate, bool *stop));

/* Return the index of certificate in path or kCFNotFound if certificate is
 not in path. */
CFIndex SecCertificatePathVCGetIndexOfCertificate(SecCertificatePathVCRef path,
                                                SecCertificateRef certificate);

/* Return the root certificate for certificatePath.  Note that root is just
 the top of the path as far as it is constructed.  It may or may not be
 trusted or self signed.  */
SecCertificateRef SecCertificatePathVCGetRoot(SecCertificatePathVCRef certificatePath);

CFArrayRef SecCertificatePathVCGetUsageConstraintsAtIndex(SecCertificatePathVCRef certificatePath, CFIndex ix);

void SecCertificatePathVCSetUsageConstraintsAtIndex(SecCertificatePathVCRef certificatePath,
                                                  CFArrayRef newConstraints, CFIndex ix);

SecKeyRef SecCertificatePathVCCopyPublicKeyAtIndex(SecCertificatePathVCRef certificatePath, CFIndex ix);

typedef CFIndex SecPathVerifyStatus;
enum {
    kSecPathVerifiesUnknown = -1,
    kSecPathVerifySuccess = 0,
    kSecPathVerifyFailed = 1
};

SecPathVerifyStatus SecCertificatePathVCVerify(SecCertificatePathVCRef certificatePath);

bool SecCertificatePathVCIsCycleInGraph(SecCertificatePathVCRef path);

bool SecCertificatePathVCIsValid(SecCertificatePathVCRef certificatePath, CFAbsoluteTime verifyTime);

bool SecCertificatePathVCHasWeakHash(SecCertificatePathVCRef certificatePath);

bool SecCertificatePathVCHasWeakKeySize(SecCertificatePathVCRef certificatePath);

CFAbsoluteTime SecCertificatePathVCGetMaximumNotBefore(SecCertificatePathVCRef certificatePath);
CFAbsoluteTime SecCertificatePathVCGetMinimumNotAfter(SecCertificatePathVCRef certificatePath);

/* Score */
CFIndex SecCertificatePathVCScore(SecCertificatePathVCRef certificatePath,
                                CFAbsoluteTime verifyTime);
CFIndex SecCertificatePathVCGetScore(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetScore(SecCertificatePathVCRef certificatePath, CFIndex score); // only sets score if new score is higher
void SecCertificatePathVCResetScore(SecCertificatePathVCRef certificatePath); // reset score to 0

/* Revocation */
void SecCertificatePathVCDeleteRVCs(SecCertificatePathVCRef path);
bool SecCertificatePathVCIsRevocationDone(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCAllocateRVCs(SecCertificatePathVCRef certificatePath, CFIndex certCount);
CFAbsoluteTime SecCertificatePathVCGetEarliestNextUpdate(SecCertificatePathVCRef path);
CFAbsoluteTime SecCertificatePathVCGetLatestThisUpdate(SecCertificatePathVCRef path);
void *SecCertificatePathVCGetRVCAtIndex(SecCertificatePathVCRef certificatePath, CFIndex ix); // Returns a SecRVCRef
bool SecCertificatePathVCRevocationCheckedAllCerts(SecCertificatePathVCRef path);
bool SecCertificatePathVCIsRevocationRequiredForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                   CFIndex ix);
void SecCertificatePathVCSetRevocationRequiredForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                    CFIndex ix);
void SecCertificatePathVCSetRevocationReasonForCertificateAtIndex(SecCertificatePathVCRef certificatePath,
                                                                  CFIndex ix, CFNumberRef revocationReason);
CFNumberRef SecCertificatePathVCGetRevocationReason(SecCertificatePathVCRef certificatePath); // returns first revocation reason found

bool SecCertificatePathVCCheckedIssuers(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetCheckedIssuers(SecCertificatePathVCRef certificatePath, bool checked);
CFIndex SecCertificatePathVCUnknownCAIndex(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetUnknownCAIndex(SecCertificatePathVCRef certificatePath, CFIndex index);

/* Did we already validate this path (setting EV, CT, RVC, etc.) */
bool SecCertificatePathVCIsPathValidated(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetPathValidated(SecCertificatePathVCRef certificatePath);

/* EV */
bool SecCertificatePathVCIsEV(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetIsEV(SecCertificatePathVCRef certificatePath, bool isEV);
bool SecCertificatePathVCIsOptionallyEV(SecCertificatePathVCRef certificatePath);

/* CT */
typedef CFIndex SecPathCTPolicy;
enum {
    kSecPathCTNotRequired = 0,
    kSecPathCTRequiredOverridable = 1,
    kSecPathCTRequired = 2
};
bool SecCertificatePathVCIsCT(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetIsCT(SecCertificatePathVCRef certificatePath, bool isCT);
SecPathCTPolicy SecCertificatePathVCRequiresCT(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetRequiresCT(SecCertificatePathVCRef certificatePath, SecPathCTPolicy requiresCT);
CFAbsoluteTime SecCertificatePathVCIssuanceTime(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetIssuanceTime(SecCertificatePathVCRef certificatePath, CFAbsoluteTime issuanceTime);

/* CA Revocation Additions */
/* Returns the index of the highest issuing CA which has matching key-based
 * revocation additions in the given path, or kCFNotFound if none is found.
 */
CFIndex SecCertificatePathVCIndexOfCAWithRevocationAdditions(SecCertificatePathVCRef certificatePath);

/* Allowlist */
bool SecCertificatePathVCIsAllowlisted(SecCertificatePathVCRef certificatePath);
void SecCertificatePathVCSetIsAllowlisted(SecCertificatePathVCRef certificatePath, bool isAllowlisted);

/* Policy Tree */
bool SecCertificatePathVCVerifyPolicyTree(SecCertificatePathVCRef path, bool anchor_trusted);

#endif /* _SECURITY_SECCERTIFICATESERVER_H_ */
