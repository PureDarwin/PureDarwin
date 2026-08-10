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

//
//  SOSRingTypes.h
//

#ifndef _sec_SOSRingTypes_
#define _sec_SOSRingTypes_


#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSRingUtils.h"

typedef struct ringfuncs_t {
    char                    *typeName;
    int                     version;
    SOSRingRef              (*sosRingCreate)(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error);
    bool                    (*sosRingResetToEmpty)(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error);
    bool                    (*sosRingResetToOffering)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    SOSRingStatus           (*sosRingDeviceIsInRing)(SOSRingRef ring, CFStringRef peerID);
    bool                    (*sosRingApply)(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingWithdraw)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingGenerationSign)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingConcordanceSign)(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    SOSConcordanceStatus    (*sosRingConcordanceTrust)(SOSFullPeerInfoRef me, CFSetRef peers,
                                                       SOSRingRef knownRing, SOSRingRef proposedRing,
                                                       SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                       CFStringRef excludePeerID, CFErrorRef *error);
    bool                    (*sosRingAccept)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingReject)(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    bool                    (*sosRingSetPayload)(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error);
    CFDataRef               (*sosRingGetPayload)(SOSRingRef ring, CFErrorRef *error);
} ringFuncStruct, *ringFuncs;

static inline SOSRingRef SOSRingCreate_ForType(CFStringRef name, SOSRingType type, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingRef retval = NULL;
    retval = SOSRingCreate_Internal(name, type, error);
    if(!retval) return NULL;
    SOSRingSetLastModifier(retval, myPeerID);
    return retval;
}

#endif /* defined(_sec_SOSRingTypes_) */
