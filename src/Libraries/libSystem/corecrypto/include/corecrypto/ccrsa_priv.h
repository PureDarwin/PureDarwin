#ifndef _CCRSA_PRIV_H_
#define _CCRSA_PRIV_H_

#include <corecrypto/ccrsa.h>
#include <corecrypto/cc_priv.h>

CC_NONNULL((1, 2, 4, 6))
int ccrsa_oaep_encode_parameter(const struct ccdigest_info* di,
                                struct ccrng_state *rng,
                                size_t r_size, cc_unit *r,
                                size_t message_len, const uint8_t *message,
                                size_t parameter_data_len, const uint8_t *parameter_data);

CC_NONNULL((1, 2, 3, 5))
int ccrsa_oaep_decode_parameter(const struct ccdigest_info* di,
                                size_t *r_len, uint8_t *r,
                                size_t s_size, cc_unit *s,
                                size_t parameter_data_len, const uint8_t *parameter_data);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3, 4, 6))
int ccrsa_encrypt_eme_pkcs1v15(ccrsa_pub_ctx_t key,
                           struct ccrng_state *rng,
                           size_t *r_size, uint8_t *r,
                           size_t s_size, const uint8_t *s);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3, 4, 5, 7))
int ccrsa_encrypt_oaep(ccrsa_pub_ctx_t key,
                   const struct ccdigest_info* di,
                   struct ccrng_state *rng,
                   size_t *r_size, uint8_t *r,
                   size_t s_size, uint8_t *s,
                   size_t parameter_data_len,
                   const uint8_t *parameter_data);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3, 5))
int ccrsa_decrypt_eme_pkcs1v15(ccrsa_full_ctx_t key,
                           size_t *r_size, uint8_t *r,
                           size_t s_size, uint8_t *s);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3, 4, 6))
int ccrsa_decrypt_oaep(ccrsa_full_ctx_t key,
                   const struct ccdigest_info* di,
                   size_t *r_size, uint8_t *r,
                       size_t c_size, uint8_t *c,
                       size_t parameter_data_len,
                       const uint8_t *parameter_data);

CC_NONNULL_TU((1,4,7)) CC_NONNULL((2, 3, 5, 6, 8))
int ccrsa_crt_makekey(cczp_t zm, const cc_unit *e, cc_unit *d, cczp_t zp, cc_unit *dp, cc_unit *qinv, cczp_t zq, cc_unit *dq);

CC_NONNULL2 CC_NONNULL4
int ccrsa_emsa_pkcs1v15_encode(size_t emlen, uint8_t *em,
		               size_t dgstlen, const uint8_t *dgst,
			       const uint8_t *oid);

CC_NONNULL2 CC_NONNULL4
int ccrsa_emsa_pkcs1v15_verify(size_t emlen, uint8_t *em,
		               size_t dgstlen, const uint8_t *dgst,
			       const uint8_t *oid);

int ccrsa_emsa_pss_encode(const struct ccdigest_info* di1, const struct ccdigest_info* di2, size_t salt_len, const void* salt, size_t data_len, const void* data, size_t output_bits, void* output);
int ccrsa_emsa_pss_decode(const struct ccdigest_info* di1, const struct ccdigest_info* di2, size_t salt_len, size_t data_len, const void* data, size_t output_bits, void* output);

CC_NONNULL((1, 3, 5))
int ccrsa_eme_pkcs1v15_encode(struct ccrng_state *rng,
		size_t r_size, cc_unit *r, size_t s_size,
		const uint8_t *s);

CC_NONNULL((1, 2, 4))
int ccrsa_eme_pkcs1v15_decode(size_t *r_size, uint8_t *r,
		size_t s_size, cc_unit *s);

CC_INLINE CC_NONNULL((1, 2, 4, 6))
int ccrsa_oaep_encode(const struct ccdigest_info* di,
                      struct ccrng_state *rng,
                      size_t r_size, cc_unit *r,
                      size_t message_len, const uint8_t *message)
{
    return ccrsa_oaep_encode_parameter(di, rng, r_size, r, message_len, message, 0, NULL);
}

CC_INLINE CC_NONNULL((1, 2, 3, 5))
int ccrsa_oaep_decode(const struct ccdigest_info* di,
                      size_t *r_len, uint8_t *r,
                      size_t s_size, cc_unit* s)
{
    return ccrsa_oaep_decode_parameter(di, r_len, r, s_size, s, 0, NULL);
}

CC_INLINE size_t ccrsa_block_size(ccrsa_pub_ctx_t key) {
    return ccn_write_uint_size(ccrsa_ctx_n(key), ccrsa_ctx_m(key));
}

CC_INLINE cc_size ccrsa_n_from_size(size_t size) {
	    return ccn_nof_size(size);
}

CC_INLINE size_t ccrsa_sizeof_n_from_size(size_t size) {
	    return ccn_sizeof_n(ccn_nof_size(size));
}

#endif
