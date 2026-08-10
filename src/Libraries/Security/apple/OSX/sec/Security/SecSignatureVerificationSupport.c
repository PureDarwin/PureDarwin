//
//  SecSignatureVerificationSupport.c
//  sec
//

#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <Security/SecSignatureVerificationSupport.h>

#include <CoreFoundation/CFString.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecBasePriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecECKeyPriv.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccder.h>

static const uint8_t *sec_decode_forced_uint(cc_size n,
    cc_unit *r, const uint8_t *der, const uint8_t *der_end)
{
    size_t len;
    der = ccder_decode_tl(CCDER_INTEGER, &len, der, der_end);
    if (der && ccn_read_uint(n, r, len, der) >= 0) {
        return der + len;
    }
    return NULL;
}

static CFErrorRef
SecCreateSignatureVerificationError(OSStatus errorCode, CFStringRef descriptionString)
{
    const CFStringRef defaultDescription = CFSTR("Error verifying signature.");
    const void* keys[1] = { kCFErrorDescriptionKey };
    const void* values[2] = { (descriptionString) ? descriptionString : defaultDescription };
    return CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault,
        kCFErrorDomainOSStatus, errorCode, keys, values, 1);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static void
SecRecreateSignatureWithAlgId(SecKeyRef publicKey, const SecAsn1AlgId *publicKeyAlgId,
    const uint8_t *oldSignature, size_t oldSignatureSize,
    uint8_t **newSignature, size_t *newSignatureSize)
#pragma clang diagnostic pop
{
    if (!publicKey || !publicKeyAlgId ||
        kSecECDSAAlgorithmID != SecKeyGetAlgorithmId(publicKey)) {
        // ECDSA SHA-256 is the only type of signature currently supported by this function
        return;
    }

    cc_size n = ccec_cp_n(ccec_cp_256());
    cc_unit r[n], s[n];

    const uint8_t *oldSignatureEnd = oldSignature + oldSignatureSize;

    oldSignature = ccder_decode_sequence_tl(&oldSignatureEnd, oldSignature, oldSignatureEnd);
    oldSignature = sec_decode_forced_uint(n, r, oldSignature, oldSignatureEnd);
    oldSignature = sec_decode_forced_uint(n, s, oldSignature, oldSignatureEnd);
    if (!oldSignature || !(oldSignatureEnd == oldSignature)) {
        // failed to decode the old signature successfully
        *newSignature = NULL;
        return;
    }

    const uint8_t *outputPointer = *newSignature;
    uint8_t *outputEndPointer = *newSignature + *newSignatureSize;

    *newSignature = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE,
        outputEndPointer, outputPointer,
        ccder_encode_integer(n, r, outputPointer, ccder_encode_integer(n, s, outputPointer, outputEndPointer)));
    long newSigSize = outputEndPointer - *newSignature;
    *newSignatureSize = (newSigSize >= 0) ? (size_t)newSigSize : 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool SecVerifySignatureWithPublicKey(SecKeyRef publicKey, const SecAsn1AlgId *publicKeyAlgId,
                                     const uint8_t *dataToHash, size_t amountToHash,
                                     const uint8_t *signatureStart, size_t signatureSize,
                                     CFErrorRef *error)
#pragma clang diagnostic pop
{
    OSStatus errorCode = errSecParam;
    require(signatureSize > 0, fail);

    errorCode = SecKeyDigestAndVerify(publicKey, publicKeyAlgId,
                                      dataToHash, amountToHash,
                                      (uint8_t*)signatureStart, signatureSize);
    require_noerr_quiet(errorCode, fail);
    return true;

fail:
    ; // Semicolon works around compiler issue that won't recognize a declaration directly after a label

    // fallback to potentially fix signatures with missing zero-byte padding.
    // worst-case is that both integers get zero-padded, plus size of each integer and sequence size increases by 1
    size_t replacementSignatureLen = signatureSize + 5;
    uint8_t *replacementSignature = malloc(replacementSignatureLen);
    require_quiet(replacementSignature, fail2);

    uint8_t *replacementSignaturePtr = replacementSignature;
    SecRecreateSignatureWithAlgId(publicKey, publicKeyAlgId, signatureStart, signatureSize, &replacementSignaturePtr, &replacementSignatureLen);
    require_quiet(replacementSignaturePtr, fail2);

    require_noerr_quiet(SecKeyDigestAndVerify(publicKey, publicKeyAlgId, dataToHash, amountToHash, replacementSignaturePtr, replacementSignatureLen), fail2);

    free(replacementSignature);
    return true;

fail2:
    if (replacementSignature) {
        free(replacementSignature);
    }
    if (error) {
        *error = SecCreateSignatureVerificationError(errorCode, CFSTR("Unable to verify signature"));
    }
    return false;
}
