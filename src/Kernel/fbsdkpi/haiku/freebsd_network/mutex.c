/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


#include "device.h"

#include <compat/sys/mutex.h>


struct mtx Giant;
struct rw_lock ifnet_rwlock;
struct mtx gIdStoreLock;


void
mtx_init(struct mtx *mutex, const char *name, const char *type,
	int options)
{
	if ((options & MTX_RECURSE) != 0) {
		recursive_lock_init_etc(&mutex->u.recursive, name,
			MUTEX_FLAG_CLONE_NAME);
		mutex->type = MTX_RECURSE;
	} else if ((options & MTX_SPIN) != 0) {
		B_INITIALIZE_SPINLOCK(&mutex->u.spinlock_.lock);
		mutex->type = MTX_SPIN;
	} else {
		mutex_init_etc(&mutex->u.mutex_.lock, name, MUTEX_FLAG_CLONE_NAME);
		mutex->u.mutex_.owner = -1;
		mutex->type = MTX_DEF;
	}
}


void
mtx_sysinit(void *arg)
{
	struct mtx_args *margs = arg;

	mtx_init((struct mtx *)margs->ma_mtx, margs->ma_desc, NULL,
	    margs->ma_opts);
}


void
mtx_destroy(struct mtx *mutex)
{
	if ((mutex->type & MTX_RECURSE) != 0) {
		recursive_lock_destroy(&mutex->u.recursive);
	} else if ((mutex->type & MTX_SPIN) != 0) {
		KASSERT(!B_SPINLOCK_IS_LOCKED(&mutex->u.spinlock_.lock), ("spin mutex is locked"));
	} else {
		mutex_destroy(&mutex->u.mutex_.lock);
	}
}


void
mtx_lock_spin(struct mtx* mutex)
{
	KASSERT(mutex->type == MTX_SPIN, ("not a spin mutex"));

	cpu_status status = disable_interrupts();
	acquire_spinlock(&mutex->u.spinlock_.lock);
	mutex->u.spinlock_.state = status;
}


void
mtx_unlock_spin(struct mtx* mutex)
{
	KASSERT(mutex->type == MTX_SPIN, ("not a spin mutex"));

	cpu_status status = mutex->u.spinlock_.state;
	release_spinlock(&mutex->u.spinlock_.lock);
	restore_interrupts(status);
}


void
_mtx_assert(struct mtx *m, int what, const char *file, int line)
{
	switch (what) {
	case MA_OWNED:
	case MA_OWNED | MA_RECURSED:
	case MA_OWNED | MA_NOTRECURSED:
		if (!mtx_owned(m))
			panic("mutex %p not owned at %s:%d",
				m, file, line);
		if (mtx_recursed(m)) {
			if ((what & MA_NOTRECURSED) != 0)
				panic("mutex %p recursed at %s:%d",
					m, file, line);
		} else if ((what & MA_RECURSED) != 0) {
			panic("mutex %p unrecursed at %s:%d",
				m, file, line);
		}
		break;
	case MA_NOTOWNED:
		if (mtx_owned(m))
			panic("mutex %p owned at %s:%d",
				m, file, line);
		break;
	default:
		panic("unknown mtx_assert at %s:%d", file, line);
	}
}


status_t
init_mutexes()
{
	mtx_init(&Giant, "Banana Giant", NULL, MTX_DEF);
	rw_lock_init(&ifnet_rwlock, "gDevices");
	mtx_init(&gIdStoreLock, "Identity Store", NULL, MTX_DEF);

	return B_OK;
}


void
uninit_mutexes()
{
	mtx_destroy(&Giant);
	rw_lock_destroy(&ifnet_rwlock);
	mtx_destroy(&gIdStoreLock);
}
