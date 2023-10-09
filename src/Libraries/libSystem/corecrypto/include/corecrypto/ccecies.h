#ifndef _CORECRYPTO_CCECIES_H_
#define _CORECRYPTO_CCECIES_H_

#include <corecrypto/cc.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccmode.h>

#define ECIES_EPH_PUBKEY_IN_SHAREDINFO1      1
#define ECIES_EXPORT_PUB_STANDARD            2
#define ECIES_EXPORT_PUB_COMPACT             4
#define ECIES_LEGACY_IV                      8 // is this right? someone please check

typedef struct ccecies_gcm {
	const struct ccdigest_info *di;
	struct ccrng_state *rng;
	const struct ccmode_gcm *gcm;
	uint32_t key_length;
	uint32_t mac_length;
	uint32_t options;
} *ccecies_gcm_t;

CC_NONNULL_TU((1)) CC_NONNULL((2))
size_t ccecies_encrypt_gcm_ciphertext_size(ccec_pub_ctx_t public_key,
		ccecies_gcm_t ecies, size_t plaintext_len);

CC_NONNULL_TU((1)) CC_NONNULL((2, 4, 9, 10))
int
ccecies_encrypt_gcm( ccec_pub_ctx_t public_key,
		const ccecies_gcm_t ecies,
		size_t plaintext_len,
		const uint8_t *plaintext,
		size_t sharedinfo1_byte_len,
		const void *sharedinfo_1,
		size_t sharedinfo2_byte_len,
		const void *sharedinfo_2,
		size_t *encrypted_blob_len,
		uint8_t *encrypted_blob);

CC_NONNULL_TU((1)) CC_NONNULL((2, 4, 9, 10))
int
ccecies_decrypt_gcm(ccec_full_ctx_t full_key,
		const ccecies_gcm_t ecies,
		size_t encrypted_blob_len,
		const uint8_t *encrypted_blob,
		size_t sharedinfo1_byte_len,
		const void *sharedinfo_1,
		size_t sharedinfo2_byte_len,
		const void *sharedinfo_2,
		size_t *plaintext_len,
		uint8_t *plaintext);

CC_NONNULL_TU((1)) CC_NONNULL((2))
size_t
ccecies_decrypt_gcm_plaintext_size(ccec_full_ctx_t full_key,
		ccecies_gcm_t ecies,
		size_t ciphertext_len);

CC_NONNULL((1, 2, 3, 4))
void
ccecies_encrypt_gcm_setup(ccecies_gcm_t ecies,
		const struct ccdigest_info *di,
		struct ccrng_state *rng,
		const struct ccmode_gcm *aes_gcm_enc,
		uint32_t cipher_key_size,
		uint32_t mac_tag_size,
		uint32_t options);

#endif

