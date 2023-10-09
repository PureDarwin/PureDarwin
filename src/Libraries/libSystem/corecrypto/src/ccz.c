#include <corecrypto/ccz.h>
#include <corecrypto/ccstubs.h>

size_t ccz_size(struct ccz_class *cls)
{
	CC_STUB(-1);
}

void ccz_init(struct ccz_class *cls, ccz *r)
{
	CC_STUB_VOID();
}

void ccz_free(ccz *r)
{
	CC_STUB_VOID();
}

void ccz_zero(ccz *r)
{
	CC_STUB_VOID();
}

void ccz_set(ccz *r, const ccz *s)
{
	CC_STUB_VOID();
}

void ccz_seti(ccz *r, uint64_t v)
{
	CC_STUB_VOID();
}

size_t ccz_bitlen(const ccz *s)
{
	CC_STUB(-1);
}

size_t ccz_trailing_zeros(const ccz *s)
{
	CC_STUB(-1);
}

size_t ccz_write_uint_size(const ccz *s)
{
	CC_STUB(-1);
}

void ccz_gcd(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_lcm(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_mulmod(ccz *r, const ccz *s, const ccz *t, const ccz *u)
{
	CC_STUB_VOID();
}

void ccz_sqrmod(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

int ccz_invmod(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB(-1);
}

void ccz_expmod(ccz *r, const ccz *s, const ccz *t, const ccz *u)
{
	CC_STUB_VOID();
}

bool ccz_bit(const ccz *s, size_t k)
{
	CC_STUB(false);
}

void ccz_set_bit(ccz *r, size_t k, bool value)
{
	CC_STUB_VOID();
}

bool ccz_is_prime(const ccz *s, unsigned depth)
{
	CC_STUB(false);
}

void ccz_write_uint(const ccz *s, size_t out_size, void *out)
{
	CC_STUB_VOID();
}

size_t ccz_write_int_size(const ccz *s)
{
	CC_STUB(-1);
}

void ccz_write_int(const ccz *s, size_t out_size, void *out)
{
	CC_STUB_VOID();
}

size_t ccz_write_radix_size(const ccz *s, unsigned radix)
{
	CC_STUB(-1);
}

void ccz_write_radix(const ccz *s, size_t out_size, void *out, unsigned radix)
{
	CC_STUB_VOID();
}

void ccz_read_uint(ccz *r, size_t data_size, const uint8_t *data)
{
	CC_STUB_VOID();
}

void ccz_read_int(ccz *r, size_t data_size, const uint8_t *data)
{
	CC_STUB_VOID();
}

int ccz_read_radix(ccz *r, size_t data_size, const char *data, unsigned radix)
{
	CC_STUB(-1);
}

int ccz_cmp(const ccz *s, const ccz *t)
{
	CC_STUB(-1);
}

int ccz_cmpi(const ccz *s, uint32_t v)
{
	CC_STUB(-1);
}

void ccz_neg(ccz *r)
{
	CC_STUB_VOID();
}

void ccz_add(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_addi(ccz *r, const ccz *s, uint32_t v)
{
	CC_STUB_VOID();
}

void ccz_sub(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_subi(ccz *r, const ccz *s, uint32_t v)
{
	CC_STUB_VOID();
}

void ccz_mul(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_muli(ccz *r, const ccz *s, uint32_t v)
{
	CC_STUB_VOID();
}

void ccz_divmod(ccz *q, ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_lsr(ccz *r, const ccz *s, size_t k)
{
	CC_STUB_VOID();
}

void ccz_lsl(ccz *r, const ccz *s, size_t k)
{
	CC_STUB_VOID();
}

void ccz_mod(ccz *r, const ccz *s, const ccz *t)
{
	CC_STUB_VOID();
}

void ccz_sqr(ccz *r, const ccz *s)
{
	CC_STUB_VOID();
}

bool ccz_is_zero(const ccz *s)
{
	CC_STUB(false);
}

bool ccz_is_one(const ccz *s)
{
	CC_STUB(false);
}

bool ccz_is_negative(const ccz *s)
{
	CC_STUB(false);
}

void ccz_print(const ccz *s)
{
	CC_STUB_VOID();
}

void ccz_lprint(const char *label, const ccz *s)
{
	CC_STUB_VOID();
}

int ccz_random_bits(ccz *r, size_t nbits, struct ccrng_state *rng)
{
	CC_STUB(-1);
}
