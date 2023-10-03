#include <corecrypto/ccchacha20poly1305.h>
#include <stdio.h>
#include <stdlib.h>

int ccchacha20(const void *key, const void *nonce, uint32_t counter, size_t dataInLength, const void *dataIn, void *dataOut) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

void *ccchacha20poly1305_info() {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

int ccchacha20poly1305_decrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, const void *tagIn) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

int ccchacha20poly1305_encrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, void *tagOut) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}