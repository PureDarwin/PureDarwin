#ifndef _CORECRYPTO_CCZ_H_
#define _CORECRYPTO_CCZ_H_

#include <corecrypto/ccn.h>
#include <corecrypto/ccrng.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define CCZ_INVALID_INPUT_ERROR -1
#define CCZ_INVALID_RADIX_ERROR -2

struct ccz {
    size_t n;
    struct ccz_class *isa;
    int sac;
    cc_unit *u;
};
typedef struct ccz ccz;

struct ccz_class {
	void *ctx;
	void *(*ccz_alloc)(void *, size_t);
	void *(*ccz_realloc)(void *, size_t, void *, size_t);
	void (*ccz_free)(void *, size_t, void *);
};

CC_NONNULL_ALL
size_t ccz_size(struct ccz_class *cls);

CC_NONNULL_ALL
void ccz_init(struct ccz_class *cls, ccz *r);

CC_NONNULL_ALL
void ccz_free(ccz *r);

CC_NONNULL_ALL
void ccz_zero(ccz *r);

CC_NONNULL_ALL
void ccz_set(ccz *r, const ccz *s);

CC_NONNULL1
void ccz_seti(ccz *r, uint64_t v);

CC_PURE CC_NONNULL_ALL
size_t ccz_bitlen(const ccz *s);

CC_PURE CC_NONNULL_ALL
size_t ccz_trailing_zeros(const ccz *s);

CC_PURE CC_NONNULL1
size_t ccz_write_uint_size(const ccz *s);

CC_NONNULL_ALL
void ccz_gcd(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_lcm(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_mulmod(ccz *r, const ccz *s, const ccz *t, const ccz *u);

CC_NONNULL_ALL
void ccz_sqrmod(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
int ccz_invmod(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_expmod(ccz *r, const ccz *s, const ccz *t, const ccz *u);

CC_PURE CC_NONNULL1
bool ccz_bit(const ccz *s, size_t k);

CC_NONNULL1
void ccz_set_bit(ccz *r, size_t k, bool value);

CC_NONNULL_ALL
bool ccz_is_prime(const ccz *s, unsigned depth);

CC_NONNULL((1,3))
void ccz_write_uint(const ccz *s, size_t out_size, void *out);

CC_PURE CC_NONNULL1
size_t ccz_write_int_size(const ccz *s);

CC_NONNULL((1,3))
void ccz_write_int(const ccz *s, size_t out_size, void *out);

CC_PURE CC_NONNULL1
size_t ccz_write_radix_size(const ccz *s, unsigned radix);

CC_NONNULL((1,3))
void ccz_write_radix(const ccz *s, size_t out_size, void *out, unsigned radix);

CC_NONNULL((1,3))
void ccz_read_uint(ccz *r, size_t data_size, const uint8_t *data);

CC_NONNULL((1,3))
void ccz_read_int(ccz *r, size_t data_size, const uint8_t *data);

CC_NONNULL((1,3))
int ccz_read_radix(ccz *r, size_t data_size, const char *data, unsigned radix);

CC_PURE CC_NONNULL_ALL
int ccz_cmp(const ccz *s, const ccz *t);

CC_PURE CC_NONNULL_ALL
int ccz_cmpi(const ccz *s, uint32_t v);

CC_NONNULL_ALL
void ccz_neg(ccz *r);

CC_NONNULL_ALL
void ccz_add(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_addi(ccz *r, const ccz *s, uint32_t v);

CC_NONNULL_ALL
void ccz_sub(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_subi(ccz *r, const ccz *s, uint32_t v);

CC_NONNULL_ALL
void ccz_mul(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_muli(ccz *r, const ccz *s, uint32_t v);

CC_NONNULL((3,4))
void ccz_divmod(ccz *q, ccz *r, const ccz *s, const ccz *t);

CC_NONNULL((1,2))
void ccz_lsr(ccz *r, const ccz *s, size_t k);

CC_NONNULL((1,2))
void ccz_lsl(ccz *r, const ccz *s, size_t k);

CC_NONNULL_ALL
void ccz_mod(ccz *r, const ccz *s, const ccz *t);

CC_NONNULL_ALL
void ccz_sqr(ccz *r, const ccz *s);

CC_PURE CC_NONNULL_ALL
bool ccz_is_zero(const ccz *s);

CC_PURE CC_NONNULL_ALL
bool ccz_is_one(const ccz *s);

CC_PURE CC_NONNULL_ALL
bool ccz_is_negative(const ccz *s);

CC_NONNULL1
void ccz_print(const ccz *s);

CC_NONNULL2
void ccz_lprint(const char *label, const ccz *s);

CC_NONNULL((1,3))
int ccz_random_bits(ccz *r, size_t nbits, struct ccrng_state *rng);

CC_INLINE CC_PURE CC_NONNULL_ALL
bool ccz_is_odd(const ccz *s) {
    return ccz_bit(s, 0);
}

CC_INLINE CC_NONNULL_ALL
void ccz_div2(ccz *r, const ccz *s) {
    ccz_lsr(r, s, 1);
}

#endif
