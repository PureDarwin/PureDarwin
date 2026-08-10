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


#ifndef _SECOTRIDENTITYPRIV_H_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecKey.h>

#include <Security/oidsalg.h>

#include <CommonCrypto/CommonDigest.h> // DIGEST_LENGTH
#include <Security/SecOTR.h>

__BEGIN_DECLS
    
// OAEP Padding, uses lots of space. Might need this to be data
// Driven when we support more key types.
#define kPaddingOverhead (2 + 2 * CC_SHA1_DIGEST_LENGTH + 1)
    
//
// Identity opaque structs
//

#define kMPIDHashSize   CC_SHA1_DIGEST_LENGTH

struct _SecOTRFullIdentity {
    CFRuntimeBase _base;
    
    SecKeyRef   publicSigningKey;
    SecKeyRef   privateSigningKey;
    CFDataRef   privateKeyPersistentRef;

    bool        isMessageProtectionKey;
    uint8_t     publicIDHash[kMPIDHashSize];
};


struct _SecOTRPublicIdentity {
    CFRuntimeBase _base;
    
    SecKeyRef   publicSigningKey;

    bool        wantsHashes;

    uint8_t     hash[kMPIDHashSize];
};

enum SecOTRError {
    secOTRErrorLocal,
    secOTRErrorOSError,
};

extern const SecAsn1AlgId *kOTRSignatureAlgIDPtr;
void EnsureOTRAlgIDInited(void);
    
// Private functions for Public and Full IDs

bool SecOTRFIAppendSignature(SecOTRFullIdentityRef fullID,
                                CFDataRef dataToHash,
                                CFMutableDataRef appendTo,
                                CFErrorRef *error);

void SecOTRFIAppendPublicHash(SecOTRFullIdentityRef fullID, CFMutableDataRef appendTo);
bool SecOTRFIComparePublicHash(SecOTRFullIdentityRef fullID, const uint8_t hash[kMPIDHashSize]);

size_t SecOTRFISignatureSize(SecOTRFullIdentityRef privateID);

bool SecOTRFICompareToPublicKey(SecOTRFullIdentityRef fullID, SecKeyRef publicKey);

bool SecOTRPIVerifySignature(SecOTRPublicIdentityRef publicID,
                                const uint8_t *dataToHash, size_t amountToHash,
                                const uint8_t *signatureStart, size_t signatureSize, CFErrorRef *error);

bool SecOTRPIEqualToBytes(SecOTRPublicIdentityRef id, const uint8_t*bytes, CFIndex size);
bool SecOTRPIEqual(SecOTRPublicIdentityRef left, SecOTRPublicIdentityRef right);

size_t SecOTRPISignatureSize(SecOTRPublicIdentityRef publicID);
    
void SecOTRPICopyHash(SecOTRPublicIdentityRef publicID, uint8_t hash[kMPIDHashSize]);
void SecOTRPIAppendHash(SecOTRPublicIdentityRef publicID, CFMutableDataRef appendTo);

bool SecOTRPICompareHash(SecOTRPublicIdentityRef publicID, const uint8_t hash[kMPIDHashSize]);

bool SecOTRPICompareToPublicKey(SecOTRPublicIdentityRef publicID, SecKeyRef publicKey);


// Utility streaming functions
OSStatus insertSize(CFIndex size, uint8_t* here);
OSStatus appendSize(CFIndex size, CFMutableDataRef into);
OSStatus readSize(const uint8_t** data, size_t* limit, uint16_t* size);

OSStatus appendPublicOctets(SecKeyRef fromKey, CFMutableDataRef appendTo);
OSStatus appendPublicOctetsAndSize(SecKeyRef fromKey, CFMutableDataRef appendTo);
OSStatus appendSizeAndData(CFDataRef data, CFMutableDataRef appendTo);

SecKeyRef CreateECPublicKeyFrom(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);
    
bool SecOTRCreateError(enum SecOTRError family, CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError);

__END_DECLS

#endif
