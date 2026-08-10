/*
 * Copyright (c) 2010-2015 Apple Inc. All Rights Reserved.
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
 * SecECKey.m - CoreFoundation based ECDSA key object
 */

#include "SecECKey.h"
#include "SecECKeyPriv.h"

#import <Foundation/Foundation.h>

#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <AssertMacros.h>
#include <Security/SecureTransport.h> /* For error codes. */
#include <CoreFoundation/CFData.h> /* For error codes. */
#include <CoreFoundation/CFNumber.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <utilities/debugging.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccder_decode_eckey.h>

#define kMaximumECKeySize 521

static CFIndex SecECKeyGetAlgorithmID(SecKeyRef key) {
    return kSecECDSAAlgorithmID;
}


/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecECPublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccec_pub_ctx_t pubkey = key->key;
    if (ccec_ctx_cp(pubkey))
        cc_clear(ccec_pub_ctx_size(ccn_sizeof_n(ccec_ctx_n(pubkey))), pubkey);
}

static ccec_const_cp_t getCPForPublicSize(CFIndex encoded_length)
{
    size_t keysize = ccec_x963_import_pub_size(encoded_length);
    if(ccec_keysize_is_supported(keysize)) {
        return ccec_get_cp(keysize);
    }
    return NULL;
}

static ccec_const_cp_t getCPForPrivateSize(CFIndex encoded_length)
{
    size_t keysize = ccec_x963_import_priv_size(encoded_length);
    if(ccec_keysize_is_supported(keysize)) {
        return ccec_get_cp(keysize);
    }
    return NULL;
}

static ccoid_t ccoid_secp192r1 = CC_EC_OID_SECP192R1;
static ccoid_t ccoid_secp256r1 = CC_EC_OID_SECP256R1;
static ccoid_t ccoid_secp224r1 = CC_EC_OID_SECP224R1;
static ccoid_t ccoid_secp384r1 = CC_EC_OID_SECP384R1;
static ccoid_t ccoid_secp521r1 = CC_EC_OID_SECP521R1;

// <rdar://problem/66864716> OID_CERTICOM is wrong
static ccoid_t ccoid_libder_secp384r1 = ((unsigned char *)"\x06\x04\x2B\x84\x00\x22");
static ccoid_t ccoid_libder_secp521r1 = ((unsigned char *)"\x06\x04\x2B\x84\x00\x23");

static ccec_const_cp_t ccec_cp_for_oid(const unsigned char *oid)
{
    if (oid!=NULL) {
        if (ccoid_equal(oid, ccoid_secp192r1)) {
            return ccec_cp_192();
        } else if (ccoid_equal(oid, ccoid_secp256r1)) {
            return ccec_cp_256();
        } else if (ccoid_equal(oid, ccoid_secp224r1)) {
            return ccec_cp_224();
        } else if (ccoid_equal(oid, ccoid_secp384r1) || ccoid_equal(oid, ccoid_libder_secp384r1)) {
            return ccec_cp_384();
        } else if (ccoid_equal(oid, ccoid_secp521r1) || ccoid_equal(oid, ccoid_libder_secp521r1)) {
            return ccec_cp_521();
        }
    }
    return (ccec_const_cp_t){NULL};
}

static OSStatus SecECPublicKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    ccec_pub_ctx_t pubkey = key->key;
    OSStatus err = errSecParam;

    switch (encoding) {
    case kSecDERKeyEncoding:
    {
        const SecDERKey *derKey = (const SecDERKey *)keyData;
        if (keyDataLength != sizeof(SecDERKey)) {
            err = errSecDecode;
            break;
        }

        require_action_quiet(derKey->parameters && derKey->parametersLength > 2 &&
                             derKey->parameters[1] <= derKey->parametersLength - 2, errOut, err = errSecDecode);
        ccec_const_cp_t cp = ccec_cp_for_oid(derKey->parameters);
        require_action_quiet(cp, errOut, err = errSecDecode);

        err = (ccec_import_pub(cp, derKey->keyLength, derKey->key, pubkey)
               ? errSecDecode : errSecSuccess);
        break;
    }
    case kSecKeyEncodingBytes:
    {
        ccec_const_cp_t cp = getCPForPublicSize(keyDataLength);
        require_action_quiet(cp, errOut, err = errSecDecode);
        err = (ccec_import_pub(cp, keyDataLength, keyData, pubkey)
               ? errSecDecode : errSecSuccess);
        break;
    }
    case kSecExtractPublicFromPrivate:
    {
        ccec_full_ctx_t fullKey = (ccec_full_ctx_t)keyData;

        cc_size fullKeyN = ccec_ctx_n(fullKey);
        require_quiet(fullKeyN <= ccn_nof(kMaximumECKeySize), errOut);
        memcpy(pubkey, fullKey, ccec_pub_ctx_size(ccn_sizeof_n(fullKeyN)));
        err = errSecSuccess;
        break;
    }
    case kSecKeyEncodingApplePkcs1:
    default:
        err = errSecParam;
        break;
    }

errOut:
    return err;
}

static CFTypeRef SecECPublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                   CFArrayRef algorithms, SecKeyOperationMode mode,
                                                   CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeVerify || !CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureDigestX962)) {
        // EC public key supports only signature verification with X962 algorithm.
        return kCFNull;
    }

    if (mode == kSecKeyOperationModePerform) {
        bool valid = false;
        int err = -1;
        size_t sigLen = CFDataGetLength(in2);
        uint8_t *sig = (uint8_t *)CFDataGetBytePtr(in2);
        ccec_pub_ctx_t pubkey = key->key;

        err = ccec_verify(pubkey, CFDataGetLength(in1), CFDataGetBytePtr(in1), sigLen, sig, &valid);
        if (err != 0) {
            SecError(errSecVerifyFailed, error, CFSTR("EC signature verification failed (ccerr %d)"), err);
            return NULL;
        } else if (!valid) {
            SecError(errSecVerifyFailed, error, CFSTR("EC signature verification failed, no match"));
            return NULL;
        } else {
            return kCFBooleanTrue;
        }
    } else {
        // Algorithm is supported.
        return kCFBooleanTrue;
    }
}

static size_t SecECPublicKeyBlockSize(SecKeyRef key) {
    /* Get key size in octets */
    return ccec_ctx_size(ccec_ctx_pub(key->key));
}

/* Encode the public key and return it in a newly allocated CFDataRef. */
static CFDataRef SecECPublicKeyExport(CFAllocatorRef allocator,
	ccec_pub_ctx_t pubkey) {
    size_t pub_size = ccec_export_pub_size(pubkey);
	CFMutableDataRef blob = CFDataCreateMutableWithScratch(allocator, pub_size);
    ccec_export_pub(pubkey, CFDataGetMutableBytePtr(blob));
	return blob;
}

static CFDataRef SecECPublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    ccec_pub_ctx_t pubkey = key->key;
    return SecECPublicKeyExport(NULL, pubkey);
}

static OSStatus SecECPublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serailziation)
{
    ccec_pub_ctx_t pubkey = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serailziation = SecECPublicKeyExport(allocator, pubkey);

    if (NULL == *serailziation)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDictionaryRef SecECPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    CFDictionaryRef dict = SecKeyGeneratePublicAttributeDictionary(key, kSecAttrKeyTypeEC);
    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(mutableDict, kSecAttrCanDerive, kCFBooleanFalse);
    CFAssignRetained(dict, mutableDict);
    return dict;
}

static const char *
getCurveName(SecKeyRef key)
{
    SecECNamedCurve curveType = SecECKeyGetNamedCurve(key);

    switch (curveType)
    {
        case kSecECCurveSecp256r1:
            return "kSecECCurveSecp256r1";
            break;
        case kSecECCurveSecp384r1:
            return "kSecECCurveSecp384r1";
            break;
        case kSecECCurveSecp521r1:
            return "kSecECCurveSecp521r1";
        default:
            return "kSecECCurveNone";
    }
}

static CFStringRef SecECPublicKeyCopyKeyDescription(SecKeyRef key)
{
    NSMutableString *strings[2];
    const char* curve = getCurveName(key);

    ccec_pub_ctx_t ecPubkey = key->key;
    size_t len = ccec_ctx_size(ecPubkey);
    NSMutableData *buffer = [NSMutableData dataWithLength:len];
    for (int i = 0; i < 2; ++i) {
        ccn_write_uint(ccec_ctx_n(ecPubkey), (i == 0) ? ccec_ctx_x(ecPubkey) : ccec_ctx_y(ecPubkey), len, buffer.mutableBytes);
        strings[i] = [NSMutableString stringWithCapacity:len * 2];
        for (size_t byteIndex = 0; byteIndex < len; ++byteIndex) {
            [strings[i] appendFormat:@"%02X", ((const uint8_t *)buffer.bytes)[byteIndex]];
        }
    }

    NSString *description = [NSString stringWithFormat:@"<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, y: %@, x: %@, addr: %p>",
                             curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version,
                             8 * SecKeyGetBlockSize(key), strings[1], strings[0], key];
    return CFBridgingRetain(description);
}

static const struct ccec_rfc6637_curve * get_rfc6637_curve(SecKeyRef key)
{
    SecECNamedCurve curveType = SecECKeyGetNamedCurve(key);

    if (curveType == kSecECCurveSecp256r1) {
        return &ccec_rfc6637_dh_curve_p256;
    } else if (curveType == kSecECCurveSecp521r1) {
        return &ccec_rfc6637_dh_curve_p521;
    }
    return NULL;
}

static CFDataRef SecECKeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    ccec_pub_ctx_t pubkey = key->key;
    int err = errSecUnimplemented;
    const struct ccec_rfc6637_curve *curve;
    const struct ccec_rfc6637_wrap *wrap = NULL;
    uint8_t sym_alg = 0;
    int32_t flags = 0;

    if (type != kSecKeyWrapPublicKeyPGP) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported key wrapping algorithm"));
        return NULL;
    }

    curve = get_rfc6637_curve(key);
    if (curve == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
        return NULL;
    }

    CFNumberRef num = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPSymAlg);
    if (!isNumber(num) || !CFNumberGetValue(num, kCFNumberSInt8Type, &sym_alg)) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown symalg given"));
        return NULL;
    }

    CFDataRef fingerprint = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPFingerprint);
    if (!isData(fingerprint) || CFDataGetLength(fingerprint) < kSecKeyWrapPGPFingerprintMinSize) {
        SecError(errSecUnsupportedOperation, error, CFSTR("invalid fingerprint"));
        return NULL;
    }

    CFTypeRef wrapAlg = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPWrapAlg);
    if (wrapAlg == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("no wrap alg"));
        return NULL;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128)) {
        wrap = &ccec_rfc6637_wrap_sha256_kek_aes128;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256)) {
        wrap = &ccec_rfc6637_wrap_sha512_kek_aes256;
    } else {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown wrap alg"));
        return NULL;
    }

    num = CFDictionaryGetValue(parameters, _kSecKeyWrapRFC6637Flags);
    if (isNumber(num)) {
        if (!CFNumberGetValue(num, kCFNumberSInt32Type, &flags)) {
            SecError(errSecUnsupportedOperation, error, CFSTR("invalid flags: %@"), num);
            return NULL;
        }
    } else if (num) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown flags"));
        return NULL;
    }

    CFIndex unwrappedKey_size = CFDataGetLength(unwrappedKey);

    CFIndex output_size = ccec_rfc6637_wrap_key_size(pubkey, flags, unwrappedKey_size);
    if (output_size == 0) {
        SecError(errSecUnsupportedOperation, error, CFSTR("can't wrap that key, can't build size"));
        return NULL;
    }

    CFMutableDataRef data = CFDataCreateMutableWithScratch(NULL, output_size);
    require_quiet(data, errOut);

    err = ccec_rfc6637_wrap_key(pubkey, CFDataGetMutableBytePtr(data), flags,
                                sym_alg, CFDataGetLength(unwrappedKey), CFDataGetBytePtr(unwrappedKey),
                                curve, wrap, CFDataGetBytePtr(fingerprint),
                                ccrng_seckey);
    if (err) {
        SecError(errSecUnsupportedOperation, error, CFSTR("Failed to wrap key"));
        CFReleaseNull(data);
    }

errOut:
    return data;
}

SecKeyDescriptor kSecECPublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "ECPublicKey",
    .extraBytes = ccec_pub_ctx_size(ccn_sizeof(kMaximumECKeySize)),
    .init = SecECPublicKeyInit,
    .destroy = SecECPublicKeyDestroy,
    .blockSize = SecECPublicKeyBlockSize,
    .copyDictionary = SecECPublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecECPublicKeyCopyExternalRepresentation,
    .describe = SecECPublicKeyCopyKeyDescription,
    .getAlgorithmID = SecECKeyGetAlgorithmID,
    .copyPublic = SecECPublicKeyCopyPublicOctets,
    .copyWrapKey = SecECKeyCopyWrapKey,
    .copyOperationResult = SecECPublicKeyCopyOperationResult,
};

/* Public Key API functions. */
SecKeyRef SecKeyCreateECPublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecECPublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}



/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecECPrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccec_full_ctx_t fullkey = key->key;

    if (ccec_ctx_cp(fullkey))
        cc_clear(ccec_full_ctx_size(ccn_sizeof_n(ccec_ctx_n(fullkey))), fullkey);
}


static OSStatus SecECPrivateKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    ccec_full_ctx_t fullkey = key->key;
    OSStatus err = errSecParam;

    switch (encoding) {
    case kSecKeyEncodingPkcs1:
    {
        /* TODO: DER import size (and thus cp), pub.x, pub.y and k. */
        //err = ecc_import(keyData, keyDataLength, fullkey);

        /* DER != PKCS#1, but we'll go along with it */
        const unsigned char *oid;
        size_t n;
        ccec_const_cp_t cp;

        require_noerr_quiet(ccec_der_import_priv_keytype(keyDataLength, keyData, (ccoid_t*)&oid, &n), abort);
        cp = ccec_cp_for_oid(oid);
        if (cp == NULL) {
            cp = ccec_curve_for_length_lookup(n * 8 /* bytes -> bits */,
                ccec_cp_192(), ccec_cp_224(), ccec_cp_256(), ccec_cp_384(), ccec_cp_521(), NULL);
        }
        require_action_quiet(cp != NULL, abort, err = errSecDecode);
        ccec_ctx_init(cp, fullkey);

        require_noerr_quiet(ccec_der_import_priv(cp, keyDataLength, keyData, fullkey), abort);
        err = errSecSuccess;
        break;
    }
    case kSecKeyEncodingBytes:
    {
        ccec_const_cp_t cp = getCPForPrivateSize(keyDataLength);
        require_quiet(cp != NULL, abort);

        ccec_ctx_init(cp, fullkey);
        size_t pubSize = ccec_export_pub_size(ccec_ctx_pub(fullkey));

        require_quiet(pubSize < (size_t) keyDataLength, abort);
        require_noerr_action_quiet(ccec_import_pub(cp, pubSize, keyData, ccec_ctx_pub(fullkey)),
                             abort,
                             err = errSecDecode);


        keyData += pubSize;
        keyDataLength -= pubSize;

        cc_unit *k = ccec_ctx_k(fullkey);
        require_noerr_action_quiet(ccn_read_uint(ccec_ctx_n(fullkey), k, keyDataLength, keyData),
                                   abort,
                                   err = errSecDecode);

        err = errSecSuccess;
        break;

    }
    case kSecGenerateKey:
    {
        CFDictionaryRef parameters = (CFDictionaryRef) keyData;

        CFTypeRef ksize = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);
        CFIndex keyLengthInBits = getIntValue(ksize);

        ccec_const_cp_t cp = ccec_get_cp(keyLengthInBits);

        if (!cp) {
            secwarning("Invalid or missing key size in: %@", parameters);
            return errSecKeySizeNotAllowed;
        }

        if (!ccec_generate_key_fips(cp, ccrng_seckey, fullkey))
            err = errSecSuccess;
        break;
    }

    default:
        break;
    }
abort:
    return err;
}

static CFTypeRef SecECPrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                    CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                                    CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    // Default answer is 'unsupported', unless we find out that we can support it.
    CFTypeRef result = kCFNull;

    ccec_full_ctx_t fullkey = key->key;
    switch (operation) {
        case kSecKeyOperationTypeSign: {
            if (CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureDigestX962)) {
                if (mode == kSecKeyOperationModePerform) {
                    // Perform x962 mode of signature.
                    size_t size = ccec_sign_max_size(ccec_ctx_cp(fullkey));
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    int err = ccec_sign(fullkey, CFDataGetLength(in1), CFDataGetBytePtr(in1),
                                        &size, CFDataGetMutableBytePtr((CFMutableDataRef)result), ccrng_seckey);
                    require_action_quiet(err == 0, out, (CFReleaseNull(result),
                                                         SecError(errSecParam, error, CFSTR("%@: X962 signing failed (ccerr %d)"),
                                                                  key, err)));
                    CFDataSetLength((CFMutableDataRef)result, size);
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        }
        case kSecKeyOperationTypeKeyExchange:
            if (CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeStandard) ||
                CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor)) {
                if (mode == kSecKeyOperationModePerform) {
                    int err;
                    ccec_const_cp_t cp = getCPForPublicSize(CFDataGetLength(in1));
                    require_action_quiet(cp != NULL, out,
                                         SecError(errSecParam, error, CFSTR("ECpriv sharedsecret: bad public key")));
                    ccec_pub_ctx_decl_cp(cp, pubkey);
                    err = ccec_import_pub(cp, CFDataGetLength(in1), CFDataGetBytePtr(in1), pubkey);
                    require_noerr_action_quiet(err, out, SecError(errSecParam, error,
                                                                  CFSTR("ECpriv sharedsecret: bad public key (err %d)"), err));
                    size_t size = ccec_ccn_size(cp);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    err = ccecdh_compute_shared_secret(fullkey, pubkey, &size,
                                                       CFDataGetMutableBytePtr((CFMutableDataRef)result), ccrng_seckey);
                    require_noerr_action_quiet(err, out, (CFReleaseNull(result),
                                                          SecError(errSecDecode, error,
                                                                   CFSTR("ECpriv failed to compute shared secret (err %d)"), err)));
                    CFDataSetLength((CFMutableDataRef)result, size);
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        default:
            break;
    }

out:
    return result;
}

static size_t SecECPrivateKeyBlockSize(SecKeyRef key) {
    ccec_full_ctx_t fullkey = key->key;
    /* Get key size in octets */
    return ccec_ctx_size(fullkey);
}

static OSStatus SecECPrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serailziation)
{
    ccec_full_ctx_t fullkey = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serailziation = SecECPublicKeyExport(allocator, ccec_ctx_pub(fullkey));

    if (NULL == *serailziation)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDataRef SecECPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    ccec_full_ctx_t fullkey = key->key;
    size_t prime_size = ccec_cp_prime_size(ccec_ctx_cp(fullkey));
    size_t key_size = ccec_export_pub_size(ccec_ctx_pub(fullkey)) + prime_size;
    CFMutableDataRef blob = CFDataCreateMutableWithScratch(NULL, key_size);
    ccec_export_pub(ccec_ctx_pub(fullkey), CFDataGetMutableBytePtr(blob));
    UInt8 *dest = CFDataGetMutableBytePtr(blob) + ccec_export_pub_size(ccec_ctx_pub(fullkey));
    const cc_unit *k = ccec_ctx_k(fullkey);
    ccn_write_uint_padded(ccec_ctx_n(fullkey), k, prime_size, dest);
    return blob;
}

static CFDictionaryRef SecECPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    /* Export the full ec key pair. */
    CFDataRef fullKeyBlob = SecECPrivateKeyCopyExternalRepresentation(key, NULL);

    CFDictionaryRef dict = SecKeyGeneratePrivateAttributeDictionary(key, kSecAttrKeyTypeEC, fullKeyBlob);
	CFReleaseSafe(fullKeyBlob);
	return dict;
}
static CFStringRef SecECPrivateKeyCopyKeyDescription(SecKeyRef key) {

    const char* curve = getCurveName(key);

	return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);

}

static CFDataRef SecECKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    const struct ccec_rfc6637_curve *curve;
    const struct ccec_rfc6637_unwrap *unwrap;
    ccec_full_ctx_t fullkey = key->key;
    CFMutableDataRef data;
    int res;
    uint8_t sym_alg = 0;
    int32_t flags = 0;

    curve = get_rfc6637_curve(key);
    if (curve == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
        return NULL;
    }

    CFTypeRef wrapAlg = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPWrapAlg);
    if (wrapAlg == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("no wrap alg"));
        return NULL;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128)) {
        unwrap = &ccec_rfc6637_unwrap_sha256_kek_aes128;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256)) {
        unwrap = &ccec_rfc6637_unwrap_sha512_kek_aes256;
    } else {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown wrap alg"));
        return NULL;
    }

    CFDataRef fingerprint = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPFingerprint);
    if (!isData(fingerprint) || CFDataGetLength(fingerprint) < kSecKeyWrapPGPFingerprintMinSize) {
        SecError(errSecUnsupportedOperation, error, CFSTR("invalid fingerprint"));
        return NULL;
    }

    CFNumberRef num = CFDictionaryGetValue(parameters, _kSecKeyWrapRFC6637Flags);
    if (isNumber(num)) {
        if (!CFNumberGetValue(num, kCFNumberSInt32Type, &flags)) {
            SecError(errSecUnsupportedOperation, error, CFSTR("invalid flags: %@"), num);
            return NULL;
        }
    } else if (num) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown flags"));
        return NULL;
    }

    size_t keysize = CFDataGetLength(wrappedKey);
    data = CFDataCreateMutableWithScratch(NULL, keysize);
    if (data == NULL)
        return NULL;

    res = ccec_rfc6637_unwrap_key(fullkey, &keysize, CFDataGetMutableBytePtr(data),
                                  flags, &sym_alg, curve, unwrap,
                                  CFDataGetBytePtr(fingerprint),
                                  CFDataGetLength(wrappedKey), CFDataGetBytePtr(wrappedKey));
    if (res != 0) {
        CFReleaseNull(data);
        SecError(errSecUnsupportedOperation, error, CFSTR("failed to wrap key"));
        return NULL;
    }
    assert(keysize <= (size_t)CFDataGetLength(data));
    CFDataSetLength(data, keysize);

    if (outParam) {
        CFMutableDictionaryRef out =  CFDictionaryCreateMutableForCFTypes(NULL);
        if (out) {
            CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt8Type, &sym_alg);
            if (num) {
                CFDictionarySetValue(out, _kSecKeyWrapPGPSymAlg, num);
                CFRelease(num);
            }
            *outParam = out;
        }
    }

    return data;
}

SecKeyDescriptor kSecECPrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "ECPrivateKey",
    .extraBytes = ccec_full_ctx_size(ccn_sizeof(kMaximumECKeySize)),

    .init = SecECPrivateKeyInit,
    .destroy = SecECPrivateKeyDestroy,
    .blockSize = SecECPrivateKeyBlockSize,
    .copyDictionary = SecECPrivateKeyCopyAttributeDictionary,
    .describe = SecECPrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecECKeyGetAlgorithmID,
    .copyPublic = SecECPrivateKeyCopyPublicOctets,
    .copyExternalRepresentation = SecECPrivateKeyCopyExternalRepresentation,
    .copyWrapKey = SecECKeyCopyWrapKey,
    .copyUnwrapKey = SecECKeyCopyUnwrapKey,
    .copyOperationResult = SecECPrivateKeyCopyOperationResult,
};

/* Private Key API functions. */
SecKeyRef SecKeyCreateECPrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecECPrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}


OSStatus SecECKeyGeneratePair(CFDictionaryRef parameters,
                              SecKeyRef *publicKey, SecKeyRef *privateKey) {
    OSStatus status = errSecParam;

    CFAllocatorRef allocator = NULL; /* @@@ get from parameters. */
    SecKeyRef pubKey = NULL;

    SecKeyRef privKey = SecKeyCreate(allocator, &kSecECPrivateKeyDescriptor,
                                     (const void*) parameters, 0, kSecGenerateKey);

    require_quiet(privKey, errOut);

    /* Create SecKeyRef's from the pkcs1 encoded keys. */
    pubKey = SecKeyCreate(allocator, &kSecECPublicKeyDescriptor,
                          privKey->key, 0, kSecExtractPublicFromPrivate);

    require_quiet(pubKey, errOut);

    if (publicKey) {
        *publicKey = pubKey;
        pubKey = NULL;
    }
    if (privateKey) {
        *privateKey = privKey;
        privKey = NULL;
    }

    status = errSecSuccess;

errOut:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);

    return status;
}


/* It's debatable whether this belongs here or in the ssl code since the
   curve values come from a tls related rfc4492. */
SecECNamedCurve SecECKeyGetNamedCurve(SecKeyRef key) {
    SecECNamedCurve result = kSecECCurveNone;
    CFDictionaryRef attributes = NULL;
    require_quiet(SecKeyGetAlgorithmId(key) == kSecECDSAAlgorithmID, out);
    require_quiet(attributes = SecKeyCopyAttributes(key), out);
    CFTypeRef bitsRef = CFDictionaryGetValue(attributes, kSecAttrKeySizeInBits);
    CFIndex bits = 0;
    require_quiet(bitsRef != NULL && CFGetTypeID(bitsRef) == CFNumberGetTypeID() &&
                  CFNumberGetValue(bitsRef, kCFNumberCFIndexType, &bits), out);
    switch (bits) {
#if 0
        case 192:
            result = kSecECCurveSecp192r1;
            break;
        case 224:
            result = kSecECCurveSecp224r1;
            break;
#endif
        case 256:
            result = kSecECCurveSecp256r1;
            break;
        case 384:
            result = kSecECCurveSecp384r1;
            break;
        case 521:
            result = kSecECCurveSecp521r1;
            break;
    }

out:
    CFReleaseSafe(attributes);
    return result;
}

CFDataRef SecECKeyCopyPublicBits(SecKeyRef key) {
    CFDataRef bytes = NULL;
    SecKeyCopyPublicBytes(key, &bytes);
    return bytes;
}

/* Vile accessors that get us the pub or priv key to use temporarily */

bool SecECDoWithFullKey(SecKeyRef key, CFErrorRef* error, void (^action)(ccec_full_ctx_t private)) {
    if (key->key_class == &kSecECPrivateKeyDescriptor) {
        action(key->key);
    } else {
        return SecError(errSecParam, error, CFSTR("Not an EC Full Key object, sorry can't do."));
    }

    return true;
}

bool SecECDoWithPubKey(SecKeyRef key, CFErrorRef* error, void (^action)(ccec_pub_ctx_t public)) {
    if (key->key_class == &kSecECPublicKeyDescriptor) {
        action(key->key);
    } else {
        return SecError(errSecParam, error, CFSTR("Not an EC Public Key object, sorry can't do."));
    }

    return true;
}

