#include <corecrypto/private/ccec_points.h>
#include <corecrypto/private/ccn_extra.h>
#include <corecrypto/private/ccec_extra.h>

void ccec_projective_point_container_init_at_infinity(cc_size n, ccec_projective_point_container_t point) {
	point.n = n;

	ccn_zero(n, point.x);
	ccn_zero(n, point.y);
	ccn_zero(n, point.z);

	// point-at-infinity in Jacobian coordinates
	// (1 : 1 : 0)
	point.x[0] = 1;
	point.y[0] = 1;
	point.z[0] = 0;
};

int ccec_projective_point_container_from_affine_point(ccec_projective_point_container_t projective, ccec_const_affine_point_container_t affine) {
	if (projective.n < affine.n)
		return 0;

	const cc_size size = ccn_sizeof_n(affine.n);

	memcpy(projective.x, affine.x, size);
	memcpy(projective.y, affine.y, size);

	ccn_zero(projective.n, projective.z);
	projective.z[0] = 1;

	if (projective.n > affine.n) {
		const cc_size extra_size = ccn_sizeof_n(projective.n - affine.n);

		memset(projective.x + affine.n, 0, extra_size);
		memset(projective.y + affine.n, 0, extra_size);
	}

	return 0;
};

bool ccec_projective_point_container_is_at_infinity(ccec_const_projective_point_container_t point) {
	return ccn_n(point.n, point.x) <= 1 && ccn_n(point.n, point.y) <= 1 && ccn_n(point.n, point.z) <= 1 && point.x[0] == 1 && point.y[0] == 1 && point.z[0] == 0;
};

int ccec_projective_point_container_copy(ccec_projective_point_container_t dest, ccec_const_projective_point_container_t src) {
	if (dest.n < src.n)
		return -1;

	const cc_size size = ccn_sizeof_n(src.n);

	memcpy(dest.x, src.x, size);
	memcpy(dest.y, src.y, size);
	memcpy(dest.z, src.z, size);

	if (dest.n > src.n) {
		const cc_size extra_size = ccn_sizeof_n(dest.n - src.n);

		memset(dest.x + src.n, 0, extra_size);
		memset(dest.y + src.n, 0, extra_size);
		memset(dest.z + src.n, 0, extra_size);
	}

	return 0;
};

int ccec_affine_point_container_from_projective_point(ccec_const_cp_t curve, ccec_affine_point_container_t affine, ccec_const_projective_point_container_t projective) {
	if (affine.n < projective.n)
		return -1;

	cczp_const_t zp = (cczp_const_t)curve.zp;
	const cc_size size = ccn_sizeof_n(projective.n);

	cc_unit z_squared_mod[projective.n];
	memset(z_squared_mod, 0, size);

	{
		cc_unit z_squared[2 * projective.n];

		ccn_mul(projective.n, z_squared, projective.z, projective.z);

		cczp_mod(zp, z_squared_mod, z_squared, NULL);
	};

	{
		cc_unit z_squared_mod_inv[projective.n];
		memset(z_squared_mod_inv, 0, size);

		ccn_modular_multiplicative_inverse(projective.n, z_squared_mod_inv, z_squared_mod, ccec_cp_p(curve));

		cc_unit double_tmp[2 * projective.n];
		ccn_mul(projective.n, double_tmp, projective.x, z_squared_mod_inv);

		memset(affine.x, 0, size);
		cczp_mod(zp, affine.x, double_tmp, NULL);
	};

	{
		cc_unit z_cubed_mod_inv[projective.n];
		memset(z_cubed_mod_inv, 0, size);

		{
			cc_unit z_cubed_mod[projective.n];
			memset(z_cubed_mod, 0, size);

			{
				cc_unit z_cubed[2 * projective.n];

				ccn_mul(projective.n, z_cubed, z_squared_mod, projective.z);

				cczp_mod(zp, z_cubed_mod, z_cubed, NULL);
			};

			ccn_modular_multiplicative_inverse(projective.n, z_cubed_mod_inv, z_cubed_mod, ccec_cp_p(curve));
		};

		cc_unit double_tmp[2 * projective.n];
		ccn_mul(projective.n, double_tmp, projective.y, z_cubed_mod_inv);

		memset(affine.y, 0, size);
		cczp_mod(zp, affine.y, double_tmp, NULL);
	};

	if (affine.n > projective.n) {
		const cc_size extra_size = ccn_sizeof_n(affine.n - projective.n);

		memset(affine.x + projective.n, 0, extra_size);
		memset(affine.y + projective.n, 0, extra_size);
	}

	return 0;
};

// initialize an affine point to the point-at-infinity
void ccec_affine_point_container_init_at_infinity(cc_size n, ccec_affine_point_container_t point) {
	point.n = n;

	ccn_zero(n, point.x);
	ccn_zero(n, point.y);

	point.x[0] = 0;
	point.y[0] = 0;
};

// check if the given point is the point-at-infinity
bool ccec_affine_point_container_is_at_infinity(ccec_const_affine_point_container_t point) {
	return ccn_n(point.n, point.x) <= 1 && ccn_n(point.n, point.y) <= 1 && point.x[0] == 0 && point.y[0] == 0;
};

// copy an affine point to another container
int ccec_affine_point_container_copy(ccec_affine_point_container_t dest, ccec_const_affine_point_container_t src) {
	if (dest.n < src.n)
		return -1;

	const cc_size size = ccn_sizeof_n(src.n);

	memcpy(dest.x, src.x, size);
	memcpy(dest.y, src.y, size);

	if (dest.n > src.n) {
		const cc_size extra_size = ccn_sizeof_n(dest.n - src.n);

		memset(dest.x + src.n, 0, extra_size);
		memset(dest.y + src.n, 0, extra_size);
	}

	return 0;
};
