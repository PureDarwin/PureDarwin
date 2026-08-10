/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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


#ifndef SOSPeerCoder_h
#define SOSPeerCoder_h
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#include "keychain/SecureObjectSync/SOSCoder.h"
#include "keychain/SecureObjectSync/SOSEngine.h"
enum SOSCoderUnwrapStatus{
    SOSCoderUnwrapError = 0,
    SOSCoderUnwrapDecoded = 1,
    SOSCoderUnwrapHandled = 2
};

bool SOSPeerCoderSendMessageIfNeeded(SOSAccount* account, SOSEngineRef engine, SOSTransactionRef txn, SOSPeerRef peer, SOSCoderRef coder, CFDataRef *message_to_send, CFStringRef peer_id, CFMutableArrayRef *attributeList, SOSEnginePeerMessageSentCallback **sentCallback, CFErrorRef *error);

enum SOSCoderUnwrapStatus SOSPeerHandleCoderMessage(SOSPeerRef peer, SOSCoderRef coder, CFStringRef peer_id, CFDataRef codedMessage, CFDataRef *decodedMessage, bool *forceSave, CFErrorRef *error);

bool SOSPeerSendMessageIfNeeded(SOSPeerRef peer, CFDataRef *message, CFDataRef *message_to_send, SOSCoderRef *coder, CFStringRef circle_id, CFStringRef peer_id, SOSEnginePeerMessageSentCallback **sentCallback, CFErrorRef *error);

#endif
