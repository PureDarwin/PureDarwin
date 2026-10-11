#include <spirv/unified1/GLSL.std.450.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a2s.h"

void a2s_fail(struct a2s *a, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(a->err, a->errlen, fmt, ap);
  va_end(ap);
  longjmp(a->fail, 1);
}

void a2s_note(struct a2s *a, const char *fmt, ...) {
  size_t n = strlen(a->r->notes);
  va_list ap;

  if (n + 2 >= sizeof(a->r->notes))
    return;

  if (n)
    a->r->notes[n++] = ';', a->r->notes[n] = 0;

  va_start(ap, fmt);
  vsnprintf(a->r->notes + n, sizeof(a->r->notes) - n, fmt, ap);
  va_end(ap);
}

struct vslot *a2s_slot(struct a2s *a, LLVMValueRef k, int create) {
  size_t i;

  if (create && (a->vt_n + 1) * 2 > a->vt_cap) {
    struct vslot *old = a->vt;
    size_t oc = a->vt_cap;

    a->vt_cap = oc ? oc * 2 : 4096;
    a->vt = calloc(a->vt_cap, sizeof(*a->vt));
    for (i = 0; i < oc; i++) {
      if (old[i].k) {
        size_t j = ((uintptr_t)old[i].k >> 4) & (a->vt_cap - 1);
        while (a->vt[j].k) {
          j = (j + 1) & (a->vt_cap - 1);
        }
        a->vt[j] = old[i];
      }
    }
    free(old);
  }

  if (!a->vt_cap)
    return 0;

  for (i = ((uintptr_t)k >> 4) & (a->vt_cap - 1); a->vt[i].k; i = (i + 1) & (a->vt_cap - 1)) {
    if (a->vt[i].k == k)
      return &a->vt[i];
  }

  if (!create)
    return 0;

  a->vt[i].k = k;
  a->vt_n++;
  return &a->vt[i];
}

void a2s_set(struct a2s *a, LLVMValueRef v, uint32_t id) {
  struct vslot *s = a2s_slot(a, v, 1);

  s->kind = VK_ID, s->id = id;
}

int a2s_bits(LLVMTypeRef t) {
  switch (LLVMGetTypeKind(t)) {
  case LLVMIntegerTypeKind:
    return (int)LLVMGetIntTypeWidth(t);
  case LLVMHalfTypeKind:
    return 16;
  case LLVMFloatTypeKind:
    return 32;
  case LLVMDoubleTypeKind:
    return 64;
  case LLVMVectorTypeKind:
    return a2s_bits(LLVMGetElementType(t));
  default:
    return 0;
  }
}

LLVMTypeRef a2s_scalar(LLVMTypeRef t) {
  return LLVMGetTypeKind(t) == LLVMVectorTypeKind ? LLVMGetElementType(t) : t;
}

unsigned a2s_lanes(LLVMTypeRef t) {
  return LLVMGetTypeKind(t) == LLVMVectorTypeKind ? LLVMGetVectorSize(t) : 1;
}

static int is_float(LLVMTypeRef t) {
  LLVMTypeKind k = LLVMGetTypeKind(a2s_scalar(t));

  return k == LLVMHalfTypeKind || k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind;
}

static int is_bool(LLVMTypeRef t) {
  LLVMTypeRef e = a2s_scalar(t);

  return LLVMGetTypeKind(e) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(e) == 1;
}

// half and double become float, every integer wider than i1 a 32 bit uint
uint32_t a2s_type(struct a2s *a, LLVMTypeRef t) {
  uint32_t m[64];
  unsigned n, i;

  switch (LLVMGetTypeKind(t)) {
  case LLVMVoidTypeKind:
    return spv_t_void(&a->s);
  case LLVMHalfTypeKind:
  case LLVMFloatTypeKind:
    return spv_t_float(&a->s);
  case LLVMDoubleTypeKind:
    a2s_note(a, "double as float");
    return spv_t_float(&a->s);
  case LLVMIntegerTypeKind:
    return LLVMGetIntTypeWidth(t) == 1 ? spv_t_bool(&a->s) : spv_t_uint(&a->s);
  case LLVMVectorTypeKind:
    n = LLVMGetVectorSize(t);
    if (n > 4)
      a2s_fail(a, "vector of %u lanes", n);
    return spv_t_vec(&a->s, a2s_type(a, LLVMGetElementType(t)), n);
  case LLVMStructTypeKind:
    n = LLVMCountStructElementTypes(t);
    if (n > 64)
      a2s_fail(a, "struct of %u members", n);
    for (i = 0; i < n; i++) {
      m[i] = a2s_type(a, LLVMStructGetTypeAtIndex(t, i));
    }
    return spv_t_struct(&a->s, m, n);
  case LLVMArrayTypeKind:
    return spv_t_array(&a->s, a2s_type(a, LLVMGetElementType(t)), spv_c_u32(&a->s, (uint32_t)LLVMGetArrayLength2(t)));
  default:
    a2s_fail(a, "no SPIR-V type for LLVM type kind %d", (int)LLVMGetTypeKind(t));
  }
}

uint32_t a2s_op(struct a2s *a, unsigned op, uint32_t type, const uint32_t *ops, unsigned n) {
  uint32_t w[40], id = spv_id(&a->s);

  w[0] = type, w[1] = id;
  memcpy(w + 2, ops, n * 4);
  spv_emit(&a->body, op, w, n + 2);
  return id;
}

uint32_t a2s_op1(struct a2s *a, unsigned op, uint32_t type, uint32_t x) {
  return a2s_op(a, op, type, &x, 1);
}

uint32_t a2s_op2(struct a2s *a, unsigned op, uint32_t type, uint32_t x, uint32_t y) {
  uint32_t w[2] = { x, y };
  return a2s_op(a, op, type, w, 2);
}

uint32_t a2s_glsl(struct a2s *a) {
  if (!a->glsl) {
    uint32_t s[8];
    a->glsl = spv_id(&a->s);
    s[0] = a->glsl;
    spv_emit(&a->s.imports, SpvOpExtInstImport, s, 1 + spv_str(s + 1, "GLSL.std.450"));
  }
  return a->glsl;
}

uint32_t a2s_ext(struct a2s *a, unsigned glsl_op, uint32_t type, const uint32_t *args, unsigned n) {
  uint32_t w[8];

  w[0] = a2s_glsl(a), w[1] = glsl_op;
  memcpy(w + 2, args, n * 4);
  return a2s_op(a, SpvOpExtInst, type, w, n + 2);
}

void a2s_stmt(struct a2s *a, unsigned op, const uint32_t *ops, unsigned n) {
  spv_emit(&a->body, op, ops, n);
}

// a uint constant of t's shape (splatted for vectors)
uint32_t a2s_uconst(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  uint32_t c = spv_c_u32(&a->s, v), parts[4];
  unsigned n = a2s_lanes(t);

  if (n == 1)
    return c;

  for (unsigned i = 0; i < n; i++) {
    parts[i] = c;
  }

  return spv_c_comp(&a->s, a2s_type(a, t), parts, n);
}

uint32_t a2s_fconst(struct a2s *a, LLVMTypeRef t, float v) {
  uint32_t c = spv_c_f32(&a->s, v), parts[4];
  unsigned n = a2s_lanes(t);

  if (n == 1)
    return c;

  for (unsigned i = 0; i < n; i++) {
    parts[i] = c;
  }

  return spv_c_comp(&a->s, a2s_type(a, t), parts, n);
}

static uint32_t bconst(struct a2s *a, LLVMTypeRef t, int v) {
  uint32_t c = spv_c_bool(&a->s, v), parts[4];
  unsigned n = a2s_lanes(t);

  if (n == 1)
    return c;

  for (unsigned i = 0; i < n; i++) {
    parts[i] = c;
  }

  return spv_c_comp(&a->s, a2s_type(a, t), parts, n);
}

// small integers live zero extended in 32 bits: the result of an op that can carry out is masked back
uint32_t a2s_mask(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  int bits = a2s_bits(t);

  if (bits >= 32)
    return v;

  return a2s_op2(a, SpvOpBitwiseAnd, a2s_type(a, t), v, a2s_uconst(a, t, (1u << bits) - 1));
}

// a small integer sign extended to 32 bits, for signed operations
uint32_t a2s_sext(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  int bits = a2s_bits(t);
  uint32_t w[3];

  if (bits >= 32)
    return v;

  w[0] = v, w[1] = spv_c_u32(&a->s, 0), w[2] = spv_c_u32(&a->s, (uint32_t)bits);
  return a2s_op(a, SpvOpBitFieldSExtract, a2s_type(a, t), w, 3);
}

// an integer of t's shape is non-zero, as bool
uint32_t a2s_bool_of(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  uint32_t bt = spv_t_vec(&a->s, spv_t_bool(&a->s), a2s_lanes(t));

  return a2s_op2(a, SpvOpINotEqual, bt, v, a2s_uconst(a, t, 0));
}

// float to the nearest half value, kept in a float. QuantizeToF16 may flush half denormals (lavapipe
// does): below 2^-14 the value is rounded to a multiple of 2^-24 here instead
uint32_t a2s_quantize(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  uint32_t ty = a2s_type(a, t), bt = spv_t_vec(&a->s, spv_t_bool(&a->s), a2s_lanes(t)), q, d, small;

  if (!a->opt->quantize_half)
    return v;

  q = a2s_op1(a, SpvOpQuantizeToF16, ty, v);
  d = a2s_op2(a, SpvOpFMul, ty, v, a2s_fconst(a, t, 16777216.0f));
  d = a2s_op2(a, SpvOpFMul, ty, a2s_ext(a, GLSLstd450RoundEven, ty, &d, 1), a2s_fconst(a, t, 1.0f / 16777216.0f));
  small = a2s_op2(a, SpvOpFOrdLessThan, bt, a2s_ext(a, GLSLstd450FAbs, ty, &v, 1), a2s_fconst(a, t, 1.0f / 16384.0f));
  return a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ small, d, q }, 3);
}

static uint32_t const_value(struct a2s *a, LLVMValueRef v) {
  LLVMTypeRef t = LLVMTypeOf(v);
  uint32_t ty = a2s_type(a, t), parts[64];
  unsigned n, i;

  if (LLVMIsAUndefValue(v) || LLVMIsAPoisonValue(v))
    return spv_c_undef(&a->s, ty);

  if (LLVMIsAConstantAggregateZero(v) || LLVMIsAConstantPointerNull(v))
    return spv_c_null(&a->s, ty);

  if (LLVMIsAConstantInt(v)) {
    unsigned long long x = LLVMConstIntGetZExtValue(v);
    int bits = a2s_bits(t);

    if (bits == 1)
      return spv_c_bool(&a->s, (int)(x & 1));

    if (bits > 32 && x >> 32 && (x >> 31) != 0x1ffffffffull)
      a2s_note(a, "i64 constant 0x%llx truncated", x);

    return spv_c_u32(&a->s, bits >= 32 ? (uint32_t)x : (uint32_t)x & ((1u << bits) - 1));
  }

  if (LLVMIsAConstantFP(v)) {
    LLVMBool lost;
    return spv_c_f32(&a->s, (float)LLVMConstRealGetDouble(v, &lost));
  }

  if (LLVMIsAConstantDataSequential(v) || LLVMIsAConstantVector(v) || LLVMIsAConstantStruct(v) ||
      LLVMIsAConstantArray(v)) {
    LLVMTypeKind k = LLVMGetTypeKind(t);

    n = k == LLVMVectorTypeKind  ? LLVMGetVectorSize(t)
        : k == LLVMArrayTypeKind ? (unsigned)LLVMGetArrayLength2(t)
                                 : LLVMCountStructElementTypes(t);
    if (n > 64)
      a2s_fail(a, "constant of %u elements", n);

    for (i = 0; i < n; i++) {
      LLVMValueRef e = LLVMIsAConstantDataSequential(v) ? LLVMGetAggregateElement(v, i) : LLVMGetOperand(v, i);

      // an undef lane in a constant vector is any value: zero keeps the constant a constant
      if (LLVMIsAUndefValue(e) || LLVMIsAPoisonValue(e))
        parts[i] = spv_c_null(&a->s, a2s_type(a, LLVMTypeOf(e)));
      else
        parts[i] = const_value(a, e);
    }

    if (n == 1 && k == LLVMVectorTypeKind)
      return parts[0];

    return spv_c_comp(&a->s, ty, parts, n);
  }

  {
    char *s = LLVMPrintValueToString(v);
    char msg[160];
    snprintf(msg, sizeof(msg), "%.150s", s);
    LLVMDisposeMessage(s);
    a2s_fail(a, "constant not handled: %s", msg);
  }
}

uint32_t a2s_value(struct a2s *a, LLVMValueRef v) {
  struct vslot *s = a2s_slot(a, v, 0);

  if (s && s->kind == VK_ID)
    return s->id;

  if (LLVMIsConstant(v) && !LLVMIsAGlobalValue(v)) {
    uint32_t id = const_value(a, v);
    a2s_set(a, v, id);
    return id;
  }

  {
    char *str = LLVMPrintValueToString(v);
    char msg[160];
    snprintf(msg, sizeof(msg), "%.150s", str);
    LLVMDisposeMessage(str);
    a2s_fail(a, "value used before it was defined: %s", msg);
  }
}

uint32_t a2s_pc_word(struct a2s *a, enum a2s_pc_kind kind, uint32_t metal) {
  struct a2s_reflect *r = a->r;
  unsigned i;

  for (i = 0; i < r->npc; i++) {
    if (r->pc[i].kind == kind && r->pc[i].metal == metal)
      return a->pc_ids[i];
  }

  if (r->npc == A2S_MAX_PC)
    a2s_fail(a, "too many push constant words");

  r->pc[r->npc].kind = kind, r->pc[r->npc].metal = metal;
  // the load goes into the prologue once the block type is known: here a placeholder id
  a->pc_ids[r->npc] = spv_id(&a->s);
  return a->pc_ids[r->npc++];
}

int a2s_add_root(struct a2s *a, enum root_kind kind, uint32_t words) {
  struct root *rt;
  uint32_t ut = spv_t_uint(&a->s), arr, sc;

  if (a->nroots == MAX_ROOTS)
    a2s_fail(a, "too many memory roots");

  rt = &a->roots[a->nroots];
  memset(rt, 0, sizeof(*rt));
  rt->kind = kind, rt->words = words;

  if (kind == ROOT_BUFFER) {
    // block { uint d[], } in StorageBuffer: the caller decorates set/binding
    uint32_t ra = a->opt->uniform_words ? spv_t_array(&a->s, ut, spv_c_u32(&a->s, words))
                                      : spv_t_rtarray_unique(&a->s, ut), st;
    spv_deco(&a->s, ra, SpvDecorationArrayStride, 1, a->opt->uniform_words ? 16 : 4);
    st = spv_t_struct_unique(&a->s, &ra, 1);
    spv_deco(&a->s, st, SpvDecorationBlock, 0);
    spv_mdeco(&a->s, st, 0, SpvDecorationOffset, 1, 0);
    sc = a->opt->uniform_words ? SpvStorageClassUniform : SpvStorageClassStorageBuffer;
    rt->var = spv_var(&a->s, spv_t_ptr(&a->s, sc, st), sc);
    a->iface[a->niface++] = rt->var;
  } else if (kind == ROOT_FN) {
    sc = SpvStorageClassFunction;
    arr = spv_t_array(&a->s, ut, spv_c_u32(&a->s, words ? words : 1));
    rt->var = spv_id(&a->s);
    spv_op(&a->vars, SpvOpVariable, 3, spv_t_ptr(&a->s, sc, arr), rt->var, sc);
  } else {
    sc = kind == ROOT_WG ? SpvStorageClassWorkgroup : SpvStorageClassPrivate;
    arr = spv_t_array(&a->s, ut, spv_c_u32(&a->s, words ? words : 1));
    rt->var = spv_var(&a->s, spv_t_ptr(&a->s, sc, arr), sc);
    a->iface[a->niface++] = rt->var;
  }

  rt->sc = sc;
  return a->nroots++;
}

// IEEE half bits of f, rounded to nearest even
static uint16_t half_bits(float f) {
  uint32_t x, sign, mant;
  int exp;

  memcpy(&x, &f, 4);
  sign = (x >> 16) & 0x8000, exp = (int)((x >> 23) & 0xff) - 127 + 15, mant = x & 0x7fffff;
  if (((x >> 23) & 0xff) == 0xff)
    return (uint16_t)(sign | 0x7c00 | (mant ? 0x200 : 0));
  if (exp >= 31)
    return (uint16_t)(sign | 0x7c00);
  if (exp <= 0) {
    uint32_t m, shift, h, rem, half;
    if (exp < -10)
      return (uint16_t)sign;
    m = mant | 0x800000, shift = (uint32_t)(14 - exp);
    h = m >> shift, rem = m & ((1u << shift) - 1), half = 1u << (shift - 1);
    if (rem > half || (rem == half && (h & 1)))
      h++;
    return (uint16_t)(sign | h);
  }
  {
    uint32_t h = (uint32_t)exp << 10 | mant >> 13, rem = mant & 0x1fff;
    if (rem > 0x1000 || (rem == 0x1000 && (h & 1)))
      h++;
    return (uint16_t)(sign | h);
  }
}

// a constant's bytes as the target lays them out, undefined and null parts stay zero
static void const_bytes(struct a2s *a, LLVMValueRef c, LLVMTypeRef t, uint8_t *dst, uint64_t room) {
  uint64_t size = LLVMABISizeOfType(a->td, t);
  LLVMTypeKind k = LLVMGetTypeKind(t);

  if (size > room)
    a2s_fail(a, "constant larger than its global");
  if (LLVMIsUndef(c) || LLVMIsPoison(c) || LLVMIsNull(c))
    return;
  if (k == LLVMIntegerTypeKind) {
    uint64_t v = LLVMConstIntGetZExtValue(c);
    unsigned n = (LLVMGetIntTypeWidth(t) + 7) / 8;
    for (unsigned i = 0; i < n && i < 8; i++)
      dst[i] = (uint8_t)(v >> (8 * i));
    return;
  }
  if (k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind || k == LLVMHalfTypeKind) {
    LLVMBool lossy;
    double d = LLVMConstRealGetDouble(c, &lossy);
    if (k == LLVMDoubleTypeKind) {
      memcpy(dst, &d, 8);
    } else if (k == LLVMFloatTypeKind) {
      float f = (float)d;
      memcpy(dst, &f, 4);
    } else {
      uint16_t h = half_bits((float)d);
      memcpy(dst, &h, 2);
    }
    return;
  }
  if (k == LLVMArrayTypeKind || k == LLVMVectorTypeKind) {
    LLVMTypeRef et = LLVMGetElementType(t);
    unsigned n = k == LLVMArrayTypeKind ? (unsigned)LLVMGetArrayLength2(t) : LLVMGetVectorSize(t);
    // vector lanes sit at their own size, array elements at the element's allocation size
    uint64_t stride = k == LLVMVectorTypeKind ? LLVMStoreSizeOfType(a->td, et) : LLVMABISizeOfType(a->td, et);
    for (unsigned i = 0; i < n; i++)
      const_bytes(a, LLVMGetAggregateElement(c, i), et, dst + i * stride, room - i * stride);
    return;
  }
  if (k == LLVMStructTypeKind) {
    unsigned n = LLVMCountStructElementTypes(t);
    for (unsigned i = 0; i < n; i++) {
      uint64_t off = LLVMOffsetOfElement(a->td, t, i);
      const_bytes(a, LLVMGetAggregateElement(c, i), LLVMStructGetTypeAtIndex(t, i), dst + off, room - off);
    }
    return;
  }
  a2s_fail(a, "constant of an unhandled kind in a constant global");
}

// a constant address space global: its bytes as an initialised Private word array, one root per global
static int const_root(struct a2s *a, LLVMValueRef g) {
  LLVMTypeRef vt = LLVMGlobalGetValueType(g);
  uint64_t size = LLVMABISizeOfType(a->td, vt);
  uint32_t words = (uint32_t)((size + 3) / 4), ut = spv_t_uint(&a->s), arr, init, *ops;
  uint8_t *bytes;
  struct root *rt;
  int r;

  for (int i = 0; i < a->ncglobal; i++) {
    if (a->cglobal[i] == g)
      return a->croot[i];
  }
  if (a->ncglobal == MAX_ROOTS || a->nroots == MAX_ROOTS)
    a2s_fail(a, "too many constant globals");
  if (!words)
    words = 1;
  bytes = calloc(words, 4);
  ops = calloc(words + 2, 4);
  if (!bytes || !ops)
    a2s_fail(a, "out of memory for a constant global");
  const_bytes(a, LLVMGetInitializer(g), vt, bytes, (uint64_t)words * 4);

  arr = spv_t_array(&a->s, ut, spv_c_u32(&a->s, words));
  ops[0] = arr, ops[1] = init = spv_id(&a->s);
  for (uint32_t i = 0; i < words; i++) {
    uint32_t w;
    memcpy(&w, bytes + 4 * i, 4);
    ops[2 + i] = spv_c_u32(&a->s, w);
  }
  spv_emit(&a->s.globals, SpvOpConstantComposite, ops, words + 2);
  free(bytes);
  free(ops);

  r = a->nroots;
  rt = &a->roots[a->nroots++];
  memset(rt, 0, sizeof(*rt));
  rt->kind = ROOT_PRIVATE, rt->words = words, rt->sc = SpvStorageClassPrivate;
  rt->var = spv_id(&a->s);
  spv_op(&a->s.globals, SpvOpVariable, 4, spv_t_ptr(&a->s, SpvStorageClassPrivate, arr), rt->var, SpvStorageClassPrivate, init);
  if (a->niface == sizeof(a->iface) / sizeof(a->iface[0]))
    a2s_fail(a, "too many interface variables");
  a->iface[a->niface++] = rt->var;

  a->cglobal[a->ncglobal] = g, a->croot[a->ncglobal++] = r;
  return r;
}

// pointers: GEP chains, bitcasts and address space casts over a root
struct aptr a2s_ptr(struct a2s *a, LLVMValueRef v) {
  struct vslot *s = a2s_slot(a, v, 0);
  struct aptr p;

  if (s && s->kind == VK_PTR)
    return s->u.p;

  if (LLVMIsAConstantExpr(v)) {
    LLVMOpcode op = LLVMGetConstOpcode(v);

    if (op == LLVMBitCast || op == LLVMAddrSpaceCast)
      return a2s_ptr(a, LLVMGetOperand(v, 0));

    if (op == LLVMGetElementPtr) {
      LLVMTypeRef ty = LLVMGetGEPSourceElementType(v);
      unsigned n = (unsigned)LLVMGetNumOperands(v), i;

      p = a2s_ptr(a, LLVMGetOperand(v, 0));
      for (i = 1; i < n; i++) {
        LLVMValueRef ix = LLVMGetOperand(v, i);
        long long k = LLVMConstIntGetSExtValue(ix);

        if (i == 1) {
          p.cst += (uint32_t)(k * (long long)LLVMABISizeOfType(a->td, ty));
        } else if (LLVMGetTypeKind(ty) == LLVMStructTypeKind) {
          p.cst += (uint32_t)LLVMOffsetOfElement(a->td, ty, (unsigned)k);
          ty = LLVMStructGetTypeAtIndex(ty, (unsigned)k);
        } else {
          ty = LLVMGetElementType(ty);
          p.cst += (uint32_t)(k * (long long)LLVMABISizeOfType(a->td, ty));
        }
      }
      return p;
    }
  }

  if (LLVMIsAGlobalVariable(v) && LLVMGetPointerAddressSpace(LLVMTypeOf(v)) == 2 && LLVMGetInitializer(v)) {
    p.root = const_root(a, v), p.dyn = 0, p.cst = 0;
    return p;
  }

  {
    char *str = LLVMPrintValueToString(v);
    char msg[160];
    snprintf(msg, sizeof(msg), "%.150s", str);
    LLVMDisposeMessage(str);
    a2s_fail(a, "pointer with unknown origin: %s", msg);
  }
}

static void set_ptr(struct a2s *a, LLVMValueRef v, struct aptr p) {
  struct vslot *s = a2s_slot(a, v, 1);

  s->kind = VK_PTR, s->u.p = p;
}

// the byte offset of p as one id
static uint32_t ptr_bytes(struct a2s *a, struct aptr p) {
  uint32_t c = spv_c_u32(&a->s, p.cst);

  if (!p.dyn)
    return c;

  return p.cst ? a2s_op2(a, SpvOpIAdd, spv_t_uint(&a->s), p.dyn, c) : p.dyn;
}

uint32_t a2s_ptr_bytes(struct a2s *a, LLVMValueRef v) {
  if (LLVMIsAConstantPointerNull(v))
    return spv_c_u32(&a->s, A2S_NULL_BYTES);
  return ptr_bytes(a, a2s_ptr(a, v));
}

uint32_t a2s_word_ptr(struct a2s *a, int root, uint32_t idx) {
  struct root *rt = &a->roots[root];
  uint32_t pt = spv_t_ptr(&a->s, rt->sc, spv_t_uint(&a->s)), w[3];

  w[0] = rt->var;
  if (rt->kind == ROOT_BUFFER) {
    w[1] = spv_c_u32(&a->s, 0), w[2] = idx;
    return a2s_op(a, SpvOpAccessChain, pt, w, 3);
  }

  w[1] = idx;
  return a2s_op(a, SpvOpAccessChain, pt, w, 2);
}

static uint32_t wload(struct a2s *a, int root, uint32_t idx) {
  return a2s_op1(a, SpvOpLoad, spv_t_uint(&a->s), a2s_word_ptr(a, root, idx));
}

static void wstore(struct a2s *a, int root, uint32_t idx, uint32_t v) {
  uint32_t w[2] = { a2s_word_ptr(a, root, idx), v };

  a2s_stmt(a, SpvOpStore, w, 2);
}

// bytes of a word another thread may share: clear ours then set them, each one atomic
static void wpartial(struct a2s *a, int root, uint32_t idx, uint32_t keep_mask, uint32_t bits) {
  struct root *rt = &a->roots[root];
  uint32_t ut = spv_t_uint(&a->s), p = a2s_word_ptr(a, root, idx), w[4];

  if (rt->kind == ROOT_FN || rt->kind == ROOT_PRIVATE) {
    uint32_t old = a2s_op1(a, SpvOpLoad, ut, p);
    uint32_t v = a2s_op2(a, SpvOpBitwiseOr, ut, a2s_op2(a, SpvOpBitwiseAnd, ut, old, keep_mask), bits);
    w[0] = p, w[1] = v;
    a2s_stmt(a, SpvOpStore, w, 2);
    return;
  }

  w[0] = p;
  w[1] = spv_c_u32(&a->s, rt->kind == ROOT_WG ? SpvScopeWorkgroup : SpvScopeDevice);
  w[2] = spv_c_u32(&a->s, 0);
  w[3] = keep_mask;
  a2s_op(a, SpvOpAtomicAnd, ut, w, 4);
  w[3] = bits;
  a2s_op(a, SpvOpAtomicOr, ut, w, 4);
}

// a scalar leaf's bits, low aligned in a uint
static uint32_t leaf_bits(struct a2s *a, LLVMTypeRef t, uint32_t v) {
  uint32_t ut = spv_t_uint(&a->s), ft = spv_t_float(&a->s);

  switch (LLVMGetTypeKind(t)) {
  case LLVMFloatTypeKind:
    return a2s_op1(a, SpvOpBitcast, ut, v);
  case LLVMHalfTypeKind: {
    // PackHalf2x16 may flush denormals: those are built by hand, sign | round(|v| * 2^24)
    uint32_t pair[2] = { v, spv_c_f32(&a->s, 0) }, vec, h, av, den, sgn, small;
    vec = a2s_op(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, ft, 2), pair, 2);
    h = a2s_ext(a, GLSLstd450PackHalf2x16, ut, &vec, 1);
    av = a2s_ext(a, GLSLstd450FAbs, ft, &v, 1);
    den = a2s_op2(a, SpvOpFMul, ft, av, spv_c_f32(&a->s, 16777216.0f));
    den = a2s_op1(a, SpvOpConvertFToU, ut, a2s_ext(a, GLSLstd450RoundEven, ft, &den, 1));
    sgn = a2s_op2(a, SpvOpBitwiseAnd, ut, a2s_op2(a, SpvOpShiftRightLogical, ut, a2s_op1(a, SpvOpBitcast, ut, v), spv_c_u32(&a->s, 16)), spv_c_u32(&a->s, 0x8000));
    small = a2s_op2(a, SpvOpFOrdLessThan, spv_t_bool(&a->s), av, spv_c_f32(&a->s, 1.0f / 16384.0f));
    return a2s_op(a, SpvOpSelect, ut, (uint32_t[]){ small, a2s_op2(a, SpvOpBitwiseOr, ut, sgn, den), h }, 3);
  }
  case LLVMIntegerTypeKind:
    if (LLVMGetIntTypeWidth(t) == 1)
      return a2s_op(a, SpvOpSelect, ut, (uint32_t[]){ v, spv_c_u32(&a->s, 1), spv_c_u32(&a->s, 0) }, 3);
    return v;
  default:
    a2s_fail(a, "memory leaf of type kind %d", (int)LLVMGetTypeKind(t));
  }
}

static uint32_t leaf_value(struct a2s *a, LLVMTypeRef t, uint32_t bits) {
  uint32_t ft = spv_t_float(&a->s);

  switch (LLVMGetTypeKind(t)) {
  case LLVMFloatTypeKind:
    return a2s_op1(a, SpvOpBitcast, ft, bits);
  case LLVMHalfTypeKind: {
    // UnpackHalf2x16 may flush denormals: those are (bits & 0x3ff) * 2^-24 with the sign
    uint32_t ut = spv_t_uint(&a->s), v2 = a2s_ext(a, GLSLstd450UnpackHalf2x16, spv_t_vec(&a->s, ft, 2), &bits, 1), w[2] = { v2, 0 };
    uint32_t f = a2s_op(a, SpvOpCompositeExtract, ft, w, 2), den, exp0;
    den = a2s_op2(a, SpvOpFMul, ft, a2s_op1(a, SpvOpConvertUToF, ft, a2s_op2(a, SpvOpBitwiseAnd, ut, bits, spv_c_u32(&a->s, 0x3ff))),
                  spv_c_f32(&a->s, 1.0f / 16777216.0f));
    den = a2s_op(a, SpvOpSelect, ft, (uint32_t[]){ a2s_op2(a, SpvOpINotEqual, spv_t_bool(&a->s), a2s_op2(a, SpvOpBitwiseAnd, ut, bits, spv_c_u32(&a->s, 0x8000)), spv_c_u32(&a->s, 0)),
                                                  a2s_op1(a, SpvOpFNegate, ft, den), den }, 3);
    exp0 = a2s_op2(a, SpvOpIEqual, spv_t_bool(&a->s), a2s_op2(a, SpvOpBitwiseAnd, ut, bits, spv_c_u32(&a->s, 0x7c00)), spv_c_u32(&a->s, 0));
    return a2s_op(a, SpvOpSelect, ft, (uint32_t[]){ exp0, den, f }, 3);
  }
  case LLVMIntegerTypeKind:
    if (LLVMGetIntTypeWidth(t) == 1)
      return a2s_op2(a, SpvOpINotEqual, spv_t_bool(&a->s), bits, spv_c_u32(&a->s, 0));
    return bits;
  default:
    a2s_fail(a, "memory leaf of type kind %d", (int)LLVMGetTypeKind(t));
  }
}

static unsigned leaf_size(struct a2s *a, LLVMTypeRef t) {
  unsigned n = (unsigned)LLVMStoreSizeOfType(a->td, t);

  return n > 4 ? 4 : n ? n : 1;
}

// a value's scalar leaves at byte offsets, as (offset, size, bits)
struct leaf {
  uint32_t off, size, bits;
};

static void collect(struct a2s *a, LLVMTypeRef t, uint32_t v, uint32_t off, struct leaf *l, unsigned *n) {
  LLVMTypeKind k = LLVMGetTypeKind(t);
  unsigned i, cnt;

  if (k == LLVMVectorTypeKind || k == LLVMArrayTypeKind || k == LLVMStructTypeKind) {
    cnt = k == LLVMVectorTypeKind  ? LLVMGetVectorSize(t)
          : k == LLVMArrayTypeKind ? (unsigned)LLVMGetArrayLength2(t)
                                   : LLVMCountStructElementTypes(t);
    for (i = 0; i < cnt; i++) {
      LLVMTypeRef et = k == LLVMStructTypeKind ? LLVMStructGetTypeAtIndex(t, i) : LLVMGetElementType(t);
      uint32_t eo = k == LLVMStructTypeKind ? (uint32_t)LLVMOffsetOfElement(a->td, t, i)
                    : k == LLVMVectorTypeKind ? i * (uint32_t)LLVMStoreSizeOfType(a->td, et)
                                              : i * (uint32_t)LLVMABISizeOfType(a->td, et);
      uint32_t ev, w[2] = { v, i };

      // <1 x T> is the scalar itself
      ev = k == LLVMVectorTypeKind && cnt == 1 ? v : a2s_op(a, SpvOpCompositeExtract, a2s_type(a, et), w, 2);
      collect(a, et, ev, off + eo, l, n);
    }
    return;
  }

  if (*n >= 256)
    a2s_fail(a, "value has too many leaves");

  l[*n].off = off, l[*n].size = leaf_size(a, t), l[*n].bits = leaf_bits(a, t, v);
  if (LLVMStoreSizeOfType(a->td, t) == 8)
    a2s_note(a, "64 bit memory leaf narrowed");
  (*n)++;
}

// the static-word case: every leaf's word index and shift known relative to a word aligned base
struct words {
  uint32_t id[256];
  uint8_t have[256];
};

typedef uint32_t (*word_fetch)(struct a2s *a, void *ctx, uint32_t word, uint32_t *shift_dyn);

// build t from leaves fetched as (word, shift)
static uint32_t assemble(struct a2s *a, LLVMTypeRef t, uint32_t off, word_fetch fetch, void *ctx) {
  LLVMTypeKind k = LLVMGetTypeKind(t);
  uint32_t parts[64], ut = spv_t_uint(&a->s);
  unsigned i, cnt;

  if (k == LLVMVectorTypeKind || k == LLVMArrayTypeKind || k == LLVMStructTypeKind) {
    cnt = k == LLVMVectorTypeKind  ? LLVMGetVectorSize(t)
          : k == LLVMArrayTypeKind ? (unsigned)LLVMGetArrayLength2(t)
                                   : LLVMCountStructElementTypes(t);
    if (cnt > 64)
      a2s_fail(a, "aggregate of %u elements in memory", cnt);

    for (i = 0; i < cnt; i++) {
      LLVMTypeRef et = k == LLVMStructTypeKind ? LLVMStructGetTypeAtIndex(t, i) : LLVMGetElementType(t);
      uint32_t eo = k == LLVMStructTypeKind ? (uint32_t)LLVMOffsetOfElement(a->td, t, i)
                    : k == LLVMVectorTypeKind ? i * (uint32_t)LLVMStoreSizeOfType(a->td, et)
                                              : i * (uint32_t)LLVMABISizeOfType(a->td, et);
      parts[i] = assemble(a, et, off + eo, fetch, ctx);
    }

    if (k == LLVMVectorTypeKind && cnt == 1)
      return parts[0];

    return a2s_op(a, SpvOpCompositeConstruct, a2s_type(a, t), parts, cnt);
  }

  {
    unsigned size = leaf_size(a, t);
    uint32_t sh = 0, w = fetch(a, ctx, off, &sh), bits;

    // fetch answers the word holding byte off and the shift of that byte in it
    bits = sh ? a2s_op2(a, SpvOpShiftRightLogical, ut, w, sh) : w;
    if (size < 4)
      bits = a2s_op2(a, SpvOpBitwiseAnd, ut, bits, spv_c_u32(&a->s, (1u << (size * 8)) - 1));
    return leaf_value(a, t, bits);
  }
}

struct mem_ctx {
  int root, aligned;
  uint32_t base, wbase, wide, cbase;
  struct words cache;
};

// the same buffer binding seen as uvec2 or uvec4 elements (aliased descriptors are allowed)
static uint32_t wide_view(struct a2s *a, int root, unsigned n) {
  struct root *rt = &a->roots[root];
  uint32_t vt, ra, st;

  if (rt->view[n])
    return rt->view[n];

  vt = spv_t_vec(&a->s, spv_t_uint(&a->s), n);
  ra = spv_t_rtarray_unique(&a->s, vt);
  spv_deco(&a->s, ra, SpvDecorationArrayStride, 1, 4 * n);
  st = spv_t_struct_unique(&a->s, &ra, 1);
  spv_deco(&a->s, st, SpvDecorationBlock, 0);
  spv_mdeco(&a->s, st, 0, SpvDecorationOffset, 1, 0);
  rt->view[n] = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassStorageBuffer, st), SpvStorageClassStorageBuffer);
  spv_deco(&a->s, rt->view[n], SpvDecorationDescriptorSet, 1, a->opt->set);
  spv_deco(&a->s, rt->view[n], SpvDecorationBinding, 1, rt->binding);
  if (rt->ro)
    spv_deco(&a->s, rt->view[n], SpvDecorationNonWritable, 0);
  a->iface[a->niface++] = rt->view[n];
  a->r->offset_align = 16;
  return rt->view[n];
}

static uint32_t fetch_mem(struct a2s *a, void *vctx, uint32_t off, uint32_t *shift) {
  struct mem_ctx *m = vctx;
  uint32_t ut = spv_t_uint(&a->s);

  if (m->aligned) {
    uint32_t wi = off >> 2;

    *shift = (off & 3) ? spv_c_u32(&a->s, (off & 3) * 8) : 0;
    if (wi < 256 && m->cache.have[wi])
      return m->cache.id[wi];

    // one uvec2/uvec4 load fills the cache for its words
    if (m->wide > 1 && wi < 256) {
      uint32_t ch = wi / m->wide, vt = spv_t_vec(&a->s, ut, m->wide), w[3], v, p;

      w[0] = wide_view(a, m->root, m->wide), w[1] = spv_c_u32(&a->s, 0);
      w[2] = ch ? a2s_op2(a, SpvOpIAdd, ut, m->cbase, spv_c_u32(&a->s, ch)) : m->cbase;
      p = a2s_op(a, SpvOpAccessChain, spv_t_ptr(&a->s, SpvStorageClassStorageBuffer, vt), w, 3);
      v = a2s_op1(a, SpvOpLoad, vt, p);
      for (uint32_t j = 0; j < m->wide && ch * m->wide + j < 256; j++) {
        m->cache.have[ch * m->wide + j] = 1;
        m->cache.id[ch * m->wide + j] = a2s_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ v, j }, 2);
      }
      return m->cache.id[wi];
    }

    {
      uint32_t idx = wi ? a2s_op2(a, SpvOpIAdd, ut, m->wbase, spv_c_u32(&a->s, wi)) : m->wbase;
      uint32_t v = wload(a, m->root, idx);
      if (wi < 256)
        m->cache.have[wi] = 1, m->cache.id[wi] = v;
      return v;
    }
  }

  {
    uint32_t b = off ? a2s_op2(a, SpvOpIAdd, ut, m->base, spv_c_u32(&a->s, off)) : m->base;
    uint32_t idx = a2s_op2(a, SpvOpShiftRightLogical, ut, b, spv_c_u32(&a->s, 2));
    uint32_t lo = a2s_op2(a, SpvOpBitwiseAnd, ut, b, spv_c_u32(&a->s, 3));

    *shift = a2s_op2(a, SpvOpShiftLeftLogical, ut, lo, spv_c_u32(&a->s, 3));
    return wload(a, m->root, idx);
  }
}

static int static_align(struct aptr p, unsigned align) {
  return !p.dyn || align >= 4;
}

uint32_t a2s_load_mem(struct a2s *a, LLVMTypeRef t, struct aptr p, unsigned align) {
  struct mem_ctx m;
  uint32_t ut = spv_t_uint(&a->s);

  memset(&m, 0, sizeof(m));
  m.root = p.root;
  m.base = ptr_bytes(a, p);

  // a word aligned base: shifts are constants and words are shared between leaves
  if (static_align(p, align) && (p.dyn || !(p.cst & 3))) {
    unsigned size = (unsigned)LLVMStoreSizeOfType(a->td, t);

    m.aligned = 1;
    m.wbase = a2s_op2(a, SpvOpShiftRightLogical, ut, m.base, spv_c_u32(&a->s, 2));
    if (a->roots[p.root].kind == ROOT_BUFFER && !a->opt->narrow_loads && !a->opt->uniform_words) {
      unsigned al = p.dyn ? align : 16;
      for (; al > 1 && (p.cst % al); al /= 2) {
      }
      if (p.dyn && p.cst % (align ? align : 1))
        al = 4;
      m.wide = al >= 16 && size >= 12 ? 4 : al >= 8 && size >= 8 ? 2 : 1;
      if (m.wide > 1)
        m.cbase = a2s_op2(a, SpvOpShiftRightLogical, ut, m.base, spv_c_u32(&a->s, m.wide == 4 ? 4 : 3));
    }
  }

  return assemble(a, t, 0, fetch_mem, &m);
}

void a2s_store_mem(struct a2s *a, LLVMTypeRef t, uint32_t v, struct aptr p, unsigned align) {
  static struct leaf l[256];
  unsigned n = 0, i;
  uint32_t ut = spv_t_uint(&a->s), base = ptr_bytes(a, p);

  collect(a, t, v, 0, l, &n);

  if (static_align(p, align) && (p.dyn || !(p.cst & 3))) {
    uint32_t wbase = a2s_op2(a, SpvOpShiftRightLogical, ut, base, spv_c_u32(&a->s, 2));
    uint32_t last = 0;

    for (i = 0; i < n; i++) {
      if (l[i].off + l[i].size > last)
        last = l[i].off + l[i].size;
    }

    // per word: the leaves in it ORed together, written whole or merged into what is there
    for (uint32_t w = 0; w * 4 < last; w++) {
      uint32_t val = 0, covered = 0, idx;

      for (i = 0; i < n; i++) {
        uint32_t sh, bits;

        if (l[i].off / 4 != w)
          continue;

        sh = (l[i].off & 3) * 8;
        bits = sh ? a2s_op2(a, SpvOpShiftLeftLogical, ut, l[i].bits, spv_c_u32(&a->s, sh)) : l[i].bits;
        val = val ? a2s_op2(a, SpvOpBitwiseOr, ut, val, bits) : bits;
        covered |= (l[i].size >= 4 ? 0xffffffffu : ((1u << (l[i].size * 8)) - 1)) << sh;
      }

      if (!val)
        continue;

      idx = w ? a2s_op2(a, SpvOpIAdd, ut, wbase, spv_c_u32(&a->s, w)) : wbase;
      if (covered == 0xffffffffu)
        wstore(a, p.root, idx, val);
      else
        wpartial(a, p.root, idx, spv_c_u32(&a->s, ~covered), val);
    }
    return;
  }

  // unknown alignment: each leaf on its own, merged into its word at a dynamic shift
  for (i = 0; i < n; i++) {
    uint32_t b = l[i].off ? a2s_op2(a, SpvOpIAdd, ut, base, spv_c_u32(&a->s, l[i].off)) : base;
    uint32_t idx = a2s_op2(a, SpvOpShiftRightLogical, ut, b, spv_c_u32(&a->s, 2));
    uint32_t sh = a2s_op2(a, SpvOpShiftLeftLogical, ut, a2s_op2(a, SpvOpBitwiseAnd, ut, b, spv_c_u32(&a->s, 3)),
                          spv_c_u32(&a->s, 3));
    uint32_t bits = a2s_op2(a, SpvOpShiftLeftLogical, ut, l[i].bits, sh), keep;

    if (l[i].size >= 4) {
      wstore(a, p.root, idx, l[i].bits);
      continue;
    }

    keep = a2s_op1(a, SpvOpNot, ut,
                   a2s_op2(a, SpvOpShiftLeftLogical, ut, spv_c_u32(&a->s, (1u << (l[i].size * 8)) - 1), sh));
    wpartial(a, p.root, idx, keep, bits);
  }
}

// bitcast through words: the source's leaves at their byte offsets, reassembled as the target
struct reg_ctx {
  struct leaf *l;
  unsigned n;
};

static uint32_t fetch_reg(struct a2s *a, void *vctx, uint32_t off, uint32_t *shift) {
  struct reg_ctx *r = vctx;
  uint32_t ut = spv_t_uint(&a->s), val = 0;
  unsigned i;

  *shift = (off & 3) ? spv_c_u32(&a->s, (off & 3) * 8) : 0;
  for (i = 0; i < r->n; i++) {
    uint32_t sh, bits;

    if (r->l[i].off / 4 != off / 4)
      continue;

    sh = (r->l[i].off & 3) * 8;
    bits = sh ? a2s_op2(a, SpvOpShiftLeftLogical, ut, r->l[i].bits, spv_c_u32(&a->s, sh)) : r->l[i].bits;
    val = val ? a2s_op2(a, SpvOpBitwiseOr, ut, val, bits) : bits;
  }

  return val ? val : spv_c_u32(&a->s, 0);
}

uint32_t a2s_bitcast(struct a2s *a, LLVMTypeRef from, LLVMTypeRef to, uint32_t v) {
  static struct leaf l[256];
  struct reg_ctx r = { l, 0 };

  // same representation: float and uint words
  if (a2s_bits(from) == 32 && a2s_bits(to) == 32 && a2s_lanes(from) == a2s_lanes(to)) {
    if (is_float(from) == is_float(to))
      return v;
    return a2s_op1(a, SpvOpBitcast, a2s_type(a, to), v);
  }

  collect(a, from, v, 0, l, &r.n);
  return assemble(a, to, 0, fetch_reg, &r);
}

static uint32_t cmp_op(struct a2s *a, LLVMValueRef in, LLVMTypeRef t, uint32_t x, uint32_t y) {
  uint32_t bt = spv_t_vec(&a->s, spv_t_bool(&a->s), a2s_lanes(t));

  if (LLVMGetInstructionOpcode(in) == LLVMFCmp) {
    static const unsigned fops[] = {
      [LLVMRealOEQ] = SpvOpFOrdEqual,          [LLVMRealOGT] = SpvOpFOrdGreaterThan,
      [LLVMRealOGE] = SpvOpFOrdGreaterThanEqual, [LLVMRealOLT] = SpvOpFOrdLessThan,
      [LLVMRealOLE] = SpvOpFOrdLessThanEqual,  [LLVMRealONE] = SpvOpFOrdNotEqual,
      [LLVMRealUEQ] = SpvOpFUnordEqual,        [LLVMRealUGT] = SpvOpFUnordGreaterThan,
      [LLVMRealUGE] = SpvOpFUnordGreaterThanEqual, [LLVMRealULT] = SpvOpFUnordLessThan,
      [LLVMRealULE] = SpvOpFUnordLessThanEqual, [LLVMRealUNE] = SpvOpFUnordNotEqual,
    };
    LLVMRealPredicate p = LLVMGetFCmpPredicate(in);

    if (p == LLVMRealPredicateFalse)
      return bconst(a, t, 0);
    if (p == LLVMRealPredicateTrue)
      return bconst(a, t, 1);
    if (p == LLVMRealORD || p == LLVMRealUNO) {
      uint32_t nx = a2s_op1(a, SpvOpIsNan, bt, x), ny = a2s_op1(a, SpvOpIsNan, bt, y);
      uint32_t any = a2s_op2(a, SpvOpLogicalOr, bt, nx, ny);
      return p == LLVMRealUNO ? any : a2s_op1(a, SpvOpLogicalNot, bt, any);
    }
    return a2s_op2(a, fops[p], bt, x, y);
  }

  {
    LLVMIntPredicate p = LLVMGetICmpPredicate(in);
    int sgn = p == LLVMIntSGT || p == LLVMIntSGE || p == LLVMIntSLT || p == LLVMIntSLE;
    unsigned op;

    if (is_bool(t)) {
      if (p == LLVMIntEQ)
        return a2s_op2(a, SpvOpLogicalEqual, bt, x, y);
      if (p == LLVMIntNE)
        return a2s_op2(a, SpvOpLogicalNotEqual, bt, x, y);
      a2s_fail(a, "ordered compare of i1");
    }

    if (sgn)
      x = a2s_sext(a, t, x), y = a2s_sext(a, t, y);

    switch (p) {
    case LLVMIntEQ: op = SpvOpIEqual; break;
    case LLVMIntNE: op = SpvOpINotEqual; break;
    case LLVMIntUGT: op = SpvOpUGreaterThan; break;
    case LLVMIntUGE: op = SpvOpUGreaterThanEqual; break;
    case LLVMIntULT: op = SpvOpULessThan; break;
    case LLVMIntULE: op = SpvOpULessThanEqual; break;
    case LLVMIntSGT: op = SpvOpSGreaterThan; break;
    case LLVMIntSGE: op = SpvOpSGreaterThanEqual; break;
    case LLVMIntSLT: op = SpvOpSLessThan; break;
    default: op = SpvOpSLessThanEqual; break;
    }
    return a2s_op2(a, op, bt, x, y);
  }
}

// a pointer through GEP: offsets summed, constant parts folded
static void gep(struct a2s *a, LLVMValueRef in) {
  LLVMTypeRef ty = LLVMGetGEPSourceElementType(in);
  struct aptr p = a2s_ptr(a, LLVMGetOperand(in, 0));
  unsigned n = (unsigned)LLVMGetNumOperands(in), i;
  uint32_t ut = spv_t_uint(&a->s);

  for (i = 1; i < n; i++) {
    LLVMValueRef ix = LLVMGetOperand(in, i);
    uint64_t stride;

    if (i > 1 && LLVMGetTypeKind(ty) == LLVMStructTypeKind) {
      unsigned k = (unsigned)LLVMConstIntGetZExtValue(ix);
      p.cst += (uint32_t)LLVMOffsetOfElement(a->td, ty, k);
      ty = LLVMStructGetTypeAtIndex(ty, k);
      continue;
    }

    if (i > 1)
      ty = LLVMGetElementType(ty);

    stride = LLVMABISizeOfType(a->td, ty);
    if (LLVMIsAConstantInt(ix)) {
      p.cst += (uint32_t)((uint64_t)LLVMConstIntGetSExtValue(ix) * stride);
    } else {
      uint32_t v = a2s_value(a, ix), off;

      // an index narrower than 32 bits is signed: widen it first
      if (a2s_bits(LLVMTypeOf(ix)) < 32)
        v = a2s_sext(a, LLVMTypeOf(ix), v);
      off = stride == 1 ? v : a2s_op2(a, SpvOpIMul, ut, v, spv_c_u32(&a->s, (uint32_t)stride));
      p.dyn = p.dyn ? a2s_op2(a, SpvOpIAdd, ut, p.dyn, off) : off;
    }
  }

  set_ptr(a, in, p);
}

static uint32_t cast(struct a2s *a, LLVMValueRef in, LLVMOpcode op) {
  LLVMValueRef src = LLVMGetOperand(in, 0);
  LLVMTypeRef st = LLVMTypeOf(src), dt = LLVMTypeOf(in);
  uint32_t v = a2s_value(a, src), ty = a2s_type(a, dt);
  int sb = a2s_bits(st), db = a2s_bits(dt);

  switch (op) {
  case LLVMZExt:
    if (is_bool(st))
      return a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ v, a2s_uconst(a, dt, 1), a2s_uconst(a, dt, 0) }, 3);
    return v;
  case LLVMSExt:
    if (is_bool(st))
      return a2s_op(a, SpvOpSelect, ty,
                    (uint32_t[]){ v, a2s_uconst(a, dt, db >= 32 ? 0xffffffffu : (1u << db) - 1), a2s_uconst(a, dt, 0) }, 3);
    return a2s_mask(a, dt, a2s_sext(a, st, v));
  case LLVMTrunc:
    if (is_bool(dt))
      return a2s_bool_of(a, st, a2s_op2(a, SpvOpBitwiseAnd, a2s_type(a, st), v, a2s_uconst(a, st, 1)));
    if (sb > 32 && db == 32)
      return v;
    return a2s_mask(a, dt, v);
  case LLVMFPExt:
    return v;
  case LLVMFPTrunc:
    return db == 16 ? a2s_quantize(a, dt, v) : v;
  case LLVMFPToUI:
    return a2s_mask(a, dt, a2s_op1(a, SpvOpConvertFToU, ty, v));
  case LLVMFPToSI:
    return a2s_mask(a, dt, a2s_op1(a, SpvOpBitcast, ty, a2s_op1(a, SpvOpConvertFToS, spv_t_vec(&a->s, spv_t_int(&a->s), a2s_lanes(dt)), v)));
  case LLVMUIToFP:
    if (is_bool(st))
      return a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ v, a2s_fconst(a, dt, 1), a2s_fconst(a, dt, 0) }, 3);
    v = a2s_op1(a, SpvOpConvertUToF, ty, v);
    return db == 16 ? a2s_quantize(a, dt, v) : v;
  case LLVMSIToFP:
    if (is_bool(st))
      return a2s_op(a, SpvOpSelect, ty, (uint32_t[]){ v, a2s_fconst(a, dt, -1), a2s_fconst(a, dt, 0) }, 3);
    v = a2s_op1(a, SpvOpConvertSToF, ty, a2s_sext(a, st, v));
    return db == 16 ? a2s_quantize(a, dt, v) : v;
  case LLVMBitCast:
    return a2s_bitcast(a, st, dt, v);
  default:
    a2s_fail(a, "cast opcode %d", (int)op);
  }
}

static uint32_t binop(struct a2s *a, LLVMValueRef in, LLVMOpcode op) {
  LLVMTypeRef t = LLVMTypeOf(in);
  uint32_t x = a2s_value(a, LLVMGetOperand(in, 0)), y = a2s_value(a, LLVMGetOperand(in, 1));
  uint32_t ty = a2s_type(a, t), r;
  int half = LLVMGetTypeKind(a2s_scalar(t)) == LLVMHalfTypeKind;

  if (is_bool(t)) {
    switch (op) {
    case LLVMAnd: return a2s_op2(a, SpvOpLogicalAnd, ty, x, y);
    case LLVMOr: return a2s_op2(a, SpvOpLogicalOr, ty, x, y);
    case LLVMXor: return a2s_op2(a, SpvOpLogicalNotEqual, ty, x, y);
    default: a2s_fail(a, "i1 arithmetic opcode %d", (int)op);
    }
  }

  switch (op) {
  case LLVMFAdd: r = a2s_op2(a, SpvOpFAdd, ty, x, y); break;
  case LLVMFSub: r = a2s_op2(a, SpvOpFSub, ty, x, y); break;
  case LLVMFMul: r = a2s_op2(a, SpvOpFMul, ty, x, y); break;
  case LLVMFDiv: r = a2s_op2(a, SpvOpFDiv, ty, x, y); break;
  case LLVMFRem: r = a2s_op2(a, SpvOpFRem, ty, x, y); break;
  case LLVMAdd: return a2s_mask(a, t, a2s_op2(a, SpvOpIAdd, ty, x, y));
  case LLVMSub: return a2s_mask(a, t, a2s_op2(a, SpvOpISub, ty, x, y));
  case LLVMMul: return a2s_mask(a, t, a2s_op2(a, SpvOpIMul, ty, x, y));
  case LLVMShl: return a2s_mask(a, t, a2s_op2(a, SpvOpShiftLeftLogical, ty, x, y));
  case LLVMLShr: return a2s_op2(a, SpvOpShiftRightLogical, ty, x, y);
  case LLVMAShr: return a2s_mask(a, t, a2s_op2(a, SpvOpShiftRightArithmetic, ty, a2s_sext(a, t, x), y));
  case LLVMAnd: return a2s_op2(a, SpvOpBitwiseAnd, ty, x, y);
  case LLVMOr: return a2s_op2(a, SpvOpBitwiseOr, ty, x, y);
  case LLVMXor: return a2s_op2(a, SpvOpBitwiseXor, ty, x, y);
  case LLVMUDiv: return a2s_op2(a, SpvOpUDiv, ty, x, y);
  case LLVMURem: return a2s_op2(a, SpvOpUMod, ty, x, y);
  case LLVMSDiv: return a2s_mask(a, t, a2s_op2(a, SpvOpSDiv, ty, a2s_sext(a, t, x), a2s_sext(a, t, y)));
  case LLVMSRem: return a2s_mask(a, t, a2s_op2(a, SpvOpSRem, ty, a2s_sext(a, t, x), a2s_sext(a, t, y)));
  default: a2s_fail(a, "binary opcode %d", (int)op);
  }

  return half ? a2s_quantize(a, t, r) : r;
}

static uint32_t shuffle(struct a2s *a, LLVMValueRef in) {
  LLVMValueRef v1 = LLVMGetOperand(in, 0), v2 = LLVMGetOperand(in, 1);
  unsigned n = LLVMGetNumMaskElements(in), n1 = a2s_lanes(LLVMTypeOf(v1)), i;
  uint32_t w[8], x = a2s_value(a, v1), y = a2s_value(a, v2), ty = a2s_type(a, LLVMTypeOf(in));

  // a one lane result is a scalar, a one lane source is not a vector either
  if (n1 == 1 || n == 1) {
    uint32_t parts[4];
    for (i = 0; i < n; i++) {
      int m = LLVMGetMaskValue(in, i);
      uint32_t src = m >= (int)n1 ? y : x, lane = (uint32_t)(m >= (int)n1 ? m - (int)n1 : m), e[2] = { src, lane };

      if (m == LLVMGetUndefMaskElem())
        parts[i] = spv_c_undef(&a->s, a2s_type(a, a2s_scalar(LLVMTypeOf(in))));
      else if (n1 == 1)
        parts[i] = src;
      else
        parts[i] = a2s_op(a, SpvOpCompositeExtract, a2s_type(a, a2s_scalar(LLVMTypeOf(in))), e, 2);
    }
    return n == 1 ? parts[0] : a2s_op(a, SpvOpCompositeConstruct, ty, parts, n);
  }

  w[0] = x, w[1] = y;
  for (i = 0; i < n; i++) {
    int m = LLVMGetMaskValue(in, i);
    w[2 + i] = m == LLVMGetUndefMaskElem() ? 0xffffffffu : (uint32_t)m;
  }

  return a2s_op(a, SpvOpVectorShuffle, ty, w, 2 + n);
}

static void indices(LLVMValueRef in, uint32_t *w, unsigned *n) {
  unsigned k = LLVMGetNumIndices(in);
  const unsigned *ix = LLVMGetIndices(in);

  for (unsigned i = 0; i < k; i++) {
    w[(*n)++] = ix[i];
  }
}

static int has_ptr(LLVMTypeRef t) {
  LLVMTypeKind k = LLVMGetTypeKind(t);

  if (k == LLVMPointerTypeKind)
    return 1;
  if (k == LLVMArrayTypeKind)
    return has_ptr(LLVMGetElementType(t));
  if (k == LLVMStructTypeKind) {
    for (unsigned i = 0; i < LLVMCountStructElementTypes(t); i++) {
      if (has_ptr(LLVMStructGetTypeAtIndex(t, i)))
        return 1;
    }
  }
  return 0;
}

// aggregates holding pointers have no SPIR-V value: an extract is followed back through the inserts
// and extracts that built it, to the member it names
static void extract_ptr_agg(struct a2s *a, LLVMValueRef in) {
  unsigned path[16], np = LLVMGetNumIndices(in), i;
  LLVMValueRef agg = LLVMGetOperand(in, 0);
  const unsigned *ix = LLVMGetIndices(in);
  struct vslot *d, *s;

  if (np > 16)
    a2s_fail(a, "extract path too deep");
  memcpy(path, ix, np * sizeof(unsigned));

  while (np) {
    if (LLVMIsAInsertValueInst(agg)) {
      unsigned ni = LLVMGetNumIndices(agg);
      const unsigned *ip = LLVMGetIndices(agg);
      unsigned common = ni < np ? ni : np;

      for (i = 0; i < common && ip[i] == path[i]; i++) {
      }

      if (i < common) {
        agg = LLVMGetOperand(agg, 0);
      } else if (ni <= np) {
        agg = LLVMGetOperand(agg, 1);
        memmove(path, path + ni, (np - ni) * sizeof(unsigned));
        np -= ni;
      } else {
        // a part of what the insert put in: what is left of it is found by later extracts
        break;
      }
    } else if (LLVMIsAExtractValueInst(agg)) {
      unsigned ne = LLVMGetNumIndices(agg);
      if (ne + np > 16)
        a2s_fail(a, "extract path too deep");
      memmove(path + ne, path, np * sizeof(unsigned));
      memcpy(path, LLVMGetIndices(agg), ne * sizeof(unsigned));
      np += ne;
      agg = LLVMGetOperand(agg, 0);
    } else {
      a2s_fail(a, "aggregate with pointers built by something other than insertvalue");
    }
  }

  if (np) {
    d = a2s_slot(a, in, 1);
    d->kind = VK_AGG;
    return;
  }

  s = a2s_slot(a, agg, 0);
  if (!s) {
    a2s_set(a, in, a2s_value(a, agg));
    return;
  }
  {
    struct vslot copy = *s;
    d = a2s_slot(a, in, 1);
    copy.k = in;
    *d = copy;
  }
}

static int is_handle_ptr(struct a2s *a, LLVMValueRef v) {
  struct vslot *s = a2s_slot(a, v, 0);

  return s && s->kind == VK_HANDLE;
}

void a2s_inst(struct a2s *a, LLVMValueRef in) {
  LLVMOpcode op = LLVMGetInstructionOpcode(in);
  LLVMTypeRef t = LLVMTypeOf(in);
  uint32_t w[16], r = 0;
  unsigned n = 0;

  switch (op) {
  case LLVMFNeg:
    r = a2s_op1(a, SpvOpFNegate, a2s_type(a, t), a2s_value(a, LLVMGetOperand(in, 0)));
    break;
  case LLVMFAdd: case LLVMFSub: case LLVMFMul: case LLVMFDiv: case LLVMFRem:
  case LLVMAdd: case LLVMSub: case LLVMMul: case LLVMShl: case LLVMLShr: case LLVMAShr:
  case LLVMAnd: case LLVMOr: case LLVMXor: case LLVMUDiv: case LLVMURem: case LLVMSDiv: case LLVMSRem:
    r = binop(a, in, op);
    break;
  case LLVMICmp:
  case LLVMFCmp: {
    LLVMValueRef x = LLVMGetOperand(in, 0), y = LLVMGetOperand(in, 1);
    LLVMTypeRef ot = LLVMTypeOf(x);

    // pointers into the same memory compare as byte offsets, null as A2S_NULL_BYTES
    if (LLVMGetTypeKind(ot) == LLVMPointerTypeKind) {
      if (!LLVMIsAConstantPointerNull(x) && !LLVMIsAConstantPointerNull(y) &&
          a2s_ptr(a, x).root != a2s_ptr(a, y).root)
        a2s_fail(a, "comparison of pointers into different memory");
      r = cmp_op(a, in, LLVMInt32TypeInContext(a->ctx), a2s_ptr_bytes(a, x), a2s_ptr_bytes(a, y));
      break;
    }
    r = cmp_op(a, in, ot, a2s_value(a, x), a2s_value(a, y));
    break;
  }
  case LLVMSelect: {
    LLVMValueRef tv = LLVMGetOperand(in, 1);

    if (LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
      LLVMValueRef fv = LLVMGetOperand(in, 2);
      int n1 = LLVMIsAConstantPointerNull(tv) != 0, n2 = LLVMIsAConstantPointerNull(fv) != 0;
      uint32_t c = a2s_value(a, LLVMGetOperand(in, 0)), ut = spv_t_uint(&a->s);
      struct aptr p = { 0, 0, 0 };

      if (n1 && n2)
        a2s_fail(a, "select between null pointers");
      p.root = a2s_ptr(a, n1 ? fv : tv).root;
      if (!n1 && !n2 && a2s_ptr(a, fv).root != p.root)
        a2s_fail(a, "select between pointers into different memory");

      w[0] = c, w[1] = a2s_ptr_bytes(a, tv), w[2] = a2s_ptr_bytes(a, fv);
      p.dyn = a2s_op(a, SpvOpSelect, ut, w, 3);
      set_ptr(a, in, p);
      return;
    }

    w[0] = a2s_value(a, LLVMGetOperand(in, 0)), w[1] = a2s_value(a, tv), w[2] = a2s_value(a, LLVMGetOperand(in, 2));
    r = a2s_op(a, SpvOpSelect, a2s_type(a, t), w, 3);
    break;
  }
  case LLVMZExt: case LLVMSExt: case LLVMTrunc: case LLVMFPExt: case LLVMFPTrunc:
  case LLVMFPToUI: case LLVMFPToSI: case LLVMUIToFP: case LLVMSIToFP:
    r = cast(a, in, op);
    break;
  case LLVMBitCast:
  case LLVMAddrSpaceCast:
    if (LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
      struct vslot *s = a2s_slot(a, LLVMGetOperand(in, 0), 0);

      // pointers keep what they point at: memory, or a texture or sampler handle
      if (s && s->kind == VK_HANDLE) {
        struct vslot *d = a2s_slot(a, in, 1);
        d->kind = VK_HANDLE, d->u.h = s->u.h;
      } else {
        set_ptr(a, in, a2s_ptr(a, LLVMGetOperand(in, 0)));
      }
      return;
    }
    r = cast(a, in, LLVMBitCast);
    break;
  case LLVMFreeze:
    r = a2s_value(a, LLVMGetOperand(in, 0));
    break;
  case LLVMExtractElement: {
    LLVMValueRef vec = LLVMGetOperand(in, 0), ix = LLVMGetOperand(in, 1);

    if (a2s_lanes(LLVMTypeOf(vec)) == 1) {
      r = a2s_value(a, vec);
    } else if (LLVMIsAConstantInt(ix)) {
      w[0] = a2s_value(a, vec), w[1] = (uint32_t)LLVMConstIntGetZExtValue(ix);
      r = a2s_op(a, SpvOpCompositeExtract, a2s_type(a, t), w, 2);
    } else {
      r = a2s_op2(a, SpvOpVectorExtractDynamic, a2s_type(a, t), a2s_value(a, vec), a2s_value(a, ix));
    }
    break;
  }
  case LLVMInsertElement: {
    LLVMValueRef ix = LLVMGetOperand(in, 2);

    if (a2s_lanes(t) == 1) {
      r = a2s_value(a, LLVMGetOperand(in, 1));
    } else if (LLVMIsAConstantInt(ix)) {
      w[0] = a2s_value(a, LLVMGetOperand(in, 1)), w[1] = a2s_value(a, LLVMGetOperand(in, 0));
      w[2] = (uint32_t)LLVMConstIntGetZExtValue(ix);
      r = a2s_op(a, SpvOpCompositeInsert, a2s_type(a, t), w, 3);
    } else {
      w[0] = a2s_value(a, LLVMGetOperand(in, 0)), w[1] = a2s_value(a, LLVMGetOperand(in, 1)), w[2] = a2s_value(a, ix);
      r = a2s_op(a, SpvOpVectorInsertDynamic, a2s_type(a, t), w, 3);
    }
    break;
  }
  case LLVMShuffleVector:
    r = shuffle(a, in);
    break;
  case LLVMExtractValue:
    if (has_ptr(LLVMTypeOf(LLVMGetOperand(in, 0)))) {
      extract_ptr_agg(a, in);
      return;
    }
    w[n++] = a2s_value(a, LLVMGetOperand(in, 0));
    indices(in, w, &n);
    r = a2s_op(a, SpvOpCompositeExtract, a2s_type(a, t), w, n);
    break;
  case LLVMInsertValue:
    if (has_ptr(t)) {
      a2s_slot(a, in, 1)->kind = VK_AGG;
      return;
    }
    w[n++] = a2s_value(a, LLVMGetOperand(in, 1));
    w[n++] = a2s_value(a, LLVMGetOperand(in, 0));
    indices(in, w, &n);
    r = a2s_op(a, SpvOpCompositeInsert, a2s_type(a, t), w, n);
    break;
  case LLVMGetElementPtr:
    gep(a, in);
    return;
  case LLVMAlloca: {
    LLVMTypeRef at = LLVMGetAllocatedType(in);
    uint32_t bytes = (uint32_t)LLVMABISizeOfType(a->td, at);
    struct aptr p = { a2s_add_root(a, ROOT_FN, (bytes + 3) / 4), 0, 0 };

    set_ptr(a, in, p);
    return;
  }
  case LLVMLoad: {
    LLVMValueRef pv = LLVMGetOperand(in, 0);

    if (is_handle_ptr(a, pv))
      a2s_fail(a, "load through a texture or sampler handle");
    r = a2s_load_mem(a, t, a2s_ptr(a, pv), LLVMGetAlignment(in));
    break;
  }
  case LLVMStore: {
    LLVMValueRef v = LLVMGetOperand(in, 0);
    a2s_store_mem(a, LLVMTypeOf(v), a2s_value(a, v), a2s_ptr(a, LLVMGetOperand(in, 1)), LLVMGetAlignment(in));
    return;
  }
  case LLVMCall: {
    LLVMValueRef callee = LLVMGetCalledValue(in);
    size_t len = 0;
    const char *name = LLVMGetValueName2(callee, &len);
    char nm[128];

    snprintf(nm, sizeof(nm), "%.*s", (int)len, name);
    if (!strncmp(nm, "llvm.lifetime.", 14) || !strncmp(nm, "llvm.dbg.", 9))
      return;
    // fmuladd may fuse, so it is the same Fma as air.fma
    if (!strncmp(nm, "llvm.fmuladd.", 13) || !strncmp(nm, "llvm.fma.", 9)) {
      char an[140];

      snprintf(an, sizeof(an), "air.fma.%s", strchr(nm + 5, '.') + 1);
      if (a2s_air_call(a, in, an))
        return;
    }
    if (a2s_air_call(a, in, nm))
      return;
    a2s_fail(a, "call to %s", nm);
  }
  default:
    a2s_fail(a, "instruction opcode %d", (int)op);
  }

  a2s_set(a, in, r);
}
