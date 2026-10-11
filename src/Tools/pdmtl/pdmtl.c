// pdmtl over airjitd: one arena stands in for the bridge, jobs go through pd_exec_* (Vulkan when
// AIRJITD_VULKAN=1 and the executor takes the job, otherwise the CPU JIT)
#include "pdmtl.h"

#include <dlfcn.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airjitd.h"
#include "backend.h"
#include "bridge.h"
#include "exec.h"
#ifdef AIRJITD_OPENGL
#include "opengl/gl.h"
#endif
#include "metallib.h"
#include "stage.h"
#include "texture.h"
#ifdef AIRJITD_VULKAN
#include "vulkan/vk.h"
#endif

#include <llvm-c/BitReader.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassBuilder.h>

struct pdmtl_fn {
  char name[96];
  uint32_t id, off, size;
};

struct pdmtl_lib {
  unsigned n;
  struct pdmtl_fn fn[64];
};

static uint8_t *arena;
static size_t arena_size, arena_top;
static uint32_t next_id = 1;

// the daemon's fault guard, minus the bridge reporting
struct guard {
  sigjmp_buf jmp;
  volatile int sig;
};

static pthread_key_t gkey;
static pthread_once_t gonce = PTHREAD_ONCE_INIT;
static sigjmp_buf job_jb;

static void gkey_init(void) {
  pthread_key_create(&gkey, 0);
}

int pd_guarded(void (*fn)(void *), void *arg) {
  struct guard g;

  pthread_once(&gonce, gkey_init);
  g.sig = 0;
  if (sigsetjmp(g.jmp, 1)) {
    pthread_setspecific(gkey, 0);
    return g.sig;
  }
  pthread_setspecific(gkey, &g);
  fn(arg);
  pthread_setspecific(gkey, 0);
  return 0;
}

void pd_job_abort(int sig, const char *why) {
  fprintf(stderr, "pdmtl: job aborted (signal %d): %s\n", sig, why ? why : "");
  siglongjmp(job_jb, 1);
}

void pd_job_blame(uint32_t id) {
  (void)id;
}

#if defined(__APPLE__) && defined(__x86_64__)
static void print_frame(void *pc) {
  Dl_info di;

  if (dladdr(pc, &di) && di.dli_fname)
    fprintf(stderr, "pdmtl:   %p %s(%s+0x%lx)\n", pc, di.dli_fname, di.dli_sname ? di.dli_sname : "?",
            (unsigned long)((char *)pc - (char *)(di.dli_saddr ? di.dli_saddr : di.dli_fbase)));
  else
    fprintf(stderr, "pdmtl:   %p\n", pc);
}
#endif

static void on_signal(int sig, siginfo_t *si, void *uc) {
  struct guard *g;

  // async-signal-unsafe, yet it is the only trace a fault leaves
  fprintf(stderr, "pdmtl: signal %d, address %p\n", sig, si ? si->si_addr : 0);
  pthread_once(&gonce, gkey_init);
  g = pthread_getspecific(gkey);
#if defined(__APPLE__) && defined(__x86_64__)
  {
    static int once;
    _STRUCT_X86_THREAD_STATE64 *ss = &((ucontext_t *)uc)->uc_mcontext->__ss;
    Dl_info here, at;
    void **fp = (void **)ss->__rbp;

    // the first fault names the pc, the return address on the stack (a call through null has pc 0) and the
    // frame-pointer chain
    if (!once) {
      Dl_info rd;

      // objc_msgSend faults: the receiver and where it lives
      fprintf(stderr, "pdmtl:   rdi %p rsi %p rax %p", (void *)ss->__rdi, (void *)ss->__rsi, (void *)ss->__rax);
      if (dladdr((void *)ss->__rdi, &rd) && rd.dli_fname)
        fprintf(stderr, " (rdi in %s %s)", rd.dli_fname, rd.dli_sname ? rd.dli_sname : "");
      fprintf(stderr, "\n");
      print_frame((void *)ss->__rip);
      print_frame(*(void **)ss->__rsp);
      for (int i = 0; i < 24 && fp && ((uintptr_t)fp & 7) == 0 && fp[1]; i++) {
        print_frame(fp[1]);
        if ((void **)fp[0] <= fp)
          break;
        fp = (void **)fp[0];
      }
      // code built without frame pointers (Mesa) leaves the chain: words on the stack that land on a named
      // function are likely return addresses
      {
        void **sp = (void **)ss->__rsp;
        int shown = 0;

        fprintf(stderr, "pdmtl:   stack scan:\n");
        for (int i = 0; i < 512 && shown < 16; i++) {
          Dl_info si2;

          if (dladdr(sp[i], &si2) && si2.dli_sname && si2.dli_saddr && sp[i] != si2.dli_saddr) {
            print_frame(sp[i]);
            shown++;
          }
        }
      }
    }
    once = 1;
    // an abort, or a fault in another library's code (outside this library and JIT code), is the app's own crash:
    // let it happen
    if (sig == SIGABRT || (!g && dladdr((void *)on_signal, &here) && dladdr((void *)ss->__rip, &at) && at.dli_fbase &&
        at.dli_fbase != here.dli_fbase)) {
      signal(sig, SIG_DFL);
      return;
    }
  }
#else
  (void)uc;
#endif
  if (g) {
    g->sig = sig;
    siglongjmp(g->jmp, 1);
  }
  siglongjmp(job_jb, 1);
}

static uint32_t alloc(size_t len) {
  size_t off = (arena_top + 255) & ~(size_t)255;

  if (off + len > arena_size) {
    fprintf(stderr, "pdmtl: arena full\n");
    abort();
  }
  arena_top = off + len;
  return (uint32_t)off;
}

int pdmtl_init(size_t bytes) {
  static const int sigs[] = { SIGILL, SIGTRAP, SIGFPE, SIGBUS, SIGSEGV, SIGABRT };
  struct sigaction sa;
  unsigned k;

  arena_size = bytes ? bytes : PD_BRIDGE_BYTES;
  arena = calloc(1, arena_size);
  if (!arena)
    return -1;
  // the request block sits where the kext puts it
  arena_top = PD_BRIDGE_REQ + 0x10000;
  // AIRJITD_VERBOSE: the executor's per-draw lines, as airjitd -v gives the daemon
  pd_log_verbose = getenv("AIRJITD_VERBOSE") != NULL;
  pd_llvm_backend.init();

  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = on_signal;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  for (k = 0; k < PD_ARRAY_LEN(sigs); k++)
    sigaction(sigs[k], &sa, 0);
  return 0;
}

struct pdmtl_buf pdmtl_new_buffer(const void *bytes, size_t len) {
  struct pdmtl_buf b;

  b.off = alloc(len);
  b.size = (uint32_t)len;
  b.contents = arena + b.off;
  if (bytes)
    memcpy(b.contents, bytes, len);
  return b;
}

struct pdmtl_tex pdmtl_new_texture(uint32_t w, uint32_t h, uint32_t fmt) {
  struct pdmtl_tex t;

  t.w = w, t.h = h, t.fmt = fmt;
  t.bpr = w * tex_bpp(fmt);
  t.off = alloc((size_t)t.bpr * h);
  t.contents = arena + t.off;
  return t;
}

static uint64_t rd(const uint8_t *p, unsigned n) {
  uint64_t v = 0;

  while (n--)
    v = v << 8 | p[n];
  return v;
}

// MTLB: a header of section offsets, then one tag list per function (NAME, MDSZ, OFFT, ... ENDT)
// whose OFFT third entry places its bitcode module inside the bitcode section
struct pdmtl_lib *pdmtl_lib_load(const char *path) {
  struct pdmtl_lib *lib = 0;
  uint8_t *m;
  long len;
  FILE *f = fopen(path, "rb");

  if (!f)
    return 0;
  fseek(f, 0, SEEK_END);
  len = ftell(f);
  rewind(f);
  m = malloc(len > 0 ? len : 1);
  if (len > 0 && fread(m, 1, len, f) == (size_t)len)
    lib = pdmtl_lib_load_data(m, (size_t)len);
  fclose(f);
  free(m);
  return lib;
}

struct pdmtl_lib *pdmtl_lib_load_data(const void *data, size_t size) {
  const uint8_t *m = data;
  long len = (long)size;
  struct pdmtl_lib *lib;

  if (len < 0x58 || memcmp(m, "MTLB", 4))
    return 0;

  lib = calloc(1, sizeof(*lib));
  uint64_t flist = rd(m + 0x18, 8), bcsec = rd(m + 0x48, 8);
  uint32_t nfn = (uint32_t)rd(m + flist, 4);
  const uint8_t *p = m + flist + 4;

  for (uint32_t i = 0; i < nfn && lib->n < PD_ARRAY_LEN(lib->fn); i++) {
    uint32_t esize = (uint32_t)rd(p, 4);
    const uint8_t *t = p + 4, *end = p + esize;
    char name[96] = "";
    uint64_t bcoff = 0, bclen = 0;

    while (t + 4 <= end && memcmp(t, "ENDT", 4)) {
      uint32_t tl = (uint32_t)rd(t + 4, 2);
      const uint8_t *v = t + 6;

      if (!memcmp(t, "NAME", 4))
        snprintf(name, sizeof(name), "%.*s", (int)tl, (const char *)v);
      else if (!memcmp(t, "MDSZ", 4))
        bclen = rd(v, 8);
      else if (!memcmp(t, "OFFT", 4))
        bcoff = rd(v + 16, 8);
      t = v + tl;
    }
    p = end;
    if (!name[0] || !bclen || bcsec + bcoff + bclen > (uint64_t)len)
      continue;

    // a single-function blob in the arena, shaped the way pd_metallib_open reads the driver's
    struct pdmtl_fn *fn = &lib->fn[lib->n++];
    size_t nl = strlen(name) + 1;

    snprintf(fn->name, sizeof(fn->name), "%s", name);
    fn->id = next_id++;
    fn->size = (uint32_t)(6 + nl + bclen);
    fn->off = alloc(fn->size);
    memcpy(arena + fn->off, "NAME", 4);
    arena[fn->off + 4] = (uint8_t)nl, arena[fn->off + 5] = (uint8_t)(nl >> 8);
    memcpy(arena + fn->off + 6, name, nl);
    memcpy(arena + fn->off + 6 + nl, m + bcsec + bcoff, bclen);
  }
  return lib;
}

unsigned pdmtl_lib_count(const struct pdmtl_lib *lib) {
  return lib->n;
}

const char *pdmtl_lib_name(const struct pdmtl_lib *lib, unsigned i) {
  return i < lib->n ? lib->fn[i].name : 0;
}

struct pdmtl_fn *pdmtl_fn_get(struct pdmtl_lib *lib, const char *name) {
  for (unsigned i = 0; i < lib->n; i++) {
    if (!strcmp(lib->fn[i].name, name))
      return &lib->fn[i];
  }
  return 0;
}

static unsigned type_bytes(LLVMTypeRef ty) {
  switch (LLVMGetTypeKind(ty)) {
  case LLVMIntegerTypeKind:
    return (LLVMGetIntTypeWidth(ty) + 7) / 8;
  case LLVMHalfTypeKind:
    return 2;
  case LLVMFloatTypeKind:
    return 4;
  case LLVMDoubleTypeKind:
    return 8;
  case LLVMVectorTypeKind:
    return LLVMGetVectorSize(ty) * type_bytes(LLVMGetElementType(ty));
  default:
    return 0;
  }
}

// a constant's value from its bytes, for the LLVM type of the global holding it
static LLVMValueRef const_from_bytes(LLVMTypeRef ty, const uint8_t *b) {
  switch (LLVMGetTypeKind(ty)) {
  case LLVMIntegerTypeKind: {
    unsigned w = LLVMGetIntTypeWidth(ty), n = (w + 7) / 8;
    unsigned long long v = 0;

    for (unsigned i = 0; i < n && i < 8; i++)
      v |= (unsigned long long)b[i] << (8 * i);
    return LLVMConstInt(ty, w == 1 ? v != 0 : v, 0);
  }
  case LLVMHalfTypeKind: {
    uint16_t h = (uint16_t)(b[0] | b[1] << 8);
    int e = h >> 10 & 31, m = h & 1023;
    double v = e == 0 ? m / 16777216.0 : e == 31 ? (m ? 0.0 / 0.0 : 1.0 / 0.0) : (1024 + m) * (double)(1 << e) / 33554432.0;

    return LLVMConstReal(ty, h >> 15 ? -v : v);
  }
  case LLVMFloatTypeKind: {
    float f;

    memcpy(&f, b, 4);
    return LLVMConstReal(ty, f);
  }
  case LLVMDoubleTypeKind: {
    double d;

    memcpy(&d, b, 8);
    return LLVMConstReal(ty, d);
  }
  case LLVMVectorTypeKind: {
    LLVMTypeRef et = LLVMGetElementType(ty);
    unsigned n = LLVMGetVectorSize(ty), es = type_bytes(et);
    LLVMValueRef e[16];

    if (n > 16)
      return 0;
    for (unsigned i = 0; i < n; i++) {
      if (!(e[i] = const_from_bytes(et, b + i * es)))
        return 0;
    }
    return LLVMConstVector(e, n);
  }
  default:
    return 0;
  }
}

static LLVMValueRef strip_casts(LLVMValueRef v) {
  while (v && LLVMIsAConstantExpr(v))
    v = LLVMGetOperand(v, 0);
  return v;
}

struct pdmtl_fn *pdmtl_fn_specialize(struct pdmtl_fn *fn, const struct pdmtl_constant *c, unsigned nc) {
  size_t nl = strlen(fn->name) + 1;
  const uint8_t *bc = arena + fn->off + 6 + nl;
  size_t bclen = fn->size - 6 - nl;
  LLVMContextRef ctx = LLVMContextCreate();
  LLVMMemoryBufferRef buf = LLVMCreateMemoryBufferWithMemoryRangeCopy((const char *)bc, bclen, fn->name);
  LLVMModuleRef mod = 0;
  struct pdmtl_fn *out = fn;

  if (LLVMParseBitcodeInContext2(ctx, buf, &mod)) {
    LLVMContextDispose(ctx);
    return fn;
  }

  unsigned nfc = LLVMGetNamedMetadataNumOperands(mod, "air.function_constants");
  LLVMValueRef ctors = LLVMGetNamedGlobal(mod, "llvm.global_ctors");

  if (!nfc && !ctors)
    goto done;

  // each !air.function_constants entry: {global, type name, name, index, required}
  LLVMValueRef *md = calloc(nfc ? nfc : 1, sizeof(*md));
  LLVMValueRef isdef = LLVMGetNamedFunction(mod, "air.is_function_constant_defined");

  LLVMGetNamedMetadataOperands(mod, "air.function_constants", md);
  for (unsigned i = 0; i < nfc; i++) {
    LLVMValueRef ops[8];
    unsigned no = LLVMGetMDNodeNumOperands(md[i]);

    if (no < 4 || no > 8)
      continue;
    LLVMGetMDNodeOperands(md[i], ops);

    LLVMValueRef g = strip_casts(ops[0]);
    unsigned nlen = 0;
    const char *name = LLVMGetMDString(ops[2], &nlen);
    unsigned index = LLVMIsAConstantInt(ops[3]) ? (unsigned)LLVMConstIntGetZExtValue(ops[3]) : ~0u;
    const struct pdmtl_constant *v = 0;

    if (!g || !LLVMIsAGlobalVariable(g))
      continue;
    for (unsigned k = 0; k < nc && !v; k++) {
      if (c[k].name[0] ? name && strlen(c[k].name) == nlen && !memcmp(c[k].name, name, nlen) : c[k].index == index)
        v = &c[k];
    }

    LLVMTypeRef ty = LLVMGlobalGetValueType(g);
    LLVMValueRef init = v && type_bytes(ty) <= sizeof(v->bytes) ? const_from_bytes(ty, v->bytes) : 0;

    LLVMSetInitializer(g, init ? init : LLVMConstNull(ty));
    LLVMSetExternallyInitialized(g, 0);
    LLVMSetGlobalConstant(g, 1);

    // is_function_constant_defined(g) becomes a plain true or false
    for (LLVMUseRef u = isdef ? LLVMGetFirstUse(isdef) : 0; u;) {
      LLVMValueRef call = LLVMGetUser(u);

      u = LLVMGetNextUse(u);
      if (LLVMIsACallInst(call) && strip_casts(LLVMGetOperand(call, 0)) == g) {
        LLVMReplaceAllUsesWith(call, LLVMConstInt(LLVMInt1TypeInContext(ctx), init != 0, 0));
        LLVMInstructionEraseFromParent(call);
      }
    }
  }
  free(md);

  // globalopt runs the static initialisers (air.static_init) at compile time and folds what they computed
  LLVMPassBuilderOptionsRef o = LLVMCreatePassBuilderOptions();
  LLVMErrorRef err = LLVMRunPasses(mod, "globalopt", 0, o);

  LLVMDisposePassBuilderOptions(o);
  if (err)
    LLVMConsumeError(err);
  if ((ctors = LLVMGetNamedGlobal(mod, "llvm.global_ctors")) && LLVMGetInitializer(ctors) &&
      LLVMGetArrayLength2(LLVMGlobalGetValueType(ctors)))
    fprintf(stderr, "pdmtl: %s keeps static initialisers the executor does not run\n", fn->name);

  LLVMMemoryBufferRef w = LLVMWriteBitcodeToMemoryBuffer(mod);
  size_t wl = LLVMGetBufferSize(w);

  out = calloc(1, sizeof(*out));
  snprintf(out->name, sizeof(out->name), "%s", fn->name);
  out->id = next_id++;
  out->size = (uint32_t)(6 + nl + wl);
  out->off = alloc(out->size);
  memcpy(arena + out->off, arena + fn->off, 6 + nl);
  memcpy(arena + out->off + 6 + nl, LLVMGetBufferStart(w), wl);
  LLVMDisposeMemoryBuffer(w);
done:
  LLVMDisposeModule(mod);
  LLVMContextDispose(ctx);
  return out;
}

unsigned pdmtl_fn_constants(const struct pdmtl_fn *fn, struct pdmtl_constant_info *out, unsigned max) {
  size_t nl = strlen(fn->name) + 1;
  LLVMContextRef ctx = LLVMContextCreate();
  LLVMMemoryBufferRef buf =
      LLVMCreateMemoryBufferWithMemoryRangeCopy((const char *)arena + fn->off + 6 + nl, fn->size - 6 - nl, fn->name);
  LLVMModuleRef mod = 0;
  unsigned n = 0;

  if (!LLVMParseBitcodeInContext2(ctx, buf, &mod)) {
    unsigned nfc = LLVMGetNamedMetadataNumOperands(mod, "air.function_constants");
    LLVMValueRef *md = calloc(nfc ? nfc : 1, sizeof(*md));

    LLVMGetNamedMetadataOperands(mod, "air.function_constants", md);
    for (unsigned i = 0; i < nfc && n < max; i++) {
      LLVMValueRef ops[8];
      unsigned no = LLVMGetMDNodeNumOperands(md[i]), tl = 0, nlen = 0;

      if (no < 4 || no > 8)
        continue;
      LLVMGetMDNodeOperands(md[i], ops);

      const char *type = LLVMGetMDString(ops[1], &tl), *name = LLVMGetMDString(ops[2], &nlen);
      struct pdmtl_constant_info *o = &out[n++];

      memset(o, 0, sizeof(*o));
      snprintf(o->type, sizeof(o->type), "%.*s", type ? (int)tl : 0, type ? type : "");
      snprintf(o->name, sizeof(o->name), "%.*s", name ? (int)nlen : 0, name ? name : "");
      o->index = LLVMIsAConstantInt(ops[3]) ? (uint32_t)LLVMConstIntGetZExtValue(ops[3]) : ~0u;
      o->required = no > 4 && LLVMIsAConstantInt(ops[4]) && LLVMConstIntGetZExtValue(ops[4]);
    }
    free(md);
    LLVMDisposeModule(mod);
  }
  LLVMContextDispose(ctx);
  return n;
}

static int ready(uint32_t id, const uint8_t *mtl, uint32_t size, const char *what) {
  struct pd_metallib ml;
  int rc;

  if (pd_stage(id)->entry)
    return 0;
  if ((rc = pd_metallib_open(mtl, size, &ml)))
    return rc;
  if ((rc = pd_llvm_backend.compile_stage(id, &ml, pd_stage(id))))
    fprintf(stderr, "pdmtl: %s %u failed to compile: %d\n", what, id, rc);
  return rc;
}

int pdmtl_dispatch_threads(struct pdmtl_fn *fn, const struct pdmtl_buf *bufs, unsigned nbufs, const uint64_t grid[3],
                           const uint64_t tpg[3]) {
  struct PDComputeRequest *r = (struct PDComputeRequest *)(arena + PD_BRIDGE_REQ);

  memset(r, 0, sizeof(*r));
  r->fn = fn->id, r->mOff = fn->off, r->mSize = fn->size, r->threads = 1;
  for (unsigned k = 0; k < 3; k++) {
    r->grid[k] = grid[k];
    r->tpg[k] = tpg[k];
  }
  for (unsigned i = 0; i < nbufs && i < PD_CS_BUFS; i++) {
    r->buf[i].id = i + 1;
    r->buf[i].off = bufs[i].off;
    r->buf[i].size = bufs[i].size;
  }
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_compute(r, arena, (uint32_t)arena_size, ready);
}

int pdmtl_draw(struct pdmtl_fn *vfn, struct pdmtl_fn *ffn, uint32_t prim, uint32_t count, const struct pdmtl_buf *vbufs,
               unsigned nvbufs, struct pdmtl_tex *t) {
  struct PDDrawRequest *r = (struct PDDrawRequest *)(arena + PD_BRIDGE_REQ);

  memset(r, 0, sizeof(*r));
  r->vfn = vfn->id, r->vmOff = vfn->off, r->vmSize = vfn->size;
  r->ffn = ffn->id, r->fmOff = ffn->off, r->fmSize = ffn->size;
  r->prim = prim, r->count = count;
  for (unsigned i = 0; i < nvbufs && i < PD_DRAW_BUFS; i++) {
    r->vbuf[i].id = i + 1;
    r->vbuf[i].off = vbufs[i].off;
    r->vbuf[i].size = vbufs[i].size;
  }
  r->targetOff = t->off, r->tw = t->w, r->th = t->h, r->tbpr = t->bpr, r->tfmt = t->fmt;
  r->viewport[2] = t->w, r->viewport[3] = t->h, r->viewport[5] = 1;
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_draw(r, arena, (uint32_t)arena_size, 1, ready);
}

int pdmtl_present_native(const struct pdmtl_tex *target) {
#ifdef AIRJITD_OPENGL
  if (target && pd_exec_on_opengl) return pd_gl_present(target->off);
#endif
  return -1;
}

int pdmtl_last_on_opengl(void) {
  return pd_exec_on_opengl;
}

int pdmtl_last_on_vulkan(void) {
  return pd_exec_on_vulkan;
}

int pdmtl_fn_compile(struct pdmtl_fn *fn) {
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return ready(fn->id, arena + fn->off, fn->size, fn->name);
}

uint8_t *pdmtl_arena(void) {
  return arena;
}

uint32_t pdmtl_arena_size(void) {
  return (uint32_t)arena_size;
}

uint32_t pdmtl_alloc(size_t len) {
  return alloc(len);
}

void pdmtl_fn_ref(const struct pdmtl_fn *fn, uint32_t *id, uint32_t *off, uint32_t *size) {
  *id = fn->id, *off = fn->off, *size = fn->size;
}

int pdmtl_exec_compute(void *req) {
  struct PDComputeRequest *r = (struct PDComputeRequest *)(arena + PD_BRIDGE_REQ);

  memcpy(r, req, sizeof(*r));
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_compute(r, arena, (uint32_t)arena_size, ready);
}

int pdmtl_exec_draw(void *req) {
  struct PDDrawRequest *r = (struct PDDrawRequest *)(arena + PD_BRIDGE_REQ);

  memcpy(r, req, sizeof(*r));
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_draw(r, arena, (uint32_t)arena_size, 1, ready);
}

int pdmtl_exec_tile(void *req) {
  struct PDTileRequest *r = (struct PDTileRequest *)(arena + PD_BRIDGE_REQ);

  memcpy(r, req, sizeof(*r));
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_tile(r, arena, (uint32_t)arena_size, ready);
}

int pdmtl_draw_ex(const struct pdmtl_draw_desc *d, struct pdmtl_tex *t) {
  struct PDDrawRequest *r = (struct PDDrawRequest *)(arena + PD_BRIDGE_REQ);

  memset(r, 0, sizeof(*r));
  r->vfn = d->vfn->id, r->vmOff = d->vfn->off, r->vmSize = d->vfn->size;
  r->ffn = d->ffn->id, r->fmOff = d->ffn->off, r->fmSize = d->ffn->size;
  r->prim = d->prim, r->count = d->count, r->instances = d->instances;
  for (unsigned i = 0; i < d->nvbufs && i < PD_DRAW_BUFS; i++) {
    r->vbuf[i].id = i + 1, r->vbuf[i].off = d->vbufs[i].off, r->vbuf[i].size = d->vbufs[i].size;
  }
  for (unsigned i = 0; i < d->nfbufs && i < PD_DRAW_BUFS; i++) {
    r->fbuf[i].id = i + 1, r->fbuf[i].off = d->fbufs[i].off, r->fbuf[i].size = d->fbufs[i].size;
  }
  r->targetOff = t->off, r->tw = t->w, r->th = t->h, r->tbpr = t->bpr, r->tfmt = t->fmt;
  // known state, all channels written: source-alpha over when blending, else a plain store
  r->blend = 1u | 15u << 28;
  if (d->blend)
    r->blend |= 2u | 4u << 2 | 5u << 7 | 1u << 12 | 5u << 17;
  r->viewport[2] = t->w, r->viewport[3] = t->h, r->viewport[5] = 1;
  if (sigsetjmp(job_jb, 1))
    return PD_ERR_FAULT;
  return pd_exec_draw(r, arena, (uint32_t)arena_size, 1, ready);
}

void pdmtl_resident(int on) {
#ifdef AIRJITD_OPENGL
  if (pd_gl_enabled()) pd_gl_set_resident(on);
#endif
#ifdef AIRJITD_VULKAN
#ifdef AIRJITD_OPENGL
  pd_vk_set_resident(on && !pd_gl_enabled(), arena, (uint32_t)arena_size);
#else
  pd_vk_set_resident(on, arena, (uint32_t)arena_size);
#endif
#endif
  (void)on;
}

void pdmtl_dirty(uint32_t off, uint32_t len) {
#ifdef AIRJITD_OPENGL
  pd_gl_dirty(off, len);
#endif
#ifdef AIRJITD_VULKAN
  pd_vk_dirty(off, len);
#endif
  (void)off, (void)len;
}

void pdmtl_sync(uint32_t off, uint32_t len) {
#ifdef AIRJITD_OPENGL
  if (pd_gl_sync(off, len)) fprintf(stderr, "pdmtl: OpenGL readback failed\n");
#endif
#ifdef AIRJITD_VULKAN
  pd_vk_sync(off, len);
#endif
  (void)off, (void)len;
}

void pdmtl_sync_all(void) {
#ifdef AIRJITD_OPENGL
  if (pd_gl_sync(0, UINT32_MAX)) fprintf(stderr, "pdmtl: OpenGL readback failed\n");
#endif
#ifdef AIRJITD_VULKAN
  pd_vk_sync_all();
#endif
}

void pdmtl_finish(void) {
#ifdef AIRJITD_VULKAN
  pd_vk_finish();
#endif
}

void pdmtl_fill(const struct PDDrawTex *t, const float rgba[4]) {
#ifdef AIRJITD_OPENGL
  if (pd_gl_enabled() && pd_gl_clear(t,rgba,arena,(uint32_t)arena_size)) {
#ifdef AIRJITD_VULKAN
    pd_vk_dirty(t->off,t->bpr*t->h);
#endif
    return;
  }
#endif

  uint8_t px[16];
  unsigned bpp = tex_bpp(t->fmt);

#ifdef AIRJITD_VULKAN
  // a resident attachment is cleared where it lives
  if (pd_vk_clear(t, rgba, 0, 1))
    return;
  pd_vk_sync(t->off, t->bpr * t->h);
#endif
  pdmtl_sync(t->off, t->bpr * t->h);
  pd_texel_store(t->fmt, px, rgba);
  for (uint32_t y = 0; y < t->h; y++) {
    for (uint32_t x = 0; x < t->w; x++) {
      memcpy(arena + t->off + (size_t)y * t->bpr + (size_t)x * bpp, px, bpp);
    }
  }
  pdmtl_dirty(t->off, t->bpr * t->h);
}

void pdmtl_fill_depth(const struct PDDrawTex *t, float depth, unsigned stencil, int which) {
  unsigned bpp = tex_bpp(t->fmt);

#ifdef AIRJITD_VULKAN
  if (pd_vk_clear(t, &depth, stencil, which))
    return;
  pd_vk_sync(t->off, t->bpr * t->h);
#endif
  pdmtl_sync(t->off, t->bpr * t->h);
  for (uint32_t y = 0; y < t->h; y++) {
    for (uint32_t x = 0; x < t->w; x++) {
      uint8_t *p = arena + t->off + (size_t)y * t->bpr + (size_t)x * bpp;

      if ((which & PDMTL_CLEAR_DEPTH) && pd_fmt_is_depth(t->fmt))
        pd_depth_store(t->fmt, p, depth);
      if ((which & PDMTL_CLEAR_STENCIL) && pd_fmt_has_stencil(t->fmt))
        pd_stencil_store(t->fmt, p, (uint8_t)stencil);
    }
  }
  pdmtl_dirty(t->off, t->bpr * t->h);
}

// level k of slice s (a 3D texture's z) of a texture with a layout
static uint8_t *level_at(const struct PDDrawTex *t, const struct PDTexLayout *l, unsigned k, unsigned s, uint32_t *bpr) {
  uint32_t h = t->h >> k ? t->h >> k : 1;

  *bpr = k ? l->level[k].bpr : t->bpr;
  if (l->type == 7)
    return arena + t->off + (k ? l->level[k].off : 0) + (size_t)s * *bpr * h;
  return arena + t->off + (size_t)s * l->sliceBytes + (k ? l->level[k].off : 0);
}

static void gen_mips(const struct PDDrawTex *t, const struct PDTexLayout *l) {
  unsigned bpp = tex_bpp(t->fmt), levels = l->levels ? l->levels : 1, three = l->type == 7, k, s;
  unsigned slices = three ? 1 : l->slices ? l->slices : 1;

  if (!(l->flags & PD_TEXLAYOUT_SET) || pd_fmt_is_int(t->fmt) || pd_fmt_is_depth(t->fmt))
    return;
  for (k = 1; k < levels && k < PD_TEX_LEVELS; k++) {
    uint32_t w = t->w >> k ? t->w >> k : 1, h = t->h >> k ? t->h >> k : 1, d = three ? (l->slices >> k ? l->slices >> k : 1) : 1;
    uint32_t pw = t->w >> (k - 1) ? t->w >> (k - 1) : 1, ph = t->h >> (k - 1) ? t->h >> (k - 1) : 1;
    uint32_t pd = three ? (l->slices >> (k - 1) ? l->slices >> (k - 1) : 1) : 1;

    for (s = 0; s < slices; s++) {
      for (uint32_t z = 0; z < d; z++) {
        for (uint32_t y = 0; y < h; y++) {
          for (uint32_t x = 0; x < w; x++) {
            // a box over the 2x2 (x2 for 3D) texels of the level above, clamped at its edges
            float sum[4] = { 0 }, v[4];
            unsigned n = 0, c;
            uint32_t dbpr, sbpr;

            for (uint32_t dz = 0; dz < (three ? 2u : 1u); dz++) {
              for (uint32_t dy = 0; dy < 2; dy++) {
                for (uint32_t dx = 0; dx < 2; dx++) {
                  uint32_t sx = 2 * x + dx < pw ? 2 * x + dx : pw - 1, sy = 2 * y + dy < ph ? 2 * y + dy : ph - 1;
                  uint32_t sz = 2 * z + dz < pd ? 2 * z + dz : pd - 1;
                  const uint8_t *src = level_at(t, l, k - 1, three ? sz : s, &sbpr);

                  pd_texel_load(t->fmt, src + (size_t)sy * sbpr + (size_t)sx * bpp, v);
                  for (c = 0; c < 4; c++) {
                    sum[c] += v[c];
                  }
                  n++;
                }
              }
            }
            for (c = 0; c < 4; c++) {
              sum[c] /= (float)n;
            }
            pd_texel_store(t->fmt, level_at(t, l, k, three ? z : s, &dbpr) + (size_t)y * dbpr + (size_t)x * bpp, sum);
          }
        }
      }
    }
  }
}

void pdmtl_generate_mipmaps(const struct PDDrawTex *t, const struct PDTexLayout *l) {
  uint32_t span = l->sliceBytes ? l->sliceBytes * (l->slices ? l->slices : 1) : t->bpr * t->h * 2 * (l->slices ? l->slices : 1);

  // read the latest GPU contents before generating levels on the CPU.
  pdmtl_sync(t->off, span);
  gen_mips(t, l);
  pdmtl_dirty(t->off, span);
}

