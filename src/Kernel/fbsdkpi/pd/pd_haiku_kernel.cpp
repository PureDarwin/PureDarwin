// SPDX-License-Identifier: MIT
// the haiku kernel api that the compat layer calls, built on the fbsdkpi seam

#include <OS.h>
#include <KernelExport.h>
#include <PCI.h>
#include <module.h>
#include <lock.h>
#include <condition_variable.h>
#include <thread.h>
#include <smp.h>
#include <interrupts.h>
#include <kernel/heap.h>
#include <kernel/vm/vm.h>
#include <slab/Slab.h>
#include <net_buffer.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pd_fbsd_os.h"
#include "pd_haiku_kernel.h"

extern "C" {

// console

void
dprintf(const char *format, ...)
{
	if (!pdos_debug_enabled())
		return;
	char buf[512];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	pdos_log(buf);
}

void
kprintf(const char *format, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	pdos_log(buf);
}

void
panic(const char *format, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	pdos_panic(buf);
}

// time

bigtime_t
system_time(void)
{
	return pdos_uptime_us();
}

nanotime_t
system_time_nsecs(void)
{
	return pdos_uptime_us() * 1000;
}

status_t
snooze(bigtime_t amount)
{
	if (amount > 0)
		pdos_sleep_us(amount);
	return B_OK;
}

status_t
snooze_etc(bigtime_t amount, int timeBase, uint32 flags)
{
	if ((flags & B_ABSOLUTE_TIMEOUT) != 0)
		amount -= system_time();
	return snooze(amount);
}

status_t
snooze_until(bigtime_t time, int timeBase)
{
	return snooze(time - system_time());
}

void
spin(bigtime_t microseconds)
{
	pdos_delay_us(microseconds);
}

// interrupts and spinlocks

cpu_status
disable_interrupts(void)
{
	return pdos_intr_disable();
}

void
restore_interrupts(cpu_status status)
{
	pdos_intr_restore((int)status);
}

bool
are_interrupts_enabled(void)
{
	int state = pdos_intr_disable();
	pdos_intr_restore(state);
	return state != 0;
}

void
acquire_spinlock(spinlock *lock)
{
	while (__atomic_exchange_n(&lock->lock, 1, __ATOMIC_ACQUIRE) != 0) {
		while (__atomic_load_n(&lock->lock, __ATOMIC_RELAXED) != 0)
			pd_cpu_pause();
	}
}

void
release_spinlock(spinlock *lock)
{
	__atomic_store_n(&lock->lock, 0, __ATOMIC_RELEASE);
}

bool
try_acquire_spinlock(spinlock *lock)
{
	return __atomic_exchange_n(&lock->lock, 1, __ATOMIC_ACQUIRE) == 0;
}

void
acquire_write_seqlock(seqlock *lock)
{
	acquire_spinlock(&lock->lock);
	__atomic_add_fetch(&lock->count, 1, __ATOMIC_RELEASE);
}

void
release_write_seqlock(seqlock *lock)
{
	__atomic_add_fetch(&lock->count, 1, __ATOMIC_RELEASE);
	release_spinlock(&lock->lock);
}

// smp

int32
smp_get_num_cpus(void)
{
	return pdos_cpu_count();
}

int32
smp_get_current_cpu(void)
{
	return pdos_cpu_number();
}

// thread ids, haiku code compares owners through find_thread(NULL), so every
// thread that enters the layer needs a stable id, including iokit threads

#define PD_THREAD_SLOTS		8192

struct pd_thread {
	void			*thread;
	thread_id		id;
	thread_func		func;
	void			*arg;
	int32			resumed;
	int32			exited;
	status_t		result;
	int32			priority;
	char			name[B_OS_NAME_LENGTH];
	void			*fbsd_thread;	// linuxkpi's struct thread, made on first use
};

static pd_thread sThreads[PD_THREAD_SLOTS];
static spinlock sThreadLock;
static int32 sNextThreadID = 1;

static pd_thread *
thread_slot_for(void *thread, bool create)
{
	cpu_status state = disable_interrupts();
	acquire_spinlock(&sThreadLock);
	uintptr_t hash = ((uintptr_t)thread >> 4) % PD_THREAD_SLOTS;
	pd_thread *found = NULL;
	pd_thread *empty = NULL;
	for (int i = 0; i < PD_THREAD_SLOTS; i++) {
		pd_thread *t = &sThreads[(hash + i) % PD_THREAD_SLOTS];
		if (t->thread == thread) {
			found = t;
			break;
		}
		if (t->thread == NULL && empty == NULL)
			empty = t;
		if (t->thread == NULL && t->id == 0)
			break;
	}
	if (found == NULL && create && empty != NULL) {
		found = empty;
		memset(found, 0, sizeof(*found));
		found->thread = thread;
		found->id = sNextThreadID++;
		found->resumed = 1;
	}
	int used = 0;
	if (found == NULL && create) {
		for (int i = 0; i < PD_THREAD_SLOTS; i++)
			used += sThreads[i].thread != NULL;
	}
	release_spinlock(&sThreadLock);
	restore_interrupts(state);
	if (found == NULL && create)
		dprintf("pd: no thread slot for %p, %d of %d in use\n", thread, used, PD_THREAD_SLOTS);
	return found;
}

static pd_thread *
thread_slot_by_id(thread_id id)
{
	for (int i = 0; i < PD_THREAD_SLOTS; i++) {
		if (sThreads[i].id == id && sThreads[i].thread != NULL)
			return &sThreads[i];
	}
	return NULL;
}

thread_id
find_thread(const char *name)
{
	if (name != NULL) {
		for (int i = 0; i < PD_THREAD_SLOTS; i++) {
			if (sThreads[i].thread != NULL && strcmp(sThreads[i].name, name) == 0)
				return sThreads[i].id;
		}
		return B_NAME_NOT_FOUND;
	}
	pd_thread *t = thread_slot_for(pdos_thread_current(), true);
	return t != NULL ? t->id : B_ERROR;
}

struct Thread *
thread_get_current_thread(void)
{
	return (struct Thread *)thread_slot_for(pdos_thread_current(), true);
}

void
thread_pin_to_current_cpu(struct Thread *thread)
{
}

void
thread_unpin_from_current_cpu(struct Thread *thread)
{
}

// a spawned thread parks until resume_thread, as on haiku

struct pd_spawn {
	thread_func		func;
	void			*arg;
	int32			resumed;
	pd_thread		*slot;
	char			name[B_OS_NAME_LENGTH];
	int32			priority;
};

static void
spawn_trampoline(void *cookie)
{
	pd_spawn *spawn = (pd_spawn *)cookie;
	pd_thread *slot = thread_slot_for(pdos_thread_current(), true);
	if (slot == NULL)
		panic("pd: spawn of %s found no thread slot", spawn->name);
	strlcpy(slot->name, spawn->name, sizeof(slot->name));
	slot->priority = spawn->priority;
	int state = pdos_wait_lock();
	slot->resumed = 0;
	spawn->slot = slot;
	pdos_wakeup(&spawn->slot, 1);

	while (__atomic_load_n(&slot->resumed, __ATOMIC_ACQUIRE) == 0)
		pdos_wait_sleep(slot, 0, 0, &state);
	pdos_wait_unlock(state);

	thread_func func = spawn->func;
	void *arg = spawn->arg;
	free(spawn);
	status_t result = func(arg);
	thread_exit_with(result);
}

thread_id
spawn_kernel_thread(thread_func function, const char *name, int32 priority,
	void *arg)
{
	pd_spawn *spawn = (pd_spawn *)calloc(1, sizeof(pd_spawn));
	if (spawn == NULL)
		return B_NO_MEMORY;
	spawn->func = function;
	spawn->arg = arg;
	spawn->priority = priority;
	strlcpy(spawn->name, name != NULL ? name : "fbsd", sizeof(spawn->name));

	// the slot is created by the new thread, wait until it exists
	void *thread = pdos_thread_start(spawn_trampoline, spawn);
	if (thread == NULL) {
		free(spawn);
		return B_NO_MORE_THREADS;
	}
	int state = pdos_wait_lock();
	while (__atomic_load_n(&spawn->slot, __ATOMIC_ACQUIRE) == NULL)
		pdos_wait_sleep(&spawn->slot, 0, 0, &state);
	pd_thread *slot = spawn->slot;
	pdos_wait_unlock(state);
	return slot->id;
}

status_t
resume_thread(thread_id id)
{
	pd_thread *t = thread_slot_by_id(id);
	if (t == NULL)
		return B_BAD_THREAD_ID;
	int state = pdos_wait_lock();
	__atomic_store_n(&t->resumed, 1, __ATOMIC_RELEASE);
	pdos_wakeup(t, 1);
	pdos_wait_unlock(state);
	return B_OK;
}

// set by the linuxkpi runtime to release a thread's struct thread on exit
void (*pd_thread_fbsd_release)(void *fbsd_thread);

void **
pd_thread_fbsd_slot(const char **name, thread_id *id)
{
	pd_thread *t = thread_slot_for(pdos_thread_current(), true);
	if (t == NULL)
		return NULL;

	*name = t->name;
	*id = t->id;
	return &t->fbsd_thread;
}

void
thread_exit_with(status_t result)
{
	pd_thread *t = thread_slot_for(pdos_thread_current(), false);
	if (t != NULL && t->fbsd_thread != NULL && pd_thread_fbsd_release != NULL) {
		pd_thread_fbsd_release(t->fbsd_thread);
		t->fbsd_thread = NULL;
	}

	if (t != NULL) {
		int state = pdos_wait_lock();
		t->result = result;
		__atomic_store_n(&t->exited, 1, __ATOMIC_RELEASE);
		pdos_wakeup(&t->exited, 1);
		pdos_wait_unlock(state);
	}
	pdos_thread_exit();
}

void
thread_exit(void)
{
	thread_exit_with(B_OK);
}

void
exit_thread(status_t status)
{
	thread_exit_with(status);
}

status_t
wait_for_thread(thread_id id, status_t *result)
{
	pd_thread *t = thread_slot_by_id(id);
	if (t == NULL)
		return B_BAD_THREAD_ID;
	if (!t->resumed)
		resume_thread(id);
	int state = pdos_wait_lock();
	while (__atomic_load_n(&t->exited, __ATOMIC_ACQUIRE) == 0)
		pdos_wait_sleep(&t->exited, 0, 0, &state);
	pdos_wait_unlock(state);
	if (result != NULL)
		*result = t->result;
	// the slot stays reserved by its thread pointer until the kernel reuses it
	t->thread = NULL;
	return B_OK;
}

status_t
set_thread_priority(thread_id id, int32 priority)
{
	pd_thread *t = thread_slot_by_id(id);
	if (t == NULL)
		return B_BAD_THREAD_ID;
	int32 old = t->priority;
	t->priority = priority;
	return old;
}

// semaphores

#define PD_SEM_SLOTS	256

struct pd_sem {
	int32			used;
	int32			count;
	int32			deleted;
	int32			waiters;
	char			name[B_OS_NAME_LENGTH];
};

static pd_sem sSems[PD_SEM_SLOTS];

sem_id
create_sem(int32 count, const char *name)
{
	int state = pdos_wait_lock();
	for (int i = 0; i < PD_SEM_SLOTS; i++) {
		if (!sSems[i].used) {
			memset(&sSems[i], 0, sizeof(pd_sem));
			sSems[i].used = 1;
			sSems[i].count = count;
			strlcpy(sSems[i].name, name != NULL ? name : "sem",
				sizeof(sSems[i].name));
			pdos_wait_unlock(state);
			return i + 1;
		}
	}
	pdos_wait_unlock(state);
	return B_NO_MORE_SEMS;
}

static pd_sem *
sem_for(sem_id id)
{
	if (id <= 0 || id > PD_SEM_SLOTS || !sSems[id - 1].used)
		return NULL;
	return &sSems[id - 1];
}

status_t
delete_sem(sem_id id)
{
	int state = pdos_wait_lock();
	pd_sem *sem = sem_for(id);
	if (sem == NULL || sem->deleted) {
		pdos_wait_unlock(state);
		return B_BAD_SEM_ID;
	}
	sem->deleted = 1;
	pdos_wakeup(sem, 1);
	if (sem->waiters == 0)
		sem->used = 0;
	pdos_wait_unlock(state);
	return B_OK;
}

status_t
acquire_sem_etc(sem_id id, int32 count, uint32 flags, bigtime_t timeout)
{
	int state = pdos_wait_lock();
	pd_sem *sem = sem_for(id);
	if (sem == NULL || sem->deleted) {
		pdos_wait_unlock(state);
		return B_BAD_SEM_ID;
	}

	int64 deadline = 0;
	if ((flags & B_RELATIVE_TIMEOUT) != 0) {
		if (timeout <= 0 && sem->count < count) {
			pdos_wait_unlock(state);
			return timeout == 0 ? B_WOULD_BLOCK : B_TIMED_OUT;
		}
		deadline = system_time() + timeout;
	} else if ((flags & B_ABSOLUTE_TIMEOUT) != 0)
		deadline = timeout;
	if (deadline == B_INFINITE_TIMEOUT)
		deadline = 0;

	status_t status = B_OK;
	sem->waiters++;
	while (sem->count < count) {
		if (sem->deleted) {
			status = B_BAD_SEM_ID;
			break;
		}
		int64 left = 0;
		if (deadline != 0) {
			left = deadline - system_time();
			if (left <= 0) {
				status = B_TIMED_OUT;
				break;
			}
		}
		int r = pdos_wait_sleep(sem, left, (flags & B_CAN_INTERRUPT) != 0,
			&state);
		if (r == PDOS_WAIT_INTR) {
			status = B_INTERRUPTED;
			break;
		}
	}
	if (status == B_OK && sem->deleted)
		status = B_BAD_SEM_ID;
	if (status == B_OK)
		sem->count -= count;
	sem->waiters--;
	if (sem->deleted && sem->waiters == 0)
		sem->used = 0;
	pdos_wait_unlock(state);
	return status;
}

status_t
acquire_sem(sem_id id)
{
	return acquire_sem_etc(id, 1, 0, 0);
}

status_t
release_sem_etc(sem_id id, int32 count, uint32 flags)
{
	int state = pdos_wait_lock();
	pd_sem *sem = sem_for(id);
	if (sem == NULL || sem->deleted) {
		pdos_wait_unlock(state);
		return B_BAD_SEM_ID;
	}
	if ((flags & B_RELEASE_ALL) != 0) {
		if (sem->count < 0)
			sem->count = 0;
		sem->count += sem->waiters > count ? sem->waiters : count;
	} else
		sem->count += count;
	pdos_wakeup(sem, 1);
	pdos_wait_unlock(state);
	return B_OK;
}

status_t
release_sem(sem_id id)
{
	return release_sem_etc(id, 1, 0);
}

status_t
get_sem_count(sem_id id, int32 *count)
{
	pd_sem *sem = sem_for(id);
	if (sem == NULL)
		return B_BAD_SEM_ID;
	*count = sem->count;
	return B_OK;
}

// mutexes, recursive locks and rw locks

static void *
mutex_impl(mutex *lock)
{
	void *impl = __atomic_load_n(&lock->impl, __ATOMIC_ACQUIRE);
	if (impl != NULL)
		return impl;
	void *fresh = pdos_mtx_alloc(lock->name);
	void *expected = NULL;
	if (!__atomic_compare_exchange_n(&lock->impl, &expected, fresh, 0,
			__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
		pdos_mtx_free(fresh);
		return expected;
	}
	return fresh;
}

void
mutex_init(mutex *lock, const char *name)
{
	lock->name = name;
	lock->impl = NULL;
	lock->holder = -1;
	lock->flags = 0;
}

void
mutex_init_etc(mutex *lock, const char *name, uint32 flags)
{
	mutex_init(lock, name);
}

void
mutex_destroy(mutex *lock)
{
	if (lock->impl != NULL)
		pdos_mtx_free(lock->impl);
	lock->impl = NULL;
}

status_t
mutex_lock(mutex *lock)
{
	pdos_mtx_lock(mutex_impl(lock));
	lock->holder = find_thread(NULL);
	return B_OK;
}

status_t
mutex_trylock(mutex *lock)
{
	if (!pdos_mtx_trylock(mutex_impl(lock)))
		return B_WOULD_BLOCK;
	lock->holder = find_thread(NULL);
	return B_OK;
}

void
mutex_unlock(mutex *lock)
{
	lock->holder = -1;
	pdos_mtx_unlock(mutex_impl(lock));
}

status_t
mutex_switch_lock(mutex *from, mutex *to)
{
	mutex_unlock(from);
	return mutex_lock(to);
}

void
recursive_lock_init(recursive_lock *lock, const char *name)
{
	mutex_init(&lock->lock, name);
	lock->holder = -1;
	lock->recursion = 0;
}

void
recursive_lock_init_etc(recursive_lock *lock, const char *name, uint32 flags)
{
	recursive_lock_init(lock, name);
}

void
recursive_lock_destroy(recursive_lock *lock)
{
	mutex_destroy(&lock->lock);
}

status_t
recursive_lock_lock(recursive_lock *lock)
{
	thread_id self = find_thread(NULL);
	if (lock->holder != self) {
		mutex_lock(&lock->lock);
		lock->holder = self;
	}
	lock->recursion++;
	return B_OK;
}

status_t
recursive_lock_trylock(recursive_lock *lock)
{
	thread_id self = find_thread(NULL);
	if (lock->holder != self) {
		if (mutex_trylock(&lock->lock) != B_OK)
			return B_WOULD_BLOCK;
		lock->holder = self;
	}
	lock->recursion++;
	return B_OK;
}

void
recursive_lock_unlock(recursive_lock *lock)
{
	if (--lock->recursion == 0) {
		lock->holder = -1;
		mutex_unlock(&lock->lock);
	}
}

int32
recursive_lock_get_recursion(recursive_lock *lock)
{
	if (lock->holder == find_thread(NULL))
		return lock->recursion;
	return -1;
}

static void *
rw_impl(rw_lock *lock)
{
	void *impl = __atomic_load_n(&lock->impl, __ATOMIC_ACQUIRE);
	if (impl != NULL)
		return impl;
	void *fresh = pdos_rw_alloc();
	void *expected = NULL;
	if (!__atomic_compare_exchange_n(&lock->impl, &expected, fresh, 0,
			__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
		pdos_rw_free(fresh);
		return expected;
	}
	return fresh;
}

void
rw_lock_init(rw_lock *lock, const char *name)
{
	lock->name = name;
	lock->impl = NULL;
	lock->holder = -1;
	lock->owner_count = 0;
}

void
rw_lock_init_etc(rw_lock *lock, const char *name, uint32 flags)
{
	rw_lock_init(lock, name);
}

void
rw_lock_destroy(rw_lock *lock)
{
	if (lock->impl != NULL)
		pdos_rw_free(lock->impl);
	lock->impl = NULL;
}

status_t
rw_lock_read_lock(rw_lock *lock)
{
	pdos_rw_rlock(rw_impl(lock));
	return B_OK;
}

status_t
rw_lock_read_unlock(rw_lock *lock)
{
	pdos_rw_runlock(rw_impl(lock));
	return B_OK;
}

status_t
rw_lock_write_lock(rw_lock *lock)
{
	pdos_rw_wlock(rw_impl(lock));
	lock->holder = find_thread(NULL);
	return B_OK;
}

status_t
rw_lock_write_unlock(rw_lock *lock)
{
	lock->holder = -1;
	pdos_rw_wunlock(rw_impl(lock));
	return B_OK;
}

bool
rw_lock_try_read_lock(rw_lock *lock)
{
	return pdos_rw_try_rlock(rw_impl(lock)) != 0;
}

bool
rw_lock_try_write_lock(rw_lock *lock)
{
	if (!pdos_rw_try_wlock(rw_impl(lock)))
		return false;

	lock->holder = find_thread(NULL);
	return true;
}

void
rw_lock_write_downgrade(rw_lock *lock)
{
	lock->holder = -1;
	pdos_rw_downgrade(rw_impl(lock));
}

// heap, every block keeps its raw allocation just before it

static void *
heap_alloc(size_t size, size_t align, bool zero, bool canWait)
{
	if (align < sizeof(void *))
		align = sizeof(void *);
	void *raw = pdos_malloc(size + align + sizeof(void *), zero, canWait);
	if (raw == NULL)
		return NULL;
	uintptr_t p = ((uintptr_t)raw + sizeof(void *) + align - 1) & ~(uintptr_t)(align - 1);
	((void **)p)[-1] = raw;
	return (void *)p;
}

void *
malloc(size_t size)
{
	return heap_alloc(size, 16, false, true);
}

void *
calloc(size_t count, size_t size)
{
	return heap_alloc(count * size, 16, true, true);
}

void
free(void *ptr)
{
	if (ptr != NULL)
		pdos_free(((void **)ptr)[-1]);
}

size_t malloc_usable_size(void *ptr);

void *
realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return malloc(size);
	void *fresh = malloc(size);
	if (fresh == NULL)
		return NULL;
	// the old size is not tracked here, the raw block is at least as big
	size_t old = malloc_usable_size(ptr);
	memcpy(fresh, ptr, old < size ? old : size);
	free(ptr);
	return fresh;
}

size_t
malloc_usable_size(void *ptr)
{
	if (ptr == NULL)
		return 0;

	void *raw = ((void **)ptr)[-1];
	return pdos_malloc_size(raw) - ((uintptr_t)ptr - (uintptr_t)raw);
}

void *
memalign_etc(size_t alignment, size_t size, uint32 flags)
{
	return heap_alloc(size, alignment, false,
		(flags & HEAP_DONT_WAIT_FOR_MEMORY) == 0);
}

void *
malloc_etc(size_t size, uint32 flags)
{
	return heap_alloc(size, 16, false, (flags & HEAP_DONT_WAIT_FOR_MEMORY) == 0);
}

void
free_etc(void *address, uint32 flags)
{
	free(address);
}

char *
strdup(const char *string)
{
	size_t len = strlen(string) + 1;
	char *copy = (char *)malloc(len);
	if (copy != NULL)
		memcpy(copy, string, len);
	return copy;
}

// areas, used for contiguous memory and for device memory mappings

#define PD_AREA_SLOTS	4096

struct pd_area {
	area_id		id;
	void		*address;
	size_t		size;
	int			kind;
};

enum { AREA_FREE = 0, AREA_CONTIG, AREA_PHYS };

static pd_area sAreas[PD_AREA_SLOTS];
static spinlock sAreaLock;
static int32 sNextArea = 1;

static area_id
area_add(void *address, size_t size, int kind)
{
	cpu_status state = disable_interrupts();
	acquire_spinlock(&sAreaLock);
	area_id id = B_NO_MEMORY;
	for (int i = 0; i < PD_AREA_SLOTS; i++) {
		if (sAreas[i].kind == AREA_FREE) {
			sAreas[i].id = id = sNextArea++;
			sAreas[i].address = address;
			sAreas[i].size = size;
			sAreas[i].kind = kind;
			break;
		}
	}
	release_spinlock(&sAreaLock);
	restore_interrupts(state);
	return id;
}

area_id
create_area_etc(team_id team, const char *name, size_t size, uint32 lock,
	uint32 protection, uint32 flags, uint32 guardSize,
	const virtual_address_restrictions *virtualRestrictions,
	const physical_address_restrictions *physicalRestrictions, void **_address)
{
	size_t align = B_PAGE_SIZE;
	uint64 high = 0;
	if (physicalRestrictions != NULL) {
		if (physicalRestrictions->alignment > align)
			align = physicalRestrictions->alignment;
		high = physicalRestrictions->high_address;
	}
	uint64 phys = 0;
	void *address = pdos_malloc_contig(size, align, high, &phys);
	if (address == NULL)
		return B_NO_MEMORY;
	if ((flags & CREATE_AREA_DONT_CLEAR) == 0)
		memset(address, 0, size);
	area_id id = area_add(address, size, AREA_CONTIG);
	if (id < 0) {
		pdos_free_contig(address, size);
		return id;
	}
	*_address = address;
	return id;
}

area_id
create_area(const char *name, void **startAddress, uint32 addressSpec,
	size_t size, uint32 lock, uint32 protection)
{
	return create_area_etc(B_SYSTEM_TEAM, name, size, lock, protection, 0, 0,
		NULL, NULL, startAddress);
}

area_id
map_physical_memory(const char *areaName, phys_addr_t physicalAddress,
	size_t size, uint32 flags, uint32 protection, void **_mappedAddress)
{
	uint64 base = physicalAddress & ~(uint64)(B_PAGE_SIZE - 1);
	size_t offset = physicalAddress - base;
	size_t mapSize = (offset + size + B_PAGE_SIZE - 1) & ~(size_t)(B_PAGE_SIZE - 1);
	void *va = pdos_map_phys(base, mapSize, 1);
	if (va == NULL)
		return B_ERROR;
	area_id id = area_add(va, mapSize, AREA_PHYS);
	if (id < 0) {
		pdos_unmap_phys(va, mapSize);
		return id;
	}
	*_mappedAddress = (uint8 *)va + offset;
	return id;
}

area_id
area_for(void *address)
{
	for (int i = 0; i < PD_AREA_SLOTS; i++) {
		if (sAreas[i].kind != AREA_FREE && (uint8 *)address >= (uint8 *)sAreas[i].address
				&& (uint8 *)address < (uint8 *)sAreas[i].address + sAreas[i].size)
			return sAreas[i].id;
	}
	return B_ERROR;
}

status_t
_get_area_info(area_id id, area_info *info, size_t size)
{
	for (int i = 0; i < PD_AREA_SLOTS; i++) {
		if (sAreas[i].kind != AREA_FREE && sAreas[i].id == id) {
			memset(info, 0, size);
			info->area = id;
			info->address = sAreas[i].address;
			info->size = sAreas[i].size;
			info->ram_size = sAreas[i].size;
			return B_OK;
		}
	}
	return B_BAD_VALUE;
}

status_t
delete_area(area_id id)
{
	for (int i = 0; i < PD_AREA_SLOTS; i++) {
		if (sAreas[i].kind != AREA_FREE && sAreas[i].id == id) {
			if (sAreas[i].kind == AREA_CONTIG)
				pdos_free_contig(sAreas[i].address, sAreas[i].size);
			else
				pdos_unmap_phys(sAreas[i].address, sAreas[i].size);
			sAreas[i].kind = AREA_FREE;
			return B_OK;
		}
	}
	return B_BAD_VALUE;
}

int32
get_memory_map(const void *buffer, size_t size, physical_entry *table,
	int32 numEntries)
{
	uintptr_t va = (uintptr_t)buffer;
	int32 count = 0;
	while (size > 0 && count < numEntries) {
		size_t inPage = B_PAGE_SIZE - (va & (B_PAGE_SIZE - 1));
		if (inPage > size)
			inPage = size;
		uint64 phys = pdos_vtophys((const void *)va);
		if (count > 0 && table[count - 1].address + table[count - 1].size == phys)
			table[count - 1].size += inPage;
		else {
			table[count].address = phys;
			table[count].size = inPage;
			count++;
		}
		va += inPage;
		size -= inPage;
	}
	if (count < numEntries)
		table[count].size = 0;
	return B_OK;
}

status_t
get_memory_map_etc(team_id team, const void *address, size_t numBytes,
	physical_entry *table, uint32 *_numEntries)
{
	get_memory_map(address, numBytes, table, *_numEntries);
	uint32 n = 0;
	while (n < *_numEntries && table[n].size != 0)
		n++;
	*_numEntries = n;
	return B_OK;
}

status_t
user_memcpy(void *to, const void *from, size_t size)
{
	memcpy(to, from, size);
	return B_OK;
}

// object caches, objects up to a page are aligned on their size so they never
// cross a page and stay physically contiguous for dma

struct object_cache {
	size_t		size;
	size_t		align;
	int32		count;
};

object_cache *
create_object_cache(const char *name, size_t objectSize, uint32 flags)
{
	object_cache *cache = (object_cache *)calloc(1, sizeof(object_cache));
	if (cache == NULL)
		return NULL;
	size_t align = 16;
	while (align < objectSize && align < B_PAGE_SIZE)
		align <<= 1;
	cache->size = objectSize;
	cache->align = align;
	return cache;
}

object_cache *
create_object_cache_etc(const char *name, size_t objectSize, size_t alignment,
	size_t maxByteUsage, size_t magazineCapacity, size_t maxMagazineCount,
	uint32 flags, void *cookie, object_cache_constructor constructor,
	object_cache_destructor destructor, object_cache_reclaimer reclaimer)
{
	return create_object_cache(name, objectSize, flags);
}

void
delete_object_cache(object_cache *cache)
{
	free(cache);
}

void *
object_cache_alloc(object_cache *cache, uint32 flags)
{
	if (cache->size > B_PAGE_SIZE) {
		uint64 phys;
		return pdos_malloc_contig(cache->size, B_PAGE_SIZE, 0, &phys);
	}
	return heap_alloc(cache->size, cache->align, false,
		(flags & CACHE_DONT_WAIT_FOR_MEMORY) == 0);
}

void
object_cache_free(object_cache *cache, void *object, uint32 flags)
{
	if (object == NULL)
		return;
	if (cache->size > B_PAGE_SIZE)
		pdos_free_contig(object, cache->size);
	else
		free(object);
}

status_t
object_cache_reserve(object_cache *cache, size_t object_count, uint32 flags)
{
	return B_OK;
}

// modules, only the pci bus manager and an empty network buffer module

static struct net_buffer_module_info sNetBufferModule;

status_t
get_module(const char *path, module_info **_info)
{
	if (strcmp(path, B_PCI_MODULE_NAME) == 0) {
		*_info = (module_info *)pd_pci_module();
		return B_OK;
	}
	if (strcmp(path, NET_BUFFER_MODULE_NAME) == 0) {
		*_info = (module_info *)&sNetBufferModule;
		return B_OK;
	}
	return B_ENTRY_NOT_FOUND;
}

status_t
put_module(const char *path)
{
	return B_OK;
}

}	// extern "C"


// condition variables, on the seam's global wait lock

static ConditionVariable *sPublished;

void
ConditionVariable::Init(const void *object, const char *objectType)
{
	fObject = object;
	fObjectType = objectType;
	fEntries = NULL;
	fNextPublished = NULL;
}

void
ConditionVariable::Publish(const void *object, const char *objectType)
{
	Init(object, objectType);
	int state = pdos_wait_lock();
	fNextPublished = sPublished;
	sPublished = this;
	pdos_wait_unlock(state);
}

void
ConditionVariable::Unpublish()
{
	int state = pdos_wait_lock();
	ConditionVariable **link = &sPublished;
	while (*link != NULL) {
		if (*link == this) {
			*link = fNextPublished;
			break;
		}
		link = &(*link)->fNextPublished;
	}
	fNextPublished = NULL;
	pdos_wait_unlock(state);
	NotifyAll(B_ENTRY_NOT_FOUND);
}

void
ConditionVariable::Add(ConditionVariableEntry *entry)
{
	int state = pdos_wait_lock();
	entry->fVariable = this;
	entry->fWaitStatus = 1;
	entry->fWaiting = true;
	entry->fNext = fEntries;
	fEntries = entry;
	pdos_wait_unlock(state);
}

// the wait lock is held by the callers of these two

static void
notify_locked(ConditionVariable *variable, bool all, status_t result)
{
	while (variable->fEntries != NULL) {
		ConditionVariableEntry *entry = variable->fEntries;
		variable->fEntries = entry->fNext;
		entry->fNext = NULL;
		entry->fVariable = NULL;
		entry->fWaitStatus = result;
		entry->fWaiting = false;
		pdos_wakeup(entry, 1);
		if (!all)
			break;
	}
}

static void
remove_entry_locked(ConditionVariable *variable, ConditionVariableEntry *entry)
{
	ConditionVariableEntry **link = &variable->fEntries;
	while (*link != NULL) {
		if (*link == entry) {
			*link = entry->fNext;
			break;
		}
		link = &(*link)->fNext;
	}
	entry->fNext = NULL;
	entry->fVariable = NULL;
}

void
ConditionVariable::NotifyOne(status_t result)
{
	int state = pdos_wait_lock();
	notify_locked(this, false, result);
	pdos_wait_unlock(state);
}

void
ConditionVariable::NotifyAll(status_t result)
{
	int state = pdos_wait_lock();
	notify_locked(this, true, result);
	pdos_wait_unlock(state);
}

void
ConditionVariable::NotifyOne(const void *object, status_t result)
{
	int state = pdos_wait_lock();
	for (ConditionVariable *v = sPublished; v != NULL; v = v->fNextPublished) {
		if (v->fObject == object) {
			notify_locked(v, false, result);
			break;
		}
	}
	pdos_wait_unlock(state);
}

void
ConditionVariable::NotifyAll(const void *object, status_t result)
{
	int state = pdos_wait_lock();
	for (ConditionVariable *v = sPublished; v != NULL; v = v->fNextPublished) {
		if (v->fObject == object)
			notify_locked(v, true, result);
	}
	pdos_wait_unlock(state);
}

status_t
ConditionVariableEntry::Wait(uint32 flags, bigtime_t timeout)
{
	int state = pdos_wait_lock();
	int64 deadline = 0;
	if ((flags & B_RELATIVE_TIMEOUT) != 0)
		deadline = system_time() + (timeout > 0 ? timeout : 0);
	else if ((flags & B_ABSOLUTE_TIMEOUT) != 0)
		deadline = timeout;
	if (deadline == B_INFINITE_TIMEOUT)
		deadline = 0;

	status_t status = B_OK;
	while (fWaiting) {
		int64 left = 0;
		if (deadline != 0) {
			left = deadline - system_time();
			if (left <= 0) {
				status = B_TIMED_OUT;
				break;
			}
		}
		int r = pdos_wait_sleep(this, left, (flags & B_CAN_INTERRUPT) != 0, &state);
		if (r == PDOS_WAIT_INTR && fWaiting) {
			status = B_INTERRUPTED;
			break;
		}
	}
	if (fWaiting) {
		if (fVariable != NULL)
			remove_entry_locked(fVariable, this);
		fWaiting = false;
	} else
		status = fWaitStatus;
	pdos_wait_unlock(state);
	return status;
}

ConditionVariableEntry::~ConditionVariableEntry()
{
	if (fWaiting) {
		int state = pdos_wait_lock();
		if (fVariable != NULL)
			remove_entry_locked(fVariable, this);
		fWaiting = false;
		pdos_wait_unlock(state);
	}
}

status_t
ConditionVariable::Wait(uint32 flags, bigtime_t timeout)
{
	ConditionVariableEntry entry;
	Add(&entry);
	return entry.Wait(flags, timeout);
}

status_t
ConditionVariable::Wait(mutex *lock, uint32 flags, bigtime_t timeout)
{
	ConditionVariableEntry entry;
	Add(&entry);
	mutex_unlock(lock);
	status_t status = entry.Wait(flags, timeout);
	mutex_lock(lock);
	return status;
}

status_t
ConditionVariable::Wait(recursive_lock *lock, uint32 flags, bigtime_t timeout)
{
	ConditionVariableEntry entry;
	Add(&entry);
	int32 recursion = lock->recursion;
	lock->recursion = 1;
	recursive_lock_unlock(lock);
	status_t status = entry.Wait(flags, timeout);
	recursive_lock_lock(lock);
	lock->recursion = recursion;
	return status;
}
