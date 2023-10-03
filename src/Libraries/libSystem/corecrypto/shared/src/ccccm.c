#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>

int ccmode_ccm_init(const struct ccmode_ccm* ccm, ccccm_ctx* _ctx, size_t rawkey_len, const void* rawkey) {
	CC_STUB_ERR();
};

int ccmode_ccm_set_iv(ccccm_ctx* _ctx, ccccm_nonce* _nonce_ctx, size_t nonce_len, const void* nonce, size_t mac_size, size_t auth_len, size_t data_len) {
	CC_STUB_ERR();
};

int ccmode_ccm_cbcmac(ccccm_ctx* _ctx, ccccm_nonce* _nonce_ctx, size_t nbytes, const void* in) {
	CC_STUB_ERR();
};

int ccmode_ccm_decrypt(ccccm_ctx *_ctx, ccccm_nonce* _nonce_ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};

int ccmode_ccm_encrypt(ccccm_ctx *_ctx, ccccm_nonce* _nonce_ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};

int ccmode_ccm_finalize(ccccm_ctx* _ctx, ccccm_nonce* _nonce_ctx, void* mac) {
	CC_STUB_ERR();
};

int ccmode_ccm_reset(ccccm_ctx* _ctx, ccccm_nonce* _nonce_ctx) {
	CC_STUB_ERR();
};
