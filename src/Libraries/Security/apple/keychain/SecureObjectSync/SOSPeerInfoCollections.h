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


#ifndef __SOSPEERINFOCOLLECTIONS__
#define __SOSPEERINFOCOLLECTIONS__

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <xpc/xpc.h>

//
// CFSet of PeerInfos by ID.
//
extern const CFSetCallBacks kSOSPeerSetCallbacks;
CFMutableSetRef CFSetCreateMutableForSOSPeerInfosByID(CFAllocatorRef allocator);
CFMutableSetRef CFSetCreateMutableForSOSPeerInfosByIDWithArray(CFAllocatorRef allocator, CFArrayRef peerInfos);

bool SOSPeerInfoSetContainsIdenticalPeers(CFSetRef set1, CFSetRef set2);
SOSPeerInfoRef SOSPeerInfoSetFindByID(CFSetRef set, CFStringRef id);

//
// Der encode
//
CFMutableSetRef SOSPeerInfoSetCreateFromArrayDER(CFAllocatorRef allocator, const CFSetCallBacks *callbacks, CFErrorRef* error,
                                                 const uint8_t** der_p, const uint8_t *der_end);
size_t SOSPeerInfoSetGetDEREncodedArraySize(CFSetRef pia, CFErrorRef *error);
uint8_t* SOSPeerInfoSetEncodeToArrayDER(CFSetRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

//
// CFArray of Peer Info handling
//

void CFArrayOfSOSPeerInfosSortByID(CFMutableArrayRef peerInfoArray);

//
// Peer Info Array Persistence
//

CFMutableArrayRef SOSPeerInfoArrayCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                                const uint8_t** der_p, const uint8_t *der_end);
size_t SOSPeerInfoArrayGetDEREncodedSize(CFArrayRef pia, CFErrorRef *error);
uint8_t* SOSPeerInfoArrayEncodeToDER(CFArrayRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);



CFArrayRef CreateArrayOfPeerInfoWithXPCObject(xpc_object_t peerArray, CFErrorRef* error);
xpc_object_t CreateXPCObjectWithArrayOfPeerInfo(CFArrayRef array, CFErrorRef *error);

#endif
