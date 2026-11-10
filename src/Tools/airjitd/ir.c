// ir.c - names air.* calls with the operations in ir.h
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "ir.h"

#include <string.h>

#define IR_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const struct {
  const char *name;
  unsigned arity;
} math[] = {
  [IR_NONE] = { "", 0 },
#define IR_MATH_ROW(id, name, arity) [IR_##id] = { name, arity },
  IR_MATH_OPS(IR_MATH_ROW)
#undef IR_MATH_ROW
};

static const char *const other[] = {
#define IR_OTHER_ROW(id, what) [IR_##id - IR_MATH_END - 1] = what,
  IR_OTHER_OPS(IR_OTHER_ROW)
#undef IR_OTHER_ROW
};

// other AIR spellings of a math op
static const struct {
  const char *name;
  enum ir_op op;
} aliases[] = {
  { "abs", IR_FABS },
  { "min", IR_FMIN },
  { "max", IR_FMAX },
  { "powr", IR_POW },
};

// families matched on the whole base, then on a base prefix
static const struct {
  const char *base;
  int prefix;
  enum ir_op op;
} families[] = {
  { "convert", 0, IR_CONVERT },
  { "pack", 0, IR_PACK },
  { "unpack", 0, IR_UNPACK },
  { "dot", 0, IR_DOT },
  { "any", 0, IR_ANY },
  { "all", 0, IR_ALL },
  { "mul_hi", 0, IR_INT_BITS },
  { "clz", 0, IR_INT_BITS },
  { "ctz", 0, IR_INT_BITS },
  { "popcount", 0, IR_INT_BITS },
  { "reverse_bits", 0, IR_INT_BITS },
  { "rotate", 0, IR_INT_BITS },
  { "abs_diff", 0, IR_INT_BITS },
  { "add_sat", 0, IR_INT_BITS },
  { "sub_sat", 0, IR_INT_BITS },
  { "extract_bits", 0, IR_INT_BITS },
  { "insert_bits", 0, IR_INT_BITS },
  { "fmin3", 0, IR_MINMAX3 },
  { "fmax3", 0, IR_MINMAX3 },
  { "fmed3", 0, IR_MINMAX3 },
  { "dfdx", 0, IR_DERIV },
  { "dfdy", 0, IR_DERIV },
  { "fwidth", 0, IR_DERIV },
  { "discard_fragment", 0, IR_DISCARD },
  { "sample_texture_", 1, IR_SAMPLE },
  { "read_texture_", 1, IR_TEX_READ },
  { "write_texture_", 1, IR_TEX_WRITE },
  { "get_read_sampler", 0, IR_READ_SAMPLER },
  { "mem_barrier", 0, IR_MEM_BARRIER },
  { "quad_shuffle", 1, IR_QUAD },
  { "quad_broadcast", 0, IR_QUAD },
  { "simd_", 1, IR_SIMD },
  { "atomic", 0, IR_ATOMIC },
  { "imageblock_data", 0, IR_IB_DATA },
  { "write_imageblock_slice_to_texture_2d", 1, IR_IB_WRITE },
};

const char *ir_op_name(enum ir_op op) {
  if (op > IR_NONE && op < IR_MATH_END)
    return math[op].name;
  if (op > IR_MATH_END && op < IR_END)
    return other[op - IR_MATH_END - 1];
  return "none";
}

unsigned ir_math_arity(enum ir_op op) {
  return ir_is_math(op) ? math[op].arity : 0;
}

static enum ir_op parse_op(const struct ir_call *c, unsigned nargs, int float_result) {
  const char *b = c->base;
  unsigned i;

  if (!strcmp(b, "wg") && !strncmp(c->rest, "barrier", 7))
    return IR_BARRIER_WG;
  if (!strcmp(b, "simdgroup") && !strncmp(c->rest, "barrier", 7))
    return IR_BARRIER_SIMD;
  if (!strcmp(b, "load") && !strncmp(c->rest, "implicit_imageblock", 19))
    return IR_IB_LOAD;
  if (!strcmp(b, "store") && !strncmp(c->rest, "implicit_imageblock", 19))
    return IR_IB_STORE;
  if (!strncmp(b, "get_", 4) && strstr(b, "_texture_"))
    return IR_TEX_QUERY;

  // integer variants carry the signedness right after the base: air.min.s.v2i16
  if (c->sign && (!strcmp(b, "min") || !strcmp(b, "max") || !strcmp(b, "clamp")))
    return IR_INT_MINMAX;
  if (c->sign && !strcmp(b, "abs"))
    return IR_INT_ABS;

  for (i = 0; i < IR_COUNT(families); i++) {
    if (families[i].prefix ? !strncmp(b, families[i].base, strlen(families[i].base)) : !strcmp(b, families[i].base))
      return families[i].op;
  }

  if (!float_result)
    return IR_NONE;
  for (i = 0; i < IR_COUNT(aliases); i++) {
    if (!strcmp(b, aliases[i].name) && math[aliases[i].op].arity == nargs)
      return aliases[i].op;
  }
  for (i = IR_NONE + 1; i < IR_MATH_END; i++) {
    if (math[i].arity == nargs && !strcmp(b, math[i].name))
      return (enum ir_op)i;
  }
  return IR_NONE;
}

int ir_parse(const char *name, unsigned nargs, int float_result, struct ir_call *c) {
  const char *dot;
  size_t n;

  memset(c, 0, sizeof(*c));
  if (strncmp(name, "air.", 4))
    return -1;
  c->name = name + 4;
  if (!strncmp(c->name, "fast_", 5))
    c->name += 5, c->fast = 1;

  dot = strchr(c->name, '.');
  n = dot ? (size_t)(dot - c->name) : strlen(c->name);
  if (n >= sizeof(c->base))
    return -1;
  memcpy(c->base, c->name, n);
  c->rest = dot ? dot + 1 : "";
  if ((c->rest[0] == 's' || c->rest[0] == 'u') && c->rest[1] == '.')
    c->sign = c->rest[0];

  c->op = parse_op(c, nargs, float_result);
  return c->op == IR_NONE ? -1 : 0;
}
