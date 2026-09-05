#include <stddef.h>
#include <sys/systm.h>
#include <corecrypto/cckprng.h>
#include <corecrypto/ccrng.h>
#include "yarrow/yarrow.h"
#if !KERNEL
#include <stdint.h>
#endif

void cckprng_init(struct cckprng_ctx *ctx, unsigned max_ngens, size_t entropybuf_nbytes, const void *entropybuf,
				  const uint32_t *entropybuf_nsamples, size_t seed_nbytes, const void *seed, size_t nonce_nbytes,
				  const void *nonce) {
	prng_error_status status = prngInitialize(&ctx->prng);
	printf("PD-CCPRNG: init ctx=0x%llx prng=0x%llx status=%d\n", (unsigned long long)(uintptr_t)ctx, (unsigned long long)(uintptr_t)ctx->prng, status);
	if (status != PRNG_SUCCESS || ctx->prng == NULL) {
		printf("PD-CCPRNG: init failed, refusing to seed null PRNG\n");
		return;
	}
	ctx->bytes_generated = ctx->bytes_since_entropy = 0;

	cckprng_reseed(ctx, seed_nbytes, seed);
	cckprng_reseed(ctx, nonce_nbytes, nonce);
	cckprng_reseed(ctx, entropybuf_nbytes, entropybuf);
}

void cckprng_init_with_getentropy(struct cckprng_ctx *ctx, unsigned max_ngens, size_t seed_nbytes, const void *seed,
								  size_t nonce_nbytes, const void *nonce, cckprng_getentropy getentropy,
								  void *getentropy_arg) {
	/* Must be at least SHA512_DIGEST_LENGTH: xnu's entropy_provide() ends with
	 * an unconditional SHA512_Final() into this buffer, ignoring the size we
	 * pass in. A smaller buffer smashes the stack (silently, in RELEASE). */
	uint8_t entropy[64];
	size_t entropy_nbytes = sizeof(entropy);

	cckprng_init(ctx, max_ngens, 0, NULL, NULL, seed_nbytes, seed, nonce_nbytes, nonce);

	if (getentropy && getentropy(&entropy_nbytes, entropy, getentropy_arg) == 0) {
		if (entropy_nbytes > sizeof(entropy)) {
			entropy_nbytes = sizeof(entropy);
		}
		cckprng_reseed(ctx, entropy_nbytes, entropy);
	}
}

void cckprng_initgen(struct cckprng_ctx *ctx, unsigned gen_idx) {
	// Nothing is needed here.
}

void cckprng_reseed(struct cckprng_ctx *ctx, size_t nbytes, const void *seed) {
	if (nbytes == 0 || seed == NULL) {
		return;
	}
	if (ctx == NULL || ctx->prng == NULL) {
		printf("PD-CCPRNG: reseed skipped ctx=0x%llx prng=0x%llx nbytes=%lu\n",
			(unsigned long long)(uintptr_t)ctx,
			(unsigned long long)(uintptr_t)(ctx ? ctx->prng : NULL),
			(unsigned long)nbytes);
		return;
	}
	printf("PD-CCPRNG: reseed ctx=0x%llx prng=0x%llx nbytes=%lu\n",
		(unsigned long long)(uintptr_t)ctx,
		(unsigned long long)(uintptr_t)ctx->prng, (unsigned long)nbytes);

	prngInput(ctx->prng, (BYTE *)seed, (UINT)nbytes, 0, 0);
	prngAllowReseed(ctx->prng, 5000);
	ctx->bytes_since_entropy = 0;
}

void cckprng_refresh(struct cckprng_ctx *ctx) {
	// Nothing is needed here, either.
}

void cckprng_generate(struct cckprng_ctx *ctx, unsigned gen_idx, size_t nbytes, void *out) {
	BYTE *buffer = (BYTE *)out;
	while (nbytes > UINT32_MAX) {
		prngOutput(ctx->prng, buffer, UINT32_MAX);
		ctx->bytes_generated += UINT32_MAX;
		ctx->bytes_since_entropy += UINT32_MAX;

		buffer += UINT32_MAX;
		nbytes -= UINT32_MAX;
		nbytes = CC_MAX(nbytes, 0);
	}

	if (nbytes != 0) {
		prngOutput(ctx->prng, buffer, (UINT)nbytes);
		ctx->bytes_generated += nbytes;
		ctx->bytes_since_entropy += nbytes;
	}
}

// MARK: -

struct ccrng_impl {
	CCRNG_STATE_COMMON;
	PrngRef prng;
};

static int ccrng_generate_impl(struct ccrng_state *rng, size_t outlen, void *out) {
	struct ccrng_impl *impl = (struct ccrng_impl *)rng;
	prngOutput(impl->prng, out, (UINT)outlen);
	return 0;
}

struct ccrng_state *ccrng(int *error) {
	static struct ccrng_impl ccrng_impl = { ccrng_generate_impl, NULL };
	static bool initialized = false;

	if (!initialized) {
		prngInitialize(&ccrng_impl.prng);
		initialized = true;
	}

	return (struct ccrng_state *)&ccrng_impl;
}
