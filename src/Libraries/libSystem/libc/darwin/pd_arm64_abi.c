#include <pthread.h>
#include <stdio.h>
#include <sys/resource.h>

/* dyld is built against the historical SDK spellings even on arm64. */
int
pd_getrlimit_unix2003(int resource, struct rlimit *rlp) __asm("_getrlimit$UNIX2003");
int
pd_getrlimit_unix2003(int resource, struct rlimit *rlp)
{
    return getrlimit(resource, rlp);
}

size_t
pd_fwrite_unix2003(const void *ptr, size_t size, size_t nitems, FILE *stream)
    __asm("_fwrite$UNIX2003");
size_t
pd_fwrite_unix2003(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
    return fwrite(ptr, size, nitems, stream);
}

#define PD_PTHREAD_ALIAS(ret, name, args, callargs) \
    ret pd_##name##_unix2003 args __asm("_" #name "$UNIX2003"); \
    ret pd_##name##_unix2003 args { return name callargs; }

PD_PTHREAD_ALIAS(int, pthread_cancel, (pthread_t t), (t))
PD_PTHREAD_ALIAS(int, pthread_cond_init, (pthread_cond_t *c, const pthread_condattr_t *a), (c, a))
PD_PTHREAD_ALIAS(int, pthread_cond_timedwait, (pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t), (c, m, t))
PD_PTHREAD_ALIAS(int, pthread_cond_wait, (pthread_cond_t *c, pthread_mutex_t *m), (c, m))
PD_PTHREAD_ALIAS(int, pthread_join, (pthread_t t, void **v), (t, v))
PD_PTHREAD_ALIAS(int, pthread_mutexattr_destroy, (pthread_mutexattr_t *a), (a))
PD_PTHREAD_ALIAS(int, pthread_rwlock_destroy, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_rwlock_init, (pthread_rwlock_t *l, const pthread_rwlockattr_t *a), (l, a))
PD_PTHREAD_ALIAS(int, pthread_rwlock_rdlock, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_rwlock_tryrdlock, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_rwlock_trywrlock, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_rwlock_unlock, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_rwlock_wrlock, (pthread_rwlock_t *l), (l))
PD_PTHREAD_ALIAS(int, pthread_setcancelstate, (int s, int *o), (s, o))
PD_PTHREAD_ALIAS(int, pthread_setcanceltype, (int t, int *o), (t, o))
PD_PTHREAD_ALIAS(int, pthread_sigmask, (int h, const sigset_t *s, sigset_t *o), (h, s, o))
PD_PTHREAD_ALIAS(void, pthread_testcancel, (void), ())

#undef PD_PTHREAD_ALIAS
