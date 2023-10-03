#ifndef _CC_PRIVATE_CC128_H_
#define _CC_PRIVATE_CC128_H_

//
// 128-bit arithmetic
//
// TODO: take advantadge of native architecture support for 128-bit integers when available
//

#include <stdint.h>
#include <stdbool.h>
#include <corecrypto/cc.h>
#include <corecrypto/cc_priv.h>

// little endian representation of a 128-bit integer
typedef struct {
	uint8_t bytes[16];
} cc128_t;

// note that this is a literal but *not* a constant
#define CC128_LITERAL16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
	((cc128_t) { .bytes = { 0x ## p, 0x ## o, 0x ## n, 0x ## m, 0x ## l, 0x ## k, 0x ## j, 0x ## i, 0x ## h, 0x ## g, 0x ## f, 0x ## e, 0x ## d, 0x ## c, 0x ## b, 0x ## a, }, })
#define CC128_LITERAL15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
	CC128_LITERAL16(0, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)
#define CC128_LITERAL14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
	CC128_LITERAL15(0, a, b, c, d, e, f, g, h, i, j, k, l, m, n)
#define CC128_LITERAL13(a, b, c, d, e, f, g, h, i, j, k, l, m) \
	CC128_LITERAL14(0, a, b, c, d, e, f, g, h, i, j, k, l, m)
#define CC128_LITERAL12(a, b, c, d, e, f, g, h, i, j, k, l) \
	CC128_LITERAL13(0, a, b, c, d, e, f, g, h, i, j, k, l)
#define CC128_LITERAL11(a, b, c, d, e, f, g, h, i, j, k) \
	CC128_LITERAL12(0, a, b, c, d, e, f, g, h, i, j, k)
#define CC128_LITERAL10(a, b, c, d, e, f, g, h, i, j) \
	CC128_LITERAL11(0, a, b, c, d, e, f, g, h, i, j)
#define  CC128_LITERAL9(a, b, c, d, e, f, g, h, i) \
	CC128_LITERAL10(0, a, b, c, d, e, f, g, h, i)
#define  CC128_LITERAL8(a, b, c, d, e, f, g, h) \
	CC128_LITERAL9(0, a, b, c, d, e, f, g, h)
#define  CC128_LITERAL7(a, b, c, d, e, f, g) \
	CC128_LITERAL8(0, a, b, c, d, e, f, g)
#define  CC128_LITERAL6(a, b, c, d, e, f) \
	CC128_LITERAL7(0, a, b, c, d, e, f)
#define  CC128_LITERAL5(a, b, c, d, e) \
	CC128_LITERAL6(0, a, b, c, d, e)
#define  CC128_LITERAL4(a, b, c, d) \
	CC128_LITERAL5(0, a, b, c, d)
#define  CC128_LITERAL3(a, b, c) \
	CC128_LITERAL4(0, a, b, c)
#define  CC128_LITERAL2(a, b) \
	CC128_LITERAL3(0, a, b)
#define  CC128_LITERAL1(a) \
	CC128_LITERAL2(0, a)

#define CC128_ZERO ((cc128_t) {0})

CC_INLINE
bool cc128_bit(const cc128_t* a, uint8_t bit_index) {
	return (a->bytes[bit_index / 8] >> (bit_index % 8)) & 1;
};

CC_INLINE
void cc128_xor(const cc128_t* a, const cc128_t* b, cc128_t* output) {
	cc_xor(sizeof(cc128_t), output, a, b);
};

CC_INLINE
void cc128_shift_right_once(const cc128_t* a, cc128_t* output) {
	for (uint8_t i = 0; i < 15; ++i)
		output->bytes[i] = ((a->bytes[i + 1] & 1) << 7) | (a->bytes[i] >> 1);
	output->bytes[15] = a->bytes[15] >> 1;
};

CC_INLINE
void cc128_mul(const cc128_t* a, const cc128_t* b, cc128_t* output) {
	cc128_t z = {0};
	cc128_t v = *b;

	for (uint8_t i = 128; i > 0; --i) {
		if (cc128_bit(a, i - 1)) {
			cc128_xor(&z, &v, &z);
		}
		bool lsb_v = cc128_bit(&v, 0);
		cc128_shift_right_once(&v, &v);
		if (lsb_v)
			v.bytes[15] ^= 0xe1;
	}

	*output = z;
};

/**
 * Increments the least significant word (32 bits) of the given block by 1.
 */
CC_INLINE
void cc128_lsw_increment(const cc128_t* a, cc128_t* output) {
	uint32_t lsw = 0;
	CC_LOAD32_LE(lsw, a->bytes);
	++lsw;
	CC_STORE32_LE(lsw, output->bytes);
	cc_copy(12, &output->bytes[4], &a->bytes[4]);
};

/**
 * Loads 128 bits from the given input buffer as a big-endian integer.
 * If there aren't enough bits available (i.e. input_length * 8 is less than 16),
 * the output will be padded with trailing zeros.
 */
CC_INLINE
void cc128_load_be(size_t input_length, const void* input, cc128_t* output) {
	const uint8_t* real_input = input;
	size_t limit = input_length < 16 ? input_length : 16;
	for (size_t i = 0; i < limit; ++i)
		output->bytes[15 - i] = real_input[i];
	for (size_t i = limit; i < 16; ++i)
		output->bytes[15 - i] = 0;
};

/**
 * Stores 128 bits into the given output buffer as a big-endian integer.
 * If there isn't enough space to store all 128 bits (i.e. output_length * 8 is less than 16),
 * the output will be truncated to the most significant bits that fit in the output buffer.
 */
CC_INLINE
void cc128_store_be(const cc128_t* input, size_t output_length, void* output) {
	uint8_t* real_output = output;
	size_t limit = output_length < 16 ? output_length : 16;
	for (size_t i = 0; i < limit; ++i)
		real_output[i] = input->bytes[15 - i];
};

/**
 * Stores the least significant given number of bits from the input block into the output block.
 * The input and output blocks MUST NOT overlap.
 */
CC_INLINE
void cc128_lsbits(const cc128_t* restrict input, uint8_t bits, cc128_t* restrict output) {
	*output = CC128_ZERO;
	uint8_t bytes = (bits + 7) / 8;

	for (uint8_t i = 0; i < bytes; ++i)
		output->bytes[i] = input->bytes[i];

	uint8_t final_byte_bits = bits % 8;
	output->bytes[bytes - 1] &= (0xff << (8 - final_byte_bits)) >> (8 - final_byte_bits);
};

/**
 * Stores the most significant given number of bits from the input block into the output block.
 * The input and output blocks MUST NOT overlap.
 */
CC_INLINE
void cc128_msbits(const cc128_t* restrict input, uint8_t bits, cc128_t* restrict output) {
	*output = CC128_ZERO;
	uint8_t bytes = (bits + 7) / 8;
	uint8_t diff = 16 - bytes;

	for (uint8_t i = 0; i < bytes; ++i)
		output->bytes[i] = input->bytes[i + diff];

	uint8_t final_byte_bits = bits % 8;
	output->bytes[bytes - 1] &= (0xff << (8 - final_byte_bits)) >> (8 - final_byte_bits);
};

#endif // _CC_PRIVATE_CC128_H_
