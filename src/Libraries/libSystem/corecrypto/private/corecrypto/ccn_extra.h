#ifndef CC_PRIVATE_CCN_EXTRA_H
#define CC_PRIVATE_CCN_EXTRA_H

#include <corecrypto/ccn.h>

// operation:
//   operand / divisor -> quotient
// sizing:
//   unitlen quotient == n_operand
//   unitlen operand == n_operand
//   unitlen divisor == n_divisor
// notes:
//   this is a fast division algorithm based on Knuth's Algorithm D
void ccn_div_long(cc_size n_operand, cc_unit* quotient, const cc_unit* operand, cc_size n_divisor, const cc_unit* divisor);

// operation:
//   find `result` such that:
//     (result * a) % b == 1
// sizing:
//   unitlen a == n
//   unitlen b == n
//   unitlen result == n
int ccn_modular_multiplicative_inverse(cc_size n, cc_unit* result, const cc_unit* a, const cc_unit* b);

#endif // CC_PRIVATE_CCN_EXTRA_H
