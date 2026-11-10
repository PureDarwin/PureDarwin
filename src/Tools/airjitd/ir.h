// ir.h - the AIR operations every airjitd backend implements, with one meaning each
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef AIRJITD_IR_H
#define AIRJITD_IR_H

// shared AIR operation names for every backend

// scalar and vector floating-point operations
#define IR_MATH_OPS(X)                                                                                                 \
  X(FLOOR, "floor", 1)                                                                                                 \
  X(CEIL, "ceil", 1)                                                                                                   \
  X(TRUNC, "trunc", 1)                                                                                                 \
  X(RINT, "rint", 1)                                                                                                   \
  X(ROUND, "round", 1)                                                                                                 \
  X(SQRT, "sqrt", 1)                                                                                                   \
  X(FABS, "fabs", 1)                                                                                                   \
  X(FMIN, "fmin", 2)                                                                                                   \
  X(FMAX, "fmax", 2)                                                                                                   \
  X(FRACT, "fract", 1)                                                                                                 \
  X(RSQRT, "rsqrt", 1)                                                                                                 \
  X(FMA, "fma", 3)                                                                                                     \
  X(SIGN, "sign", 1)                                                                                                   \
  X(CLAMP, "clamp", 3)                                                                                                 \
  X(SATURATE, "saturate", 1)                                                                                           \
  X(MIX, "mix", 3)                                                                                                     \
  X(STEP, "step", 2)                                                                                                   \
  X(SMOOTHSTEP, "smoothstep", 3)                                                                                       \
  X(SIN, "sin", 1)                                                                                                     \
  X(COS, "cos", 1)                                                                                                     \
  X(TAN, "tan", 1)                                                                                                     \
  X(ASIN, "asin", 1)                                                                                                   \
  X(ACOS, "acos", 1)                                                                                                   \
  X(ATAN, "atan", 1)                                                                                                   \
  X(SINH, "sinh", 1)                                                                                                   \
  X(COSH, "cosh", 1)                                                                                                   \
  X(TANH, "tanh", 1)                                                                                                   \
  X(EXP, "exp", 1)                                                                                                     \
  X(EXP2, "exp2", 1)                                                                                                   \
  X(EXP10, "exp10", 1)                                                                                                 \
  X(LOG, "log", 1)                                                                                                     \
  X(LOG2, "log2", 1)                                                                                                   \
  X(LOG10, "log10", 1)                                                                                                 \
  X(ATAN2, "atan2", 2)                                                                                                 \
  X(POW, "pow", 2)                                                                                                     \
  X(FMOD, "fmod", 2)                                                                                                   \
  X(SINPI, "sinpi", 1)                                                                                                 \
  X(COSPI, "cospi", 1)                                                                                                 \
  X(TANPI, "tanpi", 1)

// other AIR operation families
#define IR_OTHER_OPS(X)                                                                                                \
  X(CONVERT, "air.convert.<to>.<from>: numeric conversion, float to unsigned saturates")                             \
  X(PACK, "air.pack.<format>")                                                                                         \
  X(UNPACK, "air.unpack.<format>")                                                                                     \
  X(DOT, "air.dot: float dot product")                                                                                 \
  X(ANY, "air.any: any lane true")                                                                                     \
  X(ALL, "air.all: all lanes true")                                                                                    \
  X(INT_MINMAX, "air.min|max|clamp.s|u: integer min, max and clamp")                                                   \
  X(INT_ABS, "air.abs.s|u: integer absolute value")                                                                    \
  X(INT_BITS, "air.mul_hi, clz, ctz, popcount, reverse_bits, rotate, abs_diff, add_sat, sub_sat, extract_bits, "      \
              "insert_bits")                                                                                           \
  X(MINMAX3, "air.fmin3, fmax3, fmed3")                                                                                \
  X(DERIV, "air.dfdx, dfdy, fwidth: screen derivatives in a fragment function")                                       \
  X(DISCARD, "air.discard_fragment")                                                                                   \
  X(SAMPLE, "air.sample_texture_*")                                                                                    \
  X(TEX_READ, "air.read_texture_*")                                                                                    \
  X(TEX_WRITE, "air.write_texture_*")                                                                                  \
  X(TEX_QUERY, "air.get_<property>_texture_*")                                                                         \
  X(READ_SAMPLER, "air.get_read_sampler: MSL 4's texel-read sampler, used only by the reads")                         \
  X(BARRIER_WG, "air.wg.barrier: threadgroup barrier")                                                                 \
  X(BARRIER_SIMD, "air.simdgroup.barrier")                                                                             \
  X(MEM_BARRIER, "air.mem_barrier: memory fence without execution sync")                                              \
  X(QUAD, "air.quad_shuffle*, quad_broadcast")                                                                         \
  X(SIMD, "air.simd_*: simdgroup lane operations")                                                                     \
  X(ATOMIC, "air.atomic.*")                                                                                            \
  X(IB_DATA, "air.imageblock_data")                                                                                    \
  X(IB_WRITE, "air.write_imageblock_slice_to_texture_2d")                                                              \
  X(IB_LOAD, "air.load.implicit_imageblock")                                                                           \
  X(IB_STORE, "air.store.implicit_imageblock")

#define IR_ENUM(id, ...) IR_##id,
enum ir_op { IR_NONE, IR_MATH_OPS(IR_ENUM) IR_MATH_END, IR_OTHER_OPS(IR_ENUM) IR_END };
#undef IR_ENUM

// one air.* call: the operation, and the name pieces backends read for types and variants
struct ir_call {
  enum ir_op op;
  // the name after "air." and "fast_", and the part before its first dot
  const char *name;
  char base[48];
  // what follows the first dot, "" when none
  const char *rest;
  int fast;
  // integer variants: 's' or 'u' right after the base (air.min.s.v2i16), else 0
  char sign;
};

static inline int ir_is_math(enum ir_op op) {
  return op > IR_NONE && op < IR_MATH_END;
}

// names the air.* function: 0 and c filled, or -1 when it is no AIR operation this table knows.
// nargs and float_result pick between same-named variants (float or integer min, math arity)
int ir_parse(const char *name, unsigned nargs, int float_result, struct ir_call *c);

// the canonical AIR name of a math op ("floor"), or the family's description
const char *ir_op_name(enum ir_op op);
unsigned ir_math_arity(enum ir_op op);

#endif
