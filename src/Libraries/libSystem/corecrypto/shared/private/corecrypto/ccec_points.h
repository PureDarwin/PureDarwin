#ifndef CC_PRIVATE_CCEC_POINTS_H
#define CC_PRIVATE_CCEC_POINTS_H

#include <corecrypto/ccn.h>
#include <corecrypto/ccec.h>

// a Jacobian point container
typedef struct ccec_projective_point_container {
	cc_size n;
	cc_unit* x;
	cc_unit* y;
	cc_unit* z;
} ccec_projective_point_container_t;

typedef struct ccec_const_projective_point_container {
	cc_size n;
	const cc_unit* x;
	const cc_unit* y;
	const cc_unit* z;
} ccec_const_projective_point_container_t;

// an affine point container
typedef struct ccec_affine_point_container {
	cc_size n;
	cc_unit* x;
	cc_unit* y;
} ccec_affine_point_container_t;

typedef struct ccec_const_affine_point_container {
	cc_size n;
	const cc_unit* x;
	const cc_unit* y;
} ccec_const_affine_point_container_t;

CC_INLINE
ccec_const_projective_point_container_t ccec_projective_point_container_constify(ccec_projective_point_container_t point) {
	return (ccec_const_projective_point_container_t) {
		.n = point.n,
		.x = point.x,
		.y = point.y,
		.z = point.z,
	};
};

CC_INLINE
ccec_const_affine_point_container_t ccec_affine_point_container_constify(ccec_affine_point_container_t point) {
	return (ccec_const_affine_point_container_t) {
		.n = point.n,
		.x = point.x,
		.y = point.y,
	};
};

// initialize a projective point to the point-at-infinity
void ccec_projective_point_container_init_at_infinity(cc_size n, ccec_projective_point_container_t point);

// create a projective point from an affine point
int ccec_projective_point_container_from_affine_point(ccec_projective_point_container_t projective, ccec_const_affine_point_container_t affine);

// check if the given point is the point-at-infinity
bool ccec_projective_point_container_is_at_infinity(ccec_const_projective_point_container_t point);

// copy a projective point to another container
int ccec_projective_point_container_copy(ccec_projective_point_container_t dest, ccec_const_projective_point_container_t src);

// create an affine point from a projective point
int ccec_affine_point_container_from_projective_point(ccec_const_cp_t curve, ccec_affine_point_container_t affine, ccec_const_projective_point_container_t projective);

// initialize an affine point to the point-at-infinity
void ccec_affine_point_container_init_at_infinity(cc_size n, ccec_affine_point_container_t point);

// check if the given point is the point-at-infinity
bool ccec_affine_point_container_is_at_infinity(ccec_const_affine_point_container_t point);

// copy an affine point to another container
int ccec_affine_point_container_copy(ccec_affine_point_container_t dest, ccec_const_affine_point_container_t src);

// point1 + point2 -> result
int ccec_projective_add_points(ccec_const_cp_t curve, ccec_projective_point_container_t result, ccec_const_projective_point_container_t point1, ccec_const_projective_point_container_t point2);

// point + point -> result
int ccec_projective_double_point(ccec_const_cp_t curve, ccec_projective_point_container_t result, ccec_const_projective_point_container_t point);

// k * point -> result
int ccec_projective_multiply(ccec_const_cp_t curve, ccec_projective_point_container_t result, ccec_const_projective_point_container_t point, cc_size nk, const cc_unit* k);

/*
int ccec_affine_add_points(ccec_const_cp_t curve, ccec_affine_point_container_t result, ccec_const_affine_point_container_t point1, ccec_const_affine_point_container_t point2);

int ccec_affine_double_point(ccec_const_cp_t curve, ccec_affine_point_container_t result, ccec_const_affine_point_container_t point);

int ccec_affine_multiply(ccec_const_cp_t curve, ccec_affine_point_container_t result, ccec_const_affine_point_container_t point, cc_size nk, const cc_unit* k);
*/

#endif // CC_PRIVATE_CCEC_POINTS_H
