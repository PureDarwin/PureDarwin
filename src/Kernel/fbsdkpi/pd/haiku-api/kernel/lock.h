// SPDX-License-Identifier: MIT
// haiku's kernel lock api, on xnu locks through the fbsdkpi seam
#ifndef _PD_HAIKU_KERNEL_LOCK_H
#define _PD_HAIKU_KERNEL_LOCK_H

#include <OS.h>
#include <KernelExport.h>

typedef struct haiku_mutex {
	const char		*name;
	void			*impl;		// created on first use, so static initialisers work
	thread_id		holder;
	uint32			flags;
} haiku_mutex;

#ifndef PD_LINUXKPI
// linuxkpi has its own struct mutex, the plain names are only for haiku code
typedef haiku_mutex mutex;
#define mutex_init haiku_mutex_init
#define mutex_init_etc haiku_mutex_init_etc
#define mutex_destroy haiku_mutex_destroy
#define mutex_lock haiku_mutex_lock
#define mutex_trylock haiku_mutex_trylock
#define mutex_unlock haiku_mutex_unlock
#define mutex_switch_lock haiku_mutex_switch_lock
#endif

#define MUTEX_FLAG_CLONE_NAME	0x1
#define MUTEX_INITIALIZER(name)	{ name, NULL, -1, 0 }

typedef struct recursive_lock {
	haiku_mutex		lock;
	thread_id		holder;
	int				recursion;
} recursive_lock;

#define RECURSIVE_LOCK_INITIALIZER(name)	{ MUTEX_INITIALIZER(name), -1, 0 }

typedef struct rw_lock {
	const char		*name;
	void			*impl;
	thread_id		holder;
	int32			owner_count;
} rw_lock;

#define RW_LOCK_INITIALIZER(name)	{ name, NULL, -1, 0 }
#define RW_LOCK_FLAG_CLONE_NAME	0x1

#ifdef __cplusplus
extern "C" {
#endif

void haiku_mutex_init(haiku_mutex *lock, const char *name);
void haiku_mutex_init_etc(haiku_mutex *lock, const char *name, uint32 flags);
void haiku_mutex_destroy(haiku_mutex *lock);
status_t haiku_mutex_lock(haiku_mutex *lock);
status_t haiku_mutex_trylock(haiku_mutex *lock);
void haiku_mutex_unlock(haiku_mutex *lock);
status_t haiku_mutex_switch_lock(haiku_mutex *from, haiku_mutex *to);

void recursive_lock_init(recursive_lock *lock, const char *name);
void recursive_lock_init_etc(recursive_lock *lock, const char *name, uint32 flags);
void recursive_lock_destroy(recursive_lock *lock);
status_t recursive_lock_lock(recursive_lock *lock);
status_t recursive_lock_trylock(recursive_lock *lock);
void recursive_lock_unlock(recursive_lock *lock);
int32 recursive_lock_get_recursion(recursive_lock *lock);
#define RECURSIVE_LOCK_HOLDER(lock)	((lock)->holder)

void rw_lock_init(rw_lock *lock, const char *name);
void rw_lock_init_etc(rw_lock *lock, const char *name, uint32 flags);
void rw_lock_destroy(rw_lock *lock);
status_t rw_lock_read_lock(rw_lock *lock);
status_t rw_lock_read_unlock(rw_lock *lock);
status_t rw_lock_write_lock(rw_lock *lock);
status_t rw_lock_write_unlock(rw_lock *lock);
bool rw_lock_try_read_lock(rw_lock *lock);
bool rw_lock_try_write_lock(rw_lock *lock);
void rw_lock_write_downgrade(rw_lock *lock);

#ifdef __cplusplus
}
#endif

#define ASSERT_LOCKED_MUTEX(m)
#define ASSERT_UNLOCKED_MUTEX(m)
#define ASSERT_LOCKED_RECURSIVE(r)

#endif
