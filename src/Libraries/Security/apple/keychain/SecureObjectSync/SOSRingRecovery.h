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
 */

//
//  SOSRingRecovery.h
//  sec
//

#ifndef SOSRecoveryRing_h
#define SOSRecoveryRing_h


#include "SOSRingTypes.h"
#include <CoreFoundation/CoreFoundation.h>
#include "keychain/SecureObjectSync/SOSRecoveryKeyBag.h"

extern ringFuncStruct recovery;

static inline CFDataRef SOSRecoveryRingGetKeyBag(SOSRingRef ring, CFErrorRef *error) {
    if(SOSRingGetType(ring) != kSOSRingRecovery) return NULL;
    return SOSRingGetPayload(ring, error);
}

bool SOSRingSetRecoveryKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error);
SOSRecoveryKeyBagRef SOSRingCopyRecoveryKeyBag(SOSRingRef ring, CFErrorRef *error);

#endif /* SOSRecoveryRing_h */
