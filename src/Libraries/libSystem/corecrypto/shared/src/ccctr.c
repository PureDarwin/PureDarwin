#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>

int ccmode_ctr_init(const struct ccmode_ctr* ctr, ccctr_ctx* _ctx, size_t rawkey_len, const void* rawkey, const void* iv) {
	CC_STUB_ERR();
};

int ccmode_ctr_crypt(ccctr_ctx* ctx, size_t nbytes, const void* in, void* out) {
	CC_STUB_ERR();
};

int ccmode_ctr_setctr(const struct ccmode_ctr* ctr, ccctr_ctx* ctx, const void* iv) {
	CC_STUB_ERR();
};
