#include <corecrypto/ccmode_factory.h>
#include <corecrypto/private/ccstubs.h>

int ccmode_xts_init(const struct ccmode_xts* xts, ccxts_ctx* _ctx, size_t key_nbytes, const void* data_key, const void* tweak_key) {
	CC_STUB_ERR();
};

void ccmode_xts_key_sched(const struct ccmode_xts* xts, ccxts_ctx* _ctx, size_t key_nbytes, const void* data_key, const void* tweak_key) {
	CC_STUB_VOID();
};

void* ccmode_xts_crypt(const ccxts_ctx* _ctx, ccxts_tweak* tweak, size_t nblocks, const void* in, void* out) {
	CC_STUB(NULL);
};

int ccmode_xts_set_tweak(const ccxts_ctx* _ctx, ccxts_tweak* tweak, const void* iv) {
	CC_STUB_ERR();
};
