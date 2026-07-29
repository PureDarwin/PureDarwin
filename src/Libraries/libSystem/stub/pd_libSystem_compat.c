#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <grp.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <xlocale.h>
#include <mach/vm_types.h>
#include <mach-o/loader.h>
#include <sys/qos.h>
#include <limits.h>
#include <sysdir.h>

extern int __pd_sys_pause(void) __asm("___pause");
extern pid_t __pd_sys_waitpid(pid_t pid, int *status, int options) __asm("___waitpid");
extern int _dyld_func_lookup(const char *name, void **address);
extern FILE *__pd_fdopen_extsn(int fd, const char *mode) __asm("_fdopen$DARWIN_EXTSN");
extern FILE *__pd_fopen_extsn(const char *path, const char *mode) __asm("_fopen$DARWIN_EXTSN");

int
getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    uid_t real = getuid();
    uid_t effective = geteuid();

    if (ruid == NULL || euid == NULL || suid == NULL) {
        errno = EINVAL;
        return -1;
    }

    *ruid = real;
    *euid = effective;
    *suid = effective;
    return 0;
}

int
getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    gid_t real = getgid();
    gid_t effective = getegid();

    if (rgid == NULL || egid == NULL || sgid == NULL) {
        errno = EINVAL;
        return -1;
    }

    *rgid = real;
    *egid = effective;
    *sgid = effective;
    return 0;
}

/* setresuid/setresgid aren't real Darwin API (no such syscall exists), but
 * setreuid(2)/setregid(2) are real kernel traps - marked NO_SYSCALL_STUB in
 * syscalls.master (real Apple hides them from the public headers) but still
 * backed by the real kauth_cred_setresuid()/kauth_cred_setresgid() kernel
 * logic. custom/__setreuid.s and __setregid.s add the raw trampolines */
extern int setreuid(uid_t ruid, uid_t euid);
extern int setregid(gid_t rgid, gid_t egid);

int
setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t current_real = getuid();
    uid_t current_effective = geteuid();

    if (suid != (uid_t)-1 && suid != current_effective
        && suid != (euid != (uid_t)-1 ? euid : current_effective)) {
        errno = ENOSYS;
        return -1;
    }
    if (ruid == current_real) {
        ruid = (uid_t)-1;
    }
    if (euid == current_effective) {
        euid = (uid_t)-1;
    }
    return setreuid(ruid, euid);
}

int
setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
    gid_t current_real = getgid();
    gid_t current_effective = getegid();

    if (sgid != (gid_t)-1 && sgid != current_effective
        && sgid != (egid != (gid_t)-1 ? egid : current_effective)) {
        errno = ENOSYS;
        return -1;
    }
    if (rgid == current_real) {
        rgid = (gid_t)-1;
    }
    if (egid == current_effective) {
        egid = (gid_t)-1;
    }
    return setregid(rgid, egid);
}

int
pause(void)
{
    return __pd_sys_pause();
}

pid_t
waitpid(pid_t pid, int *status, int options)
{
    return __pd_sys_waitpid(pid, status, options);
}

FILE *
__pd_fdopen_extsn(int fd, const char *mode)
{
    return fdopen(fd, mode);
}

FILE *
__pd_fopen_extsn(const char *path, const char *mode)
{
    return fopen(path, mode);
}

/* This fork's sys/cdefs.h stubs __weak_reference to a no-op (needed for
 * static archive linking), so s_scalbn.c's `__strong_reference(scalbn,
 * ldexp)` alias never gets pulled in unless something references it by name
 * first. Forward explicitly to the real scalbn instead of aliasing. */
double
ldexp(double x, int n)
{
    return scalbn(x, n);
}

int
dlclose(void *handle)
{
    typedef int (*dlclose_fn)(void *);
    static dlclose_fn fn;

    if (fn == NULL && !_dyld_func_lookup("__dyld_dlclose", (void **)&fn)) {
        return -1;
    }

    return fn(handle);
}

void *
dlopen(const char *path, int mode)
{
    typedef void *(*dlopen_fn)(const char *, int, void *);
    static dlopen_fn fn;

    if (fn == NULL && !_dyld_func_lookup("__dyld_dlopen_internal", (void **)&fn)) {
        return NULL;
    }

    return fn(path, mode, __builtin_return_address(0));
}

void *
dlsym(void *handle, const char *symbol)
{
    typedef void *(*dlsym_fn)(void *, const char *, void *);
    static dlsym_fn fn;

    if (fn == NULL && !_dyld_func_lookup("__dyld_dlsym_internal", (void **)&fn)) {
        return NULL;
    }

    return fn(handle, symbol, __builtin_return_address(0));
}

bool
_dyld_is_memory_immutable(const void *addr, size_t length)
{
    typedef bool (*dyld_is_memory_immutable_fn)(const void *, size_t);
    static dyld_is_memory_immutable_fn fn;

    if (fn == NULL && !_dyld_func_lookup("__dyld_is_memory_immutable", (void **)&fn)) {
        return false;
    }

    return fn(addr, length);
}

char *
dlerror(void)
{
    typedef char *(*dlerror_fn)(void);
    static dlerror_fn fn;

    if (fn == NULL && !_dyld_func_lookup("__dyld_dlerror", (void **)&fn)) {
        return NULL;
    }

    return fn();
}

char *index(const char *s, int c)
{
    return __builtin_strchr(s, c);
}


/*
 * The rest of this file back-fills libc/libutil/libinfo functions that xterm
 * references but PureDarwin's static libc archives don't yet provide, so the
 * flat-namespace symbols it defers at link time actually resolve at load
 * instead of aborting dyld. Where a real implementation is cheap and correct
 * (alarm, the *_r passwd wrappers, openpty) we provide it; where the feature is
 * non-essential to xterm coming up (usershell iteration, reverse DNS) we
 * provide a minimal well-behaved stub.
 */

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <netdb.h>
#include <pwd.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>





static void
pd_pwcopy(struct passwd *dst, const struct passwd *src,
          char *buf, size_t buflen, struct passwd **result)
{
    size_t need_name = src->pw_name ? strlen(src->pw_name) + 1 : 0;
    size_t need_pass = src->pw_passwd ? strlen(src->pw_passwd) + 1 : 0;
    size_t need_gecos = src->pw_gecos ? strlen(src->pw_gecos) + 1 : 0;
    size_t need_dir = src->pw_dir ? strlen(src->pw_dir) + 1 : 0;
    size_t need_shell = src->pw_shell ? strlen(src->pw_shell) + 1 : 0;
    size_t off = 0;

    if (need_name + need_pass + need_gecos + need_dir + need_shell > buflen) {
        *result = NULL;         /* caller sees ERANGE via return value */
        return;
    }

    *dst = *src;

#define PD_PW_DUP(field, len)                                   \
    do {                                                        \
        if ((len) != 0) {                                       \
            memcpy(buf + off, src->field, (len));               \
            dst->field = buf + off;                             \
            off += (len);                                       \
        } else {                                                \
            dst->field = NULL;                                  \
        }                                                       \
    } while (0)

    PD_PW_DUP(pw_name, need_name);
    PD_PW_DUP(pw_passwd, need_pass);
    PD_PW_DUP(pw_gecos, need_gecos);
    PD_PW_DUP(pw_dir, need_dir);
    PD_PW_DUP(pw_shell, need_shell);
#undef PD_PW_DUP

    *result = dst;
}



/*
 * PureDarwin's libc archive ships none of the Unix98 pty helpers, so provide
 * them here directly over /dev/ptmx using Darwin's pty ioctls (sys/ttycom.h).
 * These are the same operations Apple's libc performs; whether they succeed at
 * runtime depends on the kernel's ptmx/ptsd driver being present.
 */
#ifndef TIOCPTYGRANT
#define TIOCPTYGRANT _IO('t', 84)
#endif
#ifndef TIOCPTYGNAME
#define TIOCPTYGNAME _IOC(IOC_OUT, 't', 83, 128)
#endif
#ifndef TIOCPTYUNLK
#define TIOCPTYUNLK _IO('t', 82)
#endif






/*
 * popen$DARWIN_EXTSN: xterm references the versioned symbol (the SDK's
 * <stdio.h> asm-renames popen). PureDarwin's libc archive exports neither
 * variant, so implement it here via fork/exec/pipe. Book-kept fds so the
 * companion pclose can reap; if pclose isn't linked the child is simply
 * reaped by the kernel at exit.
 */
extern FILE *__pd_popen_extsn(const char *command, const char *type)
    __asm("_popen$DARWIN_EXTSN");

FILE *
__pd_popen_extsn(const char *command, const char *type)
{
    int fds[2];
    int read_side;
    int want_read;
    pid_t pid;

    if (command == NULL || type == NULL
        || (type[0] != 'r' && type[0] != 'w')) {
        errno = EINVAL;
        return NULL;
    }
    want_read = (type[0] == 'r');

    if (pipe(fds) < 0) {
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }

    if (pid == 0) {
        if (want_read) {
            dup2(fds[1], STDOUT_FILENO);
        } else {
            dup2(fds[0], STDIN_FILENO);
        }
        close(fds[0]);
        close(fds[1]);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    if (want_read) {
        close(fds[1]);
        read_side = fds[0];
    } else {
        close(fds[0]);
        read_side = fds[1];
    }
    return fdopen(read_side, want_read ? "r" : "w");
}

/*
 * select$1050: xterm's SDK <sys/select.h> asm-renames select to the 10.5
 * ("UNIX2003") variant. libSystem already provides select$DARWIN_EXTSN from
 * the libc archive, so forward the 1050 alias to it.
 */
extern int __pd_select_extsn(int nfds, fd_set *readfds, fd_set *writefds,
                             fd_set *errorfds, struct timeval *timeout)
    __asm("_select$DARWIN_EXTSN");
extern int __pd_select_1050(int nfds, fd_set *readfds, fd_set *writefds,
                            fd_set *errorfds, struct timeval *timeout)
    __asm("_select$1050");

int
__pd_select_1050(int nfds, fd_set *readfds, fd_set *writefds,
                 fd_set *errorfds, struct timeval *timeout)
{
    return __pd_select_extsn(nfds, readfds, writefds, errorfds, timeout);
}

/*
 * nano (via gnulib) needs these; none of them are in libc_static/
 * libsystem_kernel, so implement them directly.
 */

#include <dirent.h>
#include <search.h>
#include <wchar.h>

/* The SDK's <signal.h> defines these as inlining macros; undef them so we
 * can provide real, linkable function symbols instead. */
#undef sigemptyset
#undef sigfillset
#undef sigaddset

int
sigemptyset(sigset_t *set)
{
    *set = 0;
    return 0;
}

int
sigfillset(sigset_t *set)
{
    *set = ~(sigset_t)0;
    return 0;
}

int
sigaddset(sigset_t *set, int signo)
{
    if (signo <= 0 || signo >= (int)(sizeof(sigset_t) * 8)) {
        errno = EINVAL;
        return -1;
    }
    *set |= ((sigset_t)1 << (signo - 1));
    return 0;
}

/*
 * rewinddir$INODE64: opendir$INODE64/readdir$INODE64 (from libc_static) read
 * directory entries directly off the fd with no additional userspace
 * buffering layer here, so resetting the fd's file offset is equivalent to
 * reopening the stream at the start.
 */
void
__pd_rewinddir_inode64(DIR *dirp) __asm("_rewinddir$INODE64");

void
__pd_rewinddir_inode64(DIR *dirp)
{
    lseek(dirfd(dirp), 0, SEEK_SET);
}


static void
pd_grcopy(struct group *dst, const struct group *src,
          char *buf, size_t buflen, struct group **result)
{
    size_t need_name = src->gr_name ? strlen(src->gr_name) + 1 : 0;
    size_t need_passwd = src->gr_passwd ? strlen(src->gr_passwd) + 1 : 0;
    size_t nmembers = 0, need_members, need_member_strings = 0, off = 0;
    size_t i;

    if (src->gr_mem != NULL) {
        while (src->gr_mem[nmembers] != NULL) {
            need_member_strings += strlen(src->gr_mem[nmembers]) + 1;
            nmembers++;
        }
    }
    need_members = (nmembers + 1) * sizeof(char *);

    if (need_name + need_passwd + need_members + need_member_strings > buflen) {
        *result = NULL;
        return;
    }

    *dst = *src;

    if (need_name != 0) {
        memcpy(buf + off, src->gr_name, need_name);
        dst->gr_name = buf + off;
        off += need_name;
    } else {
        dst->gr_name = NULL;
    }
    if (need_passwd != 0) {
        memcpy(buf + off, src->gr_passwd, need_passwd);
        dst->gr_passwd = buf + off;
        off += need_passwd;
    } else {
        dst->gr_passwd = NULL;
    }
    dst->gr_mem = (char **)(buf + off);
    off += need_members;
    for (i = 0; i < nmembers; i++) {
        size_t len = strlen(src->gr_mem[i]) + 1;
        memcpy(buf + off, src->gr_mem[i], len);
        dst->gr_mem[i] = buf + off;
        off += len;
    }
    dst->gr_mem[nmembers] = NULL;

    *result = dst;
}



extern int __pd_getgroups(int gidsetsize, gid_t grouplist[]) __asm("_getgroups");
int __pd_getgroups_extsn(int gidsetsize, gid_t grouplist[]) __asm("_getgroups$DARWIN_EXTSN");

int
__pd_getgroups_extsn(int gidsetsize, gid_t grouplist[])
{
    return __pd_getgroups(gidsetsize, grouplist);
}

int
statvfs(const char *path, struct statvfs *buf)
{
    struct statfs sf;

    if (statfs(path, &sf) != 0)
        return -1;
    memset(buf, 0, sizeof(*buf));
    buf->f_bsize   = sf.f_bsize;
    buf->f_frsize  = sf.f_bsize;
    buf->f_blocks  = sf.f_blocks;
    buf->f_bfree   = sf.f_bfree;
    buf->f_bavail  = sf.f_bavail;
    buf->f_files   = sf.f_files;
    buf->f_ffree   = sf.f_ffree;
    buf->f_favail  = sf.f_ffree;
    buf->f_fsid    = (unsigned long)sf.f_fsid.val[0];
    buf->f_flag    = 0;
    buf->f_namemax = 255;
    return 0;
}



int
pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    (void)prepare; (void)parent; (void)child;
    return 0;
}

/*
 * issetugid(): real Apple semantics report whether the process is running
 * with elevated privilege from a setuid/setgid exec (libc/CF/etc use it to
 * decide whether to trust environment variables). PureDarwin doesn't run
 * setuid binaries, so this is never true here - always report "no".
 */
int
issetugid(void)
{
    return 0;
}

/*
 * sysdir(3)
 *
 * Enumerates a standard directory across the domains it exists in, cheapest
 * domain first. The state is opaque to callers, so it just packs the directory
 * selector with the domains left to visit; 0 ends the enumeration.
 *
 * Only the directories PureDarwin actually has are mapped. An unmapped
 * directory yields no paths at all rather than a plausible-looking one, so a
 * caller iterating it finds nothing instead of a path that never exists.
 */

#define PD_SYSDIR_DIR_SHIFT     16
#define PD_SYSDIR_DOMAIN_MASK   0xffffu

static const char *
pd_sysdir_suffix(unsigned int dir)
{
    switch (dir) {
    case 5:   return "Library";                     /* SYSDIR_DIRECTORY_LIBRARY */
    case 7:   return "Users";                       /* SYSDIR_DIRECTORY_USER */
    case 8:   return "Library/Documentation";
    case 10:  return "Library/CoreServices";
    case 13:  return "Library/Caches";
    case 14:  return "Library/Application Support";
    case 1:   return "Applications";
    case 4:   return "Applications/Utilities";
    default:  return NULL;
    }
}

static const char *
pd_sysdir_domain_root(unsigned int domain_bit)
{
    switch (domain_bit) {
    case 1u << 1: return "";            /* LOCAL:   /Library */
    case 1u << 2: return "/Network";    /* NETWORK: /Network/Library */
    case 1u << 3: return "/System";     /* SYSTEM:  /System/Library */
    default:      return NULL;          /* USER needs $HOME, handled by caller */
    }
}

sysdir_search_path_enumeration_state
sysdir_start_search_path_enumeration(sysdir_search_path_directory_t dir,
                                     sysdir_search_path_domain_mask_t domainMask)
{
    if (pd_sysdir_suffix((unsigned int)dir) == NULL) {
        return 0;
    }
    return ((unsigned int)dir << PD_SYSDIR_DIR_SHIFT) |
           ((unsigned int)domainMask & PD_SYSDIR_DOMAIN_MASK);
}

sysdir_search_path_enumeration_state
sysdir_get_next_search_path_enumeration(sysdir_search_path_enumeration_state state,
                                        char *path)
{
    unsigned int dir      = state >> PD_SYSDIR_DIR_SHIFT;
    unsigned int domains  = state & PD_SYSDIR_DOMAIN_MASK;
    const char  *suffix   = pd_sysdir_suffix(dir);

    if ((state == 0) || (suffix == NULL) || (domains == 0) || (path == NULL)) {
        return 0;
    }

    /* Visit the lowest set domain bit, then clear it for the next call. */
    while (domains != 0) {
        unsigned int  bit  = domains & (~domains + 1u);
        const char   *root = pd_sysdir_domain_root(bit);

        domains &= ~bit;

        if (bit == (1u << 0)) {
            const char *home = getenv("HOME");

            if (home == NULL) {
                continue;
            }
            snprintf(path, PATH_MAX, "%s/%s", home, suffix);
        } else if (root != NULL) {
            snprintf(path, PATH_MAX, "%s/%s", root, suffix);
        } else {
            continue;
        }

        return (dir << PD_SYSDIR_DIR_SHIFT) | domains;
    }

    return 0;
}

vm_size_t vm_page_size = 4096;

int
OSAtomicCompareAndSwapPtrBarrier(void *oldValue, void *newValue, void * volatile *theValue)
{
    return __sync_bool_compare_and_swap(theValue, oldValue, newValue);
}

/*
 * _os_log_create: the symbol <os/log.h>'s os_log_create(subsystem, category)
 * macro actually emits, with the caller's &__dso_handle prepended. The dso is
 * not used; forward to the real implementation in libsystem_trace so callers
 * get a genuine os_log_t - __os_log_impl() dereferences it.
 */
extern void *os_log_create(const char *subsystem, const char *category);

void *
_os_log_create(void *dso, const char *subsystem, const char *category)
{
    (void)dso;
    return os_log_create(subsystem, category);
}

typedef unsigned int __pd_mvr_kern_return_t;
__pd_mvr_kern_return_t
mach_vm_region(unsigned int target_task, unsigned long long *address,
    unsigned long long *size, unsigned int flavor,
    int *info, unsigned int *infoCnt, unsigned int *object_name)
{
    (void)target_task; (void)address; (void)size; (void)flavor;
    (void)info; (void)infoCnt; (void)object_name;
    return 5; /* KERN_FAILURE */
}

int
pthread_getugid_np(uid_t *uid, gid_t *gid)
{
    if (uid) *uid = getuid();
    if (gid) *gid = getgid();
    return 0;
}

int
fprintf_l(FILE *stream, locale_t loc, const char *format, ...)
{
    (void)loc;
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

extern int __pd_readdir_r_inode64(DIR *dirp, struct dirent *entry, struct dirent **result) __asm("_readdir_r$INODE64");
int __pd_readdir_r_plain(DIR *dirp, struct dirent *entry, struct dirent **result) __asm("_readdir_r");
int
__pd_readdir_r_plain(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    return __pd_readdir_r_inode64(dirp, entry, result);
}

#if !defined(__arm64__) && !defined(__aarch64__)
extern DIR *__pd_opendir_inode64(const char *name) __asm("_opendir$INODE64");
DIR *__pd_opendir_plain(const char *name) __asm("_opendir");
DIR *
__pd_opendir_plain(const char *name)
{
    return __pd_opendir_inode64(name);
}

extern void __pd_rewinddir_inode64(DIR *dirp) __asm("_rewinddir$INODE64");
void __pd_rewinddir_plain(DIR *dirp) __asm("_rewinddir");
void
__pd_rewinddir_plain(DIR *dirp)
{
    __pd_rewinddir_inode64(dirp);
}

extern DIR *__pd_fdopendir_inode64(int fd) __asm("_fdopendir$INODE64");
DIR *__pd_fdopendir_plain(int fd) __asm("_fdopendir");
DIR *
__pd_fdopendir_plain(int fd)
{
    return __pd_fdopendir_inode64(fd);
}

extern int __pd_alphasort_inode64(const struct dirent **a, const struct dirent **b)
    __asm("_alphasort$INODE64");
int __pd_alphasort_plain(const struct dirent **a, const struct dirent **b)
    __asm("_alphasort");
int
__pd_alphasort_plain(const struct dirent **a, const struct dirent **b)
{
    return __pd_alphasort_inode64(a, b);
}

/*
 * daemon(3) is __DARWIN_ALIAS'd to _daemon$1050 on x86; arm64 builds libc with
 * __DARWIN_ONLY_VERS_1050 and emits the plain name, so only x86 needs the
 * forwarder for callers that reference the unsuffixed symbol.
 */
extern int __pd_daemon_1050(int nochdir, int noclose) __asm("_daemon$1050");
int __pd_daemon_plain(int nochdir, int noclose) __asm("_daemon");
int
__pd_daemon_plain(int nochdir, int noclose)
{
    return __pd_daemon_1050(nochdir, noclose);
}

extern int __pd_scandir_inode64(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **))
    __asm("_scandir$INODE64");
int __pd_scandir_plain(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **))
    __asm("_scandir");
int
__pd_scandir_plain(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **))
{
    return __pd_scandir_inode64(dirname, namelist, select, compar);
}
#endif

/*
 * Legacy OSAtomic ops (libkern/OSAtomic.h, deprecated since 10.12 in favor
 * of <stdatomic.h> but still linked by CF and others). Real implementations
 * via the same compiler intrinsics <stdatomic.h> itself would use.
 */
int32_t
OSAtomicIncrement32(volatile int32_t *theValue)
{
    return __sync_add_and_fetch(theValue, 1);
}

int32_t
OSAtomicDecrement32(volatile int32_t *theValue)
{
    return __sync_sub_and_fetch(theValue, 1);
}

/* The Barrier variants differ only in ordering guarantees; __sync_* builtins
 * are already full barriers, so the plain and Barrier forms are the same here. */
int32_t
OSAtomicIncrement32Barrier(volatile int32_t *theValue)
{
    return __sync_add_and_fetch(theValue, 1);
}

int32_t
OSAtomicDecrement32Barrier(volatile int32_t *theValue)
{
    return __sync_sub_and_fetch(theValue, 1);
}

bool
OSAtomicCompareAndSwapLongBarrier(long oldValue, long newValue, volatile long *theValue)
{
    return __sync_bool_compare_and_swap(theValue, oldValue, newValue);
}

int
OSAtomicCompareAndSwap32Barrier(int32_t oldValue, int32_t newValue, volatile int32_t *theValue)
{
    return __sync_bool_compare_and_swap(theValue, oldValue, newValue);
}

void
OSMemoryBarrier(void)
{
    __sync_synchronize();
}

/*
 * OSSpinLock: fully removed from the real SDK headers by 10.12 (superseded
 * by os_unfair_lock), but still linked here - implement both it and
 * os_unfair_lock with the same simple CAS spin loop. Not "unfair" in the
 * scheduler-hint sense real os_unfair_lock is, but correct as a mutual
 * exclusion primitive, which is all callers actually depend on.
 */
typedef volatile int32_t OSSpinLock;

void
OSSpinLockLock(OSSpinLock *lock)
{
    while (!__sync_bool_compare_and_swap(lock, 0, 1)) {
        /* spin */
    }
}

void
OSSpinLockUnlock(OSSpinLock *lock)
{
    __sync_lock_release(lock);
}

typedef struct { volatile int32_t locked; } pd_os_unfair_lock_s;

void
os_unfair_lock_lock(pd_os_unfair_lock_s *lock)
{
    while (!__sync_bool_compare_and_swap(&lock->locked, 0, 1)) {
        /* spin */
    }
}

void
os_unfair_lock_unlock(pd_os_unfair_lock_s *lock)
{
    __sync_lock_release(&lock->locked);
}

/*
 * *_l (locale-variant) wrappers: no real per-thread locale support here
 * (every locale_t is effectively "C"/"POSIX"), so these just ignore the
 * locale_t argument and call the ordinary, already-real implementation.
 */
unsigned long
strtoul_l(const char *nptr, char **endptr, int base, locale_t loc)
{
    (void)loc;
    return strtoul(nptr, endptr, base);
}

long
strtol_l(const char *nptr, char **endptr, int base, locale_t loc)
{
    (void)loc;
    return strtol(nptr, endptr, base);
}

long long
strtoll_l(const char *nptr, char **endptr, int base, locale_t loc)
{
    (void)loc;
    return strtoll(nptr, endptr, base);
}

unsigned long long
strtoull_l(const char *nptr, char **endptr, int base, locale_t loc)
{
    (void)loc;
    return strtoull(nptr, endptr, base);
}

double
strtod_l(const char *nptr, char **endptr, locale_t loc)
{
    (void)loc;
    return strtod(nptr, endptr);
}

int
strncasecmp_l(const char *s1, const char *s2, size_t n, locale_t loc)
{
    (void)loc;
    return strncasecmp(s1, s2, n);
}

int
snprintf_l(char *str, size_t size, locale_t loc, const char *format, ...)
{
    (void)loc;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/*
 * PureDarwin: real uuid_generate/uuid_generate_random/uuid_generate_time
 * (plus uuid_clear/compare/copy/is_null/unparse/pack/unpack) now come from
 * the vendored libc/uuid/uuidsrc sources (see libc/CMakeLists.txt) instead
 * of this hand-rolled arc4random-only stub.
 */

unsigned int
mk_timer_create(void)
{
    return 1; /* dummy, non-MACH_PORT_NULL port name */
}

int
mk_timer_destroy(unsigned int name)
{
    (void)name;
    return 0;
}

int
mk_timer_arm(unsigned int name, uint64_t expire_time)
{
    (void)name; (void)expire_time;
    return 0;
}

int
mk_timer_cancel(unsigned int name, uint64_t *result_time)
{
    (void)name;
    if (result_time) *result_time = 0;
    return 0;
}

int
task_threads(unsigned int task, unsigned int **thread_list, unsigned int *thread_count)
{
    (void)task;
    if (thread_list) *thread_list = NULL;
    if (thread_count) *thread_count = 0;
    return 5; /* KERN_FAILURE */
}

int
thread_resume(unsigned int thread)
{
    (void)thread;
    return 5; /* KERN_FAILURE */
}

int
thread_suspend(unsigned int thread)
{
    (void)thread;
    return 5; /* KERN_FAILURE */
}

qos_class_t
qos_class_self(void)
{
    return QOS_CLASS_UNSPECIFIED;
}

/*
 * __udivti3: compiler-rt's 128-bit unsigned division, needed whenever code
 * divides an unsigned __int128 (CFBigNumber's 128-bit arithmetic here) on a
 * target without native 128-bit division. No prebuilt libclang_rt.builtins
 * for this cross target, so implement the well-known compiler-rt algorithm
 * directly: binary long division, one bit at a time. Not fast, but CF only
 * needs this for occasional big-number formatting, not a hot path.
 */
unsigned __int128
__udivti3(unsigned __int128 a, unsigned __int128 b)
{
    if (b == 0) return 0; /* real compiler-rt traps; we don't have one to trap into */
    unsigned __int128 quotient = 0;
    unsigned __int128 remainder = 0;
    for (int i = 127; i >= 0; i--) {
        remainder = (remainder << 1) | ((a >> i) & 1);
        if (remainder >= b) {
            remainder -= b;
            quotient |= ((unsigned __int128)1 << i);
        }
    }
    return quotient;
}


/*
 * slot_name (mach/mach_init.h): hostinfo(1)'s only non-MIG dependency.
 * Real Darwin has a large historical table covering every CPU type/subtype
 * combination it ever ran on (68k, PowerPC, ARM, ...); PureDarwin only ever
 * targets CPU_TYPE_X86_64, so give real names for that family and a generic
 * fallback for anything else rather than porting the whole table.
 */
#include <mach/machine.h>

void
slot_name(cpu_type_t cpu_type, cpu_subtype_t cpu_subtype, char **cpu_name, char **cpu_subname)
{
    static char subname_buf[32];

    if (cpu_type == CPU_TYPE_X86_64) {
        *cpu_name = "x86_64";
        switch (cpu_subtype & ~CPU_SUBTYPE_MASK) {
        case CPU_SUBTYPE_X86_64_ALL:
            *cpu_subname = "all";
            return;
        case CPU_SUBTYPE_X86_64_H:
            *cpu_subname = "Haswell";
            return;
        default:
            break;
        }
    } else if (cpu_type == CPU_TYPE_I386) {
        *cpu_name = "i386";
        *cpu_subname = "all";
        return;
    } else {
        *cpu_name = "unknown";
    }

    snprintf(subname_buf, sizeof(subname_buf), "subtype %d", cpu_subtype);
    *cpu_subname = subname_buf;
}

/*
 * Batch from the full-image import sweep (2026-07-18): every symbol below is
 * a lazy bind somewhere in the shipped image (i3/glib family, ICU, CF,
 * fastfetch) that would abort the process the first time it's called.
 */

/* creat(2): historical alias for open(O_WRONLY|O_CREAT|O_TRUNC). */
int
creat(const char *path, mode_t mode)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/*
 * close$NOCANCEL: the non-cancelable variant glib picks up from the SDK
 * headers. PD's close is not a pthread cancellation point anyway, so the
 * plain syscall wrapper is exactly the right behavior.
 */
int __pd_close_nocancel(int fd) __asm("_close$NOCANCEL");
int
__pd_close_nocancel(int fd)
{
    return close(fd);
}

/*
 * realpath: stdlib.h decorates the definition in stdlib/FreeBSD/realpath.c
 * as _realpath$DARWIN_EXTSN, but ICU (built against a plain-POSIX profile)
 * imports the undecorated _realpath. Forward one to the other.
 */
extern char *__pd_realpath_extsn(const char * __restrict, char * __restrict)
    __asm("_realpath$DARWIN_EXTSN");
char *__pd_realpath_plain(const char * __restrict, char * __restrict)
    __asm("_realpath");
char *
__pd_realpath_plain(const char * __restrict path, char * __restrict resolved)
{
    return __pd_realpath_extsn(path, resolved);
}

/*
 * dlopen_preflight(3): forwarded to dyld the same way as dlopen/dlsym above.
 * CF uses it to probe bundles before loading them.
 */
bool
dlopen_preflight(const char *path)
{
    typedef bool (*preflight_fn)(const char *, void *);
    static preflight_fn fn;

    if (fn == NULL &&
        !_dyld_func_lookup("__dyld_dlopen_preflight_internal", (void **)&fn)) {
        return false;
    }

    return fn(path, __builtin_return_address(0));
}

/*
 * swap_fat_header/swap_fat_arch/swap_fat_arch_64 (libmacho): fat headers are
 * stored big-endian; these unconditionally byte-swap every field (the real
 * ones take a target byte order argument, but the only sensible use on a
 * little-endian host is a full swap, which is also an involution).
 * fastfetch uses them to inspect fat binaries.
 */
struct __pd_fat_header { uint32_t magic, nfat_arch; };
struct __pd_fat_arch { uint32_t cputype, cpusubtype, offset, size, align; };
struct __pd_fat_arch_64 {
    uint32_t cputype, cpusubtype;
    uint64_t offset, size;
    uint32_t align, reserved;
};

void
swap_fat_header(struct __pd_fat_header *h, int target_byte_order)
{
    (void)target_byte_order;
    h->magic = __builtin_bswap32(h->magic);
    h->nfat_arch = __builtin_bswap32(h->nfat_arch);
}

void
swap_fat_arch(struct __pd_fat_arch *a, uint32_t n, int target_byte_order)
{
    (void)target_byte_order;
    for (uint32_t i = 0; i < n; i++) {
        a[i].cputype = __builtin_bswap32(a[i].cputype);
        a[i].cpusubtype = __builtin_bswap32(a[i].cpusubtype);
        a[i].offset = __builtin_bswap32(a[i].offset);
        a[i].size = __builtin_bswap32(a[i].size);
        a[i].align = __builtin_bswap32(a[i].align);
    }
}

void
swap_fat_arch_64(struct __pd_fat_arch_64 *a, uint32_t n, int target_byte_order)
{
    (void)target_byte_order;
    for (uint32_t i = 0; i < n; i++) {
        a[i].cputype = __builtin_bswap32(a[i].cputype);
        a[i].cpusubtype = __builtin_bswap32(a[i].cpusubtype);
        a[i].offset = __builtin_bswap64(a[i].offset);
        a[i].size = __builtin_bswap64(a[i].size);
        a[i].align = __builtin_bswap32(a[i].align);
        a[i].reserved = __builtin_bswap32(a[i].reserved);
    }
}

/*
 * strxfrm: the FreeBSD source needs the __collate_* locale machinery, which
 * PD doesn't build (C locale only). In the C locale strxfrm is defined to be
 * a plain copy whose result compares like strcmp - i.e. strlcpy semantics.
 */
size_t
strxfrm(char * __restrict dst, const char * __restrict src, size_t n)
{
    size_t len = strlen(src);

    if (n != 0) {
        size_t copy = (len >= n) ? n - 1 : len;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}


/*
 * qsort_b: referenced by gen/fts.c for the fts_open_b (block comparator)
 * path. Implemented on top of the real FreeBSD qsort_r with a trampoline
 * that invokes the block. Blocks-ABI note: a block pointer is a struct whose
 * 4th word is the invoke function taking the block itself as the hidden
 * first argument; calling it directly avoids needing the BlocksRuntime.
 */
struct __pd_block_layout {
    void *isa;
    int flags;
    int reserved;
    int (*invoke)(void *block, const void *a, const void *b);
};

extern void qsort_r(void *, size_t, size_t, void *,
    int (*)(void *, const void *, const void *));

static int
__pd_qsort_b_thunk(void *block, const void *a, const void *b)
{
    struct __pd_block_layout *bl = block;
    return bl->invoke(bl, a, b);
}

/* gen/FreeBSD/fmtcheck.c defines the real algorithm as __fmtcheck and
 * aliases it to fmtcheck via __weak_reference - a no-op in this fork's
 * sys/cdefs.h (see the ldexp/scalbn comment above for why). Forward
 * explicitly instead of relying on the alias. */
extern const char *__fmtcheck(const char *, const char *);
const char *
fmtcheck(const char *fmt, const char *fmt_default)
{
    return __fmtcheck(fmt, fmt_default);
}

void __pd_qsort_b(void *base, size_t nel, size_t width, void *block)
    __asm("_qsort_b");
void
__pd_qsort_b(void *base, size_t nel, size_t width, void *block)
{
    qsort_r(base, nel, width, block, __pd_qsort_b_thunk);
}

int
fls(int mask)
{
    return mask ? (int)(sizeof(mask) * 8 - (unsigned)__builtin_clz((unsigned)mask)) : 0;
}

void
_Exit(int status)
{
    _exit(status);
}

char **
backtrace_symbols(void *const *buffer, int size)
{
    if (size < 0)
        return NULL;

    /* One char* per frame followed by the string storage. Each line is
     * "<idx>  <address>" - up to ~40 bytes; use a fixed generous slot. */
    const size_t slot = 48;
    char **result = (char **)malloc((size_t)size * (sizeof(char *) + slot));
    if (result == NULL)
        return NULL;

    char *strings = (char *)(result + size);
    for (int i = 0; i < size; i++) {
        char *line = strings + (size_t)i * slot;
        snprintf(line, slot, "%-3d  %p", i, buffer[i]);
        result[i] = line;
    }
    return result;
}

void
backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
    char line[48];
    for (int i = 0; i < size; i++) {
        int n = snprintf(line, sizeof(line), "%-3d  %p\n", i, buffer[i]);
        if (n > 0)
            (void)write(fd, line, (size_t)n);
    }
}
