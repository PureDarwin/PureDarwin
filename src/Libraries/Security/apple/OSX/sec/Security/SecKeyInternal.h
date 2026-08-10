/*
 * Copyright (c) 2010-2015 Apple Inc. All Rights Reserved.
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
 @header SecKeyInternal
 */

#ifndef _SECURITY_SECKEYINTERNAL_H_
#define _SECURITY_SECKEYINTERNAL_H_

#include <Security/SecBase.h>

#include <Security/SecKeyPriv.h>
#include <corecrypto/ccrng.h>

__BEGIN_DECLS

#if TARGET_OS_OSX
void SecKeySetAuxilliaryCDSAKeyForKey(SecKeyRef cf, SecKeyRef auxKey);
SecKeyRef SecKeyCopyAuxilliaryCDSAKeyForKey(SecKeyRef cf);
#endif

extern struct ccrng_state *ccrng_seckey;

enum {
    // Keep in sync with SecKeyOperationType enum in SecKey.h
    kSecKeyOperationTypeCount = 5
};

typedef struct {
    SecKeyRef key;
    SecKeyOperationType operation;
    CFMutableArrayRef algorithm;
    SecKeyOperationMode mode;
} SecKeyOperationContext;

typedef CFTypeRef (*SecKeyAlgorithmAdaptor)(SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error);

void SecKeyOperationContextDestroy(SecKeyOperationContext *context);
CFTypeRef SecKeyRunAlgorithmAndCopyResult(SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error);
SecKeyAlgorithmAdaptor SecKeyGetAlgorithmAdaptor(SecKeyOperationType operation, SecKeyAlgorithm algorithm);

__END_DECLS

#endif /* !_SECURITY_SECKEYINTERNAL_H_ */
