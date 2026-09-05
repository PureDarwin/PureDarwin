/*
 * The random, digest and hmac half of struct crypto_functions.
 *
 * These live at the tail of the struct. They are not optional: the kext hands
 * xnu a single struct, so every trailing member has to exist or xnu reads
 * function pointers from past the end of it. vm_map_store.c calls
 * crypto_random_kmem_ctx_size()/crypto_random_kmem_init() during boot.
 */

#include <libkern/crypto/register_crypto.h>
#include <sys/systm.h>
#include <corecrypto/ccdigest.h>
#include "../algorithms/yarrow/yarrow.h"

extern const struct ccdigest_info pdcmd5_di;
extern const struct ccdigest_info ccsha1_ltc_di;
extern const struct ccdigest_info ccsha256_ltc_di;
extern const struct ccdigest_info ccsha384_ltc_di;
extern const struct ccdigest_info ccsha512_ltc_di;

/*
 * A crypto_random_ctx_t is opaque to xnu, which allocates
 * crypto_random_kmem_ctx_size() bytes for it. We keep our own Yarrow handle in
 * it rather than sharing the kernel PRNG context, matching how xnu uses these
 * (a per-cpu generator for kmem randomization).
 */
struct pd_random_ctx {
	PrngRef prng;
};

size_t
pd_random_kmem_ctx_size_fn(void)
{
	return sizeof(struct pd_random_ctx);
}

void
pd_random_kmem_init_fn(crypto_random_ctx_t ctx)
{
	struct pd_random_ctx *rc = (struct pd_random_ctx *)ctx;

	rc->prng = NULL;
	if (prngInitialize(&rc->prng) != PRNG_SUCCESS) {
		rc->prng = NULL;
	}
}

void
pd_random_generate_fn(crypto_random_ctx_t ctx, void *random, size_t random_size)
{
	struct pd_random_ctx *rc = (struct pd_random_ctx *)ctx;

	if (rc == NULL || rc->prng == NULL || random_size == 0) {
		return;
	}
	prngOutput(rc->prng, (BYTE *)random, (UINT)random_size);
}

void
pd_random_uniform_fn(crypto_random_ctx_t ctx, uint64_t bound, uint64_t *random)
{
	uint64_t v = 0;

	if (random == NULL) {
		return;
	}
	if (bound == 0) {
		*random = 0;
		return;
	}

	/*
	 * Rejection sampling, so the result is uniform rather than skewed by a
	 * plain modulo of a 64-bit draw.
	 */
	uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
	do {
		pd_random_generate_fn(ctx, &v, sizeof(v));
	} while (v >= limit);

	*random = v % bound;
}

static const struct ccdigest_info *
pd_di_for_alg(crypto_digest_alg_t alg)
{
	switch (alg) {
	case CRYPTO_DIGEST_ALG_MD5:    return &pdcmd5_di;
	case CRYPTO_DIGEST_ALG_SHA1:   return &ccsha1_ltc_di;
	case CRYPTO_DIGEST_ALG_SHA256: return &ccsha256_ltc_di;
	case CRYPTO_DIGEST_ALG_SHA384: return &ccsha384_ltc_di;
	case CRYPTO_DIGEST_ALG_SHA512: return &ccsha512_ltc_di;
	default:                       return NULL;
	}
}

size_t
pd_digest_ctx_size_fn(crypto_digest_alg_t alg)
{
	const struct ccdigest_info *di = pd_di_for_alg(alg);

	return di ? (di->state_size + di->block_size + sizeof(uint64_t)) : 0;
}

void
pd_digest_init_fn(crypto_digest_alg_t alg, void *ctx, size_t ctx_size)
{
	const struct ccdigest_info *di = pd_di_for_alg(alg);

	if (di == NULL || ctx_size < pd_digest_ctx_size_fn(alg)) {
		return;
	}
	ccdigest_init(di, (ccdigest_ctx_t)ctx);
}

void
pd_digest_update_fn(crypto_digest_alg_t alg, void *ctx, size_t ctx_size,
    const void *data, size_t data_size)
{
	const struct ccdigest_info *di = pd_di_for_alg(alg);

	if (di == NULL || ctx_size < pd_digest_ctx_size_fn(alg)) {
		return;
	}
	ccdigest_update(di, (ccdigest_ctx_t)ctx, (unsigned long)data_size, data);
}

void
pd_digest_final_fn(crypto_digest_alg_t alg, void *ctx, size_t ctx_size,
    void *digest, size_t digest_size)
{
	const struct ccdigest_info *di = pd_di_for_alg(alg);

	if (di == NULL || ctx_size < pd_digest_ctx_size_fn(alg) ||
	    digest_size < di->output_size) {
		return;
	}
	ccdigest_final(di, (ccdigest_ctx_t)ctx, digest);
}

void
pd_digest_fn(crypto_digest_alg_t alg, const void *data, size_t data_size,
    void *digest, size_t digest_size)
{
	const struct ccdigest_info *di = pd_di_for_alg(alg);

	if (di == NULL || digest_size < di->output_size) {
		return;
	}
	ccdigest(di, (unsigned long)data_size, data, digest);
}

/*
 * HMAC is not wired up: nothing in the kernel calls crypto_hmac_*() today, and
 * the underlying cchmac path here is still the dummy one. These exist so the
 * struct layout matches xnu's and so a future caller fails loudly.
 */
static void
pd_hmac_unimplemented(const char *what)
{
	printf("corecrypto: crypto_hmac_%s is not implemented\n", what);
}

size_t
pd_hmac_ctx_size_fn(crypto_digest_alg_t alg __unused)
{
	pd_hmac_unimplemented("ctx_size");
	return 0;
}

void
pd_hmac_init_fn(crypto_digest_alg_t alg __unused, void *ctx __unused,
    size_t ctx_size __unused, const void *key __unused, size_t key_size __unused)
{
	pd_hmac_unimplemented("init");
}

void
pd_hmac_update_fn(crypto_digest_alg_t alg __unused, void *ctx __unused,
    size_t ctx_size __unused, const void *data __unused, size_t data_size __unused)
{
	pd_hmac_unimplemented("update");
}

void
pd_hmac_final_generate_fn(crypto_digest_alg_t alg __unused, void *ctx __unused,
    size_t ctx_size __unused, void *tag __unused, size_t tag_size __unused)
{
	pd_hmac_unimplemented("final_generate");
}

bool
pd_hmac_final_verify_fn(crypto_digest_alg_t alg __unused, void *ctx __unused,
    size_t ctx_size __unused, const void *tag __unused, size_t tag_size __unused)
{
	pd_hmac_unimplemented("final_verify");
	return false;
}

void
pd_hmac_generate_fn(crypto_digest_alg_t alg __unused, const void *key __unused,
    size_t key_size __unused, const void *data __unused, size_t data_size __unused,
    void *tag __unused, size_t tag_size __unused)
{
	pd_hmac_unimplemented("generate");
}

bool
pd_hmac_verify_fn(crypto_digest_alg_t alg __unused, const void *key __unused,
    size_t key_size __unused, const void *data __unused, size_t data_size __unused,
    const void *tag __unused, size_t tag_size __unused)
{
	pd_hmac_unimplemented("verify");
	return false;
}
