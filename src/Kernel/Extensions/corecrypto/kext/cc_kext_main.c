#include <sys/systm.h>
#include <mach/mach_types.h>
#include <libkern/crypto/register_crypto.h>
#include "prng_random.h"
#include <corecrypto/cckprng.h>
#include "../algorithms/yarrow/yarrow.h"

extern struct crypto_functions pdcrypto_internal_functions;

static const struct cckprng_funcs cc_kprng_fns = {
	.init = cckprng_init,
	.initgen = cckprng_initgen,
	.reseed = cckprng_reseed,
	.refresh = cckprng_refresh,
	.generate = cckprng_generate,
	.init_with_getentropy = cckprng_init_with_getentropy
};

static struct cckprng_ctx cc_kprng_ctx = {
	.prng = NULL,
	.bytes_since_entropy = 0,
	.bytes_generated = 0
};

kern_return_t cc_kext_start(kmod_info_t * ki, void *d)
{
	printf("corecrypto start\n");
	int ret = register_crypto_functions(&pdcrypto_internal_functions);
	printf("corecrypto register ret=%d\n", ret);
	if (ret == -1) {
		printf("warning: corecrypto could not be registered. Did another crypto handler beat us to it?\n");
	} else {
		register_and_init_prng(&cc_kprng_ctx, &cc_kprng_fns);
		printf("corecrypto loaded\n");
	}

    return KERN_SUCCESS;
}

kern_return_t cc_kext_stop(kmod_info_t *ki, void *d)
{
    return KERN_FAILURE;
}
