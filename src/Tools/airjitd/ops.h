#ifndef AIRJITD_OPS_H
#define AIRJITD_OPS_H

#include "ir.h"

// the cpu's implementation of ir.h's math ops: the C function on one float, or the native hint
// the LLVM lowering turns into its own code
enum air_native {
  AIR_CALL,
  AIR_FLOOR,
  AIR_CEIL,
  AIR_TRUNC,
  AIR_RINT,
  AIR_ROUND,
  AIR_SQRT,
  AIR_FABS,
  AIR_MINNUM,
  AIR_MAXNUM,
  AIR_FMA,
  AIR_FRACT,
  AIR_RSQRT,
  AIR_SIGN,
  AIR_CLAMP,
  AIR_SATURATE,
  AIR_MIX,
};

typedef float (*air_fn1)(float);
typedef float (*air_fn2)(float, float);
typedef float (*air_fn3)(float, float, float);

struct air_math_op {
  enum ir_op id;
  unsigned arity;
  enum air_native native;
  union {
    air_fn1 f1;
    air_fn2 f2;
    air_fn3 f3;
  } impl;
};

// the op's implementation, 0 when the cpu has none
const struct air_math_op *air_math_of(enum ir_op op);

// entry i in ir.h's order, 0 past the end or where the cpu has none
const struct air_math_op *air_math_at(unsigned i);

#endif
