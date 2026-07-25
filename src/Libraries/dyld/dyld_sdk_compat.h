/*
 * dyld_sdk_compat.h  (force-included into the dyld target)
 *
 * dyld-832 (macOS 11.4) uses bridgeos(x) in __API_AVAILABLE/__API_UNAVAILABLE/
 * __API_DEPRECATED availability lists and tests TARGET_OS_BRIDGE. Neither the
 * MacOSX11.3 SDK nor PureDarwin's bundled AvailabilityInternal.h define the
 * bridgeos platform-macro families, so those expansions leave an undefined
 * __API_*_PLATFORM_bridgeos token and clang errors "expected ','".
 *
 * Supply the missing bridgeos macros (clang recognizes the "bridgeos"
 * availability platform) and TARGET_OS_BRIDGE. Scoped to the dyld target so it
 * does not perturb the rest of the PureDarwin build.
 */
#ifndef PUREDARWIN_DYLD_SDK_COMPAT_H
#define PUREDARWIN_DYLD_SDK_COMPAT_H

#include <Availability.h>
#include <TargetConditionals.h>
#include <string.h>
#include <libkern/OSAtomic.h>
#if __has_include(<ptrauth.h>)
 #include <ptrauth.h>
#endif
/* Defines SPI_AVAILABLE / SPI_DEPRECATED (empty fallbacks). dyld_process_info.h
 * uses SPI_AVAILABLE but only pulls it transitively via the real dispatch.h,
 * which our minimal dispatch stub replaces -- so pull it explicitly here. */
#include <os/availability.h>

__BEGIN_DECLS
int32_t OSAtomicIncrement32(volatile int32_t *value);
int32_t OSAtomicDecrement32(volatile int32_t *value);
bool OSAtomicCompareAndSwapPtrBarrier(void *oldValue, void *newValue,
    void * volatile *value);
void OSMemoryBarrier(void);
void OSSpinLockLock(volatile OSSpinLock *lock);
void OSSpinLockUnlock(volatile OSSpinLock *lock);
__END_DECLS

#ifndef __API_AVAILABLE_PLATFORM_bridgeos
 #define __API_AVAILABLE_PLATFORM_bridgeos(x) bridgeos,introduced=x
#endif
#ifndef __API_DEPRECATED_PLATFORM_bridgeos
 #define __API_DEPRECATED_PLATFORM_bridgeos(x,y) bridgeos,introduced=x,deprecated=y
#endif
#ifndef __API_UNAVAILABLE_PLATFORM_bridgeos
 #define __API_UNAVAILABLE_PLATFORM_bridgeos bridgeos,unavailable
#endif

#ifndef TARGET_OS_BRIDGE
 #define TARGET_OS_BRIDGE 0
#endif

/* PureDarwin's exported <System/sys/reason.h> strips the kernel-private section
 * that defines these (they survive only in comments). dyld uses them in
 * userspace; supply the stable XNU ABI values (from xnu osfmk/kern/kcdata.h). */
#ifndef EXIT_REASON_USER_DESC_MAX_LEN
 #define EXIT_REASON_USER_DESC_MAX_LEN  1024
#endif
#ifndef EXIT_REASON_PAYLOAD_MAX_LEN
 #define EXIT_REASON_PAYLOAD_MAX_LEN    2048
#endif

#if !__has_feature(ptrauth_intrinsics) && !defined(__PTRAUTH__)
 #undef ptrauth_auth_and_resign
 #define ptrauth_auth_and_resign(value, old_key, old_data, new_key, new_data) (value)
#endif

#endif /* PUREDARWIN_DYLD_SDK_COMPAT_H */
