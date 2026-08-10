/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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

#ifndef _SECOTRDHKEY_H_
#define _SECOTRDHKEY_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <corecrypto/ccn.h>
#include <corecrypto/ccsha1.h>

__BEGIN_DECLS

typedef struct _SecOTRFullDHKey* SecOTRFullDHKeyRef;
typedef struct _SecOTRPublicDHKey* SecOTRPublicDHKeyRef;

SecOTRFullDHKeyRef SecOTRFullDHKCreate(CFAllocatorRef allocator);
SecOTRFullDHKeyRef SecOTRFullDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size);

OSStatus SecFDHKNewKey(SecOTRFullDHKeyRef key);
void SecFDHKAppendSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo);
void SecFDHKAppendPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo);
void SecFDHKAppendCompactPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo);

static const size_t kSecDHKHashSize = CCSHA1_OUTPUT_SIZE;

uint8_t* SecFDHKGetHash(SecOTRFullDHKeyRef pubKey);


SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromFullKey(CFAllocatorRef allocator, SecOTRFullDHKeyRef full);
SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromSerialization(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size);
SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromCompactSerialization(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size);
SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size);

void SecPDHKAppendSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo);
void SecPDHKAppendCompactSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo);
uint8_t* SecPDHKGetHash(SecOTRPublicDHKeyRef pubKey);

void SecPDHKeyGenerateS(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey, cc_unit* s);

bool SecDHKIsGreater(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey);

void SecOTRDHKGenerateOTRKeys(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey,
                           uint8_t* sendMessageKey, uint8_t* sendMacKey,
                           uint8_t* receiveMessageKey, uint8_t* receiveMacKey);

__END_DECLS

#endif
