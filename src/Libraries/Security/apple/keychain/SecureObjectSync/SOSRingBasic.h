//
//  SOSRingBasic.h
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#ifndef _sec_SOSRingBasic_
#define _sec_SOSRingBasic_

#include "SOSRingTypes.h"

SOSRingRef SOSRingCreate_Basic(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error);
bool SOSRingResetToEmpty_Basic(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error);
SOSRingStatus SOSRingDeviceIsInRing_Basic(SOSRingRef ring, CFStringRef peerID);

bool SOSRingResetToOffering_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingApply_Basic(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingWithdraw_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingGenerationSign_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingConcordanceSign_Basic(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSRingSetPayload_Basic(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error);
CFDataRef SOSRingGetPayload_Basic(SOSRingRef ring, CFErrorRef *error);

extern ringFuncStruct basic;

#endif /* defined(_sec_SOSRingBasic_) */
