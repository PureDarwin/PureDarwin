#include <corecrypto/ccpbkdf2.h>
#include <corecrypto/cchmac.h>
#include <corecrypto/cc_priv.h>
#include <string.h>

// the largest digest, sha512
#define PBKDF2_MAX_HLEN 64

// PBKDF2 (RFC 8018 section 5.2): each output block is the XOR of the iterated HMACs of salt || block number
int ccpbkdf2_hmac(const struct ccdigest_info *di,
                   size_t passwordLen, const void *password,
                   size_t saltLen, const void *salt,
                   size_t iterations,
                   size_t dkLen, void *dk) {
	size_t hlen = di->output_size;
	uint8_t u[PBKDF2_MAX_HLEN], t[PBKDF2_MAX_HLEN];
	uint8_t *out = dk;

	if (iterations == 0 || (dkLen && dk == NULL) || hlen > sizeof(u))
		return -1;

	for (uint32_t block = 1; dkLen > 0; block++) {
		uint8_t counter[4] = { block >> 24, block >> 16, block >> 8, block };
		size_t take = dkLen < hlen ? dkLen : hlen;

		cchmac_di_decl(di, ctx);
		cchmac_init(di, ctx, passwordLen, password);
		cchmac_update(di, ctx, saltLen, salt);
		cchmac_update(di, ctx, sizeof(counter), counter);
		cchmac_final(di, ctx, u);
		cchmac_di_clear(di, ctx);
		memcpy(t, u, hlen);

		for (size_t i = 1; i < iterations; i++) {
			cchmac(di, passwordLen, password, hlen, u, u);
			for (size_t k = 0; k < hlen; k++)
				t[k] ^= u[k];
		}
		memcpy(out, t, take);
		out += take;
		dkLen -= take;
	}
	cc_clear(sizeof(u), u);
	cc_clear(sizeof(t), t);
	return 0;
}
