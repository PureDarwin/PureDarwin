#include <libkern/crypto/register_crypto.h>
#include <corecrypto/cc_abort.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccsha1.h>
#include "../algorithms/pdcrypto_dummy.h"

extern void pdcdigest_init(const struct ccdigest_info *di, ccdigest_ctx_t ctx);
extern void pdcdigest_update(const struct ccdigest_info *di, ccdigest_ctx_t ctx, unsigned long len, const void *data);
extern void pdcdigest_final_fn(const struct ccdigest_info *di, ccdigest_ctx_t ctx, void *digest);
extern void pdcdigest_fn(const struct ccdigest_info *di, unsigned long len, const void *data, void *digest);

extern const struct ccdigest_info pdcmd5_di;
extern const struct ccmode_ecb pdcaes_ecb_encrypt;
extern const struct ccmode_ecb pdcaes_ecb_decrypt;
extern const struct ccmode_cbc pdcaes_cbc_encrypt;
extern const struct ccmode_cbc pdcaes_cbc_decrypt;
extern const struct ccdigest_info ccsha256_ltc_di;

/* Tail half of struct crypto_functions; see cc_random.c. */
extern void   pd_random_generate_fn(crypto_random_ctx_t, void *, size_t);
extern void   pd_random_uniform_fn(crypto_random_ctx_t, uint64_t, uint64_t *);
extern size_t pd_random_kmem_ctx_size_fn(void);
extern void   pd_random_kmem_init_fn(crypto_random_ctx_t);
extern size_t pd_digest_ctx_size_fn(crypto_digest_alg_t);
extern void   pd_digest_init_fn(crypto_digest_alg_t, void *, size_t);
extern void   pd_digest_update_fn(crypto_digest_alg_t, void *, size_t, const void *, size_t);
extern void   pd_digest_final_fn(crypto_digest_alg_t, void *, size_t, void *, size_t);
extern void   pd_digest_fn(crypto_digest_alg_t, const void *, size_t, void *, size_t);
extern size_t pd_hmac_ctx_size_fn(crypto_digest_alg_t);
extern void   pd_hmac_init_fn(crypto_digest_alg_t, void *, size_t, const void *, size_t);
extern void   pd_hmac_update_fn(crypto_digest_alg_t, void *, size_t, const void *, size_t);
extern void   pd_hmac_final_generate_fn(crypto_digest_alg_t, void *, size_t, void *, size_t);
extern bool   pd_hmac_final_verify_fn(crypto_digest_alg_t, void *, size_t, const void *, size_t);
extern void   pd_hmac_generate_fn(crypto_digest_alg_t, const void *, size_t, const void *, size_t, void *, size_t);
extern bool   pd_hmac_verify_fn(crypto_digest_alg_t, const void *, size_t, const void *, size_t, const void *, size_t);

const struct crypto_functions pdcrypto_internal_functions = {
	.ccdigest_init_fn = ccdigest_init,
	.ccdigest_update_fn = ccdigest_update,
	.ccmd5_di = &pdcmd5_di,
	.ccsha1_di = &ccsha1_ltc_di,

	.ccdigest_final_fn = pdcdigest_final_fn,
	.ccdigest_fn = pdcdigest_fn,
	.ccsha256_di = &ccsha256_ltc_di,
	.ccsha384_di = &ccsha384_ltc_di,
	.ccsha512_di = &ccsha512_ltc_di,
	.cchmac_init_fn = pdchmac_init_fn_dummy,
	.cchmac_update_fn = pdchmac_update_fn_dummy,
	.cchmac_final_fn = pdchmac_final_fn_dummy,
	.cchmac_fn = pdchmac_fn_dummy,
	.ccaes_ecb_encrypt = &pdcaes_ecb_encrypt,
	.ccaes_ecb_decrypt = &pdcaes_ecb_decrypt,
	.ccaes_cbc_encrypt = &pdcaes_cbc_encrypt,
	.ccaes_cbc_decrypt = &pdcaes_cbc_decrypt,
	.ccaes_xts_encrypt = &pdcaes_xts_encrypt_dummy,
	.ccaes_xts_decrypt = &pdcaes_xts_decrypt_dummy,
	.ccaes_gcm_encrypt = &pdcaes_gcm_encrypt_dummy,
	.ccaes_gcm_decrypt = &pdcaes_gcm_decrypt_dummy,
	.ccdes_ecb_encrypt = &pdcdes_ecb_encrypt_dummy,
	.ccdes_ecb_decrypt = &pdcdes_ecb_decrypt_dummy,
	.ccdes_cbc_encrypt = &pdcdes_cbc_encrypt_dummy,
	.ccdes_cbc_decrypt = &pdcdes_cbc_decrypt_dummy,
	.cctdes_ecb_encrypt = &pdctdes_ecb_encrypt_dummy,
	.cctdes_ecb_decrypt = &pdctdes_ecb_decrypt_dummy,
	.cctdes_cbc_encrypt = &pdctdes_cbc_encrypt_dummy,
	.cctdes_cbc_decrypt = &pdctdes_cbc_decrypt_dummy,
	.ccdes_key_is_weak_fn = pdcdes_key_is_weak_fn_dummy,
	.ccdes_key_set_odd_parity_fn = pdcdes_key_set_odd_parity_fn_dummy,

	.ccrng_fn = ccrng,

	.random_generate_fn = pd_random_generate_fn,
	.random_uniform_fn = pd_random_uniform_fn,
	.random_kmem_ctx_size_fn = pd_random_kmem_ctx_size_fn,
	.random_kmem_init_fn = pd_random_kmem_init_fn,

	.digest_ctx_size_fn = pd_digest_ctx_size_fn,
	.digest_init_fn = pd_digest_init_fn,
	.digest_update_fn = pd_digest_update_fn,
	.digest_final_fn = pd_digest_final_fn,
	.digest_fn = pd_digest_fn,

	.hmac_ctx_size_fn = pd_hmac_ctx_size_fn,
	.hmac_init_fn = pd_hmac_init_fn,
	.hmac_update_fn = pd_hmac_update_fn,
	.hmac_final_generate_fn = pd_hmac_final_generate_fn,
	.hmac_final_verify_fn = pd_hmac_final_verify_fn,
	.hmac_generate_fn = pd_hmac_generate_fn,
	.hmac_verify_fn = pd_hmac_verify_fn
};
