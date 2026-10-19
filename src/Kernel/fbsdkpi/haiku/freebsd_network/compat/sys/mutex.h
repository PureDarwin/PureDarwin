/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_MUTEX_H_
#define _FBSD_COMPAT_SYS_MUTEX_H_


#include <sys/_mutex.h>
#include <sys/systm.h>
#include <sys/pcpu.h>


#define MA_OWNED		0x1
#define MA_NOTOWNED		0x2
#define MA_RECURSED		0x4
#define MA_NOTRECURSED	0x8

#define	MTX_DEF			0x00000000
#define MTX_SPIN		0x00000001
#define MTX_RECURSE		0x00000004
#define MTX_QUIET		0x00040000
#define MTX_DUPOK		0x00400000


#define MTX_NETWORK_LOCK	"network driver"


extern struct mtx Giant;


void mtx_init(struct mtx*, const char*, const char*, int);
void mtx_sysinit(void *arg);
void mtx_destroy(struct mtx*);
void mtx_lock_spin(struct mtx* mutex);
void mtx_unlock_spin(struct mtx* mutex);
void _mtx_assert(struct mtx *m, int what, const char *file, int line);


#ifdef INVARIANTS
#	define	mtx_assert(m, what)						\
	_mtx_assert((m), (what), __FILE__, __LINE__)
#else
#	define	mtx_assert(m, what)
#endif


static inline void
mtx_lock(struct mtx* mutex)
{
	if (mutex->type == MTX_DEF) {
		haiku_mutex_lock(&mutex->u.mutex_.lock);
		mutex->u.mutex_.owner = find_thread(NULL);
	} else if (mutex->type == MTX_RECURSE) {
		recursive_lock_lock(&mutex->u.recursive);
	} else if (mutex->type == MTX_SPIN) {
		mtx_lock_spin(mutex);
	}
}


static inline int
mtx_trylock(struct mtx* mutex)
{
	if (mutex->type == MTX_DEF) {
		if (haiku_mutex_trylock(&mutex->u.mutex_.lock) != B_OK)
			return 0;
		mutex->u.mutex_.owner = find_thread(NULL);
		return 1;
	} else if (mutex->type == MTX_RECURSE) {
		if (recursive_lock_trylock(&mutex->u.recursive) != B_OK)
			return 0;
		return 1;
	} else if (mutex->type == MTX_SPIN) {
		return 0;
	}
	return 0;
}


static inline void
mtx_unlock(struct mtx* mutex)
{
	if (mutex->type == MTX_DEF) {
		mutex->u.mutex_.owner = -1;
		haiku_mutex_unlock(&mutex->u.mutex_.lock);
	} else if (mutex->type == MTX_RECURSE) {
		recursive_lock_unlock(&mutex->u.recursive);
	} else if (mutex->type == MTX_SPIN) {
		mtx_unlock_spin(mutex);
	}
}


static inline int
mtx_initialized(struct mtx* mutex)
{
	/* TODO */
	return 1;
}


static inline int
mtx_owned(struct mtx* mutex)
{
	if (mutex->type == MTX_DEF)
		return mutex->u.mutex_.owner == find_thread(NULL);
	if (mutex->type == MTX_RECURSE) {
#if KDEBUG
		return mutex->u.recursive.lock.holder == find_thread(NULL);
#else
		return mutex->u.recursive.holder == find_thread(NULL);
#endif
	}
	if (mutex->type == MTX_SPIN)
		return mutex->u.spinlock_.lock.lock != 0;

	return 0;
}


static inline int
mtx_recursed(struct mtx* mutex)
{
	if (mutex->type == MTX_RECURSE)
		return mutex->u.recursive.recursion != 0;
	return 0;
}


struct mtx_args {
	void		*ma_mtx;
	const char 	*ma_desc;
	int		 ma_opts;
};

#define	MTX_SYSINIT(name, mtx, desc, opts) \
	static struct mtx_args name##_args = {	\
		(mtx), \
		(desc), \
		(opts), \
	}; \
	SYSINIT(name##_mtx, SI_SUB_LOCK, SI_ORDER_MIDDLE, \
		mtx_sysinit, &name##_args); \
	SYSUNINIT(name##_mtx, SI_SUB_LOCK, SI_ORDER_MIDDLE, \
	    (system_init_func_t)mtx_destroy, (void*)mtx)


#endif	/* _FBSD_COMPAT_SYS_MUTEX_H_ */
