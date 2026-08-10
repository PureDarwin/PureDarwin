/*
 * Copyright (c) 2006-2010,2012-2015 Apple Inc. All Rights Reserved.
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
 * SecRSAKey.c - CoreFoundation based rsa key object
 */


#include "SecRSAKey.h"
#include "SecRSAKeyPriv.h"
#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <AssertMacros.h>
#include <Security/SecureTransport.h> /* For error codes. */
#include <CoreFoundation/CFData.h> /* For error codes. */
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <CoreFoundation/CFNumber.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/array_size.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

#include <libDER/asn1Types.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>

#include <CommonCrypto/CommonDigest.h>

#include <corecrypto/ccrsa_priv.h>

#include <stdint.h>
#include <string.h>

#define kMaximumRSAKeyBits (1024 * 8)

#define RSA_PKCS1_PAD_SIGN		0x01
#define RSA_PKCS1_PAD_ENCRYPT	0x02

/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecRSAPublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    if (key->key) {
        ccrsa_pub_ctx_t pubkey = key->key;
        cc_clear(ccrsa_pub_ctx_size(ccn_sizeof_n(ccrsa_ctx_n(pubkey))), pubkey);
        free(key->key);
        key->key = NULL;
    }
}

#define cc_skip_zeros(size, ptr) { while (size > 0 && *ptr == 0) { ++ptr; --size; } }

//
// pubkey is initilaized with an n which is the maximum it can hold
// We set the n to its correct value given m.
//
static int ccrsa_pub_init(ccrsa_pub_ctx_t pubkey,
                          size_t m_size, const uint8_t* m,
                          size_t e_size, const uint8_t* e)
{
    cc_skip_zeros(m_size, m);

    cc_size nm = ccn_nof_size(m_size);
    if (nm > ccrsa_ctx_n(pubkey))
        return -1;

    ccrsa_ctx_n(pubkey) = nm;

    ccn_read_uint(nm, ccrsa_ctx_m(pubkey), m_size, m);
    cczp_init(ccrsa_ctx_zm(pubkey));

    return ccn_read_uint(nm, ccrsa_ctx_e(pubkey), e_size, e);
}


static OSStatus ccrsa_pub_decode_apple(ccrsa_pub_ctx_t pubkey, size_t pkcs1_size, const uint8_t* pkcs1)
{
    OSStatus result = errSecParam;

	DERItem keyItem = {(DERByte *)pkcs1, pkcs1_size};
    DERRSAPubKeyApple decodedKey;

	require_noerr_action_quiet(DERParseSequence(&keyItem,
                                                DERNumRSAPubKeyAppleItemSpecs, DERRSAPubKeyAppleItemSpecs,
                                                &decodedKey, sizeof(decodedKey)),
                               errOut, result = errSecDecode);

    // We could honor the reciprocal, but we don't think this is used enough to care.
    // Don't bother exploding the below function to try to handle this case, it computes.

    require_noerr_quiet(ccrsa_pub_init(pubkey,
                                       decodedKey.modulus.length, decodedKey.modulus.data,
                                       decodedKey.pubExponent.length, decodedKey.pubExponent.data),
                        errOut);

    result = errSecSuccess;

errOut:
    return result;
}


static void ccasn_encode_int(cc_size n, const cc_unit*s, size_t s_size, uint8_t **buffer)
{
    **buffer = ASN1_INTEGER;
    *buffer += 1;

    DERSize itemLength = 4;
    DEREncodeLength(s_size, *buffer, &itemLength);
    *buffer += itemLength;

    ccn_write_int(n, s, s_size, *buffer);

    *buffer += s_size;
}


static OSStatus SecRSAPublicKeyInit(SecKeyRef key,
                                    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {

    OSStatus result = errSecParam;
    ccrsa_pub_ctx_t pubkey;
    size_t size_n = 0;

    switch (encoding) {
        case kSecKeyEncodingBytes: // Octets is PKCS1
        case kSecKeyEncodingPkcs1: {
            size_n = ccrsa_import_pub_n(keyDataLength, keyData);
            require_quiet(size_n != 0, errOut);
            require_quiet(size_n <= ccn_nof(kMaximumRSAKeyBits), errOut);

            key->key = calloc(1, ccrsa_pub_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            pubkey = key->key;
            ccrsa_ctx_n(pubkey) = size_n;

            require_noerr_quiet(ccrsa_import_pub(pubkey, keyDataLength, keyData), errOut);

            result = errSecSuccess;

            break;
        }
        case kSecKeyEncodingApplePkcs1:
            /* for the few uses (I can't find any) that uses kSecKeyEncodingApplePkcs1, force largest keys  */
            size_n = ccn_nof(kMaximumRSAKeyBits);

            key->key = calloc(1, ccrsa_pub_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            pubkey = key->key;
            ccrsa_ctx_n(pubkey) = size_n;

            result = ccrsa_pub_decode_apple(pubkey, keyDataLength, keyData);
            break;
        case kSecKeyEncodingRSAPublicParams:
        {
            SecRSAPublicKeyParams *params = (SecRSAPublicKeyParams *)keyData;

            size_n = ccn_nof_size(params->modulusLength);

            key->key = calloc(1, ccrsa_pub_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            pubkey = key->key;
            ccrsa_ctx_n(pubkey) = size_n;

            require_noerr_quiet(ccrsa_pub_init(pubkey,
                                               params->modulusLength, params->modulus,
                                               params->exponentLength, params->exponent), errOut);

            result = errSecSuccess;
            break;
        }
        case kSecExtractPublicFromPrivate:
        {
            ccrsa_full_ctx_t fullKey = (ccrsa_full_ctx_t) keyData;

            size_n = ccrsa_ctx_n(fullKey);

            key->key = calloc(1, ccrsa_pub_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            pubkey = key->key;
            ccrsa_ctx_n(pubkey) = size_n;

            memcpy(pubkey, ccrsa_ctx_public(fullKey), ccrsa_pub_ctx_size(ccn_sizeof_n(size_n)));
            result = errSecSuccess;
            break;
        }
        default:
            break;
    }

errOut:
    return result;
}

static CFTypeRef SecRSAPublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                    CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                                    CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result;
    require_action_quiet(CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRawCCUnit), out, result = kCFNull);

    ccrsa_pub_ctx_t pubkey = key->key;
    result = kCFBooleanTrue;
    int ccerr = 0;
    switch (operation) {
        case kSecKeyOperationTypeEncrypt:
            if (mode == kSecKeyOperationModePerform) {
                // Input buffer length must be cc_unit aligned, otherwise it is not a valid cc_unit buffer.
                CFIndex bufferSize = CFDataGetLength(in1);
                require_action_quiet(bufferSize == ccn_sizeof_size(ccrsa_block_size(pubkey)), out,
                                     (result = NULL,
                                      SecError(errSecParam, error, CFSTR("%@: sign - input buffer bad size (%d bytes)"), key,
                                               (int)bufferSize)));

                // Verify that plaintext is smaller than modulus.  Note that since we already verified that input algorithm
                // is kSecKeyAlgorithmRSAEncryptionRawCCUnit, we can safely access in1 CFDataRef contents as cc_unit *.
                require_action_quiet(ccn_cmpn(ccn_nof_size(CFDataGetLength(in1)), (const cc_unit *)CFDataGetBytePtr(in1),
                                              ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey)) < 0, out,
                                     (result = NULL,
                                      SecError(errSecParam, error, CFSTR("RSApubkey wrong size of buffer to encrypt"))));

                // Encrypt into output buffer.
                result = CFDataCreateMutableWithScratch(NULL, bufferSize);
                ccerr = ccrsa_pub_crypt(pubkey, (cc_unit *)CFDataGetMutableBytePtr((CFMutableDataRef)result),
                                        (const cc_unit *)CFDataGetBytePtr(in1));
            }
            break;
        case kSecKeyOperationTypeDecrypt:
            if (mode == kSecKeyOperationModePerform) {
                // Input buffer length must be cc_unit aligned, otherwise it is not a valid cc_unit buffer.
                CFIndex bufferSize = CFDataGetLength(in1);
                require_action_quiet(bufferSize == ccn_sizeof_size(ccrsa_block_size(pubkey)), out,
                                     (result = NULL,
                                      SecError(errSecParam, error, CFSTR("%@: sign - input buffer bad size (%d bytes)"), key,
                                               (int)bufferSize)));

                // Decrypt into output buffer.
                result = CFDataCreateMutableWithScratch(NULL, bufferSize);
                ccerr = ccrsa_pub_crypt(pubkey, (cc_unit *)CFDataGetMutableBytePtr((CFMutableDataRef)result),
                                        (const cc_unit *)CFDataGetBytePtr(in1));
            }
            break;
        default:
            result = kCFNull;
            break;
    }

    require_noerr_action_quiet(ccerr, out, (CFReleaseNull(result),
                                            SecError(errSecParam, error, CFSTR("rsa_pub_crypt failed, ccerr=%d"), ccerr)));
out:
    return result;
}

static size_t SecRSAPublicKeyBlockSize(SecKeyRef key) {
    ccrsa_pub_ctx_t pubkey = key->key;
    return ccrsa_block_size(pubkey);
}


static CFDataRef SecRSAPublicKeyCreatePKCS1(CFAllocatorRef allocator, ccrsa_pub_ctx_t pubkey)
{
    size_t m_size = ccn_write_int_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
    size_t e_size = ccn_write_int_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey));

    const size_t seq_size = DERLengthOfItem(ASN1_INTEGER, m_size) +
    DERLengthOfItem(ASN1_INTEGER, e_size);

    const size_t result_size = DERLengthOfItem(ASN1_SEQUENCE, seq_size);

	CFMutableDataRef pkcs1 = CFDataCreateMutableWithScratch(allocator, result_size);
    uint8_t *bytes = CFDataGetMutableBytePtr(pkcs1);

    *bytes++ = ONE_BYTE_ASN1_CONSTR_SEQUENCE;

    DERSize itemLength = 4;
    DEREncodeLength(seq_size, bytes, &itemLength);
    bytes += itemLength;

    ccasn_encode_int(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey), m_size, &bytes);
    ccasn_encode_int(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey), e_size, &bytes);

    return pkcs1;
}

static OSStatus SecRSAPublicKeyCopyPublicSerialization(SecKeyRef key, CFDataRef* serialized)
{
    ccrsa_pub_ctx_t pubkey = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serialized = SecRSAPublicKeyCreatePKCS1(allocator, pubkey);

    if (NULL == *serialized)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDictionaryRef SecRSAPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    CFDictionaryRef dict = SecKeyGeneratePublicAttributeDictionary(key, kSecAttrKeyTypeRSA);
    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(mutableDict, kSecAttrCanDecrypt, kCFBooleanTrue);
    CFDictionarySetValue(mutableDict, kSecAttrCanDerive, kCFBooleanFalse);
    CFAssignRetained(dict, mutableDict);
    return dict;
}

static CFDataRef SecRSAPublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    ccrsa_pub_ctx_t pubkey = key->key;
    return SecRSAPublicKeyCreatePKCS1(CFGetAllocator(key), pubkey);
}

static CFStringRef SecRSAPublicKeyCopyDescription(SecKeyRef key) {

    CFStringRef keyDescription = NULL;
    CFDataRef modRef = SecKeyCopyModulus(key);

    ccrsa_pub_ctx_t pubkey = key->key;

    CFStringRef modulusString = CFDataCopyHexString(modRef);
    require_quiet(modulusString, fail);

    keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, exponent: {hex: %llx, decimal: %lld}, modulus: %@, addr: %p>"), SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), (long long)*ccrsa_ctx_e(pubkey), (long long)*ccrsa_ctx_e(pubkey), modulusString, key);

fail:
    CFReleaseSafe(modRef);
    CFReleaseSafe(modulusString);
	if(!keyDescription)
		keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);

	return keyDescription;
}

SecKeyDescriptor kSecRSAPublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "RSAPublicKey",

    .init = SecRSAPublicKeyInit,
    .destroy = SecRSAPublicKeyDestroy,
    .blockSize = SecRSAPublicKeyBlockSize,
    .copyDictionary = SecRSAPublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecRSAPublicKeyCopyExternalRepresentation,
    .describe = SecRSAPublicKeyCopyDescription,
    .copyPublic = SecRSAPublicKeyCopyPublicSerialization,
    .copyOperationResult = SecRSAPublicKeyCopyOperationResult,
};

/* Public Key API functions. */
SecKeyRef SecKeyCreateRSAPublicKey_ios(CFAllocatorRef allocator,
                                   const uint8_t *keyData, CFIndex keyDataLength,
                                   SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecRSAPublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

SecKeyRef SecKeyCreateRSAPublicKey(CFAllocatorRef allocator,
                                   const uint8_t *keyData, CFIndex keyDataLength,
                                   SecKeyEncoding encoding) {
    return SecKeyCreateRSAPublicKey_ios(allocator, keyData,
                        keyDataLength, encoding);
}

CFDataRef SecKeyCopyModulus(SecKeyRef key) {
    CFDataRef modulus = NULL;
    if (key->key_class == &kSecRSAPublicKeyDescriptor) {
        ccrsa_pub_ctx_t pubkey = key->key;

        size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));

        CFAllocatorRef allocator = CFGetAllocator(key);
        CFMutableDataRef modulusData = CFDataCreateMutable(allocator, m_size);

        if (modulusData == NULL)
            return NULL;

        CFDataSetLength(modulusData, m_size);

        ccn_write_uint(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey), m_size, CFDataGetMutableBytePtr(modulusData));
        modulus = modulusData;
    } else if (key->key_class->copyDictionary != NULL) {
        CFDictionaryRef dict = key->key_class->copyDictionary(key);
        if (dict != NULL) {
            modulus = CFRetainSafe(CFDictionaryGetValue(dict, CFSTR("_rsam")));
            CFRelease(dict);
        }
    }

    return modulus;
}

CFDataRef SecKeyCopyExponent(SecKeyRef key) {
    CFDataRef exponent = NULL;
    if (key->key_class == &kSecRSAPublicKeyDescriptor) {
        ccrsa_pub_ctx_t pubkey = key->key;

        size_t e_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey));

        CFAllocatorRef allocator = CFGetAllocator(key);
        CFMutableDataRef exponentData = CFDataCreateMutable(allocator, e_size);

        if (exponentData == NULL)
            return NULL;

        CFDataSetLength(exponentData, e_size);

        ccn_write_uint(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey), e_size, CFDataGetMutableBytePtr(exponentData));
        exponent = exponentData;
    } else if (key->key_class->copyDictionary != NULL) {
        CFDictionaryRef dict = key->key_class->copyDictionary(key);
        if (dict != NULL) {
            exponent = CFRetainSafe(CFDictionaryGetValue(dict, CFSTR("_rsae")));
            CFRelease(dict);
        }
    }

    return exponent;
}


/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecRSAPrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    if (key->key) {
        ccrsa_full_ctx_t fullkey = key->key;
        cc_clear(ccrsa_full_ctx_size(ccn_sizeof_n(ccrsa_ctx_n(fullkey))), fullkey);
        free(key->key);
        key->key = NULL;
    }
}

static OSStatus SecRSAPrivateKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    OSStatus result = errSecParam;
    ccrsa_full_ctx_t fullkey;
    cc_size size_n = 0;

    switch (encoding) {
        case kSecKeyEncodingBytes: // Octets is PKCS1
        case kSecKeyEncodingPkcs1:
        {
            size_n = ccrsa_import_priv_n(keyDataLength,keyData);
            require_quiet(size_n != 0, errOut);
            require_quiet(size_n <= ccn_nof(kMaximumRSAKeyBits), errOut);

            key->key = calloc(1, ccrsa_full_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            fullkey = key->key;
            ccrsa_ctx_n(fullkey) = size_n;

            require_quiet(ccrsa_import_priv(fullkey, keyDataLength, keyData)==0, errOut);

            result = errSecSuccess;
            break;
        }
        case kSecGenerateKey:
        {
            CFDictionaryRef parameters = (CFDictionaryRef) keyData;

            CFTypeRef ksize = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);
            CFIndex keyLengthInBits = getIntValue(ksize);

            if (keyLengthInBits < 512 || keyLengthInBits > kMaximumRSAKeyBits) {
                secwarning("Invalid or missing key size in: %@", parameters);
                result = errSecKeySizeNotAllowed;
                goto errOut;
            }

            size_n = ccn_nof(keyLengthInBits);

            key->key = calloc(1, ccrsa_full_ctx_size(ccn_sizeof_n(size_n)));
            require_action_quiet(key->key, errOut, result = errSecAllocate);

            fullkey = key->key;
            ccrsa_ctx_n(fullkey) = size_n;

            /* TODO: Add support for kSecPublicExponent parameter. */
            static uint8_t e[] = { 0x01, 0x00, 0x01 }; // Default is 65537
            if (!ccrsa_generate_fips186_key(keyLengthInBits, fullkey, sizeof(e), e, ccrng_seckey,ccrng_seckey))
                result = errSecSuccess;
            break;
        }
        default:
            break;
    }
errOut:
    return result;
}

static CFTypeRef SecRSAPrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                     CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                                     CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = kCFNull;

    ccrsa_full_ctx_t fullkey = key->key;
    int ccerr = 0;
    switch (operation) {
        case kSecKeyOperationTypeSign:
            if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureRawCCUnit)) {
                if (mode == kSecKeyOperationModePerform) {
                    // Input buffer length must be cc_unit aligned, otherwise it is not a valid cc_unit buffer.
                    CFIndex bufferSize = CFDataGetLength(in1);
                    require_action_quiet(bufferSize == ccn_sizeof_size(ccrsa_block_size(ccrsa_ctx_public(fullkey))), out,
                                         (result = NULL,
                                          SecError(errSecParam, error, CFSTR("%@: sign - input buffer bad size (%d bytes)"), key,
                                                   (int)bufferSize)));

                    // Verify that data is smaller than modulus.  Note that since we already verified that input algorithm
                    // is kSecKeyAlgorithmRSASignatureRawCCUnit, we can safely access in1 CFDataRef contents as cc_unit *.
                    require_action_quiet(ccn_cmpn(ccn_nof_size(CFDataGetLength(in1)), (const cc_unit *)CFDataGetBytePtr(in1),
                                                  ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey)) < 0, out,
                                         (result = NULL,
                                          SecError(errSecParam, error, CFSTR("%@: sign - digest too big (%d bytes)"), key,
                                                   (int)CFDataGetLength(in1))));

                    // Encrypt buffer and write it to output data.
                    result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, bufferSize);
                    ccerr = ccrsa_priv_crypt(fullkey, (cc_unit *)CFDataGetMutableBytePtr((CFMutableDataRef)result),
                                             (const cc_unit *)CFDataGetBytePtr(in1));
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        case kSecKeyOperationTypeDecrypt:
            if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRawCCUnit)) {
                if (mode == kSecKeyOperationModePerform) {
                    // Input buffer length must be cc_unit aligned, otherwise it is not a valid cc_unit buffer.
                    CFIndex bufferSize = CFDataGetLength(in1);
                    require_action_quiet(bufferSize == ccn_sizeof_size(ccrsa_block_size(ccrsa_ctx_public(fullkey))), out,
                                         (result = NULL,
                                          SecError(errSecParam, error, CFSTR("%@: sign - input buffer bad size (%d bytes)"), key,
                                                   (int)bufferSize)));

                    // Decrypt buffer and write it to output data.
                    result = CFDataCreateMutableWithScratch(NULL, bufferSize);
                    ccerr = ccrsa_priv_crypt(fullkey, (cc_unit *)CFDataGetMutableBytePtr((CFMutableDataRef)result),
                                             (const cc_unit *)CFDataGetBytePtr(in1));
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        default:
            break;
    }

    require_noerr_action_quiet(ccerr, out, (CFReleaseNull(result),
                                            SecError(errSecParam, error, CFSTR("rsa_priv_crypt failed, ccerr=%d"), ccerr)));
out:
    return result;
}

static size_t SecRSAPrivateKeyBlockSize(SecKeyRef key) {
    ccrsa_full_ctx_t fullkey = key->key;

    return ccn_write_uint_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey));
}

static CFDataRef SecRSAPrivateKeyCreatePKCS1(CFAllocatorRef allocator, ccrsa_full_ctx_t fullkey)
{
    const size_t result_size = ccrsa_export_priv_size(fullkey);

    CFMutableDataRef pkcs1 = CFDataCreateMutable(allocator, result_size);

    if (pkcs1 == NULL) {
        return NULL;
    }

    CFDataSetLength(pkcs1, result_size);

    uint8_t *bytes = CFDataGetMutableBytePtr(pkcs1);

    if (ccrsa_export_priv(fullkey,result_size,bytes)!=0) {
        /* Decoding failed */
        CFReleaseNull(pkcs1);
        return NULL;
    }

    return pkcs1;
}

static CFDataRef SecRSAPrivateKeyCopyPKCS1(SecKeyRef key)
{
    ccrsa_full_ctx_t fullkey = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    return SecRSAPrivateKeyCreatePKCS1(allocator, fullkey);
}

static OSStatus SecRSAPrivateKeyCopyPublicSerialization(SecKeyRef key, CFDataRef* serialized)
{
    ccrsa_full_ctx_t fullkey = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serialized = SecRSAPublicKeyCreatePKCS1(allocator, ccrsa_ctx_public(fullkey));

    if (NULL == *serialized)
        return errSecDecode;
    else
        return errSecSuccess;
}


static CFDictionaryRef SecRSAPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
	CFDictionaryRef dict = NULL;
	CFDataRef fullKeyBlob = NULL;

	/* PKCS1 encode the key pair. */
	fullKeyBlob = SecRSAPrivateKeyCopyPKCS1(key);
    require_quiet(fullKeyBlob, errOut);

	dict = SecKeyGeneratePrivateAttributeDictionary(key, kSecAttrKeyTypeRSA, fullKeyBlob);
    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(mutableDict, kSecAttrCanDerive, kCFBooleanFalse);
    CFAssignRetained(dict, mutableDict);

errOut:
	CFReleaseSafe(fullKeyBlob);

	return dict;
}

static CFDataRef SecRSAPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    return SecRSAPrivateKeyCopyPKCS1(key);
}

static CFStringRef SecRSAPrivateKeyCopyDescription(SecKeyRef key){

	return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);

}

SecKeyDescriptor kSecRSAPrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "RSAPrivateKey",

    .init = SecRSAPrivateKeyInit,
    .destroy = SecRSAPrivateKeyDestroy,
    .blockSize = SecRSAPrivateKeyBlockSize,
    .copyExternalRepresentation = SecRSAPrivateKeyCopyExternalRepresentation,
    .copyDictionary = SecRSAPrivateKeyCopyAttributeDictionary,
    .describe = SecRSAPrivateKeyCopyDescription,
    .copyPublic = SecRSAPrivateKeyCopyPublicSerialization,
    .copyOperationResult = SecRSAPrivateKeyCopyOperationResult,
};

/* Private Key API functions. */
SecKeyRef SecKeyCreateRSAPrivateKey(CFAllocatorRef allocator,
                                    const uint8_t *keyData, CFIndex keyDataLength,
                                    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecRSAPrivateKeyDescriptor, keyData,
                        keyDataLength, encoding);
}


OSStatus SecRSAKeyGeneratePair(CFDictionaryRef parameters,
                               SecKeyRef *rsaPublicKey, SecKeyRef *rsaPrivateKey) {
    OSStatus status = errSecParam;

    CFAllocatorRef allocator = NULL; /* @@@ get from parameters. */

    SecKeyRef pubKey = NULL;
    SecKeyRef privKey = SecKeyCreate(allocator, &kSecRSAPrivateKeyDescriptor,
                                     (const void*) parameters, 0, kSecGenerateKey);

    require_quiet(privKey, errOut);

	/* Create SecKeyRef's from the pkcs1 encoded keys. */
    pubKey = SecKeyCreate(allocator, &kSecRSAPublicKeyDescriptor,
                          privKey->key, 0, kSecExtractPublicFromPrivate);

    require_quiet(pubKey, errOut);

    if (rsaPublicKey) {
        *rsaPublicKey = pubKey;
        pubKey = NULL;
    }
    if (rsaPrivateKey) {
        *rsaPrivateKey = privKey;
        privKey = NULL;
    }

    status = errSecSuccess;

errOut:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);

	return status;
}
