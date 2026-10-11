// single block aes for ccmp, cmac and key wrap, a one block cbc with a zero iv is ecb
#include "pd80211_compat.h"
#include <crypto/aes.h>

int
AES_Setkey(AES_CTX *ctx, const uint8_t *key, int len)
{
	if (aes_encrypt_key(key, len, &ctx->enc) != 0)
		return -1;
	if (aes_decrypt_key(key, len, &ctx->dec) != 0)
		return -1;
	return 0;
}

void
AES_Encrypt(AES_CTX *ctx, const uint8_t *in, uint8_t *out)
{
	static const uint8_t iv[16];

	aes_encrypt_cbc(in, iv, 1, out, &ctx->enc);
}

void
AES_Decrypt(AES_CTX *ctx, const uint8_t *in, uint8_t *out)
{
	static const uint8_t iv[16];

	aes_decrypt_cbc(in, iv, 1, out, &ctx->dec);
}
