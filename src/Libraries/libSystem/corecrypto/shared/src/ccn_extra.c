#include <corecrypto/private/ccn_extra.h>
#include <stdbool.h>
#include <corecrypto/cc_debug.h>

// change to 1 to debug via printing (not the best debugger method)
// useful in case you don't have access to a debugger and something's
// up
// warning: generates a *ton* of input because these methods are used often
#ifndef CCN_EXTRA_DEBUG
	#define CCN_EXTRA_DEBUG 0
#endif

#if CCN_EXTRA_DEBUG
	#include <stdio.h>
#endif

void ccn_div_long(cc_size n_operand, cc_unit* quotient, const cc_unit* operand, cc_size n_divisor, const cc_unit* divisor) {
	cc_size n = ccn_n(n_divisor, divisor);
	cc_size total_operand_n = ccn_n(n_operand, operand);

	ccn_zero(n_operand, quotient);

	if (total_operand_n < n) {
		return;
	}

	if (n <= 1) {
		cc_unit r = 0;

		for (cc_size i = total_operand_n; i > 0; --i) {
			cc_size j = i - 1;

			cc_dunit two_digits = ((cc_dunit)r << CCN_UNIT_BITS) | (cc_dunit)operand[j];
			cc_dunit q_hat = two_digits / divisor[0];
			cc_dunit r_hat = two_digits % divisor[0];

			quotient[j] = (cc_unit)(q_hat & CCN_UNIT_MASK);
			r = (cc_unit)(r_hat & CCN_UNIT_MASK);
		}

		return;
	}

	cc_size m = total_operand_n - n;

	// we're using Knuth's Algorithm D: division of nonnegative integers
	// (see "The Art of Computer Programming (vol. 2)", section 4.3.1)
	// we're operating in base 2^64

	// our dividend
	cc_unit* operand_copy = __builtin_alloca(ccn_sizeof_n(total_operand_n + 1));
	memset(operand_copy, 0, ccn_sizeof_n(total_operand_n + 1));
	memcpy(operand_copy, operand, ccn_sizeof_n(total_operand_n));

	// our divisor
	cc_unit* divisor_copy = __builtin_alloca(ccn_sizeof_n(n + 1));
	divisor_copy[n] = 0;
	memcpy(divisor_copy, divisor, ccn_sizeof_n(n));

	// 1. normalize
	// make sure that the most significant digit of the divisor
	// is greater than half of the base (greater than `2^63` in this case)
	//
	// for us, that means "eliminate leading zeros", which we do by
	// counting them and shifting all the bits left by that amount
	cc_size shift = __builtin_clzl(divisor_copy[n - 1]);
	// the `ccn_shift_*` functions currently have a bug where they will not
	// work correctly when given a 0 as the shift count
	// TODO: fix this
	if (shift > 0) {
		ccn_shift_left_multi(n + 1, divisor_copy, divisor_copy, shift);
		ccn_shift_left_multi(total_operand_n + 1, operand_copy, operand_copy, shift);
	}
	
	// note that after normalization, the divisor still has the same number of digits as before
	// but the dividend has a new most significant digit
	//
	// thus, the most significant digit of the divisor is at `n - 1`, but the most significant digit
	// of the dividend is at `(2 * n) + 1`
	//
	// however, for the purposes of the loop counter computation later on,
	// we use the original number of digits for both

	// the base we're using
	// since cc_units have architecture dependent sizes, we use the defined bit mask (which
	// is equal to the base minus 1), and we add 1
	cc_dunit base = CCN_UNIT_MASK;
	base += 1;

	// 2. initialize `j`
	// set `j` equal to the difference in the number of digits between the divisor
	// and the dividend
	//
	// ...plus an extra 1 because we're using an unsigned counter
	//
	// this step also includes:
	// 7. loop on `j`
	for (cc_size i = m + 1; i > 0; --i) {
		cc_size j = i - 1;
		cc_unit* operand_pointer = operand_copy + j;

		// 3. calculate q_hat
		// set q_hat equal to the two most significant digits of the dividend
		// divided by the most significant digit of the divisor
		//
		// also set r_hat equal to the remainder of this operation
		cc_dunit two_digits = ((cc_dunit)operand_pointer[n] << CCN_UNIT_BITS) | (cc_dunit)operand_pointer[n - 1];
		cc_dunit q_hat = two_digits / divisor_copy[n - 1];
		cc_dunit r_hat = two_digits % divisor_copy[n - 1];

		// (still part of step 3)
		while (true) {
			// if q_hat is greater than or equal to the base
			// OR
			// if q_hat times the second most significant digit of the divisor is
			// greater than r_hat times the base plus the second third most significant digit of the dividend
			// then...
			if (q_hat >= base || (q_hat * divisor_copy[n - 2]) > ((r_hat << CCN_UNIT_BITS) | operand_pointer[n - 2])) {
				// ...decrease q_hat by one
				--q_hat;
				// and add the most significant digit of the divisor to r_hat
				r_hat += divisor_copy[n - 1];
				// continue performing this test while r_hat is less than the base
				if (r_hat < base)
					continue;
			}
			break;
		}

		// btw, we use variable length arrays here because unlike
		// alloca-allocated memory, they're freed at the end of their
		// scope, and we might loop *a lot* (since we're dependent on
		// the unit count, which we don't control)

		// we only need `n + 1` units because at this point,
		// q_hat is a single unit number, not double unit (and we're only
		// using `n` units of `divisor_copy`)
		//
		// however, we need to allocate all the units necessary
		// because `ccn_mul` does a zero for the full 2n
		cc_unit result[2 * n];
		memset(result, 0, ccn_sizeof_n(2 * n));

		cc_unit q_hat_ccn[n];
		memset(q_hat_ccn, 0, ccn_sizeof_n(n));
		q_hat_ccn[0] = (cc_unit)(q_hat & CCN_UNIT_MASK);

		// 4. multiply and subtract
		// subtract `q_hat * divisor` from the t

		// `q_hat * divisor`
		ccn_mul(n, result, q_hat_ccn, divisor_copy);

		// note that in the following syntax, the end is exclusive
		// `dividend[j:j + n + 1] -= q_hat * divisor`
		// (a.k.a. subtract `q_hat * divisor` from the digits of the dividend
		// in the range from `j` up to but not including `j + n + 1`)
		//
		// we've already set up operand_pointer to point to `pow + j`
		bool underflow = ccn_sub(n + 1, operand_pointer, operand_pointer, result);

		// 5. test remainder

		// set the j-th digit of the quotient equal to q_hat
		quotient[j] = q_hat;

		// if the result of step 4 was negative
		// then...
		if (underflow) {
			// ...decrease the j-th digit of the quotient by 1
			--quotient[j];

			// and add one copy of the divisor into the range of digits
			// of the dividend that we subtracted from
			cc_unit tmp[n + 1];
			tmp[n] = 0;
			ccn_set(n, tmp, divisor_copy);
			ccn_add(n + 1, operand_pointer, operand_pointer, tmp);
		}
	}

	// step 8 *would* be "unnormalize", but we don't need to do that because we
	// don't need the remainder for our purposes here
};

int ccn_modular_multiplicative_inverse(cc_size n, cc_unit* result, const cc_unit* a, const cc_unit* b) {
	/*
		function mul_inv(a, b) {
			let t = 0
			let new_t = 1
			let r = b
			let new_r = a

			while (new_r != 0) {
				let q = floor(r / new_r)

				let tmp = new_t
				new_t = t - q * new_t
				t = tmp

				tmp = new_r
				new_r = r - q * new_r
				r = tmp
			}

			if (r > 1)
				throw "a is not invertible"
			if (t < 0)
				t += b

			return t
		}
	*/

	// let t = 0
	cc_unit t[n];
	memset(t, 0, sizeof t);

	// let new_t = 0
	cc_unit new_t[n];
	memset(new_t, 0, sizeof new_t);
	new_t[0] = 1;

	// let r = b
	cc_unit r[n];
	memcpy(r, b, sizeof r);

	// let new_r = a
	cc_unit new_r[n];
	memcpy(new_r, a, sizeof r);

	// while (new_r != 0)
	while (ccn_n(n, new_r) > 1 || new_r[0] != 0) {
		cc_unit double_tmp[2 * n];

		// let q = floor(r / new_r)
		cc_unit q[n];
		memset(q, 0, sizeof q);
		ccn_div_long(n, q, r, n, new_r);

		// let tmp = new_t
		cc_unit tmp[n];
		memcpy(tmp, new_t, sizeof tmp);

		// new_t = t - q * new_t
		ccn_mul(n, double_tmp, q, new_t);
		memset(new_t, 0, sizeof new_t);
		ccn_sub(n, new_t, t, double_tmp);

		// t = tmp
		memcpy(t, tmp, sizeof t);

		// tmp = new_r
		memcpy(tmp, new_r, sizeof tmp);

		// new_r = r - q * new_r
		ccn_mul(n, double_tmp, q, new_r);
		memset(new_r, 0, sizeof new_r);
		ccn_sub(n, new_r, r, double_tmp);

		// r = tmp
		memcpy(r, tmp, sizeof r);
	}

	// if (r > 1)
	if (ccn_n(n, r) > 1 || r[0] > 1)
		return -1;

	const cc_size most_significant_bit = n * CCN_UNIT_BITS - 1;

	// if (t < 0)
	if (ccn_bit(t, most_significant_bit))
		// t += b
		ccn_add(n, t, t, b);

	// return t
	memcpy(result, t, ccn_sizeof_n(n));

	return 0;
};
