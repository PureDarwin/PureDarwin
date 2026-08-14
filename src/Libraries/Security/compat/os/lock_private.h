/*
 * <os/lock_private.h> ships in no SDK. Security uses only the hinted variant of
 * os_unfair_lock_lock; the hints are scheduler advice, so dropping them is
 * behaviour-preserving.
 */
#ifndef PD_OS_LOCK_PRIVATE_H
#define PD_OS_LOCK_PRIVATE_H

#include <os/lock.h>

typedef uint32_t os_unfair_lock_options_t;

#define OS_UNFAIR_LOCK_NONE                 0x00000000u
#define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION 0x00010000u
#define OS_UNFAIR_LOCK_ADAPTIVE_SPIN        0x00040000u

static inline void
os_unfair_lock_lock_with_options(os_unfair_lock_t lock,
                                 os_unfair_lock_options_t options)
{
    (void)options;
    os_unfair_lock_lock(lock);
}

#endif /* PD_OS_LOCK_PRIVATE_H */
