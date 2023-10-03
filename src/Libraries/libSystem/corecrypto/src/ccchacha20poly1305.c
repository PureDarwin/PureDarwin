#include <corecrypto/ccchacha20poly1305.h>
#include <corecrypto/ccstubs.h>
#include <stdlib.h>

int ccchacha20(const void *key, const void *nonce, uint32_t counter, size_t dataInLength, const void *dataIn, void *dataOut) {
	CC_STUB(-1);
}

void *ccchacha20poly1305_info() {
	CC_STUB(NULL);
}

int ccchacha20poly1305_decrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, const void *tagIn) {
	CC_STUB_ERR();
}

int ccchacha20poly1305_encrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, void *tagOut) {
	CC_STUB_ERR();
}
