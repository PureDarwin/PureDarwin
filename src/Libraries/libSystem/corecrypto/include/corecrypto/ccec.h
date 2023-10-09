#ifndef _CORECRYPTO_CCEC_H_
#define _CORECRYPTO_CCEC_H_
#include <corecrypto/ccasn1.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/cczp.h>
#include <stdbool.h>
#include <stdarg.h>

cc_aligned_struct(8) ccec_projective_point;

// Structs/unions

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    struct ccec_point_hdr *hdr;
    ccec_projective_point *_p;
} __attribute__((transparent_union)) ccec_projective_point_t;

typedef union {
    const struct cczp   *zp;
    const struct cczp_prime *prime;
} __attribute__((transparent_union)) ccec_const_cp_t;

#define CCEC_PROJECTIVE_POINT_T_HDR(point) ((point).hdr)

#define CCEC_CONST_CP_T_ZP(cp) ((cp).zp)
#define CCEC_CONST_CP_T_PRIME(cp) ((cp).prime)
#else
typedef ccec_projective_point* ccec_projective_point_t;
typedef const struct cczp* ccec_const_cp_t;

#define CCEC_PROJECTIVE_POINT_T_HDR(point) ((struct ccec_point_hdr*)(point))

#define CCEC_CONST_CP_T_ZP(cp) ((const struct cczp*)(cp))
#define CCEC_CONST_CP_T_PRIME(cp) ((const struct cczp_prime*)(cp))
#endif

struct ccec_ctx_header {
    ccec_const_cp_t      cp;
    uint8_t              pad[16 - sizeof(ccec_const_cp_t *)];
} __attribute__((aligned(16)));

struct ccec_ctx_public {
    struct ccec_ctx_header  hdr;
    ccec_projective_point point[];
} __attribute__((aligned(16)));

typedef struct ccec_full_ctx {
    struct ccec_ctx_header  hdr;
} __attribute__((aligned(16))) ccec_full_ctx;

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    ccec_full_ctx *_full;
    struct ccec_ctx_header *hdr;
    struct ccec_ctx_body *body;
    struct ccec_ctx_public *pub;
} __attribute__((transparent_union)) ccec_full_ctx_t;

#define CCEC_FULL_CTX_HDR(ctx) ((ctx).hdr)
#define CCEC_FULL_CTX_BODY(ctx) ((ctx).body)
#define CCEC_FULL_CTX_PUB(ctx) ((ctx).pub)
#else
typedef ccec_full_ctx* ccec_full_ctx_t;

#define CCEC_FULL_CTX_HDR(ctx) ((struct ccec_ctx_header*)(ctx))
#define CCEC_FULL_CTX_BODY(ctx) ((struct ccec_ctx_body*)(ctx))
#define CCEC_FULL_CTX_PUB(ctx) ((struct ccec_ctx_public*)(ctx))
#endif

typedef struct ccec_pub_ctx {
    struct ccec_ctx_header  hdr;
} __attribute__((aligned(16))) ccec_pub_ctx;

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    ccec_pub_ctx *_pub;
    ccec_full_ctx *_full;
    struct ccec_ctx_header *hdr;
    struct ccec_ctx_body *body;
    struct ccec_ctx_public *pub;
    ccec_full_ctx_t fullt;
} __attribute__((transparent_union)) ccec_pub_ctx_t;

#define CCEC_PUB_CTX_HDR(ctx) ((ctx).hdr)
#define CCEC_PUB_CTX_BODY(ctx) ((ctx).body)
#define CCEC_PUB_CTX_PUB(ctx) ((ctx).pub)
#else
typedef ccec_pub_ctx* ccec_pub_ctx_t;

#define CCEC_PUB_CTX_HDR(ctx) ((struct ccec_ctx_header*)(ctx))
#define CCEC_PUB_CTX_BODY(ctx) ((struct ccec_ctx_body*)(ctx))
#define CCEC_PUB_CTX_PUB(ctx) ((struct ccec_ctx_public*)(ctx))
#endif

struct ccec_ctx_body {
    struct ccec_ctx_header  hdr;
    cc_unit              ccn[];
} __attribute__((aligned(16)));

// Functions needed by the following macros
CC_CONST CC_INLINE
cc_size ccec_cp_n(ccec_const_cp_t cp) {
    return CCEC_CONST_CP_T_ZP(cp)->n;
}

CC_CONST CC_INLINE
size_t ccec_ccn_size(ccec_const_cp_t cp) {
    return ccn_sizeof_n(ccec_cp_n(cp));
}

size_t ccec_compact_import_pub_size(size_t in_len);

// Macros
#define ccec_pub_ctx_size(_size_)   (sizeof(struct ccec_ctx_header) + 3 * (_size_))
#define ccec_ctx_cp(KEY)     (CCEC_PUB_CTX_HDR((ccec_pub_ctx_t)(KEY))->cp)
#define ccec_cp_prime_bitlen(CP) (ccn_bitlen(CCEC_CONST_CP_T_ZP(CP)->n, CCEC_CONST_CP_T_PRIME(CP)->ccn))
#define ccec_ctx_bitlen(KEY) (ccec_cp_prime_bitlen(ccec_ctx_cp(KEY)))
#define ccec_full_ctx_size(_size_)  (sizeof(struct ccec_ctx_header) + 4 * (_size_))
#define ccec_full_ctx_decl(_size_, _name_)  cc_ctx_decl(ccec_full_ctx, ccec_full_ctx_size(_size_), _name_)
#define ccec_full_ctx_decl_cp(_cp_, _name_) ccec_full_ctx_decl(ccec_ccn_size(_cp_), _name_)
#define ccec_pub_ctx_decl(_size_, _name_)   cc_ctx_decl(ccec_pub_ctx, ccec_pub_ctx_size(_size_), _name_)
#define ccec_pub_ctx_decl_cp(_cp_, _name_)  ccec_pub_ctx_decl(ccec_ccn_size(_cp_), _name_)
#define ccec_full_ctx_clear(_size_, _name_) cc_clear(ccec_full_ctx_size(_size_), _name_)
#define ccec_full_ctx_clear_cp(_cp_, _name_) ccec_full_ctx_clear(ccec_ccn_size(_cp_), _name_)
#define ccec_ctx_n(KEY)      (CCEC_CONST_CP_T_ZP(CCEC_PUB_CTX_HDR((ccec_pub_ctx_t)(KEY))->cp)->n)
#define ccec_cp_prime_size(CP) ((ccec_cp_prime_bitlen(CP)+7)/8)
#define ccec_ctx_size(KEY) (ccec_cp_prime_size(ccec_ctx_cp(KEY)))

#define ccec_ctx_x(KEY)  (CCEC_PUB_CTX_BODY((ccec_pub_ctx_t)(KEY))->ccn)
#define ccec_ctx_y(KEY)  (CCEC_PUB_CTX_BODY((ccec_pub_ctx_t)(KEY))->ccn + 1 * ccec_ctx_n(KEY))
#define ccec_ctx_z(KEY)  (CCEC_PUB_CTX_BODY((ccec_pub_ctx_t)(KEY))->ccn + 2 * ccec_ctx_n(KEY))
#define ccec_ctx_k(KEY)      (CCEC_FULL_CTX_BODY((ccec_full_ctx_t)(KEY))->ccn + 3 * ccec_ctx_n(KEY))

#define ccec_ctx_point(KEY)    ((ccec_projective_point_t)(CCEC_PUB_CTX_PUB((ccec_pub_ctx_t)(KEY))->point))

#define ccec_ctx_pub(KEY) ((ccec_pub_ctx_t)(KEY))
#define ccec_ctx_public(KEY) (ccec_ctx_pub(KEY))

// Functions
CC_INLINE CC_CONST CC_NONNULL_TU((2))
size_t ccec_x963_export_size_cp(bool is_full_key, ccec_const_cp_t cp) {
	return (((ccec_cp_prime_bitlen(cp) + 7) /8 ) * ((is_full_key == true ? 1 : 0) + 2)) + 1;
};

CC_INLINE CC_CONST CC_NONNULL_TU((2))
size_t ccec_x963_export_size(bool is_full_key, ccec_full_ctx_t key) {
	return ccec_x963_export_size_cp(is_full_key, ccec_ctx_cp(key));
};

CC_INLINE CC_PURE CC_NONNULL_TU((1))
size_t ccec_sign_max_size(ccec_const_cp_t cp) {
	return 3 + 2 * (3 + ccec_cp_prime_size(cp));
}

CC_INLINE CC_NONNULL_TU((1, 2))
void ccec_pub_ctx_clear_cp(ccec_const_cp_t cp, ccec_pub_ctx_t ctx) {
	cc_clear(ccec_pub_ctx_size(ccec_ccn_size(cp)) - sizeof(struct ccec_ctx_header), CCEC_PUB_CTX_PUB(ctx)->point);
};

ccec_const_cp_t ccec_get_cp(size_t keysize);

CC_NONNULL_TU((3)) CC_NONNULL2
void ccec_x963_export(const int fullkey, void *out, ccec_full_ctx_t key);

CC_NONNULL_TU((1,3)) CC_NONNULL2
int ccec_generate_key_legacy(ccec_const_cp_t cp,  struct ccrng_state *rng,
                             ccec_full_ctx_t key);

CC_NONNULL_TU((1))
size_t ccec_rfc6637_wrap_key_size(ccec_pub_ctx_t public_key,
                                  unsigned long flags,
                                  size_t key_len);

#define CCEC_RFC6637_COMPACT_KEYS   1
#define CCEC_RFC6637_DEBUG_KEYS     2

struct ccec_rfc6637_curve;
struct ccec_rfc6637_wrap;
struct ccec_rfc6637_unwrap;

#ifndef HIDE_COMPLAINT
CC_NONNULL_TU((1)) CC_NONNULL((2, 6, 7, 8, 9, 10))
int ccec_rfc6637_wrap_key(ccec_pub_ctx_t public_key,
                          void  *wrapped_key,
                          unsigned long flags,
                          uint8_t algid,
                          size_t key_len,
                          const void *key,
                          const struct ccec_rfc6637_curve *curve,
                          const struct ccec_rfc6637_wrap *wrap,
                          const uint8_t *fingerprint,
                          struct ccrng_state *rng);

CC_NONNULL_TU((1)) CC_NONNULL((2, 3, 5, 6, 7, 8, 10))
int ccec_rfc6637_unwrap_key(ccec_full_ctx_t private_key,
                            size_t *key_len,
                            void *key,
                            unsigned long flags,
                            uint8_t *symm_key_alg,
                            const struct ccec_rfc6637_curve *curve,
                            const struct ccec_rfc6637_unwrap *wrap,
                            const uint8_t *fingerprint,
                            size_t wrapped_key_len,
                            const void  *wrapped_key);
#endif

CC_CONST ccec_const_cp_t ccec_cp_192();
CC_CONST ccec_const_cp_t ccec_cp_224();
CC_CONST ccec_const_cp_t ccec_cp_256();
CC_CONST ccec_const_cp_t ccec_cp_384();



extern struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha256_kek_aes128;
extern struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha512_kek_aes256;
extern struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha256_kek_aes128;
extern struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha512_kek_aes256;
extern struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p256;
extern struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p521;

CC_NONNULL_TU((1,3)) CC_NONNULL2
int ccec_compact_generate_key(ccec_const_cp_t cp,  struct ccrng_state *rng,
                              ccec_full_ctx_t key);
CC_NONNULL_TU((3)) CC_NONNULL2
void ccec_compact_export(const int fullkey, void *out, ccec_full_ctx_t key);

void ccec_compact_export_pub(void* out, ccec_pub_ctx_t key);

CC_INLINE CC_CONST CC_NONNULL_TU((2))
size_t ccec_compact_export_size(const int fullkey, ccec_full_ctx_t key){
    return (((ccec_ctx_bitlen(key)+7)/8) * ((fullkey == 1) + 1));
}

size_t ccec_x963_import_pub_size(size_t in_len);
int ccec_keysize_is_supported(size_t keysize);
size_t ccec_x963_import_priv_size(size_t in_len);

CC_CONST ccec_const_cp_t ccec_cp_192(void);
CC_CONST ccec_const_cp_t ccec_cp_224(void);
CC_CONST ccec_const_cp_t ccec_cp_256(void);
CC_CONST ccec_const_cp_t ccec_cp_384(void);
CC_CONST ccec_const_cp_t ccec_cp_521(void);

CC_NONNULL_TU((1,4)) CC_NONNULL((3))
int ccec_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in,
                    ccec_pub_ctx_t key);

CC_NONNULL_TU((1))
size_t
ccec_signature_r_s_size(ccec_full_ctx_t key);

CC_NONNULL_TU((1)) CC_NONNULL((3, 4, 5, 6))
int ccec_verify_composite(ccec_pub_ctx_t key, size_t digest_len, const uint8_t *digest,
                          uint8_t *sig_r, uint8_t *sig_s, bool *valid);

CC_NONNULL_TU((1)) CC_NONNULL((3, 5, 6))
int ccec_verify(ccec_pub_ctx_t key, size_t digest_len, const uint8_t *digest,
                size_t sig_len, const uint8_t *sig,  bool *valid);

CC_NONNULL_TU((1)) CC_NONNULL2
void ccec_export_pub(ccec_pub_ctx_t key, void *out);

CC_INLINE CC_CONST
size_t ccec_export_pub_size_cp(ccec_const_cp_t cp) {
    return 1 + 2 * ccec_cp_prime_size(cp);
}

CC_INLINE CC_CONST CC_NONNULL_TU((1))
size_t ccec_export_pub_size(ccec_pub_ctx_t key) {
    return ccec_export_pub_size_cp(ccec_ctx_cp(key));
}

CC_NONNULL_TU((1,4)) CC_NONNULL((3))
int ccec_der_import_priv(ccec_const_cp_t cp, size_t length, const uint8_t *data, ccec_full_ctx_t full_key);

CC_NONNULL_TU((1,3)) CC_NONNULL2
int ccec_generate_key_fips(ccec_const_cp_t cp,  struct ccrng_state *rng,
                           ccec_full_ctx_t key);

CC_NONNULL_TU((1)) CC_NONNULL((3, 4, 5, 6))
int ccec_sign_composite(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
                        uint8_t *sig_r, uint8_t *sig_s, struct ccrng_state *rng);

CC_NONNULL_TU((1)) CC_NONNULL((3, 4, 5, 6))
int ccec_sign(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
              size_t *sig_len, uint8_t *sig, struct ccrng_state *rng);

CC_NONNULL_TU((3)) CC_NONNULL((2,4))
int ccec_der_import_priv_keytype(size_t len, const uint8_t * data, ccoid_t *oid, size_t *n);

const ccec_const_cp_t ccec_curve_for_length_lookup(size_t keylen, ...);

CC_INLINE
void ccec_ctx_init(ccec_const_cp_t cp, ccec_pub_ctx_t key) {
    CCEC_PUB_CTX_HDR(key)->cp = cp;
}

size_t ccec_compact_import_pub_size(size_t in_len);

CC_NONNULL_TU((1,4)) CC_NONNULL3
int ccec_compact_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key);

CC_NONNULL_TU((1,2)) CC_NONNULL((3, 4))
int ccec_compute_key(ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key,
                     size_t *computed_key_len, uint8_t *computed_key);

CC_NONNULL_TU((1,3)) CC_NONNULL2
int ccec_generate_key(ccec_const_cp_t cp, struct ccrng_state *rng,
                      ccec_full_ctx_t key);

CC_NONNULL_TU((1,2)) CC_NONNULL((3, 4))
int ccecdh_compute_shared_secret(ccec_full_ctx_t private_key,
                                 ccec_pub_ctx_t public_key,
                                 size_t *computed_shared_secret_len, uint8_t *computed_shared_secret,
                                 struct ccrng_state *masking_rng);

CC_NONNULL_TU((1))
int ccec_get_pubkey_components(ccec_pub_ctx_t key, size_t *nbits,
                           uint8_t *x, size_t *xsize,
                           uint8_t *y, size_t *ysize);

CC_NONNULL_TU((1))
int ccec_get_fullkey_components(ccec_full_ctx_t key, size_t *nbits,
                            uint8_t *x, size_t *xsize,
                            uint8_t *y, size_t *ysize,
                            uint8_t *d, size_t *dsize);

CC_NONNULL_TU((6))
int ccec_make_pub(size_t nbits,
                  size_t xlength, uint8_t *x,
                  size_t ylength, uint8_t *y,
                  ccec_pub_ctx_t key);

CC_NONNULL_TU((8))
int ccec_make_priv(size_t nbits,
                   size_t xlength, uint8_t *x,
                   size_t ylength, uint8_t *y,
                   size_t klength, uint8_t *k,
                   ccec_full_ctx_t key);

CC_NONNULL_TU((1,4)) CC_NONNULL3
int ccec_x963_import_priv(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_full_ctx_t key);

CC_NONNULL_TU((1,4)) CC_NONNULL3
int ccec_x963_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key);

size_t ccec_compact_import_priv_size(size_t in_len);

CC_NONNULL_TU((1,4)) CC_NONNULL3
int ccec_compact_import_priv(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_full_ctx_t key);
 
// Guessed
int ccec_generate_key_deterministic(ccec_const_cp_t arg0, int arg1, unsigned char *arg2, struct ccrng_state *arg3, int arg4, struct ccec_full_ctx* arg5);

#define CCEC_GENKEY_DETERMINISTIC_COMPACT 0
#define CCEC_GENKEY_DETERMINISTIC_SECBKP 1
#define CCEC_GENKEY_DETERMINISTIC_LEGACY 2 /* arbitrary guess */

size_t ccec_diversify_min_entropy_len(ccec_const_cp_t cp);

int ccec_diversify_priv_twin(ccec_const_cp_t cp, cc_unit* priv, size_t entropyLen, void* entropy, struct ccrng_state* masking_rng, ccec_full_ctx_t key);
int ccec_diversify_pub_twin(ccec_const_cp_t cp, ccec_pub_ctx_t pub, size_t entropyLen, void* entropy, struct ccrng_state* masking_rng, ccec_pub_ctx_t key);

int ccec_validate_pub(ccec_pub_ctx_t pub);

int ccec_blind(struct ccrng_state *rng_state, ccec_full_ctx_t cc_eckey_private, ccec_pub_ctx_t ccec_cryptor_eckey_public, ccec_pub_ctx_t ccec_blinded_eckey_public);
int ccec_unblind(struct ccrng_state *rng_state, ccec_full_ctx_t cc_eckey_private, ccec_pub_ctx_t ccec_cryptor_eckey_public, ccec_pub_ctx_t ccec_unblinded_eckey_public);
int ccec_generate_blinding_keys(ccec_const_cp_t cp, struct ccrng_state *rng_state, ccec_full_ctx_t cc_k_eckey_private, ccec_full_ctx_t cc_kinv_eckey_private);

#endif
