// pd_cms.c - CMS signed-message decoding over OpenSSL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// OpenSSL parses the message and checks each signature, SecTrust judges the signer with the caller's policy
// certificates-only PKCS#7 bundles, as CA issuer URLs serve, are read the same way

// OpenSSL first: libDER, under Security's headers, defines ASN1_* macros that clash with OpenSSL's type names
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <Security/Security.h>
#include "../apple/CMS/CMSDecoder.h"
#include <pthread.h>

struct _CMSDecoder {
    CFRuntimeBase base;
    CFMutableDataRef message;
    CFDataRef detachedContent;
    CMS_ContentInfo *cms;
    CFArrayRef certificates;
    bool finalized;
};

static CFTypeID decoderTypeID;

static void decoderFinalize(CFTypeRef cf)
{
    CMSDecoderRef decoder = (CMSDecoderRef)cf;

    if (decoder->message)
        CFRelease(decoder->message);
    if (decoder->detachedContent)
        CFRelease(decoder->detachedContent);
    if (decoder->certificates)
        CFRelease(decoder->certificates);
    if (decoder->cms)
        CMS_ContentInfo_free(decoder->cms);
}

static const CFRuntimeClass decoderClass = { 0, "CMSDecoder", NULL, NULL, decoderFinalize, NULL, NULL, NULL, NULL };

static void registerDecoder(void)
{
    decoderTypeID = _CFRuntimeRegisterClass(&decoderClass);
}

CFTypeID CMSDecoderGetTypeID(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once(&once, registerDecoder);
    return decoderTypeID;
}

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

static CFArrayRef copyCertificates(STACK_OF(X509) *stack)
{
    CFMutableArrayRef certificates = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    for (int i = 0; stack && i < sk_X509_num(stack); i++) {
        SecCertificateRef certificate = createCertificate(sk_X509_value(stack, i));

        if (certificate) {
            CFArrayAppendValue(certificates, certificate);
            CFRelease(certificate);
        }
    }
    return certificates;
}

OSStatus CMSDecoderCreate(CMSDecoderRef *cmsDecoderOut)
{
    CMSDecoderRef decoder;

    if (cmsDecoderOut == NULL)
        return errSecParam;
    decoder = (CMSDecoderRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, CMSDecoderGetTypeID(),
        sizeof(struct _CMSDecoder) - sizeof(CFRuntimeBase), NULL);
    if (decoder == NULL)
        return errSecAllocate;
    decoder->message = CFDataCreateMutable(kCFAllocatorDefault, 0);
    *cmsDecoderOut = decoder;
    return errSecSuccess;
}

OSStatus CMSDecoderUpdateMessage(CMSDecoderRef cmsDecoder, const void *msgBytes, size_t msgBytesLen)
{
    if (cmsDecoder == NULL || cmsDecoder->finalized || (msgBytes == NULL && msgBytesLen))
        return errSecParam;
    CFDataAppendBytes(cmsDecoder->message, msgBytes, (CFIndex)msgBytesLen);
    return errSecSuccess;
}

OSStatus CMSDecoderSetDetachedContent(CMSDecoderRef cmsDecoder, CFDataRef detachedContent)
{
    if (cmsDecoder == NULL || detachedContent == NULL)
        return errSecParam;
    if (cmsDecoder->detachedContent)
        CFRelease(cmsDecoder->detachedContent);
    cmsDecoder->detachedContent = CFRetain(detachedContent);
    return errSecSuccess;
}

OSStatus CMSDecoderFinalizeMessage(CMSDecoderRef cmsDecoder)
{
    const unsigned char *bytes;

    if (cmsDecoder == NULL || cmsDecoder->finalized)
        return errSecParam;
    cmsDecoder->finalized = true;
    bytes = CFDataGetBytePtr(cmsDecoder->message);
    cmsDecoder->cms = d2i_CMS_ContentInfo(NULL, &bytes, CFDataGetLength(cmsDecoder->message));
    if (cmsDecoder->cms == NULL) {
        ERR_clear_error();
        return errSecDecode;
    }
    STACK_OF(X509) *certs = CMS_get1_certs(cmsDecoder->cms);

    cmsDecoder->certificates = copyCertificates(certs);
    sk_X509_pop_free(certs, X509_free);
    return errSecSuccess;
}

OSStatus CMSDecoderGetNumSigners(CMSDecoderRef cmsDecoder, size_t *numSigners)
{
    STACK_OF(CMS_SignerInfo) *signers;

    if (cmsDecoder == NULL || numSigners == NULL || cmsDecoder->cms == NULL)
        return errSecParam;
    signers = CMS_get0_SignerInfos(cmsDecoder->cms);
    *numSigners = signers ? (size_t)sk_CMS_SignerInfo_num(signers) : 0;
    return errSecSuccess;
}

OSStatus CMSDecoderCopyAllCerts(CMSDecoderRef cmsDecoder, CFArrayRef *certsOut)
{
    if (cmsDecoder == NULL || certsOut == NULL || cmsDecoder->cms == NULL)
        return errSecParam;
    *certsOut = CFArrayGetCount(cmsDecoder->certificates) ? CFRetain(cmsDecoder->certificates) : NULL;
    return errSecSuccess;
}

// the signature is OpenSSL's to check, the signer's certificate SecTrust's under the caller's policy
OSStatus CMSDecoderCopySignerStatus(CMSDecoderRef cmsDecoder, size_t signerIndex, CFTypeRef policyOrArray,
    Boolean evaluateSecTrust, CMSSignerStatus *signerStatusOut, SecTrustRef *secTrustOut,
    OSStatus *certVerifyResultCodeOut)
{
    STACK_OF(CMS_SignerInfo) *signers;
    CMS_SignerInfo *signer;
    X509 *signerX509 = NULL;
    BIO *content = NULL;
    CMSSignerStatus status = kCMSSignerUnsigned;
    OSStatus certResult = errSecSuccess;
    SecTrustRef trust = NULL;

    if (signerStatusOut)
        *signerStatusOut = kCMSSignerUnsigned;
    if (secTrustOut)
        *secTrustOut = NULL;
    if (certVerifyResultCodeOut)
        *certVerifyResultCodeOut = errSecSuccess;
    if (cmsDecoder == NULL || cmsDecoder->cms == NULL)
        return errSecParam;
    signers = CMS_get0_SignerInfos(cmsDecoder->cms);
    if (signers == NULL || sk_CMS_SignerInfo_num(signers) == 0)
        goto out;
    if (signerIndex >= (size_t)sk_CMS_SignerInfo_num(signers)) {
        status = kCMSSignerInvalidIndex;
        goto out;
    }
    if (CMS_is_detached(cmsDecoder->cms) && cmsDecoder->detachedContent == NULL) {
        status = kCMSSignerNeedsDetachedContent;
        goto out;
    }
    signer = sk_CMS_SignerInfo_value(signers, (int)signerIndex);
    STACK_OF(X509) *certs = CMS_get1_certs(cmsDecoder->cms);
    CMS_set1_signers_certs(cmsDecoder->cms, certs, 0);
    sk_X509_pop_free(certs, X509_free);
    CMS_SignerInfo_get0_algs(signer, NULL, &signerX509, NULL, NULL);
    if (signerX509 == NULL) {
        status = kCMSSignerInvalidCert;
        goto out;
    }
    if (cmsDecoder->detachedContent)
        content = BIO_new_mem_buf(CFDataGetBytePtr(cmsDecoder->detachedContent),
            (int)CFDataGetLength(cmsDecoder->detachedContent));
    // the signed attributes hold the content digest, checking them checks the content
    if (content && CMS_SignerInfo_verify_content(signer, content) <= 0)
        status = kCMSSignerInvalidSignature;
    else if (CMS_SignerInfo_verify(signer) <= 0)
        status = kCMSSignerInvalidSignature;
    else
        status = kCMSSignerValid;
    ERR_clear_error();

    if (policyOrArray || secTrustOut) {
        SecCertificateRef leaf = createCertificate(signerX509);
        CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

        if (leaf) {
            CFArrayAppendValue(chain, leaf);
            CFRelease(leaf);
        }
        CFArrayAppendArray(chain, cmsDecoder->certificates, CFRangeMake(0, CFArrayGetCount(cmsDecoder->certificates)));
        SecTrustCreateWithCertificates(chain, policyOrArray, &trust);
        CFRelease(chain);
        if (trust && evaluateSecTrust) {
            CFErrorRef error = NULL;

            if (!SecTrustEvaluateWithError(trust, &error)) {
                certResult = error ? (OSStatus)CFErrorGetCode(error) : errSecNotTrusted;
                if (status == kCMSSignerValid)
                    status = kCMSSignerInvalidCert;
            }
            if (error)
                CFRelease(error);
        }
    }
out:
    if (content)
        BIO_free(content);
    if (signerStatusOut)
        *signerStatusOut = status;
    if (certVerifyResultCodeOut)
        *certVerifyResultCodeOut = certResult;
    if (secTrustOut)
        *secTrustOut = trust;
    else if (trust)
        CFRelease(trust);
    return errSecSuccess;
}

// a degenerate signed-data message, certificates and no signers, is how CA issuer URLs bundle a chain
CFArrayRef SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message)
{
    const unsigned char *bytes;
    PKCS7 *p7;
    STACK_OF(X509) *certs = NULL;
    CFArrayRef result = NULL;

    if (message == NULL)
        return NULL;
    bytes = CFDataGetBytePtr(message);
    p7 = d2i_PKCS7(NULL, &bytes, CFDataGetLength(message));
    if (p7 == NULL) {
        ERR_clear_error();
        return NULL;
    }
    if (PKCS7_type_is_signed(p7) && p7->d.sign)
        certs = p7->d.sign->cert;
    if (certs && sk_X509_num(certs) > 0)
        result = copyCertificates(certs);
    PKCS7_free(p7);
    return result;
}
