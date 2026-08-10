/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#ifndef SOSCIRCLE_PIGGIGGYBACK_H
#define SOSCIRCLE_PIGGIGGYBACK_H 1

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "keychain/SecureObjectSync/SOSCircle.h"

__BEGIN_DECLS

bool SOSPiggyBackBlobCreateFromData(SOSGenCountRef *gencount, SecKeyRef *pubKey, CFDataRef *signature,
                                    CFDataRef blobData, PiggyBackProtocolVersion version, bool *setInitialSyncTimeoutToV0, CFErrorRef *error);
bool SOSPiggyBackBlobCreateFromDER(SOSGenCountRef  *retGencount, SecKeyRef *retPubKey, CFDataRef *retSignature,
                                   const uint8_t** der_p, const uint8_t *der_end, PiggyBackProtocolVersion version, bool *setInitialSyncTimeoutToV0, CFErrorRef *error);
CFDataRef SOSPiggyBackBlobCopyEncodedData(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFErrorRef *error);

#if __OBJC__
bool SOSPiggyBackAddToKeychain(NSArray<NSData*>* identity, NSArray<NSDictionary*>*  tlk);
NSDictionary * SOSPiggyCopyInitialSyncData(const uint8_t** der, const uint8_t *der_end);
#endif

__END_DECLS

#endif /* SOSCIRCLE_PIGGIGGYBACK_H */
