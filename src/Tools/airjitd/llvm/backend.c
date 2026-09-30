#include <llvm-c/BitReader.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/ErrorHandling.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/Target.h>
#include <stdio.h>
#include <string.h>

#include "../airjitd.h"
#include "../backend.h"
#include "air.h"

extern const uint32_t init_start[] __asm("section$start$__TEXT$__init_offsets");
extern const uint32_t init_end[] __asm("section$end$__TEXT$__init_offsets");

static const char *const entry_lists[] = {
  [PD_STAGE_VERTEX] = "air.vertex",
  [PD_STAGE_FRAGMENT] = "air.fragment",
  [PD_STAGE_KERNEL] = "air.kernel",
};

static void llvm_fatal(const char *reason) {
  pd_job_abort(-1, reason);
}

static void llvm_init(void) {
  logmsg("AIRJITD: static LLVM backend (%lu static constructors suppressed)", (unsigned long)(init_end - init_start));
  LLVMInitializeAArch64TargetInfo();
  LLVMInitializeAArch64Target();
  LLVMInitializeAArch64TargetMC();
  LLVMInitializeAArch64AsmPrinter();
  LLVMInstallFatalErrorHandler(llvm_fatal);
}

// LLVMGet*Operands write every operand: fetch only when the node fits the array
static int md_ops(LLVMValueRef node, LLVMValueRef *out, unsigned cap) {
  unsigned n = LLVMGetMDNodeNumOperands(node);

  if (n > cap) {
    logmsg("AIRJITD: metadata node has %u operands, room for %u", n, cap);
    return -1;
  }

  LLVMGetMDNodeOperands(node, out);
  return (int)n;
}

static int named_md_ops(LLVMModuleRef mod, const char *name, LLVMValueRef *out, unsigned cap) {
  unsigned n = LLVMGetNamedMetadataNumOperands(mod, name);

  if (n > cap) {
    logmsg("AIRJITD: %s has %u entries, room for %u", name, n, cap);
    return -1;
  }

  if (n)
    LLVMGetNamedMetadataOperands(mod, name, out);

  return (int)n;
}

static const char *md_string(LLVMValueRef v) {
  unsigned len = 0;

  return v ? LLVMGetMDString(v, &len) : 0;
}

static int md_uint(LLVMValueRef v, unsigned *out) {
  if (!v || !LLVMIsAConstantInt(v))
    return 0;

  *out = (unsigned)LLVMConstIntGetZExtValue(v);
  return 1;
}

static int parse_module(const struct pd_metallib *lib, LLVMContextRef *ctx, LLVMModuleRef *mod) {
  LLVMMemoryBufferRef buf =
      LLVMCreateMemoryBufferWithMemoryRangeCopy((const char *)lib->bitcode, lib->bitcode_len, lib->entry);

  *ctx = LLVMContextCreate();
  *mod = 0;

  if (LLVMParseBitcodeInContext2(*ctx, buf, mod)) {
    logmsg("AIRJITD: bitcode parse failed for '%s'", lib->entry);
    return PD_ERR_PARSE;
  }

  return 0;
}

// JITs the module and looks sym up. The JIT stays alive for the life of the daemon
static int jit_module(LLVMContextRef ctx, LLVMModuleRef mod, const char *sym, uint64_t *addr) {
  LLVMOrcLLJITRef jit = 0;
  LLVMOrcThreadSafeContextRef tsctx;
  LLVMOrcThreadSafeModuleRef tsm;
  LLVMErrorRef err;

  if ((err = LLVMOrcCreateLLJIT(&jit, 0)) != 0) {
    logmsg("AIRJITD: create JIT failed: %s", LLVMGetErrorMessage(err));
    return PD_ERR_JIT_CREATE;
  }

  LLVMSetTarget(mod, LLVMOrcLLJITGetTripleString(jit));
  LLVMSetDataLayout(mod, LLVMOrcLLJITGetDataLayoutStr(jit));
  tsctx = LLVMOrcCreateNewThreadSafeContextFromLLVMContext(ctx);
  tsm = LLVMOrcCreateNewThreadSafeModule(mod, tsctx);

  if ((err = LLVMOrcLLJITAddLLVMIRModule(jit, LLVMOrcLLJITGetMainJITDylib(jit), tsm)) != 0) {
    logmsg("AIRJITD: add module failed: %s", LLVMGetErrorMessage(err));
    return PD_ERR_JIT_ADD;
  }

  if ((err = LLVMOrcLLJITLookup(jit, addr, sym)) != 0) {
    char *m = LLVMGetErrorMessage(err);
    logmsg("AIRJITD: lookup %s failed: %s", sym, m);
    pd_note(" %s: %s", sym, m);
    return PD_ERR_JIT_LOOKUP;
  }

  return 0;
}

// result layout: a packed struct of vectors, or one vector
static int result_layout(struct StageInfo *si, LLVMTypeRef retty) {
  LLVMTypeRef elems[PD_MAX_RESULTS];
  unsigned ne = 1, i;

  if (LLVMGetTypeKind(retty) == LLVMStructTypeKind) {
    ne = LLVMCountStructElementTypes(retty);
    if (ne > PD_MAX_RESULTS)
      return PD_ERR_TOO_MANY_RESULTS;

    LLVMGetStructElementTypes(retty, elems);
  } else {
    elems[0] = retty;
  }

  for (i = 0; i < ne; i++) {
    LLVMTypeRef et = elems[i];
    unsigned cnt = 1;

    if (LLVMGetTypeKind(et) == LLVMVectorTypeKind) {
      cnt = LLVMGetVectorSize(et);
      et = LLVMGetElementType(et);
    }

    si->elemCount[i] = cnt;
    si->elemHalf[i] = LLVMGetTypeKind(et) == LLVMHalfTypeKind;
    si->elemBytes[i] = cnt * (si->elemHalf[i] ? 2 : 4);
    si->outBytes += si->elemBytes[i];
  }

  si->nelems = ne;
  return 0;
}

static unsigned scalar_bytes(LLVMTypeRef t) {
  switch (LLVMGetTypeKind(t)) {
  case LLVMIntegerTypeKind:
    return LLVMGetIntTypeWidth(t) / 8;
  case LLVMHalfTypeKind:
    return 2;
  case LLVMFloatTypeKind:
    return 4;
  default:
    return 8;
  }
}

static void param_layout(struct StageInfo *si, LLVMValueRef fn) {
  unsigned i;

  for (i = 0; i < si->nparams; i++) {
    LLVMTypeRef pt = LLVMTypeOf(LLVMGetParam(fn, i));
    unsigned cnt = 1;

    if (LLVMGetTypeKind(pt) == LLVMVectorTypeKind) {
      cnt = LLVMGetVectorSize(pt);
      pt = LLVMGetElementType(pt);
    }

    si->pcount[i] = cnt;
    si->pwidth[i] = scalar_bytes(pt);
  }
}

// vertex outputs: only air.position and air.vertex_output reach the fragment
static void mark_varyings(struct StageInfo *si, LLVMValueRef outputs) {
  LLVMValueRef outs[PD_MAX_RESULTS];
  int nout = md_ops(outputs, outs, PD_MAX_RESULTS), o;

  for (o = 0; o < nout; o++) {
    LLVMValueRef f[16];
    int nf = md_ops(outs[o], f, 16), j, keep = 0;

    for (j = 0; j < nf; j++) {
      const char *sv = md_string(f[j]);
      if (sv && (!strcmp(sv, "air.position") || !strcmp(sv, "air.vertex_output")))
        keep = 1;
    }

    si->elemSkip[o] = nf > 0 && !keep;
    if (si->elemSkip[o])
      logmsg("AIRJITD:   output %d is not a varying, skipped", o);
  }
}

// fragment outputs: {"air.render_target", N, ..}. Without the entry an output goes to attachment 0
static void mark_targets(struct StageInfo *si, LLVMValueRef outputs) {
  LLVMValueRef outs[PD_MAX_RESULTS];
  int nout = md_ops(outputs, outs, PD_MAX_RESULTS), o;

  for (o = 0; o < nout; o++) {
    LLVMValueRef f[16];
    int nf = md_ops(outs[o], f, 16), j;
    unsigned v;

    for (j = 0; j + 1 < nf; j++) {
      const char *sv = md_string(f[j]);

      if (sv && !strcmp(sv, "air.render_target") && md_uint(f[j + 1], &v)) {
        si->elemRT[o] = v;
        break;
      }
    }
  }
}

// one argument descriptor: {index, "air.buffer" | "air.texture" | ..., "air.location_index", N, ...}
static void bind_param(struct StageInfo *si, LLVMValueRef desc, int stage_kind) {
  LLVMValueRef f[64];
  int nf = md_ops(desc, f, 64), j;
  unsigned pi, v;

  if (nf < 2 || !md_uint(f[0], &pi) || pi >= PD_MAX_PARAMS)
    return;

  for (j = 1; j < nf; j++) {
    const char *sv = md_string(f[j]);
    unsigned kind;

    if (!sv)
      continue;

    kind = pd_param_kind(sv, stage_kind);

    if (kind != PK_NONE) {
      si->pkind[pi] = kind;
    } else if (!strcmp(sv, "air.address_space") && j + 1 < nf && md_uint(f[j + 1], &v) && v == 3) {
      // threadgroup memory is an air.buffer in address space 3
      si->pkind[pi] = PK_TG_MEM;
    } else if (!strcmp(sv, "air.location_index") && j + 1 < nf && md_uint(f[j + 1], &v)) {
      si->ploc[pi] = v;
    }

    // framebuffer fetch reads the attachment it names
    if (kind == PK_DEST && j + 1 < nf && md_uint(f[j + 1], &v))
      si->ploc[pi] = v;
  }
}

// parameter bindings from the air.vertex / air.fragment / air.kernel entry for fn
static void bind_params(struct StageInfo *si, LLVMModuleRef mod, LLVMValueRef fn) {
  unsigned k, i;

  for (k = 0; k < PD_ARRAY_LEN(entry_lists); k++) {
    LLVMValueRef eps[16];
    int ne = named_md_ops(mod, entry_lists[k], eps, 16), e;

    for (e = 0; e < ne; e++) {
      LLVMValueRef ops[16], args[64];
      int no = md_ops(eps[e], ops, 16), na, d;

      if (no < 3 || ops[0] != fn)
        continue;

      if (k == PD_STAGE_VERTEX)
        mark_varyings(si, ops[1]);

      if (k == PD_STAGE_FRAGMENT)
        mark_targets(si, ops[1]);

      na = md_ops(ops[2], args, 64);
      for (d = 0; d < na; d++) {
        bind_param(si, args[d], (int)k);
      }
    }
  }

  for (i = 0; i < si->nparams; i++) {
    logmsg("AIRJITD:   param %u kind %u location %u", i, si->pkind[i], si->ploc[i]);
    if (si->pkind[i] == PK_DEST)
      si->readsDest = 1;
  }
}

// gives every air.* declaration a body, returning the lowering flags
static unsigned define_air(LLVMModuleRef mod) {
  unsigned flags = 0;

  for (LLVMValueRef f = LLVMGetFirstFunction(mod); f; f = LLVMGetNextFunction(f)) {
    size_t l = 0;
    const char *n = LLVMGetValueName2(f, &l);
    char nm[96];

    if (!LLVMIsDeclaration(f) || l <= 4 || memcmp(n, "air.", 4))
      continue;

    if (l >= sizeof(nm))
      l = sizeof(nm) - 1;

    memcpy(nm, n, l);
    nm[l] = 0;

    if (air_lower(mod, f, nm, &flags)) {
      char *ty = LLVMPrintTypeToString(LLVMGlobalGetValueType(f));
      logmsg("AIRJITD: no runtime for %s: %s", nm, ty);
      LLVMDisposeMessage(ty);
      pd_note("%s%s", pd_note_text()[0] ? "," : "no runtime:", nm);
    } else {
      logmsg("AIRJITD: defined %s", nm);
    }
  }

  return flags;
}

// void pd_wrap(ptr args, ptr out) { *out = fn(load slot 0, load slot 1, ...) }
static void build_wrapper(LLVMContextRef ctx, LLVMModuleRef mod, LLVMValueRef fn, unsigned np) {
  LLVMTypeRef ptr = LLVMPointerTypeInContext(ctx, 0), pts[2] = { ptr, ptr };
  LLVMTypeRef fnty = LLVMGlobalGetValueType(fn), i8 = LLVMInt8TypeInContext(ctx), i64 = LLVMInt64TypeInContext(ctx);
  LLVMValueRef wrap = LLVMAddFunction(mod, "pd_wrap", LLVMFunctionType(LLVMVoidTypeInContext(ctx), pts, 2, 0));
  LLVMBuilderRef b = LLVMCreateBuilderInContext(ctx);
  LLVMValueRef args[PD_MAX_PARAMS], call;
  unsigned i;

  LLVMPositionBuilderAtEnd(b, LLVMAppendBasicBlockInContext(ctx, wrap, "entry"));

  for (i = 0; i < np; i++) {
    LLVMValueRef idx = LLVMConstInt(i64, (unsigned long long)i * PD_ARG_SLOT, 0);
    LLVMValueRef slot = LLVMBuildInBoundsGEP2(b, i8, LLVMGetParam(wrap, 0), &idx, 1, "");
    args[i] = LLVMBuildLoad2(b, LLVMTypeOf(LLVMGetParam(fn, i)), slot, "");
  }

  call = LLVMBuildCall2(b, fnty, fn, args, np, "");

  // kernels return void
  if (LLVMGetTypeKind(LLVMGetReturnType(fnty)) != LLVMVoidTypeKind)
    LLVMBuildStore(b, call, LLVMGetParam(wrap, 1));

  LLVMBuildRetVoid(b);
  LLVMDisposeBuilder(b);
}

static int llvm_compile_stage(uint32_t id, const struct pd_metallib *lib, struct StageInfo *si) {
  LLVMContextRef ctx;
  LLVMModuleRef mod;
  LLVMValueRef fn;
  uint64_t addr = 0;
  int rc;

  if ((rc = parse_module(lib, &ctx, &mod)))
    return rc;

  fn = LLVMGetNamedFunction(mod, lib->entry);

  if (!fn)
    return PD_ERR_NO_ENTRY;

  memset(si, 0, sizeof(*si));
  si->nparams = LLVMCountParams(fn);

  if (si->nparams > PD_MAX_PARAMS)
    return PD_ERR_TOO_MANY_PARAMS;

  if ((rc = result_layout(si, LLVMGetReturnType(LLVMGlobalGetValueType(fn)))))
    return rc;

  param_layout(si, fn);
  bind_params(si, mod, fn);
  si->usesDeriv = (define_air(mod) & AIR_USES_DERIV) != 0;
  build_wrapper(ctx, mod, fn, si->nparams);

  if ((rc = jit_module(ctx, mod, "pd_wrap", &addr))) {
    pd_note(" (%s)", lib->entry);
    return rc;
  }

  si->entry = (pd_stage_entry)(uintptr_t)addr;
  logmsg("AIRJITD: stage %u '%s' wrapped at 0x%llx, %u params, %u result bytes", id, lib->entry,
         (unsigned long long)addr, si->nparams, si->outBytes);
  return 0;
}

// flatten a metadata node: strings and integers inline, nested nodes indented
static void dump_md(LLVMValueRef v, const char *what, int depth) {
  char line[400];
  int pos = 0, got;
  unsigned i;
  LLVMValueRef ops[64];

  if (depth > 3 || !v || (got = md_ops(v, ops, 64)) < 0)
    return;

  for (i = 0; i < (unsigned)got && pos < (int)sizeof(line) - 40; i++) {
    unsigned sl = 0;
    const char *sv = ops[i] ? LLVMGetMDString(ops[i], &sl) : 0;

    if (sv)
      pos += snprintf(line + pos, sizeof(line) - pos, "%.*s ", (int)sl, sv);
    else if (ops[i] && LLVMIsAConstantInt(ops[i]))
      pos += snprintf(line + pos, sizeof(line) - pos, "%llu ", LLVMConstIntGetZExtValue(ops[i]));
    else if (ops[i] && LLVMIsAMDNode(ops[i]))
      pos += snprintf(line + pos, sizeof(line) - pos, "{%u} ", i);
    else
      pos += snprintf(line + pos, sizeof(line) - pos, "<v> ");
  }

  logmsg("AIRJITD:   %s%*s%s", what, depth * 2, "", line);

  for (i = 0; i < (unsigned)got; i++) {
    if (ops[i] && !md_string(ops[i]) && LLVMIsAMDNode(ops[i]))
      dump_md(ops[i], what, depth + 1);
  }
}

static void log_signature(uint32_t id, const char *name, LLVMValueRef fn) {
  char *t = LLVMPrintTypeToString(LLVMGlobalGetValueType(fn));

  logmsg("AIRJITD: function %u '%s' type %s", id, name, t);
  LLVMDisposeMessage(t);

  for (unsigned i = 0; i < LLVMCountParams(fn); i++) {
    size_t pl = 0;
    const char *pn = LLVMGetValueName2(LLVMGetParam(fn, i), &pl);
    char *pt = LLVMPrintTypeToString(LLVMTypeOf(LLVMGetParam(fn, i)));
    logmsg("AIRJITD:   param %u '%.*s' %s", i, (int)pl, pn ? pn : "", pt);
    LLVMDisposeMessage(pt);
  }
}

// what the JIT would need for this function: its type, every declared but undefined
// symbol (the air.* intrinsics a runtime must provide) and the module's named metadata
static int llvm_introspect(uint32_t id, const struct pd_metallib *lib) {
  LLVMContextRef ctx;
  LLVMModuleRef mod;
  LLVMValueRef fn;
  int rc;

  logmsg("AIRJITD: function %u '%s' bitcode %u bytes", id, lib->entry, lib->bitcode_len);

  if ((rc = parse_module(lib, &ctx, &mod)))
    return rc;

  if ((fn = LLVMGetNamedFunction(mod, lib->entry)))
    log_signature(id, lib->entry, fn);
  else
    logmsg("AIRJITD: function %u '%s' not in module", id, lib->entry);

  for (LLVMValueRef f = LLVMGetFirstFunction(mod); f; f = LLVMGetNextFunction(f)) {
    size_t l = 0;
    const char *n;
    char *t;

    if (!LLVMIsDeclaration(f))
      continue;

    n = LLVMGetValueName2(f, &l);
    t = LLVMPrintTypeToString(LLVMGlobalGetValueType(f));
    logmsg("AIRJITD:   needs %.*s %s", (int)l, n, t);
    LLVMDisposeMessage(t);
  }

  for (LLVMNamedMDNodeRef md = LLVMGetFirstNamedMetadata(mod); md; md = LLVMGetNextNamedMetadata(md)) {
    size_t l = 0;
    const char *n = LLVMGetNamedMetadataName(md, &l);
    logmsg("AIRJITD:   metadata %.*s", (int)l, n);
  }

  // one node per entry point: the function, then its argument descriptors
  for (unsigned k = 0; k < PD_ARRAY_LEN(entry_lists); k++) {
    LLVMValueRef ops[8];
    int n = named_md_ops(mod, entry_lists[k], ops, 8);

    for (int i = 0; i < n; i++) {
      dump_md(ops[i], entry_lists[k], 0);
    }
  }

  return 0;
}

static int llvm_compile_legacy(uint32_t id, const struct pd_metallib *lib, pd_legacy_entry *entry) {
  LLVMContextRef ctx;
  LLVMModuleRef mod;
  uint64_t addr = 0;
  int rc;

  if ((rc = parse_module(lib, &ctx, &mod)))
    return rc;

  if ((rc = jit_module(ctx, mod, lib->entry, &addr)))
    return rc;

  *entry = (pd_legacy_entry)(uintptr_t)addr;
  logmsg("AIRJITD: compiled function %u '%s' at 0x%llx", id, lib->entry, (unsigned long long)addr);
  return 0;
}

const struct pd_backend pd_llvm_backend = {
  .name = "llvm",
  .init = llvm_init,
  .compile_stage = llvm_compile_stage,
  .introspect = llvm_introspect,
  .compile_legacy = llvm_compile_legacy,
};
