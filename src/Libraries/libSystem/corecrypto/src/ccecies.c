#include <corecrypto/ccecies.h>
#include <stdio.h>

size_t ccecies_encrypt_gcm_ciphertext_size(ccec_pub_ctx_t public_key,
		ccecies_gcm_t ecies, size_t plaintext_len) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 1;
}

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
		uint8_t *encrypted_blob) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

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
		uint8_t *plaintext) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

size_t
ccecies_decrypt_gcm_plaintext_size(ccec_full_ctx_t full_key,
		ccecies_gcm_t ecies,
		size_t ciphertext_len) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 1;
}

void
ccecies_encrypt_gcm_setup(ccecies_gcm_t ecies,
		const struct ccdigest_info *di,
		struct ccrng_state *rng,
		const struct ccmode_gcm *aes_gcm_enc,
		uint32_t cipher_key_size,
		uint32_t mac_tag_size,
		uint32_t options) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
