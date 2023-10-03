#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>

int ccmode_cfb8_init(const struct ccmode_cfb8* cfb8, cccfb8_ctx* _ctx, size_t rawkey_len, const void* rawkey, const void* iv) {
	CC_STUB_ERR();
};

int ccmode_cfb8_decrypt(cccfb8_ctx* _ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};

int ccmode_cfb8_encrypt(cccfb8_ctx* _ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};
