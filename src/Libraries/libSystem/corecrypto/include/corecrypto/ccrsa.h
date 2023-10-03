#ifndef _CCRSA_H_
#define _CCRSA_H_

#include <corecrypto/cc.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/cczp.h>
#include <stdbool.h>

// Structs / unions

cc_aligned_struct_name(ccrsa_full_ctx,16);
cc_aligned_struct(16) ccrsa_priv_ctx;
cc_aligned_struct(16) ccrsa_pub_ctx;

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    cczp_t zp;
    ccrsa_priv_ctx *priv;
} ccrsa_priv_ctx_t __attribute__((transparent_union));

typedef union {
    cczp_t zp;
    ccrsa_full_ctx *full;
} ccrsa_full_ctx_t __attribute__((transparent_union));

typedef union {
    cczp_t zp;
    ccrsa_pub_ctx *pub;
    ccrsa_full_ctx *full;
    ccrsa_full_ctx_t full_t;
} ccrsa_pub_ctx_t __attribute__((transparent_union));

#define CCRSA_PRIV_CTX_T_ZP(ctx) ((ctx).zp)
#define CCRSA_PRIV_CTX_T_PRIV(ctx) ((ctx).priv)

#define CCRSA_FULL_CTX_T_ZP(ctx) ((ctx).zp)
#define CCRSA_FULL_CTX_T_FULL(ctx) ((ctx).full)

#define CCRSA_PUB_CTX_T_ZP(ctx) ((ctx).zp)
#define CCRSA_PUB_CTX_T_PUB(ctx) ((ctx).pub)
#define CCRSA_PUB_CTX_T_FULL(ctx) ((ctx).full)
#define CCRSA_PUB_CTX_T_FULL_T(ctx) ((ctx).full_t)
#else
typedef ccrsa_priv_ctx* ccrsa_priv_ctx_t;
typedef ccrsa_full_ctx* ccrsa_full_ctx_t;
typedef ccrsa_pub_ctx* ccrsa_pub_ctx_t;

#define CCRSA_PRIV_CTX_T_ZP(ctx) ((cczp_t)(ctx))
#define CCRSA_PRIV_CTX_T_PRIV(ctx) ((ccrsa_priv_ctx*)(ctx))

#define CCRSA_FULL_CTX_T_ZP(ctx) ((cczp_t)(ctx))
#define CCRSA_FULL_CTX_T_FULL(ctx) ((ccrsa_full_ctx*)(ctx))

#define CCRSA_PUB_CTX_T_ZP(ctx) ((cczp_t)(ctx))
#define CCRSA_PUB_CTX_T_PUB(ctx) ((ccrsa_pub_ctx*)(ctx))
#define CCRSA_PUB_CTX_T_FULL(ctx) ((ccrsa_full_ctx*)(ctx))
#define CCRSA_PUB_CTX_T_FULL_T(ctx) ((ccrsa_full_ctx_t)(ctx))
#endif

// Macros

#define ccrsa_ctx_zm(_ctx_)        (CCRSA_PUB_CTX_T_ZP((ccrsa_pub_ctx_t)(_ctx_)))
#define ccrsa_ctx_n(_ctx_)         (CCZP_T_ZP(ccrsa_ctx_zm(_ctx_))->n)
#define ccrsa_ctx_m(_ctx_)         (CCZP_T_PRIME(ccrsa_ctx_zm(_ctx_))->ccn)
#define ccrsa_ctx_e(_ctx_)         (ccrsa_ctx_m(_ctx_) + 2 * ccrsa_ctx_n(_ctx_) + 1)
#define ccrsa_ctx_d(_ctx_)         (ccrsa_ctx_m(((ccrsa_full_ctx_t)_ctx_)) + 3 * ccrsa_ctx_n(_ctx_) + 1)

#define ccrsa_ctx_private_zq(PRIVK)   ((cczp_t)(CCZP_T_PRIME(CCRSA_PRIV_CTX_T_ZP((ccrsa_priv_ctx_t)(PRIVK)))->ccn + 2 * CCZP_T_ZP(ccrsa_ctx_private_zp(PRIVK))->n + 1))
#define ccrsa_pub_ctx_size(_size_)   (sizeof(struct cczp) + CCN_UNIT_SIZE + 3 * (_size_))
#define ccrsa_ctx_private_zp(PRIVK)   (CCRSA_PRIV_CTX_T_ZP((ccrsa_priv_ctx_t)(PRIVK)))
#define ccrsa_ctx_private_dq(PRIVK)   (CCZP_T_PRIME(CCRSA_PRIV_CTX_T_ZP((ccrsa_priv_ctx_t)(PRIVK)))->ccn + 5 * CCZP_T_ZP(ccrsa_ctx_private_zp(PRIVK))->n + 2 + ccn_nof_size(sizeof(struct cczp)))
#define ccrsa_ctx_private_qinv(PRIVK) (CCZP_T_PRIME(CCRSA_PRIV_CTX_T_ZP((ccrsa_priv_ctx_t)(PRIVK)))->ccn + 6 * CCZP_T_ZP(ccrsa_ctx_private_zp(PRIVK))->n + 2 + ccn_nof_size(sizeof(struct cczp)))

#define ccrsa_ctx_private_dp(PRIVK)   (CCZP_T_PRIME(CCRSA_PRIV_CTX_T_ZP((ccrsa_priv_ctx_t)(PRIVK)))->ccn + 4 * CCZP_T_ZP(ccrsa_ctx_private_zp(PRIVK))->n + 2 + ccn_nof_size(sizeof(struct cczp)))

#define ccrsa_priv_ctx_size(_size_)  ((sizeof(struct cczp) + CCN_UNIT_SIZE) * 2 + 7 * ccn_sizeof((ccn_bitsof_size(_size_) / 2) + 1))


#define ccrsa_ctx_public(FK)          ((ccrsa_pub_ctx_t)(FK))

#define ccrsa_full_ctx_size(_size_)  (ccrsa_pub_ctx_size(_size_) + _size_ + ccrsa_priv_ctx_size(_size_))

#define ccrsa_priv_ctx_decl(_size_, _name_)  cc_ctx_decl(ccrsa_priv_ctx, ccrsa_priv_ctx_size(_size_), _name_)

#define ccrsa_full_ctx_decl(_size_, _name_)  cc_ctx_decl(ccrsa_full_ctx, ccrsa_full_ctx_size(_size_), _name_)

// Functions

CC_CONST CC_INLINE
ccrsa_priv_ctx_t ccrsa_ctx_private(ccrsa_full_ctx_t fk) {
    uint8_t *p = (uint8_t *)CCRSA_FULL_CTX_T_FULL(fk);
    size_t p_size = ccn_sizeof_n(ccrsa_ctx_n(fk));
    p += ccrsa_pub_ctx_size(p_size) + p_size;
    ccrsa_priv_ctx *priv = (ccrsa_priv_ctx *)p;
    return (ccrsa_priv_ctx_t)priv;
}

CC_NONNULL_TU((1))
size_t ccder_encode_rsa_priv_size(const ccrsa_full_ctx_t key);

CC_NONNULL((1)) CC_NONNULL((2))
cc_size ccder_decode_rsa_pub_x509_n(const uint8_t *der, const uint8_t *der_end);

CC_NONNULL_TU((1)) CC_NONNULL((3))
int ccrsa_import_pub(ccrsa_pub_ctx_t key, size_t inlen, const uint8_t *der);

CC_NONNULL((1)) CC_NONNULL((2))
cc_size ccder_decode_rsa_pub_n(const uint8_t *der, const uint8_t *der_end);

CC_NONNULL_TU((1))
size_t ccder_encode_rsa_pub_size(const ccrsa_pub_ctx_t key);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3))
int ccrsa_pub_crypt(ccrsa_pub_ctx_t key, cc_unit *out, const cc_unit *in);

CC_NONNULL_TU((2)) CC_NONNULL((4, 5))
int ccrsa_generate_key(unsigned long nbits, ccrsa_full_ctx_t rsa_ctx,
                       size_t e_size, const void *e, struct ccrng_state *rng);

CC_NONNULL_TU((1)) CC_NONNULL((2)) CC_NONNULL((3))
uint8_t *ccder_encode_rsa_priv(const ccrsa_full_ctx_t key, const uint8_t *der, uint8_t *der_end);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3))
int ccrsa_priv_crypt(ccrsa_priv_ctx_t key, cc_unit *out, const cc_unit *in);

CC_NONNULL((1)) CC_NONNULL((2))
cc_size ccder_decode_rsa_priv_n(const uint8_t *der, const uint8_t *der_end);

CC_NONNULL_TU((16)) CC_NONNULL((3, 5, 7, 9, 11, 13, 15))
int
ccrsa_make_fips186_key(unsigned long nbits,
                       const cc_size e_n, const cc_unit *e,
                       const cc_size xp1Len, const cc_unit *xp1, const cc_size xp2Len, const cc_unit *xp2,
                       const cc_size xpLen, const cc_unit *xp,
                       const cc_size xq1Len, const cc_unit *xq1, const cc_size xq2Len, const cc_unit *xq2,
                       const cc_size xqLen, const cc_unit *xq,
                       ccrsa_full_ctx_t fk,
                       cc_size *np, cc_unit *r_p,
                       cc_size *nq, cc_unit *r_q,
                       cc_size *nm, cc_unit *r_m,
                       cc_size *nd, cc_unit *r_d);

CC_NONNULL_TU((1)) CC_NONNULL2
int ccrsa_get_pubkey_components(const ccrsa_pub_ctx_t pubkey, uint8_t *modulus, size_t *modulusLength, uint8_t *exponent, size_t *exponentLength);

CC_NONNULL_TU((1)) CC_NONNULL2
int ccrsa_get_fullkey_components(const ccrsa_full_ctx_t key, uint8_t *modulus, size_t *modulusLength, uint8_t *exponent, size_t *exponentLength,
                                 uint8_t *p, size_t *pLength, uint8_t *q, size_t *qLength);

CC_NONNULL_TU((1)) CC_NONNULL((2)) CC_NONNULL((3))
const uint8_t *ccder_decode_rsa_priv(const ccrsa_full_ctx_t key, const uint8_t *der, const uint8_t *der_end);

CC_NONNULL_TU((1)) CC_NONNULL((2, 4, 6, 7))
int ccrsa_verify_pkcs1v15(ccrsa_pub_ctx_t key, const uint8_t *oid,
                          size_t digest_len, const uint8_t *digest,
                          size_t sig_len, const uint8_t *sig,
                          bool *valid);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3))
int ccrsa_init_pub(ccrsa_pub_ctx_t key, const cc_unit *modulus,
                    const cc_unit *e);

CC_NONNULL_TU((1)) CC_NONNULL((2, 4, 5, 6))
int ccrsa_sign_pkcs1v15(ccrsa_full_ctx_t key, const uint8_t *oid,
                        size_t digest_len, const uint8_t *digest,
                        size_t *sig_len, uint8_t *sig);

CC_NONNULL_TU((2)) CC_NONNULL((4, 5, 6))
int
ccrsa_generate_fips186_key(unsigned long nbits, ccrsa_full_ctx_t fk,
                           size_t e_size, const void *eBytes,
                           struct ccrng_state *rng1, struct ccrng_state *rng2);

CC_CONST CC_INLINE CC_NONNULL((2))
cc_size ccrsa_import_priv_n(size_t inlen, const uint8_t *der) {
    return ccder_decode_rsa_priv_n(der, der + inlen);
}

CC_CONST CC_INLINE CC_NONNULL((2))
cc_size ccrsa_import_pub_n(size_t inlen, const uint8_t *der) {
    cc_size size = ccder_decode_rsa_pub_x509_n(der, der + inlen);
    if(size == 0) {
        size = ccder_decode_rsa_pub_n(der, der + inlen);
    }
    return size;
}

CC_CONST CC_INLINE CC_NONNULL_TU((1)) CC_NONNULL((3))
int ccrsa_export_priv(const ccrsa_full_ctx_t key, size_t out_len, uint8_t *out) {
    return (ccder_encode_rsa_priv(key, out, out+out_len) != out);
}

CC_CONST CC_INLINE CC_NONNULL_TU((1)) CC_NONNULL((3))
int ccrsa_import_priv(ccrsa_full_ctx_t key, size_t inlen, const uint8_t *der) {
    return (ccder_decode_rsa_priv(key, der, der+inlen) == NULL);
}

CC_NONNULL_TU((1)) CC_NONNULL((3))
int ccrsa_export_pub(const ccrsa_pub_ctx_t key, size_t out_len, uint8_t *out);

CC_CONST CC_INLINE CC_NONNULL_TU((1))
size_t ccrsa_export_priv_size(const ccrsa_full_ctx_t key) {
    return ccder_encode_rsa_priv_size(key);
}

CC_CONST CC_INLINE CC_NONNULL_TU((1))
size_t ccrsa_export_pub_size(const ccrsa_pub_ctx_t key) {
    return ccder_encode_rsa_pub_size(key);
}

int ccrsa_make_priv(ccrsa_full_ctx_t key, size_t expLen, const void* exp, size_t pLen, const void* p, size_t qLen, const void* q);
int ccrsa_make_pub(ccrsa_pub_ctx_t key, size_t expLen, const void* exp, size_t modLen, const void* mod);

int ccrsa_sign_pss(ccrsa_full_ctx_t key, const struct ccdigest_info* di1, const struct ccdigest_info* di2, size_t saltLen, struct ccrng_state* rng, size_t hashLen, const void* hash, size_t* signedDataLen, void* signedData);
int ccrsa_verify_pss(ccrsa_full_ctx_t key, const struct ccdigest_info* di1, const struct ccdigest_info* di2, size_t hashLen, const void* hash, size_t signedDataLen, const void* signedData, size_t saltLen, bool* valid);

#endif
