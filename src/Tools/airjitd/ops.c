#include "ops.h"

#include <math.h>
#include <string.h>

#include "airjitd.h"
#include "texture.h"

// builtins, so these do not need fminf and friends from libSystem's export list
static float air_fminf(float x, float y) {
  return __builtin_fminf(x, y);
}

static float air_fmaxf(float x, float y) {
  return __builtin_fmaxf(x, y);
}

static float air_rintf(float x) {
  return __builtin_rintf(x);
}

static float air_sqrtf(float x) {
  return __builtin_sqrtf(x);
}

static float air_sinpi(float x) {
  return sinf((float)M_PI * x);
}

static float air_cospi(float x) {
  return cosf((float)M_PI * x);
}

static float air_tanpi(float x) {
  return tanf((float)M_PI * x);
}

static float air_fract(float x) {
  return x - floorf(x);
}

static float air_rsqrt(float x) {
  return 1 / air_sqrtf(x);
}

// 1 for positive, -1 for negative, the input itself for zero
static float air_sign(float x) {
  return x > 0 ? 1 : x < 0 ? -1 : x;
}

static float air_saturate(float x) {
  return pd_clamp(x, 0, 1);
}

static float air_mix(float x, float y, float t) {
  return x + (y - x) * t;
}

#define OP1(name, native, fn) { name, 1, native, { .f1 = fn } }
#define OP2(name, native, fn) { name, 2, native, { .f2 = fn } }
#define OP3(name, native, fn) { name, 3, native, { .f3 = fn } }

static const struct air_math_op math_ops[] = {
  OP1("floor", AIR_FLOOR, floorf),
  OP1("ceil", AIR_CEIL, ceilf),
  OP1("trunc", AIR_TRUNC, truncf),
  OP1("rint", AIR_RINT, air_rintf),
  OP1("round", AIR_ROUND, roundf),
  OP1("sqrt", AIR_SQRT, air_sqrtf),
  OP1("fabs", AIR_FABS, fabsf),
  OP1("abs", AIR_FABS, fabsf),
  OP2("fmin", AIR_MINNUM, air_fminf),
  OP2("fmax", AIR_MAXNUM, air_fmaxf),
  OP2("min", AIR_MINNUM, air_fminf),
  OP2("max", AIR_MAXNUM, air_fmaxf),
  OP1("fract", AIR_FRACT, air_fract),
  OP1("rsqrt", AIR_RSQRT, air_rsqrt),
  OP3("fma", AIR_FMA, fmaf),
  OP1("sign", AIR_SIGN, air_sign),
  OP3("clamp", AIR_CLAMP, pd_clamp),
  OP1("saturate", AIR_SATURATE, air_saturate),
  OP3("mix", AIR_MIX, air_mix),
  OP1("sin", AIR_CALL, sinf),
  OP1("cos", AIR_CALL, cosf),
  OP1("tan", AIR_CALL, tanf),
  OP1("asin", AIR_CALL, asinf),
  OP1("acos", AIR_CALL, acosf),
  OP1("atan", AIR_CALL, atanf),
  OP1("sinh", AIR_CALL, sinhf),
  OP1("cosh", AIR_CALL, coshf),
  OP1("tanh", AIR_CALL, tanhf),
  OP1("exp", AIR_CALL, expf),
  OP1("exp2", AIR_CALL, exp2f),
  OP1("log", AIR_CALL, logf),
  OP1("log2", AIR_CALL, log2f),
  OP1("log10", AIR_CALL, log10f),
  OP2("atan2", AIR_CALL, atan2f),
  OP2("pow", AIR_CALL, powf),
  OP2("powr", AIR_CALL, powf),
  OP2("fmod", AIR_CALL, fmodf),
  OP1("sinpi", AIR_CALL, air_sinpi),
  OP1("cospi", AIR_CALL, air_cospi),
  OP1("tanpi", AIR_CALL, air_tanpi),
};

const struct air_math_op *air_math_find(const char *op, unsigned arity) {
  unsigned i;

  for (i = 0; i < PD_ARRAY_LEN(math_ops); i++) {
    if (math_ops[i].arity == arity && !strcmp(op, math_ops[i].op))
      return &math_ops[i];
  }

  return 0;
}
