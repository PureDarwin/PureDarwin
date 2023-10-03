#include <corecrypto/ccz.h>
#include <stdio.h>

size_t ccz_size(struct ccz_class *cls)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_init(struct ccz_class *cls, ccz *r)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_free(ccz *r)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_zero(ccz *r)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_set(ccz *r, const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_seti(ccz *r, uint64_t v)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccz_bitlen(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccz_trailing_zeros(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccz_write_uint_size(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_gcd(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_lcm(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_mulmod(ccz *r, const ccz *s, const ccz *t, const ccz *u)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_sqrmod(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccz_invmod(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
	return -1;
}

void ccz_expmod(ccz *r, const ccz *s, const ccz *t, const ccz *u)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

bool ccz_bit(const ccz *s, size_t k)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_set_bit(ccz *r, size_t k, bool value)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

bool ccz_is_prime(const ccz *s, unsigned depth)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_write_uint(const ccz *s, size_t out_size, void *out)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccz_write_int_size(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_write_int(const ccz *s, size_t out_size, void *out)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

size_t ccz_write_radix_size(const ccz *s, unsigned radix)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_write_radix(const ccz *s, size_t out_size, void *out, unsigned radix)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_read_uint(ccz *r, size_t data_size, const uint8_t *data)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_read_int(ccz *r, size_t data_size, const uint8_t *data)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccz_read_radix(ccz *r, size_t data_size, const char *data, unsigned radix)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccz_cmp(const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccz_cmpi(const ccz *s, uint32_t v)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_neg(ccz *r)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_add(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_addi(ccz *r, const ccz *s, uint32_t v)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_sub(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_subi(ccz *r, const ccz *s, uint32_t v)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_mul(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_muli(ccz *r, const ccz *s, uint32_t v)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_divmod(ccz *q, ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_lsr(ccz *r, const ccz *s, size_t k)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_lsl(ccz *r, const ccz *s, size_t k)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_mod(ccz *r, const ccz *s, const ccz *t)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_sqr(ccz *r, const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

bool ccz_is_zero(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

bool ccz_is_one(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

bool ccz_is_negative(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_print(const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

void ccz_lprint(const char *label, const ccz *s)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

int ccz_random_bits(ccz *r, size_t nbits, struct ccrng_state *rng)
{
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
