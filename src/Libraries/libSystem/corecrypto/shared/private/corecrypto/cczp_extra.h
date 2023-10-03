#ifndef CC_PRIVATE_CCZP_EXTRA_H
#define CC_PRIVATE_CCZP_EXTRA_H

#include <corecrypto/cczp.h>

// operation:
//   (a * b) % mod -> r
void cczp_mul_mod(cczp_const_t zp, cc_unit* r, const cc_unit* a, const cc_unit* b);

// operation:
//   (s - t) % mod -> r
// notes:
//   automatically adjusted to account for underflow
void cczp_sub_mod(cc_size n, cc_unit* r, const cc_unit* s, const cc_unit* t, cczp_const_t mod);

// operation:
//   (s + t) % mod -> r
// notes:
//   automatically adjusted to account for overflow
void cczp_add_mod(cc_size n, cc_unit* r, const cc_unit* s, const cc_unit* t, cczp_const_t mod);

#endif // CC_PRIVATE_CCZP_EXTRA_H
