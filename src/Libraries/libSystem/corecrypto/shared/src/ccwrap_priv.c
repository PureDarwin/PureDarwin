#include <corecrypto/ccwrap_priv.h>
#include <stdio.h>

int ccwrap_auth_decrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t wrappedLen, const void* wrappedKey, size_t* unwrappedLen, void* unwrappedKey, const void* iv) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

int ccwrap_auth_encrypt_withiv(const struct ccmode_ecb* mode, ccecb_ctx* ctx, size_t unwrappedLen, const void* unwrappedKey, size_t* wrappedLen, void* wrappedKey, const void* iv) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

size_t ccwrap_unwrapped_size(size_t wrappedLen) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 0;
};

size_t ccwrap_wrapped_size(size_t unwrappedLen) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 0;
};
