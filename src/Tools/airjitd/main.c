#include <IOKit/IOKitLib.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "airjitd.h"
#include "backend.h"
#include "bridge.h"
#include "compute.h"
#include "metallib.h"
#include "raster.h"
#include "stage.h"

static const struct pd_backend *backend = &pd_llvm_backend;

struct pd_job {
  struct PDBridgeHeader *h;
  uint8_t *base;
  uint32_t size;
  io_connect_t conn;
  // the kext answers the doorbell selectors, so the loop sleeps in the kernel instead of polling
  int doorbells;
};

// a job that crashes fails instead of taking the daemon (and WindowServer) down
static sigjmp_buf job_jmp;
static volatile int in_job, job_fault;
// the stage being compiled or run, blamed when the job faults
static volatile uint32_t job_stage = ~0u;
// the faulting address, logged with the stage
static void *volatile job_addr;
static pd_legacy_entry legacy[PD_MAX_STAGES];

// raster workers run on other threads: a fault there unwinds to that thread's guard
struct pd_guard {
  sigjmp_buf jmp;
  volatile int sig;
};

static pthread_key_t guard_key;
static pthread_once_t guard_once = PTHREAD_ONCE_INIT;

static void guard_key_init(void) {
  pthread_key_create(&guard_key, 0);
}

int pd_guarded(void (*fn)(void *), void *arg) {
  struct pd_guard g;

  pthread_once(&guard_once, guard_key_init);
  g.sig = 0;

  if (sigsetjmp(g.jmp, 1)) {
    pthread_setspecific(guard_key, 0);
    return g.sig;
  }

  pthread_setspecific(guard_key, &g);
  fn(arg);
  pthread_setspecific(guard_key, 0);
  return 0;
}

void pd_job_abort(int sig, const char *why) {
  if (why)
    logmsg("AIRJITD: fatal error in stage %u: %s", job_stage, why);

  if (!in_job)
    return;

  job_fault = sig;
  in_job = 0;
  siglongjmp(job_jmp, 1);
}

void pd_job_blame(uint32_t id) {
  job_stage = id;
}

static void job_signal(int sig, siginfo_t *si, void *uc) {
  struct pd_guard *g;

  (void)uc;
  job_addr = si ? si->si_addr : 0;

  pthread_once(&guard_once, guard_key_init);
  g = pthread_getspecific(guard_key);

  if (g) {
    g->sig = sig;
    siglongjmp(g->jmp, 1);
  }

  if (!in_job) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }

  pd_job_abort(sig, 0);
}

static void install_fault_handlers(void) {
  static const int sigs[] = { SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS, SIGSEGV };
  unsigned k;

  for (k = 0; k < PD_ARRAY_LEN(sigs); k++) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = job_signal;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(sigs[k], &sa, 0);
  }
}

// compiles stage id from the metallib at mtl unless it already is
static int stage_ready(uint32_t id, const uint8_t *mtl, uint32_t size, const char *what) {
  struct pd_metallib lib;
  int rc;

  if (id >= PD_MAX_STAGES)
    return PD_ERR_BAD_STAGE;

  if (pd_stage_disabled(id))
    return PD_ERR_STAGE_DISABLED;

  if (pd_stage(id)->entry)
    return 0;

  job_stage = id;
  logmsg("AIRJITD: compiling %s %u", what, id);

  if ((rc = pd_metallib_open(mtl, size, &lib)))
    return rc;

  return backend->compile_stage(id, &lib, pd_stage(id));
}

static int job_introspect(struct pd_job *j) {
  struct pd_metallib lib;
  int rc;

  if (j->h->functionID >= PD_MAX_STAGES)
    return PD_ERR_BAD_FUNCTION;

  if ((rc = pd_metallib_open(j->base + PD_BRIDGE_REQ, j->h->metallibSize, &lib)))
    return rc;

  return backend->introspect(j->h->functionID, &lib);
}

static int job_draw(struct pd_job *j) {
  struct PDDrawRequest *r = (struct PDDrawRequest *)(j->base + PD_BRIDGE_REQ);
  int rc;

  if (r->vfn >= PD_MAX_STAGES || r->ffn >= PD_MAX_STAGES)
    rc = PD_ERR_BAD_STAGE;
  else if ((rc = stage_ready(r->vfn, j->base + r->vmOff, r->vmSize, "vertex stage")) == 0)
    rc = stage_ready(r->ffn, j->base + r->fmOff, r->fmSize, "fragment stage");

  // a fault while drawing is blamed on the fragment stage, which runs per pixel
  if (!rc) {
    job_stage = r->ffn;
    rc = pd_run_draw(r, j->base, j->size, j->h->serial);
  }

  logv("AIRJITD: draw %u: prim %u count %u %s, stages %u/%u, target %ux%u fmt %u -> %d", j->h->serial, r->prim,
         r->count, r->indexed ? "indexed" : "", r->vfn, r->ffn, r->tw, r->th, r->tfmt, rc);
  return rc;
}

static int job_compute(struct pd_job *j) {
  struct PDComputeRequest *r = (struct PDComputeRequest *)(j->base + PD_BRIDGE_REQ);
  int rc = stage_ready(r->fn, j->base + r->mOff, r->mSize, "kernel");

  if (!rc) {
    job_stage = r->fn;
    rc = pd_run_compute(r, j->base, j->size);
  }

  logv("AIRJITD: compute %u: kernel %u grid %llux%llux%llu (%s) tpg %llux%llux%llu -> %d", j->h->serial, r->fn,
         (unsigned long long)r->grid[0], (unsigned long long)r->grid[1], (unsigned long long)r->grid[2],
         r->threads ? "threads" : "groups", (unsigned long long)r->tpg[0], (unsigned long long)r->tpg[1],
         (unsigned long long)r->tpg[2], rc);
  return rc;
}

// the first-pixels path: fn(data, i) for every element of the grid
static int job_legacy(struct pd_job *j) {
  struct PDBridgeHeader *h = j->h;
  uint32_t fn = h->functionID;
  uint32_t *data;
  uint64_t count, i;
  int rc;

  if (fn >= PD_MAX_STAGES)
    return PD_ERR_BAD_FUNCTION;

  if (!legacy[fn]) {
    struct pd_metallib lib;

    if ((rc = pd_metallib_open(j->base + PD_BRIDGE_REQ, h->metallibSize, &lib)))
      return rc;

    if ((rc = backend->compile_legacy(fn, &lib, &legacy[fn])))
      return rc;
  }

  data = (uint32_t *)(j->base + h->dataOffset);
  count = h->grid[0] * h->grid[1] * h->grid[2];

  if (h->dataOffset > j->size || h->dataSize > j->size - h->dataOffset || count > h->dataSize / 4)
    return PD_ERR_BAD_DATA;

  for (i = 0; i < count; i++) {
    legacy[fn](data, (uint32_t)i);
  }

  return 0;
}

static const struct {
  uint32_t kind;
  int (*run)(struct pd_job *j);
} job_kinds[] = {
  { PD_JOB_LEGACY_COMPUTE, job_legacy },
  { PD_JOB_INTROSPECT, job_introspect },
  { PD_JOB_DRAW, job_draw },
  { PD_JOB_COMPUTE, job_compute },
};

static int run_job(struct pd_job *j) {
  unsigned i;

  for (i = 0; i < PD_ARRAY_LEN(job_kinds); i++) {
    if (job_kinds[i].kind == j->h->kind)
      return job_kinds[i].run(j);
  }

  return PD_ERR_BAD_JOB;
}

static void finish_job(struct PDBridgeHeader *h, int rc) {
  if (!rc) {
    __sync_synchronize();
    h->state = PD_STATE_DONE;
    logv("AIRJITD: job %u complete", h->serial);
    return;
  }

  // a stage that cannot link now never will, so it is not recompiled every draw
  if (rc == PD_ERR_JIT_LOOKUP)
    pd_stage_disable(job_stage);

  snprintf(h->error, sizeof(h->error), "daemon error %d stage %u %s", rc, job_stage, pd_note_text());
  __sync_synchronize();
  h->state = PD_STATE_FAILED;
  logmsg("AIRJITD: job %u failed %d", h->serial, rc);
}

// sleeps in the kext until a job is queued, bounded there to 20 ms
static void wait_for_job(struct pd_job *j) {
  if (j->doorbells && IOConnectCallScalarMethod(j->conn, PD_BRIDGE_SEL_WAIT_JOB, NULL, 0, NULL, NULL) == 0)
    return;

  j->doorbells = 0;
  usleep(1000);
}

static void ring_done(struct pd_job *j) {
  if (j->doorbells)
    IOConnectCallScalarMethod(j->conn, PD_BRIDGE_SEL_DONE, NULL, 0, NULL, NULL);
}

static void serve(struct pd_job *j) {
  struct PDBridgeHeader *h = j->h;

  for (;;) {
    int rc;

    if (h->state != PD_STATE_QUEUED) {
      wait_for_job(j);
      continue;
    }

    __sync_synchronize();
    logv("AIRJITD: job %u function %u grid %llux%llux%llu buffer %u+0x%llx", h->serial, h->functionID,
           (unsigned long long)h->grid[0], (unsigned long long)h->grid[1], (unsigned long long)h->grid[2],
           h->bufferID, (unsigned long long)h->bufferOffset);
    job_stage = ~0u;
    job_fault = 0;
    pd_note_clear();

    if (sigsetjmp(job_jmp, 1)) {
      pd_stage_disable(job_stage);
      logmsg("AIRJITD: job %u faulted (signal %d) in stage %u at %p (bridge %p-%p), stage disabled", h->serial,
             job_fault, job_stage, job_addr, (void *)j->base, (void *)(j->base + j->size));
      finish_job(h, PD_ERR_FAULT);
      ring_done(j);
      continue;
    }

    uint64_t t0 = pd_now_us();

    in_job = 1;
    rc = run_job(j);
    in_job = 0;
    logv("AIRJITD: job %u kind %u took %llu us", h->serial, h->kind, (unsigned long long)(pd_now_us() - t0));
    finish_job(h, rc);
    ring_done(j);
  }
}

static int map_bridge(struct pd_job *j) {
  io_service_t service;
  io_connect_t conn = 0;
  mach_vm_address_t address = 0;
  mach_vm_size_t size = PD_BRIDGE_BYTES;

  for (;;) {
    service = IOServiceGetMatchingService(0, IOServiceMatching("PDShaderBridge"));
    if (service && IOServiceOpen(service, mach_task_self_, 0, &conn) == 0)
      break;

    if (service)
      IOObjectRelease(service);

    sleep(1);
  }

  IOObjectRelease(service);

  if (IOConnectMapMemory64(conn, PD_BRIDGE_TYPE, mach_task_self_, &address, &size, 1) != 0)
    return -1;

  j->conn = conn;
  j->doorbells = 1;
  j->h = (struct PDBridgeHeader *)(uintptr_t)address;
  j->base = (uint8_t *)j->h;
  j->size = (uint32_t)size;
  logmsg("AIRJITD: ready, bridge 0x%llx size 0x%llx", (unsigned long long)address, (unsigned long long)size);
  return 0;
}

int main(int argc, char **argv) {
  struct pd_job j;

  pd_log_verbose = argc > 1 && strcmp(argv[1], "-v") == 0;

  // O_NONBLOCK: a blocking console write would park the daemon mid-job once getty
  // owns the console, and WindowServer with it
  open("/dev/console", O_WRONLY | O_NOCTTY | O_NONBLOCK);
  logmsg("AIRJITD: starting shader daemon, %s backend", backend->name);
  backend->init();
  install_fault_handlers();

  if (map_bridge(&j)) {
    logmsg("AIRJITD: bridge map failed");
    for (;;) {
      sleep(3600);
    }
  }

  j.h->state = PD_STATE_IDLE;
  j.h->status = PD_DAEMON_READY;
  __sync_synchronize();
  serve(&j);
  return 0;
}
