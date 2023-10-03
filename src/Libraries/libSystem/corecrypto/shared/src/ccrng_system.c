#include <corecrypto/ccrng_system.h>
#include <stdio.h>

static int getrandom_entropy(struct ccrng_state* rng, unsigned long count, void* out);
extern int getentropy(void* buf, size_t len);

int ccrng_system_init(struct ccrng_system_state* rng)
{
	rng->generate = getrandom_entropy;
	return 0;
}

void ccrng_system_done(struct ccrng_system_state* rng)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

static int getrandom_entropy(struct ccrng_state* rng, unsigned long count, void* out)
{
	getentropy(out, count);
	return 0;
}

