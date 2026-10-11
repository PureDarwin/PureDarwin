#include <spirv/unified1/GLSL.std.450.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a2s.h"

static const char *const entry_lists[] = { "air.vertex", "air.fragment", "air.kernel" };

// binding numbers inside the stage's set: Metal's index plus a base per kind
enum { BIND_BUFFER = 0, BIND_TEXTURE = 32, BIND_SAMPLER = 64, BIND_CONST_SAMPLER = 80, BIND_ATTACHMENT = 96 };

static int md_ops(LLVMValueRef node, LLVMValueRef *out, unsigned cap) {
  unsigned n = LLVMGetMDNodeNumOperands(node);

  if (n > cap)
    return -1;

  LLVMGetMDNodeOperands(node, out);
  return (int)n;
}

static const char *md_str(LLVMValueRef v, char *buf, size_t len) {
  unsigned l = 0;
  const char *s = v ? LLVMGetMDString(v, &l) : 0;

  if (!s)
    return 0;

  snprintf(buf, len, "%.*s", (int)l, s);
  return buf;
}

static int md_int(LLVMValueRef v, unsigned *out) {
  if (!v || !LLVMIsAConstantInt(v))
    return 0;

  *out = (unsigned)LLVMConstIntGetZExtValue(v);
  return 1;
}

// the entry node {fn, outputs, args} for a->fn and which list it is in
static int entry_node(struct a2s *a, LLVMValueRef *ops) {
  for (unsigned k = 0; k < A2S_ARRAY_LEN(entry_lists); k++) {
    unsigned n = LLVMGetNamedMetadataNumOperands(a->mod, entry_lists[k]);
    LLVMValueRef eps[64];

    if (!n || n > 64)
      continue;

    LLVMGetNamedMetadataOperands(a->mod, entry_lists[k], eps);
    for (unsigned e = 0; e < n; e++) {
      if (md_ops(eps[e], ops, 8) >= 3 && ops[0] == a->fn)
        return (int)k;
    }
  }

  return -1;
}

uint32_t a2s_input_var(struct a2s *a, uint32_t type, SpvBuiltIn bi, int location) {
  uint32_t v;

  // one variable per builtin: an entry point may not list two
  if (location < 0) {
    for (int i = 0; i < a->nbuiltins; i++) {
      if (a->builtins[i].bi == (uint32_t)bi)
        return a->builtins[i].var;
    }
  }

  v = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassInput, type), SpvStorageClassInput);
  if (location < 0 && a->nbuiltins < (int)A2S_ARRAY_LEN(a->builtins))
    a->builtins[a->nbuiltins].bi = (uint32_t)bi, a->builtins[a->nbuiltins++].var = v;

  if (location >= 0)
    spv_deco(&a->s, v, SpvDecorationLocation, 1, (uint32_t)location);
  else
    spv_deco(&a->s, v, SpvDecorationBuiltIn, 1, bi);

  a->iface[a->niface++] = v;
  return v;
}

static uint32_t pro_op(struct a2s *a, unsigned op, uint32_t type, const uint32_t *ops, unsigned n) {
  uint32_t w[16], id = spv_id(&a->s);

  w[0] = type, w[1] = id;
  memcpy(w + 2, ops, n * 4);
  spv_emit(&a->pro, op, w, n + 2);
  return id;
}

// a 3 lane builtin as the parameter's own type: uint, uint2, uint3, ushort2 ...
static uint32_t builtin_param(struct a2s *a, LLVMTypeRef pt, uint32_t uvec3, int scalar) {
  unsigned n = a2s_lanes(pt), i;
  uint32_t ut = spv_t_uint(&a->s), parts[3], w[2];

  // a scalar builtin (an index) fills the one lane
  for (i = 0; i < n && i < 3; i++) {
    w[0] = uvec3, w[1] = i;
    parts[i] = scalar ? uvec3 : pro_op(a, SpvOpCompositeExtract, ut, w, 2);
    if (a2s_bits(pt) < 32) {
      uint32_t m[2] = { parts[i], spv_c_u32(&a->s, (1u << a2s_bits(pt)) - 1) };
      parts[i] = pro_op(a, SpvOpBitwiseAnd, ut, m, 2);
    }
  }

  return n == 1 ? parts[0] : pro_op(a, SpvOpCompositeConstruct, a2s_type(a, pt), parts, n);
}

static uint32_t builtin_vec3(struct a2s *a, SpvBuiltIn bi) {
  uint32_t t = spv_t_vec(&a->s, spv_t_uint(&a->s), 3), v = a2s_input_var(a, t, bi, -1);

  return pro_op(a, SpvOpLoad, t, &v, 1);
}

static uint32_t builtin_u32(struct a2s *a, SpvBuiltIn bi) {
  uint32_t t = spv_t_uint(&a->s), v = a2s_input_var(a, t, bi, -1);

  return pro_op(a, SpvOpLoad, t, &v, 1);
}

static struct a2s_resource *add_res(struct a2s *a, enum a2s_res_kind kind, unsigned metal, unsigned binding) {
  struct a2s_resource *r;

  if (a->r->nres == A2S_MAX_RES)
    a2s_fail(a, "too many resources");

  r = &a->r->res[a->r->nres++];
  memset(r, 0, sizeof(*r));
  r->kind = kind, r->metal = metal, r->set = a->opt->set, r->binding = binding;
  return r;
}

static void bind(struct a2s *a, uint32_t var, unsigned binding) {
  spv_deco(&a->s, var, SpvDecorationDescriptorSet, 1, a->opt->set);
  spv_deco(&a->s, var, SpvDecorationBinding, 1, binding);
}

static void set_handle(struct a2s *a, LLVMValueRef v, uint32_t var, uint32_t type, int res) {
  struct vslot *s = a2s_slot(a, v, 1);

  s->kind = VK_HANDLE, s->u.h.var = var, s->u.h.type = type, s->u.h.res = res;
}

// texture2d<half, sample>: element and dimension from the type name, storage from the access
#define A2S_LEVELS 16

// a write to this texture names a mip level that is not the constant 0
static int writes_levels(LLVMValueRef p) {
  for (LLVMUseRef u = LLVMGetFirstUse(p); u; u = LLVMGetNextUse(u)) {
    LLVMValueRef in = LLVMGetUser(u), callee, lod;
    size_t l = 0;
    const char *n;
    unsigned at;

    if (!LLVMIsACallInst(in) || !(callee = LLVMGetCalledValue(in)))
      continue;
    n = LLVMGetValueName2(callee, &l);
    // the lod operand: after the colour, which follows a slice or face for arrays and cubes
    at = !strncmp(n, "air.write_texture_2d_array", 26) || !strncmp(n, "air.write_texture_cube", 22) ? 4
         : !strncmp(n, "air.write_texture_", 18)                                                 ? 3
         : !strncmp(n, "air.write_imageblock_slice", 26)                                         ? 6
                                                                                                 : 0;
    if (!at || (unsigned)LLVMGetNumArgOperands(in) <= at)
      continue;
    lod = LLVMGetOperand(in, at);
    if (!LLVMIsAConstantInt(lod) || LLVMConstIntGetZExtValue(lod))
      return 1;
  }

  return 0;
}

// a call to an air.* function whose name starts with prefix takes p
static int used_by(LLVMValueRef p, const char *prefix) {
  for (LLVMUseRef u = LLVMGetFirstUse(p); u; u = LLVMGetNextUse(u)) {
    LLVMValueRef in = LLVMGetUser(u), callee;
    size_t l = 0;

    if (LLVMIsACallInst(in) && (callee = LLVMGetCalledValue(in)) && !strncmp(LLVMGetValueName2(callee, &l), prefix, strlen(prefix)))
      return 1;
  }
  return 0;
}

static void texture_param(struct a2s *a, LLVMValueRef p, unsigned loc, const char *tname, unsigned access) {
  struct a2s_resource *r = add_res(a, A2S_RES_TEXTURE, loc, BIND_TEXTURE + loc);
  const char *lt = strchr(tname, '<');
  uint32_t st, it, var;
  int cube = !strncmp(tname, "texturecube", 11), arrayed = strstr(tname, "_array") != 0, as_array = 0;
  SpvDim dim = !strncmp(tname, "texture3d", 9) ? SpvDim3D : cube ? SpvDimCube : SpvDim2D;

  if (!strncmp(tname, "depth", 5) || !strncmp(tname, "texture1d", 9))
    a2s_note(a, "texture type %s treated as 2D", tname);

  r->elem = lt && !strncmp(lt + 1, "uint", 4) ? 'u' : lt && !strncmp(lt + 1, "int", 3) ? 's' : 'f';
  r->dim = dim == SpvDim3D ? 3 : 2;
  r->access = access;
  r->storage = (access & A2S_WRITE) != 0;
  // a cube read or written by texel is its faces as a 2D array: Vulkan fetches no texels from cube views
  if (cube && (r->storage || used_by(p, "air.read_texture_cube"))) {
    if (used_by(p, "air.sample_texture_cube"))
      a2s_fail(a, "cube texture %u both sampled and read by texel", loc);
    as_array = 1, dim = SpvDim2D;
  }
  r->cube = cube && !as_array, r->arrayed = arrayed || as_array;
  if (cube && arrayed && !as_array)
    spv_cap(&a->s, r->storage ? SpvCapabilityImageCubeArray : SpvCapabilitySampledCubeArray);
  st = r->elem == 'u' ? spv_t_uint(&a->s) : r->elem == 's' ? spv_t_int(&a->s) : spv_t_float(&a->s);
  it = spv_t_image_arrayed(&a->s, st, dim, r->arrayed, r->storage ? 2 : 1, SpvImageFormatUnknown);
  r->levels = r->storage && writes_levels(p) ? A2S_LEVELS : 0;
  var = spv_var(&a->s,
                spv_t_ptr(&a->s, SpvStorageClassUniformConstant,
                          r->levels ? spv_t_array(&a->s, it, spv_c_u32(&a->s, r->levels)) : it),
                SpvStorageClassUniformConstant);
  bind(a, var, r->binding);
  if (r->storage && !(access & A2S_READ))
    spv_deco(&a->s, var, SpvDecorationNonReadable, 0);
  if (r->storage && (access & A2S_WRITE))
    spv_cap(&a->s, SpvCapabilityStorageImageWriteWithoutFormat);
  if (r->storage && (access & A2S_READ))
    spv_cap(&a->s, SpvCapabilityStorageImageReadWithoutFormat);
  a->iface[a->niface++] = var;
  set_handle(a, p, var, it, (int)a->r->nres - 1);
  a2s_slot(a, p, 0)->u.h.levels = r->levels;
  a2s_slot(a, p, 0)->u.h.arrayed = r->arrayed, a2s_slot(a, p, 0)->u.h.cube = cube ? (arrayed ? 3 : 2) : 0;
  a2s_slot(a, p, 0)->u.h.three = dim == SpvDim3D, a2s_slot(a, p, 0)->u.h.cube_as_array = as_array;
}

// a stage_in attribute: a 4 lane input at its attribute index, missing lanes 0, 0, 0, 1 as in Metal. Integer
// ones come in unsigned, so a signed narrow format in 32 bit lanes is not sign extended
static void vertex_input(struct a2s *a, LLVMValueRef p, unsigned loc) {
  LLVMTypeRef pt = LLVMTypeOf(p), et = a2s_scalar(pt);
  int isint = LLVMGetTypeKind(et) == LLVMIntegerTypeKind;
  uint32_t st = isint ? spv_t_uint(&a->s) : spv_t_float(&a->s), vt = spv_t_vec(&a->s, st, 4), var, v;
  unsigned n = a2s_lanes(pt);
  struct a2s_vin *vi;

  if (a->r->nvin == A2S_ARRAY_LEN(a->r->vin))
    a2s_fail(a, "too many vertex attributes");
  vi = &a->r->vin[a->r->nvin++];
  vi->location = loc, vi->elem = isint ? 'u' : 'f';
  var = a2s_input_var(a, vt, 0, (int)loc);
  v = pro_op(a, SpvOpLoad, vt, &var, 1);
  if (n < 4) {
    uint32_t sh[6] = { v, v, 0, 1, 2 };
    v = n == 1 ? pro_op(a, SpvOpCompositeExtract, st, (uint32_t[]){ v, 0 }, 2)
               : pro_op(a, SpvOpVectorShuffle, spv_t_vec(&a->s, st, n), sh, 2 + n);
  }
  // halves and narrow integers live in 32 bit lanes here already
  a2s_set(a, p, v);
}

static void sampler_var(struct a2s *a, LLVMValueRef p, enum a2s_res_kind kind, unsigned metal, unsigned binding,
                        uint64_t state) {
  struct a2s_resource *r = add_res(a, kind, metal, binding);
  uint32_t st = spv_t_sampler(&a->s), var;

  r->state = state;
  var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassUniformConstant, st), SpvStorageClassUniformConstant);
  bind(a, var, binding);
  a->iface[a->niface++] = var;
  set_handle(a, p, var, st, (int)a->r->nres - 1);

  // vulkan's unnormalized samplers forbid implicit lod: texel coordinates are divided by the size here
  if (kind == A2S_RES_CONST_SAMPLER) {
    struct a2s_sampler_desc d;
    a2s_sampler_state(state, &d);
    a2s_slot(a, p, 0)->u.h.pixel_const = d.pixel;
  } else {
    uint32_t id = spv_id(&a->s);
    spv_op(&a->s.globals, SpvOpSpecConstantFalse, 2, spv_t_bool(&a->s), id);
    spv_deco(&a->s, id, SpvDecorationSpecId, 1, A2S_SPEC_SAMPLER_PIXEL + metal);
    r->spec_id = A2S_SPEC_SAMPLER_PIXEL + metal;
    a2s_slot(a, p, 0)->u.h.pixel_spec = id;
  }
}


// a constexpr sampler's state: an i64 (Xcode 13, bit 63 set) or [2 x i64] whose first word holds it (Xcode 26)
static uint64_t a2s_const_sampler_state(LLVMValueRef init) {
  if (init && !LLVMIsAConstantInt(init) && LLVMGetTypeKind(LLVMTypeOf(init)) == LLVMArrayTypeKind)
    init = LLVMGetAggregateElement(init, 0);
  return init && LLVMIsAConstantInt(init) ? LLVMConstIntGetZExtValue(init) : 0;
}

// constexpr samplers are internal globals __air_sampler_state*: each gets its own binding
static void const_samplers(struct a2s *a) {
  unsigned k = 0;

  for (LLVMValueRef g = LLVMGetFirstGlobal(a->mod); g; g = LLVMGetNextGlobal(g)) {
    size_t l = 0;
    const char *n = LLVMGetValueName2(g, &l);
    LLVMValueRef init = LLVMGetInitializer(g);

    if (l < 19 || strncmp(n, "__air_sampler_state", 19))
      continue;

    sampler_var(a, g, A2S_RES_CONST_SAMPLER, k, BIND_CONST_SAMPLER + k, a2s_const_sampler_state(init));
    k++;
  }
}

// threadgroup globals: a word array each
static void wg_globals(struct a2s *a) {
  for (LLVMValueRef g = LLVMGetFirstGlobal(a->mod); g; g = LLVMGetNextGlobal(g)) {
    LLVMTypeRef vt = LLVMGlobalGetValueType(g);
    unsigned as = LLVMGetPointerAddressSpace(LLVMTypeOf(g));
    struct aptr p = { 0, 0, 0 };
    struct vslot *s;

    if (as != 3 || !LLVMGetFirstUse(g))
      continue;

    p.root = a2s_add_root(a, ROOT_WG, (uint32_t)((LLVMABISizeOfType(a->td, vt) + 3) / 4));
    s = a2s_slot(a, g, 1);
    s->kind = VK_PTR, s->u.p = p;
  }
}

static void buffer_param(struct a2s *a, LLVMValueRef p, unsigned loc, unsigned access, unsigned as) {
  struct a2s_resource *r;
  struct aptr ptr = { 0, 0, 0 };
  struct vslot *s;
  struct root *rt;

  if (!LLVMGetFirstUse(p))
    return;

  r = add_res(a, A2S_RES_BUFFER, loc, BIND_BUFFER + loc);
  r->access = access ? access : A2S_READ;
  if (a->opt->uniform_words &&
      (loc >= 32 || !a->opt->uniform_words[loc] || (r->access & A2S_WRITE)))
    a2s_fail(a, "buffer %u cannot use the read-only uniform backend", loc);
  ptr.root = a2s_add_root(a, ROOT_BUFFER, a->opt->uniform_words ? a->opt->uniform_words[loc] : 0);
  rt = &a->roots[ptr.root];
  bind(a, rt->var, r->binding);
  rt->binding = r->binding;
  rt->ro = !(r->access & A2S_WRITE) || as == 2;
  if (rt->ro)
    spv_deco(&a->s, rt->var, SpvDecorationNonWritable, 0);

  // the binding's byte offset comes in a push constant word
  ptr.dyn = a2s_pc_word(a, A2S_PC_BUFFER_OFFSET, loc);
  s = a2s_slot(a, p, 1);
  s->kind = VK_PTR, s->u.p = ptr;
}

// [[threadgroup(i)]]: a workgroup word array whose length (in words) is spec constant 12 + i, which the
// executor sets from setThreadgroupMemoryLength
static void tg_param(struct a2s *a, LLVMValueRef p, unsigned loc) {
  uint32_t ut = spv_t_uint(&a->s), len = spv_id(&a->s), arr;
  struct aptr ptr = { 0, 0, 0 };
  struct vslot *s;
  struct root *rt;

  if (!LLVMGetFirstUse(p))
    return;
  if (loc >= 4)
    a2s_fail(a, "threadgroup memory parameter %u", loc);
  spv_op(&a->s.globals, SpvOpSpecConstant, 3, ut, len, 1024);
  spv_deco(&a->s, len, SpvDecorationSpecId, 1, A2S_SPEC_TGMEM + loc);
  arr = spv_t_array(&a->s, ut, len);
  ptr.root = a2s_add_root(a, ROOT_WG, 0);
  rt = &a->roots[ptr.root];
  // the root's variable takes the spec sized array in place of the fixed one
  rt->var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassWorkgroup, arr), SpvStorageClassWorkgroup);
  a->iface[a->niface - 1] = rt->var;
  a->r->tg_params |= 1u << loc;
  s = a2s_slot(a, p, 1);
  s->kind = VK_PTR, s->u.p = ptr;
}

// a fragment input, varying or vertex output: float widened from half, integers flat
static uint32_t io_type(struct a2s *a, LLVMTypeRef t, char *elem) {
  *elem = LLVMGetTypeKind(a2s_scalar(t)) == LLVMIntegerTypeKind ? 'u' : 'f';
  return a2s_type(a, t);
}

static void fragment_input(struct a2s *a, LLVMValueRef p, const char *name, unsigned interp, int centroid,
                           int sample) {
  struct a2s_varying *v;
  LLVMTypeRef t = LLVMTypeOf(p);
  uint32_t ty, var;
  char elem;

  if (a->r->nvar == A2S_MAX_VARYINGS)
    a2s_fail(a, "too many fragment inputs");

  v = &a->r->var[a->r->nvar];
  memset(v, 0, sizeof(*v));
  snprintf(v->name, sizeof(v->name), "%s", name);
  ty = io_type(a, t, &elem);
  v->elem = elem, v->components = a2s_lanes(t), v->location = a->r->nvar;
  v->interp = elem != 'f' ? 2 : interp;
  var = a2s_input_var(a, ty, 0, (int)v->location);
  // location is the last word of the OpDecorate just written
  v->word = (uint32_t)a->s.annot.n - 1;
  if (v->interp == 1)
    spv_deco(&a->s, var, SpvDecorationNoPerspective, 0);
  if (v->interp == 2)
    spv_deco(&a->s, var, SpvDecorationFlat, 0);
  if (centroid)
    spv_deco(&a->s, var, SpvDecorationCentroid, 0);
  if (sample) {
    spv_deco(&a->s, var, SpvDecorationSample, 0);
    spv_cap(&a->s, SpvCapabilitySampleRateShading);
  }
  a->r->nvar++;
  a2s_set(a, p, pro_op(a, SpvOpLoad, ty, &var, 1));
}

// a module whose kernel talks about quads: Metal's quads are 2x2 blocks of a 2D threadgroup
static int uses_quads(struct a2s *a) {
  for (LLVMValueRef f = LLVMGetFirstFunction(a->mod); f; f = LLVMGetNextFunction(f)) {
    size_t l = 0;
    const char *n = LLVMGetValueName2(f, &l);
    if (l > 8 && !strncmp(n, "air.quad", 8) && LLVMGetFirstUse(f))
      return 1;
  }
  return 0;
}

// quad-major order: index i is lane i & 3 of quad i >> 2, quads row by row, so subgroup quads are Metal's
// 2x2 ones, in a workgroup that is part of a threadgroup positions follow from the global one
static void quad_layout(struct a2s *a) {
  uint32_t ut = spv_t_uint(&a->s), v3 = spv_t_vec(&a->s, ut, 3), li, q, lane, qw, wx, x, y, wg, w[3], local;
  uint32_t lx, ly, hx;

  if (a->qlocal)
    return;

  li = builtin_u32(a, SpvBuiltInLocalInvocationIndex);
  w[0] = a->wg_phys, w[1] = 0;
  wx = pro_op(a, SpvOpCompositeExtract, ut, w, 2);
  q = pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ li, spv_c_u32(&a->s, 2) }, 2);
  lane = pro_op(a, SpvOpBitwiseAnd, ut, (uint32_t[]){ li, spv_c_u32(&a->s, 3) }, 2);
  qw = pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ wx, spv_c_u32(&a->s, 1) }, 2);
  x = pro_op(a, SpvOpUMod, ut, (uint32_t[]){ q, qw }, 2);
  x = pro_op(a, SpvOpShiftLeftLogical, ut, (uint32_t[]){ x, spv_c_u32(&a->s, 1) }, 2);
  x = pro_op(a, SpvOpBitwiseOr, ut, (uint32_t[]){ x, pro_op(a, SpvOpBitwiseAnd, ut, (uint32_t[]){ lane, spv_c_u32(&a->s, 1) }, 2) }, 2);
  y = pro_op(a, SpvOpUDiv, ut, (uint32_t[]){ q, qw }, 2);
  y = pro_op(a, SpvOpShiftLeftLogical, ut, (uint32_t[]){ y, spv_c_u32(&a->s, 1) }, 2);
  y = pro_op(a, SpvOpBitwiseOr, ut, (uint32_t[]){ y, pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ lane, spv_c_u32(&a->s, 1) }, 2) }, 2);
  local = pro_op(a, SpvOpCompositeConstruct, v3, (uint32_t[]){ x, y, spv_c_u32(&a->s, 0) }, 3);
  wg = builtin_vec3(a, SpvBuiltInWorkgroupId);
  a->qglobal = pro_op(a, SpvOpIAdd, v3, (uint32_t[]){ pro_op(a, SpvOpIMul, v3, (uint32_t[]){ wg, a->wg_phys }, 2), local }, 2);
  a->qlocal = pro_op(a, SpvOpUMod, v3, (uint32_t[]){ a->qglobal, a->wg_size }, 2);
  a->lgroup = pro_op(a, SpvOpUDiv, v3, (uint32_t[]){ a->qglobal, a->wg_size }, 2);
  a->qlane = lane;
  // the index in the threadgroup, quad-major as well
  lx = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->qlocal, 0 }, 2);
  ly = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->qlocal, 1 }, 2);
  hx = pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->wg_size, 0 }, 2), spv_c_u32(&a->s, 1) }, 2);
  q = pro_op(a, SpvOpIAdd, ut, (uint32_t[]){ pro_op(a, SpvOpIMul, ut, (uint32_t[]){ pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ ly, spv_c_u32(&a->s, 1) }, 2), hx }, 2), pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ lx, spv_c_u32(&a->s, 1) }, 2) }, 2);
  a->lindex = pro_op(a, SpvOpBitwiseOr, ut, (uint32_t[]){ pro_op(a, SpvOpShiftLeftLogical, ut, (uint32_t[]){ q, spv_c_u32(&a->s, 2) }, 2), lane }, 2);
  a->r->quad_order = 1;
}

// a threadgroup (spec 0-2) without shared state may run as several workgroups (spec 8-10): its builtins
// follow from the global position, and an edge group of a dispatchThreads grid counts only its threads
static void logical_ids(struct a2s *a) {
  uint32_t ut = spv_t_uint(&a->s), v3 = spv_t_vec(&a->s, ut, 3), gid, l, li, g[3], grid, left;
  uint32_t lx, ly, lz, sx, sy;

  if (a->lpos)
    return;

  gid = builtin_vec3(a, SpvBuiltInGlobalInvocationId);
  l = a->wg_size;
  a->lpos = pro_op(a, SpvOpUMod, v3, (uint32_t[]){ gid, l }, 2);
  a->lgroup = pro_op(a, SpvOpUDiv, v3, (uint32_t[]){ gid, l }, 2);
  for (unsigned k = 0; k < 3; k++) {
    g[k] = a2s_pc_word(a, A2S_PC_GRID_X + k, 0);
  }
  grid = pro_op(a, SpvOpCompositeConstruct, v3, g, 3);
  left = pro_op(a, SpvOpISub, v3, (uint32_t[]){ grid, pro_op(a, SpvOpIMul, v3, (uint32_t[]){ a->lgroup, l }, 2) }, 2);
  a->lsize = pro_op(a, SpvOpExtInst, v3, (uint32_t[]){ a2s_glsl(a), GLSLstd450UMin, l, left }, 4);
  lx = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->lpos, 0 }, 2);
  ly = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->lpos, 1 }, 2);
  lz = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->lpos, 2 }, 2);
  sx = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->lsize, 0 }, 2);
  sy = pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ a->lsize, 1 }, 2);
  li = pro_op(a, SpvOpIMul, ut, (uint32_t[]){ pro_op(a, SpvOpIAdd, ut, (uint32_t[]){ pro_op(a, SpvOpIMul, ut, (uint32_t[]){ lz, sy }, 2), ly }, 2), sx }, 2);
  a->lindex = pro_op(a, SpvOpIAdd, ut, (uint32_t[]){ li, lx }, 2);
}

// a tile function's quads are 2x2 blocks of its threads, a kernel's are 4 consecutive thread indices
static int tile_function(struct a2s *a) {
  for (LLVMValueRef f = LLVMGetFirstFunction(a->mod); f; f = LLVMGetNextFunction(f)) {
    size_t l = 0;
    const char *n = LLVMGetValueName2(f, &l);
    if (l > 25 && strstr(n, "implicit_imageblock") && LLVMGetFirstUse(f))
      return 1;
  }
  return 0;
}

// the Metal threadgroup a thread belongs to, from its global position: the same however the group is split
uint32_t a2s_logical_group(struct a2s *a) {
  if (uses_quads(a) && tile_function(a))
    quad_layout(a);
  else
    logical_ids(a);
  return a->lgroup;
}

uint32_t a2s_logical_groups(struct a2s *a) {
  uint32_t ut = spv_t_uint(&a->s), v3 = spv_t_vec(&a->s, ut, 3), n, s;

  if (a->lgroups)
    return a->lgroups;
  n = builtin_vec3(a, SpvBuiltInNumWorkgroups);
  s = pro_op(a, SpvOpIMul, v3, (uint32_t[]){ n, a->wg_phys }, 2);
  a->lgroups = pro_op(a, SpvOpUDiv, v3, (uint32_t[]){ s, a->wg_size }, 2);
  return a->lgroups;
}

static void kernel_builtin(struct a2s *a, LLVMValueRef p, const char *kind) {
  LLVMTypeRef t = LLVMTypeOf(p);
  uint32_t v;

  a2s_logical_group(a);

  if (!strcmp(kind, "air.thread_position_in_grid"))
    v = builtin_param(a, t, a->qlocal ? a->qglobal : builtin_vec3(a, SpvBuiltInGlobalInvocationId), 0);
  else if (!strcmp(kind, "air.thread_position_in_threadgroup"))
    v = builtin_param(a, t, a->qlocal ? a->qlocal : a->lpos, 0);
  else if (!strcmp(kind, "air.threadgroup_position_in_grid"))
    v = builtin_param(a, t, a->lgroup, 0);
  else if (!strcmp(kind, "air.threadgroups_per_grid"))
    v = builtin_param(a, t, a2s_logical_groups(a), 0);
  else if (!strcmp(kind, "air.threads_per_threadgroup"))
    v = builtin_param(a, t, a->lsize ? a->lsize : a->wg_size, 0);
  else if (!strcmp(kind, "air.thread_index_in_threadgroup"))
    v = builtin_param(a, t, a->lindex, 1);
  else if (!strcmp(kind, "air.threads_per_grid")) {
    uint32_t ut = spv_t_uint(&a->s), g[3];
    for (unsigned k = 0; k < 3; k++) {
      g[k] = a2s_pc_word(a, A2S_PC_GRID_X + k, 0);
    }
    v = builtin_param(a, t, pro_op(a, SpvOpCompositeConstruct, spv_t_vec(&a->s, ut, 3), g, 3), 0);
  } else if (!strcmp(kind, "air.thread_index_in_quadgroup") && a->qlocal) {
    v = builtin_param(a, t, a->qlane, 1);
  } else if (!strcmp(kind, "air.thread_index_in_quadgroup") || !strcmp(kind, "air.thread_index_in_simdgroup") ||
             !strcmp(kind, "air.quadgroup_index_in_threadgroup") || !strcmp(kind, "air.simdgroup_index_in_threadgroup")) {
    // lanes are consecutive thread indices: 4 to a quad, 32 to a simdgroup (Apple's width)
    int quad = strstr(kind, "quadgroup") != NULL, lane = !strncmp(kind, "air.thread_index", 16);
    uint32_t m[2] = { a->lindex, spv_c_u32(&a->s, lane ? (quad ? 3 : 31) : (quad ? 2 : 5)) };
    v = builtin_param(a, t, pro_op(a, lane ? SpvOpBitwiseAnd : SpvOpShiftRightLogical, spv_t_uint(&a->s), m, 2), 1);
  } else if (!strcmp(kind, "air.threads_per_simdgroup")) {
    v = builtin_param(a, t, spv_c_u32(&a->s, 32), 1);
  } else if (!strcmp(kind, "air.simdgroups_per_threadgroup")) {
    uint32_t ut = spv_t_uint(&a->s), s = a->lsize ? a->lsize : a->wg_size, n;
    n = pro_op(a, SpvOpIMul, ut, (uint32_t[]){ pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ s, 0 }, 2),
                                               pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ s, 1 }, 2) }, 2);
    n = pro_op(a, SpvOpIMul, ut, (uint32_t[]){ n, pro_op(a, SpvOpCompositeExtract, ut, (uint32_t[]){ s, 2 }, 2) }, 2);
    n = pro_op(a, SpvOpShiftRightLogical, ut, (uint32_t[]){ pro_op(a, SpvOpIAdd, ut, (uint32_t[]){ n, spv_c_u32(&a->s, 31) }, 2), spv_c_u32(&a->s, 5) }, 2);
    v = builtin_param(a, t, n, 1);
  } else {
    a2s_fail(a, "kernel parameter %s", kind);
  }

  a2s_set(a, p, v);
}

static uint32_t size_spec(struct a2s *a, unsigned first) {
  uint32_t ut = spv_t_uint(&a->s), c[3], id;

  for (unsigned k = 0; k < 3; k++) {
    c[k] = spv_id(&a->s);
    spv_op(&a->s.globals, SpvOpSpecConstant, 3, ut, c[k], 1);
    spv_deco(&a->s, c[k], SpvDecorationSpecId, 1, first + k);
  }

  id = spv_id(&a->s);
  spv_op(&a->s.globals, SpvOpSpecConstantComposite, 5, spv_t_vec(&a->s, ut, 3), id, c[0], c[1], c[2]);
  return id;
}

// the Metal threadgroup size in spec constants 0 1 2, the Vulkan workgroup's in 8 9 10 (the way
// local_size_x_id does it): the same unless the executor splits the group
static void workgroup_size(struct a2s *a) {
  a->wg_size = size_spec(a, 0);
  a->wg_phys = size_spec(a, A2S_SPEC_PHYS_SIZE);
  spv_deco(&a->s, a->wg_phys, SpvDecorationBuiltIn, 1, SpvBuiltInWorkgroupSize);
}

static void arg(struct a2s *a, LLVMValueRef desc, int list) {
  LLVMValueRef f[64];
  int nf = md_ops(desc, f, 64), j, centroid = 0, sample = 0;
  unsigned pi, loc = 0, access = 0, as = 1, interp = 0;
  char kind[64] = "", s[128], tname[96] = "", vname[96] = "";
  LLVMValueRef p;

  if (nf < 2 || !md_int(f[0], &pi) || pi >= LLVMCountParams(a->fn))
    return;

  p = LLVMGetParam(a->fn, pi);
  md_str(f[1], kind, sizeof(kind));

  for (j = 2; j < nf; j++) {
    unsigned v;

    if (!md_str(f[j], s, sizeof(s)))
      continue;

    if (!strcmp(s, "air.location_index") && j + 1 < nf && md_int(f[j + 1], &v))
      loc = v;
    else if (!strcmp(s, "air.read"))
      access |= A2S_READ;
    else if (!strcmp(s, "air.write"))
      access |= A2S_WRITE;
    else if (!strcmp(s, "air.read_write"))
      access |= A2S_READ | A2S_WRITE;
    else if (!strcmp(s, "air.sample") && !strcmp(kind, "air.texture"))
      access |= A2S_READ;
    else if (!strcmp(s, "air.address_space") && j + 1 < nf && md_int(f[j + 1], &v))
      as = v;
    else if (!strcmp(s, "air.arg_type_name") && j + 1 < nf)
      md_str(f[j + 1], tname, sizeof(tname));
    else if (!strcmp(s, "air.no_perspective"))
      interp = 1;
    else if (!strcmp(s, "air.flat"))
      interp = 2;
    else if (!strcmp(s, "air.centroid"))
      centroid = 1;
    else if (!strcmp(s, "air.sample"))
      sample = 1;
  }

  // a render target index sits right after the kind
  if ((!strcmp(kind, "air.fragment_input") || !strcmp(kind, "air.render_target")) && nf > 2) {
    if (!strcmp(kind, "air.fragment_input"))
      md_str(f[2], vname, sizeof(vname));
    else
      md_int(f[2], &loc);
  }

  if (!strcmp(kind, "air.buffer")) {
    if (as == 3 || LLVMGetPointerAddressSpace(LLVMTypeOf(p)) == 3)
      tg_param(a, p, loc);
    else
      buffer_param(a, p, loc, access, as);
  } else if (!strcmp(kind, "air.texture")) {
    if (LLVMGetFirstUse(p))
      texture_param(a, p, loc, tname, access);
  } else if (!strcmp(kind, "air.sampler")) {
    if (LLVMGetFirstUse(p))
      sampler_var(a, p, A2S_RES_SAMPLER, loc, BIND_SAMPLER + loc, 0);
  } else if (!strcmp(kind, "air.vertex_input")) {
    if (LLVMGetFirstUse(p))
      vertex_input(a, p, loc);
  } else if (!strcmp(kind, "air.vertex_id")) {
    a2s_set(a, p, builtin_u32(a, SpvBuiltInVertexIndex));
  } else if (!strcmp(kind, "air.instance_id")) {
    a2s_set(a, p, builtin_u32(a, SpvBuiltInInstanceIndex));
  } else if (!strcmp(kind, "air.base_vertex") || !strcmp(kind, "air.base_instance")) {
    spv_cap(&a->s, SpvCapabilityDrawParameters);
    a2s_set(a, p, builtin_u32(a, kind[9] == 'v' ? SpvBuiltInBaseVertex : SpvBuiltInBaseInstance));
  } else if (!strcmp(kind, "air.position") && list == 1) {
    uint32_t t = spv_t_vec(&a->s, spv_t_float(&a->s), 4), v = a2s_input_var(a, t, SpvBuiltInFragCoord, -1);
    if (LLVMGetFirstUse(p))
      a2s_set(a, p, pro_op(a, SpvOpLoad, t, &v, 1));
  } else if (!strcmp(kind, "air.fragment_input")) {
    if (LLVMGetFirstUse(p))
      fragment_input(a, p, vname, interp, centroid, sample);
  } else if (!strcmp(kind, "air.front_facing")) {
    uint32_t t = spv_t_bool(&a->s), v = a2s_input_var(a, t, SpvBuiltInFrontFacing, -1);
    a2s_set(a, p, pro_op(a, SpvOpLoad, t, &v, 1));
  } else if (!strcmp(kind, "air.point_coord")) {
    uint32_t t = spv_t_vec(&a->s, spv_t_float(&a->s), 2), v = a2s_input_var(a, t, SpvBuiltInPointCoord, -1);
    a2s_set(a, p, pro_op(a, SpvOpLoad, t, &v, 1));
  } else if (!strcmp(kind, "air.sample_id")) {
    spv_cap(&a->s, SpvCapabilitySampleRateShading);
    a2s_set(a, p, builtin_u32(a, SpvBuiltInSampleId));
  } else if (!strcmp(kind, "air.render_target")) {
    // framebuffer fetch: the attachment as an input attachment of the same subpass
    struct a2s_resource *r = add_res(a, A2S_RES_INPUT_ATTACHMENT, loc, BIND_ATTACHMENT + loc);
    LLVMTypeRef pt = LLVMTypeOf(p);
    int isint = LLVMGetTypeKind(a2s_scalar(pt)) == LLVMIntegerTypeKind;
    uint32_t st = isint ? spv_t_uint(&a->s) : spv_t_float(&a->s), it, var, img, w[3], v4;

    r->elem = isint ? 'u' : 'f';
    it = spv_t_image(&a->s, st, SpvDimSubpassData, 2, SpvImageFormatUnknown);
    var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassUniformConstant, it), SpvStorageClassUniformConstant);
    bind(a, var, r->binding);
    spv_deco(&a->s, var, SpvDecorationInputAttachmentIndex, 1, loc);
    spv_cap(&a->s, SpvCapabilityInputAttachment);
    a->iface[a->niface++] = var;
    img = pro_op(a, SpvOpLoad, it, &var, 1);
    w[0] = img, w[1] = spv_c_comp(&a->s, spv_t_vec(&a->s, spv_t_int(&a->s), 2),
                                  (uint32_t[]){ spv_c_i32(&a->s, 0), spv_c_i32(&a->s, 0) }, 2);
    v4 = pro_op(a, SpvOpImageRead, spv_t_vec(&a->s, st, 4), w, 2);
    if (a2s_lanes(pt) < 4) {
      uint32_t sh[8] = { v4, v4, 0, 1, 2 };
      v4 = a2s_lanes(pt) == 1 ? pro_op(a, SpvOpCompositeExtract, st, (uint32_t[]){ v4, 0 }, 2)
                              : pro_op(a, SpvOpVectorShuffle, a2s_type(a, pt), sh, 2 + a2s_lanes(pt));
    }
    a2s_set(a, p, v4);
  } else if (!strcmp(kind, "air.imageblock")) {
    // the imageblock parameter itself carries nothing: imageblock_data and the implicit loads use the tile.
    // An explicit one's size came from air.imageblock_data_size
  } else if (!strncmp(kind, "air.thread", 10) || !strncmp(kind, "air.threadgroup", 15) ||
             !strncmp(kind, "air.simd", 8) || !strcmp(kind, "air.threads_per_simdgroup")) {
    kernel_builtin(a, p, kind);
  } else if (LLVMGetFirstUse(p)) {
    a2s_fail(a, "parameter kind %s", kind);
  }
}

// explicit imageblock size per thread from air.imageblock_data_size
static void imageblock_size(struct a2s *a, LLVMValueRef args) {
  LLVMValueRef d[64], f[64];
  int n = md_ops(args, d, 64);
  char s[64];

  for (int i = 0; i < n; i++) {
    int nf = md_ops(d[i], f, 64);
    for (int j = 1; j + 1 < nf; j++) {
      unsigned v;
      if (md_str(f[j], s, sizeof(s)) && !strcmp(s, "air.imageblock_data_size") && md_int(f[j + 1], &v))
        a->r->imageblock_bytes = v;
    }
  }
}

static void output_var(struct a2s *a, LLVMTypeRef t, int member, LLVMValueRef md) {
  LLVMValueRef f[32];
  int nf = md ? md_ops(md, f, 32) : 0, j;
  char s[96], elem = 'f';
  uint32_t ty, var;
  int bi = -1, loc = -1, skip = 0, index = 0;

  for (j = 0; j < nf; j++) {
    unsigned v;

    if (!md_str(f[j], s, sizeof(s)))
      continue;

    if (!strcmp(s, "air.position")) {
      bi = SpvBuiltInPosition;
    } else if (!strcmp(s, "air.point_size")) {
      bi = SpvBuiltInPointSize;
    } else if (!strcmp(s, "air.depth")) {
      bi = SpvBuiltInFragDepth;
    } else if (!strcmp(s, "air.sample_mask")) {
      bi = SpvBuiltInSampleMask;
    } else if (!strcmp(s, "air.clip_distance")) {
      skip = 1;
      a2s_note(a, "clip distance output dropped");
    } else if (!strcmp(s, "air.vertex_output") && j + 1 < nf && md_str(f[j + 1], s, sizeof(s))) {
      struct a2s_varying *v = &a->r->var[a->r->nvar];
      if (a->r->nvar == A2S_MAX_VARYINGS)
        a2s_fail(a, "too many vertex outputs");
      memset(v, 0, sizeof(*v));
      snprintf(v->name, sizeof(v->name), "%s", s);
      v->location = a->r->nvar++;
      v->components = a2s_lanes(t);
      loc = (int)v->location;
    } else if (!strcmp(s, "air.render_target") && j + 1 < nf && md_int(f[j + 1], &v)) {
      struct a2s_output *o = &a->r->out[a->r->nout++];
      unsigned ix;
      loc = (int)v;
      if (j + 2 < nf && md_int(f[j + 2], &ix))
        index = (int)ix;
      o->target = v, o->components = a2s_lanes(t);
      o->elem = LLVMGetTypeKind(a2s_scalar(t)) == LLVMIntegerTypeKind ? 'u' : 'f';
    }
  }

  // a fragment result with no metadata goes to attachment 0, a vertex one is not passed on
  if (bi < 0 && loc < 0 && !skip) {
    if (a->stage == A2S_FRAGMENT) {
      struct a2s_output *o = &a->r->out[a->r->nout++];
      loc = 0, o->target = 0, o->components = a2s_lanes(t), o->elem = 'f';
    } else {
      skip = 1;
    }
  }

  if (a->nouts == (int)A2S_ARRAY_LEN(a->outs))
    a2s_fail(a, "too many outputs");

  a->outs[a->nouts].member = member, a->outs[a->nouts].skip = skip, a->outs[a->nouts].lty = t;
  a->outs[a->nouts].builtin = bi;
  if (skip) {
    a->nouts++;
    return;
  }

  ty = bi == SpvBuiltInSampleMask ? spv_t_array(&a->s, spv_t_uint(&a->s), spv_c_u32(&a->s, 1))
       : bi == SpvBuiltInFragDepth || bi == SpvBuiltInPointSize ? spv_t_float(&a->s)
                                                                 : io_type(a, t, &elem);
  if (loc >= 0 && a->stage == A2S_VERTEX)
    a->r->var[a->r->nvar - 1].elem = elem;
  var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassOutput, ty), SpvStorageClassOutput);
  if (bi >= 0)
    spv_deco(&a->s, var, SpvDecorationBuiltIn, 1, (uint32_t)bi);
  else
    spv_deco(&a->s, var, SpvDecorationLocation, 1, (uint32_t)loc);
  if (loc >= 0 && a->stage == A2S_VERTEX)
    a->r->var[a->r->nvar - 1].word = (uint32_t)a->s.annot.n - 1;
  if (index)
    spv_deco(&a->s, var, SpvDecorationIndex, 1, (uint32_t)index);
  if (bi < 0 && a->stage == A2S_VERTEX && elem != 'f')
    spv_deco(&a->s, var, SpvDecorationFlat, 0);
  a->iface[a->niface++] = var;
  a->outs[a->nouts].var = var, a->outs[a->nouts].type = ty;
  a->nouts++;
}

static void outputs(struct a2s *a, LLVMValueRef node) {
  LLVMTypeRef rt = LLVMGetReturnType(LLVMGlobalGetValueType(a->fn));
  LLVMValueRef outs[16];
  int n = md_ops(node, outs, 16);

  if (LLVMGetTypeKind(rt) == LLVMVoidTypeKind)
    return;

  if (LLVMGetTypeKind(rt) == LLVMStructTypeKind) {
    unsigned m = LLVMCountStructElementTypes(rt);
    for (unsigned i = 0; i < m; i++) {
      output_var(a, LLVMStructGetTypeAtIndex(rt, i), (int)i, (int)i < n ? outs[i] : 0);
    }
    return;
  }

  output_var(a, rt, -1, n > 0 ? outs[0] : 0);
}

// pinning: the byte offset a constant GEP chain reaches from the buffer parameter, -1 if not constant
static long long pin_offset(struct a2s *a, LLVMValueRef ptr, LLVMValueRef param) {
  long long off = 0;

  while (ptr != param) {
    LLVMTypeRef t;
    unsigned n;

    if (!LLVMIsAGetElementPtrInst(ptr) && !(LLVMIsAConstantExpr(ptr) && LLVMGetConstOpcode(ptr) == LLVMGetElementPtr))
      return -1;
    t = LLVMGetGEPSourceElementType(ptr);
    n = (unsigned)LLVMGetNumOperands(ptr);
    for (unsigned i = 1; i < n; i++) {
      LLVMValueRef ix = LLVMGetOperand(ptr, i);
      long long v;

      if (!LLVMIsAConstantInt(ix))
        return -1;
      v = LLVMConstIntGetSExtValue(ix);
      if (i == 1) {
        off += v * (long long)LLVMABISizeOfType(a->td, t);
      } else if (LLVMGetTypeKind(t) == LLVMStructTypeKind) {
        off += (long long)LLVMOffsetOfElement(a->td, t, (unsigned)v);
        t = LLVMStructGetTypeAtIndex(t, (unsigned)v);
      } else {
        t = LLVMGetElementType(t);
        off += v * (long long)LLVMABISizeOfType(a->td, t);
      }
    }
    ptr = LLVMGetOperand(ptr, 0);
  }

  return off;
}

// the bytes at off as a constant of type t: ints, halfs, floats and vectors of them
static LLVMValueRef pin_const(struct a2s *a, LLVMTypeRef t, const uint8_t *d) {
  LLVMTypeKind k = LLVMGetTypeKind(t);
  unsigned long long v = 0;

  if (k == LLVMVectorTypeKind) {
    LLVMTypeRef et = LLVMGetElementType(t);
    unsigned n = LLVMGetVectorSize(t), es = (unsigned)LLVMABISizeOfType(a->td, et);
    LLVMValueRef el[32];

    if (n > 32)
      return 0;
    for (unsigned i = 0; i < n; i++) {
      if (!(el[i] = pin_const(a, et, d + i * es)))
        return 0;
    }
    return LLVMConstVector(el, n);
  }

  unsigned bytes = (unsigned)LLVMStoreSizeOfType(a->td, t);
  if (bytes > 8)
    return 0;
  for (unsigned i = 0; i < bytes; i++) {
    v |= (unsigned long long)d[i] << (8 * i);
  }
  if (k == LLVMIntegerTypeKind)
    return LLVMConstInt(t, v, 0);
  if (k == LLVMHalfTypeKind || k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind) {
    LLVMTypeRef it = LLVMIntTypeInContext(a->ctx, bytes * 8);
    return LLVMConstBitCast(LLVMConstInt(it, v, 0), t);
  }
  return 0;
}

unsigned a2s_pin_loads(struct a2s *a) {
  LLVMValueRef ops[8], args[64];
  unsigned done = 0;
  int na;

  if (!a->opt->npins || entry_node(a, ops) < 0 || (na = md_ops(ops[2], args, 64)) < 0)
    return 0;

  for (int i = 0; i < na; i++) {
    LLVMValueRef f[64], param;
    int nf = md_ops(args[i], f, 64);
    unsigned pi, loc = ~0u, as = 0;
    char kind[64] = "", s[128];
    const struct a2s_pin *pin = 0;

    if (nf < 2 || !md_int(f[0], &pi) || pi >= LLVMCountParams(a->fn) || !md_str(f[1], kind, sizeof(kind)) ||
        strcmp(kind, "air.buffer"))
      continue;
    for (int j = 2; j + 1 < nf; j++) {
      unsigned v;

      if (md_str(f[j], s, sizeof(s)) && md_int(f[j + 1], &v)) {
        if (!strcmp(s, "air.location_index"))
          loc = v;
        else if (!strcmp(s, "air.address_space"))
          as = v;
      }
    }
    param = LLVMGetParam(a->fn, pi);
    if (as != 2 && LLVMGetPointerAddressSpace(LLVMTypeOf(param)) != 2)
      continue;
    for (unsigned q = 0; q < a->opt->npins; q++) {
      if (a->opt->pins[q].metal == loc)
        pin = &a->opt->pins[q];
    }
    if (!pin)
      continue;

    // every load in the function reaching this parameter at a constant offset inside the pinned bytes
    for (LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(a->fn); b; b = LLVMGetNextBasicBlock(b)) {
      LLVMValueRef in = LLVMGetFirstInstruction(b), next;

      for (; in; in = next) {
        long long off;
        unsigned sz;
        LLVMValueRef c;

        next = LLVMGetNextInstruction(in);
        if (LLVMGetInstructionOpcode(in) != LLVMLoad || LLVMGetVolatile(in))
          continue;
        // only integers: flags, opcodes and counts steer the shader, floats are per-draw data
        {
          LLVMTypeRef lt = LLVMTypeOf(in);

          if (LLVMGetTypeKind(lt) == LLVMVectorTypeKind)
            lt = LLVMGetElementType(lt);
          if (LLVMGetTypeKind(lt) != LLVMIntegerTypeKind)
            continue;
        }
        off = pin_offset(a, LLVMGetOperand(in, 0), param);
        sz = (unsigned)LLVMStoreSizeOfType(a->td, LLVMTypeOf(in));
        if (off < 0 || off + sz > pin->size || !(c = pin_const(a, LLVMTypeOf(in), pin->data + off)))
          continue;
        LLVMReplaceAllUsesWith(in, c);
        LLVMInstructionEraseFromParent(in);
        done++;
        {
          struct a2s_reflect *r = a->r;
          unsigned k;

          for (k = 0; k < r->npinned; k++) {
            if (r->pinned[k].metal == loc && r->pinned[k].off <= (uint32_t)off &&
                (uint32_t)off + sz <= r->pinned[k].off + r->pinned[k].size)
              break;
          }
          if (k == r->npinned && k < A2S_ARRAY_LEN(r->pinned))
            r->pinned[r->npinned++] = (typeof(r->pinned[0])){ loc, (uint32_t)off, sz };
          else if (k == r->npinned)
            a2s_fail(a, "too many pinned ranges");
        }
      }
    }
  }

  return done;
}

void a2s_interface(struct a2s *a) {
  LLVMValueRef ops[8], args[64];
  int list = entry_node(a, ops), na, i;
  unsigned np = LLVMCountParams(a->fn);

  if (list < 0)
    a2s_fail(a, "no air.vertex/fragment/kernel entry for the function");

  if (list == 2 && a->stage != A2S_TILE)
    a->stage = A2S_KERNEL;
  else if (list == 0)
    a->stage = A2S_VERTEX;
  else if (list == 1)
    a->stage = A2S_FRAGMENT;
  a->r->stage = a->stage;

  if (a->stage == A2S_KERNEL || a->stage == A2S_TILE)
    workgroup_size(a);
  a->tile_quads = tile_function(a);

  const_samplers(a);
  wg_globals(a);

  na = md_ops(ops[2], args, 64);
  if (na < 0)
    a2s_fail(a, "too many argument descriptors");

  imageblock_size(a, ops[2]);
  for (i = 0; i < na; i++) {
    arg(a, args[i], list);
  }

  // every parameter a shader uses must have been bound
  for (unsigned k = 0; k < np; k++) {
    LLVMValueRef p = LLVMGetParam(a->fn, k);
    if (LLVMGetFirstUse(p) && !a2s_slot(a, p, 0))
      a2s_fail(a, "parameter %u has no binding", k);
  }

  outputs(a, ops[1]);
}

static uint32_t out_value(struct a2s *a, int i, uint32_t v) {
  LLVMTypeRef t = a->outs[i].lty;

  // a sample mask output is an array of one, point size and depth plain floats
  if (a->outs[i].builtin == SpvBuiltInSampleMask)
    return a2s_op(a, SpvOpCompositeConstruct, a->outs[i].type, &v, 1);

  (void)t;
  return v;
}

void a2s_return(struct a2s *a, LLVMValueRef v) {
  uint32_t val = v ? a2s_value(a, v) : 0, w[2];

  for (int i = 0; i < a->nouts; i++) {
    uint32_t m = val;

    if (a->outs[i].skip)
      continue;

    if (a->outs[i].member >= 0) {
      w[0] = val, w[1] = (uint32_t)a->outs[i].member;
      m = a2s_op(a, SpvOpCompositeExtract, a2s_type(a, a->outs[i].lty), w, 2);
    }

    w[0] = a->outs[i].var, w[1] = out_value(a, i, m);
    a2s_stmt(a, SpvOpStore, w, 2);
  }

  a2s_stmt(a, SpvOpReturn, 0, 0);
}

struct handle a2s_handle(struct a2s *a, LLVMValueRef v) {
  struct vslot *s = a2s_slot(a, v, 0);

  if (!s || s->kind != VK_HANDLE) {
    char *str = LLVMPrintValueToString(v);
    char msg[120];
    snprintf(msg, sizeof(msg), "%.110s", str);
    LLVMDisposeMessage(str);
    a2s_fail(a, "not a texture or sampler: %s", msg);
  }

  return s->u.h;
}
