#ifndef _CORECRYPTO_CCDH_H_
#define _CORECRYPTO_CCDH_H_

#include <corecrypto/ccrng.h>
#include <corecrypto/cczp.h>

#define CCDH_ERROR_DEFAULT                  -1
#define CCDH_GENERATE_KEY_TOO_MANY_TRIES    -2
#define CCDH_NOT_SUPPORTED_CONFIGURATION    -3
#define CCDH_SAFETY_CHECK                   -4
#define CCDH_PUBLIC_KEY_MISSING             -5
#define CCDH_INVALID_DOMAIN_PARAMETER       -6
#define CCDH_INVALID_INPUT                  -7
#define CCDH_DOMAIN_PARAMETER_MISMATCH      -8

cc_aligned_struct(16) ccdh_gp;

// Unions/structs

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    cczp_t  zp;
    ccdh_gp *gp;   
} __attribute__((transparent_union)) ccdh_gp_t;

typedef union {
    cczp_const_t   zp;          
    const ccdh_gp  *gp;       
    ccdh_gp_t      _ncgp;
} __attribute__((transparent_union)) ccdh_const_gp_t;

#define CCDH_GP_T_ZP(gp) ((gp).zp)
#define CCDH_GP_T_GP(gp) ((gp).gp)

#define CCDH_CONST_GP_T_ZP(gp) ((gp).zp)
#define CCDH_CONST_GP_T_GP(gp) ((gp).gp)
#else
typedef ccdh_gp* ccdh_gp_t;
typedef const ccdh_gp* ccdh_const_gp_t;

#define CCDH_GP_T_ZP(gp) ((cczp_t)(gp))
#define CCDH_GP_T_GP(gp) ((ccdh_gp*)(gp))

#define CCDH_CONST_GP_T_ZP(gp) ((cczp_const_t)(gp))
#define CCDH_CONST_GP_T_GP(gp) ((const ccdh_gp*)(gp))
#endif

struct ccdh_ctx_header {
    ccdh_const_gp_t     gp;
    uint8_t             pad[16 - sizeof(ccdh_const_gp_t *)];
} __attribute__((aligned(16)));

struct ccdh_ctx_body {
    struct ccdh_ctx_header  hdr;
    cc_unit              ccn[];
} __attribute__((aligned(16)));

struct ccdh_ctx_public {
    struct ccdh_ctx_header  hdr;
    cc_unit pub[];
} __attribute__((aligned(16)));

typedef struct ccdh_full_ctx {
    struct ccdh_ctx_header  hdr;
} __attribute__((aligned(16))) ccdh_full_ctx;

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    ccdh_full_ctx *_full;
    struct ccdh_ctx_header *hdr;
    struct ccdh_ctx_body *body;
    struct ccdh_ctx_public *pub;
} __attribute__((transparent_union)) ccdh_full_ctx_t;

#define CCDH_FULL_CTX_T_HDR(ctx) ((ctx).hdr)
#define CCDH_FULL_CTX_T_BODY(ctx) ((ctx).body)
#define CCDH_FULL_CTX_T_PUB(ctx) ((ctx).pub)
#else
typedef ccdh_full_ctx* ccdh_full_ctx_t;

#define CCDH_FULL_CTX_T_HDR(ctx) ((struct ccdh_ctx_header*)(ctx))
#define CCDH_FULL_CTX_T_BODY(ctx) ((struct ccdh_ctx_body*)(ctx))
#define CCDH_FULL_CTX_T_PUB(ctx) ((struct ccdh_ctx_public*)(ctx))
#endif

typedef struct ccdh_pub_ctx {
    struct ccdh_ctx_header  hdr;
} __attribute__((aligned(16))) ccdh_pub_ctx;

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    ccdh_pub_ctx *_pub;
    ccdh_full_ctx *_full;
    struct ccdh_ctx_header *hdr;
    struct ccdh_ctx_body *body;
    struct ccdh_ctx_public *pub;
    ccdh_full_ctx_t fullt;
} __attribute__((transparent_union)) ccdh_pub_ctx_t;

#define CCDH_PUB_CTX_T_HDR(ctx) ((ctx).hdr)
#define CCDH_PUB_CTX_T_BODY(ctx) ((ctx).body)
#define CCDH_PUB_CTX_T_PUB(ctx) ((ctx).pub)
#define CCDH_PUB_CTX_T_FULLT(ctx) ((ctx).fullt)
#else
typedef ccdh_pub_ctx* ccdh_pub_ctx_t;

#define CCDH_PUB_CTX_T_HDR(ctx) ((struct ccdh_ctx_header*)(ctx))
#define CCDH_PUB_CTX_T_BODY(ctx) ((struct ccdh_ctx_body*)(ctx))
#define CCDH_PUB_CTX_T_PUB(ctx) ((struct ccdh_ctx_public*)(ctx))
#define CCDH_PUB_CTX_T_FULLT(ctx) ((ccdh_full_ctx_t)(ctx))
#endif

// Functions
CC_NONNULL_TU((1,2)) CC_NONNULL3
int ccdh_compute_key(ccdh_full_ctx_t private_key, ccdh_pub_ctx_t public_key,
                     cc_unit *computed_key);

CC_NONNULL_TU((1)) CC_NONNULL3
int ccdh_import_pub(ccdh_const_gp_t gp, size_t in_len, const uint8_t *in,
                    ccdh_pub_ctx_t key);

CC_CONST CC_INLINE CC_NONNULL_TU((1))
cc_size ccdh_gp_n(ccdh_const_gp_t gp) {
    return cczp_n(CCDH_CONST_GP_T_ZP(gp));
}

CC_NONNULL_TU((1)) CC_NONNULL2
int ccdh_generate_key(ccdh_const_gp_t gp, struct ccrng_state *rng,
                      ccdh_full_ctx_t key);

CC_CONST CC_INLINE CC_NONNULL_TU((1))
size_t ccdh_ccn_size(ccdh_const_gp_t gp) {
    return ccn_sizeof_n(cczp_n(CCDH_CONST_GP_T_ZP(gp)));
}

CC_NONNULL_TU((1)) CC_NONNULL2
void ccdh_export_pub(ccdh_pub_ctx_t key, void *out);

#define ccdh_gp_prime_size(GP)  (ccdh_ccn_size(GP))
#define ccdh_ctx_gp(KEY)     (CCDH_PUB_CTX_T_HDR((ccdh_pub_ctx_t)(KEY))->gp)
CC_INLINE CC_CONST CC_NONNULL_TU((1))
size_t ccdh_export_pub_size(ccdh_pub_ctx_t key) {
    return ccdh_gp_prime_size(ccdh_ctx_gp(key));
}


// Macros

#define ccdh_pub_ctx_decl(_size_, _name_)   cc_ctx_decl(ccdh_pub_ctx, ccdh_pub_ctx_size(_size_), _name_)

#define ccdh_pub_ctx_decl_gp(_gp_, _name_)  ccdh_pub_ctx_decl(ccdh_ccn_size(_gp_), _name_)

#define ccdh_ctx_y(KEY)  (CCDH_PUB_CTX_T_BODY((ccdh_pub_ctx_t)(KEY))->ccn)

#define ccdh_full_ctx_size(_size_)  (sizeof(struct ccdh_ctx_header) + 2 * (_size_))

#define ccdh_gp_size(_size_) (cczp_size(_size_) + 2 * (_size_) + ccn_sizeof_n(1))

#define CCDH_GP_PRIME(_gp_)     (CCZP_PRIME(CCDH_GP_T_ZP(_gp_)))

#define CCDH_GP_N(_gp_)         (CCZP_N(CCDH_GP_T_ZP(_gp_)))

#define CCDH_GP_RECIP(_gp_)     (CCZP_RECIP(CCDH_GP_T_ZP(_gp_)))
#define CCDH_GP_G(_gp_)         (CCDH_GP_RECIP(_gp_) + 1 + ccdh_gp_n(_gp_))
#define CCDH_GP_N(_gp_)         (CCZP_N(CCDH_GP_T_ZP(_gp_)))
#define CCDH_GP_Q(_gp_)         (CCDH_GP_G(_gp_) + ccdh_gp_n(_gp_))
#define CCDH_GP_L(_gp_)         (*(cc_size *)((cc_unit *)(CCDH_GP_Q(_gp_) + ccdh_gp_n(_gp_))))
#define CCDH_GP_ZP(_gp_)        (CCDH_GP_T_ZP(_gp_))

#define ccdh_pub_ctx_size(_size_)   (sizeof(struct ccdh_ctx_header) + 1 * (_size_))

#define ccdh_gp_prime_bitlen(GP)  (ccn_bitlen(ccdh_gp_n(GP), ccdh_gp_prime(GP)))

#define ccdh_ctx_n(KEY)      (ccdh_gp_n(ccdh_ctx_gp(KEY)))

#define ccdh_ctx_public(KEY) ((ccdh_pub_ctx_t)(KEY))

CC_CONST CC_INLINE CC_NONNULL_TU((1))
const cc_unit *ccdh_gp_g(ccdh_const_gp_t gp) {
#if CORECRYPTO_USE_TRANSPARENT_UNION
    return CCDH_GP_G(gp._ncgp);
#else
    return CCDH_GP_G((ccdh_gp_t)gp);
#endif
}

CC_CONST CC_INLINE CC_NONNULL_TU((1))
const cc_unit *ccdh_gp_prime(ccdh_const_gp_t gp) {
    return cczp_prime(CCDH_CONST_GP_T_ZP(gp));
}

CC_NONNULL((1))
cc_size ccder_decode_dhparam_n(const uint8_t *der, const uint8_t *der_end);

CC_NONNULL_TU((1)) CC_NONNULL((2))
const uint8_t *ccder_decode_dhparams(ccdh_gp_t gp, const uint8_t *der, const uint8_t *der_end);

CC_INLINE CC_NONNULL_TU((1))
void ccdh_ctx_init(ccdh_const_gp_t gp, ccdh_pub_ctx_t key) {
    CCDH_PUB_CTX_T_HDR(key)->gp = gp;
}

CC_NONNULL_TU((1)) CC_NONNULL((3, 4))
int ccdh_init_gp(ccdh_gp_t gp, cc_size n, cc_unit *p, cc_unit *g, cc_size l);

int ccdh_compute_shared_secret(ccdh_full_ctx_t ctx, ccdh_pub_ctx_t pub, size_t* len, const void* key, struct ccrng_state* rng);

#endif
