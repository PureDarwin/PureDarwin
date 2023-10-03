#include <corecrypto/private/cczp_extra.h>
#include <stdbool.h>

void cczp_mul_mod(cczp_const_t zp, cc_unit* r, const cc_unit* a, const cc_unit* b) {
	const cc_size n = cczp_n((cczp_const_short_t)zp);

	cc_unit product[2 * n];

	ccn_mul(n, product, a, b);

	ccn_zero(n, r);

	cczp_mod(zp, r, product, NULL);
};

void cczp_sub_mod(cc_size n, cc_unit* result, const cc_unit* s, const cc_unit* t, cczp_const_t mod) {
	bool underflow = false;
	cc_unit mod_tmp[2 * n];

	memset(mod_tmp, 0, sizeof mod_tmp);

	underflow = ccn_sub(n, mod_tmp, s, t);

	cczp_mod(mod, result, mod_tmp, NULL);

	if (underflow)
		ccn_add(n, result, result, cczp_prime((cczp_const_short_t)mod));
};

void cczp_add_mod(cc_size n, cc_unit* result, const cc_unit* s, const cc_unit* t, cczp_const_t mod) {
	cc_unit mod_tmp[2 * n];

	memset(mod_tmp, 0, sizeof mod_tmp);

	mod_tmp[n] = ccn_add(n, mod_tmp, s, t);

	cczp_mod(mod, result, mod_tmp, NULL);
};
