#include "air.h"

#include <string.h>

#include "../airjitd.h"
#include "../ops.h"
#include "../runtime.h"
#include "../texture.h"

// not this lowering's operation, try the next one
#define AIR_SKIP 1

struct air_fn {
  LLVMContextRef ctx;
  LLVMModuleRef mod;
  LLVMValueRef fn;
  LLVMTypeRef retty;
  LLVMBuilderRef b;
  unsigned np, flags;
  const char *name;
  // the name after "air." and "fast_"
  const char *op;
};

typedef int (*air_lowering)(struct air_fn *a);

static LLVMTypeRef air_i32(struct air_fn *a) {
  return LLVMInt32TypeInContext(a->ctx);
}

static LLVMTypeRef air_f32(struct air_fn *a) {
  return LLVMFloatTypeInContext(a->ctx);
}

static LLVMTypeRef air_ptr(struct air_fn *a) {
  return LLVMPointerTypeInContext(a->ctx, 0);
}

static LLVMTypeRef air_void(struct air_fn *a) {
  return LLVMVoidTypeInContext(a->ctx);
}

static LLVMValueRef air_int(struct air_fn *a, unsigned v) {
  return LLVMConstInt(air_i32(a), v, 0);
}

static int is_vector(LLVMTypeRef t) {
  return LLVMGetTypeKind(t) == LLVMVectorTypeKind;
}

static int is_struct(LLVMTypeRef t) {
  return LLVMGetTypeKind(t) == LLVMStructTypeKind;
}

static LLVMTypeRef elem_type(LLVMTypeRef t) {
  return is_vector(t) ? LLVMGetElementType(t) : t;
}

static unsigned lane_count(LLVMTypeRef t) {
  return is_vector(t) ? LLVMGetVectorSize(t) : 1;
}

static unsigned type_bits(LLVMTypeRef t) {
  switch (LLVMGetTypeKind(t)) {
  case LLVMIntegerTypeKind:
    return LLVMGetIntTypeWidth(t);
  case LLVMHalfTypeKind:
    return 16;
  case LLVMFloatTypeKind:
    return 32;
  default:
    return 64;
  }
}

// a float constant of type ty, splatted when ty is a vector
static LLVMValueRef const_fp(LLVMTypeRef ty, double v) {
  LLVMValueRef e[16], c;
  unsigned n, i;

  if (!is_vector(ty))
    return LLVMConstReal(ty, v);

  n = LLVMGetVectorSize(ty);
  c = LLVMConstReal(LLVMGetElementType(ty), v);
  for (i = 0; i < n && i < 16; i++) {
    e[i] = c;
  }

  return LLVMConstVector(e, n);
}

static void air_begin(struct air_fn *a) {
  LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(a->ctx, a->fn, "entry");

  a->b = LLVMCreateBuilderInContext(a->ctx);
  LLVMPositionBuilderAtEnd(a->b, bb);
}

static int air_return(struct air_fn *a, LLVMValueRef v) {
  if (v)
    LLVMBuildRet(a->b, v);
  else
    LLVMBuildRetVoid(a->b);

  LLVMDisposeBuilder(a->b);
  return 0;
}

static LLVMValueRef air_param(struct air_fn *a, unsigned i) {
  return LLVMGetParam(a->fn, i);
}

static LLVMValueRef air_lane(struct air_fn *a, LLVMValueRef v, unsigned k) {
  return is_vector(LLVMTypeOf(v)) ? LLVMBuildExtractElement(a->b, v, air_int(a, k), "") : v;
}

// vec is NULL for a scalar result
static LLVMValueRef air_set_lane(struct air_fn *a, LLVMValueRef vec, LLVMValueRef v, unsigned k) {
  return vec ? LLVMBuildInsertElement(a->b, vec, v, air_int(a, k), "") : v;
}

static LLVMValueRef air_to_f32(struct air_fn *a, LLVMValueRef v) {
  return LLVMTypeOf(v) == air_f32(a) ? v : LLVMBuildFPCast(a->b, v, air_f32(a), "");
}

static LLVMValueRef air_from_f32(struct air_fn *a, LLVMValueRef v, LLVMTypeRef ty) {
  return ty == air_f32(a) ? v : LLVMBuildFPCast(a->b, v, ty, "");
}

static LLVMValueRef air_fselect(struct air_fn *a, LLVMRealPredicate p, LLVMValueRef x, LLVMValueRef y,
                                LLVMValueRef then, LLVMValueRef other) {
  return LLVMBuildSelect(a->b, LLVMBuildFCmp(a->b, p, x, y, ""), then, other, "");
}

static LLVMValueRef air_iselect(struct air_fn *a, LLVMIntPredicate p, LLVMValueRef x, LLVMValueRef y,
                                LLVMValueRef then, LLVMValueRef other) {
  return LLVMBuildSelect(a->b, LLVMBuildICmp(a->b, p, x, y, ""), then, other, "");
}

// calls a function of this process through a constant pointer, so the JIT has nothing to resolve
static LLVMValueRef air_call(struct air_fn *a, void *impl, LLVMTypeRef ret, LLVMTypeRef *pts, LLVMValueRef *args,
                             unsigned n) {
  LLVMTypeRef fty = LLVMFunctionType(ret, pts, n, 0);
  LLVMValueRef fp = LLVMConstIntToPtr(LLVMConstInt(LLVMInt64TypeInContext(a->ctx), (uint64_t)(uintptr_t)impl, 0),
                                      air_ptr(a));

  return LLVMBuildCall2(a->b, fty, fp, args, n, "");
}

// the overloaded llvm.* intrinsic on the result type
static LLVMValueRef air_intrinsic(struct air_fn *a, const char *name, LLVMValueRef *args, unsigned n) {
  unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
  LLVMValueRef decl = LLVMGetIntrinsicDeclaration(a->mod, id, &a->retty, 1);
  LLVMTypeRef fty = LLVMIntrinsicGetType(a->ctx, id, &a->retty, 1);

  return LLVMBuildCall2(a->b, fty, decl, args, n, "");
}

// each lane through a float function, with an optional leading integer argument
static LLVMValueRef air_lanewise(struct air_fn *a, void *impl, unsigned nargs, int lead, unsigned lead_value) {
  LLVMTypeRef ety = elem_type(a->retty), pts[4];
  LLVMValueRef r = is_vector(a->retty) ? LLVMGetUndef(a->retty) : NULL;
  unsigned n = lane_count(a->retty), e, j, first = lead ? 1 : 0;

  pts[0] = lead ? air_i32(a) : air_f32(a);
  for (j = 1; j < 4; j++) {
    pts[j] = air_f32(a);
  }

  for (e = 0; e < n; e++) {
    LLVMValueRef args[4];
    if (lead)
      args[0] = air_int(a, lead_value);

    for (j = 0; j < nargs; j++) {
      args[first + j] = air_to_f32(a, air_lane(a, air_param(a, j), e));
    }

    r = air_set_lane(a, r, air_from_f32(a, air_call(a, impl, air_f32(a), pts, args, first + nargs), ety), e);
  }

  return r;
}

// a texel result: a bare vector, or {vector, i8} when the declaration returns a struct
static int air_return_texel(struct air_fn *a, LLVMValueRef vec) {
  LLVMValueRef agg;

  if (!is_struct(a->retty))
    return air_return(a, vec);

  agg = LLVMBuildInsertValue(a->b, LLVMGetUndef(a->retty), vec, 0, "");
  if (LLVMCountStructElementTypes(a->retty) > 1)
    agg = LLVMBuildInsertValue(a->b, agg, LLVMConstInt(LLVMInt8TypeInContext(a->ctx), 0, 0), 1, "");

  return air_return(a, agg);
}

// the half4 variants: the same float4, narrowed to what the shader expects
static LLVMValueRef air_narrow_half4(struct air_fn *a, LLVMValueRef vec) {
  if (!strstr(a->name, "v4f16"))
    return vec;

  return LLVMBuildFPTrunc(a->b, vec, LLVMVectorType(LLVMHalfTypeInContext(a->ctx), 4), "");
}

// widens an integer or vector-of-i16 argument to the i32 a helper takes
static LLVMValueRef air_as_i32(struct air_fn *a, LLVMValueRef v) {
  LLVMTypeRef t = LLVMTypeOf(v);

  if (LLVMGetTypeKind(t) == LLVMPointerTypeKind)
    return LLVMBuildAddrSpaceCast(a->b, v, air_ptr(a), "");

  if (is_vector(t))
    return LLVMBuildBitCast(a->b, v, air_i32(a), "");

  if (LLVMGetIntTypeWidth(t) < 32)
    return LLVMBuildZExt(a->b, v, air_i32(a), "");

  return v;
}

// air.convert.<to kind>.<to type>.<from kind>.<from type>: kinds f(loat), s(igned), u(nsigned)
static int air_convert(struct air_fn *a) {
  const char *t = a->op + 8, *f = strchr(t, '.');
  char to = t[0], from;
  LLVMValueRef v, r;
  unsigned dw, sw;

  if (a->np != 1)
    return AIR_SKIP;

  if (f)
    f = strchr(f + 1, '.');

  if (!f)
    return -1;

  from = f[1];
  air_begin(a);
  v = air_param(a, 0);
  dw = type_bits(elem_type(a->retty)), sw = type_bits(elem_type(LLVMTypeOf(v)));

  if (to == 'f' && from == 'f') {
    r = dw > sw ? LLVMBuildFPExt(a->b, v, a->retty, "") : dw < sw ? LLVMBuildFPTrunc(a->b, v, a->retty, "") : v;
  } else if (to == 'f') {
    r = from == 's' ? LLVMBuildSIToFP(a->b, v, a->retty, "") : LLVMBuildUIToFP(a->b, v, a->retty, "");
  } else if (from == 'f') {
    r = to == 's' ? LLVMBuildFPToSI(a->b, v, a->retty, "") : LLVMBuildFPToUI(a->b, v, a->retty, "");
  } else if (dw > sw) {
    r = from == 's' ? LLVMBuildSExt(a->b, v, a->retty, "") : LLVMBuildZExt(a->b, v, a->retty, "");
  } else {
    r = dw < sw ? LLVMBuildTrunc(a->b, v, a->retty, "") : v;
  }

  return air_return(a, r);
}

static unsigned parse_uint(const char **q) {
  unsigned n = 0;

  while (**q >= '0' && **q <= '9') {
    n = n * 10 + (unsigned)(*(*q)++ - '0');
  }

  return n;
}

// air.unpack.<unorm|snorm><N>x<B>.<type>: N lanes of B bits from a 32 bit word, scaled to [0,1]
// or [-1,1] (signed lanes clamp at -1), then converted to the result's element type
static int air_unpack(struct air_fn *a) {
  const char *q = a->op + 7;
  int snorm = !strncmp(q, "snorm", 5), unorm = !strncmp(q, "unorm", 5);
  unsigned n, bits, k;
  LLVMTypeRef f32 = air_f32(a), i32 = air_i32(a), ety = elem_type(a->retty);
  LLVMValueRef v, r;

  if (a->np != 1)
    return AIR_SKIP;

  if (!snorm && !unorm)
    return -1;

  q += 5;
  n = parse_uint(&q);

  if (*q++ != 'x')
    return -1;

  bits = parse_uint(&q);

  if (!n || !bits || n * bits > 32)
    return -1;

  air_begin(a);
  v = air_param(a, 0);
  if (LLVMGetTypeKind(LLVMTypeOf(v)) != LLVMIntegerTypeKind) {
    LLVMDisposeBuilder(a->b);
    return -1;
  }

  if (LLVMGetIntTypeWidth(LLVMTypeOf(v)) != 32)
    v = LLVMBuildZExtOrBitCast(a->b, v, i32, "");

  r = is_vector(a->retty) ? LLVMGetUndef(a->retty) : NULL;
  for (k = 0; k < n; k++) {
    LLVMValueRef e, f;
    if (snorm) {
      // move the lane to the top, then an arithmetic shift sign-extends it
      e = LLVMBuildShl(a->b, v, LLVMConstInt(i32, 32 - (k + 1) * bits, 0), "");
      e = LLVMBuildAShr(a->b, e, LLVMConstInt(i32, 32 - bits, 0), "");
      f = LLVMBuildFDiv(a->b, LLVMBuildSIToFP(a->b, e, f32, ""), const_fp(f32, (double)((1u << (bits - 1)) - 1)), "");
      f = air_fselect(a, LLVMRealOLT, f, const_fp(f32, -1.0), const_fp(f32, -1.0), f);
    } else {
      e = LLVMBuildLShr(a->b, v, LLVMConstInt(i32, k * bits, 0), "");
      e = bits < 32 ? LLVMBuildAnd(a->b, e, LLVMConstInt(i32, (1u << bits) - 1, 0), "") : e;
      f = LLVMBuildFDiv(a->b, LLVMBuildUIToFP(a->b, e, f32, ""),
                        const_fp(f32, (double)(bits < 32 ? (1u << bits) - 1 : 0xffffffffu)), "");
    }

    if (LLVMGetTypeKind(ety) != LLVMFloatTypeKind) {
      f = LLVMGetTypeKind(ety) == LLVMHalfTypeKind ? LLVMBuildFPTrunc(a->b, f, ety, "")
                                                    : LLVMBuildFPExt(a->b, f, ety, "");
    }

    r = air_set_lane(a, r, f, k);
  }

  return air_return(a, r);
}

static LLVMValueRef air_math_native(struct air_fn *a, const struct air_math_op *m) {
  static const char *const intrinsics[] = {
    [AIR_FLOOR] = "llvm.floor",   [AIR_CEIL] = "llvm.ceil",     [AIR_TRUNC] = "llvm.trunc",
    [AIR_RINT] = "llvm.rint",     [AIR_ROUND] = "llvm.round",   [AIR_SQRT] = "llvm.sqrt",
    [AIR_FABS] = "llvm.fabs",     [AIR_MINNUM] = "llvm.minnum", [AIR_MAXNUM] = "llvm.maxnum",
    [AIR_FMA] = "llvm.fma",
  };
  LLVMValueRef x = air_param(a, 0), args[3], r;
  LLVMTypeRef ty = a->retty;
  unsigned i;

  switch (m->native) {
  case AIR_FRACT:
    return LLVMBuildFSub(a->b, x, air_intrinsic(a, "llvm.floor", &x, 1), "");
  case AIR_RSQRT:
    return LLVMBuildFDiv(a->b, const_fp(ty, 1.0), air_intrinsic(a, "llvm.sqrt", &x, 1), "");
  case AIR_SIGN:
    r = air_fselect(a, LLVMRealOLT, x, const_fp(ty, 0.0), const_fp(ty, -1.0), x);
    return air_fselect(a, LLVMRealOGT, x, const_fp(ty, 0.0), const_fp(ty, 1.0), r);
  case AIR_CLAMP:
    r = air_fselect(a, LLVMRealOLT, x, air_param(a, 1), air_param(a, 1), x);
    return air_fselect(a, LLVMRealOGT, r, air_param(a, 2), air_param(a, 2), r);
  case AIR_SATURATE:
    r = air_fselect(a, LLVMRealOLT, x, const_fp(ty, 0.0), const_fp(ty, 0.0), x);
    return air_fselect(a, LLVMRealOGT, r, const_fp(ty, 1.0), const_fp(ty, 1.0), r);
  case AIR_MIX: {
    LLVMValueRef y = air_param(a, 1), t = air_param(a, 2);
    return LLVMBuildFAdd(a->b, x, LLVMBuildFMul(a->b, LLVMBuildFSub(a->b, y, x, ""), t, ""), "");
  }
  default:
    for (i = 0; i < m->arity; i++) {
      args[i] = air_param(a, i);
    }
    return air_intrinsic(a, intrinsics[m->native], args, m->arity);
  }
}

// air.[fast_]<op>.<type> on half or float scalars and vectors, from the table in ops.c
static int air_math(struct air_fn *a) {
  const char *end = strchr(a->op, '.');
  const struct air_math_op *m;
  LLVMTypeKind ek = LLVMGetTypeKind(elem_type(a->retty));
  char op[24];
  size_t ol;

  if (!end || (ol = (size_t)(end - a->op)) >= sizeof(op))
    return AIR_SKIP;

  if (ek != LLVMHalfTypeKind && ek != LLVMFloatTypeKind)
    return AIR_SKIP;

  memcpy(op, a->op, ol);
  op[ol] = 0;
  m = air_math_find(op, a->np);

  if (!m)
    return AIR_SKIP;

  air_begin(a);

  if (m->native == AIR_CALL)
    return air_return(a, air_lanewise(a, (void *)m->impl.f1, m->arity, 0, 0));

  return air_return(a, air_math_native(a, m));
}

static int air_dot(struct air_fn *a) {
  LLVMValueRef m, r;
  unsigned n, i;

  if (a->np != 2)
    return AIR_SKIP;

  air_begin(a);
  m = LLVMBuildFMul(a->b, air_param(a, 0), air_param(a, 1), "");
  n = LLVMGetVectorSize(LLVMTypeOf(m));
  r = LLVMBuildExtractElement(a->b, m, air_int(a, 0), "");
  for (i = 1; i < n; i++) {
    r = LLVMBuildFAdd(a->b, r, LLVMBuildExtractElement(a->b, m, air_int(a, i), ""), "");
  }

  return air_return(a, r);
}

// air.any.vNi1: true if any lane is set, as a test of the lanes bitcast to iN
static int air_any(struct air_fn *a) {
  LLVMValueRef in, bits, r;
  unsigned n;

  if (a->np != 1)
    return AIR_SKIP;

  air_begin(a);
  in = air_param(a, 0);
  n = lane_count(LLVMTypeOf(in));
  bits = n > 1 ? LLVMBuildBitCast(a->b, in, LLVMIntTypeInContext(a->ctx, n), "") : in;
  r = LLVMBuildICmp(a->b, LLVMIntNE, bits, LLVMConstNull(LLVMTypeOf(bits)), "");

  if (LLVMGetTypeKind(a->retty) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(a->retty) > 1)
    r = LLVMBuildZExt(a->b, r, a->retty, "");

  return air_return(a, r);
}

// air.min/max/clamp.{s,u}.*: integer compare and select, scalar or vector
static int air_int_minmax(struct air_fn *a) {
  int clamp = a->op[0] == 'c', at = clamp ? 6 : 4;
  int sgn = a->op[at] == 's', is_max = a->op[1] == 'a';
  LLVMIntPredicate lt = sgn ? LLVMIntSLT : LLVMIntULT, gt = sgn ? LLVMIntSGT : LLVMIntUGT;
  LLVMValueRef x, lo, hi;

  if ((a->op[at] != 's' && a->op[at] != 'u') || a->op[at + 1] != '.')
    return AIR_SKIP;

  if (a->np != (clamp ? 3u : 2u))
    return -1;

  air_begin(a);
  x = air_param(a, 0), lo = air_param(a, 1);

  if (clamp) {
    hi = air_param(a, 2);
    x = air_iselect(a, lt, x, lo, lo, x);
    x = air_iselect(a, gt, x, hi, hi, x);
  } else {
    x = air_iselect(a, is_max ? gt : lt, x, lo, x, lo);
  }

  return air_return(a, x);
}

// air.abs.s.* / air.abs.u.*: integer abs, scalar or vector (unsigned is the identity)
static int air_int_abs(struct air_fn *a) {
  LLVMValueRef x;

  if (a->np != 1)
    return AIR_SKIP;

  air_begin(a);
  x = air_param(a, 0);

  if (a->op[4] == 's')
    x = air_iselect(a, LLVMIntSLT, x, LLVMConstNull(LLVMTypeOf(x)), LLVMBuildNeg(a->b, x, ""), x);

  return air_return(a, x);
}

// dfdx/dfdy/fwidth, scalar or vector, half or float: pd_deriv per lane
static int air_deriv(struct air_fn *a) {
  unsigned kind = a->op[0] == 'f' ? PD_DERIV_FWIDTH : a->op[3] == 'x' ? PD_DERIV_DX : PD_DERIV_DY;

  if (a->np != 1)
    return -1;

  air_begin(a);
  a->flags |= AIR_USES_DERIV;
  return air_return(a, air_lanewise(a, (void *)&pd_deriv, 1, 1, kind));
}

static int air_discard(struct air_fn *a) {
  air_begin(a);
  air_call(a, (void *)&pd_discard, air_void(a), NULL, NULL, 0);
  return air_return(a, NULL);
}

// air.[fast_]fmax3 / fmin3 / fmed3, scalar or vector
static int air_minmax3(struct air_fn *a) {
  LLVMValueRef a0, a1, a2, lo, hi, r;

  if (a->np != 3)
    return AIR_SKIP;

  air_begin(a);
  a0 = air_param(a, 0), a1 = air_param(a, 1), a2 = air_param(a, 2);
  lo = air_fselect(a, LLVMRealOLT, a0, a1, a0, a1);
  hi = air_fselect(a, LLVMRealOGT, a0, a1, a0, a1);

  if (a->op[2] == 'a') {
    r = air_fselect(a, LLVMRealOGT, hi, a2, hi, a2);
  } else if (a->op[2] == 'i') {
    r = air_fselect(a, LLVMRealOLT, lo, a2, lo, a2);
  } else {
    // median = max(lo, min(hi, a2))
    r = air_fselect(a, LLVMRealOLT, hi, a2, hi, a2);
    r = air_fselect(a, LLVMRealOGT, lo, r, lo, r);
  }

  return air_return(a, r);
}

// 3D textures are colour lookup tables here: answer the coordinate, an identity transform
static int air_sample_3d(struct air_fn *a) {
  LLVMValueRef coord, vec;
  LLVMTypeRef f32 = air_f32(a);
  unsigned k;

  if (a->np < 3)
    return AIR_SKIP;

  air_begin(a);
  coord = air_param(a, 2);
  vec = LLVMGetUndef(LLVMVectorType(f32, 4));
  for (k = 0; k < 4; k++) {
    LLVMValueRef e = k < 3 && is_vector(LLVMTypeOf(coord)) ? air_lane(a, coord, k) : const_fp(f32, 1.0);
    vec = LLVMBuildInsertElement(a->b, vec, air_to_f32(a, e), air_int(a, k), "");
  }

  return air_return_texel(a, air_narrow_half4(a, vec));
}

static int air_sample_2d(struct air_fn *a) {
  LLVMTypeRef f32 = air_f32(a), ptr = air_ptr(a), v4f = LLVMVectorType(f32, 4);
  LLVMTypeRef pts[5] = { ptr, ptr, f32, f32, ptr };
  LLVMValueRef args[5], out, coord;

  if (a->np < 3)
    return -1;

  air_begin(a);
  out = LLVMBuildAlloca(a->b, v4f, "out");
  coord = air_param(a, 2);
  args[0] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), ptr, "");
  args[1] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 1), ptr, "");
  args[2] = air_lane(a, coord, 0);
  args[3] = air_lane(a, coord, 1);
  args[4] = out;
  air_call(a, (void *)&pd_sample, air_void(a), pts, args, 5);
  return air_return_texel(a, air_narrow_half4(a, LLVMBuildLoad2(a->b, v4f, out, "")));
}

// the threads of a group run one after another here, so a barrier has nothing to wait for
static int air_barrier(struct air_fn *a) {
  if (LLVMGetTypeKind(a->retty) != LLVMVoidTypeKind)
    return AIR_SKIP;

  air_begin(a);
  return air_return(a, NULL);
}

// air.imageblock_data(<2 x i16> coord, i32 size, i16): this thread's element in the group's tile
static int air_imageblock_data(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), pts[3] = { i32, i32, i32 };
  LLVMValueRef args[3], r;

  if (a->np != 3)
    return AIR_SKIP;

  air_begin(a);
  args[0] = LLVMBuildBitCast(a->b, air_param(a, 0), i32, "");
  args[1] = air_param(a, 1);
  args[2] = LLVMBuildZExt(a->b, air_param(a, 2), i32, "");
  r = air_call(a, (void *)&pd_ib_data, air_ptr(a), pts, args, 3);
  return air_return(a, LLVMBuildAddrSpaceCast(a->b, r, a->retty, ""));
}

// (tex, slice, i1, <2 x i16> x3, i16, i1, i32): the tile to the texture
static int air_imageblock_write(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a);
  LLVMTypeRef pts[9] = { ptr, ptr, i32, i32, i32, i32, i32, i32, i32 };
  LLVMValueRef args[9];
  unsigned k;

  if (a->np != 9)
    return AIR_SKIP;

  air_begin(a);
  for (k = 0; k < 9; k++) {
    args[k] = air_as_i32(a, air_param(a, k));
  }

  air_call(a, (void *)&pd_ib_write, air_void(a), pts, args, 9);
  return air_return(a, NULL);
}

// air.get_width/height_texture_2d(tex, lod): the bound texture's size
static int air_texture_size(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), pts[1] = { air_ptr(a) };
  void *impl = a->op[4] == 'w' ? (void *)&pd_tex_width : (void *)&pd_tex_height;
  LLVMValueRef tex, r;

  if (a->np < 1)
    return AIR_SKIP;

  air_begin(a);
  tex = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), air_ptr(a), "");
  r = air_call(a, impl, i32, pts, &tex, 1);

  if (LLVMGetTypeKind(a->retty) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(a->retty) != 32)
    r = LLVMBuildZExtOrBitCast(a->b, LLVMBuildTrunc(a->b, r, a->retty, ""), a->retty, "");

  return air_return(a, r);
}

// the colour argument of a texture write as the float4 pd_write_tex takes
static LLVMValueRef air_texel_in(struct air_fn *a, LLVMValueRef c) {
  LLVMTypeRef f32 = air_f32(a), et = elem_type(LLVMTypeOf(c));
  LLVMValueRef vec = LLVMConstNull(LLVMVectorType(f32, 4));
  unsigned n = lane_count(LLVMTypeOf(c)), k;

  for (k = 0; k < n && k < 4; k++) {
    LLVMValueRef e = air_lane(a, c, k);
    if (LLVMGetTypeKind(et) == LLVMIntegerTypeKind)
      e = LLVMBuildUIToFP(a->b, e, f32, "");
    else if (et != f32)
      e = LLVMBuildFPExt(a->b, e, f32, "");

    vec = LLVMBuildInsertElement(a->b, vec, e, air_int(a, k), "");
  }

  return vec;
}

// the float4 pd_read_tex filled, as the vector the declaration returns
static LLVMValueRef air_texel_out(struct air_fn *a, LLVMValueRef vec) {
  LLVMTypeRef vt = is_struct(a->retty) ? LLVMStructGetTypeAtIndex(a->retty, 0) : a->retty, et = elem_type(vt);
  unsigned n = lane_count(vt), k;
  LLVMValueRef r = n > 1 ? LLVMGetUndef(vt) : NULL;

  for (k = 0; k < n; k++) {
    LLVMValueRef e = LLVMBuildExtractElement(a->b, vec, air_int(a, k < 4 ? k : 3), "");
    if (LLVMGetTypeKind(et) == LLVMIntegerTypeKind)
      e = LLVMBuildFPToUI(a->b, e, et, "");
    else if (et != air_f32(a))
      e = LLVMBuildFPTrunc(a->b, e, et, "");

    r = air_set_lane(a, r, e, k);
  }

  return r;
}

// air.read_texture_2d.*(tex, int2 coord, ...) and air.write_texture_2d.*(tex, int2 coord, colour, ...)
static int air_texture_rw(struct air_fn *a) {
  int write = a->op[0] == 'w';
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), v4f = LLVMVectorType(air_f32(a), 4);
  LLVMTypeRef pts[4] = { ptr, i32, i32, ptr };
  LLVMValueRef args[4], coord, buf;

  if (a->np < 2)
    return AIR_SKIP;

  air_begin(a);
  coord = air_param(a, 1);
  args[0] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), ptr, "");
  args[1] = air_lane(a, coord, 0);
  args[2] = air_lane(a, coord, 1);

  if (LLVMGetTypeKind(LLVMTypeOf(args[1])) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(args[1])) != 32) {
    args[1] = LLVMBuildZExt(a->b, args[1], i32, "");
    args[2] = LLVMBuildZExt(a->b, args[2], i32, "");
  }

  buf = LLVMBuildAlloca(a->b, v4f, "px");
  args[3] = buf;

  if (write) {
    LLVMBuildStore(a->b, air_texel_in(a, air_param(a, 2)), buf);
    air_call(a, (void *)&pd_write_tex, air_void(a), pts, args, 4);
    return air_return(a, NULL);
  }

  air_call(a, (void *)&pd_read_tex, air_void(a), pts, args, 4);
  return air_return_texel(a, air_texel_out(a, LLVMBuildLoad2(a->b, v4f, buf, "")));
}

// matched in order against the name after "air." and "fast_", the first that does not skip wins
static const struct {
  const char *prefix;
  int exact;
  air_lowering lower;
} lowerings[] = {
  { "convert.", 0, air_convert },
  { "unpack.", 0, air_unpack },
  { "", 0, air_math },
  { "dot.", 0, air_dot },
  { "any.v", 0, air_any },
  { "min.", 0, air_int_minmax },
  { "max.", 0, air_int_minmax },
  { "clamp.", 0, air_int_minmax },
  { "abs.s.", 0, air_int_abs },
  { "abs.u.", 0, air_int_abs },
  { "dfdx.", 0, air_deriv },
  { "dfdy.", 0, air_deriv },
  { "fwidth.", 0, air_deriv },
  { "discard_fragment", 1, air_discard },
  { "fmax3.", 0, air_minmax3 },
  { "fmin3.", 0, air_minmax3 },
  { "fmed3.", 0, air_minmax3 },
  { "sample_texture_3d.v4f", 0, air_sample_3d },
  { "sample_texture_2d.v4f32", 1, air_sample_2d },
  { "sample_texture_2d.v4f16", 1, air_sample_2d },
  { "wg.barrier", 0, air_barrier },
  { "mem_barrier", 0, air_barrier },
  { "simdgroup.barrier", 0, air_barrier },
  { "imageblock_data", 1, air_imageblock_data },
  { "write_imageblock_slice_to_texture_2d.", 0, air_imageblock_write },
  { "get_width_texture_2d", 1, air_texture_size },
  { "get_height_texture_2d", 1, air_texture_size },
  { "read_texture_2d.", 0, air_texture_rw },
  { "write_texture_2d.", 0, air_texture_rw },
};

int air_lower(LLVMModuleRef mod, LLVMValueRef fn, const char *name, unsigned *flags) {
  struct air_fn a = { 0 };
  unsigned i;

  a.ctx = LLVMGetModuleContext(mod);
  a.mod = mod;
  a.fn = fn;
  a.retty = LLVMGetReturnType(LLVMGlobalGetValueType(fn));
  a.np = LLVMCountParams(fn);
  a.name = name;
  a.op = name + 4;

  if (!strncmp(a.op, "fast_", 5))
    a.op += 5;

  for (i = 0; i < PD_ARRAY_LEN(lowerings); i++) {
    size_t n = strlen(lowerings[i].prefix);
    int rc;

    if (lowerings[i].exact ? strcmp(a.op, lowerings[i].prefix) : strncmp(a.op, lowerings[i].prefix, n))
      continue;

    rc = lowerings[i].lower(&a);
    if (rc != AIR_SKIP) {
      *flags |= a.flags;
      return rc;
    }
  }

  return -2;
}
