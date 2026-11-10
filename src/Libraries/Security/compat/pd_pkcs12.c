// pd_pkcs12.c - SecPKCS12Import over OpenSSL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// OpenSSL decodes the file, each key and its certificate come back as a SecIdentity with the rest of the chain

// OpenSSL first: libDER, under Security's headers, defines ASN1_* macros that clash with OpenSSL's type names
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdlib.h>
#include <string.h>

const CFStringRef kSecImportExportPassphrase = CFSTR("passphrase");
const CFStringRef kSecImportItemLabel = CFSTR("label");
const CFStringRef kSecImportItemKeyID = CFSTR("keyid");
const CFStringRef kSecImportItemTrust = CFSTR("trust");
const CFStringRef kSecImportItemCertChain = CFSTR("chain");
const CFStringRef kSecImportItemIdentity = CFSTR("identity");

extern SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator, SecCertificateRef certificate, SecKeyRef privateKey);

static SecCertificateRef createCertificate(X509 *x509)
{
    unsigned char *der = NULL;
    int length = i2d_X509(x509, &der);
    SecCertificateRef certificate = NULL;

    if (length > 0) {
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, der, length);

        certificate = SecCertificateCreateWithData(kCFAllocatorDefault, data);
        CFRelease(data);
    }
    OPENSSL_free(der);
    return certificate;
}

// SecKey takes RSA keys as PKCS#1 and EC keys as 04 || X || Y || K, as SecKeyCopyExternalRepresentation gives them
static CFDataRef copyKeyData(EVP_PKEY *pkey, CFStringRef *keyType)
{
    if (EVP_PKEY_is_a(pkey, "RSA")) {
        unsigned char *der = NULL;
        int length = i2d_PrivateKey(pkey, &der);
        CFDataRef data = length > 0 ? CFDataCreate(kCFAllocatorDefault, der, length) : NULL;

        OPENSSL_free(der);
        *keyType = kSecAttrKeyTypeRSA;
        return data;
    }
    if (EVP_PKEY_is_a(pkey, "EC")) {
        unsigned char point[133];
        size_t pointLength = 0;
        BIGNUM *secret = NULL;
        CFMutableDataRef data = NULL;
        int fieldBytes = (EVP_PKEY_get_bits(pkey) + 7) / 8;

        if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, point, sizeof(point), &pointLength) &&
            EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &secret) && point[0] == 0x04) {
            data = CFDataCreateMutable(kCFAllocatorDefault, 0);
            CFDataAppendBytes(data, point, pointLength);
            CFDataIncreaseLength(data, fieldBytes);
            BN_bn2binpad(secret, CFDataGetMutableBytePtr(data) + pointLength, fieldBytes);
        }
        BN_clear_free(secret);
        *keyType = kSecAttrKeyTypeECSECPrimeRandom;
        return data;
    }
    return NULL;
}

static SecKeyRef createPrivateKey(EVP_PKEY *pkey)
{
    CFStringRef keyType = NULL;
    CFDataRef data = copyKeyData(pkey, &keyType);
    CFMutableDictionaryRef attributes;
    SecKeyRef key;

    if (data == NULL)
        return NULL;
    attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kSecAttrKeyType, keyType);
    CFDictionarySetValue(attributes, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    key = SecKeyCreateWithData(data, attributes, NULL);
    CFRelease(attributes);
    CFRelease(data);
    return key;
}

OSStatus SecPKCS12Import(CFDataRef pkcs12_data, CFDictionaryRef options, CFArrayRef *items)
{
    CFStringRef passphrase = options ? CFDictionaryGetValue(options, kSecImportExportPassphrase) : NULL;
    const unsigned char *bytes;
    char *password = NULL;
    PKCS12 *p12 = NULL;
    EVP_PKEY *pkey = NULL;
    X509 *x509 = NULL;
    STACK_OF(X509) *ca = NULL;
    OSStatus status = errSecDecode;

    if (pkcs12_data == NULL || items == NULL)
        return errSecParam;
    *items = NULL;
    if (passphrase && CFGetTypeID(passphrase) == CFStringGetTypeID()) {
        CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(passphrase), kCFStringEncodingUTF8) + 1;

        password = malloc(size);
        if (password == NULL || !CFStringGetCString(passphrase, password, size, kCFStringEncodingUTF8)) {
            free(password);
            return errSecParam;
        }
    }
    bytes = CFDataGetBytePtr(pkcs12_data);
    p12 = d2i_PKCS12(NULL, &bytes, CFDataGetLength(pkcs12_data));
    if (p12 == NULL)
        goto out;
    if (!PKCS12_verify_mac(p12, password, -1)) {
        status = errSecAuthFailed;
        goto out;
    }
    if (!PKCS12_parse(p12, password, &pkey, &x509, &ca))
        goto out;

    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    SecCertificateRef certificate = x509 ? createCertificate(x509) : NULL;
    SecKeyRef key = pkey ? createPrivateKey(pkey) : NULL;

    if (certificate && key) {
        SecIdentityRef identity = SecIdentityCreate(kCFAllocatorDefault, certificate, key);
        CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFMutableDictionaryRef item = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        CFArrayAppendValue(chain, certificate);
        for (int i = 0; ca && i < sk_X509_num(ca); i++) {
            SecCertificateRef issuer = createCertificate(sk_X509_value(ca, i));

            if (issuer) {
                CFArrayAppendValue(chain, issuer);
                CFRelease(issuer);
            }
        }
        if (identity) {
            CFDictionarySetValue(item, kSecImportItemIdentity, identity);
            CFRelease(identity);
        }
        CFDictionarySetValue(item, kSecImportItemCertChain, chain);
        CFArrayAppendValue(result, item);
        CFRelease(item);
        CFRelease(chain);
        status = identity ? errSecSuccess : errSecDecode;
    } else
        // a key type SecKey cannot hold, or a file with no key, gives no identity
        status = key == NULL && pkey != NULL ? errSecUnsupportedFormat : errSecDecode;
    if (key)
        CFRelease(key);
    if (certificate)
        CFRelease(certificate);
    if (status == errSecSuccess)
        *items = result;
    else
        CFRelease(result);
out:
    if (ca)
        sk_X509_pop_free(ca, X509_free);
    if (x509)
        X509_free(x509);
    if (pkey)
        EVP_PKEY_free(pkey);
    if (p12)
        PKCS12_free(p12);
    if (password) {
        OPENSSL_cleanse(password, strlen(password));
        free(password);
    }
    return status;
}
