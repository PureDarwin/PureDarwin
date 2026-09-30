#ifndef AIRJITD_OPS_H
#define AIRJITD_OPS_H

// the elementwise AIR math operations, air.[fast_]<op>.<type>, each with the C function
// that implements it on one float. A backend may lower the native hint to its own code instead
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
  const char *op;
  unsigned arity;
  enum air_native native;
  union {
    air_fn1 f1;
    air_fn2 f2;
    air_fn3 f3;
  } impl;
};

// op is the name between "air.[fast_]" and the type suffix
const struct air_math_op *air_math_find(const char *op, unsigned arity);

#endif
