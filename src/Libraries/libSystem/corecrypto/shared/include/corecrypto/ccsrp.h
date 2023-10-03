#ifndef SRP_H
#define SRP_H

#include <corecrypto/ccn.h>
#include <corecrypto/cczp.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccdh.h>

#include <stdbool.h>

typedef ccdh_gp ccsrp_gp;
typedef ccdh_gp_t ccsrp_gp_t;
typedef ccdh_const_gp_t ccsrp_const_gp_t;

#define CCSRP_HDR_PAD 32
struct ccsrp_ctx_header {
    const struct ccdigest_info *di;
    ccsrp_const_gp_t gp;
    struct {
        unsigned int authenticated:1;
        unsigned int noUsernameInX:1;
    } flags;
    uint8_t           pad[CCSRP_HDR_PAD - (sizeof(struct ccdigest_info *)+
                        sizeof(ccsrp_const_gp_t)+sizeof(bool))];
    cc_unit              ccn[1];
} __attribute__((aligned(16)));

typedef struct ccsrp_ctx {
    struct ccsrp_ctx_header  hdr;
} __attribute__((aligned(16))) ccsrp_ctx;

struct ccsrp_ctx_body {
    struct ccsrp_ctx_header  hdr;
    cc_unit              ccn[];
} __attribute__((aligned(16)));

#if CORECRYPTO_USE_TRANSPARENT_UNION
typedef union {
    ccsrp_ctx *_full;
    struct ccsrp_ctx_header *hdr;
    struct ccsrp_ctx_body *body;
} __attribute__((transparent_union)) ccsrp_ctx_t;

#define CCSRP_CTX_T_HDR(ctx) ((ctx).hdr)
#define CCSRP_CTX_T_BODY(ctx) ((ctx).body)
#else
typedef ccsrp_ctx* ccsrp_ctx_t;

#define CCSRP_CTX_T_HDR(ctx) ((struct ccsrp_ctx_header*)(ctx))
#define CCSRP_CTX_T_BODY(ctx) ((struct ccsrp_ctx_body*)(ctx))
#endif

#define ccsrp_gpbuf_size(_gp_) (ccdh_ccn_size(_gp_)*3)
#define ccsrp_dibuf_size(_di_) ((_di_)->output_size*3)

#define ccsrp_sizeof_srp(_di_,_gp_) sizeof(struct ccsrp_ctx_header)+\
    ccsrp_gpbuf_size(_gp_)+ccsrp_dibuf_size(_di_)

#define ccsrp_ctx_decl(_di_, _gp_, _name_) \
    cc_ctx_decl(ccsrp_ctx, ccsrp_sizeof_srp(_di_,_gp_), _name_)

#define ccsrp_ctx_gp(KEY)   (CCSRP_CTX_T_HDR((ccsrp_ctx_t)(KEY))->gp)
#define ccsrp_ctx_di(KEY)   (CCSRP_CTX_T_HDR((ccsrp_ctx_t)(KEY))->di)
#define ccsrp_ctx_zp(KEY)   (CCDH_CONST_GP_T_ZP(ccsrp_ctx_gp(KEY)))
#define ccsrp_ctx_gp_g(KEY)   (ccdh_gp_g(ccsrp_ctx_gp(KEY)))
#define ccsrp_ctx_gp_l(KEY)   (ccdh_gp_l(ccsrp_ctx_gp(KEY)))
#define ccsrp_ctx_n(KEY)      (ccdh_gp_n(ccsrp_ctx_gp(KEY)))
#define ccsrp_ctx_prime(KEY)  (ccdh_gp_prime(ccsrp_ctx_gp(KEY)))
#define ccsrp_ctx_ccn(KEY)  (CCSRP_CTX_T_HDR((ccsrp_ctx_t)(KEY))->ccn)
#define ccsrp_ctx_pki_key(KEY,_N_) (ccsrp_ctx_ccn(KEY) + ccsrp_ctx_n(KEY) * _N_)
#define ccsrp_ctx_public(KEY)  (ccsrp_ctx_pki_key(KEY,0))
#define ccsrp_ctx_private(KEY)  (ccsrp_ctx_pki_key(KEY,1))
#define ccsrp_ctx_v(KEY)  (ccsrp_ctx_pki_key(KEY,2))
#define ccsrp_ctx_K(KEY) ((uint8_t *)(ccsrp_ctx_pki_key(KEY,3)))
#define ccsrp_ctx_M(KEY)    (uint8_t *)(ccsrp_ctx_K(KEY)+\
            ccsrp_ctx_di(KEY)->output_size)
#define ccsrp_ctx_HAMK(KEY) (uint8_t *)(ccsrp_ctx_K(KEY)+\
            2*ccsrp_ctx_di(KEY)->output_size)

#define ccsrp_ctx_keysize(KEY)   (ccsrp_ctx_di(KEY)->output_size)

#define ccsrp_ctx_sizeof_n(KEY)   (ccn_sizeof_n(ccsrp_ctx_n(KEY)))

CC_INLINE void
ccsrp_ctx_init(ccsrp_ctx_t srp, const struct ccdigest_info *di, ccsrp_const_gp_t gp) {
    cc_zero(ccsrp_sizeof_srp(di, gp), CCSRP_CTX_T_HDR(srp));
    CCSRP_CTX_T_HDR(srp)->di = di;
    CCSRP_CTX_T_HDR(srp)->gp = gp;
    CCSRP_CTX_T_HDR(srp)->flags.authenticated = false;
}

#define CCSRP_ERROR_DEFAULT                 CCDH_ERROR_DEFAULT
#define CCSRP_GENERATE_KEY_TOO_MANY_TRIES   CCDH_GENERATE_KEY_TOO_MANY_TRIES
#define CCSRP_NOT_SUPPORTED_CONFIGURATION   CCDH_NOT_SUPPORTED_CONFIGURATION
#define CCSRP_SAFETY_CHECK                  CCDH_SAFETY_CHECK
#define CCSRP_PUBLIC_KEY_MISSING            CCDH_PUBLIC_KEY_MISSING
#define CCSRP_INVALID_DOMAIN_PARAMETER      CCDH_INVALID_DOMAIN_PARAMETER

int
ccsrp_generate_salt_and_verification(ccsrp_ctx_t srp, struct ccrng_state *rng,
                                     const char *username,
                                     size_t password_len, const void *password,
                                     size_t salt_len, void *salt,
                                     void *verifier);

int
ccsrp_generate_verifier(ccsrp_ctx_t srp,
			const char *username,
			size_t password_len, const void *password, 
			size_t salt_len, const void *salt,
			void *verifier);

int
ccsrp_server_generate_public_key(ccsrp_ctx_t srp, struct ccrng_state *rng,
                                 const void *verifier, void *B_bytes);

int
ccsrp_server_compute_session(ccsrp_ctx_t srp,
                              const void *username,
                              size_t salt_len, const void *salt,
                              const void *A_bytes);

int
ccsrp_server_start_authentication(ccsrp_ctx_t srp, struct ccrng_state *rng,
                                  const void *username,
                                  size_t salt_len, const void *salt,
                                  const void *verifier, const void *A_bytes,
                                  void *B_bytes);

bool
ccsrp_server_verify_session(ccsrp_ctx_t srp, const void *user_M,
                            void *HAMK_bytes);

int
ccsrp_client_start_authentication(ccsrp_ctx_t srp, struct ccrng_state *rng,
                                  void *A_bytes);

int
ccsrp_client_process_challenge(ccsrp_ctx_t srp,
                               const void *username,
                               size_t password_len, const void *password,
                               size_t salt_len, const void *salt,
                               const void *B_bytes,
                               void *M_bytes);

bool
ccsrp_client_verify_session(ccsrp_ctx_t srp, const uint8_t *HAMK_bytes);

CC_INLINE bool
ccsrp_client_set_noUsernameInX(ccsrp_ctx_t srp, bool flag)
{
    return CCSRP_CTX_T_HDR(srp)->flags.noUsernameInX = !!flag;
}

CC_INLINE bool
ccsrp_is_authenticated(ccsrp_ctx_t srp) {
	return CCSRP_CTX_T_HDR(srp)->flags.authenticated;
}


CC_INLINE size_t
ccsrp_exchange_size(ccsrp_ctx_t srp) {
    return ccsrp_ctx_sizeof_n(srp);
}

CC_INLINE size_t
ccsrp_session_size(ccsrp_ctx_t srp) {
    return ccsrp_ctx_keysize(srp);
}


CC_INLINE const void *
ccsrp_get_session_key(ccsrp_ctx_t srp, size_t *key_length) {
    *key_length = ccsrp_ctx_keysize(srp);
    return ccsrp_ctx_K(srp);
}

CC_INLINE size_t
ccsrp_get_session_key_length(ccsrp_ctx_t srp) {
    return ccsrp_ctx_keysize(srp);
}

int
ccsrp_test_calculations(const struct ccdigest_info *di, ccsrp_const_gp_t gp,
                        const void *username,
                        size_t password_len, const void *password,
                        size_t salt_len, const void *salt,
                        size_t k_len, const void *k,
                        size_t x_len, const void *x,
                        size_t v_len, const void *v,
                        size_t a_len, const void *a,
                        size_t b_len, const void *b,
                        size_t A_len, const void *A,
                        size_t B_len, const void *B,
                        size_t u_len, const void *u,
                        size_t S_len, const void *S
                        );

#endif

