/*
 * Copyright 2018, Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Augustin Cavalier <waddlesplash>
 */
#ifndef _FBSD_COMPAT_SYS_SX_H_
#define _FBSD_COMPAT_SYS_SX_H_


#include <lock.h>


struct sx {
	rw_lock l;
	// the exclusive owner, kept in both modes so the layout matches
	uintptr_t sx_lock;
};

#define sx_init(lock, name)		do { rw_lock_init(&(lock)->l, name); (lock)->sx_lock = 0; } while (0)
#define sx_xlock(lock)			rw_lock_write_lock(&(lock)->l)
#define sx_xunlock(lock)		rw_lock_write_unlock(&(lock)->l)
#define sx_destroy(lock)		rw_lock_destroy(&(lock)->l)

#define sx_assert(sx, what)

#ifdef PD_LINUXKPI
// the rest of sx(9) that linuxkpi uses, it reads the owner through SX_OWNER
#undef sx_xlock
#undef sx_xunlock
#define sx_xlock(lock) do { rw_lock_write_lock(&(lock)->l); (lock)->sx_lock = (uintptr_t)curthread; } while (0)
#define sx_xunlock(lock) do { (lock)->sx_lock = 0; rw_lock_write_unlock(&(lock)->l); } while (0)
#define SX_OWNER(x) ((uintptr_t)(x))
#define sx_init_flags(lock, name, flags) sx_init(lock, name)
#define sx_slock(lock) rw_lock_read_lock(&(lock)->l)
#define sx_sunlock(lock) rw_lock_read_unlock(&(lock)->l)
#define sx_try_slock(lock) rw_lock_try_read_lock(&(lock)->l)
#define sx_try_xlock(lock) (rw_lock_try_write_lock(&(lock)->l) ? ((lock)->sx_lock = (uintptr_t)curthread, 1) : 0)
#define sx_slock_sig(lock) (sx_slock(lock), 0)
#define sx_xlock_sig(lock) ({ sx_xlock(lock); 0; })
#define sx_downgrade(lock) do { (lock)->sx_lock = 0; rw_lock_write_downgrade(&(lock)->l); } while (0)
#define sx_xlocked(lock) ((lock)->sx_lock == (uintptr_t)curthread)
#define sx_xholder(lock) ((struct thread *)(lock)->sx_lock)
#define sx_has_waiters(lock) 0
#define sx_unlock(lock) do { if (sx_xlocked(lock)) sx_xunlock(lock); else sx_sunlock(lock); } while (0)
#define SX_DUPOK 0x01
#define SX_NOWITNESS 0x04
#define SX_RECURSE 0x10
#define SX_NEW 0x20
#define SA_LOCKED 0x01
#define SA_SLOCKED 0x02
#define SA_XLOCKED 0x04
#define SA_UNLOCKED 0x00
#endif


#endif /* _FBSD_COMPAT_SYS_SX_H_ */
