#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <malloc/malloc.h>

struct __cxa_range_t { const void *addr; size_t length; };

struct ProgramVars {
	void *mh;
	int *NXArgcPtr;
	char ***NXArgvPtr;
	char ***environPtr;
	char **__prognamePtr;
};

/* Mirrors dyld::LibSystemHelpers (dyld/src/dyldLibSystemInterface.h). */
struct LibSystemHelpers {
	uintptr_t version;
	void   (*acquireGlobalDyldLock)(void);
	void   (*releaseGlobalDyldLock)(void);
	char  *(*getThreadBufferFor_dlerror)(size_t sizeRequired);
	void  *(*malloc)(size_t);
	void   (*free)(void *);
	int    (*cxa_atexit)(void (*)(void *), void *, void *);
	void   (*dyld_shared_cache_missing)(void);
	void   (*dyld_shared_cache_out_of_date)(void);
	void   (*acquireDyldInitializerLock)(void);
	void   (*releaseDyldInitializerLock)(void);
	int    (*pthread_key_create)(pthread_key_t *, void (*destructor)(void *));
	int    (*pthread_setspecific)(pthread_key_t, const void *);
	size_t (*malloc_size)(const void *ptr);
	void  *(*pthread_getspecific)(pthread_key_t);
	void   (*cxa_finalize)(const void *);
	void  *startGlueToCallExit;
	bool   (*hasPerThreadBufferFor_dlerror)(void);
	bool   (*isLaunchdOwned)(void);
	kern_return_t (*vm_alloc)(vm_map_t task, vm_address_t *addr, vm_size_t size, int flags);
	void  *(*mmap)(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
	void   (*cxa_finalize_ranges)(const struct __cxa_range_t ranges[], int count);
};

extern void *pd_libdyld_getStartGlueToCallExit(void);
extern int _dyld_func_lookup(const char *name, void **address);
extern void _pthread_set_self(void *p);   /* p == NULL selects the VARIANT_DYLD seed path */
extern void __malloc_init(const char *apple[]);
extern int mach_init(void);

struct _libpthread_functions {
	unsigned long version;
	void (*exit)(int);
	void *(*malloc)(size_t);
	void (*free)(void *);
};
extern int __pthread_init(const struct _libpthread_functions *pthread_funcs,
    const char *envp[], const char *apple[], const struct ProgramVars *vars);
static void __libdarwin_init(void) { }

extern void libdispatch_init(void);
/* Exported wrapper in libdyld.dylib (pd_libdyld_exports.c) for the hidden-
 * visibility tlv_initializer() -- see that file for why the bridge is needed. */
extern void pd_libdyld_tlv_initializer(void);

#include "../libc/darwin/libc_private.h"
extern void _malloc_fork_prepare(void);
extern void _malloc_fork_parent(void);
extern void _malloc_fork_child(void);
extern void _pthread_fork_prepare(void);
extern void _pthread_fork_parent(void);
extern void _pthread_fork_child(void);
extern void _pthread_fork_child_postinit(void);
extern int  _mach_fork_child(void);
extern void dispatch_atfork_prepare(void);
extern void dispatch_atfork_parent(void);
extern void dispatch_atfork_child(void);
extern void _notify_fork_child(void);

static void
pd_atfork_prepare(void)
{
	dispatch_atfork_prepare();
	_malloc_fork_prepare();
	_pthread_fork_prepare();
}

static void
pd_atfork_parent(void)
{
	_pthread_fork_parent();
	_malloc_fork_parent();
	dispatch_atfork_parent();
}

static void
pd_atfork_child(void)
{
	_mach_fork_child();
	_pthread_fork_child();
	_malloc_fork_child();
	_libc_fork_child();
	dispatch_atfork_child();
	_notify_fork_child();
	/* client-registered pthread_atfork() child handlers run last, same as
	 * real Apple's libSystem_atfork_child() - _pthread_fork_child() itself
	 * only does the internal (non-client) piece, see pthread_atfork.c. */
	_pthread_fork_child_postinit();
}

static pthread_mutex_t sGlobalDyldLock = PTHREAD_MUTEX_INITIALIZER;
static void acquireGlobalDyldLock(void) { pthread_mutex_lock(&sGlobalDyldLock); }
static void releaseGlobalDyldLock(void) { pthread_mutex_unlock(&sGlobalDyldLock); }
static char sDlerrorBuffer[512];
static char *getThreadBufferFor_dlerror(size_t sizeRequired)
{
	return (sizeRequired <= sizeof(sDlerrorBuffer)) ? sDlerrorBuffer : NULL;
}
static bool hasPerThreadBufferFor_dlerror(void) { return false; }

static void noop_void(void) {}

static bool isLaunchdOwned(void) { return getpid() == 1; }

extern int  __cxa_atexit(void (*)(void *), void *, void *);
extern void __cxa_finalize(const void *);
extern void __cxa_finalize_ranges(const struct __cxa_range_t ranges[], int count);

static struct LibSystemHelpers sHelpers = {
	.version                     = 13,
	.acquireGlobalDyldLock       = &acquireGlobalDyldLock,
	.releaseGlobalDyldLock       = &releaseGlobalDyldLock,
	.getThreadBufferFor_dlerror  = &getThreadBufferFor_dlerror,
	.malloc                      = &malloc,
	.free                        = &free,
	.cxa_atexit                  = &__cxa_atexit,
	.dyld_shared_cache_missing   = &noop_void,
	.dyld_shared_cache_out_of_date = &noop_void,
	.acquireDyldInitializerLock  = NULL,
	.releaseDyldInitializerLock  = NULL,
	.pthread_key_create          = &pthread_key_create,
	.pthread_setspecific         = &pthread_setspecific,
	.malloc_size                 = &malloc_size,
	.pthread_getspecific         = &pthread_getspecific,
	.cxa_finalize                = &__cxa_finalize,
	.startGlueToCallExit         = NULL,  /* filled at runtime below - see pd_libSystem_initializer() */
	.hasPerThreadBufferFor_dlerror = &hasPerThreadBufferFor_dlerror,
	.isLaunchdOwned              = &isLaunchdOwned,
	.vm_alloc                    = &vm_allocate,
	.mmap                        = &mmap,
	.cxa_finalize_ranges         = &__cxa_finalize_ranges,
};

__attribute__((constructor))
static void pd_libSystem_initializer(int argc, const char *argv[], const char *envp[],
    const char *apple[], const struct ProgramVars *vars)
{
	static int fallback_argc;
	static const char **fallback_argv;
	static const char **fallback_envp;
	static char *fallback_progname;
	static struct ProgramVars fallback_vars;

	if (vars == NULL) {
		fallback_argc = argc;
		fallback_argv = argv;
		fallback_envp = envp;
		fallback_progname = (argv && argv[0]) ? (char *)argv[0] : NULL;
		fallback_vars.mh = NULL;
		fallback_vars.NXArgcPtr = &fallback_argc;
		fallback_vars.NXArgvPtr = (char ***)&fallback_argv;
		fallback_vars.environPtr = (char ***)&fallback_envp;
		fallback_vars.__prognamePtr = &fallback_progname;
		vars = &fallback_vars;
	}

	mach_init();
	/* Real dyld seeds this minimal TSD itself, in its own separately-linked
	 * copy of pthread_static, before any dylib initializer (including this
	 * one) ever runs - see _pthread_set_self_dyld() in pthread.c. PD's dyld
	 * doesn't do that, so we seed it here instead, right before the real
	 * __pthread_init() (which expects _pthread_self_direct() to already
	 * return non-NULL) needs it. */
	_pthread_set_self(NULL);
	(void)task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bootstrap_port);

	{
		const struct _libpthread_functions pthread_funcs = {
			.version = 2,
			.exit = exit,
			.malloc = malloc,
			.free = free,
		};
		__pthread_init(&pthread_funcs, envp, apple, vars);
	}

	{
		const struct _libc_functions libc_funcs = {
			.version = 1,
			.atfork_prepare = pd_atfork_prepare,
			.atfork_parent = pd_atfork_parent,
			.atfork_child = pd_atfork_child,
			.dirhelper = NULL,
		};
		_libc_initializer(&libc_funcs, envp, apple, vars);
	}

	__malloc_init(apple);
	__libdarwin_init();

	/* Apple order: malloc -> (keymgr) -> dyld -> pthread_late -> libdispatch.
	 * PD lacks the intermediate OSX-only steps, so right after malloc is the
	 * correct earliest-safe point (pthread + malloc are up, which is all
	 * libdispatch_init() needs). Without this, notifyd aborts the moment it
	 * touches a dispatch queue/source - see extern decl above. */
	libdispatch_init();

	/* __malloc_late_init() is deliberately NOT called: its real work is
	 * wiring up dlopen-based ASan/MallocStackLogging dylib support (see
	 * stack_logging_early_finished() in malloc.c), which PD doesn't have/
	 * need - and stack_logging_early_finished() unconditionally dereferences
	 * *_NSGetEnviron(), which can still be NULL this early (environ_pointer
	 * is populated by _program_vars_init(), whose dyld-side wiring isn't
	 * guaranteed to have already resolved for every caller by this point in
	 * PD's merged single-dylib boot). Real risk for zero real benefit here. */

	sHelpers.startGlueToCallExit = pd_libdyld_getStartGlueToCallExit();

	typedef void (*register_fn)(struct LibSystemHelpers *);
	register_fn reg = NULL;
	if (_dyld_func_lookup("__dyld_register_thread_helpers", (void **)&reg) && reg != NULL)
		reg(&sHelpers);

	/* Thread-local-variable runtime. Real Apple runs this inside
	 * _dyld_initializer() (dyldAPIsInLibSystem.cpp); PD open-codes the
	 * thread-helper registration above instead of calling _dyld_initializer(),
	 * so tlv_initializer() would otherwise never run. Without it, dyld never
	 * gets the tlv_load_notification add-image callback, no image's
	 * __thread_vars descriptors are ever initialized, and the FIRST access to
	 * any __thread variable jumps through an uninitialized (null/_tlv_bootstrap)
	 * thunk -> SIGSEGV. Must come after pthread + malloc are up (it does
	 * pthread_key_create); registering here also fires the notification
	 * immediately for every already-mapped dylib (e.g. libOSMesa's glapi TLS). */
	pd_libdyld_tlv_initializer();

	/* C99 7.5(3) / rdar://11588042: errno is zero at program startup and is
	 * never set to zero by any library function - so clear whatever the init
	 * steps above dirtied before handing control to the program's main().
	 * Real libSystem_initializer() ends with exactly this. */
	errno = 0;
}
