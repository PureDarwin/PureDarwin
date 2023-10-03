#include <corecrypto/ccrng_pbkdf2_prng.h>
#include <stdio.h>

int ccrng_pbkdf2_prng_init(struct ccrng_pbkdf2_prng_state *rng, size_t maxbytes,
                           unsigned long passwordLen, const void *password,
                           unsigned long saltLen, const void *salt,
                           unsigned long iterations) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	// TODO
	return 1;
}

