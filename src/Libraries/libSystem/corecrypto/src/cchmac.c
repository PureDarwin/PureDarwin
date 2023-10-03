#include <corecrypto/cchmac.h>
#include <corecrypto/cc_priv.h>
#include <stdio.h>

void cchmac_init(const struct ccdigest_info *di, cchmac_ctx_t ctx,
                 size_t key_len, const void *key) {

	if (key_len > di->block_size)
	{
		// "keys longer than blocksize are shortened"
		ccdigest(di, key_len, key, cchmac_data(di, ctx));
		key_len = di->block_size;
	}
	else
	{
		memcpy(cchmac_data(di, ctx), key, key_len);

		// "keys shorter than blocksize are zero-padded"
		if (key_len < di->block_size)
			memset(cchmac_data(di, ctx) + key_len, 0, di->block_size - key_len);
	}

	for (int i = 0; i < di->block_size; i++)
		cchmac_data(di, ctx)[i] ^= 0x5c; // XOR opad

	// Initialize outer state digest context
	ccdigest_copy_state(di, cchmac_ostate32(di, ctx), di->initial_state);
	// H(K' XOR opad)
	di->compress((struct ccdigest_state*) cchmac_ostate32(di, ctx), 1, cchmac_data(di, ctx));

	for (int i = 0; i < di->block_size; i++)
	{
		uint8_t* b = cchmac_data(di, ctx) + i;
		*b ^= 0x5c; // unXOR opad
		*b ^= 0x36; // XOR ipad
	}

	// Initialize inner state digest context
	ccdigest_copy_state(di, cchmac_istate32(di, ctx), di->initial_state);

	// H(K' XOR ipad)
	di->compress((struct ccdigest_state*) cchmac_istate32(di, ctx), 1, cchmac_data(di, ctx));

	// Prepare for hashing the message
	cchmac_nbits(di, ctx) = di->block_size * 8;
	cchmac_num(di, ctx) = 0;
}

void cchmac_update(const struct ccdigest_info *di, cchmac_ctx_t ctx,
                   size_t data_len, const void *data) {

	// Updates istate
	ccdigest_update(di, cchmac_digest_ctx(di, ctx),
		data_len, data);
}

void cchmac_final(const struct ccdigest_info *di, cchmac_ctx_t ctx,
                  unsigned char *mac) {

	// Finalize H((K' XOR ipad) || message)
	ccdigest_final(di, cchmac_digest_ctx(di, ctx),
		cchmac_data(di, ctx));

	// H(ostate || istate)
	// istate <- ostate
	ccdigest_copy_state(di, cchmac_istate32(di, ctx),
		cchmac_ostate32(di, ctx));

	// istate hash is in data
	// this makes the next ccdigest_final call process it
	cchmac_num(di, ctx) = di->output_size;
	cchmac_nbits(di, ctx) = di->block_size * 8;

	ccdigest_final(di, cchmac_digest_ctx(di, ctx), mac);
}

void cchmac(const struct ccdigest_info *di, size_t key_len,
            const void *key, size_t data_len, const void *data,
            unsigned char *mac) {

	cchmac_di_decl(di, ctx);
	cchmac_init(di, ctx, key_len, key);
	cchmac_update(di, ctx, data_len, data);
	cchmac_final(di, ctx, mac);
	cchmac_di_clear(di, ctx);
}

int cchmac_test(const struct cchmac_test_input *input);
int cchmac_test_chunks(const struct cchmac_test_input *input, size_t chunk_size) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 0;
}
