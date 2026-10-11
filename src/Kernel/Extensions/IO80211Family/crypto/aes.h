// openbsd's aes interface over libkern's
#ifndef PD80211_AES_H
#define PD80211_AES_H
#include <libkern/crypto/aes.h>

typedef struct {
	aes_encrypt_ctx	enc;
	aes_decrypt_ctx	dec;
} AES_CTX;

int	AES_Setkey(AES_CTX *, const uint8_t *, int);
void	AES_Encrypt(AES_CTX *, const uint8_t *, uint8_t *);
void	AES_Decrypt(AES_CTX *, const uint8_t *, uint8_t *);
#endif
