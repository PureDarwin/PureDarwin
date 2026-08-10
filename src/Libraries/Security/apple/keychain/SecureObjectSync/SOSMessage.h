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


/*!
 @header SOSMessage.h
 This provides interfaces to the encoding and decoding of peer to peer
 messages in the Secure Object Syncing protocol.
 SOSMessageRef is a CFTypeRef.
 */

#ifndef _SEC_SOSMESSAGE_H_
#define _SEC_SOSMESSAGE_H_

#include "keychain/SecureObjectSync/SOSDataSource.h"
#include "keychain/SecureObjectSync/SOSManifest.h"

__BEGIN_DECLS

enum SOSMessageFlags {
    kSOSMessageGetObjects                       = (0),
    kSOSMessageJoinRequest                      = (1),
    kSOSMessagePartial                          = (2),
    kSOSMessageDigestTypesProposed              = (3),
    kSOSMessageClearGetObjects                  = (4),
    kSOSMessageDidClearGetObjectsSinceLastDelta = (5),
    kSOSMessageSkipHello                        = (6),
};
typedef uint64_t SOSMessageFlags;

enum SOSDigestTypes {
    kSOSDigestTypeSHA1                          = (0),
    kSOSDigestTypeDefault                       = kSOSDigestTypeSHA1,
    kSOSDigestTypeSHA224                        = (1),
    kSOSDigestTypeSHA256                        = (2),
    kSOSDigestTypeSHA384                        = (3),
    kSOSDigestTypeSHA512                        = (4),
};
typedef uint64_t SOSDigestTypes;

/* SOSMessage interface. */
typedef struct __OpaqueSOSMessage *SOSMessageRef;

#define kSOSMessageMaxObjectsSize (64000)
#define kSOSBackupMaxFileSize (64000)

#define kEngineMessageProtocolVersion 2

//
// MARK: SOSMessage encoding
//

// Create an SOSMessage ready to be encoded.
SOSMessageRef SOSMessageCreate(CFAllocatorRef allocator, uint64_t version, CFErrorRef *error);

SOSMessageRef SOSMessageCreateWithManifests(CFAllocatorRef allocator, SOSManifestRef sender,
                                            SOSManifestRef base, SOSManifestRef proposed,
                                            bool includeManifestDeltas, CFErrorRef *error);

bool SOSMessageSetManifests(SOSMessageRef message, SOSManifestRef sender,
                            SOSManifestRef base, SOSManifestRef proposed,
                            bool includeManifestDeltas, SOSManifestRef objectsSent,
                            CFErrorRef *error);


// Add an extension to this message
void SOSMessageAddExtension(SOSMessageRef message, CFDataRef oid, bool isCritical, CFDataRef extension);

bool SOSMessageAppendObject(SOSMessageRef message, CFDataRef object, CFErrorRef *error);

void SOSMessageSetFlags(SOSMessageRef message, SOSMessageFlags flags);

// Encode an SOSMessage, calls addObject callback and appends returned objects
// one by one, until addObject returns NULL.
CFDataRef SOSMessageCreateData(SOSMessageRef message, uint64_t sequenceNumber, CFErrorRef *error);

//
// MARK: SOSMessage decoding
//

// Decode a SOSMessage
SOSMessageRef SOSMessageCreateWithData(CFAllocatorRef allocator, CFDataRef derData, CFErrorRef *error);

// Read values from a decoded messgage

CFDataRef SOSMessageGetBaseDigest(SOSMessageRef message);

CFDataRef SOSMessageGetProposedDigest(SOSMessageRef message);

CFDataRef SOSMessageGetSenderDigest(SOSMessageRef message);

SOSMessageFlags SOSMessageGetFlags(SOSMessageRef message);

uint64_t SOSMessageGetSequenceNumber(SOSMessageRef message);

SOSManifestRef SOSMessageGetRemovals(SOSMessageRef message);

SOSManifestRef SOSMessageGetAdditions(SOSMessageRef message);

// Iterate though the extensions in a decoded SOSMessage.  If criticalOnly is
// true all non critical extensions are skipped.
void SOSMessageWithExtensions(SOSMessageRef message, bool criticalOnly,
                              void(^withExtension)(CFDataRef oid, bool isCritical,
                                                   CFDataRef extension, bool *stop));

size_t SOSMessageCountObjects(SOSMessageRef message);

// Iterate though the objects in a decoded SOSMessage.
bool SOSMessageWithObjects(SOSMessageRef message, CFErrorRef *error,
                           void(^withObject)(CFDataRef object, bool *stop));

bool SOSMessageWithSOSObjects(SOSMessageRef message, SOSDataSourceRef dataSource, CFErrorRef *error,
                           void(^withObject)(SOSObjectRef object, bool *stop));

__END_DECLS

#endif /* _SEC_SOSMESSAGE_H_ */
