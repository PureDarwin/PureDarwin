// pdnv_os.c - the os_* services NVIDIA's resource manager calls, in xnu terms, as its Linux layer defines them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <kern/clock.h>
#include <kern/locks.h>
#include <kern/thread.h>
#include <libkern/libkern.h>
#include <mach/mach_time.h>
#include <pexpert/pexpert.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <stdarg.h>

#include <nvtypes.h>
#include <nvstatus.h>
#include <os-interface.h>
#include <nv.h>
#include <rs_access.h>

#include "pdnv_port.h"

extern int cpu_number(void);
extern boolean_t ml_at_interrupt_context(void);
extern boolean_t ml_get_interrupts_enabled(void);
extern boolean_t ml_set_interrupts_enabled(boolean_t enable);
extern unsigned int ml_wait_max_cpus(void);
#include "pdnv_fmt.h"

#define X86_EFLAGS_IF   0x200

// a pointer the resource manager passes to print through must be a kernel one, anything else is logged
// with where in the image the call came from instead of being followed
#define PDNV_KPTR(p)    ((uintptr_t)(p) >= 0xffff800000000000ull)

static int
bad_text(const char *who, const void *p, const void *caller)
{
	if (PDNV_KPTR(p))
		return 0;

	IOLog("PDNVLoader: %s got text pointer 0x%llx, called from offset 0x%llx of the top 2 GB\n", who,
	    (unsigned long long)(uintptr_t)p, (unsigned long long)((uintptr_t)caller - 0xffffffff80000000ull));
	return 1;
}

// memory: a header keeps the size for the free

struct pdnv_alloc {
	NvU64 size;
	NvU64 pad;
};

NV_STATUS NV_API_CALL
os_alloc_mem(void **address, NvU64 size)
{
	struct pdnv_alloc *a;

	if (address == NULL)
		return NV_ERR_INVALID_ARGUMENT;

	*address = NULL;
	if (size + sizeof(*a) < size)
		return NV_ERR_INVALID_ARGUMENT;

	a = IOMalloc(size + sizeof(*a));
	if (a == NULL)
		return NV_ERR_NO_MEMORY;

	a->size = size;
	*address = a + 1;
	return NV_OK;
}

void NV_API_CALL
os_free_mem(void *address)
{
	struct pdnv_alloc *a = (struct pdnv_alloc *)address - 1;

	if (address == NULL)
		return;

	IOFree(a, a->size + sizeof(*a));
}

void * NV_API_CALL
os_mem_set(void *dst, NvU8 c, NvU32 len)
{
	return memset(dst, c, len);
}

void * NV_API_CALL
os_mem_copy(void *dst, const void *src, NvU32 len)
{
	return memmove(dst, src, len);
}

NvS32 NV_API_CALL
os_mem_cmp(const NvU8 *a, const NvU8 *b, NvU32 len)
{
	return memcmp(a, b, len);
}

// strings and formatting

NvS32 NV_API_CALL
os_string_compare(const char *a, const char *b)
{
	return strcmp(a, b);
}

char * NV_API_CALL
os_string_copy(char *dst, const char *src)
{
	char *d = dst;

	while ((*d++ = *src++) != '\0')
		;
	return dst;
}

NvU32 NV_API_CALL
os_string_length(const char *s)
{
	return (NvU32)strlen(s);
}

NvU32 NV_API_CALL
os_strtoul(const char *s, char **end, NvU32 base)
{
	return (NvU32)strtoul(s, end, (int)base);
}

NvS32 NV_API_CALL
os_snprintf(char *buf, NvU32 size, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (bad_text("os_snprintf", fmt, __builtin_return_address(0))) {
		if (size > 0)
			buf[0] = '\0';
		return 0;
	}
	va_start(ap, fmt);
	n = pdnv_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

// the resource manager's own log: bits 4-5 of the debug level are the threshold, all-ones (3) at start
static NvU32 sDebugLevel = ~0u;
// pdnvdebug=1 prints every level, whatever the resource manager sets
static int sDebugAll;

void NV_API_CALL
out_string(const char *str)
{
	if (bad_text("out_string", str, __builtin_return_address(0)))
		return;

	IOLog("%s", str);
}

int NV_API_CALL
nv_printf(NvU32 level, const char *fmt, ...)
{
	char line[512];
	va_list ap;

	if (bad_text("nv_printf", fmt, __builtin_return_address(0)))
		return 0;

	if (!sDebugAll && level < ((sDebugLevel >> 4) & 0x3))
		return 0;

	va_start(ap, fmt);
	pdnv_vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	IOLog("%s", line);
	return 0;
}

NV_STATUS NV_API_CALL
nv_log_error(nv_state_t *nv, NvU32 num, const char *fmt, va_list ap)
{
	char line[512];

	if (bad_text("nv_log_error", fmt, __builtin_return_address(0)))
		return NV_OK;

	pdnv_vsnprintf(line, sizeof(line), fmt, ap);
	IOLog("NVRM: error %u: %s", num, line);
	return NV_OK;
}

// hooks for Linux's crash dump records, which have no counterpart here
void NV_API_CALL
os_add_record_for_crashLog(void *buffer, NvU32 size)
{
}

void NV_API_CALL
os_delete_record_for_crashLog(void *buffer)
{
}

void NV_API_CALL
os_dbg_init(void)
{
	int on = 0;

	sDebugAll = PE_parse_boot_argn("pdnvdebug", &on, sizeof(on)) && on != 0;
}

void NV_API_CALL
os_dbg_set_level(NvU32 level)
{
	sDebugLevel = level;
}

void NV_API_CALL
os_dbg_breakpoint(void)
{
	IOLog("NVRM: breakpoint requested\n");
}

void NV_API_CALL
os_dump_stack(void)
{
	IOLog("NVRM: stack dump requested\n");
}

// the string is printed only when it can be a kernel pointer, a bad one would fault inside the panic
void NV_API_CALL
os_bug_check(NvU32 code, const char *msg)
{
	IOLog("PDNVLoader: os_bug_check 0x%x, string pointer 0x%llx, called from offset 0x%llx\n", code,
	    (unsigned long long)(uintptr_t)msg,
	    (unsigned long long)((uintptr_t)__builtin_return_address(0) - 0xffffffff80000000ull));
	if ((uintptr_t)msg >= 0xffff800000000000ull)
		panic("NVRM: bug check 0x%x: %s", code, msg);
	panic("NVRM: bug check 0x%x, string pointer 0x%llx", code, (unsigned long long)(uintptr_t)msg);
}

// spinlocks: interrupts off, the caller gets back whether they were on

typedef struct {
	IOSimpleLock *lock;
	boolean_t enabled;
} pdnv_spinlock_t;

NV_STATUS NV_API_CALL
os_alloc_spinlock(void **lock)
{
	pdnv_spinlock_t *s;

	if (os_alloc_mem((void **)&s, sizeof(*s)) != NV_OK)
		return NV_ERR_NO_MEMORY;

	s->lock = IOSimpleLockAlloc();
	if (s->lock == NULL) {
		os_free_mem(s);
		return NV_ERR_NO_MEMORY;
	}
	*lock = s;
	return NV_OK;
}

void NV_API_CALL
os_free_spinlock(void *lock)
{
	pdnv_spinlock_t *s = lock;

	IOSimpleLockFree(s->lock);
	os_free_mem(s);
}

NvU64 NV_API_CALL
os_acquire_spinlock(void *lock)
{
	pdnv_spinlock_t *s = lock;
	boolean_t enabled = ml_set_interrupts_enabled(FALSE);

	IOSimpleLockLock(s->lock);
	s->enabled = enabled;
	return enabled ? X86_EFLAGS_IF : 0;
}

void NV_API_CALL
os_release_spinlock(void *lock, NvU64 old)
{
	pdnv_spinlock_t *s = lock;
	boolean_t enabled = s->enabled;

	IOSimpleLockUnlock(s->lock);
	ml_set_interrupts_enabled(enabled);
}

// sleeping is allowed outside interrupt context with interrupts on
NvBool NV_API_CALL
os_semaphore_may_sleep(void)
{
	return !ml_at_interrupt_context() && ml_get_interrupts_enabled();
}

NvBool NV_API_CALL
os_is_isr(void)
{
	return ml_at_interrupt_context();
}

// mutexes

NV_STATUS NV_API_CALL
os_alloc_mutex(void **mutex)
{
	*mutex = IOLockAlloc();
	return *mutex != NULL ? NV_OK : NV_ERR_NO_MEMORY;
}

void NV_API_CALL
os_free_mutex(void *mutex)
{
	IOLockFree(mutex);
}

NV_STATUS NV_API_CALL
os_acquire_mutex(void *mutex)
{
	if (!os_semaphore_may_sleep())
		return NV_ERR_INVALID_REQUEST;

	IOLockLock((IOLock *)mutex);
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_cond_acquire_mutex(void *mutex)
{
	if (!os_semaphore_may_sleep())
		return NV_ERR_INVALID_REQUEST;

	return IOLockTryLock((IOLock *)mutex) ? NV_OK : NV_ERR_TIMEOUT_RETRY;
}

void NV_API_CALL
os_release_mutex(void *mutex)
{
	IOLockUnlock((IOLock *)mutex);
}

// counting semaphores: a count under a lock, sleepers woken one at a time

typedef struct {
	IOLock *lock;
	NvU32 count;
} pdnv_sema_t;

void * NV_API_CALL
os_alloc_semaphore(NvU32 initial)
{
	pdnv_sema_t *s;

	if (os_alloc_mem((void **)&s, sizeof(*s)) != NV_OK)
		return NULL;

	s->lock = IOLockAlloc();
	if (s->lock == NULL) {
		os_free_mem(s);
		return NULL;
	}
	s->count = initial;
	return s;
}

void NV_API_CALL
os_free_semaphore(void *sema)
{
	pdnv_sema_t *s = sema;

	IOLockFree(s->lock);
	os_free_mem(s);
}

NV_STATUS NV_API_CALL
os_acquire_semaphore(void *sema)
{
	pdnv_sema_t *s = sema;

	if (!os_semaphore_may_sleep())
		return NV_ERR_INVALID_REQUEST;

	IOLockLock(s->lock);
	while (s->count == 0)
		IOLockSleep(s->lock, &s->count, THREAD_UNINT);
	s->count--;
	IOLockUnlock(s->lock);
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_cond_acquire_semaphore(void *sema)
{
	pdnv_sema_t *s = sema;
	NV_STATUS status = NV_ERR_TIMEOUT_RETRY;

	IOLockLock(s->lock);
	if (s->count > 0) {
		s->count--;
		status = NV_OK;
	}
	IOLockUnlock(s->lock);
	return status;
}

NV_STATUS NV_API_CALL
os_release_semaphore(void *sema)
{
	pdnv_sema_t *s = sema;

	IOLockLock(s->lock);
	s->count++;
	IOLockWakeup(s->lock, &s->count, true);
	IOLockUnlock(s->lock);
	return NV_OK;
}

// reader-writer locks

void * NV_API_CALL
os_alloc_rwlock(void)
{
	return IORWLockAlloc();
}

void NV_API_CALL
os_free_rwlock(void *rw)
{
	IORWLockFree(rw);
}

NV_STATUS NV_API_CALL
os_acquire_rwlock_read(void *rw)
{
	if (!os_semaphore_may_sleep())
		return NV_ERR_INVALID_REQUEST;

	IORWLockRead((IORWLock *)rw);
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_acquire_rwlock_write(void *rw)
{
	if (!os_semaphore_may_sleep())
		return NV_ERR_INVALID_REQUEST;

	IORWLockWrite((IORWLock *)rw);
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_cond_acquire_rwlock_read(void *rw)
{
	return lck_rw_try_lock((lck_rw_t *)rw, LCK_RW_TYPE_SHARED) ? NV_OK : NV_ERR_TIMEOUT_RETRY;
}

NV_STATUS NV_API_CALL
os_cond_acquire_rwlock_write(void *rw)
{
	return lck_rw_try_lock((lck_rw_t *)rw, LCK_RW_TYPE_EXCLUSIVE) ? NV_OK : NV_ERR_TIMEOUT_RETRY;
}

void NV_API_CALL
os_release_rwlock_read(void *rw)
{
	IORWLockUnlock((IORWLock *)rw);
}

void NV_API_CALL
os_release_rwlock_write(void *rw)
{
	IORWLockUnlock((IORWLock *)rw);
}

// time

static NvU64
abs_to_ns(uint64_t abs)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(abs, &ns);
	return ns;
}

NvU64 NV_API_CALL
os_get_monotonic_time_ns(void)
{
	return abs_to_ns(mach_absolute_time());
}

NvU64 NV_API_CALL
os_get_monotonic_time_ns_hr(void)
{
	return abs_to_ns(mach_absolute_time());
}

NvU64 NV_API_CALL
os_get_monotonic_tick_resolution_ns(void)
{
	return 1;
}

NV_STATUS NV_API_CALL
os_get_system_time(NvU32 *sec, NvU32 *usec)
{
	clock_sec_t s;
	clock_usec_t us;

	clock_get_calendar_microtime(&s, &us);
	*sec = (NvU32)s;
	*usec = (NvU32)us;
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_delay_us(NvU32 us)
{
	IODelay(us);
	return NV_OK;
}

// sleeps when it may, spins when it must
NV_STATUS NV_API_CALL
os_delay(NvU32 ms)
{
	if (os_semaphore_may_sleep())
		IOSleep(ms);
	else
		IODelay(ms * 1000);
	return NV_OK;
}

// the processor and the process

NvU32 NV_API_CALL
os_get_cpu_count(void)
{
	return ml_wait_max_cpus();
}

NvU32 NV_API_CALL
os_get_cpu_number(void)
{
	return (NvU32)cpu_number();
}

NvU64 NV_API_CALL
os_get_cpu_frequency(void)
{
	return gPEClockFrequencyInfo.cpu_frequency_hz;
}

NvU32 NV_API_CALL
os_get_current_process(void)
{
	return (NvU32)proc_selfpid();
}

void NV_API_CALL
os_get_current_process_name(char *buf, NvU32 len)
{
	proc_selfname(buf, (int)len);
}

NV_STATUS NV_API_CALL
os_get_current_thread(NvU64 *id)
{
	*id = thread_tid(current_thread());
	return NV_OK;
}

NV_STATUS NV_API_CALL
os_get_euid(NvU32 *uid)
{
	*uid = kauth_cred_getuid(kauth_cred_get());
	return NV_OK;
}

NvBool NV_API_CALL
os_is_administrator(void)
{
	return kauth_cred_getuid(kauth_cred_get()) == 0;
}

// the calling thread is never an exiting user process here, kernel threads are those of kernel_task
NvU32 NV_API_CALL
os_get_current_process_flags(void)
{
	return proc_selfpid() == 0 ? OS_CURRENT_PROCESS_FLAG_KERNEL_THREAD : OS_CURRENT_PROCESS_FLAG_NONE;
}

// the rights the Linux layer grants by capability, root here
NvBool NV_API_CALL
os_check_access(RsAccessRight accessRight)
{
	switch (accessRight) {
	case RS_ACCESS_PERFMON:
	case RS_ACCESS_NICE:
		return os_is_administrator();
	default:
		return NV_FALSE;
	}
}

NV_STATUS NV_API_CALL
os_get_random_bytes(NvU8 *buf, NvU16 len)
{
	// read_random can block, the resource manager calls this with a spinlock held and interrupts off
	if (ml_get_interrupts_enabled()) {
		read_random(buf, len);
		return NV_OK;
	}
	while (len > 0) {
		unsigned long long v;
		unsigned char ok = 0;
		NvU16 n = len < 8 ? len : 8;

		for (int i = 0; i < 16 && !ok; i++)
			__asm__ volatile ("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
		if (!ok)
			return NV_ERR_NOT_SUPPORTED;
		memcpy(buf, &v, n);
		buf += n;
		len -= n;
	}
	return NV_OK;
}

// what this system is and is not, as the Linux layer answers on bare metal

// a desktop's answers. A laptop needs the SMBIOS chassis type, the ACPI battery device and the FADT
// low-power S0 flag here, as the Linux layer reads them
NvBool NV_API_CALL
nv_is_chassis_notebook(void)
{
	return NV_FALSE;
}

NvBool NV_API_CALL
nv_acpi_is_battery_present(void)
{
	return NV_FALSE;
}

NvBool NV_API_CALL
nv_platform_supports_s0ix(void)
{
	return NV_FALSE;
}

NvBool NV_API_CALL
os_is_efi_enabled(void)
{
	return NV_TRUE;
}

NvBool NV_API_CALL
os_is_init_ns(void)
{
	return NV_TRUE;
}

NvBool NV_API_CALL
os_is_grid_supported(void)
{
	return NV_FALSE;
}

NvU32 NV_API_CALL
os_get_grid_csp_support(void)
{
	return 0;
}

NvBool NV_API_CALL
os_is_vgx_hyper(void)
{
	return NV_FALSE;
}

NvBool NV_API_CALL
os_is_nvswitch_present(void)
{
	return NV_FALSE;
}

NV_STATUS NV_API_CALL
os_device_vm_present(void)
{
	return NV_ERR_NOT_SUPPORTED;
}

NV_STATUS NV_API_CALL
os_get_is_openrm(NvBool *open)
{
	*open = NV_FALSE;
	return NV_OK;
}

// x86-64's user address space ends below the canonical hole
NvU64 NV_API_CALL
os_get_max_user_va(void)
{
	return 0x00007ffffffff000ull;
}

void NV_API_CALL
os_flush_cpu_write_combine_buffer(void)
{
	__asm__ volatile("sfence" ::: "memory");
}
