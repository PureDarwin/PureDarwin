#include <corecrypto/ccwrap_priv.h>
#include <corecrypto/ccstubs.h>

int ccwrap_auth_decrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t wrappedLen, const void* wrappedKey, size_t* unwrappedLen, void* unwrappedKey, const void* iv) {
	CC_STUB_ERR();
};

int ccwrap_auth_encrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t unwrappedLen, const void* unwrappedKey, size_t* wrappedLen, void* wrappedKey, const void* iv) {
	CC_STUB_ERR();
};

size_t ccwrap_unwrapped_size(size_t wrappedLen) {
	CC_STUB_ERR();
};

size_t ccwrap_wrapped_size(size_t unwrappedLen) {
	CC_STUB_ERR();
};
