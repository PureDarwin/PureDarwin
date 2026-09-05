/*
 * os_sync_wait_on_address family, over the __ulock_* traps it wraps on Darwin.
 */

#include <os/os_sync_wait_on_address.h>

#include <errno.h>
#include <stdint.h>
#include <sys/ulock.h>

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
    uint32_t timeout);
extern int __ulock_wait2(uint32_t operation, void *addr, uint64_t value,
    uint64_t timeout, uint64_t value2);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

/*
 * The trap takes the width in the operation code rather than as an argument.
 * Anything but a naturally aligned 4 or 8 bytes is rejected here; the kernel
 * would fault on it instead of returning an error.
 */
static int
pd_ulock_op(size_t size, uint32_t shared, uint32_t *op)
{
	if (size == 4) {
		*op = shared ? UL_COMPARE_AND_WAIT_SHARED : UL_COMPARE_AND_WAIT;
	} else if (size == 8) {
		*op = shared ? UL_COMPARE_AND_WAIT64_SHARED : UL_COMPARE_AND_WAIT64;
	} else {
		return -1;
	}
	return 0;
}

static int
pd_check_addr(void *addr, size_t size)
{
	if (addr == NULL || ((uintptr_t)addr & (size - 1)) != 0) {
		return -1;
	}
	return 0;
}

int
os_sync_wait_on_address(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags)
{
	return os_sync_wait_on_address_with_timeout(addr, value, size, flags,
	           OS_CLOCK_MACH_ABSOLUTE_TIME, 0);
}

int
os_sync_wait_on_address_with_timeout(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags, os_clockid_t clockid,
    uint64_t timeout_ns)
{
	uint32_t op;

	if (clockid != OS_CLOCK_MACH_ABSOLUTE_TIME ||
	    (flags & ~OS_SYNC_WAIT_ON_ADDRESS_SHARED) != 0 ||
	    pd_check_addr(addr, size) != 0 ||
	    pd_ulock_op(size, flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED, &op) != 0) {
		errno = EINVAL;
		return -1;
	}
	/* A zero timeout means wait forever, which is what wait2 takes too. */
	return __ulock_wait2(op, addr, value, timeout_ns, 0);
}

int
os_sync_wait_on_address_with_deadline(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags, os_clockid_t clockid,
    uint64_t deadline)
{
	uint32_t op;

	if (clockid != OS_CLOCK_MACH_ABSOLUTE_TIME ||
	    (flags & ~OS_SYNC_WAIT_ON_ADDRESS_SHARED) != 0 ||
	    pd_check_addr(addr, size) != 0 ||
	    pd_ulock_op(size, flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED, &op) != 0) {
		errno = EINVAL;
		return -1;
	}
	if (deadline == 0) {
		errno = EINVAL;
		return -1;
	}
	/* ULF_DEADLINE reinterprets the timeout argument as an absolute
	 * mach_absolute_time deadline, which is what this entry point takes. */
	return __ulock_wait2(op | ULF_DEADLINE, addr, value, deadline, 0);
}

static int
pd_wake(void *addr, size_t size, os_sync_wake_by_address_flags_t flags,
    uint32_t extra)
{
	uint32_t op;

	if ((flags & ~OS_SYNC_WAKE_BY_ADDRESS_SHARED) != 0 ||
	    pd_check_addr(addr, size) != 0 ||
	    pd_ulock_op(size, flags & OS_SYNC_WAKE_BY_ADDRESS_SHARED, &op) != 0) {
		errno = EINVAL;
		return -1;
	}
	return __ulock_wake(op | extra, addr, 0);
}

int
os_sync_wake_by_address_any(void *addr, size_t size,
    os_sync_wake_by_address_flags_t flags)
{
	return pd_wake(addr, size, flags, 0);
}

int
os_sync_wake_by_address_all(void *addr, size_t size,
    os_sync_wake_by_address_flags_t flags)
{
	return pd_wake(addr, size, flags, ULF_WAKE_ALL);
}
