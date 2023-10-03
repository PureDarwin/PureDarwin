#include <corecrypto/ccrng_pbkdf2_prng.h>
#include <corecrypto/ccstubs.h>

int ccrng_pbkdf2_prng_init(struct ccrng_pbkdf2_prng_state *rng, size_t maxbytes,
                           unsigned long passwordLen, const void *password,
                           unsigned long saltLen, const void *salt,
                           unsigned long iterations) {
	CC_STUB_ERR();
}

