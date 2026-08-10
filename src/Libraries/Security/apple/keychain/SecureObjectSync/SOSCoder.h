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


#ifndef _SEC_SOSCODER_H_
#define _SEC_SOSCODER_H_


#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>

typedef struct __OpaqueSOSCoder *SOSCoderRef;
typedef bool (^SOSPeerSendBlock)(CFDataRef message, CFErrorRef *error);

enum {
    kSOSCoderDataReturned = 0,
    kSOSCoderNegotiating = 1,
    kSOSCoderNegotiationCompleted = 2,
    kSOSCoderFailure = 3,
    kSOSCoderStaleEvent = 4,
    kSOSCoderTooNew = 5,
    kSOSCoderForceMessage = 6,
};

typedef uint32_t SOSCoderStatus;

CFTypeID SOSCoderGetTypeID(void);

SOSCoderRef SOSCoderCreate(SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo, CFBooleanRef useCompact, CFErrorRef *error);
SOSCoderRef SOSCoderCreateFromData(CFDataRef exportedData, CFErrorRef *error);

CFDataRef SOSCoderCopyDER(SOSCoderRef coder, CFErrorRef* error);

CFStringRef SOSCoderGetID(SOSCoderRef coder);

bool SOSCoderIsFor(SOSCoderRef coder, SOSPeerInfoRef peerInfo, SOSFullPeerInfoRef myPeerInfo);

SOSCoderStatus
SOSCoderStart(SOSCoderRef coder, CFErrorRef *error);

SOSCoderStatus
SOSCoderResendDH(SOSCoderRef coder, CFErrorRef *error);

void SOSCoderPersistState(CFStringRef peer_id, SOSCoderRef coder);

SOSCoderStatus SOSCoderUnwrap(SOSCoderRef coder, CFDataRef codedMessage, CFMutableDataRef *message,
                              CFStringRef clientId, CFErrorRef *error);

SOSCoderStatus SOSCoderWrap(SOSCoderRef coder, CFDataRef message, CFMutableDataRef *codedMessage, CFStringRef clientId, CFErrorRef *error);

bool SOSCoderCanWrap(SOSCoderRef coder);

void SOSCoderReset(SOSCoderRef coder);

CFDataRef SOSCoderCopyPendingResponse(SOSCoderRef coder);
void SOSCoderConsumeResponse(SOSCoderRef coder);
bool SOSCoderIsCoderInAwaitingState(SOSCoderRef coder);
    
#endif // _SEC_SOSCODER_H_
