#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <riscv/proc_reg.h>

// 16 byte atomics for the compiler's libcalls, rv64 has no 128 bit cas without zacas
// every object hashes to one of these locks, held with interrupts off so a handler never spins on its own hart

#define ATOMIC16_LOCK_COUNT     64
#define ATOMIC16_LOCK_SHIFT     4

typedef unsigned __int128 atomic16_t;

// one lock per cache line so neighbouring locks do not bounce together
static struct {
	uint32_t        lock;
	uint32_t        pad[15];
} atomic16_locks[ATOMIC16_LOCK_COUNT] __attribute__((aligned(64)));

static inline uint32_t *
atomic16_lock_for(const volatile void *ptr)
{
	uintptr_t a = (uintptr_t)ptr >> ATOMIC16_LOCK_SHIFT;

	a ^= a >> 7;
	a ^= a >> 13;
	return &atomic16_locks[a % ATOMIC16_LOCK_COUNT].lock;
}

static inline uint64_t
atomic16_lock(const volatile void *ptr)
{
	uint32_t *lock = atomic16_lock_for(ptr);
	uint64_t istate = csr_read_clear(sstatus, SSTATUS_SIE);

	while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) != 0) {
		while (__atomic_load_n(lock, __ATOMIC_RELAXED) != 0) {
			__asm__ volatile (".insn i 0x0f, 0, x0, x0, 0x010");
		}
	}
	// the lock orders against the other holders, the fence against everything else
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	return istate;
}

static inline void
atomic16_unlock(const volatile void *ptr, uint64_t istate)
{
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	__atomic_store_n(atomic16_lock_for(ptr), 0, __ATOMIC_RELEASE);
	csr_set(sstatus, istate & SSTATUS_SIE);
}

atomic16_t __atomic_load_16(const volatile void *ptr, int memorder);
void __atomic_store_16(volatile void *ptr, atomic16_t val, int memorder);
atomic16_t __atomic_exchange_16(volatile void *ptr, atomic16_t val, int memorder);
bool __atomic_compare_exchange_16(volatile void *ptr, void *expected, atomic16_t desired,
    int success, int failure);
atomic16_t __atomic_fetch_add_16(volatile void *ptr, atomic16_t val, int memorder);
atomic16_t __atomic_fetch_sub_16(volatile void *ptr, atomic16_t val, int memorder);

atomic16_t
__atomic_load_16(const volatile void *ptr, __unused int memorder)
{
	uint64_t istate = atomic16_lock(ptr);
	atomic16_t v = *(const volatile atomic16_t *)ptr;

	atomic16_unlock(ptr, istate);
	return v;
}

void
__atomic_store_16(volatile void *ptr, atomic16_t val, __unused int memorder)
{
	uint64_t istate = atomic16_lock(ptr);

	*(volatile atomic16_t *)ptr = val;
	atomic16_unlock(ptr, istate);
}

atomic16_t
__atomic_exchange_16(volatile void *ptr, atomic16_t val, __unused int memorder)
{
	uint64_t istate = atomic16_lock(ptr);
	atomic16_t old = *(volatile atomic16_t *)ptr;

	*(volatile atomic16_t *)ptr = val;
	atomic16_unlock(ptr, istate);
	return old;
}

// on failure the current value goes back through expected
bool
__atomic_compare_exchange_16(volatile void *ptr, void *expected, atomic16_t desired,
    __unused int success, __unused int failure)
{
	uint64_t istate = atomic16_lock(ptr);
	atomic16_t cur = *(volatile atomic16_t *)ptr;
	atomic16_t exp;
	bool ok;

	memcpy(&exp, expected, sizeof(exp));
	ok = (cur == exp);
	if (ok) {
		*(volatile atomic16_t *)ptr = desired;
	}
	atomic16_unlock(ptr, istate);

	if (!ok) {
		memcpy(expected, &cur, sizeof(cur));
	}
	return ok;
}

atomic16_t
__atomic_fetch_add_16(volatile void *ptr, atomic16_t val, __unused int memorder)
{
	uint64_t istate = atomic16_lock(ptr);
	atomic16_t old = *(volatile atomic16_t *)ptr;

	*(volatile atomic16_t *)ptr = old + val;
	atomic16_unlock(ptr, istate);
	return old;
}

atomic16_t
__atomic_fetch_sub_16(volatile void *ptr, atomic16_t val, __unused int memorder)
{
	uint64_t istate = atomic16_lock(ptr);
	atomic16_t old = *(volatile atomic16_t *)ptr;

	*(volatile atomic16_t *)ptr = old - val;
	atomic16_unlock(ptr, istate);
	return old;
}
