#include <corecrypto/cccmac.h>
#include <stdio.h>

int cccmac_init(const struct ccmode_cbc *cbc, cccmac_ctx_t ctx, int key_length, const void *key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void cccmac_block_update(const struct ccmode_cbc *cbc, cccmac_ctx_t cmac,
                                       size_t nblocks, const void *data) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}


void cccmac_final(const struct ccmode_cbc *cbc, cccmac_ctx_t ctx,
                  size_t nbytes, const void *in, void *out) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void cccmac(const struct ccmode_cbc *cbc, const void *key,
            size_t data_len, const void *data,
            void *mac) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void cccmac_one_shot_generate(const struct ccmode_cbc * arg0,
	int key_size,
	const void *key,
	size_t dataLength,
	const uint8_t *data,
	int block_size,
	void *macOut) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void cccmac_update(cccmac_ctx_t ctx, size_t dataLength, const void *data) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void cccmac_final_generate(cccmac_ctx_t ctx, size_t dataLength, void *macOut) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

const struct ccmode_cbc* cccmac_cbc(cccmac_ctx_t ctx) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
