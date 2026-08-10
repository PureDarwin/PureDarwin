/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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

/*
 * SecKey.m - CoreFoundation based key object
 */
#include <AssertMacros.h>
#include <Security/SecBase.h>

#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemShim.h>
#include <Security/SecFramework.h>
#include <Security/SecCertificate.h>

#include <utilities/SecIOFormat.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>

#include <Security/SecKeyPriv.h>
#include "SecRSAKeyPriv.h"
#include "SecECKeyPriv.h"
#include "SecCTKKeyPriv.h"
#include <Security/SecBasePriv.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFPriv.h>
#include <pthread.h>
#include <string.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <Security/SecInternal.h>
#include <Security/SecRandom.h>
#include <Security/SecureTransport.h> /* For error codes. */

#include <corecrypto/ccrng_system.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

#include <asl.h>
#include <stdlib.h>
#include <os/lock.h>

#include <libDER/asn1Types.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>

CFDataRef SecKeyCopyPublicKeyHash(SecKeyRef key)
{
	CFDataRef pubKeyDigest = NULL, pubKeyBlob = NULL;

	/* encode the public key. */
    require_noerr_quiet(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require_quiet(pubKeyBlob, errOut);

	/* Calculate the digest of the public key. */
	require_quiet(pubKeyDigest = SecSHA1DigestCreate(CFGetAllocator(key),
                                                     CFDataGetBytePtr(pubKeyBlob), CFDataGetLength(pubKeyBlob)),
			errOut);
errOut:
    CFReleaseNull(pubKeyBlob);
    return pubKeyDigest;
}


/*
 */
static CFDictionaryRef SecKeyCopyAttributeDictionaryWithLocalKey(SecKeyRef key,
                                                                 CFTypeRef keyType,
                                                                 CFDataRef privateBlob)
{
	CFAllocatorRef allocator = CFGetAllocator(key);
	DICT_DECLARE(25);
	CFDataRef pubKeyDigest = NULL, pubKeyBlob = NULL;
	CFDictionaryRef dict = NULL;

    size_t sizeValue = SecKeyGetSize(key, kSecKeyKeySizeInBits);
    CFNumberRef sizeInBits = CFNumberCreate(allocator, kCFNumberLongType, &sizeValue);

	/* encode the public key. */
    require_noerr_quiet(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require_quiet(pubKeyBlob, errOut);

	/* Calculate the digest of the public key. */
	require_quiet(pubKeyDigest = SecSHA1DigestCreate(allocator,
                                                     CFDataGetBytePtr(pubKeyBlob), CFDataGetLength(pubKeyBlob)),
                  errOut);

	DICT_ADDPAIR(kSecClass, kSecClassKey);
	DICT_ADDPAIR(kSecAttrKeyClass, privateBlob ? kSecAttrKeyClassPrivate : kSecAttrKeyClassPublic);
	DICT_ADDPAIR(kSecAttrApplicationLabel, pubKeyDigest);
	DICT_ADDPAIR(kSecAttrIsPermanent, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrIsPrivate, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrIsModifiable, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrKeyType, keyType);
	DICT_ADDPAIR(kSecAttrKeySizeInBits, sizeInBits);
	DICT_ADDPAIR(kSecAttrEffectiveKeySize, sizeInBits);
	DICT_ADDPAIR(kSecAttrIsSensitive, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrWasAlwaysSensitive, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrIsExtractable, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrWasNeverExtractable, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanEncrypt, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanDecrypt, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanDerive, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanSign, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanVerify, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanSignRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanVerifyRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanWrap, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanUnwrap, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
	DICT_ADDPAIR(kSecValueData, privateBlob ? privateBlob : pubKeyBlob);
    dict = CFDictionaryCreate(allocator, keys, values, numValues, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
errOut:
	// @@@ Zero out key material.
	CFReleaseSafe(pubKeyDigest);
	CFReleaseSafe(pubKeyBlob);
	CFReleaseSafe(sizeInBits);

	return dict;
}

CFDictionaryRef SecKeyGeneratePrivateAttributeDictionary(SecKeyRef key,
                                                         CFTypeRef keyType,
                                                         CFDataRef privateBlob)
{
    return SecKeyCopyAttributeDictionaryWithLocalKey(key, keyType, privateBlob);
}

CFDictionaryRef SecKeyGeneratePublicAttributeDictionary(SecKeyRef key, CFTypeRef keyType)
{
    return SecKeyCopyAttributeDictionaryWithLocalKey(key, keyType, NULL);
}

static CFStringRef SecKeyCopyDescription(CFTypeRef cf) {
    SecKeyRef key = (SecKeyRef)cf;

    if(key->key_class->describe)
        return key->key_class->describe(key);
    else
        return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecKeyRef: %p>"), key);
}

#if TARGET_OS_OSX
static CFMutableDictionaryRef auxilliaryCDSAKeyMap;
static struct os_unfair_lock_s auxilliaryCDSAKeyMapLock = OS_UNFAIR_LOCK_INIT;

static void SecKeyDestroyAuxilliaryCDSAKeyForKey(CFTypeRef cf) {
    CFTypeRef keyToDestroy = NULL;
    os_unfair_lock_lock(&auxilliaryCDSAKeyMapLock);
    if (auxilliaryCDSAKeyMap != NULL) {
        keyToDestroy = CFDictionaryGetValue(auxilliaryCDSAKeyMap, cf);
        if (keyToDestroy != NULL) {
            CFRetain(keyToDestroy);
            CFDictionaryRemoveValue(auxilliaryCDSAKeyMap, cf);
        }
    }
    os_unfair_lock_unlock(&auxilliaryCDSAKeyMapLock);

    // Actual aux key destruction is performed outside unfair lock to avoid recursive lock.
    if (keyToDestroy != NULL) {
        CFRelease(keyToDestroy);
    }
}

void SecKeySetAuxilliaryCDSAKeyForKey(SecKeyRef cf, SecKeyRef auxKey) {
    os_unfair_lock_lock(&auxilliaryCDSAKeyMapLock);
    if (auxilliaryCDSAKeyMap == NULL) {
        // Allocate map with weak (unretained) keys (which are source SecKeys) but strong values (which are held aux CDSA keys).
        auxilliaryCDSAKeyMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    CFDictionarySetValue(auxilliaryCDSAKeyMap, cf, auxKey);
    os_unfair_lock_unlock(&auxilliaryCDSAKeyMapLock);
}

SecKeyRef SecKeyCopyAuxilliaryCDSAKeyForKey(SecKeyRef cf) {
    os_unfair_lock_lock(&auxilliaryCDSAKeyMapLock);
    if (auxilliaryCDSAKeyMap == NULL) {
        os_unfair_lock_unlock(&auxilliaryCDSAKeyMapLock);
        return NULL;
    }
    SecKeyRef result = (SecKeyRef)CFRetainSafe(CFDictionaryGetValue(auxilliaryCDSAKeyMap, cf));
    os_unfair_lock_unlock(&auxilliaryCDSAKeyMapLock);
    return result;
}
#endif

static void SecKeyDestroy(CFTypeRef cf) {
    SecKeyRef key = (SecKeyRef)cf;
#if TARGET_OS_OSX
    SecKeyDestroyAuxilliaryCDSAKeyForKey(cf);
#endif
    if (key->key_class->destroy)
        key->key_class->destroy(key);
}

static Boolean SecKeyEqual(CFTypeRef cf1, CFTypeRef cf2)
{
    SecKeyRef key1 = (SecKeyRef)cf1;
    SecKeyRef key2 = (SecKeyRef)cf2;
    if (key1 == key2)
        return true;
    if (!key2 || key1->key_class != key2->key_class)
        return false;
    if (key1->key_class->version >= 4 && key1->key_class->isEqual)
        return key1->key_class->isEqual(key1, key2);
    if (key1->key_class->extraBytes)
        return !memcmp(key1->key, key2->key, key1->key_class->extraBytes);

    /* TODO: Won't work when we get reference keys. */
    CFDictionaryRef d1, d2;
    d1 = SecKeyCopyAttributeDictionary(key1);
    d2 = SecKeyCopyAttributeDictionary(key2);
    // Returning NULL is an error; bail out of the equality check
    if(!d1 || !d2) {
        CFReleaseSafe(d1);
        CFReleaseSafe(d2);
        return false;
    }
    Boolean result = CFEqual(d1, d2);
    CFReleaseSafe(d1);
    CFReleaseSafe(d2);
    return result;
}

struct ccrng_state *ccrng_seckey;

CFGiblisWithFunctions(SecKey, NULL, NULL, SecKeyDestroy, SecKeyEqual, NULL, NULL, SecKeyCopyDescription, NULL, NULL, ^{
    static struct ccrng_system_state ccrng_system_state_seckey;
    ccrng_seckey = (struct ccrng_state *)&ccrng_system_state_seckey;
    ccrng_system_init(&ccrng_system_state_seckey);
})

static bool getBoolForKey(CFDictionaryRef dict, CFStringRef key, bool default_value) {
	CFTypeRef value = CFDictionaryGetValue(dict, key);
	if (value) {
		if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
			return CFBooleanGetValue(value);
		} else {
			secwarning("Value %@ for key %@ is not bool", value, key);
		}
	}

	return default_value;
}

static OSStatus add_key(SecKeyRef key, CFMutableDictionaryRef dict) {
	CFDictionarySetValue(dict, kSecValueRef, key);
    CFDictionaryRef keyAttributes = SecKeyCopyAttributes(key);
    if (keyAttributes != NULL && CFDictionaryContainsKey(keyAttributes, kSecAttrAccessControl)) {
        // Avoid overriding ACL from the key with source ACL of the call; ACL of the key might be already processed during key generation and should be preferred to source unprocessed ACL.
        CFDictionaryRemoveValue(dict, kSecAttrAccessControl);
    }
    CFReleaseNull(keyAttributes);
    return SecItemAdd(dict, NULL);
}

static void merge_params_applier(const void *key, const void *value,
                                 void *context) {
	CFMutableDictionaryRef result = (CFMutableDictionaryRef)context;
	CFDictionaryAddValue(result, key, value);
}

/* Create a mutable dictionary that is based on the subdictionary for key
 with any attributes from the top level dict merged in. */
static CF_RETURNS_RETAINED CFMutableDictionaryRef merge_params(CFDictionaryRef dict,
                                                               CFStringRef key) {
	CFDictionaryRef subdict = CFDictionaryGetValue(dict, key);
	CFMutableDictionaryRef result;

	if (subdict) {
		result = CFDictionaryCreateMutableCopy(NULL, 0, subdict);
		/* Add everything in dict not already in result to result. */
		CFDictionaryApplyFunction(dict, merge_params_applier, result);
	} else {
		result = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	}

	/* Remove values that only belong in the top level dict. */
	CFDictionaryRemoveValue(result, kSecPublicKeyAttrs);
	CFDictionaryRemoveValue(result, kSecPrivateKeyAttrs);
	CFDictionaryRemoveValue(result, kSecAttrKeyType);
	CFDictionaryRemoveValue(result, kSecAttrKeySizeInBits);

	return result;
}

CFIndex SecKeyGetAlgorithmId(SecKeyRef key) {
    if (!key || !key->key_class)  {
    // TBD: somehow, a key can be created with a NULL key_class in the
    // SecCertificateCopyPublicKey -> SecKeyCreatePublicFromDER code path
        return kSecNullAlgorithmID;
    }
    /* This method was added to version 1 keys. */
    if (key->key_class->version > 0 && key->key_class->getAlgorithmID) {
        return key->key_class->getAlgorithmID(key);
    }
    /* All version 0 keys were RSA. */
    return kSecRSAAlgorithmID;
}

/* Generate a private/public keypair. */
OSStatus SecKeyGeneratePair(CFDictionaryRef parameters,
                            SecKeyRef *publicKey, SecKeyRef *privateKey) {
    @autoreleasepool {
        OSStatus result = errSecUnsupportedAlgorithm;
        SecKeyRef privKey = NULL;
        SecKeyRef pubKey = NULL;
        CFMutableDictionaryRef pubParams = merge_params(parameters, kSecPublicKeyAttrs),
        privParams = merge_params(parameters, kSecPrivateKeyAttrs);
        CFStringRef ktype = CFDictionaryGetValue(parameters, kSecAttrKeyType);
        CFStringRef tokenID = CFDictionaryGetValue(parameters, kSecAttrTokenID);

        require_quiet(ktype, errOut);

        if (tokenID != NULL) {
            result = SecCTKKeyGeneratePair(parameters, &pubKey, &privKey);
        } else if (CFEqual(ktype, kSecAttrKeyTypeECSECPrimeRandom)) {
            result = SecECKeyGeneratePair(parameters, &pubKey, &privKey);
        } else if (CFEqual(ktype, kSecAttrKeyTypeRSA)) {
            result = SecRSAKeyGeneratePair(parameters, &pubKey, &privKey);
        }

        require_noerr_quiet(result, errOut);

        // Store the keys in the keychain if they are marked as permanent. Governed by kSecAttrIsPermanent attribute, with default
        // to 'false' (ephemeral keys), except private token-based keys, in which case the default is 'true' (permanent keys).
        if (getBoolForKey(pubParams, kSecAttrIsPermanent, false)) {
            CFDictionaryRemoveValue(pubParams, kSecAttrTokenID);
            require_noerr_quiet(result = add_key(pubKey, pubParams), errOut);
        }
        if (getBoolForKey(privParams, kSecAttrIsPermanent, CFDictionaryContainsKey(privParams, kSecAttrTokenID))) {
            require_noerr_quiet(result = add_key(privKey, privParams), errOut);
        }

        if (publicKey) {
            *publicKey = pubKey;
            pubKey = NULL;
        }
        if (privateKey) {
            *privateKey = privKey;
            privKey = NULL;
        }

    errOut:
        CFReleaseSafe(pubParams);
        CFReleaseSafe(privParams);
        CFReleaseSafe(pubKey);
        CFReleaseSafe(privKey);

        return result;
    }
}

SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey) {
    return SecKeyCopyPublicKey(privateKey);
}

CFDictionaryRef CreatePrivateKeyMatchingQuery(SecKeyRef publicKey, bool returnPersistentRef)
{
    const CFTypeRef refType = (returnPersistentRef) ? kSecReturnPersistentRef: kSecReturnRef;

    CFDataRef public_key_hash = SecKeyCopyPublicKeyHash(publicKey);

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         kSecAttrApplicationLabel,  public_key_hash,
                                                         refType,                   kCFBooleanTrue,
                                                         NULL);
    CFReleaseNull(public_key_hash);

    return query;
}

CFDataRef SecKeyCreatePersistentRefToMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error) {
    CFTypeRef persistentRef = NULL;
    CFDictionaryRef query = CreatePrivateKeyMatchingQuery(publicKey, true);

    require_quiet(SecError(SecItemCopyMatching(query, &persistentRef),error ,
                           CFSTR("Error finding persistent ref to key from public: %@"), publicKey), fail);
fail:
    CFReleaseNull(query);
    return (CFDataRef)persistentRef;
}

SecKeyRef SecKeyCopyMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error) {
    SecKeyRef privateKey = NULL;
    CFTypeRef queryResult = NULL;
    CFDictionaryRef query = NULL;

    require_action_quiet(publicKey != NULL, errOut, SecError(errSecParam, error, CFSTR("Null Public Key")));

    query = CreatePrivateKeyMatchingQuery(publicKey, false);

    require_quiet(SecError(SecItemCopyMatching(query, &queryResult), error,
                           CFSTR("Error finding private key from public: %@"), publicKey), errOut);

    if (CFGetTypeID(queryResult) == SecKeyGetTypeID()) {
        privateKey = (SecKeyRef) queryResult;
        queryResult = NULL;
    }

errOut:
    CFReleaseNull(query);
    CFReleaseNull(queryResult);
    return privateKey;
}

OSStatus SecKeyGetMatchingPrivateKeyStatus(SecKeyRef publicKey, CFErrorRef *error) {
    OSStatus retval = errSecParam;
    CFTypeRef private_key = NULL;
    CFDictionaryRef query = NULL;

    require_action_quiet(publicKey != NULL, errOut, SecError(errSecParam, error, NULL, CFSTR("Null Public Key")));

    query = CreatePrivateKeyMatchingQuery(publicKey, false);

    retval = SecItemCopyMatching(query, &private_key);

    if (!retval && CFGetTypeID(private_key) != SecKeyGetTypeID()) {
        retval = errSecInternalComponent;
    }

errOut:
    CFReleaseNull(query);
    CFReleaseNull(private_key);
    return retval;
}


SecKeyRef SecKeyCreatePublicFromDER(CFAllocatorRef allocator,
                                    const SecAsn1Oid *oid, const SecAsn1Item *params,
                                    const SecAsn1Item *keyData) {
    SecKeyRef publicKey = NULL;
	if (SecAsn1OidCompare(oid, &CSSMOID_RSA)) {
        /* pkcs1 1 */
        /* Note that we call SecKeyCreateRSAPublicKey_ios directly instead of
           SecKeyCreateRSAPublicKey, since on OS X the latter function will return
           a CSSM SecKeyRef, and we always want an iOS format SecKeyRef here.
         */
		publicKey = SecKeyCreateRSAPublicKey_ios(allocator,
                                             keyData->Data, keyData->Length, kSecKeyEncodingPkcs1);
	} else if (SecAsn1OidCompare(oid, &CSSMOID_ecPublicKey)) {
        SecDERKey derKey = {
            .oid = oid->Data,
            .oidLength = oid->Length,
            .key = keyData->Data,
            .keyLength = keyData->Length,
        };
        if (params) {
            derKey.parameters = params->Data;
            derKey.parametersLength = params->Length;
        }
		publicKey = SecKeyCreateECPublicKey(allocator,
                                            (const uint8_t *)&derKey, sizeof(derKey), kSecDERKeyEncoding);
    } else {
		secwarning("Unsupported algorithm oid");
	}

    return publicKey;
}


SecKeyRef SecKeyCreateFromSubjectPublicKeyInfoData(CFAllocatorRef allocator, CFDataRef subjectPublicKeyInfoData)
{
    DERReturn drtn;

    DERItem subjectPublicKeyInfoDER = {
        .data = (uint8_t *)CFDataGetBytePtr(subjectPublicKeyInfoData),
        .length = (DERSize)CFDataGetLength(subjectPublicKeyInfoData),
    };
    DERSubjPubKeyInfo subjectPublicKeyInfo;
    DERAlgorithmId algorithmId;
    DERItem pubKeyBytes;

    drtn = DERParseSequence(&subjectPublicKeyInfoDER,
                            DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
                            &subjectPublicKeyInfo, sizeof(subjectPublicKeyInfo));

    require_noerr_quiet(drtn, out);

    drtn = DERParseSequenceContent(&subjectPublicKeyInfo.algId,
                                   DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                   &algorithmId, sizeof(algorithmId));
    require_noerr_quiet(drtn, out);

    DERByte unusedBits;
    drtn = DERParseBitString(&subjectPublicKeyInfo.pubKey, &pubKeyBytes, &unusedBits);
    require_noerr_quiet(drtn, out);

    /* Convert DERItem to SecAsn1Item : */
    const SecAsn1Oid oid = { .Data = algorithmId.oid.data, .Length = algorithmId.oid.length };
    const SecAsn1Item params = { .Data = algorithmId.params.data, .Length = algorithmId.params.length };
    const SecAsn1Item pubKey = { .Data = pubKeyBytes.data, .Length = pubKeyBytes.length };

    return SecKeyCreatePublicFromDER(allocator, &oid, &params, &pubKey);

out:

    return NULL;
}

static const DERByte oidRSA[] = {
    0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
};
static const DERByte oidECsecp256[] = {
    0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
};
static const DERByte oidECsecp384[] = {
    0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22,
};
static const DERByte oidECsecp521[] = {
    0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23,
};


CFDataRef SecKeyCopySubjectPublicKeyInfo(SecKeyRef key)
{
    CFMutableDataRef data = NULL;
    CFDataRef publicKey = NULL;
    CFDataRef dataret = NULL;
    DERSubjPubKeyInfo spki;
    DERReturn drtn;
    size_t zeroPad = 0;

    memset(&spki, 0, sizeof(spki));

    /* encode the public key. */
    require_noerr_quiet(SecKeyCopyPublicBytes(key, &publicKey), errOut);
    require_quiet(publicKey, errOut);

    require_quiet(CFDataGetLength(publicKey) != 0, errOut);

    // Add prefix 00 is needed to avoid creating negative bit strings
    if (((uint8_t *)CFDataGetBytePtr(publicKey))[0] & 0x80)
        zeroPad = 1;


    CFMutableDataRef paddedKey = CFDataCreateMutable(NULL, 0);
    /* the bit strings bits used field first */
    CFDataAppendBytes(paddedKey, (const UInt8 *)"\x00", 1);
    if (zeroPad)
        CFDataAppendBytes(paddedKey, (const UInt8 *)"\x00", 1);

    CFDataAppendBytes(paddedKey, CFDataGetBytePtr(publicKey), CFDataGetLength(publicKey));
    CFTransferRetained(publicKey, paddedKey);

    spki.pubKey.data = (DERByte *)CFDataGetBytePtr(publicKey);
    spki.pubKey.length = CFDataGetLength(publicKey);

    // Encode algId according to algorithm used.
    CFIndex algorithm = SecKeyGetAlgorithmId(key);
    if (algorithm == kSecRSAAlgorithmID) {
        spki.algId.data = (DERByte *)oidRSA;
        spki.algId.length = sizeof(oidRSA);
    } else if (algorithm == kSecECDSAAlgorithmID) {
        SecECNamedCurve curve = SecECKeyGetNamedCurve(key);
        switch(curve) {
            case kSecECCurveSecp256r1:
                spki.algId.data = (DERByte *)oidECsecp256;
                spki.algId.length = sizeof(oidECsecp256);
                break;
            case kSecECCurveSecp384r1:
                spki.algId.data = (DERByte *)oidECsecp384;
                spki.algId.length = sizeof(oidECsecp384);
                break;
            case kSecECCurveSecp521r1:
                spki.algId.data = (DERByte *)oidECsecp521;
                spki.algId.length = sizeof(oidECsecp521);
                break;
            default:
                goto errOut;
        }
    } else {
        goto errOut;
    }

    DERSize size = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, &spki,
                                              DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs);
    data = CFDataCreateMutable(kCFAllocatorDefault, size);
    CFDataSetLength(data, size);

    drtn = DEREncodeSequence(ASN1_CONSTR_SEQUENCE, &spki,
                             DERNumSubjPubKeyInfoItemSpecs,
                             DERSubjPubKeyInfoItemSpecs,
                             CFDataGetMutableBytePtr(data), &size);
    require_quiet(drtn == DR_Success, errOut);
    CFDataSetLength(data, size);

    dataret = CFRetain(data);
errOut:
    CFReleaseNull(data);
    CFReleaseNull(publicKey);

    return dataret;
}



SecKeyRef SecKeyCreate(CFAllocatorRef allocator,
                       const SecKeyDescriptor *key_class, const uint8_t *keyData,
                       CFIndex keyDataLength, SecKeyEncoding encoding) {
	if (!key_class) return NULL;
    size_t size = sizeof(struct __SecKey) + key_class->extraBytes;
    SecKeyRef result = (SecKeyRef)_CFRuntimeCreateInstance(allocator,
                                                           SecKeyGetTypeID(), size - sizeof(CFRuntimeBase), NULL);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0, size - sizeof(result->_base));
        result->key_class = key_class;
        if (key_class->extraBytes) {
            /* Make result->key point to the extraBytes we allocated. */
            result->key = ((char*)result) + sizeof(*result);
        }
        if (key_class->init) {
			OSStatus status;
			status = key_class->init(result, keyData, keyDataLength, encoding);
			if (status) {
				secwarning("init %s key: %" PRIdOSStatus, key_class->name, status);
				CFRelease(result);
				result = NULL;
			}
		}
    }
    return result;
}

// Generic wrapper helper for invoking new-style CFDataRef-based operations with ptr/length arguments
// used by legacy RawSign-style functions.
static OSStatus SecKeyPerformLegacyOperation(SecKeyRef key,
                                             const uint8_t *in1Ptr, size_t in1Len,
                                             const uint8_t *in2Ptr, size_t in2Len,
                                             uint8_t *outPtr, size_t *outLen,
                                             CFTypeRef (^operation)(CFDataRef in1, CFDataRef in2, CFRange *resultRange, CFErrorRef *error)) {
    CFErrorRef error = NULL;
    OSStatus status = errSecSuccess;
    CFDataRef in1 = CFDataCreateWithBytesNoCopy(NULL, in1Ptr, in1Len, kCFAllocatorNull);
    CFDataRef in2 = CFDataCreateWithBytesNoCopy(NULL, in2Ptr, in2Len, kCFAllocatorNull);
    CFRange range = { 0, -1 };
    CFTypeRef output = operation(in1, in2, &range, &error);
    require_quiet(output, out);
    if (CFGetTypeID(output) == CFDataGetTypeID() && outLen != NULL) {
        if (range.length == -1) {
            range.length = CFDataGetLength(output);
        }
        require_action_quiet((size_t)range.length <= *outLen, out,
                             SecError(errSecParam, &error, CFSTR("buffer too small")));
        *outLen = range.length;
        CFDataGetBytes(output, range, outPtr);
    }

out:
    CFReleaseSafe(in1);
    CFReleaseSafe(in2);
    CFReleaseSafe(output);
    if (error != NULL) {
        status = SecErrorGetOSStatus(error);
        if (status == errSecVerifyFailed) {
            // Legacy functions used errSSLCrypto, while new implementation uses errSecVerifyFailed.
            status = errSSLCrypto;
        }
        CFRelease(error);
    }
    return status;
}

static SecKeyAlgorithm SecKeyGetSignatureAlgorithmForPadding(SecKeyRef key, SecPadding padding, size_t digestSize) {
    switch (SecKeyGetAlgorithmId(key)) {
        case kSecRSAAlgorithmID: {
            switch (padding) {
                case kSecPaddingNone:
                    return kSecKeyAlgorithmRSASignatureRaw;
                case kSecPaddingPKCS1:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw;
                case kSecPaddingPKCS1SHA1:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
                case kSecPaddingPKCS1SHA224:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224;
                case kSecPaddingPKCS1SHA256:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
                case kSecPaddingPKCS1SHA384:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
                case kSecPaddingPKCS1SHA512:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
                default:
                    return NULL;
            }
        }
        case kSecECDSAAlgorithmID:
            switch (padding) {
                case kSecPaddingSigRaw:
                    return kSecKeyAlgorithmECDSASignatureRFC4754;
                case kSecPaddingPKCS1: {
                    // If digest has known size of some hash function, explicitly encode that hash type in the algorithm.
                    if (digestSize == ccsha1_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA1;
                    } else if (digestSize == ccsha224_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA224;
                    } else if (digestSize == ccsha256_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA256;
                    } else if (digestSize == ccsha384_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA384;
                    } else if (digestSize == ccsha512_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA512;
                    }

                    // Fall through to common case, no break here.
                }
                default:
                    // Although it is not very logical, previous SecECKey implementation really considered
                    // anything else than SigRaw (incl. None!) as PKCS1 (i.e. x962), so we keep the behaviour
                    // for backward compatibility.
                    return kSecKeyAlgorithmECDSASignatureDigestX962;
            }
        default:
            return NULL;
    }
}

#if TARGET_OS_OSX
static SecKeyAlgorithm SecKeyGetSignatureAlgorithmForPadding_macOS(SecKeyRef key, SecPadding padding, size_t digestSize) {
    switch (SecKeyGetAlgorithmId(key)) {
        case kSecRSAAlgorithmID: {
            // On CSSM-based implementation, these functions actually did hash its input,
            // so keep doing that for backward compatibility.
            switch (padding) {
                case kSecPaddingNone:
                    return kSecKeyAlgorithmRSASignatureRaw;
                case kSecPaddingPKCS1:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw;
                case kSecPaddingPKCS1SHA1:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
                case kSecPaddingPKCS1SHA224:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224;
                case kSecPaddingPKCS1SHA256:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
                case kSecPaddingPKCS1SHA384:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384;
                case kSecPaddingPKCS1SHA512:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512;
                default:
                    return NULL;
            }
        }
        case kSecECDSAAlgorithmID:
            switch (padding) {
                case kSecPaddingSigRaw:
                    return kSecKeyAlgorithmECDSASignatureRFC4754;
                case kSecPaddingPKCS1: {
                    // If digest has known size of some hash function, explicitly encode that hash type in the algorithm.
                    if (digestSize == ccsha1_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA1;
                    } else if (digestSize == ccsha224_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA224;
                    } else if (digestSize == ccsha256_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA256;
                    } else if (digestSize == ccsha384_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA384;
                    } else if (digestSize == ccsha512_di()->output_size) {
                        return kSecKeyAlgorithmECDSASignatureDigestX962SHA512;
                    }

                    // Fall through to common case, no break here.
                }
                default:
                    // Although it is not very logical, previous SecECKey implementation really considered
                    // anything else than SigRaw (incl. None!) as PKCS1 (i.e. x962), so we keep the behaviour
                    // for backward compatibility.
                    return kSecKeyAlgorithmECDSASignatureDigestX962;
            }
        default:
            return NULL;
    }
}
#endif // TARGET_OS_OSX

#undef SecKeyRawSign
OSStatus SecKeyRawSign(
                       SecKeyRef           key,            /* Private key */
                       SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                       const uint8_t       *dataToSign,	/* signature over this data */
                       size_t              dataToSignLen,	/* length of dataToSign */
                       uint8_t             *sig,			/* signature, RETURNED */
                       size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding(key, padding, dataToSignLen);
    if (algorithm == NULL) {
        return errSecParam;
    }
    return SecKeyPerformLegacyOperation(key, dataToSign, dataToSignLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

#if TARGET_OS_OSX
OSStatus SecKeyRawSign_macOS(
                       SecKeyRef           key,            /* Private key */
                       SecPadding          padding,        /* kSecPaddingNone or kSecPaddingPKCS1 */
                       const uint8_t       *dataToSign,    /* signature over this data */
                       size_t              dataToSignLen,    /* length of dataToSign */
                       uint8_t             *sig,            /* signature, RETURNED */
                       size_t              *sigLen) {        /* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding_macOS(key, padding, dataToSignLen);
    if (algorithm == NULL) {
        return errSecParam;
    }
    return SecKeyPerformLegacyOperation(key, dataToSign, dataToSignLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}
#endif

#undef SecKeyRawVerify
OSStatus SecKeyRawVerify(
                         SecKeyRef           key,            /* Public key */
                         SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                         const uint8_t       *signedData,	/* signature over this data */
                         size_t              signedDataLen,	/* length of dataToSign */
                         const uint8_t       *sig,			/* signature */
                         size_t              sigLen) {		/* length of signature */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding(key, padding, signedDataLen);
    if (algorithm == NULL) {
        return errSecParam;
    }
    OSStatus status = SecKeyPerformLegacyOperation(key, signedData, signedDataLen, sig, sigLen, NULL, NULL,
                                                   ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                                       return SecKeyVerifySignature(key, algorithm, in1, in2, error)
                                                       ? kCFBooleanTrue : NULL;
                                                   });
    return status;
}

#if TARGET_OS_OSX
OSStatus SecKeyRawVerify_macOS(
                         SecKeyRef           key,            /* Public key */
                         SecPadding          padding,        /* kSecPaddingNone or kSecPaddingPKCS1 */
                         const uint8_t       *signedData,    /* signature over this data */
                         size_t              signedDataLen,    /* length of dataToSign */
                         const uint8_t       *sig,            /* signature */
                         size_t              sigLen) {        /* length of signature */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding_macOS(key, padding, signedDataLen);
    if (algorithm == NULL) {
        return errSecParam;
    }
    OSStatus status = SecKeyPerformLegacyOperation(key, signedData, signedDataLen, sig, sigLen, NULL, NULL,
                                                   ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                                       return SecKeyVerifySignature(key, algorithm, in1, in2, error)
                                                       ? kCFBooleanTrue : NULL;
                                                   });
    return status;
}
#endif

static SecKeyAlgorithm SecKeyGetEncryptionAlgorithmForPadding(SecKeyRef key, SecPadding padding) {
    switch (SecKeyGetAlgorithmId(key)) {
        case kSecRSAAlgorithmID:
            switch (padding) {
                case kSecPaddingNone:
                    return kSecKeyAlgorithmRSAEncryptionRaw;
                case kSecPaddingPKCS1:
                    return kSecKeyAlgorithmRSAEncryptionPKCS1;
                case kSecPaddingOAEP:
                    return kSecKeyAlgorithmRSAEncryptionOAEPSHA1;
                default:
                    return NULL;
            }
        default:
            return NULL;
    }
}

OSStatus SecKeyEncrypt(
                       SecKeyRef           key,                /* Public key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t		*plainText,
                       size_t              plainTextLen,		/* length of plainText */
                       uint8_t             *cipherText,
                       size_t              *cipherTextLen) {	/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetEncryptionAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }

    return SecKeyPerformLegacyOperation(key, plainText, plainTextLen, NULL, 0, cipherText, cipherTextLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateEncryptedData(key, algorithm, in1, error);
                                        });
}

OSStatus SecKeyDecrypt(
                       SecKeyRef           key,                /* Private key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t       *cipherText,
                       size_t              cipherTextLen,		/* length of cipherText */
                       uint8_t             *plainText,
                       size_t              *plainTextLen) {	/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetEncryptionAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }
    return SecKeyPerformLegacyOperation(key, cipherText, cipherTextLen, NULL, 0, plainText, plainTextLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            CFDataRef decrypted = SecKeyCreateDecryptedData(key, algorithm, in1, error);
                                            const UInt8 *data;
                                            if (decrypted != NULL && algorithm == kSecKeyAlgorithmRSAEncryptionRaw &&
                                                *(data = CFDataGetBytePtr(decrypted)) == 0x00) {
                                                // Strip zero-padding from the beginning of the block, as the contract of this
                                                // function says.
                                                range->length = CFDataGetLength(decrypted);
                                                while (*data == 0x00 && range->length > 0) {
                                                    range->location++;
                                                    range->length--;
                                                    data++;
                                                }
                                            }
                                            return decrypted;
                                        });
}

size_t SecKeyGetBlockSize(SecKeyRef key) {
    if (key->key_class->blockSize)
        return key->key_class->blockSize(key);
    return 0;
}

/* Private API functions. */

CFDictionaryRef SecKeyCopyAttributeDictionary(SecKeyRef key) {
    return SecKeyCopyAttributes(key);
}

SecKeyRef SecKeyCreateFromAttributeDictionary(CFDictionaryRef refAttributes) {
    CFErrorRef error = NULL;
    SecKeyRef key = SecKeyCreateWithData(CFDictionaryGetValue(refAttributes, kSecValueData), refAttributes, &error);
    if (key == NULL) {
        CFStringRef description = CFErrorCopyDescription(error);
        secwarning("%@", description);
        CFRelease(description);
        CFRelease(error);
    }
    return key;
}

static SecKeyAlgorithm SecKeyGetAlgorithmForSecAsn1AlgId(SecKeyRef key, const SecAsn1AlgId *algId, bool digestData) {
    static const struct TableItem {
        const SecAsn1Oid *oid1, *oid2;
        const SecKeyAlgorithm *algorithms[2];
    } translationTableRSA[] = {
        { &CSSMOID_SHA1WithRSA, &CSSMOID_SHA1, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
        } },
        { &CSSMOID_SHA224WithRSA, &CSSMOID_SHA224, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
        } },
        { &CSSMOID_SHA256WithRSA, &CSSMOID_SHA256, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
        } },
        { &CSSMOID_SHA384WithRSA, &CSSMOID_SHA384, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
        } },
        { &CSSMOID_SHA512WithRSA, &CSSMOID_SHA512, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
        } },
        { &CSSMOID_MD5, NULL, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5,
        } },
        { &CSSMOID_MD5WithRSA, NULL, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5,
        } },
        { NULL },
    }, translationTableECDSA[] = {
        { &CSSMOID_ECDSA_WithSHA1, &CSSMOID_SHA1, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
        } },
        { &CSSMOID_ECDSA_WithSHA224, &CSSMOID_SHA224, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
        } },
        { &CSSMOID_ECDSA_WithSHA256, &CSSMOID_SHA256, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
        } },
        { &CSSMOID_ECDSA_WithSHA384, &CSSMOID_SHA384, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
        } },
        { &CSSMOID_ECDSA_WithSHA512, &CSSMOID_SHA512, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA512,
        } },
        { NULL },
    };

    const struct TableItem *table;
    switch (SecKeyGetAlgorithmId(key)) {
        case kSecRSAAlgorithmID:
            table = translationTableRSA;
            break;
        case kSecECDSAAlgorithmID:
            table = translationTableECDSA;
            break;
        default:
            return NULL;
    }

    for (; table->oid1 != NULL; table++) {
        if (SecAsn1OidCompare(table->oid1, &algId->algorithm) ||
            (table->oid2 != NULL && SecAsn1OidCompare(table->oid2, &algId->algorithm))) {
            return *table->algorithms[digestData];
        }
    }
    return NULL;
}

OSStatus SecKeyDigestAndVerify(
                               SecKeyRef           key,            /* Private key */
                               const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                               const uint8_t       *dataToDigest,	/* signature over this data */
                               size_t              dataToDigestLen,/* length of dataToDigest */
                               const uint8_t       *sig,			/* signature to verify */
                               size_t              sigLen) {		/* length of sig */

    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, true);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, dataToDigest, dataToDigestLen, sig, sigLen, NULL, NULL,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyVerifySignature(key, algorithm, in1, in2, error) ?
                                            kCFBooleanTrue : NULL;
                                        });
}

OSStatus SecKeyDigestAndSign(
                             SecKeyRef           key,            /* Private key */
                             const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                             const uint8_t       *dataToDigest,	/* signature over this data */
                             size_t              dataToDigestLen,/* length of dataToDigest */
                             uint8_t             *sig,			/* signature, RETURNED */
                             size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, true);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, dataToDigest, dataToDigestLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

OSStatus SecKeyVerifyDigest(
                            SecKeyRef           key,            /* Private key */
                            const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                            const uint8_t       *digestData,	/* signature over this digest */
                            size_t              digestDataLen,/* length of dataToDigest */
                            const uint8_t       *sig,			/* signature to verify */
                            size_t              sigLen) {		/* length of sig */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, false);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, digestData, digestDataLen, sig, sigLen, NULL, NULL,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyVerifySignature(key, algorithm, in1, in2, error) ?
                                            kCFBooleanTrue : NULL;
                                        });
}

OSStatus SecKeySignDigest(
                          SecKeyRef           key,            /* Private key */
                          const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                          const uint8_t       *digestData,	/* signature over this digest */
                          size_t              digestDataLen,/* length of digestData */
                          uint8_t             *sig,			/* signature, RETURNED */
                          size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, false);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, digestData, digestDataLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

#if TARGET_OS_OSX
/* On OS X, SecKeyGetAlgorithmID has a different function signature (two arguments,
   with output in the second argument). Therefore, avoid implementing this function here
   if compiling for OS X.
 */
#else
// Export original SecKeyGetAlgorithmID symbol for backward binary compatibility.
#undef SecKeyGetAlgorithmID
CFIndex SecKeyGetAlgorithmID(SecKeyRef key);
CFIndex SecKeyGetAlgorithmID(SecKeyRef key) {
	return SecKeyGetAlgorithmId(key);
}
#endif

OSStatus SecKeyCopyPublicBytes(SecKeyRef key, CFDataRef* serializedPublic) {
    if (key->key_class->version > 1 && key->key_class->copyPublic)
        return key->key_class->copyPublic(key, serializedPublic);
    return errSecUnimplemented;
}

SecKeyRef SecKeyCreateFromPublicBytes(CFAllocatorRef allocator, CFIndex algorithmID, const uint8_t *keyData, CFIndex keyDataLength)
{
    switch (algorithmID)
    {
        case kSecRSAAlgorithmID:
            return SecKeyCreateRSAPublicKey(allocator,
                                            keyData, keyDataLength,
                                            kSecKeyEncodingBytes);
        case kSecECDSAAlgorithmID:
            return SecKeyCreateECPublicKey(allocator,
                                           keyData, keyDataLength,
                                           kSecKeyEncodingBytes);
        default:
            return NULL;
    }
}

SecKeyRef SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef serialized)
{
    return SecKeyCreateFromPublicBytes(allocator, algorithmID, CFDataGetBytePtr(serialized), CFDataGetLength(serialized));
}

// This is a bit icky hack to avoid changing the vtable for
// SecKey.
size_t SecKeyGetSize(SecKeyRef key, SecKeySize whichSize)
{
    size_t result = SecKeyGetBlockSize(key);

    if (whichSize == 0 || whichSize == 10) {
        // kSecKeyKeySizeInBits is declared as 0 on iOS (SPI) and 10 on macOS (API). Unified implementation
        // here deals with both values.
        whichSize = kSecKeyKeySizeInBits;
    }

    if (kSecECDSAAlgorithmID == SecKeyGetAlgorithmId(key)) {
        switch (whichSize) {
            case kSecKeyEncryptedDataSize:
                result = 0;
                break;
            case kSecKeySignatureSize:
                result = (result >= 66 ? 9 : 8) + 2 * result;
                break;
            case kSecKeyKeySizeInBits:
                if (result >= 66)
                    return 521;
        }
    }

    if (whichSize == kSecKeyKeySizeInBits)
        result *= 8;

    return result;

}

OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData)
{
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecReturnRef,             kCFBooleanTrue,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecValuePersistentRef,    persistentRef,
                                                         NULL);
    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching(query, &foundRef);

    if (status == errSecSuccess) {
        if (CFGetTypeID(foundRef) == SecKeyGetTypeID()) {
            *lookedUpData = (SecKeyRef) foundRef;
            foundRef = NULL;
            status = errSecSuccess;
        } else {
            status = errSecItemNotFound;
        }
    }

    CFReleaseSafe(foundRef);
    CFReleaseSafe(query);

    return status;
}

OSStatus SecKeyCopyPersistentRef(SecKeyRef key, CFDataRef* persistentRef)
{
    if (!key) {
        secerror("SecKeyCopyPersistentRef: Need a key reference for this to work");
        return errSecParam;
    }
    if (!persistentRef) {
        secerror("SecKeyCopyPersistentRef: Need a persistentRef pointer for this to work");
        return errSecParam;
    }    

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecReturnPersistentRef,   kCFBooleanTrue,
                                                         kSecValueRef,              key,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         NULL);
    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching(query, &foundRef);

    if (status == errSecSuccess) {
        if (CFGetTypeID(foundRef) == CFDataGetTypeID()) {
            *persistentRef = foundRef;
            foundRef = NULL;
        } else {
            secerror("SecKeyCopyPersistentRef: SecItemCopyMatching returned success, but we got type %lu instead of CFData for key %@.", CFGetTypeID(foundRef), key);
            status = errSecItemNotFound;
        }
    } else {
        secerror("SecKeyCopyPersistentRef: received status %i for key %@", (int)status, key);
        CFStringRef str = CFStringCreateWithFormat(NULL, NULL, CFSTR("Expected to find persistentref for key %@"), key);
        __security_stackshotreport(str, (int)status);
        CFReleaseNull(str);
    }

    CFReleaseSafe(foundRef);
    CFReleaseSafe(query);

    return status;
}

/*
 *
 */

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL(_kSecKeyWrapPGPSymAlg, "kSecKeyWrapPGPSymAlg");
SEC_CONST_DECL(_kSecKeyWrapPGPFingerprint, "kSecKeyWrapPGPFingerprint");
SEC_CONST_DECL(_kSecKeyWrapPGPWrapAlg, "kSecKeyWrapPGPWrapAlg");
SEC_CONST_DECL(_kSecKeyWrapRFC6637Flags, "kSecKeyWrapPGPECFlags");
SEC_CONST_DECL(_kSecKeyWrapRFC6637WrapDigestSHA256KekAES128, "kSecKeyWrapPGPECWrapDigestSHA256KekAES128");
SEC_CONST_DECL(_kSecKeyWrapRFC6637WrapDigestSHA512KekAES256, "kSecKeyWrapPGPECWrapDigestSHA512KekAES256");

#undef SEC_CONST_DECL

CFDataRef
_SecKeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    if (error)
        *error = NULL;
    if (outParam)
        *outParam = NULL;
    if (key->key_class->version > 2 && key->key_class->copyWrapKey)
        return key->key_class->copyWrapKey(key, type, unwrappedKey, parameters, outParam, error);
    SecError(errSecUnsupportedOperation, error, CFSTR("No key wrap supported for key %@"), key);
    return NULL;
}

CFDataRef
_SecKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    if (error)
        *error = NULL;
    if (outParam)
        *outParam = NULL;
    if (key->key_class->version > 2 && key->key_class->copyUnwrapKey)
        return key->key_class->copyUnwrapKey(key, type, wrappedKey, parameters, outParam, error);

    SecError(errSecUnsupportedOperation, error, CFSTR("No key unwrap for key %@"), key);
    return NULL;
}

static CFIndex SecKeyParamsGetCFIndex(CFTypeRef value, CFStringRef errName, CFErrorRef *error) {
    CFIndex result = -1;
    CFNumberRef localValue = NULL;
    
    if (isString(value)) {
        CFNumberFormatterRef formatter = CFNumberFormatterCreate(kCFAllocatorDefault, CFLocaleGetSystem(), kCFNumberFormatterDecimalStyle);
        localValue = CFNumberFormatterCreateNumberFromString(kCFAllocatorDefault, formatter, value, NULL, kCFNumberFormatterParseIntegersOnly);
        CFReleaseSafe(formatter);
    
        if (localValue) {
            CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%@"), localValue);
            if (CFEqual(t, value)) {
                value = localValue;
            }
            CFReleaseSafe(t);
        }
    }

    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue(value, kCFNumberCFIndexType, &result) || result < 0) {
            SecError(errSecParam, error, CFSTR("Unsupported %@: %@"), errName, value);
        }
    } else {
        SecError(errSecParam, error, CFSTR("Unsupported %@: %@"), errName, value);
    }
    
    CFReleaseSafe(localValue);
    return result;
}

SecKeyRef SecKeyCreateWithData(CFDataRef keyData, CFDictionaryRef parameters, CFErrorRef *error) {

    SecKeyRef key = NULL;
    CFAllocatorRef allocator = NULL;

    if (CFDictionaryGetValue(parameters, kSecAttrTokenID) != NULL) {
        return SecKeyCreateCTKKey(allocator, parameters, error);
    }
    else if (!keyData) {
        SecError(errSecParam, error, CFSTR("Failed to provide key data to SecKeyCreateWithData"));
        return NULL;
    }
    /* First figure out the key type (algorithm). */
    CFIndex algorithm, class;
    CFTypeRef ktype = CFDictionaryGetValue(parameters, kSecAttrKeyType);
    require_quiet((algorithm = SecKeyParamsGetCFIndex(ktype, CFSTR("key type"), error)) >= 0, out);
    CFTypeRef kclass = CFDictionaryGetValue(parameters, kSecAttrKeyClass);
    require_quiet((class = SecKeyParamsGetCFIndex(kclass, CFSTR("key class"), error)) >= 0, out);

    switch (class) {
        case 0: // kSecAttrKeyClassPublic
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    key = SecKeyCreateRSAPublicKey(allocator,
                                                   CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                   kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("RSA public key creation from data failed"));
                    }
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    key = SecKeyCreateECPublicKey(allocator,
                                                  CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                  kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("EC public key creation from data failed"));
                    }
                    break;
                default:
                    SecError(errSecParam, error, CFSTR("Unsupported public key type: %@"), ktype);
                    break;
            };
            break;
        case 1: // kSecAttrKeyClassPrivate
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    key = SecKeyCreateRSAPrivateKey(allocator,
                                                    CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                    kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("RSA private key creation from data failed"));
                    }
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    key = SecKeyCreateECPrivateKey(allocator,
                                                   CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                   kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("EC private key creation from data failed"));
                    }
                    break;
                default:
                    SecError(errSecParam, error, CFSTR("Unsupported private key type: %@"), ktype);
                    break;
            };
            break;
        case 2: // kSecAttrKeyClassSymmetric
            SecError(errSecUnimplemented, error, CFSTR("Unsupported symmetric key type: %@"), ktype);
            break;
        default:
            SecError(errSecParam, error, CFSTR("Unsupported key class: %@"), kclass);
            break;
    }

out:
    return key;
}

// Similar to CFErrorPropagate, but does not consult input value of *error, it can contain any garbage and if overwritten, previous value is never released.
static inline bool SecKeyErrorPropagate(bool succeeded, CFErrorRef possibleError CF_CONSUMED, CFErrorRef *error) {
    if (succeeded) {
        return true;
    } else {
        if (error) {
            *error = possibleError;
        } else {
            CFRelease(possibleError);
        }
        return false;
    }
}

CFDataRef SecKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    if (!key->key_class->copyExternalRepresentation) {
        if (error != NULL) {
            *error = NULL;
        }
        SecError(errSecUnimplemented, error, CFSTR("export not implemented for key %@"), key);
        return NULL;
    }

    CFErrorRef localError = NULL;
    CFDataRef result = key->key_class->copyExternalRepresentation(key, &localError);
    SecKeyErrorPropagate(result != NULL, localError, error);
    return result;
}

CFDictionaryRef SecKeyCopyAttributes(SecKeyRef key) {
    if (key->key_class->copyDictionary) {
        return key->key_class->copyDictionary(key);
    } else {
        // Create dictionary with basic values derived from other known information of the key.
        CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFIndex blockSize = SecKeyGetBlockSize(key) * 8;
        if (blockSize > 0) {
            CFNumberRef blockSizeRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &blockSize);
            CFDictionarySetValue(dict, kSecAttrKeySizeInBits, blockSizeRef);
            CFRelease(blockSizeRef);
        }

        switch (SecKeyGetAlgorithmId(key)) {
            case kSecRSAAlgorithmID:
                CFDictionarySetValue(dict, kSecAttrKeyType, kSecAttrKeyTypeRSA);
                break;
            case kSecECDSAAlgorithmID:
                CFDictionarySetValue(dict, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
                break;
        }

        if (key->key_class->rawSign != NULL || key->key_class->decrypt != NULL) {
            CFDictionarySetValue(dict, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
        } else if (key->key_class->rawVerify != NULL || key->key_class->encrypt != NULL) {
            CFDictionarySetValue(dict, kSecAttrKeyClass, kSecAttrKeyClassPublic);
        }

        return dict;
    }
}

SecKeyRef SecKeyCopyPublicKey(SecKeyRef key) {
    SecKeyRef result = NULL;
    if (key->key_class->version >= 4 && key->key_class->copyPublicKey) {
        result = key->key_class->copyPublicKey(key);
        if (result != NULL) {
            return result;
        }
    }

    CFDataRef serializedPublic = NULL;

    require_noerr_quiet(SecKeyCopyPublicBytes(key, &serializedPublic), fail);
    require_quiet(serializedPublic, fail);

    result = SecKeyCreateFromPublicData(kCFAllocatorDefault, SecKeyGetAlgorithmId(key), serializedPublic);

fail:
    CFReleaseSafe(serializedPublic);
    return result;
}

SecKeyRef SecKeyCreateRandomKey(CFDictionaryRef parameters, CFErrorRef *error) {
    @autoreleasepool {
        SecKeyRef privKey = NULL, pubKey = NULL;
        OSStatus status = SecKeyGeneratePair(parameters, &pubKey, &privKey);
        if (status != errSecSuccess) {
            if (error != NULL) {
                *error = NULL;
            }
            SecError(status, error, CFSTR("Key generation failed, error %d"), (int)status);
        }
        CFReleaseSafe(pubKey);
        return privKey;
    }
}

SecKeyRef SecKeyCreateDuplicate(SecKeyRef key) {
    if (key->key_class->version >= 4 && key->key_class->createDuplicate) {
        return key->key_class->createDuplicate(key);
    } else {
        return (SecKeyRef)CFRetain(key);
    }
}

Boolean SecKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    if (key == NULL) {
        SecCTKKeySetTestMode(name, value);
        return true;
    } else if (key->key_class->version >= 4 && key->key_class->setParameter) {
        CFErrorRef localError = NULL;
        Boolean result = key->key_class->setParameter(key, name, value, &localError);
        SecKeyErrorPropagate(result, localError, error);
        return result;
    } else {
        if (error != NULL) {
            *error = NULL;
        }
        return SecError(errSecUnimplemented, error, CFSTR("setParameter not implemented for %@"), key);
    }
}

#pragma mark Generic algorithm adaptor lookup and invocation

static CFTypeRef SecKeyCopyBackendOperationResult(SecKeyOperationContext *context, SecKeyAlgorithm algorithm,
                                                  CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = kCFNull;
    assert(CFArrayGetCount(context->algorithm) > 0);
    if (context->key->key_class->version >= 4 && context->key->key_class->copyOperationResult != NULL) {
        return context->key->key_class->copyOperationResult(context->key, context->operation, algorithm,
                                                            context->algorithm, context->mode, in1, in2, error);
    }

    // Mapping from algorithms to legacy SecPadding values.
    static const struct {
        const SecKeyAlgorithm *algorithm;
        CFIndex keyAlg;
        SecPadding padding;
    } paddingMap[] = {
        { &kSecKeyAlgorithmRSASignatureRaw, kSecRSAAlgorithmID, kSecPaddingNone },
        { &kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw, kSecRSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmECDSASignatureRFC4754, kSecECDSAAlgorithmID, kSecPaddingSigRaw },
        { &kSecKeyAlgorithmECDSASignatureDigestX962, kSecECDSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmRSAEncryptionRaw, kSecRSAAlgorithmID, kSecPaddingNone },
        { &kSecKeyAlgorithmRSAEncryptionPKCS1, kSecRSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmRSAEncryptionOAEPSHA1, kSecRSAAlgorithmID, kSecPaddingOAEP },
    };
    SecPadding padding = (SecPadding)-1;
    CFIndex keyAlg = SecKeyGetAlgorithmId(context->key);
    for (size_t i = 0; i < array_size(paddingMap); ++i) {
        if (keyAlg == paddingMap[i].keyAlg && CFEqual(algorithm, *paddingMap[i].algorithm)) {
            padding = paddingMap[i].padding;
            break;
        }
    }
    require_quiet(padding != (SecPadding)-1, out);

    // Check legacy virtual table entries.
    size_t size = 0;
    OSStatus status = errSecSuccess;
    switch (context->operation) {
        case kSecKeyOperationTypeSign:
            if (context->key->key_class->rawSign != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeySignatureSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->rawSign(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        case kSecKeyOperationTypeVerify:
            if (context->key->key_class->rawVerify != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    status = context->key->key_class->rawVerify(context->key, padding,
                                                                CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                                CFDataGetBytePtr(in2), CFDataGetLength(in2));
                }
            }
            break;
        case kSecKeyOperationTypeEncrypt:
            if (context->key->key_class->encrypt != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeyEncryptedDataSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->encrypt(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        case kSecKeyOperationTypeDecrypt:
            if (context->key->key_class->decrypt != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeyEncryptedDataSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->decrypt(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        default:
            goto out;
    }

    if (status == errSecSuccess) {
        if (CFGetTypeID(result) == CFDataGetTypeID()) {
            CFDataSetLength((CFMutableDataRef)result, size);
        }
    } else {
        SecError(status, error, CFSTR("legacy SecKey backend operation:%d(%d) failed"), (int)context->operation, (int)padding);
        CFReleaseNull(result);
    }

out:
    return result;
}

CFTypeRef SecKeyRunAlgorithmAndCopyResult(SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    @autoreleasepool {
        // Check algorithm array for cycles; if any value of it is duplicated inside, report 'algorithm not found' error.
        CFIndex algorithmCount = CFArrayGetCount(context->algorithm);
        for (CFIndex index = 0; index < algorithmCount - 1; index++) {
            SecKeyAlgorithm indexAlgorithm = CFArrayGetValueAtIndex(context->algorithm, index);
            for (CFIndex tested = index + 1; tested < algorithmCount; tested++) {
                require_quiet(!CFEqual(indexAlgorithm, CFArrayGetValueAtIndex(context->algorithm, tested)), fail);
            }
        }

        SecKeyAlgorithm algorithm = CFArrayGetValueAtIndex(context->algorithm, algorithmCount - 1);
        CFTypeRef output = SecKeyCopyBackendOperationResult(context, algorithm, in1, in2, error);
        if (output != kCFNull) {
            // Backend handled the operation, return result.
            return output;
        }

        // To silence static analyzer.
        CFReleaseSafe(output);

        // Get adaptor which is able to handle requested algorithm.
        SecKeyAlgorithmAdaptor adaptor = SecKeyGetAlgorithmAdaptor(context->operation, algorithm);
        require_quiet(adaptor != NULL, fail);

        // Invoke the adaptor and return result.
        CFTypeRef result = adaptor(context, in1, in2, error);
        require_quiet(result != kCFNull, fail);
        return result;

    fail:
        if (context->mode == kSecKeyOperationModePerform) {
            SecError(errSecParam, error, CFSTR("%@: algorithm not supported by the key %@"),
                     CFArrayGetValueAtIndex(context->algorithm, 0), context->key);
            return NULL;
        } else {
            return kCFNull;
        }
    }
}

#pragma mark Algorithm-related SecKey API entry points

static CFMutableArrayRef SecKeyCreateAlgorithmArray(SecKeyAlgorithm algorithm) {
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(result, algorithm);
    return result;
}

CFDataRef SecKeyCreateSignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef dataToSign, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    SecKeyOperationContext context = { key, kSecKeyOperationTypeSign, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, dataToSign, NULL, &localError);
    SecKeyOperationContextDestroy(&context);
    SecKeyErrorPropagate(result != NULL, localError, error);
    return result;
}

Boolean SecKeyVerifySignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef signedData, CFDataRef signature,
                              CFErrorRef *error) {
    CFErrorRef localError = NULL;
    SecKeyOperationContext context = { key, kSecKeyOperationTypeVerify, SecKeyCreateAlgorithmArray(algorithm) };
    CFTypeRef res = SecKeyRunAlgorithmAndCopyResult(&context, signedData, signature, &localError);
    Boolean result = CFEqualSafe(res, kCFBooleanTrue);
    CFReleaseSafe(res);
    SecKeyOperationContextDestroy(&context);
    SecKeyErrorPropagate(result, localError, error);
    return result;
}

CFDataRef SecKeyCreateEncryptedDataWithParameters(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plaintext,
                                                  CFDictionaryRef parameters, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    SecKeyOperationContext context = { key, kSecKeyOperationTypeEncrypt, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, plaintext, parameters, &localError);
    SecKeyOperationContextDestroy(&context);
    SecKeyErrorPropagate(result, localError, error);
    return result;
}

CFDataRef SecKeyCreateEncryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plaintext, CFErrorRef *error) {
    return SecKeyCreateEncryptedDataWithParameters(key, algorithm, plaintext, NULL, error);
}

CFDataRef SecKeyCreateDecryptedDataWithParameters(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef ciphertext,
                                                  CFDictionaryRef parameters, CFErrorRef *error) {
    SecKeyOperationContext context = { key, kSecKeyOperationTypeDecrypt, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, ciphertext, parameters, error);
    SecKeyOperationContextDestroy(&context);
    return result;
}

CFDataRef SecKeyCreateDecryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef ciphertext, CFErrorRef *error) {
    return SecKeyCreateDecryptedDataWithParameters(key, algorithm, ciphertext, NULL, error);
}

CFDataRef SecKeyCopyKeyExchangeResult(SecKeyRef key, SecKeyAlgorithm algorithm, SecKeyRef publicKey,
                                      CFDictionaryRef parameters, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    CFDataRef publicKeyData = NULL, result = NULL;
    SecKeyOperationContext context = { key, kSecKeyOperationTypeKeyExchange, SecKeyCreateAlgorithmArray(algorithm) };
    require_quiet(publicKeyData = SecKeyCopyExternalRepresentation(publicKey, error), out);
    result = SecKeyRunAlgorithmAndCopyResult(&context, publicKeyData, parameters, &localError);
    SecKeyErrorPropagate(result != NULL, localError, error);

out:
    CFReleaseSafe(publicKeyData);
    SecKeyOperationContextDestroy(&context);
    return result;
}

Boolean SecKeyIsAlgorithmSupported(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm) {
    SecKeyOperationContext context = { key, operation, SecKeyCreateAlgorithmArray(algorithm), kSecKeyOperationModeCheckIfSupported };
    CFErrorRef error = NULL;
    CFTypeRef res = SecKeyRunAlgorithmAndCopyResult(&context, NULL, NULL, &error);
    Boolean result = CFEqualSafe(res, kCFBooleanTrue);
    CFReleaseSafe(res);
    CFReleaseSafe(error);
    SecKeyOperationContextDestroy(&context);
    return result;
}
