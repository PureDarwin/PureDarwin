#include <corecrypto/private/cc128.h>
#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>
#include <corecrypto/ccmode.h>
#include <string.h>
#include <stdio.h>

#define CCGCM_MODE_INITIALIZE 0
#define CCGCM_MODE_IV         1
#define CCGCM_MODE_AAD        2
#define CCGCM_MODE_CIPHERTEXT 3
#define CCGCM_MODE_FINALIZE   4

#define CTX_U_BUFFER(index) ((uint8_t*)ctx->u + (index * ccn_sizeof_size(ctx->ecb->block_size)))
#define CTX_U_BLOCK(index) ((cc128_t*)CTX_U_BUFFER(index))

CC_INLINE
int perform_block_cipher(struct _ccmode_gcm_key* ctx, const cc128_t* input, cc128_t* output) {
	int status = CCERR_OK;
	uint8_t* temp = CTX_U_BUFFER(4);
	cc128_store_be(input, 16, temp);
	if ((status = ctx->ecb->ecb(ctx->ecb_key, 1, temp, temp)) != CCERR_OK)
		goto out;
	cc128_load_be(16, temp, output);
out:
	return status;
};

int ccmode_gcm_init(const struct ccmode_gcm* gcm, ccgcm_ctx* _ctx, size_t rawkey_len, const void* rawkey) {
	int status = CCERR_OK;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;

	ctx->ecb = gcm->custom;
	ctx->ecb_key = ctx->u + (5 * ccn_sizeof_size(ctx->ecb->block_size));

	if ((status = ctx->ecb->init(ctx->ecb, ctx->ecb_key, rawkey_len, rawkey)) != CCERR_OK)
		goto out;

	ctx->encdec = gcm->encdec;
	ctx->flags = 0;
	ctx->buf_nbytes = 0;
	ctx->aad_nbytes = 0;
	ctx->text_nbytes = 0;
	ctx->state = CCGCM_MODE_INITIALIZE;

	cc_zero(5 * ccn_sizeof_size(ctx->ecb->block_size), CTX_U_BUFFER(0));

	cc_zero(sizeof(ctx->H), ctx->H);
	cc_zero(sizeof(ctx->X), ctx->X);
	cc_zero(sizeof(ctx->Y), ctx->Y);
	cc_zero(sizeof(ctx->Y_0), ctx->Y_0);
	cc_zero(sizeof(ctx->buf), ctx->buf);

	if ((status = perform_block_cipher(ctx, hash_subkey, hash_subkey)) != CCERR_OK)
		goto out;

out:
	return status;
};

int ccmode_gcm_set_iv(ccgcm_ctx* _ctx, size_t iv_size, const void* iv) {
	int status = CCERR_OK;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;
	cc128_t* initial_cb = (cc128_t*)ctx->Y_0;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;
	cc128_t* cache = (cc128_t*)ctx->buf;

	const uint8_t* iv_buf = iv;

	if (ctx->state == CCGCM_MODE_INITIALIZE)
		ctx->state = CCGCM_MODE_IV;

	if (ctx->state != CCGCM_MODE_IV)
		return CCMODE_INVALID_CALL_SEQUENCE;

	for (size_t i = 0; i < iv_size; ++i) {
		// `aad_nbytes` is used as the IV length when we're in IV mode
		const size_t current_block_index = 15 - (ctx->aad_nbytes % 16);

		cache->bytes[current_block_index] = iv_buf[i];

		if (current_block_index == 0) {
			cc128_xor(initial_cb, cache, initial_cb);
			cc128_mul(initial_cb, hash_subkey, initial_cb);
			cc_zero(16, cache->bytes);
		}

		++ctx->aad_nbytes;
	}

	return status;
};

CC_INLINE
int ccgcm_process_iv(struct _ccmode_gcm_key* ctx) {
	int status = CCERR_OK;
	uint8_t* current_iv = CTX_U_BUFFER(0);
	cc128_t* initial_cb = (cc128_t*)ctx->Y_0;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* cb = (cc128_t*)ctx->Y;
	cc128_t* processed_cb = CTX_U_BLOCK(1);

	if (ctx->aad_nbytes == 12) {
		// we only do this when IV is 12 bytes because the only time
		// the IV is incremented for reuse is when it's set through
		// `ccgcm_init_with_iv`
		for (size_t i = 0; i < 12; ++i)
			current_iv[i] = cache->bytes[15 - i];

		cache->bytes[0] = 1;
		cc_copy(16, initial_cb->bytes, cache->bytes);
	} else {
		if (ctx->aad_nbytes % 16 != 0) {
			cc128_xor(initial_cb, cache, initial_cb);
			cc128_mul(initial_cb, hash_subkey, initial_cb);
			cc_zero(16, cache->bytes);
		}

		const size_t iv_bits = ctx->aad_nbytes * 8;

		for (size_t i = 0; i < sizeof(size_t); ++i)
			cache->bytes[i] = (iv_bits >> (i * 8)) & 0xff;

		cc128_xor(initial_cb, cache, initial_cb);
		cc128_mul(initial_cb, hash_subkey, initial_cb);
	}

	cc_zero(16, cache->bytes);

	cc128_lsw_increment(initial_cb, cb);
	if ((status = perform_block_cipher(ctx, cb, processed_cb)) != CCERR_OK)
		return status;

	ctx->aad_nbytes = 0;

	return status;
};

int ccmode_gcm_aad(ccgcm_ctx* _ctx, size_t buf_len, const void* in) {
	int status = CCERR_OK;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;
	const uint8_t* in_buf = in;
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;

	if (ctx->state == CCGCM_MODE_IV) {
		if ((status = ccgcm_process_iv(ctx)) != CCERR_OK)
			goto out;
		ctx->state = CCGCM_MODE_AAD;
	}

	if (ctx->state != CCGCM_MODE_AAD)
		return CCMODE_INVALID_CALL_SEQUENCE;

	for (size_t i = 0; i < buf_len; ++i) {
		const size_t current_block_index = 15 - (ctx->aad_nbytes % 16);

		cache->bytes[current_block_index] = in_buf[i];

		if (current_block_index == 0) {
			cc128_xor(hash, cache, hash);
			cc128_mul(hash, hash_subkey, hash);
			cc_zero(16, cache->bytes);
		}

		++ctx->aad_nbytes;
	}

out:
	return status;
};

CC_INLINE
int ccgcm_process_aad(struct _ccmode_gcm_key* ctx) {
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;

	if (ctx->aad_nbytes % 16 != 0) {
		cc128_xor(hash, cache, hash);
		cc128_mul(hash, hash_subkey, hash);
		cc_zero(16, cache->bytes);
	}

	return CCERR_OK;
};

int ccmode_gcm_encrypt(ccgcm_ctx* _ctx, size_t buf_len, const void* in, void* out) {
	int status = CCERR_OK;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;

	const uint8_t* in_buf = in;
	uint8_t* out_buf = out;
	bool decrypting = ctx->encdec == CCMODE_GCM_DECRYPTOR;
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;
	cc128_t* cb = (cc128_t*)ctx->Y;
	cc128_t* processed_cb = CTX_U_BLOCK(1);

	if (ctx->state == CCGCM_MODE_IV) {
		if ((status = ccgcm_process_iv(ctx)) != CCERR_OK)
			return status;
		ctx->state = CCGCM_MODE_AAD;
	}

	if (ctx->state == CCGCM_MODE_AAD) {
		if ((status = ccgcm_process_aad(ctx)) != CCERR_OK)
			return status;
		ctx->state = CCGCM_MODE_CIPHERTEXT;
	}

	if (ctx->state != CCGCM_MODE_CIPHERTEXT)
		return CCMODE_INVALID_CALL_SEQUENCE;

	for (size_t i = 0; i < buf_len; ++i) {
		const size_t current_block_index = 15 - (ctx->text_nbytes % 16);

		if (decrypting) {
			cache->bytes[current_block_index] = in_buf[i];
		}
		out_buf[i] = in_buf[i] ^ processed_cb->bytes[current_block_index];
		if (!decrypting) {
			cache->bytes[current_block_index] = out_buf[i];
		}

		if (current_block_index == 0) {
			cc128_xor(hash, cache, hash);
			cc128_mul(hash, hash_subkey, hash);
			cc_zero(16, cache->bytes);
		}

		++ctx->text_nbytes;

		if (ctx->text_nbytes % 16 == 0) {
			cc128_lsw_increment(cb, cb);
			if ((status = perform_block_cipher(ctx, cb, processed_cb)) != CCERR_OK)
				return status;
		}
	}

	return status;
};

int ccmode_gcm_decrypt(ccgcm_ctx* _ctx, size_t nbytes, const void* in, void* out) {
	return ccmode_gcm_encrypt(_ctx, nbytes, in, out);
};

CC_INLINE
int ccgcm_process_ciphertext(struct _ccmode_gcm_key* ctx) {
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;

	if (ctx->text_nbytes % 16 != 0) {
		cc128_xor(hash, cache, hash);
		cc128_mul(hash, hash_subkey, hash);
		cc_zero(16, cache->bytes);
	}

	return CCERR_OK;
};

// `input` is assumed to be in normal byte order (big-endian; most significant byte at lowest address)
// `output` is in the same order as `input`
CC_INLINE
int ccgcm_gctr_128(struct _ccmode_gcm_key* ctx, const size_t input_len, const uint8_t* const input, uint8_t* const output) {
	if (input_len == 0)
		return CCERR_OK;

	int status = CCERR_OK;
	const size_t n = (input_len + 15) / 16;
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;
	cc128_t* cb = (cc128_t*)ctx->Y;
	cc128_t* processed_cb = CTX_U_BLOCK(1);
	cc128_t* temp = CTX_U_BLOCK(4);
	cc128_t* initial_cb = (cc128_t*)ctx->Y_0;
	size_t current_len = input_len;
	const uint8_t* current_input = input;
	uint8_t* current_output = output;

	cc_copy(16, cb->bytes, initial_cb->bytes);

	for (size_t i = 0; i < n - 1; ++i) {
		cc128_load_be(current_len, current_input, cache);
		if ((status = perform_block_cipher(ctx, cb, processed_cb)) != CCERR_OK)
			return status;
		cc128_xor(cache, processed_cb, cache);
		cc128_store_be(cache, current_len, current_output);

		cc128_lsw_increment(cb, cb);

		current_len -= 16;
		current_input += 16;
		current_output += 16;
	}

	const size_t final_block_bytes = input_len - ((n - 1) * 8);
	cc128_load_be(current_len, current_input, cache);

	if ((status = perform_block_cipher(ctx, cb, processed_cb)) != CCERR_OK)
		return status;
	cc128_msbits(processed_cb, final_block_bytes * 8, temp);
	cc128_xor(cache, temp, cache);
	cc128_store_be(cache, current_len, current_output);

	return status;
};

int ccmode_gcm_finalize(ccgcm_ctx* _ctx, size_t tag_size, void* tag) {
	int status = CCERR_OK;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;
	cc128_t* cache = (cc128_t*)ctx->buf;
	cc128_t* hash = (cc128_t*)ctx->X;
	cc128_t* hash_subkey = (cc128_t*)ctx->H;
	uint8_t* hash_buf = CTX_U_BUFFER(2);
	uint8_t* result_buf = CTX_U_BUFFER(3);

	if (ctx->state == CCGCM_MODE_IV) {
		if ((status = ccgcm_process_iv(ctx)) != CCERR_OK)
			return status;
		ctx->state = CCGCM_MODE_AAD;
	}

	if (ctx->state == CCGCM_MODE_AAD) {
		if ((status = ccgcm_process_aad(ctx)) != CCERR_OK)
			return status;
		ctx->state = CCGCM_MODE_CIPHERTEXT;
	}

	if (ctx->state == CCGCM_MODE_CIPHERTEXT) {
		if ((status = ccgcm_process_ciphertext(ctx)) != CCERR_OK)
			return status;
		ctx->state = CCGCM_MODE_FINALIZE;
	}

	if (ctx->state != CCGCM_MODE_FINALIZE)
		return CCMODE_INVALID_CALL_SEQUENCE;

	const size_t aad_bits = ctx->aad_nbytes * 8;
	const size_t cipher_bits = ctx->text_nbytes * 8;

	uint8_t* tag_buf = tag;

	for (size_t i = 0; i < sizeof(size_t); ++i)
		cache->bytes[8 + i] = (aad_bits >> (i * 8)) & 0xff;

	for (size_t i = 0; i < sizeof(size_t); ++i)
		cache->bytes[i] = (cipher_bits >> (i * 8)) & 0xff;

	cc128_xor(hash, cache, hash);
	cc128_mul(hash, hash_subkey, hash);

	cc128_store_be(hash, 16, hash_buf);
	cc_zero(16, result_buf);

	ccgcm_gctr_128(ctx, 16, hash_buf, result_buf);

	if (ctx->encdec == CCMODE_GCM_DECRYPTOR)
		status = cc_cmp_safe(tag_size < 16 ? tag_size : 16, tag_buf, result_buf) == 0 ? CCERR_OK : CCMODE_INTEGRITY_FAILURE;

	for (size_t i = 0; i < 16 && i < tag_size; ++i)
		tag_buf[i] = result_buf[i];

	return status;
};

int ccmode_gcm_reset(ccgcm_ctx* _ctx) {
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;

	// *don't* zero the ECB key (it's preserved across resets)
	//cc_zero(ccn_sizeof_size(ctx->ecb->size), ctx->ecb_key);

	ctx->buf_nbytes = 0;
	ctx->aad_nbytes = 0;
	ctx->text_nbytes = 0;
	ctx->state = CCGCM_MODE_INITIALIZE;

	// skip the first block because that contains the current IV (which is preserved across resets)
	cc_zero(4 * ccn_sizeof_size(ctx->ecb->block_size), CTX_U_BUFFER(1));

	// *don't* zero the hash subkey (it's preserved across resets)
	//cc_zero(sizeof(ctx->H), ctx->H);
	cc_zero(sizeof(ctx->X), ctx->X);
	cc_zero(sizeof(ctx->Y), ctx->Y);
	cc_zero(sizeof(ctx->Y_0), ctx->Y_0);
	cc_zero(sizeof(ctx->buf), ctx->buf);

	return CCERR_OK;
};

int ccgcm_init_with_iv(const struct ccmode_gcm* mode, ccgcm_ctx* ctx, size_t key_nbytes, const void* key, const void* iv) {
	int status = CCERR_OK;

	if ((status = ccgcm_init(mode, ctx, key_nbytes, key)) != CCERR_OK)
		return status;

	if ((status = ccgcm_set_iv(mode, ctx, 12, iv)) != CCERR_OK)
		return status;

	return 0;
};

int ccgcm_inc_iv(const struct ccmode_gcm* mode, ccgcm_ctx* _ctx, void* iv) {
	uint8_t* iv_buf = iv;
	struct _ccmode_gcm_key* ctx = (struct _ccmode_gcm_key*)_ctx;
	cc128_t* cache = (cc128_t*)ctx->buf;
	uint8_t* current_iv = CTX_U_BUFFER(0);

	cc_zero(16, cache->bytes);

	for (size_t i = 0; i < 12; ++i)
		cache->bytes[15 - i] = current_iv[i];

	for (size_t i = 0; i < 8; ++i) {
		cache->bytes[4 + i] += 1;
		if (cache->bytes[4 + i] != 0)
			break;
	}

	ctx->aad_nbytes = 12;

	ccgcm_process_iv(ctx);

	memcpy(iv_buf, current_iv, 12);

	return 0;
};

int ccgcm_set_iv_legacy(const struct ccmode_gcm *mode, ccgcm_ctx *ctx, size_t iv_nbytes, const void *iv) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
};

int ccgcm_one_shot(const struct ccmode_gcm* mode, size_t key_len, const void* key, size_t iv_len, const void* iv, size_t adata_len, const void* adata, size_t nbytes, const void* in, void* out, size_t tag_len, void* tag) {
	CC_STUB_ERR();
};

int ccgcm_one_shot_legacy(const struct ccmode_gcm* mode, size_t key_len, const void* key, size_t iv_len, const void* iv, size_t adata_len, const void* adata, size_t nbytes, const void* in, void* out, size_t tag_len, void* tag) {
	CC_STUB_ERR();
};
