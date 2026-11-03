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
#include <math.h>
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

double nan(const char *tagp) __asm("_nan");
double nan(const char *tagp)
{
	(void)tagp;
	return __builtin_nan("");
}

float nanf(const char *tagp) __asm("_nanf");
float nanf(const char *tagp)
{
	(void)tagp;
	return __builtin_nanf("");
}

long double nanl(const char *tagp) __asm("_nanl");
long double nanl(const char *tagp)
{
	(void)tagp;
	return (long double)__builtin_nan("");
}

int __availability_version_check(uint32_t count, void *versions)
{
	(void)count;
	(void)versions;
	return 1;
}

void dispatch_once_f(long *predicate, void *context, void (*function)(void *))
{
	if (*predicate == 0) {
		*predicate = 1;
		function(context);
	}
}

int fls(int value)
{
	int result = 0;
	while (value != 0) {
		result++;
		value = (unsigned)value >> 1;
	}
	return result;
}

extern int __pd_close_default(int fd) __asm("_close");
extern int __pd_closedir_default(DIR *dirp) __asm("_closedir");
extern int __pd_connect_default(int socket, const struct sockaddr *address, socklen_t address_len) __asm("_connect");
extern int __pd_fcntl_syscall(int fd, int cmd, long arg) __asm("___fcntl");
extern int __pd_mprotect_default(void *addr, size_t len, int prot) __asm("_mprotect");
#if defined(__arm64__) || defined(__aarch64__)
extern DIR *__pd_opendir_native(const char *path) __asm("_opendir");
#else
extern DIR *__pd_opendir_inode64(const char *path) __asm("_opendir$INODE64");
#endif

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






#if !defined(__arm__) && !defined(__arm64__) && !defined(__aarch64__)
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
