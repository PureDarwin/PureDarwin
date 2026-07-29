/*
 * pd_stubs.c - permissive PureDarwin implementations of the Apple
 * source-available policy hooks that dyld calls but for which there is no open
 * implementation (AMFI, libsandbox). These match the declarations in
 * compat-include/{libamfi.h,sandbox/private.h}. PureDarwin has no AMFI or
 * sandbox, so both are fully permissive.
 */
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef PD_LD64LLD_WEAK_DYLD_FALLBACKS
#define PD_DYLD_FALLBACK_ATTR __attribute__((weak))
#else
#define PD_DYLD_FALLBACK_ATTR
#endif

int NXArgc = 0;
const char **NXArgv = 0;
PD_DYLD_FALLBACK_ATTR const char **environ = 0;
const char *__progname = 0;

extern int __pd_close_default(int fd) __asm("_close");
extern int __pd_closedir_default(DIR *dirp) __asm("_closedir");
extern int __pd_connect_default(int socket, const struct sockaddr *address, socklen_t address_len) __asm("_connect");
extern int __pd_fcntl_syscall(int fd, int cmd, long arg) __asm("___fcntl");
extern int __pd_getrlimit_unix2003(int resource, struct rlimit *rlp) __asm("_getrlimit$UNIX2003");
extern int __pd_mprotect_default(void *addr, size_t len, int prot) __asm("_mprotect");
extern int __pd_open_unix2003(const char *path, int flags, mode_t mode) __asm("_open$UNIX2003");
#if defined(__arm64__) || defined(__aarch64__)
extern DIR *__pd_opendir_native(const char *path) __asm("_opendir");
#else
extern DIR *__pd_opendir_inode64(const char *path) __asm("_opendir$INODE64");
#endif
extern ssize_t __pd_pread_default(int fd, void *buf, size_t nbyte, off_t offset) __asm("_pread");
extern ssize_t __pd_write_default(int fd, const void *buf, size_t nbyte) __asm("_write");
extern int __pd_pthread_cond_wait_unix2003(pthread_cond_t *cond, pthread_mutex_t *mutex) __asm("_pthread_cond_wait$UNIX2003");
extern int __pd_pthread_rwlock_destroy_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_destroy$UNIX2003");
extern int __pd_pthread_rwlock_rdlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_rdlock$UNIX2003");
extern int __pd_pthread_rwlock_unlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_unlock$UNIX2003");
extern int __pd_pthread_rwlock_wrlock_unix2003(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_wrlock$UNIX2003");
extern size_t __pd_fwrite_unix2003(const void *ptr, size_t size, size_t nitems, FILE *stream) __asm("_fwrite$UNIX2003");

int __pd_close_unix2003(int fd) __asm("_close$UNIX2003");
int __pd_close_unix2003(int fd) { return __pd_close_default(fd); }

int __pd_closedir_unix2003(DIR *dirp) __asm("_closedir$UNIX2003");
int __pd_closedir_unix2003(DIR *dirp) { return __pd_closedir_default(dirp); }

int __pd_connect_unix2003(int socket, const struct sockaddr *address, socklen_t address_len) __asm("_connect$UNIX2003");
int __pd_connect_unix2003(int socket, const struct sockaddr *address, socklen_t address_len)
{
	return __pd_connect_default(socket, address, address_len);
}

int __pd_fcntl_default(int fd, int cmd, ...) __asm("_fcntl");
int __pd_fcntl_default(int fd, int cmd, ...)
{
	va_list ap;
	long arg;

	va_start(ap, cmd);
	arg = va_arg(ap, long);
	va_end(ap);
	return __pd_fcntl_syscall(fd, cmd, arg);
}

int __pd_getrlimit_default(int resource, struct rlimit *rlp) __asm("_getrlimit");
int __pd_getrlimit_default(int resource, struct rlimit *rlp)
{
	return __pd_getrlimit_unix2003(resource, rlp);
}

int __pd_mprotect_unix2003(void *addr, size_t len, int prot) __asm("_mprotect$UNIX2003");
int __pd_mprotect_unix2003(void *addr, size_t len, int prot)
{
	return __pd_mprotect_default(addr, len, prot);
}

int __pd_open_default(const char *path, int flags, ...) __asm("_open");
int __pd_open_default(const char *path, int flags, ...)
{
	va_list ap;
	mode_t mode = 0;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int);
		va_end(ap);
	}
	return __pd_open_unix2003(path, flags, mode);
}

DIR *__pd_opendir_inode64_unix2003(const char *path) __asm("_opendir$INODE64$UNIX2003");
DIR *__pd_opendir_inode64_unix2003(const char *path)
{
#if defined(__arm64__) || defined(__aarch64__)
	return __pd_opendir_native(path);
#else
	return __pd_opendir_inode64(path);
#endif
}

ssize_t __pd_pread_unix2003(int fd, void *buf, size_t nbyte, off_t offset) __asm("_pread$UNIX2003");
ssize_t __pd_pread_unix2003(int fd, void *buf, size_t nbyte, off_t offset)
{
	return __pd_pread_default(fd, buf, nbyte, offset);
}

ssize_t __pd_write_unix2003(int fd, const void *buf, size_t nbyte) __asm("_write$UNIX2003");
ssize_t __pd_write_unix2003(int fd, const void *buf, size_t nbyte)
{
	return __pd_write_default(fd, buf, nbyte);
}

int __pd_pthread_cond_wait_default(pthread_cond_t *cond, pthread_mutex_t *mutex) __asm("_pthread_cond_wait");
int __pd_pthread_cond_wait_default(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	return __pd_pthread_cond_wait_unix2003(cond, mutex);
}

int __pd_pthread_rwlock_destroy_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_destroy");
int __pd_pthread_rwlock_destroy_default(pthread_rwlock_t *rwlock)
{
	return __pd_pthread_rwlock_destroy_unix2003(rwlock);
}

int __pd_pthread_rwlock_rdlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_rdlock");
int __pd_pthread_rwlock_rdlock_default(pthread_rwlock_t *rwlock)
{
	return __pd_pthread_rwlock_rdlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_unlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_unlock");
int __pd_pthread_rwlock_unlock_default(pthread_rwlock_t *rwlock)
{
	return __pd_pthread_rwlock_unlock_unix2003(rwlock);
}

int __pd_pthread_rwlock_wrlock_default(pthread_rwlock_t *rwlock) __asm("_pthread_rwlock_wrlock");
int __pd_pthread_rwlock_wrlock_default(pthread_rwlock_t *rwlock)
{
	return __pd_pthread_rwlock_wrlock_unix2003(rwlock);
}

#if !defined(__arm64__) && !defined(__aarch64__)
size_t __pd_fwrite_default(const void *ptr, size_t size, size_t nitems, FILE *stream) __asm("_fwrite");
size_t __pd_fwrite_default(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
	return __pd_fwrite_unix2003(ptr, size, nitems, stream);
}
#endif

/* AMFI: grant every dyld capability. */
int amfi_check_dyld_policy_self(uint64_t input_flags, uint64_t *output_flags)
{
	(void)input_flags;
	if (output_flags)
		*output_flags = ~0ull;   /* allow @path, path vars, fallback paths, interposing, ... */
	return 0;
}

/* sandbox: nothing is sandboxed. */
int sandbox_check(int pid, const char *operation, unsigned int type, ...)
{
	(void)pid; (void)operation; (void)type;
	return 0;   /* 0 == allowed / not-in-sandbox */
}

/* voucher_mach_msg_{adopt,revert}: libdispatch's mach-voucher hooks. dyld's
 * mach_msg wrappers reference them, but PureDarwin has no libdispatch voucher
 * machinery; no-op them (adopt returns "no previous voucher"). */
typedef unsigned int mach_voucher_t;
typedef struct mach_msg_header_t mach_msg_header_t;
mach_voucher_t voucher_mach_msg_adopt(mach_msg_header_t *msg)
{
	(void)msg;
	return 0;   /* MACH_VOUCHER_NULL / MACH_PORT_NULL */
}
void voucher_mach_msg_revert(mach_voucher_t voucher)
{
	(void)voucher;
}

#include <stddef.h>
extern void *malloc(size_t);
extern void abort(void);
void *aligned_alloc(size_t alignment, size_t size)
{
	if (alignment > 16)
		abort();   /* dyld's pool only guarantees 16-byte alignment */
	return malloc(size);
}

extern int getentropy(void *buf, size_t buflen);

uint32_t arc4random(void)
{
	uint32_t v = 0;
	if (getentropy(&v, sizeof(v)) != 0)
		v = 0;
	return v;
}

void arc4random_buf(void *buf, size_t nbytes)
{
	unsigned char *p = (unsigned char *)buf;
	/* getentropy() only guarantees up to 256 bytes per call. */
	while (nbytes > 0) {
		size_t chunk = nbytes > 256 ? 256 : nbytes;
		if (getentropy(p, chunk) != 0)
			break;
		p += chunk;
		nbytes -= chunk;
	}
}

void arc4random_stir(void) { /* no persistent state to restir */ }
void arc4random_addrandom(unsigned char *data, int datalen)
{
	(void)data; (void)datalen; /* getentropy() needs no caller-supplied entropy */
}

extern void _ZN4dyld4haltEPKc(const char *msg) __attribute__((noreturn));

void *_Block_copy(const void *block)
{
	(void)block;
	_ZN4dyld4haltEPKc("_Block_copy()");
}

void _Block_release(const void *block)
{
	(void)block;
	_ZN4dyld4haltEPKc("_Block_release()");
}
