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
#include <utilities/array_size.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>

#include <Security/oidsalg.h>
#include <Security/SecCertificatePriv.h>

#include "SecOTRErrors.h"

#include <TargetConditionals.h>

//
// Algorthim ID initialization
//

#define kMessageIdentityRSAKeyBits 1280
#define kMessageIdentityECKeyBits 256

const SecAsn1AlgId *kOTRSignatureAlgIDPtr;

void EnsureOTRAlgIDInited(void)
{
    static dispatch_once_t kSignatureAlgID_ONCE;
    static SecAsn1AlgId kOTRECSignatureAlgID;

    dispatch_once(&kSignatureAlgID_ONCE, ^{
        kOTRECSignatureAlgID.algorithm = CSSMOID_ECDSA_WithSHA1;
        kOTRSignatureAlgIDPtr = &kOTRECSignatureAlgID;
    });
}


static CFStringRef sSigningKeyName = CFSTR("OTR Signing Key");

//
// SecOTRFullIdentity implementation
//

CFGiblisFor(SecOTRFullIdentity);

static CF_RETURNS_RETAINED CFStringRef SecOTRFullIdentityCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecOTRFullIdentityRef requestor = (SecOTRFullIdentityRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRPublicIdentity: %p %02x%02x%02x%02x%02x%02x%02x%02x>"),
                                    requestor,
                                    requestor->publicIDHash[0], requestor->publicIDHash[1],
                                    requestor->publicIDHash[2], requestor->publicIDHash[3],
                                    requestor->publicIDHash[4], requestor->publicIDHash[5],
                                    requestor->publicIDHash[6], requestor->publicIDHash[7]);
}

static void SecOTRFullIdentityDestroy(CFTypeRef cf) {
    SecOTRFullIdentityRef requestor = (SecOTRFullIdentityRef)cf;

    CFReleaseNull(requestor->privateSigningKey);
    CFReleaseNull(requestor->publicSigningKey);
    CFReleaseNull(requestor->privateKeyPersistentRef);
}


//
// Shared statics
//

static OSStatus SecOTRFIPurgeFromKeychainByValue(SecKeyRef key, CFStringRef label)
{
    OSStatus status;
    const void *keys[] =   { kSecClass,
        kSecAttrLabel,
        kSecValueRef,
    };
    const void *values[] = { kSecClassKey,
        label,
        key,
    };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, array_size(values), NULL, NULL);
    status = SecItemDelete(dict);
    CFReleaseSafe(dict);

    return status;
}

static bool SecKeyDigestAndSignWithError(
                                  SecKeyRef           key,            /* Private key */
                                  const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                                  const uint8_t       *dataToDigest,	/* signature over this data */
                                  size_t              dataToDigestLen,/* length of dataToDigest */
                                  uint8_t             *sig,			/* signature, RETURNED */
                                  size_t              *sigLen,		/* IN/OUT */
                                  CFErrorRef          *error) {

    OSStatus status = SecKeyDigestAndSign(key, algId, dataToDigest, dataToDigestLen, sig, sigLen);
    require_noerr(status, fail);
    return true;
fail:
    SecOTRCreateError(secOTRErrorOSError, status, CFSTR("Error signing message. OSStatus in error code."), NULL, error);
    return false;
}

//
// SecOTRFullIdentity Functions
//

static bool SecOTRFICachePublicHash(SecOTRFullIdentityRef fullID, CFErrorRef *error)
{
    SecOTRPublicIdentityRef pubID = SecOTRPublicIdentityCopyFromPrivate(NULL, fullID, error);

    require(pubID, fail);

    SecOTRPICopyHash(pubID, fullID->publicIDHash);

fail:
    CFReleaseSafe(pubID);
    return (pubID != NULL); // This is safe because we're not accessing the value after release, just checking if it ever had a value of some nature.
}

static SecKeyRef SecOTRCreateSigningKey(CFAllocatorRef allocator) {
    SecKeyRef publicKey = NULL;
    SecKeyRef fullKey = NULL;
    CFDictionaryRef keygen_parameters = NULL;
    const int signing_keySizeLocal = kMessageIdentityECKeyBits;
    CFNumberRef signing_bitsize = CFNumberCreate(allocator, kCFNumberIntType, &signing_keySizeLocal);

    const void *signing_keygen_keys[] = { kSecAttrKeyType,
        kSecAttrKeySizeInBits,
        kSecAttrIsPermanent,
        kSecAttrAccessible,
        kSecAttrLabel,
    };

    const void *signing_keygen_vals[] = { kSecAttrKeyTypeEC,
        signing_bitsize,
        kCFBooleanTrue,
        kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate,
        sSigningKeyName
    };
    keygen_parameters = CFDictionaryCreate(allocator,
                                           signing_keygen_keys, signing_keygen_vals, array_size(signing_keygen_vals),
                                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFReleaseNull(signing_bitsize);
    require_noerr_action(SecKeyGeneratePair(keygen_parameters, &publicKey, &fullKey),
                         errOut,
                         secerror("keygen failed"));
    SecOTRFIPurgeFromKeychainByValue(publicKey, sSigningKeyName);

    CFReleaseNull(keygen_parameters);
errOut:
    return fullKey;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreate(CFAllocatorRef allocator, CFErrorRef *error)
{
    SecKeyRef signingKey = SecOTRCreateSigningKey(allocator);
    SecOTRFullIdentityRef newID = SecOTRFullIdentityCreateFromSecKeyRefSOS(allocator, signingKey, error);
    CFReleaseNull(signingKey);

    return newID;
}


static
OSStatus SecOTRFICreatePrivateKeyReadPersistentRef(const uint8_t **bytes, size_t *size, SecKeyRef* privateKey, CFDataRef *newPersistentRef)
{
    OSStatus status = errSecParam;
    uint16_t dataSize;
    CFDataRef foundPersistentRef = NULL;

    require_quiet(newPersistentRef, fail);

    require_noerr_quiet(readSize(bytes, size, &dataSize), fail);
    require_quiet(dataSize <= *size, fail);

    foundPersistentRef = CFDataCreate(kCFAllocatorDefault, *bytes, dataSize);
    require_quiet(foundPersistentRef, fail);

    require_noerr_quiet(status = SecKeyFindWithPersistentRef(foundPersistentRef, privateKey), fail);

    *bytes += dataSize;
    *size -= dataSize;

    status = errSecSuccess;

    *newPersistentRef = foundPersistentRef;
    foundPersistentRef = NULL;

fail:
    CFReleaseSafe(foundPersistentRef);
    return status;
}

static
OSStatus SecOTRFICreateKeysFromReadPersistentRef(const uint8_t **bytes, size_t *size, SecKeyRef *publicKey, SecKeyRef* privateKey, CFDataRef *persistentRef)
{
    SecKeyRef foundKey = NULL;
    CFDataRef foundRef = NULL;

    OSStatus status = SecOTRFICreatePrivateKeyReadPersistentRef(bytes, size, &foundKey, &foundRef);
    require_noerr_quiet(status, fail);

    require_action_quiet(*publicKey = SecKeyCreatePublicFromPrivate(foundKey), fail, status = errSecInternalComponent);

    *privateKey = foundKey;
    foundKey = NULL;
    *persistentRef = foundRef;
    foundRef = NULL;

fail:
    CFReleaseSafe(foundKey);
    CFReleaseSafe(foundRef);
    return status;
}

typedef SecKeyRef (*SecOTRPublicKeyCreateFunction)(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);

static
OSStatus SecOTRFICreateKeysFromReadPersistentRefAndPublicKey(const uint8_t **bytes, size_t *size, SecKeyRef *publicKey, SecKeyRef* privateKey, CFDataRef *persistentRef, SecOTRPublicKeyCreateFunction createPublic)
{
    SecKeyRef foundKey = NULL;
    CFDataRef foundRef = NULL;

    OSStatus status = SecOTRFICreatePrivateKeyReadPersistentRef(bytes, size, &foundKey, &foundRef);
    require_noerr_quiet(status, fail);

    require_action_quiet(*publicKey = createPublic(NULL, bytes, size), fail, status = errSecInternalComponent);

    *privateKey = foundKey;
    foundKey = NULL;
    *persistentRef = foundRef;
    foundRef = NULL;

fail:
    CFReleaseSafe(foundKey);
    CFReleaseSafe(foundRef);
    return status;
}

static
OSStatus SecOTRFIInitFromV1Bytes(SecOTRFullIdentityRef newID, CFAllocatorRef allocator,
                                const uint8_t **bytes,size_t *size) {
    OSStatus status;
    require_action(**bytes == 1, fail, status = errSecParam);
    ++*bytes;
    --*size;

    require_noerr_quiet(status = SecOTRFICreateKeysFromReadPersistentRef(bytes, size, &newID->publicSigningKey, &newID->privateSigningKey, &newID->privateKeyPersistentRef), fail);

    return status;

fail:
    CFReleaseNull(newID->publicSigningKey);
    CFReleaseNull(newID->privateSigningKey);
    CFReleaseNull(newID->privateKeyPersistentRef);

    return status;
}

static
OSStatus SecOTRFIInitFromV2Bytes(SecOTRFullIdentityRef newID, CFAllocatorRef allocator,
                                const uint8_t **bytes,size_t *size) {
    OSStatus status;
    require_action(**bytes == 2, fail, status = errSecParam);
    ++*bytes;
    --*size;

    require_noerr_quiet(status = SecOTRFICreateKeysFromReadPersistentRefAndPublicKey(bytes, size, &newID->publicSigningKey, &newID->privateSigningKey, &newID->privateKeyPersistentRef, &CreateECPublicKeyFrom), fail);
    newID->isMessageProtectionKey = false;
    
    return status;

fail:
    CFReleaseNull(newID->privateKeyPersistentRef);
    CFReleaseNull(newID->publicSigningKey);
    CFReleaseNull(newID->privateSigningKey);

    return status;
}

static
OSStatus SecOTRFIInitFromV3Bytes(SecOTRFullIdentityRef newID, CFAllocatorRef allocator,
                                const uint8_t **bytes,size_t *size) {
    OSStatus status = errSecInvalidData;
    require_action(**bytes == 3, fail, status = errSecParam);
    ++*bytes;
    --*size;
    
    uint16_t dataSize;
    require_noerr_quiet(readSize(bytes, size, &dataSize), fail);
    
    int32_t keysz32 = 256;
    CFNumberRef ksizeNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDataRef keyBytes = CFDataCreate(allocator, *bytes, dataSize);
    
    CFDictionaryRef dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom,
                                                       kSecAttrKeyClass, kSecAttrKeyClassPrivate,
                                                       kSecAttrKeySizeInBits, ksizeNumber,
                                                       kSecAttrIsPermanent, kCFBooleanFalse, NULL);
    
    CFErrorRef error;
    SecKeyRef key = SecKeyCreateWithData(keyBytes, dict, &error);
    
    CFReleaseSafe(dict);
    CFReleaseSafe(keyBytes);
    CFReleaseSafe(ksizeNumber);
    
    if (key == NULL) {
        CFRelease(error);
        return errSecInvalidData;
    }
    
    newID->privateKeyPersistentRef = NULL;
    newID->isMessageProtectionKey = true;
    newID->privateSigningKey = key;
    newID->publicSigningKey = SecKeyCopyPublicKey(newID->privateSigningKey);
    status = errSecSuccess;
    
fail:
    return status;
}

// TODO: Probably move to SecKey
static CFDataRef SecKeyCreatePersistentRef(SecKeyRef theKey, CFErrorRef *error) {
    CFDataRef persistentRef = NULL;
    OSStatus status = SecKeyCopyPersistentRef(theKey, &persistentRef);
    if (status)
        SecError(status, error, CFSTR("failed to find persistent ref for key: %d"), (int)status);
    return persistentRef;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                            CFErrorRef *error) {
      // TODO - make sure this is an appropriate key type
      SecOTRFullIdentityRef newID = CFTypeAllocate(SecOTRFullIdentity, struct _SecOTRFullIdentity, allocator);
      CFRetainAssign(newID->privateSigningKey, privateKey);
      require_action(newID->publicSigningKey = SecKeyCreatePublicFromPrivate(privateKey), fail,
                     SecError(errSecInternalComponent, error, CFSTR("Failed to extract public key from private key")));
      // MessageProtection keys are no longer having persistent references.
      newID->privateKeyPersistentRef = NULL;
      newID->isMessageProtectionKey = true;
      
      require(SecOTRFICachePublicHash(newID, error), fail);
      return newID;
  fail:
      CFReleaseNull(newID);
      return NULL;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRefSOS(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                            CFErrorRef *error) {
    SecOTRFullIdentityRef newID = SecOTRFullIdentityCreateFromSecKeyRef(allocator, privateKey, error);
    require(newID->privateKeyPersistentRef = SecKeyCreatePersistentRef(privateKey, error), fail);
    newID->isMessageProtectionKey = false;
    return newID;
fail:
    CFReleaseNull(newID);
    return NULL;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t *size, CFErrorRef *error)
{
    SecOTRFullIdentityRef newID = CFTypeAllocate(SecOTRFullIdentity, struct _SecOTRFullIdentity, allocator);
    EnsureOTRAlgIDInited();

    require(*size > 0, fail);
    OSStatus status;
    switch (**bytes) {
        case 1:
            require_noerr_action_quiet(status = SecOTRFIInitFromV1Bytes(newID, allocator, bytes, size), fail, SecError(status, error, CFSTR("failed to decode v1 otr session: %d"), (int)status));
            break;
        case 2:
            require_noerr_action_quiet(status = SecOTRFIInitFromV2Bytes(newID, allocator, bytes, size), fail, SecError(status, error, CFSTR("failed to decode v2 otr session: %d"), (int)status));
            break;
        case 3:
            require_noerr_action_quiet(status = SecOTRFIInitFromV3Bytes(newID, allocator, bytes, size), fail, SecError(status, error, CFSTR("failed to decode v3 otr session: %d"), (int)status));
            break;
        case 0: // Version 0 was used in seeds of 5.0, transition from those seeds unsupported - keys were in exported data.
        default:
            SecError(errSecParam, error, CFSTR("unknown otr session version %hhu"), **bytes);
            goto fail;
    }

    require(SecOTRFICachePublicHash(newID, error), fail);

    return newID;

fail:
    if (NULL != newID) {
        SecOTRFIPurgeFromKeychain(newID, NULL);
    }
    CFReleaseSafe(newID);
    return NULL;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error)
{
    if (data == NULL)
        return NULL;

    size_t size = (size_t)CFDataGetLength(data);
    const uint8_t* bytes = CFDataGetBytePtr(data);

    return SecOTRFullIdentityCreateFromBytes(allocator, &bytes, &size, error);
}

bool SecOTRFIPurgeFromKeychain(SecOTRFullIdentityRef thisID, CFErrorRef *error)
{
    OSStatus result = SecOTRFIPurgeFromKeychainByValue(thisID->privateSigningKey, sSigningKeyName);
    if (errSecSuccess == result) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, result, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}


static OSStatus SecOTRFIPurgeAllFromKeychainByLabel(CFStringRef label)
{
    OSStatus status;
    const void *keys[] =   { kSecClass,
        kSecAttrKeyClass,
        kSecAttrLabel,
    };
    const void *values[] = { kSecClassKey,
        kSecAttrKeyClassPrivate,
        label,
    };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, array_size(values), NULL, NULL);
    bool deleteAtLeastOne = false;
    int loopLimiter = 500;
    do {
        status = SecItemDelete(dict);
        if (status == errSecSuccess) {
            deleteAtLeastOne = true;
        }
        loopLimiter--;
    } while ((status == errSecSuccess) && (loopLimiter > 0));
    if ((status == errSecItemNotFound) && (deleteAtLeastOne)) {
        // We've looped until we can't delete any more keys.
        // Since this will produce an expected 'itemNotFound', but we don't want to break the contract above
        // (and also we want to make sense)
        // we muffle the non-error to a success case, which it is.
        status = errSecSuccess;
    }
    CFReleaseSafe(dict);

    return status;
}

bool SecOTRFIPurgeAllFromKeychain(CFErrorRef *error)
{
    OSStatus result = SecOTRFIPurgeAllFromKeychainByLabel(sSigningKeyName);
    if (errSecSuccess == result) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, result, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}

static OSStatus SecOTRFIAppendV3Serialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto)
{
    const uint8_t version = 3;
    CFIndex start = CFDataGetLength(serializeInto);
    CFDataAppendBytes(serializeInto, &version, sizeof(version));
    CFErrorRef error = nil;
    CFDataRef privKeyBytes = SecKeyCopyExternalRepresentation(fullID->privateSigningKey, &error);
    require(privKeyBytes != nil, fail);
    appendSizeAndData(privKeyBytes, serializeInto);
    CFReleaseSafe(privKeyBytes);
    return errSecSuccess;

fail:
    CFDataSetLength(serializeInto, start);

    return errSecParam;
}

static OSStatus SecOTRFIAppendV2Serialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto)
{
    const uint8_t version = 2;
    CFIndex start = CFDataGetLength(serializeInto);

    CFDataAppendBytes(serializeInto, &version, sizeof(version));
    appendSizeAndData(fullID->privateKeyPersistentRef, serializeInto);
    require(errSecSuccess == appendPublicOctetsAndSize(fullID->publicSigningKey, serializeInto), fail);
    return errSecSuccess;

fail:
    CFDataSetLength(serializeInto, start);

    return errSecParam;
}


bool SecOTRFIAppendSerialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto, CFErrorRef *error)
{
    OSStatus status = errSecParam;
    if (fullID->isMessageProtectionKey) {
        status = SecOTRFIAppendV3Serialization(fullID, serializeInto);
    } else {
        status = SecOTRFIAppendV2Serialization(fullID, serializeInto);
    }
    
    if (errSecSuccess == status) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, status, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}

size_t SecOTRFISignatureSize(SecOTRFullIdentityRef fullID)
{
    return SecKeyGetSize(fullID->publicSigningKey, kSecKeySignatureSize);
}

bool SecOTRFICompareToPublicKey(SecOTRFullIdentityRef fullID, SecKeyRef publicKey) {
    return CFEqualSafe(fullID->publicSigningKey, publicKey);
}

bool SecOTRFIAppendSignature(SecOTRFullIdentityRef fullID,
                                CFDataRef dataToHash,
                                CFMutableDataRef appendTo,
                                CFErrorRef *error)
{
    const size_t signatureSize = SecOTRFISignatureSize(fullID);
    const CFIndex sourceLength = CFDataGetLength(dataToHash);
    const uint8_t* sourceData = CFDataGetBytePtr(dataToHash);

    CFIndex start = CFDataGetLength(appendTo);

    require(((CFIndex)signatureSize) >= 0, fail);

    CFDataIncreaseLength(appendTo, (CFIndex)signatureSize + 1);

    uint8_t *size = CFDataGetMutableBytePtr(appendTo) + start;
    uint8_t* signatureStart = size + 1;
    size_t signatureUsed = signatureSize;

   require(SecKeyDigestAndSignWithError(fullID->privateSigningKey, kOTRSignatureAlgIDPtr,
                                    sourceData, (size_t)sourceLength,
                                    signatureStart, &signatureUsed, error), fail);

    require(signatureUsed < 256, fail);
    require(((CFIndex)signatureUsed) >= 0, fail);
    *size = signatureUsed;

    CFDataSetLength(appendTo, start + (CFIndex)signatureUsed + 1);

    return true;

fail:
    CFDataSetLength(appendTo, start);

    return false;
}

void SecOTRFIAppendPublicHash(SecOTRFullIdentityRef fullID, CFMutableDataRef appendTo)
{
    CFDataAppendBytes(appendTo, fullID->publicIDHash, sizeof(fullID->publicIDHash));
}

bool SecOTRFIComparePublicHash(SecOTRFullIdentityRef fullID, const uint8_t hash[kMPIDHashSize])
{
    return 0 == memcmp(hash, fullID->publicIDHash, kMPIDHashSize);
}
