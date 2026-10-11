// linux stand-ins for the darwin pieces airjitd's executor calls: a thread pool behind dispatch_apply_f,
// and the Mach-O section bounds llvm/backend.c names (no __init_offsets section here)
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "dispatch/dispatch.h"

// an empty section: both bounds at one address
const uint32_t pd_init_offsets[1] __asm("section$start$__TEXT$__init_offsets");
__asm(".globl \"section$end$__TEXT$__init_offsets\"\n"
      ".set \"section$end$__TEXT$__init_offsets\", \"section$start$__TEXT$__init_offsets\"");

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cv = PTHREAD_COND_INITIALIZER, done_cv = PTHREAD_COND_INITIALIZER;
static void *job_ctx;
static void (*job_fn)(void *, size_t);
static size_t job_n, job_next, job_done;
static unsigned job_gen;
static int nthreads;

// takes indices until the current job runs out, called with mu held
static void drain(void) {
  while (job_next < job_n) {
    size_t i = job_next++;

    pthread_mutex_unlock(&mu);
    job_fn(job_ctx, i);
    pthread_mutex_lock(&mu);
    if (++job_done == job_n)
      pthread_cond_broadcast(&done_cv);
  }
}

static void *worker(void *arg) {
  unsigned seen = 0;

  (void)arg;
  pthread_mutex_lock(&mu);
  for (;;) {
    while (job_gen == seen) {
      pthread_cond_wait(&work_cv, &mu);
    }
    seen = job_gen;
    drain();
  }
  return 0;
}

dispatch_queue_t dispatch_get_global_queue(long id, unsigned long flags) {
  (void)id, (void)flags;
  return 0;
}

void dispatch_apply_f(size_t n, dispatch_queue_t q, void *ctx, void (*fn)(void *, size_t)) {
  (void)q;
  pthread_mutex_lock(&mu);
  if (!nthreads) {
    nthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    for (int i = 1; i < nthreads; i++) {
      pthread_t t;
      pthread_create(&t, 0, worker, 0);
    }
  }
  job_ctx = ctx, job_fn = fn, job_n = n, job_next = 0, job_done = 0, job_gen++;
  pthread_cond_broadcast(&work_cv);
  drain();
  while (job_done < job_n) {
    pthread_cond_wait(&done_cv, &mu);
  }
  pthread_mutex_unlock(&mu);
}

dispatch_queue_t dispatch_queue_create(const char *label, void *attr) {
  (void)label, (void)attr;
  return (dispatch_queue_t)1;
}

void dispatch_async_f(dispatch_queue_t q, void *ctx, void (*fn)(void *)) {
  (void)q;
  fn(ctx);
}
