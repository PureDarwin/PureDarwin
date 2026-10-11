#include "air.h"

#include <stdio.h>
#include <string.h>

#include "../airjitd.h"
#include "../ops.h"
#include "../runtime.h"
#include "../texture.h"
#include "../../air2spv/air2spv.h"

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

// the runtime functions shader code calls. Code refers to them by name, so an object compiled by one run
// of the daemon links against the next run's addresses
static const struct {
  const char *name;
  void *impl;
} runtime_fns[] = {
  { "pd_rt_deriv", (void *)&pd_deriv },
  { "pd_rt_discard", (void *)&pd_discard },
  { "pd_rt_sample_ex", (void *)&pd_sample_ex },
  { "pd_rt_ib_data", (void *)&pd_ib_data },
  { "pd_rt_ib_write", (void *)&pd_ib_write },
  { "pd_rt_ib_load", (void *)&pd_ib_load },
  { "pd_rt_ib_store", (void *)&pd_ib_store },
  { "pd_rt_tex_width", (void *)&pd_tex_width },
  { "pd_rt_tex_height", (void *)&pd_tex_height },
  { "pd_rt_read_tex", (void *)&pd_read_tex },
  { "pd_rt_write_tex", (void *)&pd_write_tex },
  { "pd_rt_quad_shuffle", (void *)&pd_quad_shuffle },
  { "pd_rt_barrier", (void *)&pd_barrier },
  { "pd_rt_lanes", (void *)&pd_lanes },
  { "pd_rt_read_tex_u", (void *)&pd_read_tex_u },
  { "pd_rt_write_tex_u", (void *)&pd_write_tex_u },
  { "pd_rt_sample_full", (void *)&pd_sample_full },
  { "pd_rt_read_tex_l", (void *)&pd_read_tex_l },
  { "pd_rt_write_tex_l", (void *)&pd_write_tex_l },
  { "pd_rt_read_tex_lu", (void *)&pd_read_tex_lu },
  { "pd_rt_write_tex_lu", (void *)&pd_write_tex_lu },
  { "pd_rt_tex_query", (void *)&pd_tex_query },
};

// a constexpr sampler's AIR state: an i64 (Xcode 13, bit 63 set) or [2 x i64] whose first word holds it
// (Xcode 26)
static uint64_t const_sampler_state(LLVMValueRef init) {
  if (!LLVMIsAConstantInt(init) && LLVMGetTypeKind(LLVMTypeOf(init)) == LLVMArrayTypeKind)
    init = LLVMGetAggregateElement(init, 0);
  return init && LLVMIsAConstantInt(init) ? LLVMConstIntGetZExtValue(init) : 0;
}

void air_const_samplers(LLVMModuleRef mod) {
  LLVMContextRef ctx = LLVMGetModuleContext(mod);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx), at = LLVMArrayType2(i32, 2);
  LLVMValueRef g, next;

  for (g = LLVMGetFirstGlobal(mod); g; g = next) {
    size_t l = 0;
    const char *n = LLVMGetValueName2(g, &l);
    LLVMValueRef init = LLVMGetInitializer(g), parts[2], ng;
    char name[64];

    next = LLVMGetNextGlobal(g);
    if (l < 19 || strncmp(n, "__air_sampler_state", 19) || !init)
      continue;

    parts[0] = LLVMConstInt(i32, PD_SAMPLER_MAGIC, 0);
    parts[1] = LLVMConstInt(i32, a2s_sampler_flags(const_sampler_state(init)), 0);
    snprintf(name, sizeof(name), "pd_sampler%.*s", (int)(l - 19), n + 19);
    ng = LLVMAddGlobalInAddressSpace(mod, at, name, LLVMGetPointerAddressSpace(LLVMTypeOf(g)));
    LLVMSetInitializer(ng, LLVMConstArray2(i32, parts, 2));
    LLVMSetGlobalConstant(ng, 1);
    LLVMSetLinkage(ng, LLVMInternalLinkage);
    LLVMSetAlignment(ng, 8);
    LLVMReplaceAllUsesWith(g, ng);
    LLVMDeleteGlobal(g);
  }
}

int air_runtime(unsigned i, char *name, size_t len, void **impl) {
  const struct air_math_op *m;

  if (i < PD_ARRAY_LEN(runtime_fns)) {
    snprintf(name, len, "%s", runtime_fns[i].name);
    *impl = runtime_fns[i].impl;
    return 1;
  }

  if (!(m = air_math_at(i - (unsigned)PD_ARRAY_LEN(runtime_fns))))
    return 0;

  snprintf(name, len, "pd_rt_math_%s_%u", m->op, m->arity);
  *impl = (void *)m->impl.f1;
  return 1;
}

// calls a runtime function by its name, or through a constant pointer when it has none, which keeps the
// stage out of the cache
static LLVMValueRef air_call(struct air_fn *a, void *impl, LLVMTypeRef ret, LLVMTypeRef *pts, LLVMValueRef *args,
                             unsigned n) {
  LLVMTypeRef fty = LLVMFunctionType(ret, pts, n, 0);
  LLVMValueRef fp = 0;
  char name[64];
  void *p;
  unsigned i;

  for (i = 0; air_runtime(i, name, sizeof(name), &p); i++) {
    if (p != impl)
      continue;

    if (!(fp = LLVMGetNamedFunction(a->mod, name)))
      fp = LLVMAddFunction(a->mod, name, fty);
    break;
  }

  if (!fp) {
    fp = LLVMConstIntToPtr(LLVMConstInt(LLVMInt64TypeInContext(a->ctx), (uint64_t)(uintptr_t)impl, 0), air_ptr(a));
    a->flags |= AIR_ABSOLUTE;
  }

  return LLVMBuildCall2(a->b, fty, fp, args, n, "");
}

// the overloaded llvm.* intrinsic on the result type
static LLVMValueRef air_intrinsic(struct air_fn *a, const char *name, LLVMValueRef *args, unsigned n) {
  unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
  LLVMValueRef decl = LLVMGetIntrinsicDeclaration(a->mod, id, &a->retty, 1);
  LLVMTypeRef fty = LLVMIntrinsicGetType(a->ctx, id, &a->retty, 1);

  return LLVMBuildCall2(a->b, fty, decl, args, n, "");
}

// an overloaded llvm.* intrinsic on type t
static LLVMValueRef air_intrinsic_on(struct air_fn *a, const char *name, LLVMTypeRef t, LLVMValueRef *args, unsigned n) {
  unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
  LLVMValueRef decl = LLVMGetIntrinsicDeclaration(a->mod, id, &t, 1);
  LLVMTypeRef fty = LLVMIntrinsicGetType(a->ctx, id, &t, 1);

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

// air.any/all.vNi1: true if any/every lane is set, as a test of the lanes bitcast to iN
static int air_any(struct air_fn *a) {
  LLVMValueRef in, bits, r;
  int all = a->op[1] == 'l';
  unsigned n;

  if (a->np != 1)
    return AIR_SKIP;

  air_begin(a);
  in = air_param(a, 0);
  n = lane_count(LLVMTypeOf(in));
  bits = n > 1 ? LLVMBuildBitCast(a->b, in, LLVMIntTypeInContext(a->ctx, n), "") : in;
  if (all)
    r = LLVMBuildICmp(a->b, LLVMIntEQ, bits, LLVMConstAllOnes(LLVMTypeOf(bits)), "");
  else
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

static int is_int_type(LLVMTypeRef t) {
  return LLVMGetTypeKind(elem_type(t)) == LLVMIntegerTypeKind;
}

// lanes of v (up to n) into a stack array of n elements of type et, the rest zero
static LLVMValueRef air_lanes_to_mem(struct air_fn *a, LLVMValueRef v, LLVMTypeRef et, unsigned n) {
  LLVMTypeRef at = LLVMArrayType2(et, n);
  LLVMValueRef mem = LLVMBuildAlloca(a->b, at, "");
  unsigned k, nl = v ? lane_count(LLVMTypeOf(v)) : 0;

  LLVMBuildStore(a->b, LLVMConstNull(at), mem);
  for (k = 0; k < nl && k < n; k++) {
    LLVMValueRef e = air_lane(a, v, k), idx[2] = { air_int(a, 0), air_int(a, k) };

    if (et == air_f32(a))
      e = air_to_f32(a, e);
    else
      e = LLVMBuildSExtOrBitCast(a->b, e, et, "");
    LLVMBuildStore(a->b, e, LLVMBuildGEP2(a->b, at, mem, idx, 2, ""));
  }
  return mem;
}

// air.sample_texture_<type>[_grad].<ty>(tex, sampler, coord[, slice], [gradients,] i1, offset, i1, lod, min lod,
// i32): a float right after an i1 is the lod (or bias), any other the min lod
static int air_sample(struct air_fn *a) {
  const char *ty = a->op + 15;
  int array = strstr(ty, "_array") != 0, three = !strncmp(ty, "3d", 2), grad = strstr(ty, "_grad") != 0;
  LLVMTypeRef f32 = air_f32(a), ptr = air_ptr(a), i32 = air_i32(a), v4f = LLVMVectorType(f32, 4);
  LLVMTypeRef pts[9] = { ptr, ptr, ptr, i32, ptr, i32, f32, ptr, ptr };
  LLVMValueRef args[9], out, off = 0, gx = 0, gy = 0, given = 0, lod = 0, mode;
  unsigned k = 3, prev_i1 = 0;

  if (a->np < 3)
    return AIR_SKIP;

  air_begin(a);
  out = LLVMBuildAlloca(a->b, v4f, "out");
  args[0] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), ptr, "");
  args[1] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 1), ptr, "");
  args[2] = air_lanes_to_mem(a, air_param(a, 2), f32, 3);
  args[3] = array && k < a->np ? air_as_i32(a, air_param(a, k++)) : air_int(a, 0);

  for (; k < a->np; k++) {
    LLVMValueRef p = air_param(a, k);
    LLVMTypeRef t = LLVMTypeOf(p);
    int i1 = LLVMGetTypeKind(t) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(t) == 1;

    if (is_vector(t) && is_int_type(t)) {
      off = p;
    } else if (is_vector(t)) {
      if (!gx)
        gx = p;
      else
        gy = p;
    } else if (!is_int_type(t) && prev_i1 && !lod) {
      lod = p, given = air_param(a, k - 1);
    }
    prev_i1 = i1;
  }

  args[4] = air_lanes_to_mem(a, off, i32, 3);
  if (grad) {
    LLVMTypeRef gt = LLVMArrayType2(f32, 6);
    LLVMValueRef g = LLVMBuildAlloca(a->b, gt, ""), gs[2] = { gx, gy };
    unsigned s, l;

    LLVMBuildStore(a->b, LLVMConstNull(gt), g);
    for (s = 0; s < 2; s++) {
      for (l = 0; gs[s] && l < lane_count(LLVMTypeOf(gs[s])) && l < 3; l++) {
        LLVMValueRef idx[2] = { air_int(a, 0), air_int(a, s * 3 + l) };
        LLVMBuildStore(a->b, air_to_f32(a, air_lane(a, gs[s], l)), LLVMBuildGEP2(a->b, gt, g, idx, 2, ""));
      }
    }
    args[7] = g;
    mode = air_int(a, PD_LOD_GRAD);
  } else {
    args[7] = LLVMConstNull(ptr);
    // no lod given: the implicit lod plus the value as a bias
    mode = given ? LLVMBuildSelect(a->b, given, air_int(a, PD_LOD_LEVEL), air_int(a, PD_LOD_BIAS), "") : air_int(a, PD_LOD_BIAS);
    if (!given || !LLVMIsAConstantInt(given) || !LLVMConstIntGetZExtValue(given))
      a->flags |= AIR_USES_LOD;
  }
  // a 3D texture the request gave no layout answers its coordinate, the lookup tables it holds
  if (three)
    mode = LLVMBuildOr(a->b, mode, air_int(a, PD_LOD_3D_COMPAT), "");
  args[5] = mode;
  args[6] = lod ? air_to_f32(a, lod) : const_fp(f32, 0.0);
  args[8] = out;
  air_call(a, (void *)&pd_sample_full, air_void(a), pts, args, 9);
  return air_return_texel(a, air_narrow_half4(a, LLVMBuildLoad2(a->b, v4f, out, "")));
}

// a memory barrier has nothing to order here, a threadgroup barrier hands the thread back to the group's
// scheduler until every thread has reached it (compute.c runs such groups as fibers)
static int air_barrier(struct air_fn *a) {
  if (LLVMGetTypeKind(a->retty) != LLVMVoidTypeKind)
    return AIR_SKIP;

  air_begin(a);
  if (!strncmp(a->op, "wg.barrier", 10)) {
    a->flags |= AIR_USES_BARRIER;
    air_call(a, (void *)&pd_barrier, air_void(a), NULL, NULL, 0);
  }
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

// air.get_<width|height|depth|num_mip_levels|array_size>_texture_<type>(tex[, lod])
static int air_texture_size(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), pts[3] = { air_ptr(a), i32, i32 };
  unsigned what = a->op[4] == 'w' ? 0 : a->op[4] == 'h' ? 1 : a->op[4] == 'd' ? 2 : a->op[4] == 'n' ? 3 : 4;
  LLVMValueRef args[3], r;

  if (a->np < 1)
    return AIR_SKIP;

  air_begin(a);
  args[0] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), air_ptr(a), "");
  args[1] = air_int(a, what);
  args[2] = a->np > 1 && is_int_type(LLVMTypeOf(air_param(a, 1))) ? air_as_i32(a, air_param(a, 1)) : air_int(a, 0);
  r = air_call(a, (void *)&pd_tex_query, i32, pts, args, 3);

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

// air.read_texture_<type>.*(tex, coord[, slice or face], lod, ...) and air.write_texture_<type>.*(tex, coord[,
// slice or face], colour, lod, ...): a cube array takes the face, then the slice
static int air_texture_rw(struct air_fn *a) {
  int write = a->op[0] == 'w';
  const char *ty = a->op + (write ? 14 : 13);
  int cube = !strncmp(ty, "cube", 4), array = strstr(ty, "_array") != 0, three = !strncmp(ty, "3d", 2);
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), v4f = LLVMVectorType(air_f32(a), 4), v4i = LLVMVectorType(i32, 4);
  LLVMTypeRef pts[6] = { ptr, i32, i32, i32, i32, ptr }, vt;
  LLVMValueRef args[6], coord, buf, col = 0, vec;
  unsigned k = 2, n, c;

  if (a->np < 2)
    return AIR_SKIP;

  air_begin(a);
  coord = air_param(a, 1);
  args[0] = LLVMBuildAddrSpaceCast(a->b, air_param(a, 0), ptr, "");
  args[1] = LLVMBuildZExtOrBitCast(a->b, air_lane(a, coord, 0), i32, "");
  args[2] = LLVMBuildZExtOrBitCast(a->b, air_lane(a, coord, 1), i32, "");
  args[3] = three && lane_count(LLVMTypeOf(coord)) > 2 ? LLVMBuildZExtOrBitCast(a->b, air_lane(a, coord, 2), i32, "") : air_int(a, 0);
  if ((cube || array) && k < a->np)
    args[3] = air_as_i32(a, air_param(a, k++));
  if (cube && array && k < a->np)
    args[3] = LLVMBuildAdd(a->b, LLVMBuildMul(a->b, air_as_i32(a, air_param(a, k++)), air_int(a, 6), ""), args[3], "");
  if (write && k < a->np)
    col = air_param(a, k++);
  args[4] = k < a->np && is_int_type(LLVMTypeOf(air_param(a, k))) && !is_vector(LLVMTypeOf(air_param(a, k)))
              ? air_as_i32(a, air_param(a, k))
              : air_int(a, 0);

  vt = write ? (col ? LLVMTypeOf(col) : v4f) : is_struct(a->retty) ? LLVMStructGetTypeAtIndex(a->retty, 0) : a->retty;
  n = lane_count(vt);

  // integer texels keep their bits: uint4 through the integer runtime calls
  if (LLVMGetTypeKind(elem_type(vt)) == LLVMIntegerTypeKind) {
    buf = LLVMBuildAlloca(a->b, v4i, "px");
    args[5] = buf;
    if (write) {
      vec = LLVMConstNull(v4i);
      for (c = 0; c < n && c < 4; c++) {
        vec = LLVMBuildInsertElement(a->b, vec, LLVMBuildZExtOrBitCast(a->b, air_lane(a, col, c), i32, ""), air_int(a, c), "");
      }
      LLVMBuildStore(a->b, vec, buf);
      air_call(a, (void *)&pd_write_tex_lu, air_void(a), pts, args, 6);
      return air_return(a, NULL);
    }
    air_call(a, (void *)&pd_read_tex_lu, air_void(a), pts, args, 6);
    vec = LLVMBuildLoad2(a->b, v4i, buf, "");
    {
      LLVMValueRef r = n > 1 ? LLVMGetUndef(vt) : NULL;
      for (c = 0; c < n; c++) {
        LLVMValueRef e = LLVMBuildTruncOrBitCast(a->b, LLVMBuildExtractElement(a->b, vec, air_int(a, c < 4 ? c : 3), ""), elem_type(vt), "");
        r = air_set_lane(a, r, e, c);
      }
      return air_return_texel(a, r);
    }
  }

  buf = LLVMBuildAlloca(a->b, v4f, "px");
  args[5] = buf;

  if (write) {
    LLVMBuildStore(a->b, air_texel_in(a, col), buf);
    air_call(a, (void *)&pd_write_tex_l, air_void(a), pts, args, 6);
    return air_return(a, NULL);
  }

  air_call(a, (void *)&pd_read_tex_l, air_void(a), pts, args, 6);
  return air_return_texel(a, air_texel_out(a, LLVMBuildLoad2(a->b, v4f, buf, "")));
}

// air.load.implicit_imageblock.<ty>(i32 render target, <2 x i16> pixel in the tile, i32, i16): the
// pass's colour attachment at that pixel
static int air_ib_load(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), v4f = LLVMVectorType(air_f32(a), 4);
  LLVMTypeRef pts[3] = { i32, i32, ptr };
  LLVMValueRef args[3], buf;

  if (a->np < 2)
    return AIR_SKIP;

  air_begin(a);
  buf = LLVMBuildAlloca(a->b, v4f, "px");
  args[0] = air_as_i32(a, air_param(a, 0));
  args[1] = air_as_i32(a, air_param(a, 1));
  args[2] = buf;
  air_call(a, (void *)&pd_ib_load, air_void(a), pts, args, 3);
  return air_return(a, air_texel_out(a, LLVMBuildLoad2(a->b, v4f, buf, "")));
}

// air.store.implicit_imageblock.<ty>(value, i32 render target, <2 x i16> pixel, i32, i16)
static int air_ib_store(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), v4f = LLVMVectorType(air_f32(a), 4);
  LLVMTypeRef pts[3] = { i32, i32, ptr };
  LLVMValueRef args[3], buf;

  if (a->np < 3)
    return AIR_SKIP;

  air_begin(a);
  buf = LLVMBuildAlloca(a->b, v4f, "px");
  LLVMBuildStore(a->b, air_texel_in(a, air_param(a, 0)), buf);
  args[0] = air_as_i32(a, air_param(a, 1));
  args[1] = air_as_i32(a, air_param(a, 2));
  args[2] = buf;
  air_call(a, (void *)&pd_ib_store, air_void(a), pts, args, 3);
  return air_return(a, NULL);
}

// air.quad_shuffle[_xor|_up|_down].<ty> and air.quad_broadcast.<ty>(value, lane or mask): the value
// goes through memory so any scalar or vector type works
static int air_quad(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), vt;
  LLVMTypeRef pts[4] = { ptr, i32, i32, i32 };
  LLVMValueRef args[4], buf;
  unsigned mode = PD_QUAD_INDEX, size;

  if (a->np != 2)
    return AIR_SKIP;

  if (!strncmp(a->op, "quad_shuffle_xor.", 17))
    mode = PD_QUAD_XOR;
  else if (!strncmp(a->op, "quad_shuffle_up.", 16))
    mode = PD_QUAD_UP;
  else if (!strncmp(a->op, "quad_shuffle_down.", 18))
    mode = PD_QUAD_DOWN;

  vt = LLVMTypeOf(air_param(a, 0));
  size = (lane_count(vt) * type_bits(elem_type(vt)) + 7) / 8;
  air_begin(a);
  // fragments exchange through the raster's quad passes, compute groups through their fibers
  a->flags |= AIR_USES_QUAD | AIR_USES_BARRIER;
  buf = LLVMBuildAlloca(a->b, vt, "v");
  LLVMBuildStore(a->b, air_param(a, 0), buf);
  args[0] = buf;
  args[1] = air_int(a, size);
  args[2] = air_int(a, mode);
  args[3] = air_as_i32(a, air_param(a, 1));
  air_call(a, (void *)&pd_quad_shuffle, air_void(a), pts, args, 4);
  return air_return(a, LLVMBuildLoad2(a->b, vt, buf, ""));
}


// widened to twice the bits: for mul_hi
static LLVMTypeRef wide_int(struct air_fn *a, LLVMTypeRef t) {
  LLVMTypeRef e = LLVMIntTypeInContext(a->ctx, 2 * LLVMGetIntTypeWidth(elem_type(t)));

  return is_vector(t) ? LLVMVectorType(e, LLVMGetVectorSize(t)) : e;
}

// a scalar in every lane of an n lane vector
static LLVMValueRef splat_val(struct air_fn *a, unsigned n, LLVMValueRef v) {
  LLVMTypeRef vt = LLVMVectorType(LLVMTypeOf(v), n);
  LLVMValueRef one = LLVMBuildInsertElement(a->b, LLVMGetUndef(vt), v, air_int(a, 0), "");

  return LLVMBuildShuffleVector(a->b, one, LLVMGetUndef(vt), LLVMConstNull(LLVMVectorType(air_i32(a), n)), "");
}

static LLVMValueRef splat_int(struct air_fn *a, LLVMTypeRef t, unsigned long long v) {
  LLVMValueRef e[16], c = LLVMConstInt(elem_type(t), v, 0);
  unsigned n = lane_count(t);

  if (!is_vector(t))
    return c;
  for (unsigned i = 0; i < n && i < 16; i++) {
    e[i] = c;
  }
  return LLVMConstVector(e, n);
}

// the integer operations Metal's integer functions lower to: mul_hi, clz, ctz, popcount, rotate,
// abs_diff, add_sat, sub_sat, extract_bits, insert_bits, reverse_bits
static int air_int_bits(struct air_fn *a) {
  const char *op = a->op;
  int sgn = strstr(op, ".s.") != NULL;
  LLVMTypeRef t = a->retty;
  LLVMValueRef x, y, r, args[3];
  unsigned bits = LLVMGetTypeKind(elem_type(t)) == LLVMIntegerTypeKind ? LLVMGetIntTypeWidth(elem_type(t)) : 0;

  if (!bits)
    return AIR_SKIP;

  air_begin(a);
  x = air_param(a, 0);

  if (!strncmp(op, "mul_hi.", 7)) {
    LLVMTypeRef w = wide_int(a, t);
    LLVMValueRef xw = sgn ? LLVMBuildSExt(a->b, x, w, "") : LLVMBuildZExt(a->b, x, w, "");
    LLVMValueRef yw = sgn ? LLVMBuildSExt(a->b, air_param(a, 1), w, "") : LLVMBuildZExt(a->b, air_param(a, 1), w, "");
    r = LLVMBuildLShr(a->b, LLVMBuildMul(a->b, xw, yw, ""), splat_int(a, w, bits), "");
    r = LLVMBuildTrunc(a->b, r, t, "");
  } else if (!strncmp(op, "clz.", 4) || !strncmp(op, "ctz.", 4)) {
    args[0] = x, args[1] = LLVMConstInt(LLVMInt1TypeInContext(a->ctx), 0, 0);
    r = air_intrinsic(a, op[1] == 'l' ? "llvm.ctlz" : "llvm.cttz", args, 2);
  } else if (!strncmp(op, "popcount.", 9)) {
    r = air_intrinsic(a, "llvm.ctpop", &x, 1);
  } else if (!strncmp(op, "reverse_bits.", 13)) {
    r = air_intrinsic(a, "llvm.bitreverse", &x, 1);
  } else if (!strncmp(op, "rotate.", 7)) {
    args[0] = x, args[1] = x, args[2] = air_param(a, 1);
    r = air_intrinsic(a, "llvm.fshl", args, 3);
  } else if (!strncmp(op, "abs_diff.", 9)) {
    y = air_param(a, 1);
    LLVMValueRef gt = LLVMBuildICmp(a->b, sgn ? LLVMIntSGT : LLVMIntUGT, x, y, "");
    r = LLVMBuildSelect(a->b, gt, LLVMBuildSub(a->b, x, y, ""), LLVMBuildSub(a->b, y, x, ""), "");
  } else if (!strncmp(op, "add_sat.", 8) || !strncmp(op, "sub_sat.", 8)) {
    static const char *const names[2][2] = { { "llvm.uadd.sat", "llvm.usub.sat" }, { "llvm.sadd.sat", "llvm.ssub.sat" } };
    args[0] = x, args[1] = air_param(a, 1);
    r = air_intrinsic(a, names[sgn][op[0] == 's'], args, 2);
  } else if (!strncmp(op, "extract_bits.", 13)) {
    // count bits from offset: left aligned, then shifted back down with the sign or zeros
    LLVMValueRef off = air_param(a, 1), cnt = air_param(a, 2), top, sh;
    LLVMTypeRef et = elem_type(t);
    off = LLVMBuildZExtOrBitCast(a->b, LLVMBuildTruncOrBitCast(a->b, off, et, ""), et, "");
    cnt = LLVMBuildZExtOrBitCast(a->b, LLVMBuildTruncOrBitCast(a->b, cnt, et, ""), et, "");
    if (is_vector(t)) {
      off = splat_val(a, lane_count(t), off);
      cnt = splat_val(a, lane_count(t), cnt);
    }
    top = LLVMBuildSub(a->b, splat_int(a, t, bits), LLVMBuildAdd(a->b, off, cnt, ""), "");
    sh = LLVMBuildSub(a->b, splat_int(a, t, bits), cnt, "");
    r = LLVMBuildShl(a->b, x, top, "");
    r = sgn ? LLVMBuildAShr(a->b, r, sh, "") : LLVMBuildLShr(a->b, r, sh, "");
    // a count of 0 yields 0, a shift by the full width would be poison
    r = LLVMBuildSelect(a->b, LLVMBuildICmp(a->b, LLVMIntEQ, cnt, splat_int(a, t, 0), ""), splat_int(a, t, 0), r, "");
  } else if (!strncmp(op, "insert_bits.", 12)) {
    LLVMValueRef ins = air_param(a, 1), off = air_param(a, 2), cnt = air_param(a, 3), mask;
    LLVMTypeRef et = elem_type(t);
    off = LLVMBuildZExtOrBitCast(a->b, LLVMBuildTruncOrBitCast(a->b, off, et, ""), et, "");
    cnt = LLVMBuildZExtOrBitCast(a->b, LLVMBuildTruncOrBitCast(a->b, cnt, et, ""), et, "");
    if (is_vector(t)) {
      off = splat_val(a, lane_count(t), off);
      cnt = splat_val(a, lane_count(t), cnt);
    }
    // ((1 << cnt) - 1) without the shift by the full width: all ones shifted right
    mask = LLVMBuildLShr(a->b, splat_int(a, t, ~0ull), LLVMBuildSub(a->b, splat_int(a, t, bits), cnt, ""), "");
    mask = LLVMBuildSelect(a->b, LLVMBuildICmp(a->b, LLVMIntEQ, cnt, splat_int(a, t, 0), ""), splat_int(a, t, 0), mask, "");
    mask = LLVMBuildShl(a->b, mask, off, "");
    r = LLVMBuildOr(a->b, LLVMBuildAnd(a->b, x, LLVMBuildNot(a->b, mask, ""), ""),
                    LLVMBuildAnd(a->b, LLVMBuildShl(a->b, ins, off, ""), mask, ""), "");
  } else {
    LLVMDisposeBuilder(a->b);
    return AIR_SKIP;
  }

  return air_return(a, r);
}

// air.pack.<unorm|snorm><4x8|2x16>.<ty>: each lane clamped, scaled, rounded to even and packed low first
static int air_pack(struct air_fn *a) {
  const char *q = a->op + 5;
  int snorm = !strncmp(q, "snorm", 5);
  unsigned n = q[5] == '4' ? 4 : 2, bits = n == 4 ? 8 : 16, k;
  LLVMTypeRef f32 = air_f32(a), i32 = air_i32(a);
  LLVMValueRef v, r = LLVMConstInt(i32, 0, 0);

  if (a->np != 1 || (strncmp(q, "unorm", 5) && !snorm))
    return AIR_SKIP;

  air_begin(a);
  v = air_param(a, 0);
  for (k = 0; k < n; k++) {
    LLVMValueRef e = air_to_f32(a, air_lane(a, v, k)), lo = const_fp(f32, snorm ? -1.0 : 0.0), hi = const_fp(f32, 1.0);
    LLVMValueRef lane;
    e = air_fselect(a, LLVMRealOLT, e, lo, lo, e);
    e = air_fselect(a, LLVMRealOGT, e, hi, hi, e);
    e = LLVMBuildFMul(a->b, e, const_fp(f32, snorm ? (double)((1u << (bits - 1)) - 1) : (double)((1u << bits) - 1)), "");
    e = air_intrinsic_on(a, "llvm.rint", f32, &e, 1);
    lane = LLVMBuildFPToSI(a->b, e, i32, "");
    lane = LLVMBuildAnd(a->b, lane, LLVMConstInt(i32, (1u << bits) - 1, 0), "");
    r = LLVMBuildOr(a->b, r, LLVMBuildShl(a->b, lane, LLVMConstInt(i32, k * bits, 0), ""), "");
  }

  if (LLVMGetIntTypeWidth(a->retty) != 32)
    r = LLVMBuildTruncOrBitCast(a->b, r, a->retty, "");
  return air_return(a, r);
}

// air.atomic.<global|local>.<op>[.s|.u].<ty>(ptr, [value,] order, scope, volatile): real atomics, since a
// dispatch's groups run on several cores at once
static int air_atomic(struct air_fn *a) {
  const char *q = strchr(a->op, '.') + 1, *opn = strchr(q, '.');
  int sgn, flt = 0;
  char op[16];
  size_t ol;
  LLVMValueRef p, v, r;
  LLVMAtomicRMWBinOp bop;

  if (!opn)
    return AIR_SKIP;
  opn++;
  ol = strcspn(opn, ".");
  if (ol >= sizeof(op))
    return AIR_SKIP;
  memcpy(op, opn, ol);
  op[ol] = 0;
  sgn = strstr(opn, ".s.") != NULL;
  flt = strstr(opn, ".f32") != NULL || strstr(opn, ".f16") != NULL;

  air_begin(a);
  p = air_param(a, 0);

  if (!strcmp(op, "load")) {
    r = LLVMBuildLoad2(a->b, a->retty, p, "");
    LLVMSetOrdering(r, LLVMAtomicOrderingSequentiallyConsistent);
    LLVMSetAlignment(r, LLVMGetIntTypeWidth(elem_type(a->retty)) / 8 ? LLVMGetIntTypeWidth(elem_type(a->retty)) / 8 : 4);
    return air_return(a, r);
  }

  if (!strcmp(op, "store")) {
    LLVMValueRef st = LLVMBuildStore(a->b, air_param(a, 1), p);
    LLVMSetOrdering(st, LLVMAtomicOrderingSequentiallyConsistent);
    LLVMSetAlignment(st, 4);
    return air_return(a, NULL);
  }

  if (!strcmp(op, "cmpxchg")) {
    // (ptr, ptr expected, desired, ...) -> i1: the old value goes back to *expected
    LLVMValueRef ep = air_param(a, 1), exp, des = air_param(a, 2), cx, old, ok;
    LLVMTypeRef vt = LLVMTypeOf(des);
    exp = LLVMBuildLoad2(a->b, vt, ep, "");
    cx = LLVMBuildAtomicCmpXchg(a->b, p, exp, des, LLVMAtomicOrderingSequentiallyConsistent,
                                LLVMAtomicOrderingSequentiallyConsistent, 0);
    old = LLVMBuildExtractValue(a->b, cx, 0, ""), ok = LLVMBuildExtractValue(a->b, cx, 1, "");
    LLVMBuildStore(a->b, old, ep);
    if (LLVMGetTypeKind(a->retty) == LLVMVoidTypeKind)
      return air_return(a, NULL);
    return air_return(a, LLVMBuildZExtOrBitCast(a->b, ok, a->retty, ""));
  }

  if (!strcmp(op, "add"))
    bop = flt ? LLVMAtomicRMWBinOpFAdd : LLVMAtomicRMWBinOpAdd;
  else if (!strcmp(op, "sub"))
    bop = flt ? LLVMAtomicRMWBinOpFSub : LLVMAtomicRMWBinOpSub;
  else if (!strcmp(op, "min"))
    bop = flt ? LLVMAtomicRMWBinOpFMin : sgn ? LLVMAtomicRMWBinOpMin : LLVMAtomicRMWBinOpUMin;
  else if (!strcmp(op, "max"))
    bop = flt ? LLVMAtomicRMWBinOpFMax : sgn ? LLVMAtomicRMWBinOpMax : LLVMAtomicRMWBinOpUMax;
  else if (!strcmp(op, "and"))
    bop = LLVMAtomicRMWBinOpAnd;
  else if (!strcmp(op, "or"))
    bop = LLVMAtomicRMWBinOpOr;
  else if (!strcmp(op, "xor"))
    bop = LLVMAtomicRMWBinOpXor;
  else if (!strcmp(op, "xchg"))
    bop = LLVMAtomicRMWBinOpXchg;
  else {
    LLVMDisposeBuilder(a->b);
    return -1;
  }

  v = air_param(a, 1);
  r = LLVMBuildAtomicRMW(a->b, bop, p, v, LLVMAtomicOrderingSequentiallyConsistent, 0);
  return air_return(a, LLVMGetTypeKind(a->retty) == LLVMVoidTypeKind ? NULL : r);
}

// simdgroup operations, exchanged through memory between the fibers of the group (compute.c), 32 lanes
static const struct {
  const char *name;
  unsigned op;
  int arg;
} simd_ops[] = {
  { "simd_shuffle_xor.", PD_LANES_XOR, 1 },        { "simd_shuffle_up.", PD_LANES_UP, 1 },
  { "simd_shuffle_down.", PD_LANES_DOWN, 1 },      { "simd_shuffle.", PD_LANES_INDEX, 1 },
  { "simd_broadcast_first.", PD_LANES_FIRST, 0 },  { "simd_broadcast.", PD_LANES_INDEX, 1 },
  { "simd_sum.", PD_LANES_SUM, 0 },                { "simd_product.", PD_LANES_PRODUCT, 0 },
  { "simd_min.", PD_LANES_MIN, 0 },                { "simd_max.", PD_LANES_MAX, 0 },
  { "simd_and.", PD_LANES_AND, 0 },                { "simd_or.", PD_LANES_OR, 0 },
  { "simd_xor.", PD_LANES_XOR_ALL, 0 },            { "simd_prefix_exclusive_sum.", PD_LANES_PREFIX_EX_SUM, 0 },
  { "simd_prefix_inclusive_sum.", PD_LANES_PREFIX_IN_SUM, 0 },
  { "simd_prefix_exclusive_product.", PD_LANES_PREFIX_EX_PRODUCT, 0 },
  { "simd_prefix_inclusive_product.", PD_LANES_PREFIX_IN_PRODUCT, 0 },
  { "simd_all", PD_LANES_ALL, 0 },                 { "simd_any", PD_LANES_ANY, 0 },
};

static int air_simd(struct air_fn *a) {
  LLVMTypeRef i32 = air_i32(a), ptr = air_ptr(a), vt;
  LLVMTypeRef pts[7] = { i32, i32, i32, i32, ptr, ptr, i32 };
  LLVMValueRef args[7], in, out, v;
  unsigned i, kind;

  for (i = 0; i < PD_ARRAY_LEN(simd_ops); i++) {
    if (!strncmp(a->op, simd_ops[i].name, strlen(simd_ops[i].name)))
      break;
  }
  if (i == PD_ARRAY_LEN(simd_ops) || a->np < 1)
    return AIR_SKIP;

  v = air_param(a, 0);
  vt = LLVMTypeOf(v);
  kind = LLVMGetTypeKind(elem_type(vt)) == LLVMFloatTypeKind  ? PD_LANES_F32
         : LLVMGetTypeKind(elem_type(vt)) == LLVMHalfTypeKind ? PD_LANES_F16
         : strstr(a->op, ".s.") ? (type_bits(elem_type(vt)) == 16 ? PD_LANES_S16 : PD_LANES_S32)
         : type_bits(elem_type(vt)) == 16 ? PD_LANES_U16 : type_bits(elem_type(vt)) == 1 ? PD_LANES_BOOL : PD_LANES_U32;

  air_begin(a);
  a->flags |= AIR_USES_BARRIER;
  in = LLVMBuildAlloca(a->b, vt, "in");
  out = LLVMBuildAlloca(a->b, LLVMGetTypeKind(a->retty) == LLVMVoidTypeKind ? vt : a->retty, "out");
  if (type_bits(elem_type(vt)) == 1) {
    // a bool travels as one byte per lane
    LLVMTypeRef bt = LLVMInt8TypeInContext(a->ctx);
    in = LLVMBuildAlloca(a->b, bt, "b");
    LLVMBuildStore(a->b, LLVMBuildZExt(a->b, v, bt, ""), in);
    out = LLVMBuildAlloca(a->b, bt, "o");
  } else {
    LLVMBuildStore(a->b, v, in);
  }
  args[0] = air_int(a, simd_ops[i].op);
  args[1] = air_int(a, 32);
  args[2] = air_int(a, kind);
  args[3] = air_int(a, lane_count(vt));
  args[4] = in, args[5] = out;
  args[6] = simd_ops[i].arg && a->np > 1 ? air_as_i32(a, air_param(a, 1)) : air_int(a, 0);
  air_call(a, (void *)&pd_lanes, air_void(a), pts, args, 7);
  if (type_bits(elem_type(vt)) == 1)
    return air_return(a, LLVMBuildTrunc(a->b, LLVMBuildLoad2(a->b, LLVMInt8TypeInContext(a->ctx), out, ""), a->retty, ""));
  return air_return(a, LLVMBuildLoad2(a->b, a->retty, out, ""));
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
  { "all.v", 0, air_any },
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
  { "sample_texture_", 0, air_sample },
  { "wg.barrier", 0, air_barrier },
  { "mem_barrier", 0, air_barrier },
  { "simdgroup.barrier", 0, air_barrier },
  { "imageblock_data", 1, air_imageblock_data },
  { "write_imageblock_slice_to_texture_2d.", 0, air_imageblock_write },
  { "get_width_texture_", 0, air_texture_size },
  { "get_height_texture_", 0, air_texture_size },
  { "get_depth_texture_", 0, air_texture_size },
  { "get_num_mip_levels_texture_", 0, air_texture_size },
  { "get_array_size_texture_", 0, air_texture_size },
  { "read_texture_", 0, air_texture_rw },
  { "write_texture_", 0, air_texture_rw },
  { "load.implicit_imageblock.", 0, air_ib_load },
  { "store.implicit_imageblock.", 0, air_ib_store },
  { "quad_shuffle", 0, air_quad },
  { "quad_broadcast.", 0, air_quad },
  { "mul_hi.", 0, air_int_bits },
  { "clz.", 0, air_int_bits },
  { "ctz.", 0, air_int_bits },
  { "popcount.", 0, air_int_bits },
  { "reverse_bits.", 0, air_int_bits },
  { "rotate.", 0, air_int_bits },
  { "abs_diff.", 0, air_int_bits },
  { "add_sat.", 0, air_int_bits },
  { "sub_sat.", 0, air_int_bits },
  { "extract_bits.", 0, air_int_bits },
  { "insert_bits.", 0, air_int_bits },
  { "pack.", 0, air_pack },
  { "atomic.", 0, air_atomic },
  { "simd_", 0, air_simd },
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
