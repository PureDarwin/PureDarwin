#include <corecrypto/ccec25519.h>
#include <corecrypto/ccstubs.h>

int cccurve25519(ccec25519key shared_secret, ccec25519secretkey private_key, ccec25519pubkey public_key) {
	CC_STUB(-1);
};

int cccurve25519_make_key_pair(struct ccrng_state* rng, ccec25519pubkey public_key, ccec25519secretkey private_key) {
	CC_STUB(-1);
};
