#include <corecrypto/ccmode_factory.h>

int ccmode_cbc_init(const struct ccmode_cbc* cbc, cccbc_ctx* _ctx, size_t rawkey_len, const void* rawkey) {
	int status = CCERR_OK;
	struct _ccmode_cbc_key* ctx = (struct _ccmode_cbc_key*)_ctx;
	ctx->ecb = cbc->custom;

	ccecb_ctx* ecb_ctx = (ccecb_ctx*)((char*)ctx->u + ccn_sizeof_size(ctx->ecb->block_size));
	if ((status = ctx->ecb->init(ctx->ecb, ecb_ctx, rawkey_len, rawkey)) != CCERR_OK)
		goto out;

	// `_ccmode_cbc_key` includes space for a single block in the context, which i *think* is supposed to be scratch space.
	cc_zero(ccn_sizeof_size(ctx->ecb->block_size), ctx->u);

out:
	return status;
};

int ccmode_cbc_encrypt(const cccbc_ctx* _ctx, cccbc_iv* _iv, size_t nblocks, const void* in, void* out) {
	int status = CCERR_OK;
	struct _ccmode_cbc_key* ctx = (struct _ccmode_cbc_key*)_ctx;
	ccecb_ctx* ecb_ctx = (ccecb_ctx*)((char*)ctx->u + ccn_sizeof_size(ctx->ecb->block_size));
	const uint8_t* input = in;
	uint8_t* output = out;
	uint8_t* scratch_space = (uint8_t*)ctx->u;
	uint8_t* iv = (uint8_t*)_iv;

	for (size_t i = 0; i < nblocks; ++i) {
		cc_xor(ctx->ecb->block_size, scratch_space, input, iv);

		if ((status = ctx->ecb->ecb(ecb_ctx, 1, scratch_space, output)) != CCERR_OK)
			goto out;

		cc_copy(ctx->ecb->block_size, iv, output);

		input += ctx->ecb->block_size;
		output += ctx->ecb->block_size;
	}

out:
	return status;
};

int ccmode_cbc_decrypt(const cccbc_ctx* _ctx, cccbc_iv* _iv, size_t nblocks, const void* in, void* out) {
	int status = CCERR_OK;
	struct _ccmode_cbc_key* ctx = (struct _ccmode_cbc_key*)_ctx;
	ccecb_ctx* ecb_ctx = (ccecb_ctx*)((char*)ctx->u + ccn_sizeof_size(ctx->ecb->block_size));
	const uint8_t* input = in;
	uint8_t* output = out;
	uint8_t* scratch_space = (uint8_t*)ctx->u;
	uint8_t* iv = (uint8_t*)_iv;

	for (size_t i = 0; i < nblocks; ++i) {
		if ((status = ctx->ecb->ecb(ecb_ctx, 1, input, scratch_space)) != CCERR_OK)
			goto out;

		cc_xor(ctx->ecb->block_size, output, scratch_space, iv);

		cc_copy(ctx->ecb->block_size, iv, input);

		input += ctx->ecb->block_size;
		output += ctx->ecb->block_size;
	}

out:
	return status;
};
