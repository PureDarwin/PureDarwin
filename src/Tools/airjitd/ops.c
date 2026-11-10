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

// Metal's smoothstep(edge0, edge1, x): the Hermite curve over x clamped into the edges
static float air_smoothstep(float e0, float e1, float x) {
  float t = pd_clamp((x - e0) / (e1 - e0), 0, 1);

  return t * t * (3 - 2 * t);
}

// step(edge, x): 0 below the edge, else 1
static float air_step(float edge, float x) {
  return x < edge ? 0 : 1;
}

static float air_exp10(float x) {
  return exp2f(x * 3.32192809489f);
}

#define OP1(id, native, fn) [IR_##id] = { IR_##id, 1, native, { .f1 = fn } }
#define OP2(id, native, fn) [IR_##id] = { IR_##id, 2, native, { .f2 = fn } }
#define OP3(id, native, fn) [IR_##id] = { IR_##id, 3, native, { .f3 = fn } }

// the cpu's implementation of each math op in ir.h
static const struct air_math_op math_ops[IR_MATH_END] = {
  OP1(FLOOR, AIR_FLOOR, floorf),
  OP1(CEIL, AIR_CEIL, ceilf),
  OP1(TRUNC, AIR_TRUNC, truncf),
  OP1(RINT, AIR_RINT, air_rintf),
  OP1(ROUND, AIR_ROUND, roundf),
  OP1(SQRT, AIR_SQRT, air_sqrtf),
  OP1(FABS, AIR_FABS, fabsf),
  OP2(FMIN, AIR_MINNUM, air_fminf),
  OP2(FMAX, AIR_MAXNUM, air_fmaxf),
  OP1(FRACT, AIR_FRACT, air_fract),
  OP1(RSQRT, AIR_RSQRT, air_rsqrt),
  OP3(FMA, AIR_FMA, fmaf),
  OP1(SIGN, AIR_SIGN, air_sign),
  OP3(CLAMP, AIR_CLAMP, pd_clamp),
  OP1(SATURATE, AIR_SATURATE, air_saturate),
  OP3(MIX, AIR_MIX, air_mix),
  OP2(STEP, AIR_CALL, air_step),
  OP3(SMOOTHSTEP, AIR_CALL, air_smoothstep),
  OP1(SIN, AIR_CALL, sinf),
  OP1(COS, AIR_CALL, cosf),
  OP1(TAN, AIR_CALL, tanf),
  OP1(ASIN, AIR_CALL, asinf),
  OP1(ACOS, AIR_CALL, acosf),
  OP1(ATAN, AIR_CALL, atanf),
  OP1(SINH, AIR_CALL, sinhf),
  OP1(COSH, AIR_CALL, coshf),
  OP1(TANH, AIR_CALL, tanhf),
  OP1(EXP, AIR_CALL, expf),
  OP1(EXP2, AIR_CALL, exp2f),
  OP1(EXP10, AIR_CALL, air_exp10),
  OP1(LOG, AIR_CALL, logf),
  OP1(LOG2, AIR_CALL, log2f),
  OP1(LOG10, AIR_CALL, log10f),
  OP2(ATAN2, AIR_CALL, atan2f),
  OP2(POW, AIR_CALL, powf),
  OP2(FMOD, AIR_CALL, fmodf),
  OP1(SINPI, AIR_CALL, air_sinpi),
  OP1(COSPI, AIR_CALL, air_cospi),
  OP1(TANPI, AIR_CALL, air_tanpi),
};

const struct air_math_op *air_math_at(unsigned i) {
  return i + 1 < IR_MATH_END ? air_math_of((enum ir_op)(i + 1)) : 0;
}

const struct air_math_op *air_math_of(enum ir_op op) {
  return ir_is_math(op) && math_ops[op].arity ? &math_ops[op] : 0;
}
