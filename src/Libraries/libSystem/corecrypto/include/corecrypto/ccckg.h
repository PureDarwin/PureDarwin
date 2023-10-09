#ifndef _CORECRYPTO_CCCKG_H_
#define _CORECRYPTO_CCCKG_H_

#include <corecrypto/cc.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccrng.h>

typedef struct __attribute__((aligned(16))) ccckg_ctx {
	ccec_const_cp_t cp;
}* ccckg_ctx_t;

size_t ccckg_sizeof_commitment(ccec_const_cp_t cp, const struct ccdigest_info* digest_info);

size_t ccckg_sizeof_share(ccec_const_cp_t cp, const struct ccdigest_info* digest_info);

size_t ccckg_sizeof_opening(ccec_const_cp_t cp, const struct ccdigest_info* digest_info);

size_t ccckg_sizeof_ctx(ccec_const_cp_t cp, const struct ccdigest_info* digest_info);

int ccckg_init(ccckg_ctx_t ctx, ccec_const_cp_t cp, const struct ccdigest_info* digest_info, struct ccrng_state* rng);

int ccckg_contributor_commit(ccckg_ctx_t ctx, size_t commitment_length, void* commitment);

int ccckg_contributor_finish(ccckg_ctx_t ctx, size_t share_length, const void* share, size_t opening_length, void* opening, ccec_pub_ctx_t ec_pub, size_t shared_key_length, void* shared_key);

int ccckg_owner_finish(ccckg_ctx_t ctx, size_t opening_length, const void* opening, ccec_full_ctx_t ec_full, size_t shared_key_length, void* shared_key);

int ccckg_owner_generate_share(ccckg_ctx_t ctx, size_t commitment_length, const void* commitment, size_t share_length, void* share);

#endif // _CORECRYPTO_CCCKG_H_
