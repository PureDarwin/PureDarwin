#include <corecrypto/private/ccec_extra.h>
#include <corecrypto/private/ccec_points.h>
#include <corecrypto/private/cczp_extra.h>
#include <corecrypto/cc_debug.h>

// https://en.wikibooks.org/wiki/Cryptography/Prime_Curve/Jacobian_Coordinates

int ccec_projective_add_points(ccec_const_cp_t curve, ccec_projective_point_container_t result, ccec_const_projective_point_container_t point1, ccec_const_projective_point_container_t point2) {
	if (point1.n != point2.n)
		return -1;
	if (point1.n != result.n)
		return -1;

	const cc_size n = point1.n;
	const cc_size size = ccn_sizeof_n(n);
	cczp_const_t zp = (cczp_const_t)curve.zp;

	bool point1_is_at_infinity = ccec_projective_point_container_is_at_infinity(point1);
	bool point2_is_at_infinity = ccec_projective_point_container_is_at_infinity(point2);

	if (point1_is_at_infinity && point2_is_at_infinity) {
		ccec_projective_point_container_init_at_infinity(n, result);

		#if CCEC_PROJECTIVE_POINT_DEBUG
			puts("  result = point-at-infinity");
			puts("}");
		#endif

		return 0;
	} else if (point1_is_at_infinity) {
		ccec_projective_point_container_copy(result, point2);

		return 0;
	} else if (point2_is_at_infinity) {
		ccec_projective_point_container_copy(result, point1);

		return 0;
	}

	cc_unit u1[n];
	cc_unit u2[n];
	cc_unit s1[n];
	cc_unit s2[n];

	cczp_mul_mod(zp, u1, point1.x, point2.z);
	cczp_mul_mod(zp, u1, u1, point2.z);

	cczp_mul_mod(zp, u2, point2.x, point1.z);
	cczp_mul_mod(zp, u2, u2, point1.z);

	cczp_mul_mod(zp, s1, point1.y, point2.z);
	cczp_mul_mod(zp, s1, s1, point2.z);
	cczp_mul_mod(zp, s1, s1, point2.z);

	cczp_mul_mod(zp, s2, point2.y, point1.z);
	cczp_mul_mod(zp, s2, s2, point1.z);
	cczp_mul_mod(zp, s2, s2, point1.z);

	if (ccn_cmp(n, u1, u2) == 0) {
		if (ccn_cmp(n, s1, s2) == 0) {
			#if CCEC_PROJECTIVE_POINT_DEBUG
				puts("  error = wrong function (use `ccec_projective_double_point`)");
				puts("}");
			#endif

			return -1;
		} else {
			ccec_projective_point_container_init_at_infinity(n, result);

			#if CCEC_PROJECTIVE_POINT_DEBUG
				puts("  result = point-at-infinity");
				puts("}");
			#endif

			return 0;
		}
	}

	cc_unit h[n];
	cc_unit r[n];

	memset(h, 0, size);
	memset(r, 0, size);

	cczp_sub_mod(n, h, u2, u1, zp);
	cczp_sub_mod(n, r, s2, s1, zp);

	cc_unit h_squared_mod[n];
	memset(h_squared_mod, 0, size);
	cczp_mul_mod(zp, h_squared_mod, h, h);

	cc_unit h_cubed_mod[n];
	memset(h_cubed_mod, 0, size);
	cczp_mul_mod(zp, h_cubed_mod, h_squared_mod, h);

	cc_unit u1_h_squared[n];
	memset(u1_h_squared, 0, size);
	cczp_mul_mod(zp, u1_h_squared, u1, h_squared_mod);

	// calculate x of result
	{
		cc_unit r_squared_mod[2 * n];
		memset(r_squared_mod, 0, 2 * size);
		cczp_mul_mod(zp, result.x, r, r);

		cczp_sub_mod(n, result.x, result.x, h_cubed_mod, zp);
		cczp_sub_mod(n, result.x, result.x, u1_h_squared, zp);
		cczp_sub_mod(n, result.x, result.x, u1_h_squared, zp);
	};

	// calculate y of result
	{
		cczp_sub_mod(n, result.y, u1_h_squared, result.x, zp);

		cczp_mul_mod(zp, result.y, result.y, r);

		cc_unit s1_h_cubed[n];
		memset(s1_h_cubed, 0, size);
		cczp_mul_mod(zp, s1_h_cubed, s1, h_cubed_mod);

		cczp_sub_mod(n, result.y, result.y, s1_h_cubed, zp);
	};

	// calculate z of result
	{
		memset(result.z, 0, size);

		cczp_mul_mod(zp, result.z, h, point1.z);
		cczp_mul_mod(zp, result.z, result.z, point2.z);
	};

	return 0;
};

int ccec_projective_double_point(ccec_const_cp_t curve, ccec_projective_point_container_t result, ccec_const_projective_point_container_t point) {
	if (point.n != result.n)
		return -1;

	const cc_size n = point.n;
	const cc_size size = ccn_sizeof_n(n);
	cczp_const_t zp = (cczp_const_t)curve.zp;

	if (ccn_n(n, point.y) <= 1 && point.y[0] == 0) {
		ccec_projective_point_container_init_at_infinity(n, result);

		return 0;
	}

	cc_unit constant[n];
	memset(constant, 0, size);

	cc_unit s[n];
	memset(s, 0, size);

	cc_unit m[n];
	memset(m, 0, size);

	{
		constant[0] = 4;

		cczp_mul_mod(zp, s, constant, point.x);

		cc_unit y_squared_mod[n];
		memset(y_squared_mod, 0, size);
		cczp_mul_mod(zp, y_squared_mod, point.y, point.y);

		cczp_mul_mod(zp, s, s, y_squared_mod);
	};

	{
		constant[0] = 3;

		cczp_mul_mod(zp, m, point.x, point.x);

		cczp_mul_mod(zp, m, m, constant);

		cc_unit a_z_fourth[n];
		memset(a_z_fourth, 0, size);

		cczp_mul_mod(zp, a_z_fourth, point.z, point.z);
		cczp_mul_mod(zp, a_z_fourth, a_z_fourth, point.z);
		cczp_mul_mod(zp, a_z_fourth, a_z_fourth, point.z);
		cczp_mul_mod(zp, a_z_fourth, a_z_fourth, ccec_cp_a(curve));

		cczp_add_mod(n, m, m, a_z_fourth, zp);
	};

	// calculate x of result
	{
		cczp_mul_mod(zp, result.x, m, m);

		cczp_sub_mod(n, result.x, result.x, s, zp);
		cczp_sub_mod(n, result.x, result.x, s, zp);
	};

	// calculate y of result
	{
		cczp_sub_mod(n, result.y, s, result.x, zp);

		cczp_mul_mod(zp, result.y, result.y, m);

		cc_unit y_fourth[n];
		memset(y_fourth, 0, size);
		cczp_mul_mod(zp, y_fourth, point.y, point.y);
		cczp_mul_mod(zp, y_fourth, y_fourth, point.y);
		cczp_mul_mod(zp, y_fourth, y_fourth, point.y);

		constant[0] = 8;
		cczp_mul_mod(zp, y_fourth, y_fourth, constant);

		cczp_sub_mod(n, result.y, result.y, y_fourth, zp);
	};

	// calculate z of result
	{
		cczp_mul_mod(zp, result.z, point.y, point.z);

		cczp_add_mod(n, result.z, result.z, result.z, zp);
	};

	return 0;
};

int ccec_projective_multiply(ccec_const_cp_t curve, ccec_projective_point_container_t R, ccec_const_projective_point_container_t P, cc_size nk, const cc_unit* k) {
	const cc_size n = ccec_cp_n(curve);
	const cc_size size = ccn_sizeof_n(n);

	cc_unit nx[n];
	cc_unit ny[n];
	cc_unit nz[n];
	cc_unit qx[n];
	cc_unit qy[n];
	cc_unit qz[n];

	struct ccec_projective_point_container N = {
		.n = n,
		.x = nx,
		.y = ny,
		.z = nz,
	};
	struct ccec_projective_point_container Q = {
		.n = n,
		.x = qx,
		.y = qy,
		.z = qz,
	};

	ccec_projective_point_container_copy(N, P);

	ccec_projective_point_container_init_at_infinity(n, Q);

	const cc_size bits_in_k = ccn_bitlen(nk, k);

	for (cc_size i = 0; i < bits_in_k; ++i) {
		cc_unit tmpx[n];
		cc_unit tmpy[n];
		cc_unit tmpz[n];

		struct ccec_projective_point_container TMP = {
			.n = n,
			.x = tmpx,
			.y = tmpy,
			.z = tmpz,
		};

		if (ccn_bit(k, i)) {
			ccec_projective_point_container_init_at_infinity(n, TMP);

			ccec_projective_add_points(curve, TMP, ccec_projective_point_container_constify(Q), ccec_projective_point_container_constify(N));

			ccec_projective_point_container_copy(Q, ccec_projective_point_container_constify(TMP));
		}

		ccec_projective_point_container_init_at_infinity(n, TMP);

		ccec_projective_double_point(curve, TMP, ccec_projective_point_container_constify(N));

		ccec_projective_point_container_copy(N, ccec_projective_point_container_constify(TMP));
	}

	ccec_projective_point_container_copy(R, ccec_projective_point_container_constify(Q));

	return 0;
};
