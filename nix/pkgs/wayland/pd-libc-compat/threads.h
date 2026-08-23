/* C11 <threads.h> over pthreads.
 *
 * Darwin's libc implements the threading model but never shipped the C11
 * spelling of it, so ports that use mtx_t/cnd_t/thrd_t do not compile. The
 * mapping is one-to-one apart from thrd_create, where C11's int-returning
 * start routine has to be trampolined through pthread's void *-returning one.
 *
 * Header-only: everything is static inline, so there is nothing to link.
 */
#ifndef PD_THREADS_H
#define PD_THREADS_H

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

enum {
    thrd_success  = 0,
    thrd_nomem    = 1,
    thrd_timedout = 2,
    thrd_busy     = 3,
    thrd_error    = 4,
};

enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2,
};

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t  cnd_t;
typedef pthread_key_t   tss_t;
typedef pthread_once_t  once_flag;

typedef int  (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

#define ONCE_FLAG_INIT   PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

/* --- mutexes --- */

static inline int
mtx_init(mtx_t *mtx, int type)
{
    pthread_mutexattr_t attr;
    int rc;

    if (pthread_mutexattr_init(&attr) != 0)
        return thrd_error;

    if (type & mtx_recursive)
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    rc = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);

    return rc == 0 ? thrd_success : thrd_error;
}

static inline int
mtx_lock(mtx_t *mtx)
{
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int
mtx_trylock(mtx_t *mtx)
{
    int rc = pthread_mutex_trylock(mtx);
    if (rc == 0)
        return thrd_success;
    return rc == EBUSY ? thrd_busy : thrd_error;
}

static inline int
mtx_unlock(mtx_t *mtx)
{
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline void
mtx_destroy(mtx_t *mtx)
{
    pthread_mutex_destroy(mtx);
}

/* --- condition variables --- */

static inline int
cnd_init(cnd_t *cond)
{
    return pthread_cond_init(cond, NULL) == 0 ? thrd_success : thrd_error;
}

static inline int
cnd_signal(cnd_t *cond)
{
    return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
}

static inline int
cnd_broadcast(cnd_t *cond)
{
    return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
}

static inline int
cnd_wait(cnd_t *cond, mtx_t *mtx)
{
    return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
}

static inline int
cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts)
{
    int rc = pthread_cond_timedwait(cond, mtx, ts);
    if (rc == 0)
        return thrd_success;
    return rc == ETIMEDOUT ? thrd_timedout : thrd_error;
}

static inline void
cnd_destroy(cnd_t *cond)
{
    pthread_cond_destroy(cond);
}

/* --- threads --- */

struct pd_thrd_trampoline_ctx {
    thrd_start_t func;
    void        *arg;
};

/* C11 start routines return int, pthread's return void *. Carry the result
 * through as an intptr_t so thrd_join can hand it back. */
static inline void *
pd_thrd_trampoline(void *raw)
{
    struct pd_thrd_trampoline_ctx ctx = *(struct pd_thrd_trampoline_ctx *)raw;
    free(raw);
    return (void *)(intptr_t)ctx.func(ctx.arg);
}

static inline int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    struct pd_thrd_trampoline_ctx *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
        return thrd_nomem;

    ctx->func = func;
    ctx->arg  = arg;

    if (pthread_create(thr, NULL, pd_thrd_trampoline, ctx) != 0) {
        free(ctx);
        return thrd_error;
    }
    return thrd_success;
}

static inline int
thrd_join(thrd_t thr, int *res)
{
    void *result;

    if (pthread_join(thr, &result) != 0)
        return thrd_error;
    if (res != NULL)
        *res = (int)(intptr_t)result;
    return thrd_success;
}

static inline int
thrd_detach(thrd_t thr)
{
    return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
}

static inline thrd_t
thrd_current(void)
{
    return pthread_self();
}

static inline int
thrd_equal(thrd_t a, thrd_t b)
{
    return pthread_equal(a, b);
}

static inline void
thrd_exit(int res)
{
    pthread_exit((void *)(intptr_t)res);
}

static inline void
thrd_yield(void)
{
    sched_yield();
}

static inline int
thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
    return nanosleep(duration, remaining) == 0 ? 0 : -1;
}

/* thread-specific storage and once */

static inline int
tss_create(tss_t *key, tss_dtor_t dtor)
{
    return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

static inline void *
tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int
tss_set(tss_t key, void *value)
{
    return pthread_setspecific(key, value) == 0 ? thrd_success : thrd_error;
}

static inline void
tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

static inline void
call_once(once_flag *flag, void (*func)(void))
{
    pthread_once(flag, func);
}

#endif /* PD_THREADS_H */
