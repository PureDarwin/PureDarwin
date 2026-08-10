/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * SecKeyAdaptors.m - Implementation of assorted algorithm adaptors for SecKey.
 * Algorithm adaptor is able to perform some transformation on provided input and calculated results and invoke
 * underlying operation with different algorithm. Typical adaptors are message->digest or unpadded->padded.
 * To invoke underlying operation, add algorithm to the context algorithm array and invoke SecKeyRunAlgorithmAndCopyResult().
 */

#import <Foundation/Foundation.h>

#include <Security/SecBase.h>
#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCFAllocator.h>

#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecBuffer.h>

#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccrsa_priv.h>
#include <corecrypto/ccansikdf.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccder.h>


#pragma mark Algorithm constants value definitions

const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureRaw = CFSTR("algid:sign:RSA:raw");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureRawCCUnit = CFSTR("algid:sign:RSA:raw-cc");

const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw = CFSTR("algid:sign:RSA:digest-PKCS1v15");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5 = CFSTR("algid:sign:RSA:digest-PKCS1v15:MD5");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA512");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPSSSHA1 = CFSTR("algid:sign:RSA:digest-PSS:SHA1:SHA1:20");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPSSSHA224 = CFSTR("algid:sign:RSA:digest-PSS:SHA224:SHA224:24");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPSSSHA256 = CFSTR("algid:sign:RSA:digest-PSS:SHA256:SHA256:32");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPSSSHA384 = CFSTR("algid:sign:RSA:digest-PSS:SHA384:SHA384:48");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPSSSHA512 = CFSTR("algid:sign:RSA:digest-PSS:SHA512:SHA512:64");

const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5 = CFSTR("algid:sign:RSA:message-PKCS1v15:MD5");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA512");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePSSSHA1 = CFSTR("algid:sign:RSA:message-PSS:SHA1:SHA1:20");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePSSSHA224 = CFSTR("algid:sign:RSA:message-PSS:SHA224:SHA224:24");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePSSSHA256 = CFSTR("algid:sign:RSA:message-PSS:SHA256:SHA256:32");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePSSSHA384 = CFSTR("algid:sign:RSA:message-PSS:SHA384:SHA384:48");
const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePSSSHA512 = CFSTR("algid:sign:RSA:message-PSS:SHA512:SHA512:64");

const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureRFC4754 = CFSTR("algid:sign:ECDSA:RFC4754");

const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962 = CFSTR("algid:sign:ECDSA:digest-X962");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962SHA1 = CFSTR("algid:sign:ECDSA:digest-X962:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962SHA224 = CFSTR("algid:sign:ECDSA:digest-X962:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962SHA256 = CFSTR("algid:sign:ECDSA:digest-X962:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962SHA384 = CFSTR("algid:sign:ECDSA:digest-X962:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureDigestX962SHA512 = CFSTR("algid:sign:ECDSA:digest-X962:SHA512");

const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA1 = CFSTR("algid:sign:ECDSA:message-X962:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA224 = CFSTR("algid:sign:ECDSA:message-X962:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA256 = CFSTR("algid:sign:ECDSA:message-X962:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA384 = CFSTR("algid:sign:ECDSA:message-X962:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA512 = CFSTR("algid:sign:ECDSA:message-X962:SHA512");

const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionRaw = CFSTR("algid:encrypt:RSA:raw");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionRawCCUnit = CFSTR("algid:encrypt:RSA:raw-cc");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionPKCS1 = CFSTR("algid:encrypt:RSA:PKCS1");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA1 = CFSTR("algid:encrypt:RSA:OAEP:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA224 = CFSTR("algid:encrypt:RSA:OAEP:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA256 = CFSTR("algid:encrypt:RSA:OAEP:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA384 = CFSTR("algid:encrypt:RSA:OAEP:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA512 = CFSTR("algid:encrypt:RSA:OAEP:SHA512");

const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM = CFSTR("algid:encrypt:RSA:OAEP:SHA1:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM = CFSTR("algid:encrypt:RSA:OAEP:SHA224:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM = CFSTR("algid:encrypt:RSA:OAEP:SHA256:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM = CFSTR("algid:encrypt:RSA:OAEP:SHA384:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM = CFSTR("algid:encrypt:RSA:OAEP:SHA512:AESGCM");

const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA1:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA224:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA256:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA384:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA512:AESGCM");

const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA1:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA224:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA256:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA384:AESGCM");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA512:AESGCM");

const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA224:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA256:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA384:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM = CFSTR("algid:encrypt:ECIES:ECDH:KDFX963:SHA512:AESGCM-KDFIV");

const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA224AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA224:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA256:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA384AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA384:AESGCM-KDFIV");
const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA512AESGCM = CFSTR("algid:encrypt:ECIES:ECDHC:KDFX963:SHA512:AESGCM-KDFIV");

const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandard = CFSTR("algid:keyexchange:ECDH");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1 = CFSTR("algid:keyexchange:ECDH:KDFX963:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA224 = CFSTR("algid:keyexchange:ECDH:KDFX963:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA256 = CFSTR("algid:keyexchange:ECDH:KDFX963:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA384 = CFSTR("algid:keyexchange:ECDH:KDFX963:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA512 = CFSTR("algid:keyexchange:ECDH:KDFX963:SHA512");

const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactor = CFSTR("algid:keyexchange:ECDHC");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA1 = CFSTR("algid:keyexchange:ECDHC:KDFX963:SHA1");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA224 = CFSTR("algid:keyexchange:ECDHC:KDFX963:SHA224");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA256 = CFSTR("algid:keyexchange:ECDHC:KDFX963:SHA256");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA384 = CFSTR("algid:keyexchange:ECDHC:KDFX963:SHA384");
const SecKeyAlgorithm kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA512 = CFSTR("algid:keyexchange:ECDHC:KDFX963:SHA512");

const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionAKSSmartCard = CFSTR("algid:encrypt:ECIES:ECDH:SHA256:2PubKeys");

void SecKeyOperationContextDestroy(SecKeyOperationContext *context) {
    CFReleaseNull(context->algorithm);
}

static void PerformWithCFDataBuffer(CFIndex size, void (^operation)(uint8_t *buffer, CFDataRef data)) {
    PerformWithBuffer(size, ^(size_t size, uint8_t *buffer) {
        CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)buffer, size, kCFAllocatorNull);
        operation(buffer, data);
        CFRelease(data);
    });
}

static CFDataRef SecKeyCopyDigestForMessage(SecKeyOperationContext *context, CFDataRef message, CFDataRef in2,
                                            const struct ccdigest_info *di, CFErrorRef *error) {
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    __block CFTypeRef result;
    PerformWithCFDataBuffer(di->output_size, ^(uint8_t *buffer, CFDataRef data) {
        ccdigest(di, CFDataGetLength(message), CFDataGetBytePtr(message), buffer);
        result = SecKeyRunAlgorithmAndCopyResult(context, data, in2, error);
    });
    return result;
}

static CFTypeRef SecKeyCopyECDSASignatureForDigest(SecKeyOperationContext *context, CFDataRef digest, CFDataRef in2,
                                                   SecKeyAlgorithm algorithm, const struct ccdigest_info *di, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmECDSASignatureDigestX962);
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    if (CFDataGetLength(digest) != (CFIndex)di->output_size) {
        SecError(errSecParam, error, CFSTR("bad digest size for signing with algorithm %@"), algorithm);
        return NULL;
    }

    return SecKeyRunAlgorithmAndCopyResult(context, digest, in2, error);
}

#define DIGEST_RSA_ADAPTORS(name, di) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessage ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSASignatureDigest ## name); \
    return SecKeyCopyDigestForMessage(context, in1, in2, di, error); \
}

#define DIGEST_ECDSA_ADAPTORS(name, di) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessage ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmECDSASignatureDigest ## name); \
    return SecKeyCopyDigestForMessage(context, in1, in2, di, error); \
} \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigest ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyCopyECDSASignatureForDigest(context, in1, in2, kSecKeyAlgorithmECDSASignatureDigest ## name, di, error); \
}

DIGEST_RSA_ADAPTORS(PKCS1v15SHA1, ccsha1_di())
DIGEST_RSA_ADAPTORS(PKCS1v15SHA224, ccsha224_di())
DIGEST_RSA_ADAPTORS(PKCS1v15SHA256, ccsha256_di())
DIGEST_RSA_ADAPTORS(PKCS1v15SHA384, ccsha384_di())
DIGEST_RSA_ADAPTORS(PKCS1v15SHA512, ccsha512_di())
DIGEST_RSA_ADAPTORS(PKCS1v15MD5, ccmd5_di())
DIGEST_RSA_ADAPTORS(PSSSHA1, ccsha1_di())
DIGEST_RSA_ADAPTORS(PSSSHA224, ccsha224_di())
DIGEST_RSA_ADAPTORS(PSSSHA256, ccsha256_di())
DIGEST_RSA_ADAPTORS(PSSSHA384, ccsha384_di())
DIGEST_RSA_ADAPTORS(PSSSHA512, ccsha512_di())
DIGEST_ECDSA_ADAPTORS(X962SHA1, ccsha1_di())
DIGEST_ECDSA_ADAPTORS(X962SHA224, ccsha224_di())
DIGEST_ECDSA_ADAPTORS(X962SHA256, ccsha256_di())
DIGEST_ECDSA_ADAPTORS(X962SHA384, ccsha384_di())
DIGEST_ECDSA_ADAPTORS(X962SHA512, ccsha512_di())

#undef DIGEST_RSA_ADAPTORS
#undef DIGEST_ECDSA_ADAPTORS

typedef CF_ENUM(CFIndex, SecKeyECSignatureType) {
    kSecKeyECSignatureTypeRFC4754,
    kSecKeyECSignatureTypeX962,
};

static CFDataRef SecKeyCopyConvertedECDSASignature(SecKeyOperationContext *context, SecKeyECSignatureType targetType, CFDataRef sourceSignature, OSStatus errorStatus, CFErrorRef *error) {
    CFMutableDataRef targetSignature = NULL;
    CFIndex keySize = SecKeyGetBlockSize(context->key);
    cc_size ccn = ccn_nof_size(keySize);
    cc_unit ccr[ccn], ccs[ccn];
    const uint8_t *data = CFDataGetBytePtr(sourceSignature);
    if (targetType == kSecKeyECSignatureTypeRFC4754) {
        // Extract ASN.1 DER signature and create raw r-s big-endian encoded signature.
        const uint8_t *der_end = data + CFDataGetLength(sourceSignature);
        if (ccder_decode_seqii(ccn, ccr, ccs, data, der_end) == der_end) {
            targetSignature = CFDataCreateMutableWithScratch(kCFAllocatorDefault, keySize * 2);
            uint8_t *target = CFDataGetMutableBytePtr(targetSignature);
            ccn_write_uint_padded(ccn, ccr, keySize, target);
            ccn_write_uint_padded(ccn, ccs, keySize, target + keySize);
        } else {
            SecError(errorStatus, error, CFSTR("Wrong ECDSA X962 signature"));
        }
    } else {
        // Extract raw r-s big-endian encoded signature and encode ASN.1 DER signature.
        if (CFDataGetLength(sourceSignature) == 2 * (CFIndex)keySize &&
            ccn_read_uint(ccn, ccr, keySize, data) == 0 && ccn_read_uint(ccn, ccs, keySize, data + keySize) == 0) {
            size_t s_len = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                                        ccder_sizeof_integer(ccn, ccr) +
                                        ccder_sizeof_integer(ccn, ccs));
            targetSignature = CFDataCreateMutableWithScratch(kCFAllocatorDefault, s_len);
            uint8_t *der = CFDataGetMutableBytePtr(targetSignature);
            uint8_t *der_end = der + s_len;
            if (ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                            ccder_encode_integer(ccn, ccr, der,
                                                                 ccder_encode_integer(ccn, ccs, der, der_end))) == NULL) {
                CFReleaseNull(targetSignature);
                SecError(errSecInternal, error, CFSTR("Failed to encode X962 signature"));
            }
        } else {
            SecError(errorStatus, error, CFSTR("Wrong ECDSA RFC4754 signature"));
        }
    }

    return targetSignature;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureConvert(SecKeyOperationContext *context,
                                                                             SecKeyECSignatureType signatureType,
                                                                             CFTypeRef digest, CFTypeRef in2, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, signatureType == kSecKeyECSignatureTypeRFC4754 ? kSecKeyAlgorithmECDSASignatureDigestX962 : kSecKeyAlgorithmECDSASignatureRFC4754);
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    CFDataRef signature = SecKeyRunAlgorithmAndCopyResult(context, digest, in2, error);
    if (signature == NULL || CFEqual(signature, kCFNull)) {
        return signature;
    }

    // Convert the signature.
    CFAssignRetained(signature, SecKeyCopyConvertedECDSASignature(context, signatureType, signature, errSecParam, error));
    return signature;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureConvert(SecKeyOperationContext *context,
                                                                               SecKeyECSignatureType signatureType,
                                                                               CFTypeRef digest, CFTypeRef signature, CFErrorRef *error) {
    SecKeyECSignatureType targetSignatureType = (signatureType == kSecKeyECSignatureTypeRFC4754) ? kSecKeyECSignatureTypeX962 : kSecKeyECSignatureTypeRFC4754;
    CFArrayAppendValue(context->algorithm, targetSignatureType == kSecKeyECSignatureTypeRFC4754 ? kSecKeyAlgorithmECDSASignatureRFC4754 : kSecKeyAlgorithmECDSASignatureDigestX962);
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    CFDataRef convertedSignature = SecKeyCopyConvertedECDSASignature(context, targetSignatureType, signature, errSecVerifyFailed, error);
    CFTypeRef result = NULL;
    if (convertedSignature != NULL) {
        result = SecKeyRunAlgorithmAndCopyResult(context, digest, convertedSignature, error);
        CFReleaseNull(convertedSignature);
    }
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureRFC4754(SecKeyOperationContext *context,
                                                                             CFTypeRef digest, CFTypeRef signature, CFErrorRef *error) {
    return SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureConvert(context, kSecKeyECSignatureTypeRFC4754, digest, signature, error);
}
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureDigestX962(SecKeyOperationContext *context,
                                                                                CFTypeRef digest, CFTypeRef signature, CFErrorRef *error) {
    return SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureConvert(context, kSecKeyECSignatureTypeX962, digest, signature, error);
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureRFC4754(SecKeyOperationContext *context,
                                                                               CFTypeRef digest, CFTypeRef signature, CFErrorRef *error) {
    return SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureConvert(context, kSecKeyECSignatureTypeRFC4754, digest, signature, error);
}
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureDigestX962(SecKeyOperationContext *context,
                                                                                  CFTypeRef digest, CFTypeRef signature, CFErrorRef *error) {
    return SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureConvert(context, kSecKeyECSignatureTypeX962, digest, signature, error);
}

static CFDataRef SecKeyRSACopyBigEndianToCCUnit(CFDataRef bigEndian, size_t size) {
    CFMutableDataRef result = NULL;
    if (bigEndian != NULL) {
        size_t dataSize = CFDataGetLength(bigEndian);
        if (dataSize > size) {
            size = dataSize;
        }
        result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, ccn_sizeof_size(size));
        ccn_read_uint(ccn_nof_size(size), (cc_unit *)CFDataGetMutableBytePtr(result), dataSize, CFDataGetBytePtr(bigEndian));
    }
    return result;
}

static void PerformWithBigEndianToCCUnit(CFDataRef bigEndian, size_t size, void (^operation)(CFDataRef ccunits)) {
    if (bigEndian == NULL) {
        return operation(NULL);
    }
    size_t dataSize = CFDataGetLength(bigEndian);
    if (dataSize > size) {
        size = dataSize;
    }
    PerformWithCFDataBuffer(ccn_sizeof_size(size), ^(uint8_t *buffer, CFDataRef data) {
        ccn_read_uint(ccn_nof_size(size), (cc_unit *)buffer, dataSize, CFDataGetBytePtr(bigEndian));
        operation(data);
    });
}

static CFDataRef SecKeyRSACopyCCUnitToBigEndian(CFDataRef ccunits, size_t size) {
    CFMutableDataRef result = NULL;
    if (ccunits != NULL) {
        cc_size n = ccn_nof_size(CFDataGetLength(ccunits));
        const cc_unit *s = (const cc_unit *)CFDataGetBytePtr(ccunits);
        result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, size);
        ccn_write_uint_padded(n, s, CFDataGetLength(result), CFDataGetMutableBytePtr(result));
    }
    return result;
}

static void PerformWithCCUnitToBigEndian(CFDataRef ccunits, size_t size, void (^operation)(CFDataRef bigEndian)) {
    if (ccunits == NULL) {
        return operation(NULL);
    }
    PerformWithCFDataBuffer(size, ^(uint8_t *buffer, CFDataRef data) {
        cc_size n = ccn_nof_size(CFDataGetLength(ccunits));
        const cc_unit *s = (const cc_unit *)CFDataGetBytePtr(ccunits);
        ccn_write_uint_padded(n, s, size, buffer);
        operation(data);
    });
}

static CFTypeRef SecKeyRSACopyEMSASignature(SecKeyOperationContext *context,
                                            CFDataRef in1, CFDataRef in2, CFErrorRef *error, bool pss, const struct ccdigest_info *di) {
    CFDictionaryRef parameters = NULL;
    __block CFTypeRef result = NULL;

    require_action_quiet(parameters = SecKeyCopyAttributes(context->key), out,
                         SecError(errSecParam, error, CFSTR("Unable to export key parameters")));
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeRSA), out, result = kCFNull);
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyClass), kSecAttrKeyClassPrivate), out, result = kCFNull);
    CFReleaseNull(parameters);

    if (pss) {
        // Verify that algorithm is compatible with the modulus size.
        size_t blockSize = SecKeyGetBlockSize(context->key);
        require_action_quiet(blockSize >= di->output_size * 2 + 2, out,
                             SecError(errSecParam, error, CFSTR("algorithm %@ incompatible with %lubit RSA key"),
                                      CFArrayGetValueAtIndex(context->algorithm, CFArrayGetCount(context->algorithm) - 1),
                                      blockSize * 8));
    }

    if (!pss && di != NULL) {
        CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw);
    }

    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSASignatureRawCCUnit);
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    size_t size = SecKeyGetBlockSize(context->key);
    if (size == 0) {
        SecError(errSecParam, error, CFSTR("expecting RSA key"));
        return NULL;
    }
    PerformWithCFDataBuffer(ccn_sizeof_size(size), ^(uint8_t *buffer, CFDataRef data) {
        NSMutableData *s = [NSMutableData dataWithLength:size];
        require_action_quiet(s != nil, out, SecError(errSecAllocate, error, CFSTR("out of memory")));
        if (pss) {
            NSMutableData *salt = [NSMutableData dataWithLength:di->output_size];
            require_action_quiet(salt != nil, out, SecError(errSecAllocate, error, CFSTR("out of memory")));
            int err = ccrng_generate(ccrng_seckey, di->output_size, salt.mutableBytes);
            require_noerr_action_quiet(err, out, SecError(errSecInternal, error, CFSTR("PSS salt gen fail (%zu bytes), err %d"),
                                                          di->output_size, err));
            err = ccrsa_emsa_pss_encode(di, di, di->output_size, salt.bytes,
                                        CFDataGetLength(in1), CFDataGetBytePtr(in1), size * 8 - 1, s.mutableBytes);
            require_noerr_action_quiet(err, out, SecError(errSecParam, error, CFSTR("RSASSA-PSS incompatible algorithm for key size")));
        } else {
            int err = ccrsa_emsa_pkcs1v15_encode(size, s.mutableBytes, CFDataGetLength(in1), CFDataGetBytePtr(in1), di ? di->oid : NULL);
            require_noerr_action_quiet(err, out, SecError(errSecParam, error, CFSTR("RSAsign wrong input data length")));
        }
        ccn_read_uint(ccn_nof_size(size), (cc_unit *)buffer, size, s.bytes);
        require_quiet(result = SecKeyRunAlgorithmAndCopyResult(context, data, NULL, error), out);
        CFAssignRetained(result, SecKeyRSACopyCCUnitToBigEndian(result, SecKeyGetBlockSize(context->key)));
    out:;
    });

out:
    CFReleaseSafe(parameters);
    return result;
}

#define RSA_EMSA_SIGN_ADAPTOR(name, pss, di) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigest ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSACopyEMSASignature(context, in1, in2, error, pss, di); \
}

RSA_EMSA_SIGN_ADAPTOR(PKCS1v15SHA1, false, ccsha1_di())
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15SHA224, false, ccsha224_di())
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15SHA256, false, ccsha256_di())
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15SHA384, false, ccsha384_di())
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15SHA512, false, ccsha512_di())
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15Raw, false, NULL)
RSA_EMSA_SIGN_ADAPTOR(PKCS1v15MD5, false, ccmd5_di())
RSA_EMSA_SIGN_ADAPTOR(PSSSHA1, true, ccsha1_di())
RSA_EMSA_SIGN_ADAPTOR(PSSSHA224, true, ccsha224_di())
RSA_EMSA_SIGN_ADAPTOR(PSSSHA256, true, ccsha256_di())
RSA_EMSA_SIGN_ADAPTOR(PSSSHA384, true, ccsha384_di())
RSA_EMSA_SIGN_ADAPTOR(PSSSHA512, true, ccsha512_di())

#undef RSA_EMSA_SIGN_ADAPTOR

static CFTypeRef SecKeyAlgorithmAdaptorCopyBigEndianToCCUnit(SecKeyOperationContext *context,
                                                             CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    __block CFTypeRef result = NULL;
    PerformWithBigEndianToCCUnit(in1, SecKeyGetBlockSize(context->key), ^(CFDataRef ccunits) {
        result = SecKeyRunAlgorithmAndCopyResult(context, ccunits, in2, error);
        if (result != NULL) {
            CFAssignRetained(result, SecKeyRSACopyCCUnitToBigEndian(result, SecKeyGetBlockSize(context->key)));
        }
    });
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyCCUnitToBigEndian(SecKeyOperationContext *context,
                                                             CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    __block CFTypeRef result = NULL;
    PerformWithCCUnitToBigEndian(in1, SecKeyGetBlockSize(context->key), ^(CFDataRef bigEndian) {
        result = SecKeyRunAlgorithmAndCopyResult(context, bigEndian, in2, error);
        if (result != NULL) {
            CFAssignRetained(result, SecKeyRSACopyBigEndianToCCUnit(result, SecKeyGetBlockSize(context->key)));
        }
    });
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureRaw(SecKeyOperationContext *context,
                                                                             CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSASignatureRawCCUnit);
    return SecKeyAlgorithmAdaptorCopyBigEndianToCCUnit(context, in1, in2, error);
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureRawCCUnit(SecKeyOperationContext *context,
                                                                                   CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSASignatureRaw);
    return SecKeyAlgorithmAdaptorCopyCCUnitToBigEndian(context, in1, in2, error);
}

static bool SecKeyVerifyBadSignature(CFErrorRef *error) {
    return SecError(errSecVerifyFailed, error, CFSTR("RSA signature verification failed, no match"));
}

static CFTypeRef SecKeyRSAVerifyAdaptorCopyResult(SecKeyOperationContext *context, CFTypeRef signature, CFErrorRef *error,
                                                  Boolean (^verifyBlock)(CFDataRef decrypted)) {
    CFTypeRef result = NULL;
    context->operation = kSecKeyOperationTypeDecrypt;
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSAEncryptionRaw);
    result = SecKeyRunAlgorithmAndCopyResult(context, signature, NULL, error);
    if (context->mode == kSecKeyOperationModePerform && result != NULL) {
        if (verifyBlock(result)) {
            CFRetainAssign(result, kCFBooleanTrue);
        } else {
            CFRetainAssign(result, kCFBooleanFalse);
            SecKeyVerifyBadSignature(error);
        }
    }
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureRaw(SecKeyOperationContext *context,
                                                                         CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    return SecKeyRSAVerifyAdaptorCopyResult(context, in2, error, ^Boolean(CFDataRef decrypted) {
        // Skip zero-padding from the beginning of the decrypted signature.
        const UInt8 *data = CFDataGetBytePtr(decrypted);
        CFIndex length = CFDataGetLength(decrypted);
        while (*data == 0x00 && length > 0) {
            data++;
            length--;
        }
        // The rest of the decrypted signature must be the same as input data.
        return length == CFDataGetLength(in1) && memcmp(CFDataGetBytePtr(in1), data, length) == 0;
    });
};

#define PKCS1v15_EMSA_VERIFY_ADAPTOR(name, oid) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15 ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSAVerifyAdaptorCopyResult(context, in2, error, ^Boolean(CFDataRef decrypted) { \
        return ccrsa_emsa_pkcs1v15_verify(CFDataGetLength(decrypted), \
                                          (uint8_t *)CFDataGetBytePtr(decrypted), \
                                          CFDataGetLength(in1), CFDataGetBytePtr(in1), oid) == 0; \
    }); \
}

#define PSS_EMSA_VERIFY_ADAPTOR(name, di) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSS ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSAVerifyAdaptorCopyResult(context, in2, error, ^Boolean(CFDataRef decrypted) { \
        return ccrsa_emsa_pss_decode(di, di, di->output_size, CFDataGetLength(in1), CFDataGetBytePtr(in1), \
                                     CFDataGetLength(decrypted) * 8 - 1, (uint8_t *)CFDataGetBytePtr(decrypted)) == 0; \
    }); \
}

PKCS1v15_EMSA_VERIFY_ADAPTOR(SHA1, ccsha1_di()->oid)
PKCS1v15_EMSA_VERIFY_ADAPTOR(SHA224, ccsha224_di()->oid)
PKCS1v15_EMSA_VERIFY_ADAPTOR(SHA256, ccsha256_di()->oid)
PKCS1v15_EMSA_VERIFY_ADAPTOR(SHA384, ccsha384_di()->oid)
PKCS1v15_EMSA_VERIFY_ADAPTOR(SHA512, ccsha512_di()->oid)
PKCS1v15_EMSA_VERIFY_ADAPTOR(Raw, NULL)
PKCS1v15_EMSA_VERIFY_ADAPTOR(MD5, ccmd5_di()->oid)

PSS_EMSA_VERIFY_ADAPTOR(SHA1, ccsha1_di())
PSS_EMSA_VERIFY_ADAPTOR(SHA224, ccsha224_di())
PSS_EMSA_VERIFY_ADAPTOR(SHA256, ccsha256_di())
PSS_EMSA_VERIFY_ADAPTOR(SHA384, ccsha384_di())
PSS_EMSA_VERIFY_ADAPTOR(SHA512, ccsha512_di())

#undef PKCS1v15_EMSA_VERIFY_ADAPTOR
#undef PSS_EMSA_VERIFY_ADAPTOR

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRaw(SecKeyOperationContext *context,
                                                                                  CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSAEncryptionRawCCUnit);
    return SecKeyAlgorithmAdaptorCopyBigEndianToCCUnit(context, in1, in2, error);
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRawCCUnit(SecKeyOperationContext *context,
                                                                                        CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSAEncryptionRaw);
    return SecKeyAlgorithmAdaptorCopyCCUnitToBigEndian(context, in1, in2, error);
}

static CFTypeRef SecKeyRSACopyEncryptedWithPadding(SecKeyOperationContext *context, const struct ccdigest_info *di,
                                                   CFDataRef in1, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSAEncryptionRawCCUnit);
    size_t size = SecKeyGetBlockSize(context->key);
    size_t minSize = (di != NULL) ? di->output_size * 2 + 2 : 11;
    if (size < minSize) {
        return kCFNull;
    }
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    __block CFTypeRef result = NULL;
    PerformWithCFDataBuffer(ccn_sizeof_size(size), ^(uint8_t *buffer, CFDataRef data) {
        int err;
        if (di != NULL) {
            err = ccrsa_oaep_encode(di, ccrng_seckey, size, (cc_unit *)buffer,
                                    CFDataGetLength(in1), CFDataGetBytePtr(in1));
        } else {
            err = ccrsa_eme_pkcs1v15_encode(ccrng_seckey, size, (cc_unit *)buffer,
                                            CFDataGetLength(in1), CFDataGetBytePtr(in1));
        }
        require_noerr_action_quiet(err, out, SecError(errSecParam, error,
                                                      CFSTR("RSAencrypt wrong input size (err %d)"), err));
        cc_clear(ccn_sizeof_size(size) - size, buffer + size);
        require_quiet(result = SecKeyRunAlgorithmAndCopyResult(context, data, NULL, error), out);
        CFAssignRetained(result, SecKeyRSACopyCCUnitToBigEndian(result, SecKeyGetBlockSize(context->key)));
    out:;
    });
    return result;
}

static CFTypeRef SecKeyRSACopyDecryptedWithPadding(SecKeyOperationContext *context, const struct ccdigest_info *di,
                                                   CFDataRef in1, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmRSAEncryptionRawCCUnit);
    size_t minSize = (di != NULL) ? di->output_size * 2 + 2 : 11;
    if (SecKeyGetBlockSize(context->key) < minSize) {
        return kCFNull;
    }
    if (context->mode == kSecKeyOperationModeCheckIfSupported) {
        return SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error);
    }

    __block CFMutableDataRef result = NULL;
    PerformWithBigEndianToCCUnit(in1, SecKeyGetBlockSize(context->key), ^(CFDataRef ccunits) {
        CFDataRef cc_result = NULL;
        require_quiet(cc_result = SecKeyRunAlgorithmAndCopyResult(context, ccunits, NULL, error), out);
        size_t size = SecKeyGetBlockSize(context->key);
        result = CFDataCreateMutableWithScratch(NULL, size);
        int err;
        if (di != NULL) {
            err = ccrsa_oaep_decode(di, &size, CFDataGetMutableBytePtr(result),
                                    size, (cc_unit *)CFDataGetBytePtr(cc_result));
        } else {
            err = ccrsa_eme_pkcs1v15_decode(&size, CFDataGetMutableBytePtr(result),
                                            size, (cc_unit *)CFDataGetBytePtr(cc_result));
        }
        require_noerr_action_quiet(err, out, (CFReleaseNull(result),
                                              SecError(errSecParam, error, CFSTR("RSAdecrypt wrong input (err %d)"), err)));
        CFDataSetLength(result, size);
    out:
        CFReleaseSafe(cc_result);
    });
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionPKCS1(SecKeyOperationContext *context,
                                                                             CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    return SecKeyRSACopyEncryptedWithPadding(context, NULL, in1, error);
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionPKCS1(SecKeyOperationContext *context,
                                                                             CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    return SecKeyRSACopyDecryptedWithPadding(context, NULL, in1, error);
}

#define RSA_OAEP_CRYPT_ADAPTOR(name, di) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEP ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSACopyEncryptedWithPadding(context, di, in1, error); \
} \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEP ## name( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSACopyDecryptedWithPadding(context, di, in1, error); \
}

RSA_OAEP_CRYPT_ADAPTOR(SHA1, ccsha1_di());
RSA_OAEP_CRYPT_ADAPTOR(SHA224, ccsha224_di());
RSA_OAEP_CRYPT_ADAPTOR(SHA256, ccsha256_di());
RSA_OAEP_CRYPT_ADAPTOR(SHA384, ccsha384_di());
RSA_OAEP_CRYPT_ADAPTOR(SHA512, ccsha512_di());

#undef RSA_OAEP_CRYPT_ADAPTOR

const SecKeyKeyExchangeParameter kSecKeyKeyExchangeParameterRequestedSize = CFSTR("requestedSize");
const SecKeyKeyExchangeParameter kSecKeyKeyExchangeParameterSharedInfo = CFSTR("sharedInfo");

const CFStringRef kSecKeyEncryptionParameterSymmetricKeySizeInBits = CFSTR("symKeySize");
const CFStringRef kSecKeyEncryptionParameterSymmetricAAD = CFSTR("aad");
const CFStringRef kSecKeyEncryptionParameterRecryptParameters = CFSTR("recryptParams");
const CFStringRef kSecKeyEncryptionParameterRecryptCertificate = CFSTR("recryptCert");

static CFTypeRef SecKeyECDHCopyX963Result(SecKeyOperationContext *context, const struct ccdigest_info *di,
                                          CFTypeRef in1, CFTypeRef params, CFErrorRef *error) {
    CFTypeRef result = NULL, sharedSecret;
    require_quiet(sharedSecret = SecKeyRunAlgorithmAndCopyResult(context, in1, NULL, error), out);

    if (context->mode == kSecKeyOperationModePerform) {
        // Parse params.
        CFTypeRef value = NULL;
        CFIndex requestedSize = 0;
        require_action_quiet((value = CFDictionaryGetValue(params, kSecKeyKeyExchangeParameterRequestedSize)) != NULL
                             && CFGetTypeID(value) == CFNumberGetTypeID() &&
                             CFNumberGetValue(value, kCFNumberCFIndexType, &requestedSize), out,
                             SecError(errSecParam, error, CFSTR("kSecKeyKeyExchangeParameterRequestedSize is missing")));
        size_t sharedInfoLength = 0;
        const void *sharedInfo = NULL;
        if ((value = CFDictionaryGetValue(params, kSecKeyKeyExchangeParameterSharedInfo)) != NULL) {
            require_action_quiet(CFGetTypeID(value) == CFDataGetTypeID(), out, SecError(errSecParam, error, CFSTR("ECDHKeyExchange wrong sharedInfo type (must be CFData/NSData)")));
            sharedInfo = CFDataGetBytePtr(value);
            sharedInfoLength = CFDataGetLength(value);
        }

        CFMutableDataRef kdfResult = CFDataCreateMutableWithScratch(kCFAllocatorDefault, requestedSize);
        int err = ccansikdf_x963(di, CFDataGetLength(sharedSecret), CFDataGetBytePtr(sharedSecret), sharedInfoLength, sharedInfo,
                                 requestedSize, CFDataGetMutableBytePtr(kdfResult));
        require_noerr_action_quiet(err, out, (CFReleaseNull(kdfResult),
                                              SecError(errSecParam, error, CFSTR("ECDHKeyExchange wrong input (%d)"), err)));
        CFAssignRetained(result, kdfResult);
    } else {
        // In test-only mode, propagate result (YES/NO) of underlying operation.
        CFRetainAssign(result, sharedSecret);
    }
out:
    CFReleaseNull(sharedSecret);
    return result;
}

#define ECDH_X963_ADAPTOR(hashname, di, cofactor) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDH ## cofactor ## X963 ## hashname( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    CFArrayAppendValue(context->algorithm, kSecKeyAlgorithmECDHKeyExchange ## cofactor); \
    return SecKeyECDHCopyX963Result(context, di, in1, in2, error); \
}

ECDH_X963_ADAPTOR(SHA1, ccsha1_di(), Standard)
ECDH_X963_ADAPTOR(SHA224, ccsha224_di(), Standard)
ECDH_X963_ADAPTOR(SHA256, ccsha256_di(), Standard)
ECDH_X963_ADAPTOR(SHA384, ccsha384_di(), Standard)
ECDH_X963_ADAPTOR(SHA512, ccsha512_di(), Standard)
ECDH_X963_ADAPTOR(SHA1, ccsha1_di(), Cofactor)
ECDH_X963_ADAPTOR(SHA224, ccsha224_di(), Cofactor)
ECDH_X963_ADAPTOR(SHA256, ccsha256_di(), Cofactor)
ECDH_X963_ADAPTOR(SHA384, ccsha384_di(), Cofactor)
ECDH_X963_ADAPTOR(SHA512, ccsha512_di(), Cofactor)

#undef ECDH_X963_ADAPTOR

// Extract number value of either CFNumber or CFString.
static CFIndex SecKeyGetCFIndexFromRef(CFTypeRef ref) {
    CFIndex result = 0;
    if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue(ref, kCFNumberCFIndexType, &result)) {
            result = 0;
        }
    } else if (CFGetTypeID(ref) == CFStringGetTypeID()) {
        result = CFStringGetIntValue(ref);
    }
    return result;
}

typedef CFDataRef (*SecKeyECIESKeyExchangeCopyResult)(SecKeyOperationContext *context, SecKeyAlgorithm keyExchangeAlgorithm, bool encrypt, CFDataRef ephemeralPubKey, CFDataRef pubKey, bool variableIV, CFDictionaryRef inParams, CFErrorRef *error);
typedef Boolean (*SecKeyECIESEncryptCopyResult)(CFDataRef keyExchangeResult, CFDataRef inData, CFDictionaryRef inParams, CFMutableDataRef result, CFErrorRef *error);
typedef CFDataRef SecKeyECIESDecryptCopyResult(CFDataRef keyExchangeResult, CFDataRef inData, CFDictionaryRef inParams, CFErrorRef *error);

static CFTypeRef SecKeyECIESCopyEncryptedData(SecKeyOperationContext *context, SecKeyAlgorithm keyExchangeAlgorithm,
                                              SecKeyECIESKeyExchangeCopyResult keyExchangeCopyResult,
                                              SecKeyECIESEncryptCopyResult encryptCopyResult, bool variableIV,
                                              CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFDictionaryRef parameters = NULL;
    SecKeyRef ephemeralPrivateKey = NULL, ephemeralPublicKey = NULL;
    CFDataRef pubKeyData = NULL, ephemeralPubKeyData = NULL, keyExchangeResult = NULL;
    CFTypeRef result = NULL;
    SecKeyRef originalKey = context->key;
    CFMutableDataRef ciphertext = NULL;

    require_action_quiet(parameters = SecKeyCopyAttributes(context->key), out,
                         SecError(errSecParam, error, CFSTR("Unable to export key parameters")));
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandom), out, result = kCFNull);
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyClass), kSecAttrKeyClassPublic), out, result = kCFNull);

    // Generate ephemeral key.
    require_quiet(pubKeyData = SecKeyCopyExternalRepresentation(context->key, error), out);
    CFAssignRetained(parameters, CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
#if TARGET_OS_OSX
                                                              kSecUseDataProtectionKeychain, kCFBooleanTrue,
#endif
                                                              kSecAttrKeyType, CFDictionaryGetValue(parameters, kSecAttrKeyType),
                                                              kSecAttrKeySizeInBits, CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits),
                                                              NULL));
    require_quiet(ephemeralPrivateKey = SecKeyCreateRandomKey(parameters, error), out);
    require_action_quiet(ephemeralPublicKey = SecKeyCopyPublicKey(ephemeralPrivateKey), out,
                         SecError(errSecParam, error, CFSTR("Unable to get public key from generated ECkey")));
    require_quiet(ephemeralPubKeyData = SecKeyCopyExternalRepresentation(ephemeralPublicKey, error), out);

    context->key = ephemeralPrivateKey;
    require_quiet(keyExchangeResult = keyExchangeCopyResult(context, keyExchangeAlgorithm, true,
                                                            ephemeralPubKeyData, pubKeyData, variableIV, in2, error), out);
    if (context->mode == kSecKeyOperationModePerform) {
        // Encrypt input data using AES-GCM.
        ciphertext = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, ephemeralPubKeyData);
        require_quiet(encryptCopyResult(keyExchangeResult, in1, in2, ciphertext, error), out);
        result = CFRetain(ciphertext);
    } else {
        result = CFRetain(keyExchangeResult);
    }

out:
    CFReleaseSafe(parameters);
    CFReleaseSafe(ephemeralPrivateKey);
    CFReleaseSafe(ephemeralPublicKey);
    CFReleaseSafe(pubKeyData);
    CFReleaseSafe(ephemeralPubKeyData);
    CFReleaseSafe(keyExchangeResult);
    CFReleaseSafe(ciphertext);
    context->key = originalKey;
    return result;
}

static CFTypeRef SecKeyECIESCopyDecryptedData(SecKeyOperationContext *context, SecKeyAlgorithm keyExchangeAlgorithm,
                                              SecKeyECIESKeyExchangeCopyResult keyExchangeCopyResult,
                                              SecKeyECIESDecryptCopyResult decryptCopyResult, bool variableIV,
                                              CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = NULL;
    CFDictionaryRef parameters = NULL;
    CFDataRef ephemeralPubKeyData = NULL, keyExchangeResult = NULL, pubKeyData = NULL;
    SecKeyRef pubKey = NULL;
    CFDataRef ciphertext = NULL;
    const UInt8 *ciphertextBuffer = NULL;
    CFIndex keySize = 0;

    require_action_quiet(parameters = SecKeyCopyAttributes(context->key), out,
                         SecError(errSecParam, error, CFSTR("Unable to export key parameters")));
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandom), out, result = kCFNull);
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyClass), kSecAttrKeyClassPrivate), out, result = kCFNull);

    if (context->mode == kSecKeyOperationModePerform) {
        // Extract ephemeral public key from the packet.
        keySize = (SecKeyGetCFIndexFromRef(CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits)) + 7) / 8;
        require_action_quiet(CFDataGetLength(in1) >= keySize * 2 + 1, out,
                             SecError(errSecParam, error, CFSTR("%@: too small input packet for ECIES decrypt"), context->key));
        ciphertextBuffer = CFDataGetBytePtr(in1);
        ephemeralPubKeyData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, ciphertextBuffer, keySize * 2 + 1, kCFAllocatorNull);
        ciphertextBuffer += keySize * 2 + 1;

        require_action_quiet(pubKey = SecKeyCopyPublicKey(context->key), out,
                             SecError(errSecParam, error, CFSTR("%@: Unable to get public key"), context->key));
        require_quiet(pubKeyData = SecKeyCopyExternalRepresentation(pubKey, error), out);
    }

    // Perform keyExchange operation.
    require_quiet(keyExchangeResult = keyExchangeCopyResult(context, keyExchangeAlgorithm, false,
                                                            ephemeralPubKeyData, pubKeyData, variableIV, in2, error), out);
    if (context->mode == kSecKeyOperationModePerform) {
        // Decrypt ciphertext using AES-GCM.
        ciphertext = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, ciphertextBuffer, CFDataGetLength(in1) - (keySize * 2 + 1),
                                                 kCFAllocatorNull);
        require_quiet(result = decryptCopyResult(keyExchangeResult, ciphertext, in2, error), out);
    } else {
        result = CFRetain(keyExchangeResult);
    }

out:
    CFReleaseSafe(parameters);
    CFReleaseSafe(ephemeralPubKeyData);
    CFReleaseSafe(keyExchangeResult);
    CFReleaseSafe(pubKeyData);
    CFReleaseSafe(pubKey);
    CFReleaseSafe(ciphertext);
    return result;
}

static const CFIndex kSecKeyIESTagLength = 16;
static const UInt8 kSecKeyIESIV[16] = { 0 };

static CFDataRef SecKeyECIESKeyExchangeKDFX963CopyResult(SecKeyOperationContext *context, SecKeyAlgorithm keyExchangeAlgorithm,
                                                         bool encrypt, CFDataRef ephemeralPubKey, CFDataRef pubKey, bool variableIV,
                                                         CFDictionaryRef inParams, CFErrorRef *error) {
    NSDictionary *parametersForKeyExchange, *inputParameters = (__bridge NSDictionary *)inParams;
    NSData *result;
    NSMutableData *sharedInfoForKeyExchange;

    CFArrayAppendValue(context->algorithm, keyExchangeAlgorithm);
    context->operation = kSecKeyOperationTypeKeyExchange;

    if (context->mode == kSecKeyOperationModePerform) {
        NSInteger keySize = 0;
        NSNumber *keySizeObject = inputParameters[(__bridge id)kSecKeyEncryptionParameterSymmetricKeySizeInBits];
        if (keySizeObject != nil) {
            if (![keySizeObject isKindOfClass:NSNumber.class]) {
                SecError(errSecParam, error, CFSTR("Bad requested kSecKeyEncryptionParameterSymmetricKeySizeInBits: %@"), keySizeObject);
                return NULL;
            }
            keySize = keySizeObject.integerValue / 8;
        } else {
            // Use 128bit AES for EC keys <= 256bit, 256bit AES for larger keys.
            keySize = ((CFDataGetLength(pubKey) - 1) / 2) * 8;
            keySize = (keySize > 256) ? (256 / 8) : (128 / 8);
        }

        if (variableIV) {
            keySize += sizeof(kSecKeyIESIV);
        }

        // Generate shared secret using KDF.
        sharedInfoForKeyExchange = ((__bridge NSData *)ephemeralPubKey).mutableCopy;
        NSData *sharedInfo = inputParameters[(__bridge id)kSecKeyKeyExchangeParameterSharedInfo];
        if (sharedInfo != nil) {
            [sharedInfoForKeyExchange appendData:sharedInfo];
        }
        parametersForKeyExchange = @{ (__bridge id)kSecKeyKeyExchangeParameterSharedInfo: sharedInfoForKeyExchange,
                                      (__bridge id)kSecKeyKeyExchangeParameterRequestedSize: @(keySize) };
    }

    result = CFBridgingRelease(SecKeyRunAlgorithmAndCopyResult(context, encrypt ? pubKey : ephemeralPubKey, (__bridge CFDictionaryRef)parametersForKeyExchange, error));
    if (context->mode == kSecKeyOperationModePerform && !variableIV && result != NULL) {
        // Append all-zero IV to the result.
        NSMutableData *data = result.mutableCopy;
        [data appendBytes:kSecKeyIESIV length:sizeof(kSecKeyIESIV)];
        result = [NSData dataWithData:data];
    }
    return CFBridgingRetain(result);
}

static Boolean SecKeyECIESEncryptAESGCMCopyResult(CFDataRef keyExchangeResult, CFDataRef inData, CFDictionaryRef inParams,
                                                  CFMutableDataRef result, CFErrorRef *error) {
    Boolean res = FALSE;
    CFIndex prefix = CFDataGetLength(result);
    CFDataSetLength(result, prefix + CFDataGetLength(inData) + kSecKeyIESTagLength);
    UInt8 *resultBuffer = CFDataGetMutableBytePtr(result) + prefix;
    UInt8 *tagBuffer = resultBuffer + CFDataGetLength(inData);
    CFIndex aesKeySize = CFDataGetLength(keyExchangeResult) - sizeof(kSecKeyIESIV);
    const UInt8 *ivBuffer = CFDataGetBytePtr(keyExchangeResult) + aesKeySize;
    CFDataRef aad = inParams ? CFDictionaryGetValue(inParams, kSecKeyEncryptionParameterSymmetricAAD) : NULL;
    require_action_quiet(ccgcm_one_shot(ccaes_gcm_encrypt_mode(),
                                        aesKeySize, CFDataGetBytePtr(keyExchangeResult),
                                        sizeof(kSecKeyIESIV), ivBuffer,
                                        aad ? CFDataGetLength(aad) : 0, aad ? CFDataGetBytePtr(aad) : NULL,
                                        CFDataGetLength(inData), CFDataGetBytePtr(inData),
                                        resultBuffer, kSecKeyIESTagLength, tagBuffer) == 0, out,
                         SecError(errSecParam, error, CFSTR("ECIES: Failed to aes-gcm encrypt data")));
    res = TRUE;
out:
    return res;
}

static CFDataRef SecKeyECIESDecryptAESGCMCopyResult(CFDataRef keyExchangeResult, CFDataRef inData, CFDictionaryRef inParams,
                                                    CFErrorRef *error) {
    CFDataRef result = NULL;
    CFMutableDataRef plaintext = NULL;
    CFMutableDataRef tag = NULL;
    require_action_quiet(CFDataGetLength(inData) >= kSecKeyIESTagLength, out, SecError(errSecParam, error, CFSTR("ECIES: Input data too short")));
    plaintext = CFDataCreateMutableWithScratch(kCFAllocatorDefault, CFDataGetLength(inData) - kSecKeyIESTagLength);
    tag = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), kSecKeyIESTagLength);
    CFDataGetBytes(inData, CFRangeMake(CFDataGetLength(inData) - kSecKeyIESTagLength, kSecKeyIESTagLength),
                   CFDataGetMutableBytePtr(tag));
    CFIndex aesKeySize = CFDataGetLength(keyExchangeResult) - sizeof(kSecKeyIESIV);
    const UInt8 *ivBuffer = CFDataGetBytePtr(keyExchangeResult) + aesKeySize;
    CFDataRef aad = inParams ? CFDictionaryGetValue(inParams, kSecKeyEncryptionParameterSymmetricAAD) : NULL;
    require_action_quiet(ccgcm_one_shot(ccaes_gcm_decrypt_mode(),
                                        aesKeySize, CFDataGetBytePtr(keyExchangeResult),
                                        sizeof(kSecKeyIESIV), ivBuffer,
                                        aad ? CFDataGetLength(aad) : 0, aad ? CFDataGetBytePtr(aad) : NULL,
                                        CFDataGetLength(plaintext), CFDataGetBytePtr(inData), CFDataGetMutableBytePtr(plaintext),
                                        kSecKeyIESTagLength, CFDataGetMutableBytePtr(tag)) == 0, out,
                         SecError(errSecParam, error, CFSTR("ECIES: Failed to aes-gcm decrypt data")));
    result = CFRetain(plaintext);
out:
    CFReleaseSafe(plaintext);
    CFReleaseSafe(tag);
    return result;
}

#define ECIES_X963_ADAPTOR(hashname, cofactor, namepart, variableIV) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIES ## cofactor ## namepart ## hashname( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyECIESCopyEncryptedData(context, kSecKeyAlgorithmECDHKeyExchange ## cofactor ## X963 ## hashname, \
         SecKeyECIESKeyExchangeKDFX963CopyResult, SecKeyECIESEncryptAESGCMCopyResult, variableIV, in1, in2, error); \
} \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIES ## cofactor ## namepart ## hashname( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyECIESCopyDecryptedData(context, kSecKeyAlgorithmECDHKeyExchange ## cofactor ## X963 ## hashname, \
        SecKeyECIESKeyExchangeKDFX963CopyResult, SecKeyECIESDecryptAESGCMCopyResult, variableIV, in1, in2, error); \
}

ECIES_X963_ADAPTOR(SHA1, Standard, X963, false)
ECIES_X963_ADAPTOR(SHA224, Standard, X963, false)
ECIES_X963_ADAPTOR(SHA256, Standard, X963, false)
ECIES_X963_ADAPTOR(SHA384, Standard, X963, false)
ECIES_X963_ADAPTOR(SHA512, Standard, X963, false)
ECIES_X963_ADAPTOR(SHA1, Cofactor, X963, false)
ECIES_X963_ADAPTOR(SHA224, Cofactor, X963, false)
ECIES_X963_ADAPTOR(SHA256, Cofactor, X963, false)
ECIES_X963_ADAPTOR(SHA384, Cofactor, X963, false)
ECIES_X963_ADAPTOR(SHA512, Cofactor, X963, false)

ECIES_X963_ADAPTOR(SHA224, Standard, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA256, Standard, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA384, Standard, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA512, Standard, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA224, Cofactor, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA256, Cofactor, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA384, Cofactor, VariableIVX963, true)
ECIES_X963_ADAPTOR(SHA512, Cofactor, VariableIVX963, true)

#undef ECIES_X963_ADAPTOR

static CFDataRef SecKeyECIESKeyExchangeSHA2562PubKeysCopyResult(SecKeyOperationContext *context, SecKeyAlgorithm keyExchangeAlgorithm,
                                                                bool encrypt, CFDataRef ephemeralPubKey, CFDataRef pubKey, bool variableIV,
                                                                CFDictionaryRef inParams, CFErrorRef *error) {
    CFArrayAppendValue(context->algorithm, keyExchangeAlgorithm);
    context->operation = kSecKeyOperationTypeKeyExchange;
    CFMutableDataRef result = (CFMutableDataRef)SecKeyRunAlgorithmAndCopyResult(context, ephemeralPubKey, NULL, error);
    if (result != NULL && context->mode == kSecKeyOperationModePerform) {
        const struct ccdigest_info *di = ccsha256_di();
        ccdigest_di_decl(di, ctx);
        ccdigest_init(di, ctx);
        ccdigest_update(di, ctx, CFDataGetLength(result), CFDataGetBytePtr(result));
        ccdigest_update(di, ctx, CFDataGetLength(ephemeralPubKey), CFDataGetBytePtr(ephemeralPubKey));
        ccdigest_update(di, ctx, CFDataGetLength(pubKey), CFDataGetBytePtr(pubKey));
        CFAssignRetained(result, CFDataCreateMutableWithScratch(kCFAllocatorDefault, di->output_size));
        ccdigest_final(di, ctx, CFDataGetMutableBytePtr(result));
    }
    return result;
}

static CFDataRef SecKeyECIESDecryptAESCBCCopyResult(CFDataRef keyExchangeResult, CFDataRef inData, CFDictionaryRef inParams,
                                                    CFErrorRef *error) {
    CFMutableDataRef result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, CFDataGetLength(inData));
    cccbc_one_shot(ccaes_cbc_decrypt_mode(),
                   CFDataGetLength(keyExchangeResult), CFDataGetBytePtr(keyExchangeResult),
                   NULL, CFDataGetLength(keyExchangeResult) / CCAES_BLOCK_SIZE,
                   CFDataGetBytePtr(inData), CFDataGetMutableBytePtr(result));
    return result;
}

static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIES_Standard_SHA256_2PubKeys(
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    return SecKeyECIESCopyDecryptedData(context, kSecKeyAlgorithmECDHKeyExchangeStandard,
                                        SecKeyECIESKeyExchangeSHA2562PubKeysCopyResult,
                                        SecKeyECIESDecryptAESCBCCopyResult, false,
                                        in1, in2, error);
}

static CFTypeRef SecKeyRSAAESGCMCopyEncryptedData(SecKeyOperationContext *context, SecKeyAlgorithm keyWrapAlgorithm,
                                                  CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = NULL;
    CFDictionaryRef parameters = NULL;
    CFDataRef pubKeyData = NULL, wrappedKey = NULL, sessionKey = NULL;
    CFMutableDataRef ciphertext = NULL;

    require_action_quiet(parameters = SecKeyCopyAttributes(context->key), out,
                         SecError(errSecParam, error, CFSTR("Unable to export key parameters")));
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeRSA), out, result = kCFNull);
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyClass), kSecAttrKeyClassPublic), out, result = kCFNull);

    CFArrayAppendValue(context->algorithm, keyWrapAlgorithm);
    require_action_quiet(context->mode == kSecKeyOperationModePerform, out,
                         result = SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error));

    // Generate session key.  Use 128bit AES for RSA keys < 4096bit, 256bit AES for larger keys.
    require_quiet(pubKeyData = SecKeyCopyExternalRepresentation(context->key, error), out);
    CFIndex keySize = SecKeyGetCFIndexFromRef(CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits));
    require_action_quiet(sessionKey = CFDataCreateWithRandomBytes((keySize >= 4096) ? (256 / 8) : (128 / 8)), out,
                         SecError(errSecParam, error, CFSTR("Failed to generate session key")));

    // Encrypt session key using wrapping algorithm and store at the beginning of the result packet.
    require_action_quiet(wrappedKey = SecKeyRunAlgorithmAndCopyResult(context, sessionKey, NULL, error), out,
                         CFReleaseNull(result));
    ciphertext = CFDataCreateMutableWithScratch(kCFAllocatorDefault, CFDataGetLength(wrappedKey) + CFDataGetLength(in1) + kSecKeyIESTagLength);
    UInt8 *resultBuffer = CFDataGetMutableBytePtr(ciphertext);
    CFDataGetBytes(wrappedKey, CFRangeMake(0, CFDataGetLength(wrappedKey)), resultBuffer);
    resultBuffer += CFDataGetLength(wrappedKey);

    // Encrypt input data using AES-GCM.
    UInt8 *tagBuffer = resultBuffer + CFDataGetLength(in1);
    require_action_quiet(ccgcm_one_shot(ccaes_gcm_encrypt_mode(),
                                        CFDataGetLength(sessionKey), CFDataGetBytePtr(sessionKey),
                                        sizeof(kSecKeyIESIV), kSecKeyIESIV,
                                        CFDataGetLength(pubKeyData), CFDataGetBytePtr(pubKeyData),
                                        CFDataGetLength(in1), CFDataGetBytePtr(in1), resultBuffer,
                                        kSecKeyIESTagLength, tagBuffer) == 0, out,
                         SecError(errSecParam, error, CFSTR("RSAWRAP: Failed to aes-gcm encrypt data")));
    result = CFRetain(ciphertext);

out:
    CFReleaseSafe(parameters);
    CFReleaseSafe(pubKeyData);
    CFReleaseSafe(wrappedKey);
    CFReleaseSafe(sessionKey);
    CFReleaseSafe(ciphertext);
    return result;
}

static CFTypeRef SecKeyRSAAESGCMCopyDecryptedData(SecKeyOperationContext *context, SecKeyAlgorithm keyWrapAlgorithm,
                                                  CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = NULL;
    CFDictionaryRef parameters = NULL;
    CFMutableDataRef plaintext = NULL, tag = NULL;
    CFDataRef pubKeyData = NULL, sessionKey = NULL;
    SecKeyRef pubKey = NULL;

    require_action_quiet(parameters = SecKeyCopyAttributes(context->key), out,
                         SecError(errSecParam, error, CFSTR("Unable to export key parameters")));
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeRSA), out, result = kCFNull);
    require_action_quiet(CFEqual(CFDictionaryGetValue(parameters, kSecAttrKeyClass), kSecAttrKeyClassPrivate), out, result = kCFNull);

    CFArrayAppendValue(context->algorithm, keyWrapAlgorithm);
    require_action_quiet(context->mode == kSecKeyOperationModePerform, out,
                         result = SecKeyRunAlgorithmAndCopyResult(context, NULL, NULL, error));

    // Extract encrypted session key.
    require_action_quiet(pubKey = SecKeyCopyPublicKey(context->key), out,
                         SecError(errSecParam, error, CFSTR("%@: unable to get public key"), context->key));
    require_quiet(pubKeyData = SecKeyCopyExternalRepresentation(pubKey, error), out);

    CFIndex wrappedKeySize = SecKeyGetBlockSize(context->key);
    require_action_quiet(CFDataGetLength(in1) >= wrappedKeySize + kSecKeyIESTagLength, out,
                         SecError(errSecParam, error, CFSTR("RSA-WRAP too short input data")));
    sessionKey = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, CFDataGetBytePtr(in1), wrappedKeySize, kCFAllocatorNull);

    // Decrypt session key.
    CFAssignRetained(sessionKey, SecKeyRunAlgorithmAndCopyResult(context, sessionKey, NULL, error));
    require_quiet(sessionKey, out);
    CFIndex keySize = SecKeyGetCFIndexFromRef(CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits));
    keySize = (keySize >= 4096) ? (256 / 8) : (128 / 8);
    require_action_quiet(CFDataGetLength(sessionKey) == keySize, out,
                         SecError(errSecParam, error, CFSTR("RSA-WRAP bad ciphertext, unexpected session key size")));

    // Decrypt ciphertext using AES-GCM.
    plaintext = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), CFDataGetLength(in1) - wrappedKeySize - kSecKeyIESTagLength);
    tag = CFDataCreateMutableWithScratch(kCFAllocatorDefault, kSecKeyIESTagLength);
    CFDataGetBytes(in1, CFRangeMake(CFDataGetLength(in1) - kSecKeyIESTagLength, kSecKeyIESTagLength),
                   CFDataGetMutableBytePtr(tag));
    const UInt8 *ciphertextBuffer = CFDataGetBytePtr(in1);
    ciphertextBuffer += wrappedKeySize;
    require_action_quiet(ccgcm_one_shot(ccaes_gcm_decrypt_mode(),
                                        CFDataGetLength(sessionKey), CFDataGetBytePtr(sessionKey),
                                        sizeof(kSecKeyIESIV), kSecKeyIESIV,
                                        CFDataGetLength(pubKeyData), CFDataGetBytePtr(pubKeyData),
                                        CFDataGetLength(plaintext), ciphertextBuffer, CFDataGetMutableBytePtr(plaintext),
                                        kSecKeyIESTagLength, CFDataGetMutableBytePtr(tag)) == 0, out,
                         SecError(errSecParam, error, CFSTR("RSA-WRAP: Failed to aes-gcm decrypt data")));
    result = CFRetain(plaintext);

out:
    CFReleaseSafe(parameters);
    CFReleaseSafe(sessionKey);
    CFReleaseSafe(tag);
    CFReleaseSafe(pubKeyData);
    CFReleaseSafe(pubKey);
    CFReleaseSafe(plaintext);
    return result;
}

#define RSA_OAEP_AESGCM_ADAPTOR(hashname) \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEP ## hashname ## AESGCM( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSAAESGCMCopyEncryptedData(context, kSecKeyAlgorithmRSAEncryptionOAEP ## hashname, in1, in2, error); \
} \
static CFTypeRef SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEP ## hashname ## AESGCM( \
        SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) { \
    return SecKeyRSAAESGCMCopyDecryptedData(context, kSecKeyAlgorithmRSAEncryptionOAEP ## hashname, in1, in2, error); \
}

RSA_OAEP_AESGCM_ADAPTOR(SHA1)
RSA_OAEP_AESGCM_ADAPTOR(SHA224)
RSA_OAEP_AESGCM_ADAPTOR(SHA256)
RSA_OAEP_AESGCM_ADAPTOR(SHA384)
RSA_OAEP_AESGCM_ADAPTOR(SHA512)

#undef RSA_OAEP_AESGCM_ADAPTOR

SecKeyAlgorithmAdaptor SecKeyGetAlgorithmAdaptor(SecKeyOperationType operation, SecKeyAlgorithm algorithm) {
    static CFDictionaryRef adaptors[kSecKeyOperationTypeCount];
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const void *signKeys[] = {
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5,

            kSecKeyAlgorithmRSASignatureDigestPSSSHA1,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA224,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA256,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA384,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA512,

            kSecKeyAlgorithmRSASignatureRaw,
            kSecKeyAlgorithmRSASignatureRawCCUnit,

            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5,

            kSecKeyAlgorithmRSASignatureMessagePSSSHA1,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA224,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA256,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA384,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA512,

            kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA512,

            kSecKeyAlgorithmECDSASignatureDigestX962SHA1,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA224,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA256,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA384,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA512,

            kSecKeyAlgorithmECDSASignatureRFC4754,
            kSecKeyAlgorithmECDSASignatureDigestX962,
        };
        const void *signValues[] = {
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15SHA512,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15Raw,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPKCS1v15MD5,

            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPSSSHA1,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPSSSHA224,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPSSSHA256,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPSSSHA384,
            SecKeyAlgorithmAdaptorCopyResult_Sign_RSASignatureDigestPSSSHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureRaw,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureRawCCUnit,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA512,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15MD5,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureRFC4754,
            SecKeyAlgorithmAdaptorCopyResult_Sign_ECDSASignatureDigestX962,
        };
        check_compile_time(array_size(signKeys) == array_size(signValues));
        adaptors[kSecKeyOperationTypeSign] = CFDictionaryCreate(kCFAllocatorDefault, signKeys, signValues,
                                                                array_size(signKeys), &kCFTypeDictionaryKeyCallBacks, NULL);

        const void *verifyKeys[] = {
            kSecKeyAlgorithmRSASignatureRaw,

            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5,

            kSecKeyAlgorithmRSASignatureDigestPSSSHA1,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA224,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA256,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA384,
            kSecKeyAlgorithmRSASignatureDigestPSSSHA512,

            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
            kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5,

            kSecKeyAlgorithmRSASignatureMessagePSSSHA1,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA224,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA256,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA384,
            kSecKeyAlgorithmRSASignatureMessagePSSSHA512,

            kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
            kSecKeyAlgorithmECDSASignatureMessageX962SHA512,

            kSecKeyAlgorithmECDSASignatureDigestX962SHA1,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA224,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA256,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA384,
            kSecKeyAlgorithmECDSASignatureDigestX962SHA512,

            kSecKeyAlgorithmECDSASignatureRFC4754,
            kSecKeyAlgorithmECDSASignatureDigestX962,
        };
        const void *verifyValues[] = {
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureRaw,

            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15SHA512,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15Raw,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPKCS1v15MD5,

            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSSSHA1,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSSSHA224,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSSSHA256,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSSSHA384,
            SecKeyAlgorithmAdaptorCopyResult_Verify_RSASignatureDigestPSSSHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15SHA512,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePKCS1v15MD5,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_RSASignatureMessagePSSSHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureMessageX962SHA512,

            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA1,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA224,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA256,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA384,
            SecKeyAlgorithmAdaptorCopyResult_SignVerify_ECDSASignatureDigestX962SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureRFC4754,
            SecKeyAlgorithmAdaptorCopyResult_Verify_ECDSASignatureDigestX962,
        };
        check_compile_time(array_size(verifyKeys) == array_size(verifyValues));
        adaptors[kSecKeyOperationTypeVerify] = CFDictionaryCreate(kCFAllocatorDefault, verifyKeys, verifyValues,
                                                                  array_size(verifyKeys), &kCFTypeDictionaryKeyCallBacks, NULL);

        const void *encryptKeys[] = {
            kSecKeyAlgorithmRSAEncryptionRaw,
            kSecKeyAlgorithmRSAEncryptionRawCCUnit,

            kSecKeyAlgorithmRSAEncryptionPKCS1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512,

            kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA512AESGCM,
        };
        const void *encryptValues[] = {
            SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRaw,
            SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRawCCUnit,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionPKCS1,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA1,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA224,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA256,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA384,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA512,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA1AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA224AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA256AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA384AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_RSAEncryptionOAEPSHA512AESGCM,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardVariableIVX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardVariableIVX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardVariableIVX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESStandardVariableIVX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorVariableIVX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorVariableIVX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorVariableIVX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Encrypt_ECIESCofactorVariableIVX963SHA512,
        };
        check_compile_time(array_size(encryptKeys) == array_size(encryptValues));
        adaptors[kSecKeyOperationTypeEncrypt] = CFDictionaryCreate(kCFAllocatorDefault, encryptKeys, encryptValues,
                                                                   array_size(encryptKeys), &kCFTypeDictionaryKeyCallBacks, NULL);

        const void *decryptKeys[] = {
            kSecKeyAlgorithmRSAEncryptionRaw,
            kSecKeyAlgorithmRSAEncryptionRawCCUnit,

            kSecKeyAlgorithmRSAEncryptionPKCS1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512,

            kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA224AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA384AESGCM,
            kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA512AESGCM,

            kSecKeyAlgorithmECIESEncryptionAKSSmartCard,
        };
        const void *decryptValues[] = {
            SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRaw,
            SecKeyAlgorithmAdaptorCopyResult_EncryptDecrypt_RSAEncryptionRawCCUnit,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionPKCS1,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA1,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA224,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA256,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA384,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA512,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA1AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA224AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA256AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA384AESGCM,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_RSAEncryptionOAEPSHA512AESGCM,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardVariableIVX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardVariableIVX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardVariableIVX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESStandardVariableIVX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorVariableIVX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorVariableIVX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorVariableIVX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIESCofactorVariableIVX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_Decrypt_ECIES_Standard_SHA256_2PubKeys,
        };
        check_compile_time(array_size(decryptKeys) == array_size(decryptValues));
        adaptors[kSecKeyOperationTypeDecrypt] = CFDictionaryCreate(kCFAllocatorDefault, decryptKeys, decryptValues,
                                                                   array_size(decryptKeys), &kCFTypeDictionaryKeyCallBacks, NULL);

        const void *keyExchangeKeys[] = {
            kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1,
            kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA224,
            kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA256,
            kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA384,
            kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA512,

            kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA1,
            kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA224,
            kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA256,
            kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA384,
            kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA512,
        };
        const void *keyExchangeValues[] = {

            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHStandardX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHStandardX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHStandardX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHStandardX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHStandardX963SHA512,

            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHCofactorX963SHA1,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHCofactorX963SHA224,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHCofactorX963SHA256,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHCofactorX963SHA384,
            SecKeyAlgorithmAdaptorCopyResult_KeyExchange_ECDHCofactorX963SHA512,
        };
        check_compile_time(array_size(keyExchangeKeys) == array_size(keyExchangeKeys));
        adaptors[kSecKeyOperationTypeKeyExchange] = CFDictionaryCreate(kCFAllocatorDefault, keyExchangeKeys, keyExchangeValues,
                                                                       array_size(keyExchangeKeys), &kCFTypeDictionaryKeyCallBacks, NULL);
    });
    
    return CFDictionaryGetValue(adaptors[operation], algorithm);
}
