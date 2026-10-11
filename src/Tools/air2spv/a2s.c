#include <llvm-c/BitReader.h>
#include <llvm-c/Error.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a2s.h"

// one function's bitcode and entry name in a metallib, the same way airjitd's metallib.c finds them
struct pd_metallib {
  const uint8_t *bitcode;
  uint32_t bitcode_len;
  char entry[96];
};

static const uint8_t *find_bytes(const uint8_t *p, uint32_t n, const char *q, uint32_t m) {
  for (uint32_t i = 0; m <= n && i <= n - m; i++) {
    if (!memcmp(p + i, q, m))
      return p + i;
  }

  return 0;
}

static int pd_metallib_open(const uint8_t *mtl, uint32_t size, struct pd_metallib *lib) {
  const uint8_t *bc = find_bytes(mtl, size, "\xde\xc0\x17\x0b", 4), *namep = find_bytes(mtl, size, "NAME", 4);
  uint32_t nlen;

  // precompiled air64 libraries carry raw bitcode after the function list
  if (!bc)
    bc = find_bytes(mtl, size, "BC\xc0\xde", 4);

  if (!bc || !namep || namep + 6 > mtl + size)
    return -1;

  nlen = namep[4] | (uint32_t)namep[5] << 8;
  if (!nlen || nlen >= sizeof(lib->entry) || namep + 6 + nlen > mtl + size)
    return -1;

  memcpy(lib->entry, namep + 6, nlen);
  lib->entry[nlen - 1] = 0;
  lib->bitcode_len = size - (uint32_t)(bc - mtl);

  // the wrapper header {magic, version, offset, size, cputype} bounds the stream
  if (bc[0] == 0xde && lib->bitcode_len >= 20) {
    uint32_t off = bc[8] | bc[9] << 8 | bc[10] << 16 | (uint32_t)bc[11] << 24;
    uint32_t sz = bc[12] | bc[13] << 8 | bc[14] << 16 | (uint32_t)bc[15] << 24;
    if (off <= lib->bitcode_len && sz <= lib->bitcode_len - off) {
      bc += off;
      lib->bitcode_len = sz;
    }
  }

  lib->bitcode = bc;
  return 0;
}

// re-rolling: Metal's compiler unrolls loops whose body calls a large function and the inliner then pastes it once per iteration.
// a straight run of calls to the same function, each taking the previous result, becomes a loop with one call so drivers compile the body once
#define REROLL_MAX 16
#define REROLL_MIN_CALLS 3
#define REROLL_MIN_INSTS 64

static unsigned fn_insts(LLVMValueRef f) {
  unsigned n = 0;

  for (LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(f); b; b = LLVMGetNextBasicBlock(b)) {
    for (LLVMValueRef in = LLVMGetFirstInstruction(b); in; in = LLVMGetNextInstruction(in)) {
      n++;
    }
  }

  return n;
}

// no memory, no calls: safe to compute before the run
static int pure_inst(LLVMValueRef in) {
  switch (LLVMGetInstructionOpcode(in)) {
  case LLVMLoad: case LLVMStore: case LLVMCall: case LLVMFence: case LLVMAtomicRMW: case LLVMAtomicCmpXchg:
  case LLVMAlloca: case LLVMPHI: case LLVMVAArg: case LLVMLandingPad: case LLVMInvoke:
    return 0;
  default:
    return !LLVMIsATerminatorInst(in);
  }
}

static LLVMValueRef callee_of(LLVMValueRef in) {
  LLVMValueRef f;

  if (!LLVMIsACallInst(in) || !(f = LLVMGetCalledValue(in)) || !LLVMIsAFunction(f) || LLVMIsDeclaration(f))
    return 0;
  return f;
}

// the value of argument j at iteration i, as a select chain over the counter
static LLVMValueRef by_iter(LLVMBuilderRef bld, LLVMValueRef iv, LLVMValueRef *v, unsigned k) {
  LLVMValueRef r = v[k - 1];

  for (int i = (int)k - 2; i >= 0; i--) {
    LLVMValueRef eq = LLVMBuildICmp(bld, LLVMIntEQ, iv, LLVMConstInt(LLVMTypeOf(iv), (unsigned)i, 0), "");
    r = LLVMBuildSelect(bld, eq, v[i], r, "");
  }

  return r;
}

// pointer arguments only vary as constant-index GEPs of one base, so the root stays known
static int gep_parts(LLVMValueRef p, LLVMValueRef *base, LLVMTypeRef *ty, long long *idx) {
  LLVMValueRef c;

  if (!LLVMIsAGetElementPtrInst(p)) {
    *base = p, *ty = 0, *idx = 0;
    return 1;
  }
  if (LLVMGetNumIndices(p) != 1 || !LLVMIsAConstantInt(c = LLVMGetOperand(p, 1)))
    return 0;
  *base = LLVMGetOperand(p, 0), *ty = LLVMGetGEPSourceElementType(p), *idx = LLVMConstIntGetSExtValue(c);
  return 1;
}

// calls[0..k): same function, calls[i] passes calls[i-1] as argument chain. Returns 1 when rolled
static int reroll_run(struct a2s *a, LLVMValueRef *calls, unsigned k, unsigned chain) {
  LLVMValueRef first = calls[0], last = calls[k - 1], fn = LLVMGetCalledValue(first);
  LLVMBasicBlockRef blk = LLVMGetInstructionParent(first), pre, loop;
  LLVMValueRef parent = LLVMGetBasicBlockParent(blk), in, next, args[32], vals[REROLL_MAX];
  LLVMTypeRef fty = LLVMGetCalledFunctionType(first), i32 = LLVMInt32TypeInContext(a->ctx), i64 = LLVMInt64TypeInContext(a->ctx);
  unsigned nargs = (unsigned)LLVMGetNumArgOperands(first);
  LLVMBuilderRef bld;

  if (nargs > 32)
    return 0;

  // between the calls only argument math, and no intermediate result escapes but through the chain
  for (in = LLVMGetNextInstruction(first); in != last; in = LLVMGetNextInstruction(in)) {
    if (callee_of(in) == fn)
      continue;
    if (!pure_inst(in))
      return 0;
    for (unsigned o = 0; o < (unsigned)LLVMGetNumOperands(in); o++) {
      for (unsigned i = 0; i < k; i++) {
        if (LLVMGetOperand(in, o) == calls[i])
          return 0;
      }
    }
  }
  for (unsigned i = 0; i + 1 < k; i++) {
    for (LLVMUseRef u = LLVMGetFirstUse(calls[i]); u; u = LLVMGetNextUse(u)) {
      if (LLVMGetUser(u) != calls[i + 1])
        return 0;
    }
  }
  for (unsigned j = 0; j < nargs; j++) {
    LLVMValueRef b0, b;
    LLVMTypeRef t0 = 0, t;
    long long ix;

    if (j == chain || LLVMGetTypeKind(LLVMTypeOf(LLVMGetOperand(first, j))) != LLVMPointerTypeKind)
      continue;
    if (!gep_parts(LLVMGetOperand(first, j), &b0, &t0, &ix))
      return 0;
    for (unsigned i = 1; i < k; i++) {
      LLVMValueRef p = LLVMGetOperand(calls[i], j);

      if (p == LLVMGetOperand(first, j))
        continue;
      if (!gep_parts(p, &b, &t, &ix) || b != b0 || (t && t0 && t != t0))
        return 0;
      if (!t0)
        t0 = t;
    }
  }

  // pre: the block up to the run plus the hoisted argument math, loop: one call, blk keeps the rest
  pre = LLVMInsertBasicBlockInContext(a->ctx, blk, "reroll.pre");
  loop = LLVMInsertBasicBlockInContext(a->ctx, blk, "reroll.loop");
  {
    LLVMValueRef users[64];
    unsigned nu = 0;

    for (LLVMUseRef u = LLVMGetFirstUse(LLVMBasicBlockAsValue(blk)); u && nu < 64; u = LLVMGetNextUse(u)) {
      users[nu++] = LLVMGetUser(u);
    }
    for (unsigned q = 0; q < nu; q++) {
      if (!LLVMIsATerminatorInst(users[q]) || LLVMGetInstructionParent(users[q]) == loop)
        continue;
      for (unsigned s = 0; s < LLVMGetNumSuccessors(users[q]); s++) {
        if (LLVMGetSuccessor(users[q], s) == blk)
          LLVMSetSuccessor(users[q], s, pre);
      }
    }
  }

  bld = LLVMCreateBuilderInContext(a->ctx);
  LLVMPositionBuilderAtEnd(bld, pre);
  for (in = LLVMGetFirstInstruction(blk); in != first; in = next) {
    next = LLVMGetNextInstruction(in);
    LLVMInstructionRemoveFromParent(in);
    LLVMInsertIntoBuilder(bld, in);
  }
  for (in = LLVMGetNextInstruction(first); in != last; in = next) {
    next = LLVMGetNextInstruction(in);
    if (callee_of(in) == fn)
      continue;
    LLVMInstructionRemoveFromParent(in);
    LLVMInsertIntoBuilder(bld, in);
  }
  LLVMBuildBr(bld, loop);

  LLVMPositionBuilderAtEnd(bld, loop);
  {
    LLVMValueRef iv = LLVMBuildPhi(bld, i32, "reroll.i"), acc = LLVMBuildPhi(bld, LLVMTypeOf(first), "reroll.acc");
    LLVMValueRef zero = LLVMConstInt(i32, 0, 0), init = LLVMGetOperand(first, chain), call, inc, more;
    LLVMBasicBlockRef inb[1] = { pre };

    for (unsigned j = 0; j < nargs; j++) {
      LLVMValueRef a0 = LLVMGetOperand(first, j);
      int same = 1;

      if (j == chain) {
        args[j] = acc;
        continue;
      }
      for (unsigned i = 1; i < k; i++) {
        vals[i] = LLVMGetOperand(calls[i], j);
        same &= vals[i] == a0;
      }
      vals[0] = a0;
      if (same) {
        args[j] = a0;
      } else if (LLVMGetTypeKind(LLVMTypeOf(a0)) == LLVMPointerTypeKind) {
        LLVMValueRef base = 0, idx[REROLL_MAX];
        LLVMTypeRef ty = 0, t;
        long long ix;

        for (unsigned i = 0; i < k; i++) {
          gep_parts(vals[i], &base, &t, &ix);
          if (t)
            ty = t;
          idx[i] = LLVMConstInt(i64, (unsigned long long)ix, 1);
        }
        args[j] = LLVMBuildInBoundsGEP2(bld, ty, base, (LLVMValueRef[]){ by_iter(bld, iv, idx, k) }, 1, "");
      } else {
        args[j] = by_iter(bld, iv, vals, k);
      }
    }

    call = LLVMBuildCall2(bld, fty, fn, args, nargs, "");
    LLVMSetInstructionCallConv(call, LLVMGetInstructionCallConv(first));
    inc = LLVMBuildAdd(bld, iv, LLVMConstInt(i32, 1, 0), "");
    more = LLVMBuildICmp(bld, LLVMIntULT, inc, LLVMConstInt(i32, k, 0), "");
    LLVMBuildCondBr(bld, more, loop, blk);

    LLVMAddIncoming(iv, &zero, inb, 1);
    LLVMAddIncoming(acc, &init, inb, 1);
    inb[0] = loop;
    LLVMAddIncoming(iv, &inc, inb, 1);
    LLVMAddIncoming(acc, &call, inb, 1);

    LLVMReplaceAllUsesWith(last, call);
  }
  LLVMDisposeBuilder(bld);

  for (int i = (int)k - 1; i >= 0; i--) {
    LLVMInstructionEraseFromParent(calls[i]);
  }
  (void)parent;
  return 1;
}

static void reroll_calls(struct a2s *a, LLVMValueRef f) {
  int again = 1;

  while (again) {
    again = 0;
    for (LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(f); b && !again; b = LLVMGetNextBasicBlock(b)) {
      for (LLVMValueRef in = LLVMGetFirstInstruction(b); in && !again; in = LLVMGetNextInstruction(in)) {
        LLVMValueRef fn = callee_of(in), calls[REROLL_MAX], x;
        unsigned k = 1, chain = ~0u, n;

        if (!fn || fn_insts(fn) < REROLL_MIN_INSTS || LLVMGetTypeKind(LLVMTypeOf(in)) == LLVMVoidTypeKind)
          continue;
        calls[0] = in;
        n = (unsigned)LLVMGetNumArgOperands(in);
        for (x = LLVMGetNextInstruction(in); x && k < REROLL_MAX; x = LLVMGetNextInstruction(x)) {
          unsigned c = ~0u;

          if (callee_of(x) != fn) {
            if (pure_inst(x))
              continue;
            break;
          }
          for (unsigned j = 0; j < n; j++) {
            if (LLVMGetOperand(x, j) == calls[k - 1])
              c = j;
          }
          if (c == ~0u || (chain != ~0u && c != chain))
            break;
          chain = c, calls[k++] = x;
        }
        if (k >= REROLL_MIN_CALLS && reroll_run(a, calls, k, chain))
          again = 1;
      }
    }
  }
}

// every other function is inlined into the entry: SPIR-V gets one function, handles never cross calls
static void inline_all(struct a2s *a) {
  unsigned ai = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
  unsigned ni = LLVMGetEnumAttributeKindForName("noinline", 8);

  for (LLVMValueRef f = LLVMGetFirstFunction(a->mod); f; f = LLVMGetNextFunction(f)) {
    if (f == a->fn || LLVMIsDeclaration(f))
      continue;

    LLVMRemoveEnumAttributeAtIndex(f, LLVMAttributeFunctionIndex, ni);
    LLVMAddAttributeAtIndex(f, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(a->ctx, ai, 0));
    LLVMSetLinkage(f, LLVMInternalLinkage);
  }
}

static void run_passes(struct a2s *a, const char *passes) {
  LLVMPassBuilderOptionsRef o = LLVMCreatePassBuilderOptions();
  LLVMErrorRef err = LLVMRunPasses(a->mod, passes, 0, o);

  LLVMDisposePassBuilderOptions(o);
  if (err) {
    char *m = LLVMGetErrorMessage(err);
    char msg[200];
    snprintf(msg, sizeof(msg), "%.190s", m);
    LLVMDisposeErrorMessage(m);
    a2s_fail(a, "passes %s: %s", passes, msg);
  }
}

static void pc_block(struct a2s *a, struct spv_buf *out) {
  uint32_t ut = spv_t_uint(&a->s), m[A2S_MAX_PC], st, var, pt;
  unsigned n = a->r->npc, k;

  if (!n)
    return;

  for (k = 0; k < n; k++) {
    m[k] = ut;
  }

  st = spv_t_struct_unique(&a->s, m, n);
  spv_deco(&a->s, st, SpvDecorationBlock, 0);
  for (k = 0; k < n; k++) {
    spv_mdeco(&a->s, st, k, SpvDecorationOffset, 1, a->opt->pc_base + 4 * k);
  }

  var = spv_var(&a->s, spv_t_ptr(&a->s, SpvStorageClassPushConstant, st), SpvStorageClassPushConstant);
  a->iface[a->niface++] = var;
  pt = spv_t_ptr(&a->s, SpvStorageClassPushConstant, ut);
  for (k = 0; k < n; k++) {
    uint32_t p = spv_id(&a->s);
    spv_op(out, SpvOpAccessChain, 4, pt, p, var, spv_c_u32(&a->s, k));
    spv_op(out, SpvOpLoad, 3, ut, a->pc_ids[k], p);
  }
}

// kernels without barriers: threads past the grid in the push constants return at once
static void guard(struct a2s *a, struct spv_buf *out, uint32_t entry_label) {
  uint32_t ut = spv_t_uint(&a->s), v3 = spv_t_vec(&a->s, ut, 3), bt = spv_t_bool(&a->s);
  uint32_t gid_var, gid, g[3], grid, ge, any, ret = spv_id(&a->s), id;

  gid_var = a2s_input_var(a, v3, SpvBuiltInGlobalInvocationId, -1);
  for (unsigned k = 0; k < 3; k++) {
    g[k] = a2s_pc_word(a, A2S_PC_GRID_X + k, 0);
  }

  gid = spv_id(&a->s);
  spv_op(out, SpvOpLoad, 3, v3, gid, gid_var);
  if (a->qglobal)
    gid = a->qglobal;
  grid = spv_id(&a->s);
  spv_op(out, SpvOpCompositeConstruct, 5, v3, grid, g[0], g[1], g[2]);
  ge = spv_id(&a->s);
  spv_op(out, SpvOpUGreaterThanEqual, 4, spv_t_vec(&a->s, bt, 3), ge, gid, grid);
  any = spv_id(&a->s);
  spv_op(out, SpvOpAny, 3, bt, any, ge);
  spv_op(out, SpvOpSelectionMerge, 2, entry_label, 0);
  spv_op(out, SpvOpBranchConditional, 3, any, ret, entry_label);
  spv_op(out, SpvOpLabel, 1, ret);
  spv_op(out, SpvOpReturn, 0);
  (void)id;
}

static void needs(struct a2s *a) {
  static const struct {
    SpvCapability c;
    const char *name;
  } names[] = {
    { SpvCapabilityImageQuery, "ImageQuery" },
    { SpvCapabilityStorageImageWriteWithoutFormat, "StorageImageWriteWithoutFormat" },
    { SpvCapabilityStorageImageReadWithoutFormat, "StorageImageReadWithoutFormat" },
    { SpvCapabilityDemoteToHelperInvocation, "DemoteToHelperInvocation" },
    { SpvCapabilityGroupNonUniformQuad, "GroupNonUniformQuad" },
    { SpvCapabilityGroupNonUniform, "GroupNonUniform" },
    { SpvCapabilityInputAttachment, "InputAttachment" },
    { SpvCapabilitySampleRateShading, "SampleRateShading" },
    { SpvCapabilityDrawParameters, "DrawParameters" },
  };
  char *o = a->r->needs;

  for (unsigned i = 0; i < A2S_ARRAY_LEN(names); i++) {
    int have = 0;
    for (size_t k = 0; k < a->s.caps.n; k += 2) {
      if (a->s.caps.w[k + 1] == (uint32_t)names[i].c)
        have = 1;
    }
    if (have)
      snprintf(o + strlen(o), sizeof(a->r->needs) - strlen(o), "%s%s", *o ? "," : "", names[i].name);
  }
}

static void finish(struct a2s *a, uint32_t **words, size_t *nwords) {
  struct spv_buf fn = { 0 }, pro2 = { 0 };
  uint32_t vt = spv_t_void(&a->s), fid = spv_id(&a->s), w[200], n = 0, model, pro_label = spv_id(&a->s);
  int kernel = a->stage == A2S_KERNEL || a->stage == A2S_TILE;

  // the grid guard adds push constant words, so it is built before the block is
  a->r->guard = a->stage == A2S_KERNEL && !a->uses_barrier;
  a->r->barrier = a->uses_barrier;
  for (int i = 0; i < a->nroots; i++) {
    if (a->roots[i].kind == ROOT_WG)
      a->r->group_shared = 1;
  }
  // quads exchange inside a workgroup either way: a split keeps whole quads
  a->r->group_shared |= a->shader_barrier || a->r->imageblock_bytes || a->lane_exchange;
  if (a->r->guard)
    guard(a, &pro2, a->first_label);
  else
    spv_op(&pro2, SpvOpBranch, 1, a->first_label);

  spv_op(&fn, SpvOpFunction, 4, vt, fid, 0, spv_t_fn(&a->s, vt));
  spv_op(&fn, SpvOpLabel, 1, pro_label);
  spv_append(&fn, &a->vars);
  pc_block(a, &fn);
  spv_append(&fn, &a->pro);
  spv_append(&fn, &pro2);
  spv_append(&fn, &a->body);
  spv_op(&fn, SpvOpFunctionEnd, 0);
  spv_append(&a->s.funcs, &fn);
  free(fn.w), free(pro2.w);

  model = a->stage == A2S_VERTEX ? SpvExecutionModelVertex : a->stage == A2S_FRAGMENT ? SpvExecutionModelFragment
                                                                                       : SpvExecutionModelGLCompute;
  w[n++] = model, w[n++] = fid;
  n += spv_str(w + n, "main");
  for (unsigned i = 0; i < a->niface && n < 200; i++) {
    w[n++] = a->iface[i];
  }
  spv_emit(&a->s.entry, SpvOpEntryPoint, w, n);

  if (a->stage == A2S_FRAGMENT) {
    spv_op(&a->s.modes, SpvOpExecutionMode, 2, fid, SpvExecutionModeOriginUpperLeft);
    for (int i = 0; i < a->nouts; i++) {
      if (a->outs[i].builtin == SpvBuiltInFragDepth)
        spv_op(&a->s.modes, SpvOpExecutionMode, 2, fid, SpvExecutionModeDepthReplacing);
    }
  }
  if (kernel)
    spv_op(&a->s.modes, SpvOpExecutionMode, 5, fid, SpvExecutionModeLocalSize, 1, 1, 1);

  spv_op(&a->s.model, SpvOpMemoryModel, 2, SpvAddressingModelLogical, SpvMemoryModelGLSL450);
  needs(a);

  // varying Location words: from the annotation section to the whole binary
  {
    uint32_t before = 5 + (uint32_t)(a->s.caps.n + a->s.exts.n + a->s.imports.n + a->s.model.n + a->s.entry.n +
                                     a->s.modes.n + a->s.debug.n);
    for (unsigned i = 0; i < a->r->nvar; i++) {
      a->r->var[i].word += before;
    }
  }
  *words = spv_finish(&a->s, nwords);
}

// tried in order: as compiled, short circuits folded into selects, then LLVM's structurizer
static const char *const pipelines[] = {
  "always-inline,globaldce,function(mergereturn)",
  "always-inline,globaldce,function(simplifycfg<bonus-inst-threshold=64;keep-loops>,mergereturn)",
  ("always-inline,globaldce,function(lower-switch,mergereturn,fix-irreducible,unify-loop-exits,structurizecfg,"
   "simplifycfg<no-keep-loops>)"),
};

static int attempt(const struct pd_metallib *lib, const struct a2s_options *opt, int structurize, int promote,
                   uint32_t **words, size_t *nwords, struct a2s_reflect *refl, char *err, size_t errlen) {
  struct a2s *a = calloc(1, sizeof(*a));
  LLVMMemoryBufferRef buf;
  volatile int rc = 0;

  a->opt = opt, a->r = refl, a->err = err, a->errlen = errlen, a->structurize = structurize;
  memset(refl, 0, sizeof(*refl));
  snprintf(refl->entry, sizeof(refl->entry), "%s", lib->entry);
  refl->pc_base = opt->pc_base;
  refl->offset_align = 4;
  spv_init(&a->s);
  a->ctx = LLVMContextCreate();
  buf = LLVMCreateMemoryBufferWithMemoryRangeCopy((const char *)lib->bitcode, lib->bitcode_len, lib->entry);

  if (setjmp(a->fail)) {
    rc = -1;
    // A2S_TRACE: each attempt's error, the last one alone reaches the caller
    if (getenv("A2S_TRACE"))
      fprintf(stderr, "a2s: attempt %d%s: %s\n", structurize, promote ? " promoted" : "", err);
    goto out;
  }

  if (LLVMParseBitcodeInContext2(a->ctx, buf, &a->mod))
    a2s_fail(a, "bitcode does not parse");

  if (!(a->fn = LLVMGetNamedFunction(a->mod, lib->entry)))
    a2s_fail(a, "no function %s", lib->entry);

  if (!getenv("A2S_NO_REROLL"))
    reroll_calls(a, a->fn);
  inline_all(a);
  run_passes(a, pipelines[structurize]);
  // buffer pointers kept in a private struct (a context the shader passes around) have no SPIR-V value to
  // store: sroa turns the struct's fields back into the arguments
  if (promote)
    run_passes(a, "function(sroa)");
  // A2S_DUMP=FILE: the module as the walk sees it
  if (getenv("A2S_DUMP")) {
    char path[512];
    snprintf(path, sizeof(path), "%s.%d%s.ll", getenv("A2S_DUMP"), structurize, promote ? "p" : "");
    LLVMPrintModuleToFile(a->mod, path, 0);
  }
  a->td = LLVMGetModuleDataLayout(a->mod);
  // known configuration bytes: fold their loads, then drop what they switch off. a bare instcombine in a textual
  // pipeline checks it reached a fixpoint in one go and is a fatal error (the whole daemon) when it did not
  if (a2s_pin_loads(a))
    run_passes(a, "function(sccp,instcombine<no-verify-fixpoint>,simplifycfg<keep-loops>,adce,mergereturn),"
                  "globaldce");

  spv_cap(&a->s, SpvCapabilityShader);
  a2s_interface(a);
  a2s_cfg(a);
  finish(a, words, nwords);

out:
  if (a->mod)
    LLVMDisposeModule(a->mod);
  LLVMContextDispose(a->ctx);
  spv_free(&a->s);
  free(a->vars.w), free(a->pro.w), free(a->body.w), free(a->vt);
  if (a->b) {
    for (int i = 0; i < a->nb; i++) {
      free(a->b[i].succ), free(a->b[i].body);
    }
    free(a->b);
  }
  free(a);
  return rc;
}

int a2s_translate(const uint8_t *mtl, uint32_t size, const struct a2s_options *opt, uint32_t **words,
                  size_t *nwords, struct a2s_reflect *refl, char *err, size_t errlen) {
  struct pd_metallib lib;
  char first[256];

  if (pd_metallib_open(mtl, size, &lib)) {
    snprintf(err, errlen, "not a metallib with bitcode");
    return -1;
  }

  // a second round with private structs promoted, only for what the first could not place
  for (int promote = 0; promote < 2; promote++) {
    if (promote && !strstr(err, "value used before it was defined"))
      return -1;
    if (!attempt(&lib, opt, 0, promote, words, nwords, refl, err, errlen))
      return 0;

    // control flow the direct walk cannot structure: folded short circuits, then LLVM's structurizer
    for (int k = 1; k < 3; k++) {
      if (!strstr(err, "structured") && !strstr(err, "loop with more than one exit"))
        break;

      if (k == 1)
        snprintf(first, sizeof(first), "%s", err);
      if (!attempt(&lib, opt, k, promote, words, nwords, refl, err, errlen)) {
        snprintf(refl->notes + strlen(refl->notes), sizeof(refl->notes) - strlen(refl->notes), "%s%s (%s)",
                 refl->notes[0] ? ";" : "", k == 1 ? "short circuits folded" : "structurized", first);
        return 0;
      }
    }
  }
  return -1;
}

unsigned a2s_relocate(uint32_t *frag_words, const struct a2s_reflect *frag, const struct a2s_reflect *vert) {
  unsigned matched = 0;

  // the reflected word offsets are into the annotation section: find it again by scanning for the
  // OpDecorate Location instructions in order
  for (unsigned i = 0; i < frag->nvar; i++) {
    for (unsigned k = 0; k < vert->nvar; k++) {
      if (strcmp(frag->var[i].name, vert->var[k].name))
        continue;
      frag_words[frag->var[i].word] = vert->var[k].location;
      matched++;
      break;
    }
  }

  return matched;
}

static void json_str(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s; s++) {
    if (*s == '"' || *s == '\\')
      fputc('\\', f);
    fputc(*s, f);
  }
  fputc('"', f);
}

void a2s_reflect_json(const struct a2s_reflect *r, void *file) {
  static const char *const stages[] = { "vertex", "fragment", "kernel", "tile" };
  static const char *const kinds[] = { "buffer", "texture", "sampler", "const_sampler", "input_attachment",
                                       "attachment_image" };
  static const char *const pcs[] = { "buffer_offset", "grid_x", "grid_y", "grid_z" };
  FILE *f = file;
  unsigned i;

  fprintf(f, "{\"stage\":\"%s\",\"entry\":", stages[r->stage]);
  json_str(f, r->entry);
  fprintf(f, ",\"resources\":[");
  for (i = 0; i < r->nres; i++) {
    const struct a2s_resource *x = &r->res[i];
    fprintf(f, "%s{\"kind\":\"%s\",\"metal\":%u,\"set\":%u,\"binding\":%u,\"access\":%u", i ? "," : "",
            kinds[x->kind], x->metal, x->set, x->binding, x->access);
    if (x->kind == A2S_RES_TEXTURE || x->kind == A2S_RES_INPUT_ATTACHMENT || x->kind == A2S_RES_ATTACHMENT_IMAGE)
      fprintf(f, ",\"dim\":%u,\"storage\":%u,\"elem\":\"%c\",\"levels\":%u", x->dim, x->storage,
              x->elem ? x->elem : 'f', x->levels);
    if (x->kind == A2S_RES_CONST_SAMPLER) {
      struct a2s_sampler_desc d;
      a2s_sampler_state(x->state, &d);
      fprintf(f, ",\"state\":\"0x%016llx\",\"addr\":[%u,%u,%u],\"mag\":%u,\"min\":%u,\"mip\":%u,\"pixel\":%d",
              (unsigned long long)x->state, d.addr[0], d.addr[1], d.addr[2], d.mag, d.min, d.mip, d.pixel);
    }
    if (x->kind == A2S_RES_SAMPLER)
      fprintf(f, ",\"spec_id\":%u", x->spec_id);
    fputc('}', f);
  }
  fprintf(f, "],\"varyings\":[");
  for (i = 0; i < r->nvar; i++) {
    fprintf(f, "%s{\"name\":", i ? "," : "");
    json_str(f, r->var[i].name);
    fprintf(f, ",\"location\":%u,\"components\":%u,\"elem\":\"%c\",\"interp\":%u,\"word\":%u}", r->var[i].location,
            r->var[i].components, r->var[i].elem ? r->var[i].elem : 'f', r->var[i].interp, r->var[i].word);
  }
  fprintf(f, "],\"outputs\":[");
  for (i = 0; i < r->nout; i++) {
    fprintf(f, "%s{\"target\":%u,\"components\":%u,\"elem\":\"%c\"}", i ? "," : "", r->out[i].target,
            r->out[i].components, r->out[i].elem);
  }
  fprintf(f, "],\"push_constants\":{\"base\":%u,\"words\":[", r->pc_base);
  for (i = 0; i < r->npc; i++) {
    fprintf(f, "%s{\"kind\":\"%s\",\"metal\":%u}", i ? "," : "", pcs[r->pc[i].kind], r->pc[i].metal);
  }
  fprintf(f, "]},\"group_shared\":%d,\"offset_align\":%u,\"quad_order\":%d,\"guard\":%d,\"barrier\":%d,\"imageblock_bytes\":%u,\"needs\":", r->group_shared, r->offset_align, r->quad_order, r->guard, r->barrier,
          r->imageblock_bytes);
  json_str(f, r->needs);
  fprintf(f, ",\"notes\":");
  json_str(f, r->notes);
  fprintf(f, ",\"pinned\":[");
  for (i = 0; i < r->npinned; i++) {
    fprintf(f, "%s{\"metal\":%u,\"off\":%u,\"size\":%u}", i ? "," : "", r->pinned[i].metal, r->pinned[i].off,
            r->pinned[i].size);
  }
  fprintf(f, "]");
  fprintf(f, "}\n");
}
