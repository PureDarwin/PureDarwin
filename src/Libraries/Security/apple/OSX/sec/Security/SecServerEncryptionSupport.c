//
//  SecServerEncryptionSupport.c
//  sec
//
//

#include <TargetConditionals.h>

#include <AssertMacros.h>
#include <Security/SecServerEncryptionSupport.h>
#include <Security/SecECKeyPriv.h>

#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecKeyInternal.h>

#include <Security/SecBasePriv.h>

#include <corecrypto/ccsha2.h>
#include <corecrypto/ccecies.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccder.h>

//
// We assume that SecKey is set up for this to work.
// Specifically ccrng_seckey needs to be initialized
//
// As it happens we work in terms of SecKeys at the
// higher level, so we're good.
//

const uint32_t kBlobCipherKeySize = CCAES_KEY_SIZE_128;
const uint32_t kBlobMacSize = 16;

static void InitServerECIES(ccecies_gcm_t ecies, const struct ccmode_gcm *gcm_mode)
{
    ccecies_encrypt_gcm_setup(ecies,
                              ccsha256_di(),
                              ccrng_seckey,
                              gcm_mode,
                              kBlobCipherKeySize,
                              kBlobMacSize,
                              ECIES_EXPORT_PUB_STANDARD
                              +ECIES_EPH_PUBKEY_IN_SHAREDINFO1
                              +ECIES_LEGACY_IV);
}

//
// Der Encode
//

//
//    EncryptedPayloadToServerVersion ::= INTEGER {v1(1)}
//
//    EncryptedPayloadToServer ::= SEQUENCE {
//        version EncryptedPayloadToServerVersion DEFAULT v1,
//        ephemeralPublicKey OCTET STRING,
//        GCMEncryptedData OCTET STRING,
//        GCMTag OCTET STRING
//    }


enum {
    SERVER_BLOB_ENCRYPTED_DATA = 0
};

static size_t sizeof_implicit_nocopy(ccder_tag implicit_tag, size_t space)
{
    return ccder_sizeof(implicit_tag, space);
}

static uint8_t *encode_implicit_nocopy(ccder_tag implicit_tag, size_t size, uint8_t**start, const uint8_t *der, uint8_t *der_end)
{
    if (start == NULL)
        return NULL;

    return ccder_encode_tl(implicit_tag, size, der,
                           (*start = ccder_encode_body_nocopy(size, der, der_end)));
}

static uint8_t *encode_octect_string_nocopy(size_t size, uint8_t**start, const uint8_t *der, uint8_t *der_end)
{
    return encode_implicit_nocopy(CCDER_OCTET_STRING, size, start, der, der_end);
}

static size_t sizeof_server_blob(size_t public_key_size,
                                 size_t ciphertext_size,
                                 size_t verifier_size)
{
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        sizeof_implicit_nocopy(CCDER_OCTET_STRING, public_key_size) +
                        sizeof_implicit_nocopy(CCDER_OCTET_STRING, ciphertext_size) +
                        sizeof_implicit_nocopy(CCDER_OCTET_STRING, verifier_size));
}


static uint8_t* encode_empty_server_blob_for(size_t public_key_size, uint8_t **public_key_start,
                                             size_t ciphertext_size, uint8_t **ciphertext,
                                             size_t verifier_size, uint8_t **verifier,
                                             const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
              encode_octect_string_nocopy(public_key_size, public_key_start, der,
              encode_octect_string_nocopy(ciphertext_size, ciphertext, der,
              encode_octect_string_nocopy(verifier_size, verifier, der, der_end))));
}

static const uint8_t* decode_octect_string(size_t* size, const uint8_t ** start, const uint8_t *der, const uint8_t* der_end)
{
    if (size == NULL)
        return NULL;

    der = ccder_decode_tl(CCDER_OCTET_STRING, size, der, der_end);

    if (der && start) {
        *start = der;
        der += *size;
    }

    return der;
}

static const uint8_t* decode_server_blob(size_t *public_key_size, const uint8_t **public_key_start,
                                         size_t *ciphertext_size, const uint8_t **ciphertext,
                                         size_t *verifier_size, const uint8_t **verifier,
                                         CFErrorRef *error, const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *sequence_end;
    der = ccder_decode_sequence_tl(&sequence_end, der, der_end);

    if (der_end != sequence_end)
        der = NULL;

    der = decode_octect_string(public_key_size, public_key_start, der, der_end);
    der = decode_octect_string(ciphertext_size, ciphertext, der, der_end);
    der = decode_octect_string(verifier_size, verifier, der, der_end);

    return der;
}

static CFMutableDataRef CreateDataForEncodeEncryptedBlobOf(ccec_pub_ctx_t public_key,
                                                           size_t public_key_size, uint8_t **public_key_start,
                                                           size_t ciphertext_size, uint8_t **ciphertext,
                                                           size_t verifier_size, uint8_t **verifier,
                                                           CFErrorRef *error)
{
    CFMutableDataRef result = NULL;
    CFMutableDataRef allocated = CFDataCreateMutableWithScratch(kCFAllocatorDefault, sizeof_server_blob(public_key_size, ciphertext_size, verifier_size));

    require_action_quiet(allocated, fail, SecError(errSecAllocate, error, CFSTR("failed to create data")));

    uint8_t *der = CFDataGetMutableBytePtr(allocated);
    uint8_t *der_end = der + CFDataGetLength(allocated);

    der = encode_empty_server_blob_for(public_key_size, public_key_start,
                                       ciphertext_size, ciphertext,
                                       verifier_size, verifier,
                                       der, der_end);

    require_action_quiet(der, fail, SecError(errSecParam, error, CFSTR("Encoding failed")));

    CFRetainAssign(result, allocated);

fail:
    CFReleaseNull(allocated);
    return result;
}

static bool ParseAndFindEncryptedData(CFDataRef blob,
                                      size_t *public_key_size, const uint8_t **public_key_start,
                                      size_t *ciphertext_size, const uint8_t **ciphertext,
                                      size_t *verifier_size, const uint8_t **verifier,
                                      CFErrorRef *error)
{
    bool success = false;
    const uint8_t *der = CFDataGetBytePtr(blob);
    const uint8_t *der_end = der + CFDataGetLength(blob);
    der = decode_server_blob(public_key_size, public_key_start,
                             ciphertext_size, ciphertext,
                             verifier_size, verifier,
                             error, der, der_end);

    require_action_quiet(der == der_end, fail, SecError(errSecParam, error, CFSTR("Blob failed to decode")));

    success = true;
fail:
    return success;

}

static size_t ccec_x963_pub_export_size(ccec_pub_ctx_t key)
{
    return ccec_x963_export_size(0,key);
}

CFDataRef SecCopyEncryptedToServerKey(SecKeyRef publicKey, CFDataRef dataToEncrypt, CFErrorRef *error)
{
    __block CFDataRef result = NULL;


    SecECDoWithPubKey(publicKey, error, ^(ccec_pub_ctx_t public_key) {
        CFMutableDataRef encrypted = NULL;

        struct ccecies_gcm ecies_encrypt;
        InitServerECIES(&ecies_encrypt, ccaes_gcm_encrypt_mode());

        size_t plain_size = CFDataGetLength(dataToEncrypt);
        size_t encrypted_size = ccecies_encrypt_gcm_ciphertext_size(public_key, &ecies_encrypt, plain_size);

        CFMutableDataRef encryption_temp = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encrypted_size);
        require_action_quiet(encryption_temp, errout, SecError(errSecAllocate, error, CFSTR("failed to create data")));

        uint8_t *encryption_buffer = (uint8_t *) CFDataGetMutableBytePtr(encryption_temp);

        int encrypt_result = ccecies_encrypt_gcm(public_key,
                                                 &ecies_encrypt,
                                                 plain_size, CFDataGetBytePtr(dataToEncrypt),
                                                 0, NULL,
                                                 0, NULL,
                                                 &encrypted_size, encryption_buffer);


        size_t public_key_size = ccec_x963_pub_export_size(public_key);
        uint8_t *public_key_data = NULL;
        size_t ciphertext_size = plain_size;
        uint8_t *ciphertext =  NULL;
        size_t tag_size = kBlobMacSize;
        uint8_t *tag = NULL;

        require_action_quiet(public_key_size + ciphertext_size + tag_size == encrypted_size, errout, SecError(errSecInternal, error, CFSTR("Allocation mismatch")));

        encrypted = CreateDataForEncodeEncryptedBlobOf(public_key,
                                                       public_key_size, &public_key_data,
                                                       ciphertext_size, &ciphertext,
                                                       tag_size, &tag,
                                                       error);
        require_quiet(encrypted, errout);

        //
        // Core crypto SPI a work in progress, until then we copy.
        //

        memcpy(public_key_data, encryption_buffer, public_key_size);
        memcpy(ciphertext, encryption_buffer + public_key_size, ciphertext_size);
        memcpy(tag, encryption_buffer + public_key_size + ciphertext_size, tag_size);

        require_action_quiet(encrypt_result == 0, errout, SecError(errSecBadReq, error, CFSTR("ccecies_encrypt_gcm failed %d"), encrypt_result));

        CFRetainAssign(result, encrypted);
    errout:
        CFReleaseSafe(encryption_temp);
        CFReleaseSafe(encrypted);
    });


    return result;
}


CFDataRef SecCopyDecryptedForServer(SecKeyRef serverFullKey, CFDataRef blob, CFErrorRef* error)
{
    __block CFDataRef result = NULL;

    SecECDoWithFullKey(serverFullKey, error, ^(ccec_full_ctx_t private_key) {
        CFMutableDataRef plain = NULL;
        CFMutableDataRef crypto_buffer = NULL;
        size_t encrypted_size;

        size_t plain_size;

        struct ccecies_gcm ecies_decrypt;
        InitServerECIES(&ecies_decrypt, ccaes_gcm_decrypt_mode());
        size_t public_key_size;
        const uint8_t *public_key_start = NULL;
        size_t ciphertext_size;
        const uint8_t *ciphertext = NULL;
        size_t verifier_size;
        const uint8_t *verifier = NULL;

        require_quiet(ParseAndFindEncryptedData(blob,
                                                &public_key_size, &public_key_start,
                                                &ciphertext_size, &ciphertext,
                                                &verifier_size, &verifier,
                                                error), errout);

        require_quiet(public_key_start, errout); // Silence analyzer, shouldn't ever happen.
        require_quiet(ciphertext, errout); // Silence analyzer, shouldn't ever happen.
        require_quiet(verifier, errout); // Silence analyzer, shouldn't ever happen.

        encrypted_size = public_key_size + ciphertext_size + verifier_size;
        crypto_buffer = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encrypted_size);
        require_action_quiet(crypto_buffer, errout, SecError(errSecAllocate, error, CFSTR("failed to create data")));

        uint8_t *crypto_buffer_ptr = CFDataGetMutableBytePtr(crypto_buffer);
        memcpy(crypto_buffer_ptr, public_key_start, public_key_size);
        memcpy(crypto_buffer_ptr + public_key_size, ciphertext, ciphertext_size);
        memcpy(crypto_buffer_ptr + public_key_size + ciphertext_size, verifier, verifier_size);


        plain_size = ccecies_decrypt_gcm_plaintext_size(private_key, &ecies_decrypt, encrypted_size);
        plain = CFDataCreateMutableWithScratch(kCFAllocatorDefault, plain_size);

        int decrypt_result = ccecies_decrypt_gcm(private_key,
                                                 &ecies_decrypt,
                                                 encrypted_size, crypto_buffer_ptr,
                                                 0, NULL,
                                                 0, NULL,
                                                 &plain_size, CFDataGetMutableBytePtr(plain));

        require_action_quiet(decrypt_result == 0, errout, SecError(errSecBadReq, error, CFSTR("ccecies_decrypt_gcm failed %d"), decrypt_result));

        CFRetainAssign(result, plain);
        
    errout:
        CFReleaseSafe(plain);
        CFReleaseSafe(crypto_buffer);
    });

    return result;
}

#if TARGET_OS_OSX
#include <Security/SecTrustInternal.h>
#endif

CFDataRef SecCopyEncryptedToServer(SecTrustRef trustedEvaluation, CFDataRef dataToEncrypt, CFErrorRef *error)
{
    CFDataRef result = NULL;
    SecKeyRef trustKey = SecTrustCopyKey(trustedEvaluation);

    require_action_quiet(trustKey, fail,
                         SecError(errSecInteractionNotAllowed, error, CFSTR("Failed to get key out of trust ref, was it evaluated?")));


    result = SecCopyEncryptedToServerKey(trustKey, dataToEncrypt, error);

fail:
    CFReleaseNull(trustKey);
    return result;
}
