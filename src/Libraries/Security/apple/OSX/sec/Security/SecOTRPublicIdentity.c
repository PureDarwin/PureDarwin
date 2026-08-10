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


#include "SecOTR.h"
#include "SecOTRIdentityPriv.h"
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccder.h>

#import <sys/syslog.h>

#include "SecOTRErrors.h"
#include <TargetConditionals.h>

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

/* 
 * Support for encoding and decoding DH parameter blocks.
 * Apple form encodes the reciprocal of the prime p.
 */

enum {
    kOTRPIDER_SigningID = 1,
    kOTRPIDER_SupportsHashes =3,
};

//
// SecOTRPublicIdentity implementation
//

CFGiblisFor(SecOTRPublicIdentity);

static bool sAdvertiseHashes = false;

static CF_RETURNS_RETAINED CFStringRef SecOTRPublicIdentityCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecOTRPublicIdentityRef requestor = (SecOTRPublicIdentityRef)cf;
   return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRPublicIdentity: %p %02x%02x%02x%02x%02x%02x%02x%02x>"),
                                    requestor,
                                    requestor->hash[0], requestor->hash[1],
                                    requestor->hash[2], requestor->hash[3],
                                    requestor->hash[4], requestor->hash[5],
                                    requestor->hash[6], requestor->hash[7]);
}

static void SecOTRPublicIdentityDestroy(CFTypeRef cf) {
    SecOTRPublicIdentityRef requestor = (SecOTRPublicIdentityRef)cf;
    
    CFReleaseNull(requestor->publicSigningKey);
}

static bool SecKeyDigestAndVerifyWithError(
                                           SecKeyRef           key,            /* Public key */
                                           const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                                           const uint8_t       *dataToDigest,	/* signature over this data */
                                           size_t              dataToDigestLen,/* length of dataToDigest */
                                           uint8_t             *sig,			/* signature to verify */
                                           size_t              sigLen,         /* length of sig */
                                           CFErrorRef          *error) {
    
    OSStatus status = SecKeyDigestAndVerify(key, algId, dataToDigest, dataToDigestLen, sig, sigLen);
    require_noerr(status, fail);
    return true;
fail:
    SecOTRCreateError(secOTRErrorOSError, status, CFSTR("Error verifying message. OSStatus in error code."), NULL, error);
    return false;
}

static bool SecOTRPICacheHash(SecOTRPublicIdentityRef pubID, CFErrorRef *error)
{
    bool result = false;
    
    CFMutableDataRef stream = CFDataCreateMutable(NULL, 0);

    require(SecOTRPIAppendSerialization(pubID, stream, error), fail);

    CCDigest(kCCDigestSHA1, CFDataGetBytePtr(stream), (CC_LONG)CFDataGetLength(stream), pubID->hash);
    
    result = true;

fail:
    CFReleaseSafe(stream);
    return result;
}

void SecOTRAdvertiseHashes(bool advertise) {
    sAdvertiseHashes = advertise;
}

bool SecOTRPICompareToPublicKey(SecOTRPublicIdentityRef pubID, SecKeyRef publicKey) {
    return CFEqualSafe(pubID->publicSigningKey, publicKey);
}

SecOTRPublicIdentityRef SecOTRPublicIdentityCopyFromPrivate(CFAllocatorRef allocator, SecOTRFullIdentityRef fullID, CFErrorRef *error)
{
    SecOTRPublicIdentityRef result = CFTypeAllocate(SecOTRPublicIdentity, struct _SecOTRPublicIdentity, allocator);

    EnsureOTRAlgIDInited();

    result->publicSigningKey = fullID->publicSigningKey;
    CFRetain(result->publicSigningKey);
    
    require(SecOTRPICacheHash(result, error), fail);

    return result;

fail:
    CFReleaseSafe(result);
    return NULL;
}


SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef publicKey,
                                                            CFErrorRef *error) {
    // TODO - make sure this is an appropriate key type
    SecOTRPublicIdentityRef result = CFTypeAllocate(SecOTRPublicIdentity, struct _SecOTRPublicIdentity, allocator);
    result->publicSigningKey = publicKey;
    CFRetain(result->publicSigningKey);
    require(SecOTRPICacheHash(result, error), fail);
    return result;
fail:
    CFRelease(result->publicSigningKey);
    CFReleaseSafe(result);
    return NULL;
}

typedef SecKeyRef (*SecOTRPublicKeyCreateFunction)(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);

static SecKeyRef SecOTRCreatePublicKeyFrom(const uint8_t* keyData, size_t keyDataSize, ccder_tag tagContainingKey, SecOTRPublicKeyCreateFunction createFunction)
{
    SecKeyRef createdKey = NULL;
    
    createdKey = createFunction(kCFAllocatorDefault, &keyData, &keyDataSize);
    
    require(createdKey != NULL, fail);
    require(keyDataSize == 0, fail);

    return createdKey;

fail:
    CFReleaseSafe(createdKey);
    return NULL;
}
                                          

SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t* size, CFErrorRef *error)
{
    CFErrorRef stackedError = NULL;
    
    SecOTRPublicIdentityRef newID = CFTypeAllocate(SecOTRPublicIdentity, struct _SecOTRPublicIdentity, allocator);
    
    EnsureOTRAlgIDInited();
    
    const uint8_t* fullSequenceEnd = *bytes + *size;

    const uint8_t* keyData = ccder_decode_sequence_tl(&fullSequenceEnd, *bytes, fullSequenceEnd);
    require(keyData != NULL, fail);
    size_t fullSize = (size_t)(fullSequenceEnd - *bytes);
    
    size_t   keyDataSize;
    keyData = ccder_decode_tl(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SigningID, &keyDataSize, keyData, fullSequenceEnd);
    newID->publicSigningKey = SecOTRCreatePublicKeyFrom(keyData, keyDataSize, kOTRPIDER_SigningID, &CreateECPublicKeyFrom);
    require(newID->publicSigningKey != NULL, fail);
    keyData += keyDataSize;
    
    newID->wantsHashes = (NULL != ccder_decode_tl(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SupportsHashes, &keyDataSize, keyData, fullSequenceEnd));

    require(SecOTRPICacheHash(newID, &stackedError), fail);
    
    *bytes += fullSize;
    *size -= fullSize;

    return newID;
    
fail:
    SecOTRCreateError(secOTRErrorLocal, kSecOTRErrorCreatePublicIdentity, CFSTR("Error creating public identity from bytes"), stackedError, error);
    CFReleaseSafe(newID);
    return NULL;
}

SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef serializedData, CFErrorRef *error)
{
    if (serializedData == NULL)
        return NULL;

    size_t length = (size_t)CFDataGetLength(serializedData);
    const uint8_t* bytes = CFDataGetBytePtr(serializedData);
    return SecOTRPublicIdentityCreateFromBytes(allocator, &bytes, &length, error);
}

bool SecOTRPIEqualToBytes(SecOTRPublicIdentityRef id, const uint8_t*bytes, CFIndex size)
{
    CFDataRef dataToMatch = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes, size, kCFAllocatorNull);
    CFMutableDataRef idStreamed = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    SecOTRPIAppendSerialization(id, idStreamed, NULL);
    
    bool equal = CFEqualSafe(dataToMatch, idStreamed);

    if (!equal) {
        CFDataPerformWithHexString(dataToMatch, ^(CFStringRef dataToMatchString) {
            CFDataPerformWithHexString(idStreamed, ^(CFStringRef idStreamedString) {
                secnotice("otr", "ID Comparison failed: d: %@ id: %@", dataToMatchString, idStreamedString);
            });
        });
    }

    CFReleaseNull(dataToMatch);
    CFReleaseNull(idStreamed);
    
    return equal;
}

bool SecOTRPIEqual(SecOTRPublicIdentityRef left, SecOTRPublicIdentityRef right)
{
    if (left == right)
        return true;

    CFMutableDataRef leftData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFMutableDataRef rightData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    SecOTRPIAppendSerialization(left, leftData, NULL);
    SecOTRPIAppendSerialization(right, rightData, NULL);
    
    bool match = CFEqualSafe(leftData, rightData);
    
    CFReleaseNull(leftData);
    CFReleaseNull(rightData);
    
    return match;
}

size_t SecOTRPISignatureSize(SecOTRPublicIdentityRef publicID)
{
    return SecKeyGetSize(publicID->publicSigningKey, kSecKeySignatureSize);
}

bool SecOTRPIAppendSerialization(SecOTRPublicIdentityRef publicID, CFMutableDataRef serializeInto, CFErrorRef *error)
{
    CFIndex start = CFDataGetLength(serializeInto);
    CFMutableDataRef signingKeySerialized = CFDataCreateMutable(kCFAllocatorDefault, 0);

    uint8_t sendHashes[1] = { 0xFF };

    require_noerr(appendPublicOctetsAndSize(publicID->publicSigningKey, signingKeySerialized), fail);

    size_t outputSize = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                                     ccder_sizeof_implicit_raw_octet_string(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SigningID, (size_t)CFDataGetLength(signingKeySerialized)) +
                                     (sAdvertiseHashes ? ccder_sizeof_implicit_raw_octet_string(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SupportsHashes, sizeof(sendHashes)) : 0));

    CFDataIncreaseLength(serializeInto, outputSize);

    uint8_t *outputBuffer = CFDataGetMutableBytePtr(serializeInto) + start;
    uint8_t *outputBufferEnd = outputBuffer + outputSize;

    uint8_t *result = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, outputBufferEnd, outputBuffer,
                      ccder_encode_implicit_raw_octet_string(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SigningID, (size_t)CFDataGetLength(signingKeySerialized), CFDataGetBytePtr(signingKeySerialized), outputBuffer,
                      sAdvertiseHashes ? ccder_encode_implicit_raw_octet_string(CCDER_CONTEXT_SPECIFIC | kOTRPIDER_SupportsHashes, sizeof(sendHashes), sendHashes, outputBuffer, outputBufferEnd) : outputBufferEnd));

    require_quiet(result == outputBuffer, fail);

    CFReleaseSafe(signingKeySerialized);

    return true;
    
fail:
    CFReleaseSafe(signingKeySerialized);
    
    CFDataSetLength(serializeInto, start);
    
    SecOTRCreateError(secOTRErrorLocal, kSecOTRErrorCreatePublicBytes, CFSTR("Unable to create public key bytes"), NULL, error);
    return false;
}

static const uint8_t *mp_decode_forced_uint(cc_size n, cc_unit *r, const uint8_t *der, const uint8_t *der_end) {
    size_t len;
    der = ccder_decode_tl(CCDER_INTEGER, &len, der, der_end);
    if (der && ccn_read_uint(n, r, len, der) >= 0)
        return der + len;
    
    return NULL;
}
    
static void SecOTRPIRecreateSignature(const uint8_t *oldSignature, size_t oldSignatureSize, uint8_t **newSignature, size_t *newSignatureSize)
{
    cc_unit r[ccec_cp_n(ccec_cp_256())], s[ccec_cp_n(ccec_cp_256())];
    cc_size n = ccec_cp_n(ccec_cp_256());

    const uint8_t *oldSignatureEnd = oldSignature + oldSignatureSize;
    
    oldSignature = ccder_decode_sequence_tl(&oldSignatureEnd, oldSignature, oldSignatureEnd);
    oldSignature = mp_decode_forced_uint(n, r, oldSignature, oldSignatureEnd);
    oldSignature = mp_decode_forced_uint(n, s, oldSignature, oldSignatureEnd);
    (void) oldSignature; // We could check for a good end, but it'll come out when we re-encode.
    
    const uint8_t *outputPointer = *newSignature;
    uint8_t *outputEndPointer = *newSignature + *newSignatureSize;
    
    *newSignature = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, outputEndPointer, outputPointer, ccder_encode_integer(n, r, outputPointer, ccder_encode_integer(n, s, outputPointer, outputEndPointer)));
    long newSigSize = outputEndPointer - *newSignature;
    *newSignatureSize = (newSigSize >= 0) ? (size_t)newSigSize : 0;
}

bool SecOTRPIVerifySignature(SecOTRPublicIdentityRef publicID,
                                const uint8_t *dataToHash, size_t amountToHash,
                                const uint8_t *signatureStart, size_t signatureSize, CFErrorRef *error)
{
    require(signatureSize > 0, fail);
    require(*signatureStart == signatureSize - 1, fail);
    signatureSize -= 1;
    signatureStart += 1;

    require(SecKeyDigestAndVerifyWithError(publicID->publicSigningKey, kOTRSignatureAlgIDPtr,
                                 dataToHash, amountToHash,
                                 (uint8_t*)signatureStart, signatureSize, NULL), fail);
    return true;
fail: ;
    uint8_t *replacementSignature = malloc(signatureSize + 3);
    require(replacementSignature != NULL, fail2);
    
    size_t replacementSignatureLen = sizeof(replacementSignature);
    uint8_t *replacementSignaturePtr = replacementSignature;
    
    SecOTRPIRecreateSignature(signatureStart, signatureSize, &replacementSignaturePtr, &replacementSignatureLen);
    
    require_action(replacementSignaturePtr, fail2, SecOTRCreateError(secOTRErrorLocal, kSecOTRErrorSignatureDidNotRecreate, CFSTR("Unable to recreate signature blob."), NULL, error));

    require(SecKeyDigestAndVerifyWithError(publicID->publicSigningKey, kOTRSignatureAlgIDPtr,
                                           dataToHash, amountToHash,
                                           replacementSignaturePtr, replacementSignatureLen, error), fail2);
    free(replacementSignature);
    return true;

fail2:
    free(replacementSignature);
    return false;
}

void SecOTRPICopyHash(SecOTRPublicIdentityRef publicID, uint8_t hash[kMPIDHashSize])
{
    memcpy(hash, publicID->hash, kMPIDHashSize);
}

void SecOTRPIAppendHash(SecOTRPublicIdentityRef publicID, CFMutableDataRef appendTo)
{
    CFDataAppendBytes(appendTo, publicID->hash, sizeof(publicID->hash));
}

bool SecOTRPICompareHash(SecOTRPublicIdentityRef publicID, const uint8_t hash[kMPIDHashSize])
{
    return 0 == memcmp(hash, publicID->hash, kMPIDHashSize);
}
