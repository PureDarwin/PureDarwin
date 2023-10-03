#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>

int ccmode_cfb_init(const struct ccmode_cfb* cfb, cccfb_ctx* _ctx, size_t rawkey_len, const void* rawkey, const void* iv) {
	CC_STUB_ERR();
};

int ccmode_cfb_decrypt(cccfb_ctx* _ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};

int ccmode_cfb_encrypt(cccfb_ctx* _ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};
