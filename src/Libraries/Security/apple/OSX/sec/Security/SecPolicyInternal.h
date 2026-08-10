/*
 * Copyright (c) 2008-2017 Apple Inc. All Rights Reserved.
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
    @header SecPolicyPriv
    The functions provided in SecPolicyInternal provide the interface to
    trust policies used by SecTrust.
*/

#ifndef _SECURITY_SECPOLICYINTERNAL_H_
#define _SECURITY_SECPOLICYINTERNAL_H_

#include <xpc/xpc.h>

#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRuntime.h>

__BEGIN_DECLS

/********************************************************
 ****************** SecPolicy struct ********************
 ********************************************************/
struct __SecPolicy {
    CFRuntimeBase		_base;
    CFStringRef			_oid;
    CFStringRef		_name;
	CFDictionaryRef		_options;
};

SecPolicyRef SecPolicyCreate(CFStringRef oid, CFStringRef name, CFDictionaryRef options);

CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy);

xpc_object_t SecPolicyArrayCopyXPCArray(CFArrayRef policies, CFErrorRef *error);

CFArrayRef SecPolicyArrayCreateDeserialized(CFArrayRef serializedPolicies);
CFArrayRef SecPolicyArrayCreateSerialized(CFArrayRef policies);

/*
 * MARK: SecLeafPVC functions
 */

typedef struct OpaqueSecLeafPVC *SecLeafPVCRef;

struct OpaqueSecLeafPVC {
    SecCertificateRef leaf;
    CFArrayRef policies;
    CFAbsoluteTime verifyTime;
    CFArrayRef details;
    CFMutableDictionaryRef info;
    CFDictionaryRef callbacks;
    CFIndex policyIX;
    bool result;
};

void SecLeafPVCInit(SecLeafPVCRef pvc, SecCertificateRef leaf, CFArrayRef policies, CFAbsoluteTime verifyTime);
void SecLeafPVCDelete(SecLeafPVCRef pvc);
bool SecLeafPVCLeafChecks(SecLeafPVCRef pvc);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYINTERNAL_H_ */
