// SPDX-License-Identifier: MIT
// FreeBSD's rwlock(9) on haiku rw_lock
#pragma once
#include <lock.h>

struct rwlock {
	rw_lock l;
};

#define rw_init(rw, name) rw_lock_init(&(rw)->l, name)
#define rw_init_flags(rw, name, opts) rw_lock_init(&(rw)->l, name)
#define rw_destroy(rw) rw_lock_destroy(&(rw)->l)
#define rw_rlock(rw) rw_lock_read_lock(&(rw)->l)
#define rw_runlock(rw) rw_lock_read_unlock(&(rw)->l)
#define rw_wlock(rw) rw_lock_write_lock(&(rw)->l)
#define rw_wunlock(rw) rw_lock_write_unlock(&(rw)->l)
#define rw_try_rlock(rw) rw_lock_try_read_lock(&(rw)->l)
#define rw_try_wlock(rw) rw_lock_try_write_lock(&(rw)->l)
#define rw_wowned(rw) ((rw)->l.holder == find_thread(NULL))
#define rw_assert(rw, what)
#define RA_LOCKED 0x01
#define RA_RLOCKED 0x02
#define RA_WLOCKED 0x04
#define RA_UNLOCKED 0x00
#define RW_DUPOK 0x01
#define RW_NOWITNESS 0x04
#define RW_RECURSE 0x10
#define RW_NEW 0x20
