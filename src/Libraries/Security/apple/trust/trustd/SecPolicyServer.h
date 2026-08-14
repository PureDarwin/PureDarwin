/*
 * Copyright (c) 2008-2010,2012-2017 Apple Inc. All Rights Reserved.
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
    @header SecPolicyServer
    The functions provided in SecPolicyServer.h provide an interface to
    trust policies dealing with certificate revocation.
*/

#ifndef _SECURITY_SECPOLICYSERVER_H_
#define _SECURITY_SECPOLICYSERVER_H_

#include <Security/SecTrust.h>
#include "Security/SecPolicyInternal.h"
#include <Security/SecTrustSettings.h>

#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecCertificateServer.h"

__BEGIN_DECLS

#define kSecPolicySHA256Size 32

void SecPVCInit(SecPVCRef pvc, SecPathBuilderRef builder, CFArrayRef policies);
void SecPVCDelete(SecPVCRef pvc);
void SecPVCSetPath(SecPVCRef pvc, SecCertificatePathVCRef path);
SecPolicyRef SecPVCGetPolicy(SecPVCRef pv);
SecCertificateRef SecPVCGetCertificateAtIndex(SecPVCRef pvc, CFIndex ix);
CFIndex SecPVCGetCertificateCount(SecPVCRef pvc);
CFAbsoluteTime SecPVCGetVerifyTime(SecPVCRef pvc);

/* Set the string result as the reason for the sub policy check key
   failing.  The policy check function should continue processing if
   this function returns true. */
bool SecPVCSetResult(SecPVCRef pv, CFStringRef key, CFIndex ix, CFTypeRef result);
bool SecPVCSetResultForced(SecPVCRef pvc, CFStringRef key, CFIndex ix, CFTypeRef result, bool force);
bool SecPVCSetResultForcedWithTrustResult(SecPVCRef pvc, CFStringRef key, CFIndex ix, CFTypeRef result, bool force,
                                          SecTrustResultType overrideDefaultTR);

/* Is the current result considered successful. */
bool SecPVCIsOkResult(SecPVCRef pvc);

/* Compute details */
void SecPVCComputeDetails(SecPVCRef pvc, SecCertificatePathVCRef path);

/* Run static leaf checks on the path in pvc. */
SecTrustResultType SecPVCLeafChecks(SecPVCRef pvc);

/* Run static parent checks on the path in pvc. */
bool SecPVCParentCertificateChecks(SecPVCRef pvc, CFIndex ix);

/* Run dynamic checks on the complete path in pvc.  Return true if the
   operation is complete, returns false if an async backgroup request was
   scheduled.  Upon completion of the async background job
   SecPathBuilderStep() should be called. */
void SecPVCPathChecks(SecPVCRef pvc);

/* Check whether revocation responses were received for certificates
 * in the path in pvc. If a valid response was not obtained for a
 * certificate, this sets the appropriate error result if revocation
 * was required, and/or definitive revocation info is present. */
void SecPVCPathCheckRevocationResponsesReceived(SecPVCRef pvc);

typedef void (*SecPolicyCheckFunction)(SecPVCRef pv, CFStringRef key);

/*
 * Used by SecTrust to verify if a particular certificate chain matches
 * this policy.  Returns true if the policy accepts the certificate chain.
*/
bool SecPolicyValidate(SecPolicyRef policy, SecPVCRef pvc, CFStringRef key);

void SecPolicyServerInitialize(void);

bool SecPolicyIsEVPolicy(const DERItem *policyOID);

bool SecPVCIsAnchorPerConstraints(SecPVCRef pvc, SecCertificateSourceRef source, SecCertificateRef certificate);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYSERVER_H_ */
