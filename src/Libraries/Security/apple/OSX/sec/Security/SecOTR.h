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


#ifndef _SECOTR_H_
#define _SECOTR_H_

/*
 * Message Protection interfaces
*/

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <Security/SecKey.h>

#include <stdint.h>

__BEGIN_DECLS

/*!
    @typedef 
    @abstract   Full identity (public and private) for Message Protection
    @discussion Abstracts what kind of crypto is going on beyond it being public/priate
*/
typedef struct _SecOTRFullIdentity* SecOTRFullIdentityRef;

/*!
     @typedef 
     @abstract   Public identity for Message Protection message validation and encryption to send
     @discussion Abstracts what kind of crypto is going on beyond it being public/priate
 */
typedef struct _SecOTRPublicIdentity* SecOTRPublicIdentityRef;

/*
 * Full identity functions
 */
SecOTRFullIdentityRef SecOTRFullIdentityCreate(CFAllocatorRef allocator, CFErrorRef *error);

// This variant is used by MessageProtection that doesn't use persistent references anymore.
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                            CFErrorRef *error);

// This variant is used by SOS, and still relies on privateKey having a persistent reference.
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRefSOS(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                                CFErrorRef *error);
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef serializedData, CFErrorRef *error);
    
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t *size, CFErrorRef *error);

bool SecOTRFIPurgeFromKeychain(SecOTRFullIdentityRef thisID, CFErrorRef *error);

bool SecOTRFIAppendSerialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto, CFErrorRef *error);


bool SecOTRFIPurgeAllFromKeychain(CFErrorRef *error);


/*
 * Public identity functions
 */
SecOTRPublicIdentityRef SecOTRPublicIdentityCopyFromPrivate(CFAllocatorRef allocator, SecOTRFullIdentityRef fullID, CFErrorRef *error);
    
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef publicKey,
                                                                    CFErrorRef *error);
    
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef serializedData, CFErrorRef *error);
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t * size, CFErrorRef *error);

bool SecOTRPIAppendSerialization(SecOTRPublicIdentityRef publicID, CFMutableDataRef serializeInto, CFErrorRef *error);

void SecOTRAdvertiseHashes(bool advertise);
    
__END_DECLS
        
#endif
