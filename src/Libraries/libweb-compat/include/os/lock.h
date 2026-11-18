// lock.h - os_unfair_lock for the browser build, where everything runs on the page's one thread
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __OS_LOCK__
#define __OS_LOCK__

#include <stdint.h>

typedef struct os_unfair_lock_s {
    uint32_t _os_unfair_lock_opaque;
} os_unfair_lock, *os_unfair_lock_t;

#define OS_UNFAIR_LOCK_INIT ((os_unfair_lock){ 0 })

// there is no other thread to keep out
static inline void os_unfair_lock_lock(os_unfair_lock_t lock) { (void)lock; }
static inline void os_unfair_lock_unlock(os_unfair_lock_t lock) { (void)lock; }
static inline _Bool os_unfair_lock_trylock(os_unfair_lock_t lock) { (void)lock; return 1; }
static inline void os_unfair_lock_assert_owner(os_unfair_lock_t lock) { (void)lock; }

#endif
