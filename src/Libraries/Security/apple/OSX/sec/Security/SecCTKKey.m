/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#include <AssertMacros.h>
#include <Security/SecFramework.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecAccessControlPriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <ctkclient/ctkclient.h>
#include <libaks_acl_cf_keys.h>
#include <coreauthd_spi.h>
#include "OSX/sec/Security/SecItemShim.h"

#include "SecECKey.h"
#include "SecRSAKey.h"
#include "SecCTKKeyPriv.h"

const CFStringRef kSecUseToken = CFSTR("u_Token");

typedef struct {
    TKTokenRef token;
    CFStringRef token_id;
    CFDataRef object_id;
    SecCFDictionaryCOW auth_params;
    SecCFDictionaryCOW attributes;
    CFMutableDictionaryRef params;
} SecCTKKeyData;

static void SecCTKKeyDestroy(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    CFReleaseNull(kd->token);
    CFReleaseNull(kd->token_id);
    CFReleaseNull(kd->object_id);
    CFReleaseNull(kd->auth_params.mutable_dictionary);
    CFReleaseNull(kd->attributes.mutable_dictionary);
    CFReleaseNull(kd->params);
}

static CFIndex SecCTKGetAlgorithmID(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    if (CFEqualSafe(CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandom) ||
        CFEqualSafe(CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandomPKA) ||
        CFEqualSafe(CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeyType), kSecAttrKeyTypeSecureEnclaveAttestation)) {
        return kSecECDSAAlgorithmID;
    }
    return kSecRSAAlgorithmID;
}

static SecItemAuthResult SecCTKProcessError(CFStringRef operation, TKTokenRef token, CFDataRef object_id, CFArrayRef *ac_pairs, CFErrorRef *error) {
    if (CFEqualSafe(CFErrorGetDomain(*error), CFSTR(kTKErrorDomain)) &&
        CFErrorGetCode(*error) == kTKErrorCodeAuthenticationNeeded &&
        operation != NULL) {
        CFDataRef access_control = TKTokenCopyObjectAccessControl(token, object_id, error);
        if (access_control != NULL) {
            CFArrayRef ac_pair = CFArrayCreateForCFTypes(NULL, access_control, operation, NULL);
            CFAssignRetained(*ac_pairs, CFArrayCreateForCFTypes(NULL, ac_pair, NULL));

            CFReleaseNull(*error);
            CFRelease(ac_pair);
            CFRelease(access_control);
            return kSecItemAuthResultNeedAuth;
        }
    }
    return kSecItemAuthResultError;
}

static TKTokenRef SecCTKKeyCreateToken(SecKeyRef key, CFDictionaryRef auth_params, CFDictionaryRef *last_params, CFErrorRef *error) {
    TKTokenRef token = NULL;
    SecCTKKeyData *kd = key->key;
    SecCFDictionaryCOW attributes = { auth_params };
    if (kd->params && CFDictionaryGetCount(kd->params) > 0) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attributes), CFSTR(kTKTokenCreateAttributeAuxParams), kd->params);
    }
    require_quiet(token = SecTokenCreate(kd->token_id, &attributes, error), out);
    if (last_params != NULL) {
        CFAssignRetained(*last_params, auth_params ? CFDictionaryCreateCopy(NULL, auth_params) : NULL);
    }

out:
    CFReleaseNull(attributes.mutable_dictionary);
    return token;
}

static TKTokenRef SecCTKKeyCopyToken(SecKeyRef key, CFErrorRef *error) {
    SecCTKKeyData *kd = key->key;
    TKTokenRef token = CFRetainSafe(kd->token);
    if (token == NULL) {
        token = SecCTKKeyCreateToken(key, kd->auth_params.dictionary, NULL, error);
    }
    return token;
}

static CFTypeRef SecCTKKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                              CFArrayRef algorithms, SecKeyOperationMode mode,
                                              CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    SecCTKKeyData *kd = key->key;
    __block SecCFDictionaryCOW auth_params = { kd->auth_params.dictionary };
    __block CFDictionaryRef last_params = kd->auth_params.dictionary ? CFDictionaryCreateCopy(NULL, kd->auth_params.dictionary) : NULL;
    __block TKTokenRef token = CFRetainSafe(kd->token);
    __block CFTypeRef result = kCFNull;

    CFErrorRef localError = NULL;
    SecItemAuthDo(&auth_params, &localError, ^SecItemAuthResult(CFArrayRef *ac_pairs, CFErrorRef *error) {
        if (!CFEqualSafe(last_params, auth_params.dictionary) || token == NULL) {
            // token was not connected yet or auth_params were modified, so reconnect the token in order to update the attributes.
            CFAssignRetained(token, SecCTKKeyCreateToken(key, auth_params.dictionary, &last_params, error));
            if (token == NULL) {
                return kSecItemAuthResultError;
            }
        }

        result = kCFBooleanTrue;
        if (mode == kSecKeyOperationModePerform) {
            // Check, whether we are not trying to perform the operation with large data.  If yes, explicitly do the check whether
            // the operation is supported first, in order to avoid jetsam of target extension with operation type which is typically
            // not supported by the extension at all.
            // <rdar://problem/31762984> unable to decrypt large data with kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM
            CFIndex inputSize = 0;
            if (in1 != NULL && CFGetTypeID(in1) == CFDataGetTypeID()) {
                inputSize += CFDataGetLength(in1);
            }
            if (in2 != NULL && CFGetTypeID(in2) == CFDataGetTypeID()) {
                inputSize += CFDataGetLength(in2);
            }
            if (inputSize > 32 * 1024) {
                result = TKTokenCopyOperationResult(token, kd->object_id, operation, algorithms, kSecKeyOperationModeCheckIfSupported,
                                                    NULL, NULL, error);
            }
        }

        if (CFEqualSafe(result, kCFBooleanTrue)) {
            result = TKTokenCopyOperationResult(token, kd->object_id, operation, algorithms, mode, in1, in2, error);
        }

        if (result != NULL) {
            return kSecItemAuthResultOK;
        }

        CFStringRef AKSOperation = NULL;
        switch (operation) {
            case kSecKeyOperationTypeSign:
                AKSOperation = kAKSKeyOpSign;
                break;
            case kSecKeyOperationTypeDecrypt: {
                AKSOperation = kAKSKeyOpDecrypt;
                if (in2 != NULL && CFGetTypeID(in2) == CFDictionaryGetTypeID() && CFDictionaryGetValue(in2, kSecKeyEncryptionParameterRecryptCertificate) != NULL) {
                    // This is actually recrypt operation, which is special separate AKS operation.
                    AKSOperation = kAKSKeyOpECIESTranscode;
                }
                break;
            }
            case kSecKeyOperationTypeKeyExchange:
                AKSOperation = kAKSKeyOpComputeKey;
                break;
            default:
                break;;
        }
        return SecCTKProcessError(AKSOperation, token, kd->object_id, ac_pairs, error);
    }, ^{
        CFAssignRetained(token, SecCTKKeyCreateToken(key, auth_params.dictionary, &last_params, NULL));
    });

    CFErrorPropagate(localError, error);
    CFReleaseNull(auth_params.mutable_dictionary);
    CFReleaseNull(token);
    CFReleaseNull(last_params);
    return result;
}

static size_t SecCTKKeyBlockSize(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    CFTypeRef keySize = CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeySizeInBits);
    if (CFGetTypeID(keySize) == CFNumberGetTypeID()) {
        CFIndex bitSize;
        if (CFNumberGetValue(keySize, kCFNumberCFIndexType, &bitSize))
            return (bitSize + 7) / 8;
    }

    return 0;
}

static OSStatus SecCTKKeyCopyPublicOctets(SecKeyRef key, CFDataRef *data) {
    OSStatus status = errSecSuccess;
    CFErrorRef error = NULL;
    CFDataRef publicData = NULL;
    TKTokenRef token = NULL;

    SecCTKKeyData *kd = key->key;
    require_action_quiet(token = SecCTKKeyCopyToken(key, &error), out, status = SecErrorGetOSStatus(error));
    require_action_quiet(publicData = TKTokenCopyPublicKeyData(token, kd->object_id, &error), out,
                         status = SecErrorGetOSStatus(error));
    *data = publicData;

out:
    CFReleaseSafe(error);
    CFReleaseSafe(token);
    return status;
}

static CFStringRef SecCTKKeyCopyKeyDescription(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecKeyRef:('%@') %p>"),
                                    CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrTokenID), key);
}

// Attributes allowed to be exported from all internal key attributes.
static const CFStringRef *kSecExportableCTKKeyAttributes[] = {
    &kSecClass,
    &kSecAttrTokenID,
    &kSecAttrKeyClass,
    &kSecAttrAccessControl,
    &kSecAttrIsPrivate,
    &kSecAttrIsModifiable,
    &kSecAttrKeyType,
    &kSecAttrKeySizeInBits,
    &kSecAttrEffectiveKeySize,
    &kSecAttrIsSensitive,
    &kSecAttrWasAlwaysSensitive,
    &kSecAttrIsExtractable,
    &kSecAttrWasNeverExtractable,
    &kSecAttrCanEncrypt,
    &kSecAttrCanDecrypt,
    &kSecAttrCanDerive,
    &kSecAttrCanSign,
    &kSecAttrCanVerify,
    &kSecAttrCanSignRecover,
    &kSecAttrCanVerifyRecover,
    &kSecAttrCanWrap,
    &kSecAttrCanUnwrap,
    NULL
};

static CFDictionaryRef SecCTKKeyCopyAttributeDictionary(SecKeyRef key) {
    CFMutableDictionaryRef attrs = NULL;
    CFErrorRef error = NULL;
    CFDataRef publicData = NULL, digest = NULL;
    TKTokenRef token = NULL;
    SecCTKKeyData *kd = key->key;

    // Encode ApplicationLabel as SHA1 digest of public key bytes.
    require_quiet(token = SecCTKKeyCopyToken(key, &error), out);
    require_quiet(publicData = TKTokenCopyPublicKeyData(token, kd->object_id, &error), out);

    // Calculate the digest of the public key.
    require_quiet(digest = SecSHA1DigestCreate(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData)), out);
    attrs = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(key));
    CFDictionarySetValue(attrs, kSecAttrApplicationLabel, digest);

    for (const CFStringRef **attrKey = &kSecExportableCTKKeyAttributes[0]; *attrKey != NULL; attrKey++) {
        CFTypeRef value = CFDictionaryGetValue(kd->attributes.dictionary, **attrKey);
        if (value != NULL) {
            CFDictionarySetValue(attrs, **attrKey, value);
        }
    }

    // Consistently with existing RSA and EC software keys implementation, mark all keys as permanent ones.
    CFDictionarySetValue(attrs, kSecAttrIsPermanent, kCFBooleanTrue);

    // Always export token_id and object_id.
    CFDictionarySetValue(attrs, kSecAttrTokenID, kd->token_id);
    CFDictionarySetValue(attrs, kSecAttrTokenOID, kd->object_id);

out:
    CFReleaseSafe(error);
    CFReleaseSafe(publicData);
    CFReleaseSafe(digest);
    CFReleaseSafe(token);
    return attrs;
}

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef key);

static Boolean SecCTKKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    SecCTKKeyData *kd = key->key;
    CFTypeRef acm_reference = NULL;

    static const CFStringRef *const knownUseFlags[] = {
        &kSecUseOperationPrompt,
        &kSecUseAuthenticationContext,
        &kSecUseAuthenticationUI,
        &kSecUseCallerName,
        &kSecUseCredentialReference,
    };

    // Check, whether name is part of known use flags.
    bool isUseFlag = false;
    for (size_t i = 0; i < array_size(knownUseFlags); i++) {
        if (CFEqual(*knownUseFlags[i], name)) {
            isUseFlag = true;
            break;
        }
    }

    if (CFEqual(name, kSecUseAuthenticationContext)) {
        // Preprocess LAContext to ACMRef value.
        if (value != NULL) {
            require_quiet(acm_reference = LACopyACMContext(value, error), out);
            value = acm_reference;
        }
        name = kSecUseCredentialReference;
    }

    // Release existing token connection to enforce creation of new connection with new params.
    CFReleaseNull(kd->token);

    if (isUseFlag) {
        if (value != NULL) {
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->auth_params), name, value);
        } else {
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->auth_params), name);
        }
    } else {
        if (kd->params == NULL) {
            kd->params = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        if (value != NULL) {
            CFDictionarySetValue(kd->params, name, value);
        } else {
            CFDictionaryRemoveValue(kd->params, name);
        }
    }

out:
    CFReleaseSafe(acm_reference);
    return TRUE;
}

static Boolean SecCTKKeyIsEqual(SecKeyRef key1, SecKeyRef key2) {
    SecCTKKeyData *kd1 = key1->key;
    SecCTKKeyData *kd2 = key2->key;

    return CFEqual(kd1->token_id, kd2->token_id) && CFEqual(kd1->object_id, kd2->object_id);
}

static SecKeyDescriptor kSecCTKKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "CTKKey",
    .extraBytes = sizeof(SecCTKKeyData),

    .destroy = SecCTKKeyDestroy,
    .blockSize = SecCTKKeyBlockSize,
    .copyDictionary = SecCTKKeyCopyAttributeDictionary,
    .describe = SecCTKKeyCopyKeyDescription,
    .getAlgorithmID = SecCTKGetAlgorithmID,
    .copyPublic = SecCTKKeyCopyPublicOctets,
    .copyOperationResult = SecCTKKeyCopyOperationResult,
    .isEqual = SecCTKKeyIsEqual,
    .createDuplicate = SecCTKKeyCreateDuplicate,
    .setParameter = SecCTKKeySetParameter,
};

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef key) {
    SecKeyRef result = SecKeyCreate(CFGetAllocator(key), &kSecCTKKeyDescriptor, 0, 0, 0);
    SecCTKKeyData *kd = key->key, *rd = result->key;
    rd->token = CFRetainSafe(kd->token);
    rd->object_id = CFRetainSafe(kd->object_id);
    rd->token_id = CFRetainSafe(kd->token_id);
    if (kd->attributes.dictionary != NULL) {
        rd->attributes.dictionary = kd->attributes.dictionary;
        SecCFDictionaryCOWGetMutable(&rd->attributes);
    }
    if (kd->auth_params.dictionary != NULL) {
        rd->auth_params.dictionary = kd->auth_params.dictionary;
        SecCFDictionaryCOWGetMutable(&rd->auth_params);
    }
    return result;
}

SecKeyRef SecKeyCreateCTKKey(CFAllocatorRef allocator, CFDictionaryRef refAttributes, CFErrorRef *error) {
    SecKeyRef result = NULL;
    SecKeyRef key = SecKeyCreate(allocator, &kSecCTKKeyDescriptor, 0, 0, 0);
    SecCTKKeyData *kd = key->key;
    kd->token = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecUseToken));
    kd->object_id = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecAttrTokenOID));
    kd->token_id = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecAttrTokenID));
    kd->attributes.dictionary = refAttributes;
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecUseToken);
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrTokenOID);
    SecItemAuthCopyParams(&kd->auth_params, &kd->attributes);
    if (CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrIsPrivate) == NULL) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrIsPrivate, kCFBooleanTrue);
    }

    CFMutableDictionaryRef attrs = NULL;
    if (kd->token == NULL) {
        require_quiet(kd->token = SecCTKKeyCopyToken(key, error), out);
        if (kd->token != NULL) {
            attrs = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, kd->attributes.dictionary);
            CFAssignRetained(kd->object_id, TKTokenCreateOrUpdateObject(kd->token, kd->object_id, attrs, error));
            require_quiet(kd->object_id, out);
            CFDictionaryForEach(attrs, ^(const void *key, const void *value) {
                CFDictionaryAddValue(SecCFDictionaryCOWGetMutable(&kd->attributes), key, value);
            });
            
            CFTypeRef accc = CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrAccessControl);
            if (accc && CFDataGetTypeID() == CFGetTypeID(accc)) {
                SecAccessControlRef ac = SecAccessControlCreateFromData(kCFAllocatorDefault, accc, error);
                require_quiet(ac, out);
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrAccessControl, ac);
                CFRelease(ac);
            }
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrTokenOID);
        }
        require_quiet(kd->token != NULL && kd->object_id != NULL, out);
    }

    // Convert some attributes which are stored as numbers in iOS keychain but a lot of code counts that the values
    // are actually strings as specified by kSecAttrXxx constants.
    static const CFStringRef *numericAttributes[] = {
        &kSecAttrKeyType,
        &kSecAttrKeyClass,
        NULL,
    };

    for (const CFStringRef **attrName = &numericAttributes[0]; *attrName != NULL; attrName++) {
        CFTypeRef value = CFDictionaryGetValue(kd->attributes.dictionary, **attrName);
        if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
            CFIndex number;
            if (CFNumberGetValue(value, kCFNumberCFIndexType, &number)) {
                CFStringRef newValue = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%ld"), (long)number);
                if (newValue != NULL) {
                    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), **attrName, newValue);
                    CFRelease(newValue);
                }
            }
        }
    }

    // Sanitize some important attributes.
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecClass, kSecClassKey);
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrKeyClass, kSecAttrKeyClassPrivate);

    result = (SecKeyRef)CFRetain(key);

out:
    CFReleaseSafe(attrs);
    CFReleaseSafe(key);
    return result;
}

OSStatus SecCTKKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    OSStatus status;
    __block SecCFDictionaryCOW attrs = { NULL };
    CFDataRef publicData = NULL;

    require_action_quiet(publicKey != NULL, out, status = errSecParam);
    require_action_quiet(privateKey != NULL, out, status = errSecParam);

    // Simply adding key on the token without value will cause the token to generate the key.
    // Prepare dictionary specifying item to add.
    attrs.dictionary = CFDictionaryGetValue(parameters, kSecPrivateKeyAttrs);

    CFDictionaryForEach(parameters, ^(const void *key, const void *value) {
        if (!CFEqual(key, kSecPrivateKeyAttrs) && !CFEqual(key, kSecPublicKeyAttrs)) {
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attrs), key, value);
        }
    });
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&attrs), kSecValueData);
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attrs), kSecClass, kSecClassKey);
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attrs), kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attrs), kSecReturnRef, kCFBooleanTrue);

    // Do not automatically store tke key into the keychain, caller will do it on its own if it is really requested.
    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attrs), kSecAttrIsPermanent, kCFBooleanFalse);

    // Add key from given attributes to the token (having no data will cause the token to actually generate the key).
    require_noerr_quiet(status = SecItemAdd(attrs.dictionary, (CFTypeRef *)privateKey), out);

    // Create non-token public key.
    require_noerr_quiet(status = SecCTKKeyCopyPublicOctets(*privateKey, &publicData), out);
    if (CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeEC) ||
        CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandomPKA) ||
        CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeSecureEnclaveAttestation)) {
        *publicKey = SecKeyCreateECPublicKey(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData),
                                             kSecKeyEncodingBytes);
    } else if (CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeRSA)) {
        *publicKey = SecKeyCreateRSAPublicKey(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData),
                                              kSecKeyEncodingBytes);
    }

    if (*publicKey != NULL) {
        status = errSecSuccess;
    } else {
        status = errSecInvalidKey;
        CFReleaseNull(*privateKey);
    }

out:
    CFReleaseSafe(attrs.mutable_dictionary);
    CFReleaseSafe(publicData);
    return status;
}

const CFStringRef kSecKeyParameterSETokenAttestationNonce = CFSTR("com.apple.security.seckey.setoken.attestation-nonce");

SecKeyRef SecKeyCopyAttestationKey(SecKeyAttestationKeyType keyType, CFErrorRef *error) {
    CFDictionaryRef attributes = NULL;
    CFDataRef object_id = NULL;
    SecKeyRef key = NULL;

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.sik" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t sikObjectIDBytes[] = { 0x04, 21, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 's', 'i', 'k' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.gid" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t gidObjectIDBytes[] = { 0x04, 21, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'g', 'i', 'd' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.uikc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t uikCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'u', 'i', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.uikp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t uikProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'u', 'i', 'k', 'p' };

    static const uint8_t casdObjectIDBytes[] = { 0x04, 27, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 'c', 'e', 'l', 'e', 'm', 't', 'o', 'k', 'e', 'n', '.', 'c', 'a', 's', 'd' };

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.oikc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t oikCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'o', 'i', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.oikp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t oikProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'o', 'i', 'k', 'p' };

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.dakc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t dakCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'd', 'a', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.dakp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t dakProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'd', 'a', 'k', 'p' };

    CFStringRef token = kSecAttrTokenIDAppleKeyStore;
    
    switch (keyType) {
        case kSecKeyAttestationKeyTypeSIK:
            object_id = CFDataCreate(kCFAllocatorDefault, sikObjectIDBytes, sizeof(sikObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeGID:
            object_id = CFDataCreate(kCFAllocatorDefault, gidObjectIDBytes, sizeof(gidObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeUIKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, uikCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeUIKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, uikProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeSecureElement:
            object_id = CFDataCreate(kCFAllocatorDefault, casdObjectIDBytes, sizeof(casdObjectIDBytes));
            token = kSecAttrTokenIDSecureElement;
            break;
        case kSecKeyAttestationKeyTypeOIKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, oikCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeOIKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, oikProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeDAKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, dakCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeyAttestationKeyTypeDAKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, dakProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        default:
            SecError(errSecParam, error, CFSTR("unexpected attestation key type %d"), (int)keyType);
            goto out;
    }

    attributes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                              kSecAttrTokenOID, object_id,
                                              kSecAttrTokenID, token,
                                              NULL);
    key = SecKeyCreateCTKKey(kCFAllocatorDefault, attributes, error);

out:
    CFReleaseSafe(attributes);
    CFReleaseSafe(object_id);
    return key;
}

CFDataRef SecKeyCreateAttestation(SecKeyRef key, SecKeyRef keyToAttest, CFErrorRef *error) {
    __block CFDictionaryRef attributes = NULL, outputAttributes = NULL;
    CFDataRef attestationData = NULL;
    CFErrorRef localError = NULL;
    SecCTKKeyData *attestingKeyData = key->key;
    SecCTKKeyData *keyToAttestData = keyToAttest->key;
    __block TKTokenRef token = NULL;

    if (error == NULL) {
        error = &localError;
    }

    __block SecCFDictionaryCOW auth_params = { keyToAttestData->auth_params.dictionary };

    require_action_quiet(key->key_class == &kSecCTKKeyDescriptor, out,
                         SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported by key %@"), key));
    require_action_quiet(keyToAttest->key_class == &kSecCTKKeyDescriptor, out,
                         SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported for key %@"), keyToAttest));

    attributes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                              CFSTR(kTKTokenControlAttribAttestingKey), attestingKeyData->object_id,
                                              CFSTR(kTKTokenControlAttribKeyToAttest), keyToAttestData->object_id,
                                              NULL);

    bool ok = SecItemAuthDo(&auth_params, error, ^SecItemAuthResult(CFArrayRef *ac_pairs, CFErrorRef *error) {
        if (auth_params.mutable_dictionary != NULL || token == NULL) {
            CFAssignRetained(token, SecCTKKeyCopyToken(key, error));
            if (token == NULL) {
                return kSecItemAuthResultError;
            }
        }

        outputAttributes = TKTokenControl(token, attributes, error);
        return outputAttributes ? kSecItemAuthResultOK : SecCTKProcessError(kAKSKeyOpAttest, keyToAttestData->token, keyToAttestData->object_id, ac_pairs, error);
    }, NULL);
    require_quiet(ok, out);
    require_action_quiet(attestationData = CFRetainSafe(CFDictionaryGetValue(outputAttributes, CFSTR(kTKTokenControlAttribAttestationData))),
                         out, SecError(errSecInternal, error, CFSTR("could not get attestation data")));

out:
    CFReleaseSafe(attributes);
    CFReleaseSafe(outputAttributes);
    CFReleaseSafe(localError);
    CFReleaseSafe(auth_params.mutable_dictionary);
    CFReleaseSafe(token);
    return attestationData;
}

Boolean SecKeyControlLifetime(SecKeyRef key, SecKeyControlLifetimeType type, CFErrorRef *error) {
    NSError *localError;
    __block id token;
    if (error == NULL) {
        error = (void *)&localError;
    }

    SecCTKKeyData *keyData = key->key;
    NSDictionary *attributes = @{
        @kTKTokenControlAttribLifetimeControlKey: (__bridge NSData *)keyData->object_id,
        @kTKTokenControlAttribLifetimeType: @(type),
    };

    if (key->key_class != &kSecCTKKeyDescriptor) {
        return SecError(errSecUnsupportedOperation, error, CFSTR("lifetimecontrol not supported for key %@"), key);
    }

    __block SecCFDictionaryCOW auth_params = { keyData->auth_params.dictionary };
    return SecItemAuthDo(&auth_params, error, ^SecItemAuthResult(CFArrayRef *ac_pairs, CFErrorRef *error) {
        if (auth_params.mutable_dictionary != NULL || token == NULL) {
            token = CFBridgingRelease(SecCTKKeyCopyToken(key, error));
            if (token == nil) {
                return kSecItemAuthResultError;
            }
        }

        NSDictionary *outputAttributes = CFBridgingRelease(TKTokenControl((__bridge TKTokenRef)token, (__bridge CFDictionaryRef)attributes, error));
        return outputAttributes ? kSecItemAuthResultOK : kSecItemAuthResultError;
    }, NULL);
}

void SecCTKKeySetTestMode(CFStringRef tokenID, CFTypeRef enable) {
    CFErrorRef error = NULL;
    CFDictionaryRef options = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kSecAttrTokenID, tokenID, @kTKTokenCreateAttributeTestMode, enable, nil);
    TKTokenRef token = TKTokenCreate(options, &error);
    if (token == NULL) {
        secerror("Failed to set token attributes %@: error %@", options, error);
    }
    CFReleaseNull(options);
    CFReleaseNull(error);
    CFReleaseNull(token);
}
