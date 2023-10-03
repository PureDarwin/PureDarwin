// Clang is being weird
#define HIDE_COMPLAINT
#include <corecrypto/ccec.h>
#include <stdio.h>
#include <corecrypto/cc_debug.h>
#include <corecrypto/ccder.h>
#include <corecrypto/private/ccec_points.h>
#include <corecrypto/private/ccec_extra.h>
#include <corecrypto/private/ccn_extra.h>
#include <corecrypto/private/cczp_extra.h>

// essential reading: http://www.secg.org/sec1-v2.pdf

size_t ccec_compact_import_pub_size(size_t in_len) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

#define k_ccec_cp_t_null ((ccec_const_cp_t)((const struct cczp_prime*)(NULL)))

ccec_const_cp_t ccec_get_cp(size_t keysize) {
	switch (keysize) {
		case 192:
			return ccec_cp_192();
		case 224:
			return ccec_cp_224();
		case 256:
			return ccec_cp_256();
		case 384:
			return ccec_cp_384();
		case 521:
			return ccec_cp_521();
		default:
			break;
	}
	return k_ccec_cp_t_null;
}

void ccec_x963_export(const int fullkey, void *out, ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
int ccec_generate_key_legacy(ccec_const_cp_t cp,  struct ccrng_state *rng,
                             ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccec_rfc6637_wrap_key_size(ccec_pub_ctx_t public_key,
                                  unsigned long flags,
                                  size_t key_len) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_rfc6637_wrap_key(ccec_pub_ctx_t public_key,
                          void  *wrapped_key,
                          unsigned long flags,
                          uint8_t algid,
                          size_t key_len,
                          const void *key,
                          const struct ccec_rfc6637_curve *curve,
                          const struct ccec_rfc6637_wrap *wrap,
                          const uint8_t *fingerprint,
                          struct ccrng_state *rng) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

int ccec_rfc6637_unwrap_key(ccec_full_ctx_t private_key,
                            size_t *key_len,
                            void *key,
                            unsigned long flags,
                            uint8_t *symm_key_alg,
                            const struct ccec_rfc6637_curve *curve,
                            const struct ccec_rfc6637_unwrap *wrap,
                            const uint8_t *fingerprint,
                            size_t wrapped_key_len,
                            const void  *wrapped_key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

#if CCN_UNIT_SIZE == 8
	#define CC_UNIT_FORMAT_1(a) 0x00000000000000 ## a
	#define CC_UNIT_FORMAT_2(a, b)  0x000000000000 ## a ## b
	#define CC_UNIT_FORMAT_3(a, b, c) 0x0000000000 ## a ## b ## c
	#define CC_UNIT_FORMAT_4(a, b, c, d) 0x00000000 ## a ## b ## c ## d
	#define CC_UNIT_FORMAT_5(a, b, c, d, e) 0x000000 ## a ## b ## c ## d ## e
	#define CC_UNIT_FORMAT_6(a, b, c, d, e, f) 0x0000 ## a ## b ## c ## d ## e ## f
	#define CC_UNIT_FORMAT_7(a, b, c, d, e, f, g) 0x00 ## a ## b ## c ## d ## e ## f ## g
	#define CC_UNIT_FORMAT_8(a, b, c, d, e, f, g, h) 0x ## a ## b ## c ## d ## e ## f ## g ## h
#elif CCN_UNIT_SIZE == 4
	#define CC_UNIT_FORMAT_1(a) 0x000000 ## a
	#define CC_UNIT_FORMAT_2(a, b)  0x0000 ## a ## b
	#define CC_UNIT_FORMAT_3(a, b, c) 0x00 ## a ## b ## c
	#define CC_UNIT_FORMAT_4(a, b, c, d) 0x ## a ## b ## c ## d
	#define CC_UNIT_FORMAT_5(a, b, c, d, e) 0x ## b ## c ## d ## e, 0x000000 ## a
	#define CC_UNIT_FORMAT_6(a, b, c, d, e, f) 0x ## c ## d ## e ## f, 0x0000 ## a ## b
	#define CC_UNIT_FORMAT_7(a, b, c, d, e, f, g) 0x ## d ## e ## f ## g, 0x00 ## a ## b ## c,
	#define CC_UNIT_FORMAT_8(a, b, c, d, e, f, g, h) 0x ## e ## f ## g ## h, 0x ## a ## b ## c ## d
#elif CCN_UNIT_SIZE == 2
	#define CC_UNIT_FORMAT_1(a) 0x00 ## a
	#define CC_UNIT_FORMAT_2(a, b) 0x ## a ## b
	#define CC_UNIT_FORMAT_3(a, b, c) 0x ## b ## c, 0x00 ## a
	#define CC_UNIT_FORMAT_4(a, b, c, d) 0x ## c ## d, 0x ## a ## b
	#define CC_UNIT_FORMAT_5(a, b, c, d, e) 0x ## d ## e, 0x ## b ## c, 0x00 ## a
	#define CC_UNIT_FORMAT_6(a, b, c, d, e, f) 0x ## e ## f, 0x ## c ## d, 0x ## a ## b
	#define CC_UNIT_FORMAT_7(a, b, c, d, e, f, g) 0x ## f ## g, 0x ## d ## e, 0x ## b ## c 0x00 ## a
	#define CC_UNIT_FORMAT_8(a, b, c, d, e, f, g, h) 0x ## g ## h, 0x ## e ## f, 0x ## c ## d, 0x ## a ## b
#elif CCN_UNIT_SIZE == 1
	#define CC_UNIT_FORMAT_1(a) 0x ## a
	#define CC_UNIT_FORMAT_2(a, b) 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_3(a, b, c) 0x ## c, 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_4(a, b, c, d) 0x ## d, 0x ## c, 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_5(a, b, c, d, e) 0x ## e, 0x ## d, 0x ## c, 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_6(a, b, c, d, e, f) 0x ## f, 0x ## e, 0x ## d, 0x ## c, 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_7(a, b, c, d, e, f, g) 0x ## g, 0x ## f, 0x ## e, 0x ## d, 0x ## c, 0x ## b, 0x ## a
	#define CC_UNIT_FORMAT_8(a, b, c, d, e, f, g, h) 0x ## h, 0x ## g, 0x ## f, 0x ## e, 0x ## d, 0x ## c, 0x ## b, 0x ## a
#else
	#error "Invalid CCN unit size"
#endif


// everything in these arrays is little endian
// except for the order of the arguments to CC_UNIT_FORMAT_8
// the hex digit arguments to it should be in normal big-endian hex format

static const struct {
	struct cczp hdr;
	cc_unit ccn[(8 * ccn_nof(192)) + 3];
} k_ccec_cp_192 = {
	.hdr = (const struct cczp) {
		.n = ccn_nof(192),
		.options = 0,
		.mod_prime = &cczp_mod,
	},
	.ccn = {
		// modulus p {
			// full hex = fffffffffffffffffffffffffffffffeffffffffffffffff
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// reciprocal of modulus p {
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_1(01),
		// }
		// coefficient a {
			// full hex = fffffffffffffffffffffffffffffffefffffffffffffffc
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fc),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// coefficient b {
			// full hex = 64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1
			CC_UNIT_FORMAT_8(fe, b8, de, ec, c1, 46, b9, b1),
			CC_UNIT_FORMAT_8(0f, a7, e9, ab, 72, 24, 30, 49),
			CC_UNIT_FORMAT_8(64, 21, 05, 19, e5, 9c, 80, e7),
		// }
		// x value of coordinate G {
			// full hex = 188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012
			CC_UNIT_FORMAT_8(f4, ff, 0a, fd, 82, ff, 10, 12),
			CC_UNIT_FORMAT_8(7c, bf, 20, eb, 43, a1, 88, 00),
			CC_UNIT_FORMAT_8(18, 8d, a8, 0e, b0, 30, 90, f6),
		// }
		// y value of coordinate G {
			// full hex = 07192b95ffc8da78631011ed6b24cdd573f977a11e794811
			CC_UNIT_FORMAT_8(73, f9, 77, a1, 1e, 79, 48, 11),
			CC_UNIT_FORMAT_8(63, 10, 11, ed, 6b, 24, cd, d5),
			CC_UNIT_FORMAT_8(07, 19, 2b, 95, ff, c8, da, 78),
		// }
		// order n {
			// full hex = ffffffffffffffffffffffff99def836146bc9b1b4d22831
			CC_UNIT_FORMAT_8(14, 6b, c9, b1, b4, d2, 28, 31),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 99, de, f8, 36),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// reciprocal of order n {
			// full hex = 1000000000000000000000000662107c9eb94364e4b2dd7cf
			CC_UNIT_FORMAT_8(eb, 94, 36, 4e, 4b, 2d, d7, cf),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 66, 21, 07, c9),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_1(01),
		// }
		// cofactor h {
			// full hex = 01
			CC_UNIT_FORMAT_1(01),
		// }
	},
};

static const struct {
	struct cczp hdr;
	cc_unit ccn[(8 * ccn_nof(224)) + 3];
} k_ccec_cp_224 = {
	.hdr = (const struct cczp) {
		.n = ccn_nof(224),
		.options = 0,
		.mod_prime = &cczp_mod,
	},
	.ccn = {
		// modulus p {
			// full hex = ffffffffffffffffffffffffffffffff000000000000000000000001
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(FF, FF, FF, FF, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(FF, FF, FF, FF, FF, FF, FF, FF),
			CC_UNIT_FORMAT_4(FF, FF, FF, FF),
		// }
		// reciprocal of modulus p {
			// full hex = 0100000000000000000000000000000000ffffffffffffffffffffffff
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_5(01, 00, 00, 00, 00),
			// necessary because the reciprocal *must* be `n + 1` units in length
			#if CCN_UNIT_SIZE >= 5
				CC_UNIT_FORMAT_1(00),
			#endif
		// }
		// coefficient a {
			// full hex = fffffffffffffffffffffffffffffffefffffffffffffffffffffffe
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(ff, ff, ff, fe, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_4(ff, ff, ff, ff),
		// }
		// coefficient b {
			// full hex = b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4
			CC_UNIT_FORMAT_8(27, 0b, 39, 43, 23, 55, ff, b4),
			CC_UNIT_FORMAT_8(50, 44, b0, b7, d7, bf, d8, ba),
			CC_UNIT_FORMAT_8(0c, 04, b3, ab, f5, 41, 32, 56),
			CC_UNIT_FORMAT_4(b4, 05, 0a, 85),
		// }
		// x value of coordinate G {
			// full hex = b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21
			CC_UNIT_FORMAT_8(34, 32, 80, d6, 11, 5c, 1d, 21),
			CC_UNIT_FORMAT_8(4a, 03, c1, d3, 56, c2, 11, 22),
			CC_UNIT_FORMAT_8(6b, b4, bf, 7f, 32, 13, 90, b9),
			CC_UNIT_FORMAT_4(b7, 0e, 0c, bd),
		// }
		// y value of coordinate G {
			// full hex = bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34
			CC_UNIT_FORMAT_8(44, d5, 81, 99, 85, 00, 7e, 34),
			CC_UNIT_FORMAT_8(cd, 43, 75, a0, 5a, 07, 47, 64),
			CC_UNIT_FORMAT_8(b5, f7, 23, fb, 4c, 22, df, e6),
			CC_UNIT_FORMAT_4(bd, 37, 63, 88),
		// }
		// order n {
			// full hex = ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d
			CC_UNIT_FORMAT_8(13, dd, 29, 45, 5c, 5c, 2a, 3d),
			CC_UNIT_FORMAT_8(ff, ff, 16, a2, e0, b8, f0, 3e),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_4(ff, ff, ff, ff),
		// }
		// reciprocal of order n {
			// full hex = 10000000000000000000000000000e95d1f470fc1ec22d6baa3a3d5c3
			CC_UNIT_FORMAT_8(ec, 22, d6, ba, a3, a3, d5, c3),
			CC_UNIT_FORMAT_8(00, 00, e9, 5d, 1f, 47, 0f, c1),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_5(01, 00, 00, 00, 00),
			// necessary because the reciprocal *must* be `n + 1` units in length
			#if CCN_UNIT_SIZE >= 5
				CC_UNIT_FORMAT_1(00),
			#endif
		// }
		// cofactor h {
			// full hex = 01
			CC_UNIT_FORMAT_1(01),
		// }
	},
};

static const struct {
	struct cczp hdr;
	cc_unit ccn[(8 * ccn_nof(256)) + 3];
} k_ccec_cp_256 = {
	.hdr = (const struct cczp) {
		.n = ccn_nof(256),
		.options = 0,
		.mod_prime = &cczp_mod,
	},
	.ccn = {
		// modulus p {
			// full hex = ffffffff00000001000000000000000000000000ffffffffffffffffffffffff
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 01),
		// }
		// reciprocal of modulus p {
			// full hex = 0100000000fffffffffffffffefffffffefffffffeffffffff0000000000000003
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 03),
			CC_UNIT_FORMAT_8(ff, ff, ff, fe, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, fe, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_1(01),
		// }
		// coefficient a {
			// full hex = ffffffff00000001000000000000000000000000fffffffffffffffffffffffc
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fc),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 01),
		// }
		// coefficient b {
			// full hex = 5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
			CC_UNIT_FORMAT_8(3b, ce, 3c, 3e, 27, d2, 60, 4b),
			CC_UNIT_FORMAT_8(65, 1d, 06, b0, cc, 53, b0, f6),
			CC_UNIT_FORMAT_8(b3, eb, bd, 55, 76, 98, 86, bc),
			CC_UNIT_FORMAT_8(5a, c6, 35, d8, aa, 3a, 93, e7),
		// }
		// x value of coordinate G {
			// full hex = 6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296
			CC_UNIT_FORMAT_8(f4, a1, 39, 45, d8, 98, c2, 96),
			CC_UNIT_FORMAT_8(77, 03, 7d, 81, 2d, eb, 33, a0),
			CC_UNIT_FORMAT_8(f8, bc, e6, e5, 63, a4, 40, f2),
			CC_UNIT_FORMAT_8(6b, 17, d1, f2, e1, 2c, 42, 47),
		// }
		// y value of coordinate G {
			// full hex = 4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5
			CC_UNIT_FORMAT_8(cb, b6, 40, 68, 37, bf, 51, f5),
			CC_UNIT_FORMAT_8(2b, ce, 33, 57, 6b, 31, 5e, ce),
			CC_UNIT_FORMAT_8(8e, e7, eb, 4a, 7c, 0f, 9e, 16),
			CC_UNIT_FORMAT_8(4f, e3, 42, e2, fe, 1a, 7f, 9b),
		// }
		// order n {
			// full hex = ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
			CC_UNIT_FORMAT_8(f3, b9, ca, c2, fc, 63, 25, 51),
			CC_UNIT_FORMAT_8(bc, e6, fa, ad, a7, 17, 9e, 84),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 00),
		// }
		// reciprocal of order n {
			// full hex = 100000000fffffffffffffffeffffffff43190552df1a6c21012ffd85eedf9bfe
			CC_UNIT_FORMAT_8(01, 2f, fd, 85, ee, df, 9b, fe),
			CC_UNIT_FORMAT_8(43, 19, 05, 52, df, 1a, 6c, 21),
			CC_UNIT_FORMAT_8(ff, ff, ff, fe, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_1(01),
		// }
		// cofactor h {
			// full hex = 01
			CC_UNIT_FORMAT_1(01),
		// }
	},
};

static const struct {
	struct cczp hdr;
	cc_unit ccn[(8 * ccn_nof(384)) + 3];
} k_ccec_cp_384 = {
	.hdr = (const struct cczp) {
		.n = ccn_nof(384),
		.options = 0,
		.mod_prime = &cczp_mod,
	},
	.ccn = {
		// modulus p {
			// full hex = fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// reciprocal of modulus p {
			// full hex = 01000000000000000000000000000000000000000000000000000000000000000100000000ffffffffffffffff00000001
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_1(01),
		// }
		// coefficient a {
			// full hex = fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000fffffffc
			CC_UNIT_FORMAT_8(00, 00, 00, 00, ff, ff, ff, fc),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fe),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// coefficient b {
			// full hex = b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef
			CC_UNIT_FORMAT_8(2a, 85, c8, ed, d3, ec, 2a, ef),
			CC_UNIT_FORMAT_8(c6, 56, 39, 8d, 8a, 2e, d1, 9d),
			CC_UNIT_FORMAT_8(03, 14, 08, 8f, 50, 13, 87, 5a),
			CC_UNIT_FORMAT_8(18, 1d, 9c, 6e, fe, 81, 41, 12),
			CC_UNIT_FORMAT_8(98, 8e, 05, 6b, e3, f8, 2d, 19),
			CC_UNIT_FORMAT_8(b3, 31, 2f, a7, e2, 3e, e7, e4),
		// }
		// x value of coordinate G {
			// full hex = aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7
			CC_UNIT_FORMAT_8(3a, 54, 5e, 38, 72, 76, 0a, b7),
			CC_UNIT_FORMAT_8(55, 02, f2, 5d, bf, 55, 29, 6c),
			CC_UNIT_FORMAT_8(59, f7, 41, e0, 82, 54, 2a, 38),
			CC_UNIT_FORMAT_8(6e, 1d, 3b, 62, 8b, a7, 9b, 98),
			CC_UNIT_FORMAT_8(8e, b1, c7, 1e, f3, 20, ad, 74),
			CC_UNIT_FORMAT_8(aa, 87, ca, 22, be, 8b, 05, 37),
		// }
		// y value of coordinate G {
			// full hex = 3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f
			CC_UNIT_FORMAT_8(7a, 43, 1d, 7c, 90, ea, 0e, 5f),
			CC_UNIT_FORMAT_8(0a, 60, b1, ce, 1d, 7e, 81, 9d),
			CC_UNIT_FORMAT_8(e9, da, 31, 13, b5, f0, b8, c0),
			CC_UNIT_FORMAT_8(f8, f4, 1d, bd, 28, 9a, 14, 7c),
			CC_UNIT_FORMAT_8(5d, 9e, 98, bf, 92, 92, dc, 29),
			CC_UNIT_FORMAT_8(36, 17, de, 4a, 96, 26, 2c, 6f),
		// }
		// order n {
			// full hex = ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973
			CC_UNIT_FORMAT_8(ec, ec, 19, 6a, cc, c5, 29, 73),
			CC_UNIT_FORMAT_8(58, 1a, 0d, b2, 48, b0, a7, 7a),
			CC_UNIT_FORMAT_8(c7, 63, 4d, 81, f4, 37, 2d, df),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
		// }
		// reciprocal of order n {
			// full hex = 1000000000000000000000000000000000000000000000000389cb27e0bc8d220a7e5f24db74f58851313e695333ad68d
			CC_UNIT_FORMAT_8(13, 13, e6, 95, 33, 3a, d6, 8d),
			CC_UNIT_FORMAT_8(a7, e5, f2, 4d, b7, 4f, 58, 85),
			CC_UNIT_FORMAT_8(38, 9c, b2, 7e, 0b, c8, d2, 20),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_1(01),
		// }
		// cofactor h {
			// full hex = 01
			CC_UNIT_FORMAT_1(01),
		// }
	},
};

static const struct {
	struct cczp hdr;
	cc_unit ccn[(8 * ccn_nof(521)) + 3];
} k_ccec_cp_521 = {
	.hdr = (const struct cczp) {
		.n = ccn_nof(521),
		.options = 0,
		.mod_prime = &cczp_mod,
	},
	.ccn = {
		// modulus p {
			// full hex = 01ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_2(01, ff),
		// }
		// reciprocal of modulus p {
			// full hex = 20000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 01),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_2(02, 00),
			// necessary because the reciprocal *must* be `n + 1` units in length
			CC_UNIT_FORMAT_1(00),
		// }
		// coefficient a {
			// full hex = 01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fc),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_2(01, ff),
		// }
		// coefficient b {
			// full hex = 0051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00
			CC_UNIT_FORMAT_8(ef, 45, 1f, d4, 6b, 50, 3f, 00),
			CC_UNIT_FORMAT_8(35, 73, df, 88, 3d, 2c, 34, f1),
			CC_UNIT_FORMAT_8(16, 52, c0, bd, 3b, b1, bf, 07),
			CC_UNIT_FORMAT_8(56, 19, 39, 51, ec, 7e, 93, 7b),
			CC_UNIT_FORMAT_8(b8, b4, 89, 91, 8e, f1, 09, e1),
			CC_UNIT_FORMAT_8(a2, da, 72, 5b, 99, b3, 15, f3),
			CC_UNIT_FORMAT_8(92, 9a, 21, a0, b6, 85, 40, ee),
			CC_UNIT_FORMAT_8(95, 3e, b9, 61, 8e, 1c, 9a, 1f),
			CC_UNIT_FORMAT_2(00, 51),
		// }
		// x value of coordinate G {
			// full hex = 00c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66
			CC_UNIT_FORMAT_8(f9, 7e, 7e, 31, c2, e5, bd, 66),
			CC_UNIT_FORMAT_8(33, 48, b3, c1, 85, 6a, 42, 9b),
			CC_UNIT_FORMAT_8(fe, 1d, c1, 27, a2, ff, a8, de),
			CC_UNIT_FORMAT_8(a1, 4b, 5e, 77, ef, e7, 59, 28),
			CC_UNIT_FORMAT_8(f8, 28, af, 60, 6b, 4d, 3d, ba),
			CC_UNIT_FORMAT_8(9c, 64, 81, 39, 05, 3f, b5, 21),
			CC_UNIT_FORMAT_8(9e, 3e, cb, 66, 23, 95, b4, 42),
			CC_UNIT_FORMAT_8(85, 8e, 06, b7, 04, 04, e9, cd),
			CC_UNIT_FORMAT_2(00, c6),
		// }
		// y value of coordinate G {
			// full hex = 011839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650
			CC_UNIT_FORMAT_8(88, be, 94, 76, 9f, d1, 66, 50),
			CC_UNIT_FORMAT_8(35, 3c, 70, 86, a2, 72, c2, 40),
			CC_UNIT_FORMAT_8(c5, 50, b9, 01, 3f, ad, 07, 61),
			CC_UNIT_FORMAT_8(97, ee, 72, 99, 5e, f4, 26, 40),
			CC_UNIT_FORMAT_8(17, af, bd, 17, 27, 3e, 66, 2c),
			CC_UNIT_FORMAT_8(98, f5, 44, 49, 57, 9b, 44, 68),
			CC_UNIT_FORMAT_8(5c, 8a, 5f, b4, 2c, 7d, 1b, d9),
			CC_UNIT_FORMAT_8(39, 29, 6a, 78, 9a, 3b, c0, 04),
			CC_UNIT_FORMAT_2(01, 18),
		// }
		// order n {
			// full hex = 01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409
			CC_UNIT_FORMAT_8(bb, 6f, b7, 1e, 91, 38, 64, 09),
			CC_UNIT_FORMAT_8(3b, b5, c9, b8, 89, 9c, 47, ae),
			CC_UNIT_FORMAT_8(7f, cc, 01, 48, f7, 09, a5, d0),
			CC_UNIT_FORMAT_8(51, 86, 87, 83, bf, 2f, 96, 6b),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, fa),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_8(ff, ff, ff, ff, ff, ff, ff, ff),
			CC_UNIT_FORMAT_2(01, ff),
		// }
		// reciprocal of order n {
			// full hex = 2000000000000000000000000000000000000000000000000000000000000000005ae79787c40d069948033feb708f65a2fc44a36477663b851449048e16ec79bf7
			CC_UNIT_FORMAT_8(44, 90, 48, e1, 6e, c7, 9b, f7),
			CC_UNIT_FORMAT_8(c4, 4a, 36, 47, 76, 63, b8, 51),
			CC_UNIT_FORMAT_8(80, 33, fe, b7, 08, f6, 5a, 2f),
			CC_UNIT_FORMAT_8(ae, 79, 78, 7c, 40, d0, 69, 94),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 05),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_8(00, 00, 00, 00, 00, 00, 00, 00),
			CC_UNIT_FORMAT_2(02, 00),
			// necessary because the reciprocal *must* be `n + 1` units in length
			CC_UNIT_FORMAT_1(00),
		// }
		// cofactor h {
			// full hex = 01
			CC_UNIT_FORMAT_1(01),
		// }
	},
};

CC_CONST ccec_const_cp_t ccec_cp_192() {
	// the casts are necessary to shut up some compiler warnings
	return (ccec_const_cp_t)((const struct cczp_prime*)(&k_ccec_cp_192));
};

CC_CONST ccec_const_cp_t ccec_cp_224() {
	return (ccec_const_cp_t)((const struct cczp_prime*)(&k_ccec_cp_224));
};

CC_CONST ccec_const_cp_t ccec_cp_256() {
	return (ccec_const_cp_t)((const struct cczp_prime*)(&k_ccec_cp_256));
};

CC_CONST ccec_const_cp_t ccec_cp_384() {
	return (ccec_const_cp_t)((const struct cczp_prime*)(&k_ccec_cp_384));
};

CC_CONST ccec_const_cp_t ccec_cp_521(void) {
	return (ccec_const_cp_t)((const struct cczp_prime*)(&k_ccec_cp_521));
};

int ccec_compact_generate_key(ccec_const_cp_t cp,  struct ccrng_state *rng,
                              ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccec_compact_export(const int fullkey, void *out, ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccec_compact_export_pub(void* out, ccec_pub_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
};

size_t ccec_x963_import_pub_size(size_t in_len) {
	return ccn_bitsof_size((in_len - 1) / 2);
};

int ccec_keysize_is_supported(size_t keysize) {
	switch (keysize) {
		case 192:
		case 224:
		case 256:
		case 384:
		case 521:
			return 1;
		default:
			break;
	}
	return 0;
};

size_t ccec_x963_import_priv_size(size_t in_len) {
	return ccn_bitsof_size((in_len - 1) / 3);
};

int ccec_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t* in, ccec_pub_ctx_t key) {
	if (in_len < 1) {
		#if DEBUG
			printf("%s: empty public key\n", __PRETTY_FUNCTION__);
		#endif
		return -1;
	}

	if (in[0] != 0x04) {
		#if DEBUG
			printf("%s: unsupported public key encoding\n", __PRETTY_FUNCTION__);
		#endif
		return -1;
	}

	const cc_size n = cczp_n((cczp_const_short_t)cp.zp);
	const cc_size size = ccn_sizeof_n(n);

	if (in_len != (2 * size) + 1) {
		#if DEBUG
			printf("%s: invalid public key size (expected %zu bytes; got %zu)\n", __PRETTY_FUNCTION__, (2 * size) + 1, in_len);
		#endif
		return -1;
	}

	ccec_ctx_cp(key) = cp;

	cc_unit* x = ccec_ctx_x(key);
	cc_unit* y = ccec_ctx_y(key);
	cc_unit* z = ccec_ctx_z(key);

	ccn_read_uint(n, x, size, in + 1);
	ccn_read_uint(n, y, size, in + 1 + size);

	ccn_zero(n, z);
	z[0] = 1;

	return 0;
};

size_t
ccec_signature_r_s_size(ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_verify_composite(ccec_pub_ctx_t key, size_t digest_len, const uint8_t *digest,
                          uint8_t *sig_r, uint8_t *sig_s, bool *valid) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_verify(ccec_pub_ctx_t key, size_t digest_len, const uint8_t* digest, size_t sig_len, const uint8_t* sig, bool* valid) {
	const cc_size n = ccec_ctx_n(key);
	const ccec_const_cp_t curve = ccec_ctx_cp(key);
	const cc_unit* const order = ccec_cp_o(curve);
	const size_t log_2_n = ccn_bitlen(n, order);

	uint8_t zp_bytes[cczp_size(ccn_sizeof_n(n))];
	memset(zp_bytes, 0, sizeof zp_bytes);
	// casts necessary to shut some compiler warnings up
	cczp_t zp = (cczp_t)(struct cczp*)zp_bytes;
	CCZP_N(zp) = n;
	memcpy(CCZP_PRIME(zp), order, ccn_sizeof_n((2 * n) + 1));
	zp.zp->mod_prime = cczp_mod;

	cc_unit e[n];
	memset(e, 0, sizeof e);

	ccn_read_uint(n, e, (log_2_n >= 8 * digest_len) ? (log_2_n + 7) / 8 : digest_len, digest);

	cc_unit r[n];
	cc_unit s[n];
	memset(r, 0, sizeof r);
	memset(s, 0, sizeof s);

	const uint8_t* const der_end = sig + sig_len;
	const uint8_t* seq_end = NULL;
	const uint8_t* const seq_start = ccder_decode_sequence_tl(&seq_end, sig, der_end);
	const uint8_t* const s_start = ccder_decode_uint(n, r, seq_start, der_end);
	ccder_decode_uint(n, s, s_start, der_end);

	cc_unit s_inv[n];
	memset(s_inv, 0, sizeof s_inv);

	ccn_modular_multiplicative_inverse(n, s_inv, s, order);

	// double-sized so that we can use `cczp_mod` on it later
	cc_unit rx[2 * n];
	cc_unit ry[n];
	cc_unit rz[n];

	memset(rx, 0, sizeof rx);
	memset(ry, 0, sizeof ry);
	memset(rz, 0, sizeof rz);

	ccec_projective_point_container_t R = {
		.n = n,
		.x = rx,
		.y = ry,
		.z = rz,
	};

	{
		cc_unit u1[n];
		cc_unit u2[n];
		memset(u1, 0, sizeof u1);
		memset(u2, 0, sizeof u2);

		cczp_mul_mod(zp, u1, e, s_inv);
		cczp_mul_mod(zp, u2, r, s_inv);

		cc_unit u1gx[n];
		cc_unit u1gy[n];
		cc_unit u1gz[n];

		memset(u1gx, 0, sizeof u1gx);
		memset(u1gy, 0, sizeof u1gy);
		memset(u1gz, 0, sizeof u1gz);

		ccec_projective_point_container_t u1g = {
			.n = n,
			.x = u1gx,
			.y = u1gy,
			.z = u1gz,
		};

		cc_unit u2qx[n];
		cc_unit u2qy[n];
		cc_unit u2qz[n];

		memset(u2qx, 0, sizeof u2qx);
		memset(u2qy, 0, sizeof u2qy);
		memset(u2qz, 0, sizeof u2qz);

		ccec_projective_point_container_t u2q = {
			.n = n,
			.x = u2qx,
			.y = u2qy,
			.z = u2qz,
		};

		cc_unit tmp_z[n];
		memset(tmp_z, 0, sizeof tmp_z);
		tmp_z[0] = 1;

		ccec_projective_multiply(curve, u1g, (ccec_const_projective_point_container_t) {
			.n = n,
			.x = ccec_cp_x(curve),
			.y = ccec_cp_y(curve),
			.z = tmp_z,
		}, n, u1);

		ccec_projective_multiply(curve, u2q, (ccec_const_projective_point_container_t) {
			.n = n,
			.x = ccec_ctx_x(key),
			.y = ccec_ctx_y(key),
			.z = ccec_ctx_z(key),
		}, n, u2);

		ccec_projective_add_points(curve, R, ccec_projective_point_container_constify(u1g), ccec_projective_point_container_constify(u2q));
	};

	ccec_affine_point_container_from_projective_point(curve, (ccec_affine_point_container_t) {
		.n = n,
		.x = rx,
		.y = ry,
	}, ccec_projective_point_container_constify(R));

	cc_unit v[n];
	memset(v, 0, sizeof v);

	cczp_mod(zp, v, rx, NULL);

	*valid = !memcmp(v, r, sizeof v);

	return 0;
};

void ccec_export_pub(ccec_pub_ctx_t key, void* out) {
	const cc_size n = ccec_ctx_n(key);
	const cc_size total_size = ccec_export_pub_size(key);
	const cc_size int_size = (total_size - 1) / 2;

	uint8_t* data = out;

	data[0] = 0x04;

	ccn_write_uint_padded(n, ccec_ctx_x(key), int_size, data + 1);
	ccn_write_uint_padded(n, ccec_ctx_y(key), int_size, data + 1 + int_size);
};

int ccec_der_import_priv(ccec_const_cp_t cp, size_t length, const uint8_t *data, ccec_full_ctx_t full_key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_generate_key_fips(ccec_const_cp_t cp,  struct ccrng_state *rng,
                           ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_sign_composite(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
                        uint8_t *sig_r, uint8_t *sig_s, struct ccrng_state *rng) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_sign(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
              size_t *sig_len, uint8_t *sig, struct ccrng_state *rng) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_der_import_priv_keytype(size_t len, const uint8_t * data, ccoid_t *oid, size_t *n) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

const ccec_const_cp_t ccec_curve_for_length_lookup(size_t keylen, ...) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_compact_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_compute_key(ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key,
                     size_t *computed_key_len, uint8_t *computed_key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_generate_key(ccec_const_cp_t cp, struct ccrng_state* rng, ccec_full_ctx_t key) {
	ccec_ctx_cp(key) = cp;

	const cc_size n = ccec_cp_n(cp);
	const cc_size size = ccn_sizeof_n(n);

	cc_unit* x = ccec_ctx_x(key);
	cc_unit* y = ccec_ctx_y(key);
	cc_unit* z = ccec_ctx_z(key);
	cc_unit* k = ccec_ctx_k(key);

	ccn_zero(n, z);
	z[0] = 1;

	ccec_projective_point_container_t result = {
		.n = n,
		.x = x,
		.y = y,
		.z = z,
	};

	ccec_projective_point_container_init_at_infinity(n, result);
	ccn_zero(n, k);

	{
	// temporary fixed value for k, for debugging
	#if DEBUG
		// k = 0xf6e190614fcd4621dc96afb3da3ea63db115f6d946db03400eb2ea7f960db6bb
		cc_unit tmp[ccn_nof(256)] = {
			CC_UNIT_FORMAT_8(0e, b2, ea, 7f, 96, 0d, b6, bb),
			CC_UNIT_FORMAT_8(b1, 15, f6, d9, 46, db, 03, 40),
			CC_UNIT_FORMAT_8(dc, 96, af, b3, da, 3e, a6, 3d),
			CC_UNIT_FORMAT_8(f6, e1, 90, 61, 4f, cd, 46, 21),
		};
		memcpy(k, tmp, sizeof tmp);
	#else
		cc_unit tmp_k[2 * n];
		memset(tmp_k, 0, sizeof tmp_k);

		ccn_random_bits(ccec_cp_prime_bitlen(cp), tmp_k, rng);

		uint8_t tmp_zp_bytes[cczp_size(size)];
		memset(tmp_zp_bytes, 0, sizeof tmp_zp_bytes);
		// casts necessary to shut some compiler warnings up
		cczp_t tmp_zp = (cczp_t)(struct cczp*)tmp_zp_bytes;

		CCZP_N(tmp_zp) = n;
		memcpy(CCZP_PRIME(tmp_zp), ccec_cp_o(cp), ccn_sizeof_n((2 * n) + 1));
		tmp_zp.zp->mod_prime = cczp_mod;

		cczp_mod(tmp_zp, k, tmp_k, NULL);
	#endif
	};

	cc_unit tmpz[n];
	memset(tmpz, 0, sizeof tmpz);
	tmpz[0] = 1;
	ccec_projective_multiply(cp, result, (ccec_const_projective_point_container_t) {
		.n = n,
		.x = ccec_cp_x(cp),
		.y = ccec_cp_y(cp),
		.z = tmpz,
	}, n, k);

	ccec_affine_point_container_from_projective_point(cp, (ccec_affine_point_container_t) {
		.n = n,
		.x = x,
		.y = y,
	}, ccec_projective_point_container_constify(result));

	ccn_zero(n, z);
	z[0] = 1;

	return 0;
};

int ccecdh_compute_shared_secret(ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key, size_t* computed_shared_secret_len, uint8_t* computed_shared_secret, struct ccrng_state* masking_rng) {
	ccec_const_cp_t pub_curve = ccec_ctx_cp(public_key);

	// TODO: check that both keys are using the same curve
	//ccec_const_cp_t priv_curve = ccec_ctx_cp(public_key);

	const cc_size n = ccec_ctx_n(public_key);
	const cc_size size = ccn_sizeof_n(n);

	const cc_unit* k = ccec_ctx_k(private_key);

	ccec_const_projective_point_container_t pub_point = {
		.n = n,
		.x = ccec_ctx_x(public_key),
		.y = ccec_ctx_y(public_key),
		.z = ccec_ctx_z(public_key),
	};

	cc_unit product_x[n];
	cc_unit product_y[n];
	cc_unit product_z[n];

	memset(product_x, 0, sizeof product_x);
	memset(product_y, 0, sizeof product_y);
	memset(product_z, 0, sizeof product_z);

	ccec_projective_point_container_t product = {
		.n = n,
		.x = product_x,
		.y = product_y,
		.z = product_z,
	};

	ccec_projective_multiply(pub_curve, product, pub_point, n, k);

	cc_unit final_x[n];
	cc_unit final_y[n];

	memset(final_x, 0, sizeof final_x);
	memset(final_y, 0, sizeof final_y);

	ccec_affine_point_container_from_projective_point(pub_curve, (ccec_affine_point_container_t) {
		.n = n,
		.x = final_x,
		.y = final_y,
	}, ccec_projective_point_container_constify(product));

	memset(computed_shared_secret, 0, *computed_shared_secret_len);

	*computed_shared_secret_len = ccn_write_uint_size(n, final_x);

	ccn_write_uint_padded(n, final_x, *computed_shared_secret_len, computed_shared_secret);

	return 0;
};

int ccec_get_pubkey_components(ccec_pub_ctx_t key, size_t *nbits,
                           uint8_t *x, size_t *xsize,
                           uint8_t *y, size_t *ysize) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_get_fullkey_components(ccec_full_ctx_t key, size_t *nbits,
                            uint8_t *x, size_t *xsize,
                            uint8_t *y, size_t *ysize,
                            uint8_t *d, size_t *dsize) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_make_pub(size_t nbits,
                  size_t xlength, uint8_t *x,
                  size_t ylength, uint8_t *y,
                  ccec_pub_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_make_priv(size_t nbits,
                   size_t xlength, uint8_t *x,
                   size_t ylength, uint8_t *y,
                   size_t klength, uint8_t *k,
                   ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_x963_import_priv(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccec_x963_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccec_compact_import_priv_size(size_t in_len) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return 1;
}

int ccec_compact_import_priv(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}
 
// Guessed
int ccec_generate_key_deterministic(ccec_const_cp_t arg0, int arg1, unsigned char *arg2, struct ccrng_state *arg3, int arg4, struct ccec_full_ctx* arg5) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

size_t ccec_diversify_min_entropy_len(ccec_const_cp_t cp) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

int ccec_diversify_priv_twin(ccec_const_cp_t cp, cc_unit* priv, size_t entropyLen, void* entropy, struct ccrng_state* masking_rng, ccec_full_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

int ccec_diversify_pub_twin(ccec_const_cp_t cp, ccec_pub_ctx_t pub, size_t entropyLen, void* entropy, struct ccrng_state* masking_rng, ccec_pub_ctx_t key) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

int ccec_validate_pub(ccec_pub_ctx_t pub) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
};

int ccec_blind(struct ccrng_state *rng_state, ccec_full_ctx_t cc_eckey_private, ccec_pub_ctx_t ccec_cryptor_eckey_public, ccec_pub_ctx_t ccec_blinded_eckey_public) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

int ccec_unblind(struct ccrng_state *rng_state, ccec_full_ctx_t cc_eckey_private, ccec_pub_ctx_t ccec_cryptor_eckey_public, ccec_pub_ctx_t ccec_unblinded_eckey_public) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

int ccec_generate_blinding_keys(ccec_const_cp_t cp, struct ccrng_state *rng_state, ccec_full_ctx_t cc_k_eckey_private, ccec_full_ctx_t cc_kinv_eckey_private) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

struct ccec_rfc6637_wrap
{
    int dummy;
};

struct ccec_rfc6637_unwrap
{
    int dummy;
};

struct ccec_rfc6637_curve
{
    int dummy;
};



struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha256_kek_aes128;
struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha512_kek_aes256;
struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha256_kek_aes128;
struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha512_kek_aes256;
struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p256;
struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p521;

