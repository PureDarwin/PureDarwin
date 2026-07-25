/*
 * $UNIX2003-suffixed ABI variants: the real SDK headers asm-rename many libc
 * calls to these mangled forms when __DARWIN_UNIX03 is set and
 * __DARWIN_ONLY_UNIX_CONFORMANCE ISN'T. libsystem_kernel_static now sets
 * __DARWIN_ONLY_UNIX_CONFORMANCE=1 (see libsystem_kernel/CMakeLists.txt),
 * matching libc_static and real Apple's own build (inherited there from an
 * internal BSD.xcconfig not present in this open-source drop) - so
 * wrappers/unix03/chmod.c's "chmod" function (and fchmod/getrlimit/
 * setrlimit/mmap/munmap/open/kill/fcntl/sigsuspend) compile as the PLAIN
 * symbol, matching what internal callers elsewhere in this tree
 * (mkpath_np.c, sysconf.c, raise.c, pause.c, ...) already expect. External
 * code still built without that define (most nix/pkgs/*.nix packages) gets
 * the $UNIX2003-mangled name from the SDK headers though, so this file
 * forwards that mangled name to the real plain function.
 *
 * mprotect is the mirror image: syscall.map aliases plain "_mprotect"
 * directly to the raw syscall (real, no wrapper.c needed), but nothing
 * provides a "$UNIX2003" sibling, so that one forwards the other way.
 *
 * Extern declarations here use a private local name (via __asm) rather than
 * the real SDK's own declaration, so this file doesn't need to include (and
 * fight the __DARWIN_ALIAS renaming of) <fcntl.h>/<sys/mman.h>/etc.
 */
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/types.h>

struct rlimit;
struct timespec;

extern int __pd_real_chmod(const char *path, mode_t mode) __asm("_chmod");
extern int __pd_real_fchmod(int fd, mode_t mode) __asm("_fchmod");
extern int __pd_real_fcntl(int fd, int cmd, long arg) __asm("_fcntl");
extern int __pd_real_getrlimit(int resource, struct rlimit *rlp) __asm("_getrlimit");
extern int __pd_real_kill(pid_t pid, int sig) __asm("_kill");
extern void *__pd_real_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) __asm("_mmap");
extern int __pd_real_munmap(void *addr, size_t len) __asm("_munmap");
extern int __pd_real_open(const char *path, int flags, mode_t mode) __asm("_open");
extern int __pd_real_setrlimit(int resource, const struct rlimit *rlp) __asm("_setrlimit");
extern int __pd_real_sigsuspend(const void *set) __asm("_sigsuspend");
extern int __pd_real_mprotect(void *addr, size_t len, int prot) __asm("_mprotect");
extern ssize_t __pd_real_write(int fd, const void *buf, size_t nbyte) __asm("_write");
extern unsigned int __pd_real_sleep(unsigned int seconds) __asm("_sleep");
extern int __pd_real_nanosleep(const struct timespec *rqtp, struct timespec *rmtp) __asm("_nanosleep");

int __pd_chmod_unix2003(const char *path, mode_t mode) __asm("_chmod$UNIX2003");
int
__pd_chmod_unix2003(const char *path, mode_t mode)
{
    return __pd_real_chmod(path, mode);
}

int __pd_fchmod_unix2003(int fd, mode_t mode) __asm("_fchmod$UNIX2003");
int
__pd_fchmod_unix2003(int fd, mode_t mode)
{
    return __pd_real_fchmod(fd, mode);
}

int __pd_fcntl_unix2003(int fd, int cmd, ...) __asm("_fcntl$UNIX2003");
int
__pd_fcntl_unix2003(int fd, int cmd, ...)
{
    va_list ap;
    long arg;

    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
    return __pd_real_fcntl(fd, cmd, arg);
}

int __pd_getrlimit_unix2003(int resource, struct rlimit *rlp) __asm("_getrlimit$UNIX2003");
int
__pd_getrlimit_unix2003(int resource, struct rlimit *rlp)
{
    return __pd_real_getrlimit(resource, rlp);
}

int __pd_kill_unix2003(pid_t pid, int sig) __asm("_kill$UNIX2003");
int
__pd_kill_unix2003(pid_t pid, int sig)
{
    return __pd_real_kill(pid, sig);
}

void *__pd_mmap_unix2003(void *addr, size_t len, int prot, int flags, int fd, off_t offset) __asm("_mmap$UNIX2003");
void *
__pd_mmap_unix2003(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    return __pd_real_mmap(addr, len, prot, flags, fd, offset);
}

int __pd_munmap_unix2003(void *addr, size_t len) __asm("_munmap$UNIX2003");
int
__pd_munmap_unix2003(void *addr, size_t len)
{
    return __pd_real_munmap(addr, len);
}

int __pd_open_unix2003(const char *path, int flags, ...) __asm("_open$UNIX2003");
int
__pd_open_unix2003(const char *path, int flags, ...)
{
    va_list ap;
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    return __pd_real_open(path, flags, mode);
}

int __pd_setrlimit_unix2003(int resource, const struct rlimit *rlp) __asm("_setrlimit$UNIX2003");
int
__pd_setrlimit_unix2003(int resource, const struct rlimit *rlp)
{
    return __pd_real_setrlimit(resource, rlp);
}

int __pd_sigsuspend_unix2003(const void *set) __asm("_sigsuspend$UNIX2003");
int
__pd_sigsuspend_unix2003(const void *set)
{
    return __pd_real_sigsuspend(set);
}

int __pd_mprotect_unix2003(void *addr, size_t len, int prot) __asm("_mprotect$UNIX2003");
int
__pd_mprotect_unix2003(void *addr, size_t len, int prot)
{
    return __pd_real_mprotect(addr, len, prot);
}

ssize_t __pd_write_unix2003(int fd, const void *buf, size_t nbyte) __asm("_write$UNIX2003");
ssize_t
__pd_write_unix2003(int fd, const void *buf, size_t nbyte)
{
    return __pd_real_write(fd, buf, nbyte);
}

unsigned int __pd_sleep_unix2003(unsigned int seconds) __asm("_sleep$UNIX2003");
unsigned int
__pd_sleep_unix2003(unsigned int seconds)
{
    return __pd_real_sleep(seconds);
}

int __pd_nanosleep_unix2003(const struct timespec *rqtp, struct timespec *rmtp) __asm("_nanosleep$UNIX2003");
int
__pd_nanosleep_unix2003(const struct timespec *rqtp, struct timespec *rmtp)
{
    return __pd_real_nanosleep(rqtp, rmtp);
}

extern int __pd_pthread_cancel_unix2003(pthread_t thread) __asm("_pthread_cancel$UNIX2003");
extern int __pd_pthread_cond_init_unix2003(pthread_cond_t *cond, const pthread_condattr_t *attr) __asm("_pthread_cond_init$UNIX2003");
extern int __pd_pthread_cond_timedwait_unix2003(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) __asm("_pthread_cond_timedwait$UNIX2003");
extern int __pd_pthread_cond_wait_unix2003(pthread_cond_t *cond, pthread_mutex_t *mutex) __asm("_pthread_cond_wait$UNIX2003");
extern int __pd_pthread_join_unix2003(pthread_t thread, void **value_ptr) __asm("_pthread_join$UNIX2003");
extern int __pd_pthread_mutexattr_destroy_unix2003(pthread_mutexattr_t *attr) __asm("_pthread_mutexattr_destroy$UNIX2003");
extern int __pd_pthread_rwlock_destroy_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_destroy$UNIX2003");
extern int __pd_pthread_rwlock_init_unix2003(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) __asm("_pthread_rwlock_init$UNIX2003");
extern int __pd_pthread_rwlock_rdlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_rdlock$UNIX2003");
extern int __pd_pthread_rwlock_tryrdlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_tryrdlock$UNIX2003");
extern int __pd_pthread_rwlock_trywrlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_trywrlock$UNIX2003");
extern int __pd_pthread_rwlock_unlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_unlock$UNIX2003");
extern int __pd_pthread_rwlock_wrlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_wrlock$UNIX2003");
extern int __pd_pthread_setcancelstate_unix2003(int state, int *oldstate) __asm("_pthread_setcancelstate$UNIX2003");
extern int __pd_pthread_setcanceltype_unix2003(int type, int *oldtype) __asm("_pthread_setcanceltype$UNIX2003");
extern int __pd_pthread_sigmask_unix2003(int how, const sigset_t *set, sigset_t *oset) __asm("_pthread_sigmask$UNIX2003");
extern void __pd_pthread_testcancel_unix2003(void) __asm("_pthread_testcancel$UNIX2003");

int __pd_pthread_cancel_default(pthread_t thread) __asm("_pthread_cancel");
int
__pd_pthread_cancel_default(pthread_t thread)
{
    return __pd_pthread_cancel_unix2003(thread);
}

int __pd_pthread_cond_init_default(pthread_cond_t *cond, const pthread_condattr_t *attr) __asm("_pthread_cond_init");
int
__pd_pthread_cond_init_default(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    return __pd_pthread_cond_init_unix2003(cond, attr);
}

int __pd_pthread_cond_timedwait_default(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) __asm("_pthread_cond_timedwait");
int
__pd_pthread_cond_timedwait_default(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
    return __pd_pthread_cond_timedwait_unix2003(cond, mutex, abstime);
}

int __pd_pthread_cond_wait_default(pthread_cond_t *cond, pthread_mutex_t *mutex) __asm("_pthread_cond_wait");
int
__pd_pthread_cond_wait_default(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    return __pd_pthread_cond_wait_unix2003(cond, mutex);
}

int __pd_pthread_join_default(pthread_t thread, void **value_ptr) __asm("_pthread_join");
int
__pd_pthread_join_default(pthread_t thread, void **value_ptr)
{
    return __pd_pthread_join_unix2003(thread, value_ptr);
}

int __pd_pthread_mutexattr_destroy_default(pthread_mutexattr_t *attr) __asm("_pthread_mutexattr_destroy");
int
__pd_pthread_mutexattr_destroy_default(pthread_mutexattr_t *attr)
{
    return __pd_pthread_mutexattr_destroy_unix2003(attr);
}

int __pd_pthread_rwlock_destroy_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_destroy");
int
__pd_pthread_rwlock_destroy_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_destroy_unix2003(rwlock);
}

int __pd_pthread_rwlock_init_default(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) __asm("_pthread_rwlock_init");
int
__pd_pthread_rwlock_init_default(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
    return __pd_pthread_rwlock_init_unix2003(rwlock, attr);
}

int __pd_pthread_rwlock_rdlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_rdlock");
int
__pd_pthread_rwlock_rdlock_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_rdlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_tryrdlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_tryrdlock");
int
__pd_pthread_rwlock_tryrdlock_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_tryrdlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_trywrlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_trywrlock");
int
__pd_pthread_rwlock_trywrlock_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_trywrlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_unlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_unlock");
int
__pd_pthread_rwlock_unlock_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_unlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_wrlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_wrlock");
int
__pd_pthread_rwlock_wrlock_default(pthread_rwlock_t *rwlock)
{
    return __pd_pthread_rwlock_wrlock_unix2003(rwlock);
}

int __pd_pthread_setcancelstate_default(int state, int *oldstate) __asm("_pthread_setcancelstate");
int
__pd_pthread_setcancelstate_default(int state, int *oldstate)
{
    return __pd_pthread_setcancelstate_unix2003(state, oldstate);
}

int __pd_pthread_setcanceltype_default(int type, int *oldtype) __asm("_pthread_setcanceltype");
int
__pd_pthread_setcanceltype_default(int type, int *oldtype)
{
    return __pd_pthread_setcanceltype_unix2003(type, oldtype);
}

int __pd_pthread_sigmask_default(int how, const sigset_t *set, sigset_t *oset) __asm("_pthread_sigmask");
int
__pd_pthread_sigmask_default(int how, const sigset_t *set, sigset_t *oset)
{
    return __pd_pthread_sigmask_unix2003(how, set, oset);
}

void __pd_pthread_testcancel_default(void) __asm("_pthread_testcancel");
void
__pd_pthread_testcancel_default(void)
{
    __pd_pthread_testcancel_unix2003();
}
