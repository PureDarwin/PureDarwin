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


#include <Security/SecOTRSession.h>

#include <CoreFoundation/CFData.h>

#ifndef _SECOTRPACKETS_H_
#define _SECOTRPACKETS_H_

void SecOTRAppendDHMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendDHKeyMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendRevealSignatureMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendSignatureMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);

typedef enum {
    kDHMessage = 0x02,
    kDataMessage = 0x03,
    kDHKeyMessage = 0x0A,
    kRevealSignatureMessage = 0x11,
    kSignatureMessage = 0x12,

    kEvenCompactDataMessage = 0x20,
    kOddCompactDataMessage = 0x21,
    
    kEvenCompactDataMessageWithHashes = 0x30,
    kOddCompactDataMessageWithHashes = 0x31,

    kInvalidMessage = 0xFF
} OTRMessageType;

#endif
