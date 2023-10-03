#ifndef _CORECRYPTO_CCCHACHA20POLY1305_H_
#define _CORECRYPTO_CCCHACHA20POLY1305_H_

#include <stddef.h>
#include <stdint.h>

#define CCCHACHA20_KEY_NBYTES 0
#define CCCHACHA20_NONCE_NBYTES 0
#define CCPOLY1305_TAG_NBYTES 0

void *ccchacha20poly1305_info();
int ccchacha20(const void *key, const void *nonce, uint32_t counter, size_t dataInLength, const void *dataIn, void *dataOut);
int ccchacha20poly1305_decrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, const void *tagIn);
int ccchacha20poly1305_encrypt_oneshot(void *ccchacha20poly1305_info_unknown, const void *key, const void *iv, size_t aDataLen, const void *aData, size_t dataInLength, const void *dataIn, void *dataOut, void *tagOut);

#endif // _CORECRYPTO_CCCHACHA20POLY1305_H_