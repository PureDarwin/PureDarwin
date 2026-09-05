/*
 * Futex-style wait/wake on an address, introduced in macOS 14.4. PureDarwin
 * reports 26.5, so ports that gate on MAC_OS_VERSION_14_4 expect this to be
 * here. Implemented over the __ulock_* traps, which is what it wraps.
 */

#ifndef __OS_SYNC_WAIT_ON_ADDRESS__
#define __OS_SYNC_WAIT_ON_ADDRESS__

#include <sys/cdefs.h>
#include <stdint.h>
#include <stddef.h>
#include <Availability.h>
#include <os/base.h>
#include <os/clock.h>

__BEGIN_DECLS

OS_ENUM(os_sync_wait_on_address_flags, uint32_t,
    OS_SYNC_WAIT_ON_ADDRESS_NONE   = 0x00000000,
    OS_SYNC_WAIT_ON_ADDRESS_SHARED = 0x00000001,
    );

OS_ENUM(os_sync_wake_by_address_flags, uint32_t,
    OS_SYNC_WAKE_BY_ADDRESS_NONE   = 0x00000000,
    OS_SYNC_WAKE_BY_ADDRESS_SHARED = 0x00000001,
    );

/*
 * All five return the number of waiters remaining, or -1 with errno set.
 * size must be 4 or 8, and addr must be aligned to it.
 */
__API_AVAILABLE(macos(14.4))
int os_sync_wait_on_address(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags);

__API_AVAILABLE(macos(14.4))
int os_sync_wait_on_address_with_deadline(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags, os_clockid_t clockid, uint64_t deadline);

__API_AVAILABLE(macos(14.4))
int os_sync_wait_on_address_with_timeout(void *addr, uint64_t value, size_t size,
    os_sync_wait_on_address_flags_t flags, os_clockid_t clockid,
    uint64_t timeout_ns);

__API_AVAILABLE(macos(14.4))
int os_sync_wake_by_address_any(void *addr, size_t size,
    os_sync_wake_by_address_flags_t flags);

__API_AVAILABLE(macos(14.4))
int os_sync_wake_by_address_all(void *addr, size_t size,
    os_sync_wake_by_address_flags_t flags);

__END_DECLS

#endif /* __OS_SYNC_WAIT_ON_ADDRESS__ */
