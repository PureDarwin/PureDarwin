#include <corecrypto/ccckg.h>
#include <corecrypto/private/ccstubs.h>

size_t ccckg_sizeof_commitment(ccec_const_cp_t cp, const struct ccdigest_info* digest_info) {
	CC_STUB(0);
};

size_t ccckg_sizeof_share(ccec_const_cp_t cp, const struct ccdigest_info* digest_info) {
	CC_STUB(0);
};

size_t ccckg_sizeof_opening(ccec_const_cp_t cp, const struct ccdigest_info* digest_info) {
	CC_STUB(0);
};

size_t ccckg_sizeof_ctx(ccec_const_cp_t cp, const struct ccdigest_info* digest_info) {
	CC_STUB(0);
};

int ccckg_init(ccckg_ctx_t ctx, ccec_const_cp_t cp, const struct ccdigest_info* digest_info, struct ccrng_state* rng) {
	CC_STUB_ERR();
};

int ccckg_contributor_commit(ccckg_ctx_t ctx, size_t commitment_length, void* commitment) {
	CC_STUB_ERR();
};

int ccckg_contributor_finish(ccckg_ctx_t ctx, size_t share_length, const void* share, size_t opening_length, void* opening, ccec_pub_ctx_t ec_pub, size_t shared_key_length, void* shared_key) {
	CC_STUB_ERR();
};

int ccckg_owner_finish(ccckg_ctx_t ctx, size_t opening_length, const void* opening, ccec_full_ctx_t ec_full, size_t shared_key_length, void* shared_key) {
	CC_STUB_ERR();
};

int ccckg_owner_generate_share(ccckg_ctx_t ctx, size_t commitment_length, const void* commitment, size_t share_length, void* share) {
	CC_STUB_ERR();
};
