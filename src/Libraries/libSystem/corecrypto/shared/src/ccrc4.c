#include <corecrypto/ccrc4.h>
#include <stdio.h>

const struct ccrc4_info *ccrc4(void) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

int ccrc4_test(const struct ccrc4_info *rc4, const struct ccrc4_vector *v) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 0;
}

typedef uint32_t eay_RC4_INT;

typedef struct eay_rc4_key_st
{
	eay_RC4_INT x,y;
	eay_RC4_INT data[256];
} eay_RC4_KEY;

static void eay_RC4(ccrc4_ctx *skey, unsigned long len, const void *in, void *out) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

static void eay_RC4_set_key(ccrc4_ctx *skey, size_t keylen, const void *keydata) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

const struct ccrc4_info ccrc4_eay = {
    .size = sizeof(eay_RC4_KEY),
    .init = eay_RC4_set_key,
    .crypt = eay_RC4,
};
