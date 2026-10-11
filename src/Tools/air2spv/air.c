#include <spirv/unified1/GLSL.std.450.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a2s.h"

// one call being lowered: its operands, result type and the name after "air." and "fast_"
struct call {
  LLVMValueRef in;
  LLVMTypeRef rt;
  const char *op;
  unsigned n;
};

static LLVMValueRef arg_v(struct call *c, unsigned i) {
  return LLVMGetOperand(c->in, i);
}

static uint32_t arg(struct a2s *a, struct call *c, unsigned i) {
  return a2s_value(a, LLVMGetOperand(c->in, i));
}

static LLVMTypeRef arg_t(struct call *c, unsigned i) {
  return LLVMTypeOf(LLVMGetOperand(c->in, i));
}

static int is_half(LLVMTypeRef t) {
  return LLVMGetTypeKind(a2s_scalar(t)) == LLVMHalfTypeKind;
}

static void done(struct a2s *a, struct call *c, uint32_t v) {
  if (is_half(c->rt))
    v = a2s_quantize(a, c->rt, v);
  a2s_set(a, c->in, v);
}

// GLSL.std.450 elementwise math: name, arity, op, negative ops are lowered here
enum { M_SATURATE = -1, M_LOG10 = -2, M_FMOD = -3, M_SINPI = -4, M_COSPI = -5, M_TANPI = -6, M_EXP10 = -7 };

static const struct {
  const char *name;
  unsigned arity;
  int op;
} math[] = {
  { "floor", 1, GLSLstd450Floor },   { "ceil", 1, GLSLstd450Ceil },       { "trunc", 1, GLSLstd450Trunc },
  { "rint", 1, GLSLstd450RoundEven }, { "round", 1, GLSLstd450Round },    { "sqrt", 1, GLSLstd450Sqrt },
  { "fabs", 1, GLSLstd450FAbs },     { "abs", 1, GLSLstd450FAbs },        { "fmin", 2, GLSLstd450FMin },
  { "fmax", 2, GLSLstd450FMax },     { "min", 2, GLSLstd450FMin },        { "max", 2, GLSLstd450FMax },
  { "fract", 1, GLSLstd450Fract },   { "rsqrt", 1, GLSLstd450InverseSqrt }, { "fma", 3, GLSLstd450Fma },
  { "sign", 1, GLSLstd450FSign },    { "clamp", 3, GLSLstd450FClamp },    { "saturate", 1, M_SATURATE },
  { "mix", 3, GLSLstd450FMix },      { "sin", 1, GLSLstd450Sin },         { "cos", 1, GLSLstd450Cos },
  { "tan", 1, GLSLstd450Tan },       { "asin", 1, GLSLstd450Asin },       { "acos", 1, GLSLstd450Acos },
  { "atan", 1, GLSLstd450Atan },     { "sinh", 1, GLSLstd450Sinh },       { "cosh", 1, GLSLstd450Cosh },
  { "tanh", 1, GLSLstd450Tanh },     { "exp", 1, GLSLstd450Exp },         { "exp2", 1, GLSLstd450Exp2 },
  { "log", 1, GLSLstd450Log },       { "log2", 1, GLSLstd450Log2 },       { "log10", 1, M_LOG10 },
  { "atan2", 2, GLSLstd450Atan2 },   { "pow", 2, GLSLstd450Pow },         { "powr", 2, GLSLstd450Pow },
  { "fmod", 2, M_FMOD },             { "sinpi", 1, M_SINPI },             { "cospi", 1, M_COSPI },
  { "tanpi", 1, M_TANPI },           { "exp10", 1, M_EXP10 },             { "step", 2, GLSLstd450Step },
  { "smoothstep", 3, GLSLstd450SmoothStep },
};

static int air_math(struct a2s *a, struct call *c, const char *name) {
  uint32_t ty = a2s_type(a, c->rt), args[3], r;
  LLVMTypeKind ek = LLVMGetTypeKind(a2s_scalar(c->rt));
  unsigned i, k;

  if (ek != LLVMHalfTypeKind && ek != LLVMFloatTypeKind)
    return 0;

  for (i = 0; i < A2S_ARRAY_LEN(math); i++) {
    if (math[i].arity == c->n && !strcmp(math[i].name, name))
      break;
  }

  if (i == A2S_ARRAY_LEN(math))
    return 0;

  for (k = 0; k < c->n; k++) {
    args[k] = arg(a, c, k);
  }

  switch (math[i].op) {
  case M_SATURATE:
    args[1] = a2s_fconst(a, c->rt, 0), args[2] = a2s_fconst(a, c->rt, 1);
    r = a2s_ext(a, GLSLstd450FClamp, ty, args, 3);
    break;
  case M_LOG10:
    r = a2s_op2(a, SpvOpFMul, ty, a2s_ext(a, GLSLstd450Log2, ty, args, 1), a2s_fconst(a, c->rt, 0.30102999566f));
    break;
  case M_EXP10:
    args[0] = a2s_op2(a, SpvOpFMul, ty, args[0], a2s_fconst(a, c->rt, 3.32192809489f));
    r = a2s_ext(a, GLSLstd450Exp2, ty, args, 1);
    break;
  case M_FMOD:
    r = a2s_op2(a, SpvOpFRem, ty, args[0], args[1]);
    break;
  case M_SINPI:
  case M_COSPI:
  case M_TANPI:
    args[0] = a2s_op2(a, SpvOpFMul, ty, args[0], a2s_fconst(a, c->rt, 3.14159265359f));
    r = a2s_ext(a, math[i].op == M_SINPI ? GLSLstd450Sin : math[i].op == M_COSPI ? GLSLstd450Cos : GLSLstd450Tan, ty,
                args, 1);
    break;
  default:
    r = a2s_ext(a, (unsigned)math[i].op, ty, args, c->n);
  }

  done(a, c, r);
  return 1;
}

// air.convert.<to kind>.<to type>.<from kind>.<from type>
static int air_convert(struct a2s *a, struct call *c) {
  const char *t = c->op + 8, *f = strchr(t, '.');
  LLVMTypeRef st = arg_t(c, 0), dt = c->rt;
  uint32_t v = arg(a, c, 0), ty = a2s_type(a, dt), r;
  char to = t[0], from;
  int sb = a2s_bits(st), db = a2s_bits(dt);
  int sbool = sb == 1, dbool = db == 1;

  if (f)
    f = strchr(f + 1, '.');
  if (!f)
    return 0;

  from = f[1];

  if (to == 'f' && from == 'f') {
    r = db == 16 && sb > 16 ? a2s_quantize(a, dt, v) : v;
  } else if (to == 'f') {
    if (sbool)
      r = a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ v, a2s_fconst(a, dt, 1), a2s_fconst(a, dt, 0) }, 3);
    else if (from == 's')
      r = a2s_op1(a, SpvOpConvertSToF, ty, a2s_sext(a, st, v));
    else
      r = a2s_op1(a, SpvOpConvertUToF, ty, v);
    if (db == 16)
      r = a2s_quantize(a, dt, r);
  } else if (from == 'f') {
    if (dbool) {
      uint32_t bt = a2s_type(a, dt);
      r = a2s_op2(a, SpvOpFUnordNotEqual, bt, v, a2s_fconst(a, st, 0));
    } else if (to == 's') {
      uint32_t it = spv_t_vec(&a->s, spv_t_int(&a->s), a2s_lanes(dt));
      r = a2s_mask(a, dt, a2s_op1(a, SpvOpBitcast, ty, a2s_op1(a, SpvOpConvertFToS, it, v)));
    } else {
      // metal saturates float to unsigned: negative and NaN become 0
      uint32_t z = a2s_fconst(a, st, 0), args[3] = { v, z, v };
      args[0] = a2s_ext(a, GLSLstd450NMax, a2s_type(a, st), (uint32_t[]){ v, z }, 2);
      r = a2s_mask(a, dt, a2s_op1(a, SpvOpConvertFToU, ty, args[0]));
    }
  } else if (sbool) {
    r = a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ v, a2s_uconst(a, dt, 1), a2s_uconst(a, dt, 0) }, 3);
  } else if (dbool) {
    r = a2s_bool_of(a, st, v);
  } else if (db > sb) {
    r = from == 's' ? a2s_mask(a, dt, a2s_sext(a, st, v)) : v;
  } else {
    r = a2s_mask(a, dt, v);
  }

  a2s_set(a, c->in, r);
  return 1;
}

// air.unpack.<unorm|snorm><N>x<B>.<type>
static int air_unpack(struct a2s *a, struct call *c) {
  static const struct {
    const char *name;
    int op;
    unsigned lanes;
  } u[] = {
    { "unorm4x8", GLSLstd450UnpackUnorm4x8, 4 },
    { "snorm4x8", GLSLstd450UnpackSnorm4x8, 4 },
    { "unorm2x16", GLSLstd450UnpackUnorm2x16, 2 },
    { "snorm2x16", GLSLstd450UnpackSnorm2x16, 2 },
  };
  const char *q = c->op + 7;
  uint32_t v = arg(a, c, 0), r;

  for (unsigned i = 0; i < A2S_ARRAY_LEN(u); i++) {
    if (strncmp(q, u[i].name, strlen(u[i].name)))
      continue;

    r = a2s_ext(a, (unsigned)u[i].op, spv_t_vec(&a->s, spv_t_float(&a->s), u[i].lanes), &v, 1);
    done(a, c, r);
    return 1;
  }

  return 0;
}

static int air_dot(struct a2s *a, struct call *c) {
  uint32_t r = a2s_op2(a, SpvOpDot, a2s_type(a, c->rt), arg(a, c, 0), arg(a, c, 1));

  done(a, c, r);
  return 1;
}

static int air_anyall(struct a2s *a, struct call *c) {
  uint32_t v = arg(a, c, 0), r;
  int all = c->op[1] == 'l';

  r = a2s_lanes(arg_t(c, 0)) == 1 ? v : a2s_op1(a, all ? SpvOpAll : SpvOpAny, spv_t_bool(&a->s), v);
  if (a2s_bits(c->rt) > 1)
    r = a2s_op(a, SpvOpSelect, a2s_type(a, c->rt), (uint32_t[]){ r, a2s_uconst(a, c->rt, 1), a2s_uconst(a, c->rt, 0) }, 3);
  a2s_set(a, c->in, r);
  return 1;
}

// min/max/clamp/abs on integers: .s. or .u. after the op name
static int air_int(struct a2s *a, struct call *c, const char *name, char sgn) {
  uint32_t ty = a2s_type(a, c->rt), args[3], r;
  int s = sgn == 's';
  unsigned k;

  for (k = 0; k < c->n; k++) {
    args[k] = s ? a2s_sext(a, c->rt, arg(a, c, k)) : arg(a, c, k);
  }

  if (!strcmp(name, "min"))
    r = a2s_ext(a, s ? GLSLstd450SMin : GLSLstd450UMin, ty, args, 2);
  else if (!strcmp(name, "max"))
    r = a2s_ext(a, s ? GLSLstd450SMax : GLSLstd450UMax, ty, args, 2);
  else if (!strcmp(name, "clamp"))
    r = a2s_ext(a, s ? GLSLstd450SClamp : GLSLstd450UClamp, ty, args, 3);
  else if (!strcmp(name, "abs"))
    r = s ? a2s_ext(a, GLSLstd450SAbs, ty, args, 1) : args[0];
  else
    return 0;

  a2s_set(a, c->in, s ? a2s_mask(a, c->rt, r) : r);
  return 1;
}

static int air_deriv(struct a2s *a, struct call *c, const char *name) {
  unsigned op = name[0] == 'f' ? SpvOpFwidth : name[3] == 'x' ? SpvOpDPdx : SpvOpDPdy;

  if (a->stage != A2S_FRAGMENT)
    a2s_fail(a, "derivative outside a fragment function");

  done(a, c, a2s_op1(a, op, a2s_type(a, c->rt), arg(a, c, 0)));
  return 1;
}

static int air_minmax3(struct a2s *a, struct call *c, const char *name) {
  uint32_t ty = a2s_type(a, c->rt), x = arg(a, c, 0), y = arg(a, c, 1), z = arg(a, c, 2), r;
  uint32_t xy[2] = { x, y };

  if (name[2] == 'a') {
    r = a2s_ext(a, GLSLstd450FMax, ty, (uint32_t[]){ a2s_ext(a, GLSLstd450FMax, ty, xy, 2), z }, 2);
  } else if (name[2] == 'i') {
    r = a2s_ext(a, GLSLstd450FMin, ty, (uint32_t[]){ a2s_ext(a, GLSLstd450FMin, ty, xy, 2), z }, 2);
  } else {
    uint32_t lo = a2s_ext(a, GLSLstd450FMin, ty, xy, 2), hi = a2s_ext(a, GLSLstd450FMax, ty, xy, 2);
    r = a2s_ext(a, GLSLstd450FMax, ty, (uint32_t[]){ lo, a2s_ext(a, GLSLstd450FMin, ty, (uint32_t[]){ hi, z }, 2) }, 2);
  }

  done(a, c, r);
  return 1;
}

// a texel result: the vector, or {vector, i8} when the call returns a struct
static void texel_result(struct a2s *a, struct call *c, uint32_t v4, LLVMTypeRef vt) {
  uint32_t r = v4, w[2];
  unsigned n = a2s_lanes(vt);

  if (n < 4) {
    uint32_t sh[8] = { v4, v4, 0, 1, 2, 3 };
    r = n == 1 ? a2s_op(a, SpvOpCompositeExtract, a2s_type(a, vt), (uint32_t[]){ v4, 0 }, 2)
               : a2s_op(a, SpvOpVectorShuffle, a2s_type(a, vt), sh, 2 + n);
  }

  if (LLVMGetTypeKind(c->rt) == LLVMStructTypeKind) {
    w[0] = r;
    w[1] = spv_c_u32(&a->s, 0);
    r = a2s_op(a, SpvOpCompositeConstruct, a2s_type(a, c->rt), w, LLVMCountStructElementTypes(c->rt));
  }

  a2s_set(a, c->in, r);
}

static LLVMTypeRef texel_type(struct call *c) {
  return LLVMGetTypeKind(c->rt) == LLVMStructTypeKind ? LLVMStructGetTypeAtIndex(c->rt, 0) : c->rt;
}

static uint32_t elem_type(struct a2s *a, struct handle h) {
  char e = h.res >= 0 ? a->r->res[h.res].elem : 'f';

  return e == 'u' ? spv_t_uint(&a->s) : e == 's' ? spv_t_int(&a->s) : spv_t_float(&a->s);
}

// v (n lanes of type et) with one more lane x on the end
static uint32_t join_lane(struct a2s *a, uint32_t v, unsigned n, uint32_t et, uint32_t x) {
  uint32_t parts[4];
  unsigned k;

  for (k = 0; k < n && k < 3; k++) {
    parts[k] = n == 1 ? v : a2s_op(a, SpvOpCompositeExtract, et, (uint32_t[]){ v, k }, 2);
  }
  parts[k] = x;
  return a2s_op(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, et, k + 1), parts, k + 1);
}

// air.sample_texture_<type>[_grad](tex, sampler, coord[, slice], [gradients,] i1, offset, i1, lod, min lod, i32):
// a float right after an i1 is the lod (or bias), any other the min lod
static int air_sample(struct a2s *a, struct call *c) {
  struct handle tex = a2s_handle(a, arg_v(c, 0)), smp = a2s_handle(a, arg_v(c, 1));
  const char *ty = c->op + 15;
  int arrayed = strstr(ty, "_array") != 0, grad = strstr(ty, "_grad") != 0;
  uint32_t img, sm, si, w[16], n = 0, mask = 0, v4, ft = spv_t_float(&a->s), coord, gx = 0, gy = 0;
  int explicit_lod = a->stage != A2S_FRAGMENT, lod_given = 0, prev_i1 = 0;
  uint32_t lod = 0, bias = 0, off = 0;
  LLVMValueRef ov = 0, lv = 0, lg = 0;
  unsigned k = 3;

  if (tex.cube_as_array)
    a2s_fail(a, "cube texture sampled and read by texel");
  coord = arg(a, c, 2);
  // an array's layer joins the coordinate as a float
  if (arrayed && c->n > 3) {
    uint32_t layer = a2s_op1(a, SpvOpConvertUToF, ft, arg(a, c, k++));
    coord = join_lane(a, coord, a2s_lanes(arg_t(c, 2)), ft, layer);
  }

  for (; k < c->n; k++) {
    LLVMValueRef v = arg_v(c, k);
    LLVMTypeRef vt = LLVMTypeOf(v);
    int isint = LLVMGetTypeKind(a2s_scalar(vt)) == LLVMIntegerTypeKind;
    int i1 = isint && LLVMGetIntTypeWidth(a2s_scalar(vt)) == 1;

    if (LLVMGetTypeKind(vt) == LLVMVectorTypeKind && isint) {
      ov = v;
    } else if (LLVMGetTypeKind(vt) == LLVMVectorTypeKind) {
      if (!gx)
        gx = a2s_value(a, v);
      else
        gy = a2s_value(a, v);
    } else if (!isint && prev_i1 && !lv) {
      lv = v, lg = arg_v(c, k - 1);
    } else if (!isint && !(LLVMIsAConstantFP(v) && LLVMConstRealGetDouble(v, &(LLVMBool){ 0 }) == 0.0)) {
      a2s_note(a, "sample min lod clamp ignored");
    }
    prev_i1 = i1;
  }

  if (lv) {
    lod_given = LLVMIsAConstantInt(lg) ? (int)LLVMConstIntGetZExtValue(lg) : 1;
    if (lod_given)
      lod = a2s_value(a, lv);
    else if (!(LLVMIsAConstantFP(lv) && LLVMConstRealGetDouble(lv, &(LLVMBool){ 0 }) == 0.0))
      bias = a2s_value(a, lv);
  }

  if (ov && !LLVMIsAConstantAggregateZero(ov)) {
    unsigned k2, nl = a2s_lanes(LLVMTypeOf(ov));
    uint32_t it = spv_t_vec(&a->s, spv_t_int(&a->s), nl), parts[3];

    if (!LLVMIsAConstant(ov))
      a2s_fail(a, "sample with a texel offset that is not a constant");
    for (k2 = 0; k2 < nl && k2 < 3; k2++) {
      parts[k2] = spv_c_i32(&a->s, (int32_t)LLVMConstIntGetSExtValue(LLVMGetAggregateElement(ov, k2)));
    }
    off = spv_c_comp(&a->s, it, parts, nl);
  }

  img = a2s_op1(a, SpvOpLoad, tex.type, tex.var);
  sm = a2s_op1(a, SpvOpLoad, smp.type, smp.var);

  w[n++] = 0;
  w[n++] = coord;

  // texel coordinates over the level 0 size give what a normalized sampler takes
  if ((smp.pixel_const || smp.pixel_spec) && !arrayed && !tex.cube && !tex.three) {
    LLVMTypeRef ct = arg_t(c, 2);
    unsigned nl = a2s_lanes(ct);
    uint32_t iv = spv_t_vec(&a->s, spv_t_int(&a->s), nl), fv = a2s_type(a, ct), sz, div;

    spv_cap(&a->s, SpvCapabilityImageQuery);
    sz = a2s_op1(a, SpvOpConvertSToF, fv, a2s_op2(a, SpvOpImageQuerySizeLod, iv, img, spv_c_i32(&a->s, 0)));
    div = a2s_op2(a, SpvOpFDiv, fv, w[1], sz);
    w[1] = smp.pixel_const ? div : a2s_op(a, SpvOpSelect, fv, (uint32_t[]){ smp.pixel_spec, div, w[1] }, 3);
  }

  si = a2s_op2(a, SpvOpSampledImage, spv_t_sampled(&a->s, tex.type), img, sm);
  w[0] = si;
  if (grad && gx && gy)
    mask |= SpvImageOperandsGradMask;
  else if (lod_given || explicit_lod)
    mask |= SpvImageOperandsLodMask;
  else if (bias)
    mask |= SpvImageOperandsBiasMask;
  // cubes take no texel offset
  if (off && !tex.cube)
    mask |= SpvImageOperandsConstOffsetMask;

  if (mask) {
    w[n++] = mask;
    if (mask & SpvImageOperandsBiasMask)
      w[n++] = bias;
    if (mask & SpvImageOperandsLodMask)
      w[n++] = lod_given ? lod : spv_c_f32(&a->s, 0);
    if (mask & SpvImageOperandsGradMask)
      w[n++] = gx, w[n++] = gy;
    if (mask & SpvImageOperandsConstOffsetMask)
      w[n++] = off;
  }

  v4 = a2s_op(a, mask & (SpvImageOperandsLodMask | SpvImageOperandsGradMask) ? SpvOpImageSampleExplicitLod : SpvOpImageSampleImplicitLod,
              spv_t_vec(&a->s, elem_type(a, tex), 4), w, n);
  texel_result(a, c, v4, texel_type(c));
  return 1;
}

// integer coordinates as a signed vector: images take any integer, this keeps the validator content
static uint32_t coords(struct a2s *a, LLVMValueRef v) {
  return a2s_value(a, v);
}

// a texel coordinate with the slice of an array or the face of a cube (bound as a 2D array) joined on, and
// where the next operand is. A cube array's face comes before its slice
static uint32_t texel_coord(struct a2s *a, struct call *c, struct handle h, const char *ty, unsigned *next) {
  uint32_t coord = coords(a, arg_v(c, 1)), ut = spv_t_uint(&a->s), layer;
  int cube = !strncmp(ty, "cube", 4), arrayed = strstr(ty, "_array") != 0;

  *next = 2;
  if (!(cube || arrayed) || c->n <= 2)
    return coord;
  layer = arg(a, c, (*next)++);
  if (cube && arrayed && *next < c->n)
    layer = a2s_op2(a, SpvOpIAdd, ut, layer, a2s_op2(a, SpvOpIMul, ut, arg(a, c, (*next)++), spv_c_u32(&a->s, 6)));
  (void)h;
  return join_lane(a, coord, a2s_lanes(arg_t(c, 1)), ut, layer);
}

// a storage texture's texel with red and blue swapped when spec constant 64 + metal says its BGRA bytes
// sit in an RGBA image
static uint32_t bgra_swap(struct a2s *a, struct handle h, uint32_t v4, uint32_t vt) {
  struct a2s_resource *r;
  uint32_t b4 = spv_t_vec(&a->s, spv_t_bool(&a->s), 4), sw;

  if (h.res < 0 || !a->r->res[h.res].storage || a->r->res[h.res].metal >= 16)
    return v4;
  r = &a->r->res[h.res];
  if (!r->bgra_spec) {
    r->bgra_spec = spv_id(&a->s);
    spv_op(&a->s.globals, SpvOpSpecConstantFalse, 2, spv_t_bool(&a->s), r->bgra_spec);
    spv_deco(&a->s, r->bgra_spec, SpvDecorationSpecId, 1, A2S_SPEC_BGRA + r->metal);
  }
  sw = a2s_op(a, SpvOpVectorShuffle, vt, (uint32_t[]){ v4, v4, 2, 1, 0, 3 }, 6);
  return a2s_op(a, SpvOpSelect, vt,
                (uint32_t[]){ a2s_op(a, SpvOpCompositeConstruct, b4, (uint32_t[]){ r->bgra_spec, r->bgra_spec, r->bgra_spec, r->bgra_spec }, 4),
                              sw, v4 }, 3);
}

// air.read_texture_<type>[.i16][.u].<ty>(tex, coord[, slice or face], lod, ...)
static int air_read_tex(struct a2s *a, struct call *c) {
  struct handle tex = a2s_handle(a, arg_v(c, 0));
  int storage = tex.res >= 0 && a->r->res[tex.res].storage;
  uint32_t img = a2s_op1(a, SpvOpLoad, tex.type, tex.var), w[6], n = 0, v4;
  uint32_t vt = spv_t_vec(&a->s, elem_type(a, tex), 4);
  unsigned next;

  w[n++] = img;
  w[n++] = texel_coord(a, c, tex, c->op + 13, &next);
  if (!storage) {
    w[n++] = SpvImageOperandsLodMask;
    w[n++] = c->n > next ? arg(a, c, next) : spv_c_u32(&a->s, 0);
  }

  v4 = a2s_op(a, storage ? SpvOpImageRead : SpvOpImageFetch, vt, w, n);
  if (storage)
    v4 = bgra_swap(a, tex, v4, vt);
  texel_result(a, c, v4, texel_type(c));
  return 1;
}

// writes texel at coord of the level's view of a texture written at several levels. Without dynamic
// indexing of image arrays a switch picks the view by constant index
static void image_store(struct a2s *a, struct handle h, uint32_t lod, uint32_t coord, uint32_t texel) {
  uint32_t pt = spv_t_ptr(&a->s, SpvStorageClassUniformConstant, h.type), idx, img, w[40], merge, labs[16];
  unsigned n = 0, k;

  // metal writes NaN to a normalized format as 0, Vulkan leaves the conversion open. Spec constant
  // 48 + i says texture i has a normalized format
  if (h.res >= 0 && a->r->res[h.res].elem == 'f' && a->r->res[h.res].metal < 16) {
    uint32_t ft = spv_t_float(&a->s), b4 = spv_t_vec(&a->s, spv_t_bool(&a->s), 4), v4 = spv_t_vec(&a->s, ft, 4), spec, zero, nan;
    struct a2s_resource *r = &a->r->res[h.res];

    if (!r->unorm_spec) {
      spec = spv_id(&a->s);
      spv_op(&a->s.globals, SpvOpSpecConstantFalse, 2, spv_t_bool(&a->s), spec);
      spv_deco(&a->s, spec, SpvDecorationSpecId, 1, A2S_SPEC_UNORM + r->metal);
      r->unorm_spec = spec;
    }
    zero = spv_c_null(&a->s, v4);
    nan = a2s_op1(a, SpvOpIsNan, b4, texel);
    nan = a2s_op2(a, SpvOpLogicalAnd, b4, nan,
                  a2s_op(a, SpvOpCompositeConstruct, b4, (uint32_t[]){ r->unorm_spec, r->unorm_spec, r->unorm_spec, r->unorm_spec }, 4));
    texel = a2s_op(a, SpvOpSelect, v4, (uint32_t[]){ nan, zero, texel }, 3);
  }
  if (h.res >= 0 && a->r->res[h.res].elem == 'f')
    texel = bgra_swap(a, h, texel, spv_t_vec(&a->s, spv_t_float(&a->s), 4));

  if (!h.levels) {
    img = a2s_op1(a, SpvOpLoad, h.type, h.var);
    a2s_stmt(a, SpvOpImageWrite, (uint32_t[]){ img, coord, texel }, 3);
    return;
  }

  idx = a2s_ext(a, GLSLstd450UMin, spv_t_uint(&a->s), (uint32_t[]){ lod, spv_c_u32(&a->s, h.levels - 1) }, 2);
  if (!a->opt->no_dynamic_index) {
    img = a2s_op1(a, SpvOpLoad, h.type, a2s_op2(a, SpvOpAccessChain, pt, h.var, idx));
    a2s_stmt(a, SpvOpImageWrite, (uint32_t[]){ img, coord, texel }, 3);
    return;
  }

  merge = spv_id(&a->s);
  w[n++] = idx, w[n++] = merge;
  for (k = 0; k < h.levels && k < 16; k++) {
    labs[k] = spv_id(&a->s);
    w[n++] = k, w[n++] = labs[k];
  }
  spv_op(&a->body, SpvOpSelectionMerge, 2, merge, 0);
  spv_emit(&a->body, SpvOpSwitch, w, n);
  for (k = 0; k < h.levels && k < 16; k++) {
    spv_op(&a->body, SpvOpLabel, 1, labs[k]);
    img = a2s_op1(a, SpvOpLoad, h.type, a2s_op2(a, SpvOpAccessChain, pt, h.var, spv_c_u32(&a->s, k)));
    a2s_stmt(a, SpvOpImageWrite, (uint32_t[]){ img, coord, texel }, 3);
    spv_op(&a->body, SpvOpBranch, 1, merge);
  }
  spv_op(&a->body, SpvOpLabel, 1, merge);
}

// air.write_texture_<type>[.i16].<ty>(tex, coord[, slice or face], colour, lod, i32)
static int air_write_tex(struct a2s *a, struct call *c) {
  struct handle tex = a2s_handle(a, arg_v(c, 0));
  unsigned next, nl;
  uint32_t coord = texel_coord(a, c, tex, c->op + 14, &next), col, lod;
  LLVMTypeRef ct;

  if (c->n <= next)
    a2s_fail(a, "texture write without a colour");
  ct = arg_t(c, next);
  col = arg(a, c, next);
  lod = c->n > next + 1 ? arg(a, c, next + 1) : spv_c_u32(&a->s, 0);
  nl = a2s_lanes(ct);

  // images take four components
  if (nl < 4) {
    uint32_t parts[4], z = elem_type(a, tex) == spv_t_float(&a->s) ? spv_c_f32(&a->s, 0) : spv_c_u32(&a->s, 0);
    for (unsigned k = 0; k < 4; k++) {
      parts[k] = k < nl ? (nl == 1 ? col : a2s_op(a, SpvOpCompositeExtract, elem_type(a, tex), (uint32_t[]){ col, k }, 2)) : z;
    }
    col = a2s_op(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, elem_type(a, tex), 4), parts, 4);
  }

  image_store(a, tex, lod, coord, col);
  return 1;
}

// air.get_<width|height|depth|array_size|num_mip_levels>_texture_<type>(tex[, lod])
static int air_tex_size(struct a2s *a, struct call *c) {
  struct handle tex = a2s_handle(a, arg_v(c, 0));
  int storage = tex.res >= 0 && a->r->res[tex.res].storage;
  const char *what = c->op + 4;
  uint32_t img = a2s_op1(a, SpvOpLoad, tex.type, tex.var), ut = spv_t_uint(&a->s), sz, r;
  unsigned comps = 2 + (tex.arrayed || tex.three ? 1 : 0), pick;

  spv_cap(&a->s, SpvCapabilityImageQuery);
  if (!strncmp(what, "num_mip_levels", 14)) {
    r = storage ? spv_c_u32(&a->s, tex.levels ? tex.levels : 1) : a2s_op1(a, SpvOpImageQueryLevels, ut, img);
    a2s_set(a, c->in, a2s_mask(a, c->rt, r));
    return 1;
  }
  if (storage)
    sz = a2s_op1(a, SpvOpImageQuerySize, spv_t_vec(&a->s, ut, comps), img);
  else
    sz = a2s_op2(a, SpvOpImageQuerySizeLod, spv_t_vec(&a->s, ut, comps), img, c->n > 1 ? arg(a, c, 1) : spv_c_u32(&a->s, 0));

  pick = what[0] == 'w' ? 0 : what[0] == 'h' ? 1 : 2;
  if (pick >= comps)
    r = spv_c_u32(&a->s, 1);
  else
    r = a2s_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ sz, pick }, 2);
  // a cube array bound as faces counts its cubes
  if (what[0] == 'a' && tex.cube_as_array && tex.cube == 3)
    r = a2s_op2(a, SpvOpUDiv, ut, r, spv_c_u32(&a->s, 6));
  a2s_set(a, c->in, a2s_mask(a, c->rt, r));
  return 1;
}

// mem flags: 1 device, 2 threadgroup, 4 texture, 8 threadgroup imageblock
static uint32_t semantics(uint32_t flags) {
  uint32_t s = SpvMemorySemanticsAcquireReleaseMask;

  if (flags & 1)
    s |= SpvMemorySemanticsUniformMemoryMask;
  if (flags & (2 | 8))
    s |= SpvMemorySemanticsWorkgroupMemoryMask;
  if (flags & 4)
    s |= SpvMemorySemanticsImageMemoryMask;
  return flags ? s : 0;
}

static int air_barrier(struct a2s *a, struct call *c, int simd, int control) {
  uint32_t flags = c->n > 0 && LLVMIsAConstantInt(arg_v(c, 0)) ? (uint32_t)LLVMConstIntGetZExtValue(arg_v(c, 0)) : 3;
  uint32_t scope = spv_c_u32(&a->s, simd ? SpvScopeSubgroup : SpvScopeWorkgroup), w[3];

  w[0] = scope, w[1] = scope, w[2] = spv_c_u32(&a->s, semantics(flags));
  if (control) {
    a2s_stmt(a, SpvOpControlBarrier, w, 3);
    a->uses_barrier = 1, a->shader_barrier = 1;
  } else {
    a2s_stmt(a, SpvOpMemoryBarrier, w + 1, 2);
  }

  return 1;
}

static int air_discard(struct a2s *a, struct call *c) {
  (void)c;
  // OpKill ends a block, so it gets one of its own: a selection on true whose merge carries on with
  // the rest of the original block, which keeps the control flow structured
  if (a->opt->kill_discard) {
    uint32_t kill = spv_id(&a->s), rest = spv_id(&a->s), w[3] = { spv_c_bool(&a->s, 1), kill, rest };

    a2s_stmt(a, SpvOpSelectionMerge, (uint32_t[]){ rest, SpvSelectionControlMaskNone }, 2);
    a2s_stmt(a, SpvOpBranchConditional, w, 3);
    a2s_stmt(a, SpvOpLabel, &kill, 1);
    a2s_stmt(a, SpvOpKill, 0, 0);
    a2s_stmt(a, SpvOpLabel, &rest, 1);
    return 1;
  }
  spv_ext(&a->s, "SPV_EXT_demote_to_helper_invocation");
  spv_cap(&a->s, SpvCapabilityDemoteToHelperInvocation);
  a2s_stmt(a, SpvOpDemoteToHelperInvocation, 0, 0);
  a->uses_demote = 1;
  return 1;
}


// integer functions: mul_hi, clz, ctz, popcount, reverse_bits, rotate, abs_diff, add_sat, sub_sat,
// extract_bits, insert_bits. Small integers are zero extended in 32 bits, bits is the AIR width
static int air_int_bits(struct a2s *a, struct call *c, const char *op) {
  LLVMTypeRef t = c->rt;
  uint32_t ty = a2s_type(a, t), bt = spv_t_vec(&a->s, spv_t_bool(&a->s), a2s_lanes(t)), x, y, r;
  int sgn = strstr(c->op, ".s.") != NULL, bits = a2s_bits(t);

  if (LLVMGetTypeKind(a2s_scalar(t)) != LLVMIntegerTypeKind || bits > 32)
    return 0;
  x = arg(a, c, 0);

  if (!strcmp(op, "mul_hi")) {
    y = arg(a, c, 1);
    if (bits == 32) {
      uint32_t st = spv_t_struct(&a->s, (uint32_t[]){ ty, ty }, 2);
      if (sgn) {
        uint32_t it = spv_t_vec(&a->s, spv_t_int(&a->s), a2s_lanes(t)), ist = spv_t_struct(&a->s, (uint32_t[]){ it, it }, 2);
        r = a2s_op2(a, SpvOpSMulExtended, ist, a2s_op1(a, SpvOpBitcast, it, x), a2s_op1(a, SpvOpBitcast, it, y));
        r = a2s_op1(a, SpvOpBitcast, ty, a2s_op(a, SpvOpCompositeExtract, it, (uint32_t[]){ r, 1 }, 2));
      } else {
        r = a2s_op2(a, SpvOpUMulExtended, st, x, y);
        r = a2s_op(a, SpvOpCompositeExtract, ty, (uint32_t[]){ r, 1 }, 2);
      }
    } else {
      if (sgn)
        x = a2s_sext(a, t, x), y = a2s_sext(a, t, y);
      r = a2s_op2(a, sgn ? SpvOpShiftRightArithmetic : SpvOpShiftRightLogical, ty, a2s_op2(a, SpvOpIMul, ty, x, y),
                  a2s_uconst(a, t, (uint32_t)bits));
      r = a2s_mask(a, t, r);
    }
  } else if (!strcmp(op, "clz")) {
    // FindUMsb answers -1 for 0, so 31 - msb is 32 there, narrower types subtract their missing bits
    r = a2s_op2(a, SpvOpISub, ty, a2s_uconst(a, t, 31), a2s_ext(a, GLSLstd450FindUMsb, ty, &x, 1));
    if (bits < 32)
      r = a2s_op2(a, SpvOpISub, ty, r, a2s_uconst(a, t, 32 - (uint32_t)bits));
  } else if (!strcmp(op, "ctz")) {
    r = a2s_ext(a, GLSLstd450FindILsb, ty, &x, 1);
    r = a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ a2s_op2(a, SpvOpIEqual, bt, x, a2s_uconst(a, t, 0)), a2s_uconst(a, t, (uint32_t)bits), r }, 3);
  } else if (!strcmp(op, "popcount")) {
    r = a2s_op1(a, SpvOpBitCount, ty, x);
  } else if (!strcmp(op, "reverse_bits")) {
    r = a2s_op1(a, SpvOpBitReverse, ty, x);
    if (bits < 32)
      r = a2s_op2(a, SpvOpShiftRightLogical, ty, r, a2s_uconst(a, t, 32 - (uint32_t)bits));
  } else if (!strcmp(op, "rotate")) {
    uint32_t n = a2s_op2(a, SpvOpUMod, ty, arg(a, c, 1), a2s_uconst(a, t, (uint32_t)bits));
    uint32_t back = a2s_op2(a, SpvOpUMod, ty, a2s_op2(a, SpvOpISub, ty, a2s_uconst(a, t, (uint32_t)bits), n), a2s_uconst(a, t, (uint32_t)bits));
    r = a2s_op2(a, SpvOpBitwiseOr, ty, a2s_op2(a, SpvOpShiftLeftLogical, ty, x, n), a2s_op2(a, SpvOpShiftRightLogical, ty, x, back));
    r = a2s_mask(a, t, r);
  } else if (!strcmp(op, "abs_diff")) {
    uint32_t xs = sgn ? a2s_sext(a, t, x) : x, ys = sgn ? a2s_sext(a, t, arg(a, c, 1)) : arg(a, c, 1);
    uint32_t hi = a2s_ext(a, sgn ? GLSLstd450SMax : GLSLstd450UMax, ty, (uint32_t[]){ xs, ys }, 2);
    uint32_t lo = a2s_ext(a, sgn ? GLSLstd450SMin : GLSLstd450UMin, ty, (uint32_t[]){ xs, ys }, 2);
    r = a2s_mask(a, t, a2s_op2(a, SpvOpISub, ty, hi, lo));
  } else if (!strcmp(op, "add_sat") || !strcmp(op, "sub_sat")) {
    int add = op[0] == 'a';
    y = arg(a, c, 1);
    if (!sgn) {
      uint32_t s = a2s_op2(a, add ? SpvOpIAdd : SpvOpISub, ty, x, y), over;
      if (bits < 32) {
        // the true result fits 32 bits: clamp it to the range
        uint32_t it = spv_t_vec(&a->s, spv_t_int(&a->s), a2s_lanes(t)), si = a2s_op1(a, SpvOpBitcast, it, s);
        si = a2s_ext(a, GLSLstd450SClamp, it, (uint32_t[]){ si, a2s_op1(a, SpvOpBitcast, it, a2s_uconst(a, t, 0)), a2s_op1(a, SpvOpBitcast, it, a2s_uconst(a, t, (1u << bits) - 1)) }, 3);
        r = a2s_op1(a, SpvOpBitcast, ty, si);
      } else {
        over = a2s_op2(a, add ? SpvOpULessThan : SpvOpUGreaterThan, bt, s, x);
        r = a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ over, a2s_uconst(a, t, add ? 0xffffffffu : 0), s }, 3);
      }
    } else {
      uint32_t xs = a2s_sext(a, t, x), ys = a2s_sext(a, t, y), s = a2s_op2(a, add ? SpvOpIAdd : SpvOpISub, ty, xs, ys);
      if (bits < 32) {
        uint32_t it = spv_t_vec(&a->s, spv_t_int(&a->s), a2s_lanes(t)), si = a2s_op1(a, SpvOpBitcast, it, s);
        uint32_t lo = a2s_op1(a, SpvOpBitcast, it, a2s_uconst(a, t, (uint32_t)-(1 << (bits - 1)))), hi = a2s_op1(a, SpvOpBitcast, it, a2s_uconst(a, t, (1u << (bits - 1)) - 1));
        r = a2s_mask(a, t, a2s_op1(a, SpvOpBitcast, ty, a2s_ext(a, GLSLstd450SClamp, it, (uint32_t[]){ si, lo, hi }, 3)));
      } else {
        // overflow when the operands' signs (y's flipped for sub) agree and the result's differs
        uint32_t sx = a2s_op2(a, SpvOpShiftRightLogical, ty, xs, a2s_uconst(a, t, 31));
        uint32_t sy = a2s_op2(a, SpvOpShiftRightLogical, ty, ys, a2s_uconst(a, t, 31));
        uint32_t sr = a2s_op2(a, SpvOpShiftRightLogical, ty, s, a2s_uconst(a, t, 31));
        uint32_t same = add ? a2s_op2(a, SpvOpIEqual, bt, sx, sy) : a2s_op2(a, SpvOpINotEqual, bt, sx, sy);
        uint32_t over = a2s_op2(a, SpvOpLogicalAnd, bt, same, a2s_op2(a, SpvOpINotEqual, bt, sr, sx));
        uint32_t sat = a2s_op2(a, SpvOpIAdd, ty, a2s_uconst(a, t, 0x7fffffffu), sx);
        r = a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ over, sat, s }, 3);
      }
    }
  } else if (!strcmp(op, "extract_bits") || !strcmp(op, "insert_bits")) {
    int ins = op[0] == 'i';
    uint32_t off = arg(a, c, ins ? 2 : 1), cnt = arg(a, c, ins ? 3 : 2), w[4];
    if (ins) {
      w[0] = x, w[1] = arg(a, c, 1), w[2] = off, w[3] = cnt;
      r = a2s_mask(a, t, a2s_op(a, SpvOpBitFieldInsert, ty, w, 4));
    } else {
      w[0] = sgn ? a2s_op2(a, SpvOpShiftLeftLogical, ty, x, a2s_uconst(a, t, 32 - (uint32_t)bits)) : x;
      w[1] = sgn ? a2s_op2(a, SpvOpIAdd, spv_t_uint(&a->s), off, spv_c_u32(&a->s, 32 - (uint32_t)bits)) : off;
      w[2] = cnt;
      if (!sgn || bits == 32)
        w[0] = x, w[1] = off;
      r = a2s_mask(a, t, a2s_op(a, sgn ? SpvOpBitFieldSExtract : SpvOpBitFieldUExtract, ty, w, 3));
    }
  } else {
    return 0;
  }

  a2s_set(a, c->in, r);
  return 1;
}

// air.pack.<unorm|snorm><4x8|2x16>
static int air_pack(struct a2s *a, struct call *c) {
  const char *q = c->op + 5;
  int snorm = !strncmp(q, "snorm", 5), four = q[5] == '4';
  uint32_t v = arg(a, c, 0), ft = spv_t_float(&a->s), vt = spv_t_vec(&a->s, ft, four ? 4 : 2), r;
  unsigned op = snorm ? (four ? GLSLstd450PackSnorm4x8 : GLSLstd450PackSnorm2x16) : (four ? GLSLstd450PackUnorm4x8 : GLSLstd450PackUnorm2x16);

  if (strncmp(q, "unorm", 5) && !snorm)
    return 0;
  (void)vt;
  r = a2s_ext(a, op, spv_t_uint(&a->s), &v, 1);
  a2s_set(a, c->in, a2s_mask(a, c->rt, r));
  return 1;
}

// air.atomic.<global|local>.<op>[.s|.u].<ty>(ptr, [value,] order, scope, volatile) on the word the
// pointer names: 32 bit integer atomics only
static int air_atomic(struct a2s *a, struct call *c) {
  const char *q = strchr(c->op, '.') + 1, *opn = strchr(q, '.') + 1;
  struct aptr p = a2s_ptr(a, arg_v(c, 0));
  struct root *rt = &a->roots[p.root];
  uint32_t ut = spv_t_uint(&a->s), idx, wp, w[6], scope, sem = spv_c_u32(&a->s, 0), r = 0;
  int sgn = strstr(opn, ".s.") != NULL;
  unsigned op;

  if (strstr(opn, "f32") || strstr(opn, "f16") || strstr(opn, "i64"))
    a2s_fail(a, "atomic %s", c->op);
  idx = p.dyn ? a2s_op2(a, SpvOpIAdd, ut, p.dyn, spv_c_u32(&a->s, p.cst)) : spv_c_u32(&a->s, p.cst);
  idx = a2s_op2(a, SpvOpShiftRightLogical, ut, idx, spv_c_u32(&a->s, 2));
  wp = a2s_word_ptr(a, p.root, idx);
  scope = spv_c_u32(&a->s, rt->kind == ROOT_WG ? SpvScopeWorkgroup : SpvScopeDevice);

  if (!strncmp(opn, "load", 4)) {
    w[0] = wp, w[1] = scope, w[2] = sem;
    a2s_set(a, c->in, a2s_op(a, SpvOpAtomicLoad, ut, w, 3));
    return 1;
  }
  if (!strncmp(opn, "store", 5)) {
    w[0] = wp, w[1] = scope, w[2] = sem, w[3] = arg(a, c, 1);
    a2s_stmt(a, SpvOpAtomicStore, w, 4);
    return 1;
  }
  if (!strncmp(opn, "cmpxchg", 7)) {
    // (ptr, ptr expected, desired, ...): the old value goes back to *expected, the result is success
    struct aptr e = a2s_ptr(a, arg_v(c, 1));
    LLVMTypeRef i32t = LLVMInt32TypeInContext(a->ctx);
    uint32_t exp = a2s_load_mem(a, i32t, e, 4), old;
    w[0] = wp, w[1] = scope, w[2] = sem, w[3] = sem, w[4] = arg(a, c, 2), w[5] = exp;
    old = a2s_op(a, SpvOpAtomicCompareExchange, ut, w, 6);
    a2s_store_mem(a, i32t, old, e, 4);
    if (LLVMGetTypeKind(c->rt) != LLVMVoidTypeKind)
      a2s_set(a, c->in, a2s_op2(a, SpvOpIEqual, spv_t_bool(&a->s), old, exp));
    return 1;
  }

  if (!strncmp(opn, "add", 3))
    op = SpvOpAtomicIAdd;
  else if (!strncmp(opn, "sub", 3))
    op = SpvOpAtomicISub;
  else if (!strncmp(opn, "min", 3))
    op = sgn ? SpvOpAtomicSMin : SpvOpAtomicUMin;
  else if (!strncmp(opn, "max", 3))
    op = sgn ? SpvOpAtomicSMax : SpvOpAtomicUMax;
  else if (!strncmp(opn, "and", 3))
    op = SpvOpAtomicAnd;
  else if (!strncmp(opn, "or", 2))
    op = SpvOpAtomicOr;
  else if (!strncmp(opn, "xor", 3))
    op = SpvOpAtomicXor;
  else if (!strncmp(opn, "xchg", 4))
    op = SpvOpAtomicExchange;
  else
    a2s_fail(a, "atomic %s", c->op);

  w[0] = wp, w[1] = scope, w[2] = sem, w[3] = arg(a, c, 1);
  r = a2s_op(a, op, ut, w, 4);
  if (LLVMGetTypeKind(c->rt) != LLVMVoidTypeKind)
    a2s_set(a, c->in, r);
  return 1;
}

// lanes of a compute group cross through a workgroup array between two barriers: quads are 4 consecutive
// indices, simdgroups 32 (Apple's width), in uniform control flow as Metal's are
#define A2S_LANE_SLOTS 1024

static uint32_t lane_mem(struct a2s *a) {
  if (!a->quad_mem) {
    uint32_t u4 = spv_t_vec(&a->s, spv_t_uint(&a->s), 4);
    uint32_t arr = spv_t_array(&a->s, u4, spv_c_u32(&a->s, A2S_LANE_SLOTS));
    a->quad_mem = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassWorkgroup, arr), SpvStorageClassWorkgroup);
    a->iface[a->niface++] = a->quad_mem;
  }
  return a->quad_mem;
}

static void lane_barrier(struct a2s *a) {
  uint32_t bar[3];

  bar[0] = bar[1] = spv_c_u32(&a->s, SpvScopeWorkgroup);
  bar[2] = spv_c_u32(&a->s, SpvMemorySemanticsAcquireReleaseMask | SpvMemorySemanticsWorkgroupMemoryMask);
  a2s_stmt(a, SpvOpControlBarrier, bar, 3);
  a->uses_barrier = 1, a->lane_exchange = 1;
}

static uint32_t lane_pack(struct a2s *a, LLVMTypeRef vt, uint32_t v) {
  uint32_t ut = spv_t_uint(&a->s), parts[4];
  unsigned nl = a2s_lanes(vt), i;
  int isf = LLVMGetTypeKind(a2s_scalar(vt)) != LLVMIntegerTypeKind, isb = a2s_bits(vt) == 1;

  for (i = 0; i < 4; i++) {
    uint32_t e = i >= nl ? 0 : nl == 1 ? v : a2s_op(a, SpvOpCompositeExtract, a2s_type(a, a2s_scalar(vt)), (uint32_t[]){ v, i }, 2);
    parts[i] = !e ? spv_c_u32(&a->s, 0) : isb ? a2s_op(a, SpvOpSelect, ut, (uint32_t[]){ e, spv_c_u32(&a->s, 1), spv_c_u32(&a->s, 0) }, 3)
                                       : isf ? a2s_op1(a, SpvOpBitcast, ut, e) : e;
  }
  return a2s_op(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, ut, 4), parts, 4);
}

static uint32_t lane_unpack(struct a2s *a, LLVMTypeRef vt, uint32_t u4) {
  uint32_t ut = spv_t_uint(&a->s), parts[4];
  unsigned nl = a2s_lanes(vt), i;
  int isf = LLVMGetTypeKind(a2s_scalar(vt)) != LLVMIntegerTypeKind, isb = a2s_bits(vt) == 1;

  for (i = 0; i < nl; i++) {
    uint32_t e = a2s_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ u4, i }, 2);
    parts[i] = isb ? a2s_op2(a, SpvOpINotEqual, spv_t_bool(&a->s), e, spv_c_u32(&a->s, 0))
               : isf ? a2s_op1(a, SpvOpBitcast, spv_t_float(&a->s), e) : e;
  }
  return nl == 1 ? parts[0] : a2s_op(a, SpvOpCompositeConstruct, a2s_type(a, vt), parts, nl);
}

static uint32_t lane_load(struct a2s *a, LLVMTypeRef vt, uint32_t idx) {
  uint32_t u4 = spv_t_vec(&a->s, spv_t_uint(&a->s), 4), pt = spv_t_ptr(&a->s, SpvStorageClassWorkgroup, u4);

  return lane_unpack(a, vt, a2s_op1(a, SpvOpLoad, u4, a2s_op2(a, SpvOpAccessChain, pt, lane_mem(a), idx)));
}

enum { L_INDEX, L_XOR, L_UP, L_DOWN, L_FIRST, L_SUM, L_PRODUCT, L_MIN, L_MAX, L_AND, L_OR, L_XORR, L_EXSUM, L_INSUM,
       L_EXPROD, L_INPROD, L_ALL, L_ANY };

// one accumulation step of a reduction
static uint32_t lane_fold(struct a2s *a, LLVMTypeRef vt, unsigned op, int sgn, uint32_t acc, uint32_t v) {
  uint32_t ty = a2s_type(a, vt);
  int isf = LLVMGetTypeKind(a2s_scalar(vt)) != LLVMIntegerTypeKind, isb = a2s_bits(vt) == 1;

  switch (op) {
  case L_SUM: case L_EXSUM: case L_INSUM: return a2s_op2(a, isf ? SpvOpFAdd : SpvOpIAdd, ty, acc, v);
  case L_PRODUCT: case L_EXPROD: case L_INPROD: return a2s_op2(a, isf ? SpvOpFMul : SpvOpIMul, ty, acc, v);
  case L_MIN: return a2s_ext(a, isf ? GLSLstd450FMin : sgn ? GLSLstd450SMin : GLSLstd450UMin, ty, (uint32_t[]){ acc, v }, 2);
  case L_MAX: return a2s_ext(a, isf ? GLSLstd450FMax : sgn ? GLSLstd450SMax : GLSLstd450UMax, ty, (uint32_t[]){ acc, v }, 2);
  case L_AND: case L_ALL: return a2s_op2(a, isb ? SpvOpLogicalAnd : SpvOpBitwiseAnd, ty, acc, v);
  case L_OR: case L_ANY: return a2s_op2(a, isb ? SpvOpLogicalOr : SpvOpBitwiseOr, ty, acc, v);
  default: return a2s_op2(a, isb ? SpvOpLogicalNotEqual : SpvOpBitwiseXor, ty, acc, v);
  }
}

static uint32_t lanes(struct a2s *a, struct call *c, unsigned op, unsigned width, uint32_t larg, int sgn) {
  LLVMTypeRef vt = arg_t(c, 0);
  uint32_t ut = spv_t_uint(&a->s), bt = spv_t_bool(&a->s), u4 = spv_t_vec(&a->s, ut, 4), me, base, w[3], r;
  uint32_t pt = spv_t_ptr(&a->s, SpvStorageClassWorkgroup, u4);

  if (!a->wg_size || a->stage == A2S_TILE)
    a2s_fail(a, "lane operation %s outside a compute kernel", c->op);
  a2s_logical_group(a);
  me = a->lindex;
  base = a2s_op2(a, SpvOpBitwiseAnd, ut, me, spv_c_u32(&a->s, ~(width - 1)));

  w[0] = a2s_op2(a, SpvOpAccessChain, pt, lane_mem(a), me), w[1] = lane_pack(a, vt, arg(a, c, 0));
  a2s_stmt(a, SpvOpStore, w, 2);
  lane_barrier(a);

  if (op <= L_FIRST) {
    uint32_t src;
    if (op == L_INDEX)
      src = a2s_op2(a, SpvOpBitwiseOr, ut, base, a2s_op2(a, SpvOpBitwiseAnd, ut, larg, spv_c_u32(&a->s, width - 1)));
    else if (op == L_XOR)
      src = a2s_op2(a, SpvOpBitwiseXor, ut, me, a2s_op2(a, SpvOpBitwiseAnd, ut, larg, spv_c_u32(&a->s, width - 1)));
    else if (op == L_FIRST)
      src = base;
    else {
      // up/down past the group's edge keep their own value
      uint32_t s = a2s_op2(a, op == L_UP ? SpvOpISub : SpvOpIAdd, ut, me, larg);
      uint32_t in = a2s_op2(a, SpvOpIEqual, bt, a2s_op2(a, SpvOpBitwiseAnd, ut, s, spv_c_u32(&a->s, ~(width - 1))), base);
      src = a2s_op(a, SpvOpSelect, ut, (uint32_t[]){ in, s, me }, 3);
    }
    r = lane_load(a, vt, src);
  } else {
    // a loop over the group's lanes (prefix ops stop at this lane) into an accumulator
    uint32_t ty = a2s_type(a, vt), acc = spv_id(&a->s), kv = spv_id(&a->s), lh = spv_id(&a->s), lb = spv_id(&a->s);
    uint32_t lbody = spv_id(&a->s), lc = spv_id(&a->s), lm = spv_id(&a->s), k, end, init;
    int isf = LLVMGetTypeKind(a2s_scalar(vt)) != LLVMIntegerTypeKind, isb = a2s_bits(vt) == 1;

    init = op == L_PRODUCT || op == L_EXPROD || op == L_INPROD ? (isf ? a2s_fconst(a, vt, 1) : a2s_uconst(a, vt, 1))
           : op == L_ALL ? spv_c_bool(&a->s, 1) : op == L_ANY ? spv_c_bool(&a->s, 0)
           : op == L_AND ? a2s_uconst(a, vt, 0xffffffffu)
           : op == L_MIN || op == L_MAX ? 0 : isf ? a2s_fconst(a, vt, 0) : isb ? spv_c_bool(&a->s, 0) : a2s_uconst(a, vt, 0);
    if (!init)
      init = lane_load(a, vt, base);
    end = op == L_EXSUM || op == L_EXPROD ? me : op == L_INSUM || op == L_INPROD ? a2s_op2(a, SpvOpIAdd, ut, me, spv_c_u32(&a->s, 1))
                                                 : a2s_op2(a, SpvOpIAdd, ut, base, spv_c_u32(&a->s, width));
    spv_op(&a->vars, SpvOpVariable, 3, spv_t_ptr(&a->s, SpvStorageClassFunction, ty), acc, SpvStorageClassFunction);
    spv_op(&a->vars, SpvOpVariable, 3, spv_t_ptr(&a->s, SpvStorageClassFunction, ut), kv, SpvStorageClassFunction);
    spv_op(&a->body, SpvOpStore, 2, acc, init);
    spv_op(&a->body, SpvOpStore, 2, kv, base);
    spv_op(&a->body, SpvOpBranch, 1, lh);
    spv_op(&a->body, SpvOpLabel, 1, lh);
    spv_op(&a->body, SpvOpLoopMerge, 3, lm, lc, 0);
    spv_op(&a->body, SpvOpBranch, 1, lb);
    spv_op(&a->body, SpvOpLabel, 1, lb);
    k = a2s_op1(a, SpvOpLoad, ut, kv);
    spv_op(&a->body, SpvOpBranchConditional, 3, a2s_op2(a, SpvOpULessThan, bt, k, end), lbody, lm);
    spv_op(&a->body, SpvOpLabel, 1, lbody);
    spv_op(&a->body, SpvOpStore, 2, acc, lane_fold(a, vt, op, sgn, a2s_op1(a, SpvOpLoad, ty, acc), lane_load(a, vt, k)));
    spv_op(&a->body, SpvOpBranch, 1, lc);
    spv_op(&a->body, SpvOpLabel, 1, lc);
    spv_op(&a->body, SpvOpStore, 2, kv, a2s_op2(a, SpvOpIAdd, ut, k, spv_c_u32(&a->s, 1)));
    spv_op(&a->body, SpvOpBranch, 1, lh);
    spv_op(&a->body, SpvOpLabel, 1, lm);
    r = a2s_op1(a, SpvOpLoad, ty, acc);
  }

  lane_barrier(a);
  return r;
}

static const struct {
  const char *name;
  unsigned op;
  int arg;
} simd_ops[] = {
  { "simd_shuffle_xor.", L_XOR, 1 },     { "simd_shuffle_up.", L_UP, 1 },        { "simd_shuffle_down.", L_DOWN, 1 },
  { "simd_shuffle.", L_INDEX, 1 },       { "simd_broadcast_first.", L_FIRST, 0 }, { "simd_broadcast.", L_INDEX, 1 },
  { "simd_sum.", L_SUM, 0 },             { "simd_product.", L_PRODUCT, 0 },      { "simd_min.", L_MIN, 0 },
  { "simd_max.", L_MAX, 0 },             { "simd_and.", L_AND, 0 },              { "simd_or.", L_OR, 0 },
  { "simd_xor.", L_XORR, 0 },            { "simd_prefix_exclusive_sum.", L_EXSUM, 0 },
  { "simd_prefix_inclusive_sum.", L_INSUM, 0 }, { "simd_prefix_exclusive_product.", L_EXPROD, 0 },
  { "simd_prefix_inclusive_product.", L_INPROD, 0 }, { "simd_all", L_ALL, 0 }, { "simd_any", L_ANY, 0 },
};

static int air_simd(struct a2s *a, struct call *c) {
  for (unsigned i = 0; i < A2S_ARRAY_LEN(simd_ops); i++) {
    uint32_t larg;
    if (strncmp(c->op, simd_ops[i].name, strlen(simd_ops[i].name)))
      continue;
    larg = simd_ops[i].arg && c->n > 1 ? arg(a, c, 1) : spv_c_u32(&a->s, 0);
    a2s_set(a, c->in, lanes(a, c, simd_ops[i].op, 32, larg, strstr(c->op, ".s.") != NULL));
    return 1;
  }
  return 0;
}

// quad ops: a constant xor mask is a QuadSwap, a constant lane a QuadBroadcast. Without subgroup quads
// (PowerVR) kernels go through workgroup memory, a partner being the index with its lane bits changed
#define A2S_QUAD_SLOTS 1024

static uint32_t quad_memory(struct a2s *a, struct call *c, unsigned mode, unsigned k) {
  LLVMTypeRef vt = arg_t(c, 0);
  unsigned nl = a2s_lanes(vt), i;
  uint32_t ut = spv_t_uint(&a->s), u4 = spv_t_vec(&a->s, ut, 4), v = arg(a, c, 0), parts[4], packed, idx, src;
  uint32_t pt = spv_t_ptr(&a->s, SpvStorageClassWorkgroup, u4), bar[3], w[3], p, got;
  int isf = LLVMGetTypeKind(a2s_scalar(vt)) != LLVMIntegerTypeKind;

  if (a2s_bits(vt) == 1 || !a->wg_size)
    a2s_fail(a, "quad op through memory on a %s", a2s_bits(vt) == 1 ? "bool" : "non-kernel");

  if (!a->quad_mem) {
    uint32_t arr = spv_t_array(&a->s, u4, spv_c_u32(&a->s, A2S_QUAD_SLOTS));
    a->quad_mem = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassWorkgroup, arr), SpvStorageClassWorkgroup);
    a->iface[a->niface++] = a->quad_mem;
  }

  for (i = 0; i < 4; i++) {
    uint32_t e = i >= nl ? 0 : nl == 1 ? v : a2s_op(a, SpvOpCompositeExtract, a2s_type(a, a2s_scalar(vt)), (uint32_t[]){ v, i }, 2);
    parts[i] = !e ? spv_c_u32(&a->s, 0) : isf ? a2s_op1(a, SpvOpBitcast, ut, e) : e;
  }
  packed = a2s_op(a, SpvOpCompositeConstruct, u4, parts, 4);
  idx = a2s_op1(a, SpvOpLoad, ut, a2s_input_var(a, ut, SpvBuiltInLocalInvocationIndex, -1));
  if (mode == 0)
    src = a2s_op2(a, SpvOpBitwiseXor, ut, idx, spv_c_u32(&a->s, k));
  else
    src = a2s_op2(a, SpvOpBitwiseOr, ut, a2s_op2(a, SpvOpBitwiseAnd, ut, idx, spv_c_u32(&a->s, ~3u)), spv_c_u32(&a->s, k));

  bar[0] = bar[1] = spv_c_u32(&a->s, SpvScopeWorkgroup);
  bar[2] = spv_c_u32(&a->s, SpvMemorySemanticsAcquireReleaseMask | SpvMemorySemanticsWorkgroupMemoryMask);
  p = a2s_op2(a, SpvOpAccessChain, pt, a->quad_mem, idx);
  w[0] = p, w[1] = packed;
  a2s_stmt(a, SpvOpStore, w, 2);
  a2s_stmt(a, SpvOpControlBarrier, bar, 3);
  got = a2s_op1(a, SpvOpLoad, u4, a2s_op2(a, SpvOpAccessChain, pt, a->quad_mem, src));
  a2s_stmt(a, SpvOpControlBarrier, bar, 3);
  a->uses_barrier = 1;

  for (i = 0; i < nl; i++) {
    uint32_t e = a2s_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ got, i }, 2);
    parts[i] = isf ? a2s_op1(a, SpvOpBitcast, spv_t_float(&a->s), e) : e;
  }
  return nl == 1 ? parts[0] : a2s_op(a, SpvOpCompositeConstruct, a2s_type(a, vt), parts, nl);
}

static int air_quad(struct a2s *a, struct call *c) {
  uint32_t ty = a2s_type(a, arg_t(c, 0)), v = arg(a, c, 0), w[4];
  LLVMValueRef m = arg_v(c, 1);
  unsigned k;

  // a kernel's quads are 4 consecutive thread indices (tile functions keep 2x2 blocks)
  if (a->wg_size && a->stage != A2S_TILE && !a->tile_quads) {
    unsigned op = !strncmp(c->op, "quad_shuffle_xor.", 17) ? L_XOR : !strncmp(c->op, "quad_shuffle_up.", 16) ? L_UP
                  : !strncmp(c->op, "quad_shuffle_down.", 18) ? L_DOWN : L_INDEX;
    a2s_set(a, c->in, lanes(a, c, op, 4, arg(a, c, 1), 0));
    return 1;
  }

  if (a->opt->quad_memory && a->wg_size && LLVMIsAConstantInt(m)) {
    k = (unsigned)LLVMConstIntGetZExtValue(m) & 3;
    if (!strncmp(c->op, "quad_shuffle_xor.", 17))
      a2s_set(a, c->in, k ? quad_memory(a, c, 0, k) : v);
    else if (!strncmp(c->op, "quad_broadcast.", 15) || !strncmp(c->op, "quad_shuffle.", 13))
      a2s_set(a, c->in, quad_memory(a, c, 1, k));
    else
      a2s_fail(a, "quad op %s", c->op);
    return 1;
  }

  spv_cap(&a->s, SpvCapabilityGroupNonUniformQuad);
  a->uses_quad = 1;
  if (!LLVMIsAConstantInt(m))
    a2s_fail(a, "quad op with a lane that is not a constant");

  k = (unsigned)LLVMConstIntGetZExtValue(m) & 3;
  w[0] = spv_c_u32(&a->s, SpvScopeSubgroup), w[1] = v;

  if (!strncmp(c->op, "quad_shuffle_xor.", 17)) {
    if (!k) {
      a2s_set(a, c->in, v);
      return 1;
    }
    w[2] = spv_c_u32(&a->s, k == 1 ? 0 : k == 2 ? 1 : 2);
    a2s_set(a, c->in, a2s_op(a, SpvOpGroupNonUniformQuadSwap, ty, w, 3));
    return 1;
  }

  if (!strncmp(c->op, "quad_broadcast.", 15) || !strncmp(c->op, "quad_shuffle.", 13)) {
    w[2] = spv_c_u32(&a->s, k);
    a2s_set(a, c->in, a2s_op(a, SpvOpGroupNonUniformQuadBroadcast, ty, w, 3));
    return 1;
  }

  a2s_fail(a, "quad op %s", c->op);
}

// the explicit imageblock: per thread elements of a threadgroup sized tile in workgroup memory
static int imageblock_root(struct a2s *a) {
  static const uint32_t max_threads = 1024;
  uint32_t bytes = a->r->imageblock_bytes ? a->r->imageblock_bytes : 8;

  for (int i = 0; i < a->nroots; i++) {
    if (a->roots[i].kind == ROOT_WG && a->roots[i].pc_off == 0xffffffffu)
      return i;
  }

  {
    int r = a2s_add_root(a, ROOT_WG, max_threads * bytes / 4);
    a->roots[r].pc_off = 0xffffffffu;
    a2s_note(a, "imageblock sized for %u threads", max_threads);
    return r;
  }
}

static uint32_t lane16(struct a2s *a, uint32_t v2, unsigned k) {
  return a2s_op(a, SpvOpCompositeExtract, spv_t_uint(&a->s), (uint32_t[]){ v2, k }, 2);
}

// the imageblock's width: spec constant 5 from the encoder's setImageblockWidth, 0 = the threadgroup's
static uint32_t ib_width(struct a2s *a) {
  uint32_t ut = spv_t_uint(&a->s), tg, zero;

  if (a->ib_w)
    return a->ib_w;
  a->ib_spec = spv_id(&a->s);
  spv_op(&a->s.globals, SpvOpSpecConstant, 3, ut, a->ib_spec, 0);
  spv_deco(&a->s, a->ib_spec, SpvDecorationSpecId, 1, A2S_SPEC_IMAGEBLOCK);
  // in the prologue, so every use is dominated
  tg = spv_id(&a->s), zero = spv_id(&a->s), a->ib_w = spv_id(&a->s);
  spv_op(&a->pro, SpvOpCompositeExtract, 4, ut, tg, a->wg_size, 0);
  spv_op(&a->pro, SpvOpIEqual, 4, spv_t_bool(&a->s), zero, a->ib_spec, spv_c_u32(&a->s, 0));
  spv_op(&a->pro, SpvOpSelect, 5, ut, a->ib_w, zero, tg, a->ib_spec);
  return a->ib_w;
}

static uint32_t ib_offset(struct a2s *a, uint32_t x, uint32_t y) {
  uint32_t ut = spv_t_uint(&a->s), wx = ib_width(a);
  uint32_t bytes = a->r->imageblock_bytes ? a->r->imageblock_bytes : 8;
  uint32_t idx = a2s_op2(a, SpvOpIAdd, ut, a2s_op2(a, SpvOpIMul, ut, y, wx), x);

  return a2s_op2(a, SpvOpIMul, ut, idx, spv_c_u32(&a->s, bytes));
}

// air.imageblock_data(<2 x i16> coord, i32, i16): this coordinate's element
static int air_ib_data(struct a2s *a, struct call *c) {
  uint32_t co = arg(a, c, 0);
  struct aptr p = { imageblock_root(a), 0, 0 };
  struct vslot *s;

  if (!a->wg_size)
    a2s_fail(a, "imageblock outside a kernel");

  p.dyn = ib_offset(a, lane16(a, co, 0), lane16(a, co, 1));
  s = a2s_slot(a, c->in, 1);
  s->kind = VK_PTR, s->u.p = p;
  return 1;
}

// (tex, slice, i1, <2 x i16> origin, <2 x i16> size, <2 x i16> dest, i16 lod, i1, i32): the calling
// thread copies the region element by element, the colour being the element's leading half4
static int air_ib_write(struct a2s *a, struct call *c) {
  struct handle tex = a2s_handle(a, arg_v(c, 0));
  struct aptr slice = a2s_ptr(a, arg_v(c, 1));
  uint32_t ut = spv_t_uint(&a->s), bt = spv_t_bool(&a->s);
  uint32_t org = arg(a, c, 3), size = arg(a, c, 4), dst = arg(a, c, 5);
  uint32_t ox = lane16(a, org, 0), oy = lane16(a, org, 1), w = lane16(a, size, 0), h = lane16(a, size, 1);
  uint32_t dx = lane16(a, dst, 0), dy = lane16(a, dst, 1), total = a2s_op2(a, SpvOpIMul, ut, w, h);
  uint32_t ivar = spv_id(&a->s), lh = spv_id(&a->s), lb = spv_id(&a->s), lbody = spv_id(&a->s);
  uint32_t lc = spv_id(&a->s), lm = spv_id(&a->s), i, x, y, ws[4];
  LLVMTypeRef h4 = LLVMVectorType(LLVMHalfTypeInContext(a->ctx), 4);
  struct aptr e = slice;


  spv_op(&a->vars, SpvOpVariable, 3, spv_t_ptr(&a->s, SpvStorageClassFunction, ut), ivar, SpvStorageClassFunction);
  spv_op(&a->body, SpvOpStore, 2, ivar, spv_c_u32(&a->s, 0));
  spv_op(&a->body, SpvOpBranch, 1, lh);
  spv_op(&a->body, SpvOpLabel, 1, lh);
  spv_op(&a->body, SpvOpLoopMerge, 3, lm, lc, 0);
  spv_op(&a->body, SpvOpBranch, 1, lb);
  spv_op(&a->body, SpvOpLabel, 1, lb);
  i = a2s_op1(a, SpvOpLoad, ut, ivar);
  spv_op(&a->body, SpvOpBranchConditional, 3, a2s_op2(a, SpvOpULessThan, bt, i, total), lbody, lm);
  spv_op(&a->body, SpvOpLabel, 1, lbody);
  x = a2s_op2(a, SpvOpUMod, ut, i, w), y = a2s_op2(a, SpvOpUDiv, ut, i, w);

  // the element at origin + (x, y) of the slice, relative to the slice's own element
  e.dyn = a2s_op2(a, SpvOpIAdd, ut, slice.dyn ? slice.dyn : spv_c_u32(&a->s, 0),
                  ib_offset(a, a2s_op2(a, SpvOpIAdd, ut, ox, x), a2s_op2(a, SpvOpIAdd, ut, oy, y)));
  {
    uint32_t col = a2s_load_mem(a, h4, e, 8), co;
    co = a2s_op2(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, ut, 2), a2s_op2(a, SpvOpIAdd, ut, dx, x),
                 a2s_op2(a, SpvOpIAdd, ut, dy, y));
    (void)ws;
    image_store(a, tex, c->n > 6 ? arg(a, c, 6) : spv_c_u32(&a->s, 0), co, col);
  }

  spv_op(&a->body, SpvOpBranch, 1, lc);
  spv_op(&a->body, SpvOpLabel, 1, lc);
  spv_op(&a->body, SpvOpStore, 2, ivar, a2s_op2(a, SpvOpIAdd, ut, i, spv_c_u32(&a->s, 1)));
  spv_op(&a->body, SpvOpBranch, 1, lh);
  spv_op(&a->body, SpvOpLabel, 1, lm);
  return 1;
}

// the tile function's implicit imageblock is the pass's colour attachments, as storage images
static struct handle attachment(struct a2s *a, unsigned rt) {
  struct a2s_resource *r;
  uint32_t it, var;

  if (rt >= 8)
    a2s_fail(a, "implicit imageblock render target %u", rt);
  if (a->att[rt].var)
    return a->att[rt];

  if (a->r->nres == A2S_MAX_RES)
    a2s_fail(a, "too many resources");
  r = &a->r->res[a->r->nres];
  memset(r, 0, sizeof(*r));
  r->kind = A2S_RES_ATTACHMENT_IMAGE, r->metal = rt, r->set = a->opt->set, r->binding = 96 + rt;
  r->elem = 'f', r->storage = 1, r->dim = 2, r->access = A2S_READ | A2S_WRITE;
  it = spv_t_image(&a->s, spv_t_float(&a->s), SpvDim2D, 2, SpvImageFormatUnknown);
  var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassUniformConstant, it), SpvStorageClassUniformConstant);
  spv_deco(&a->s, var, SpvDecorationDescriptorSet, 1, a->opt->set);
  spv_deco(&a->s, var, SpvDecorationBinding, 1, r->binding);
  spv_cap(&a->s, SpvCapabilityStorageImageReadWithoutFormat);
  spv_cap(&a->s, SpvCapabilityStorageImageWriteWithoutFormat);
  a->iface[a->niface++] = var;
  a->att[rt].var = var, a->att[rt].type = it, a->att[rt].res = (int)a->r->nres++;
  return a->att[rt];
}

// the pixel of the attachment a tile thread addresses: tile origin (spec constants 3 and 4) plus coord
static uint32_t tile_pixel(struct a2s *a, uint32_t co) {
  uint32_t ut = spv_t_uint(&a->s), v2 = spv_t_vec(&a->s, ut, 2), wg, org;

  wg = a2s_logical_group(a);
  org = a2s_op(a, SpvOpVectorShuffle, v2, (uint32_t[]){ wg, wg, 0, 1 }, 4);
  org = a2s_op2(a, SpvOpIMul, v2, org, a->tile_size);
  return a2s_op2(a, SpvOpIAdd, v2, org, co);
}

static void tile_setup(struct a2s *a) {
  uint32_t ut = spv_t_uint(&a->s), c[2], id;

  if (a->tile_wgid)
    return;

  a->tile_wgid = a2s_input_var(a, spv_t_vec(&a->s, ut, 3), SpvBuiltInWorkgroupId, -1);

  for (unsigned k = 0; k < 2; k++) {
    c[k] = spv_id(&a->s);
    spv_op(&a->s.globals, SpvOpSpecConstant, 3, ut, c[k], 32);
    spv_deco(&a->s, c[k], SpvDecorationSpecId, 1, 3 + k);
  }

  id = spv_id(&a->s);
  spv_op(&a->s.globals, SpvOpSpecConstantComposite, 4, spv_t_vec(&a->s, ut, 2), id, c[0], c[1]);
  a->tile_size = id;
  a2s_note(a, "tile size in spec constants 3 and 4");
}

// air.load/store.implicit_imageblock.<ty>: the colour attachment at the tile pixel
static int air_ib_rw(struct a2s *a, struct call *c, int store) {
  unsigned vi = store ? 1 : 0;
  LLVMValueRef rtv = arg_v(c, vi);
  struct handle h;
  uint32_t co, img, w[3];
  unsigned rt;

  if (!LLVMIsAConstantInt(rtv))
    a2s_fail(a, "implicit imageblock with a render target that is not a constant");

  rt = (unsigned)LLVMConstIntGetZExtValue(rtv);
  a->stage = A2S_TILE, a->r->stage = A2S_TILE;
  tile_setup(a);
  h = attachment(a, rt);
  co = tile_pixel(a, arg(a, c, vi + 1));
  img = a2s_op1(a, SpvOpLoad, h.type, h.var);

  if (store) {
    uint32_t col = arg(a, c, 0);
    w[0] = img, w[1] = co, w[2] = col;
    a2s_stmt(a, SpvOpImageWrite, w, 3);
    return 1;
  }

  w[0] = img, w[1] = co;
  {
    uint32_t v4 = a2s_op(a, SpvOpImageRead, spv_t_vec(&a->s, spv_t_float(&a->s), 4), w, 2);
    texel_result(a, c, v4, c->rt);
  }
  return 1;
}

int a2s_air_call(struct a2s *a, LLVMValueRef in, const char *name) {
  struct call c;
  char op[48];
  const char *dot;
  size_t ol;

  if (strncmp(name, "air.", 4))
    return 0;

  c.in = in;
  c.rt = LLVMTypeOf(in);
  c.n = (unsigned)LLVMGetNumArgOperands(in);
  c.op = name + 4;
  if (!strncmp(c.op, "fast_", 5))
    c.op += 5;

  dot = strchr(c.op, '.');
  ol = dot ? (size_t)(dot - c.op) : strlen(c.op);
  if (ol >= sizeof(op))
    return 0;
  memcpy(op, c.op, ol);
  op[ol] = 0;

  // integer variants carry the signedness right after the op: air.min.s.v2i16
  if (dot && (dot[1] == 's' || dot[1] == 'u') && dot[2] == '.' &&
      (!strcmp(op, "min") || !strcmp(op, "max") || !strcmp(op, "clamp") || !strcmp(op, "abs")))
    return air_int(a, &c, op, dot[1]);

  if (!strcmp(op, "convert"))
    return air_convert(a, &c);
  if (!strcmp(op, "mul_hi") || !strcmp(op, "clz") || !strcmp(op, "ctz") || !strcmp(op, "popcount") ||
      !strcmp(op, "reverse_bits") || !strcmp(op, "rotate") || !strcmp(op, "abs_diff") || !strcmp(op, "add_sat") ||
      !strcmp(op, "sub_sat") || !strcmp(op, "extract_bits") || !strcmp(op, "insert_bits"))
    return air_int_bits(a, &c, op);
  if (!strcmp(op, "pack"))
    return air_pack(a, &c);
  if (!strcmp(op, "atomic"))
    return air_atomic(a, &c);
  if (!strncmp(op, "simd_", 5))
    return air_simd(a, &c);
  if (!strcmp(op, "unpack"))
    return air_unpack(a, &c);
  if (!strcmp(op, "dot"))
    return air_dot(a, &c);
  if (!strcmp(op, "any") || !strcmp(op, "all"))
    return air_anyall(a, &c);
  if (!strcmp(op, "dfdx") || !strcmp(op, "dfdy") || !strcmp(op, "fwidth"))
    return air_deriv(a, &c, op);
  if (!strcmp(op, "fmax3") || !strcmp(op, "fmin3") || !strcmp(op, "fmed3"))
    return air_minmax3(a, &c, op);
  if (!strcmp(op, "discard_fragment"))
    return air_discard(a, &c);
  if (!strncmp(op, "sample_texture_", 15))
    return air_sample(a, &c);
  if (!strncmp(op, "read_texture_", 13))
    return air_read_tex(a, &c);
  if (!strncmp(op, "write_texture_", 14))
    return air_write_tex(a, &c);
  if (!strncmp(op, "get_", 4) && strstr(op, "_texture_"))
    return air_tex_size(a, &c);
  if (!strcmp(op, "wg"))
    return air_barrier(a, &c, 0, 1);
  if (!strcmp(op, "simdgroup"))
    return air_barrier(a, &c, 1, 1);
  if (!strcmp(op, "mem_barrier"))
    return air_barrier(a, &c, 0, 0);
  if (!strcmp(op, "quad_shuffle_xor") || !strcmp(op, "quad_broadcast") || !strcmp(op, "quad_shuffle"))
    return air_quad(a, &c);
  if (!strcmp(op, "imageblock_data"))
    return air_ib_data(a, &c);
  if (!strncmp(op, "write_imageblock_slice_to_texture_2d", 36))
    return air_ib_write(a, &c);
  if (!strcmp(op, "load") && dot && !strncmp(dot, ".implicit_imageblock", 20))
    return air_ib_rw(a, &c, 0);
  if (!strcmp(op, "store") && dot && !strncmp(dot, ".implicit_imageblock", 20))
    return air_ib_rw(a, &c, 1);

  return air_math(a, &c, op);
}
