//
//  CommonKeyDerivation.c
//  CommonCrypto
//
//  Created by Richard Murphy on 1/21/14.
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#include <stdio.h>
#include "testbyteBuffer.h"
#include "testutil.h"
#include "capabilities.h"
#include "testmore.h"
#include <string.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonKeyDerivationSPI.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonKeyDerivation.h>

typedef struct KDFVector_t {
    char *password;
    char *salt;
    int saltLen; //negative means get the salt from the string
    unsigned rounds;
    CCDigestAlgorithm alg;
    int dklen;
    char *expectedstr;
    int expected_failure;
} KDFVector;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static KDFVector kdfv[] = {
    // Test Case PBKDF2 - HMACSHA1 http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-00
    { "password", "salt", -1, 1   , kCCDigestSHA1, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6", 0 },
    { "password", "salt", -1, 2   , kCCDigestSHA1, 20, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", 0 },
    { "password", "salt", -1, 4096, kCCDigestSHA1, 20, "4b007901b765489abead49d926f721d065a429c1", 0 },
    { "password", "salt", -1, 1   , 0            , 20, NULL, kCCParamError},
    { "",         "salt", -1, 1   , kCCDigestSHA1, 20, "a33dddc30478185515311f8752895d36ea4363a2", kCCParamError},
    {.password=NULL},
};

static KDFVector kdfv_for_OriginalKDF[] = {
    // some extra
    { "password", "salt",  0, 4096, kCCDigestSHA1, 20, "546878f250c3baf85d44fbf77435a03828811dfb", 0 },
    { "password", NULL  ,  0, 4096, kCCDigestSHA1, 20, "546878f250c3baf85d44fbf77435a03828811dfb", 0 },
    { "password", NULL  ,999, 4096, kCCDigestSHA1, 20, "", kCCParamError },
    {.password=NULL},
};
#pragma clang diagnostic pop
static int testOriginalKDF(KDFVector *v, int testid) {
    CCPseudoRandomAlgorithm prf = digestID2PRF(v->alg);
    byteBuffer derivedKey = mallocByteBuffer(v->dklen);
    byteBuffer expected = hexStringToBytesIfNotNULL(v->expectedstr);
    
    size_t saltLen;
    
    if (v->saltLen<0&& v->salt!=NULL)
        saltLen = strlen(v->salt);
    else
        saltLen = v->saltLen;
    
    int retval = CCKeyDerivationPBKDF(kCCPBKDF2, v->password, strlen(v->password), (uint8_t *) v->salt, saltLen, prf, v->rounds, derivedKey->bytes, derivedKey->len);
    if(v->expected_failure && strlen(v->password)!=0) {
        is(retval, v->expected_failure, "Test %d: CCPBKDF2 should have failed",testid);
    } else {
        is(retval,0,"Test %d: non-zero return value",testid);
        is(derivedKey->len,expected->len,"Test %d: Length failure",testid);
        ok_memcmp(derivedKey->bytes, expected->bytes, expected->len, "Test %d: Derived key failure", testid);
    }
    
    free(derivedKey);
    free(expected);
    return 1;
}

static int testNewKDF(KDFVector *v, int testid) {
    byteBuffer derivedKey = mallocByteBuffer(v->dklen);
    byteBuffer expected = hexStringToBytesIfNotNULL(v->expectedstr);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CCStatus retval = CCKeyDerivationHMac(kCCKDFAlgorithmPBKDF2_HMAC, v->alg, v->rounds, v->password, strlen(v->password), NULL, 0, NULL, 0, NULL, 0, v->salt, strlen(v->salt), derivedKey->bytes, derivedKey->len);
#pragma clang diagnostic pop
    if(v->expected_failure) {
        is(retval, v->expected_failure,"Test %d: PBKDF2_HMAC Expected failure",testid);
    } else {
        is(retval,0,"Test %d: non-zero return value",testid);
        is(derivedKey->len,expected->len,"Test %d: Length failure",testid);
        ok_memcmp(derivedKey->bytes, expected->bytes, expected->len, "Test %d: Derived key failure", testid);
    }
    free(derivedKey);
    free(expected);
    return 1;
}

#define CCDIGEST_TYPE_SHA1 1
#define CCDIGEST_TYPE_SHA256 256
#define CCDIGEST_TYPE_SHA512 512

typedef struct {
    int type;
    const char *ikm;
    const char *salt;
    const char *info;
    size_t len;
    const char *prk;
    const char *okm;
} test_vector_t;

static const test_vector_t hkdf_sha256_tests[] = {
    // RFC 5869 Test Case 1
    { /* Type */ CCDIGEST_TYPE_SHA256,
      /* IKM */ "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      /* Salt */ "000102030405060708090a0b0c",
      /* Info */ "f0f1f2f3f4f5f6f7f8f9",
      /* Len */ 42,
      /* PRK */
      "077709362c2e32df0ddc3f0dc47bba63"
      "90b6c73bb50f9c3122ec844ad7c2b3e5",
      /* OKM */
      "3cb25f25faacd57a90434f64d0362f2a"
      "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865" },
    // RFC 5869 Test Case 2
    { /* Type */ CCDIGEST_TYPE_SHA256,
      /* IKM */
      "000102030405060708090a0b0c0d0e0f"
      "101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f"
      "303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f",
      /* Salt */
      "606162636465666768696a6b6c6d6e6f"
      "707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f"
      "909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
      /* Info */
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
      "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
      "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
      /* Len */ 82,
      /* PRK */
      "06a6b88c5853361a06104c9ceb35b45c"
      "ef760014904671014a193f40c15fc244",
      /* OKM */
      "b11e398dc80327a1c8e7f78c596a4934"
      "4f012eda2d4efad8a050cc4c19afa97c"
      "59045a99cac7827271cb41c65e590e09"
      "da3275600c2f09b8367793a9aca3db71"
      "cc30c58179ec3e87c14c01d5c1f3434f"
      "1d87" },
    // RFC 5869 Test Case 3
    { /* Type */ CCDIGEST_TYPE_SHA256,
      /* IKM */ "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      /* Salt */ "",
      /* Info */ "",
      /* Len */ 42,
      /* PRK */
      "19ef24a32c717b167f33a91d6f648bdf"
      "96596776afdb6377ac434c1c293ccb04",
      /* OKM */
      "8da4e775a563c18f715f802a063c5a31"
      "b8a11f5c5ee1879ec3454e5f3c738d2d"
      "9d201395faa4b61a96c8" },
    // RFC 5869 Test Case 4
    { /* Type */ CCDIGEST_TYPE_SHA1,
      /* IKM */ "0b0b0b0b0b0b0b0b0b0b0b",
      /* Salt */ "000102030405060708090a0b0c",
      /* Info */ "f0f1f2f3f4f5f6f7f8f9",
      /* Len */ 42,
      /* PRK */ "9b6c18c432a7bf8f0e71c8eb88f4b30baa2ba243",
      /* OKM */
      "085a01ea1b10f36933068b56efa5ad81"
      "a4f14b822f5b091568a9cdd4f155fda2"
      "c22e422478d305f3f896" },
    // RFC 5869 Test Case 5
    { /* Type */ CCDIGEST_TYPE_SHA1,
      /* IKM */
      "000102030405060708090a0b0c0d0e0f"
      "101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f"
      "303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f",
      /* Salt */
      "606162636465666768696a6b6c6d6e6f"
      "707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f"
      "909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
      /* Info */
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
      "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
      "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
      /* Len */ 82,
      /* PRK */ "8adae09a2a307059478d309b26c4115a224cfaf6",
      /* OKM */
      "0bd770a74d1160f7c9f12cd5912a06eb"
      "ff6adcae899d92191fe4305673ba2ffe"
      "8fa3f1a4e5ad79f3f334b3b202b2173c"
      "486ea37ce3d397ed034c7f9dfeb15c5e"
      "927336d0441f4c4300e2cff0d0900b52"
      "d3b4" },
    // RFC 5869 Test Case 6
    { /* Type */ CCDIGEST_TYPE_SHA1,
      /* IKM */ "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
      /* Salt */ "",
      /* Info */ "",
      /* Len */ 42,
      /* PRK */ "da8c8a73c7fa77288ec6f5e7c297786aa0d32d01",
      /* OKM */
      "0ac1af7002b3d761d1e55298da9d0506"
      "b9ae52057220a306e07b6b87e8df21d0"
      "ea00033de03984d34918" },
    // RFC 5869 Test Case 7
    { /* Type */ CCDIGEST_TYPE_SHA1,
      /* IKM */ "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
      /* Salt */ "",
      /* Info */ "",
      /* Len */ 42,
      /* PRK */ "2adccada18779e7c2077ad2eb19d3f3e731385dd",
      /* OKM */
      "2c91117204d745f3500d636a62f64f0a"
      "b3bae548aa53d423b0d1f27ebba6f5e5"
      "673a081d70cce7acfc48" },
    // RFC 5869 Test Case 1 (updated for SHA-512)
    { /* Type */ CCDIGEST_TYPE_SHA512,
      /* IKM */ "0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B",
      /* Salt */ "000102030405060708090A0B0C",
      /* Info */ "F0F1F2F3F4F5F6F7F8F9",
      /* Len */ 42,
      /* PRK */ "",
      /* OKM */
      "832390086CDA71FB47625BB5CEB168E4"
      "C8E26A1A16ED34D9FC7FE92C14815793"
      "38DA362CB8D9F925D7CB" },
    // RFC 5869 Test Case 2 (updated for SHA-512)
    { /* Type */ CCDIGEST_TYPE_SHA512,
      /* IKM */
      "000102030405060708090A0B0C0D0E0F"
      "101112131415161718191A1B1C1D1E1F"
      "202122232425262728292A2B2C2D2E2F"
      "303132333435363738393A3B3C3D3E3F"
      "404142434445464748494A4B4C4D4E4F",
      /* Salt */
      "606162636465666768696A6B6C6D6E6F"
      "707172737475767778797A7B7C7D7E7F"
      "808182838485868788898A8B8C8D8E8F"
      "909192939495969798999A9B9C9D9E9F"
      "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF",
      /* Info */
      "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
      "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
      "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
      "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF",
      /* Len */ 82,
      /* PRK */ "",
      /* OKM */
      "CE6C97192805B346E6161E821ED16567"
      "3B84F400A2B514B2FE23D84CD189DDF1"
      "B695B48CBD1C8388441137B3CE28F16A"
      "A64BA33BA466B24DF6CFCB021ECFF235"
      "F6A2056CE3AF1DE44D572097A8505D9E"
      "7A93" },
    // RFC 5869 Test Case 3 (updated for SHA-512)
    { /* Type */ CCDIGEST_TYPE_SHA512,
      /* IKM */ "0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B",
      /* Salt */ "",
      /* Info */ "",
      /* Len */ 42,
      /* PRK */ "",
      /* OKM */
      "F5FA02B18298A72A8C23898A8703472C"
      "6EB179DC204C03425C970E3B164BF90F"
      "FF22D04836D0E2343BAC" },
};

static void testHKDFExpandExtract()
{
    for (size_t test_index = 0; test_index < sizeof(hkdf_sha256_tests) / sizeof(hkdf_sha256_tests[0]); test_index++) {
        const test_vector_t vector = hkdf_sha256_tests[test_index];

        byteBuffer salt = hexStringToBytes(vector.salt);
        byteBuffer context = hexStringToBytes(vector.info);
        byteBuffer ikm = hexStringToBytes(vector.ikm);
        byteBuffer okm = hexStringToBytes(vector.okm);
        byteBuffer prk = hexStringToBytes(vector.prk);

        uint8_t actual_prk[64] = {};
        uint8_t derived_key[1024] = {};

        CCKDFParametersRef params;
        CCStatus status = CCKDFParametersCreateHkdf(&params, salt->bytes, salt->len, context->bytes, context->len);
        ok(status == kCCSuccess, "Parameter creation failed. Expected %d, got %d", kCCSuccess, status);

        CCDigestAlgorithm algorithm = kCCDigestSHA256;
        size_t output_size = 0;
        switch (vector.type) {
            case CCDIGEST_TYPE_SHA1:
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
                algorithm = kCCDigestSHA1;
    #pragma clang diagnostic pop
                output_size = 20;
                break;
            case CCDIGEST_TYPE_SHA256:
                algorithm = kCCDigestSHA256;
                output_size = 32;
                break;
            case CCDIGEST_TYPE_SHA512:
                algorithm = kCCDigestSHA512;
                output_size = 64;
                break;
        }

        status = CCHKDFExtract(params, algorithm, ikm->bytes, ikm->len, actual_prk, output_size);
        ok(status == kCCSuccess, "Extract failed. Expected %d, got %d", kCCSuccess, status);
        if (prk->bytes) {
            ok(0 == memcmp(actual_prk, prk->bytes, prk->len), "Invalid PRK");
        }

        status = CCHKDFExpand(params, algorithm, actual_prk, output_size, derived_key, vector.len);
        ok(status == kCCSuccess, "Expand failed. Expected %d, got %d", kCCSuccess, status);
        if (okm->bytes) {
            ok(0 == memcmp(derived_key, okm->bytes, vector.len), "Invalid derived key");
        }

        free(salt);
        free(context);
        free(ikm);
        free(prk);
        free(okm);
    }
}

int CommonKeyDerivation(int __unused argc, char *const * __unused argv) {
	plan_tests(81);
    
    int i;
    for(i=0; kdfv[i].password!=NULL; i++) {
        testOriginalKDF(&kdfv[i],i);
        testNewKDF(&kdfv[i],i);
    }
    
    for(int k=0; kdfv_for_OriginalKDF[k].password!=NULL; k++, i++) {
        testOriginalKDF(&kdfv_for_OriginalKDF[k],i);
    }


    unsigned iter;
    // Password of length 10byte, salt 16bytes, output 32bytes, 100msec
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 0, kCCPRFHmacAlgSHA1, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA1 no salt:   %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA1, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA1:   %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA224, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA224: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA256, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA256: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA384, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA384: %7lu", iter);
    iter=CCCalibratePBKDF(kCCPBKDF2, 10, 16, kCCPRFHmacAlgSHA512, 32, 100);
    diag("CCCalibratePBKDF kCCPBKDF2 100ms for kCCPRFHmacAlgSHA512: %7lu", iter);

    testHKDFExpandExtract();

    return 0;
}
