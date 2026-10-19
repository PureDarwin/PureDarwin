/*
 * Copyright 2001-2011 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PTHREAD_H_
#define _PTHREAD_H_


#include <sched.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>


#define PTHREAD_MUTEX_DEFAULT		0
#define PTHREAD_MUTEX_NORMAL		1
#define PTHREAD_MUTEX_ERRORCHECK	2
#define PTHREAD_MUTEX_RECURSIVE		3

#define PTHREAD_PROCESS_PRIVATE		0
#define PTHREAD_PROCESS_SHARED		1

/*
 * Flags for threads and thread attributes.
 */
#define PTHREAD_DETACHED			0x1
#define PTHREAD_SCOPE_SYSTEM		0x2
#define PTHREAD_INHERIT_SCHED		0x4
#define PTHREAD_NOFLOAT				0x8

#define PTHREAD_CREATE_DETACHED		PTHREAD_DETACHED
#define PTHREAD_CREATE_JOINABLE		0
#define PTHREAD_SCOPE_PROCESS		0
#define PTHREAD_EXPLICIT_SCHED		0

/*
 * Flags for cancelling threads
 */
#define PTHREAD_CANCEL_ENABLE		0
#define PTHREAD_CANCEL_DISABLE		1
#define PTHREAD_CANCEL_DEFERRED		0
#define PTHREAD_CANCEL_ASYNCHRONOUS	2
#define PTHREAD_CANCELED			((void *) 1)

#define PTHREAD_ONCE_INIT 			{ -1 }

#define PTHREAD_BARRIER_SERIAL_THREAD 1

#define PTHREAD_PRIO_NONE			0
#define PTHREAD_PRIO_INHERIT		1
#define PTHREAD_PRIO_PROTECT		2


/* private structure */
struct __pthread_cleanup_handler {
	struct __pthread_cleanup_handler *previous;
	void	(*function)(void *argument);
	void	*argument;
};

#define pthread_cleanup_push(func, arg) \
	do { \
		struct __pthread_cleanup_handler __handler; \
		__handler.function = (func); \
		__handler.argument = (arg); \
		__pthread_cleanup_push_handler(&__handler);

#define pthread_cleanup_pop(execute) \
		if (execute) \
			__handler.function(__handler.argument); \
		__pthread_cleanup_pop_handler(); \
	} while (0)


#ifdef __cplusplus
extern "C" {
#endif

#define PTHREAD_MUTEX_INITIALIZER \
	{ PTHREAD_MUTEX_DEFAULT, 0, -42, -1, 0 }
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP \
	{ PTHREAD_MUTEX_RECURSIVE, 0, -42, -1, 0 }
#define PTHREAD_COND_INITIALIZER	\
	{ 0, -42, NULL, 0, 0 }
#define PTHREAD_RWLOCK_INITIALIZER	\
	{ 0, -1, {{0}} }

/* mutex functions */
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);
extern int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex,
	int *_priorityCeiling);
extern int pthread_mutex_init(pthread_mutex_t *mutex,
	const pthread_mutexattr_t *attr);
extern int pthread_mutex_lock(pthread_mutex_t *mutex);
extern int pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
	int newPriorityCeiling, int *_oldPriorityCeiling);
extern int pthread_mutex_clocklock(pthread_mutex_t *mutex,
	clockid_t clock_id, const struct timespec *abstime);
extern int pthread_mutex_timedlock(pthread_mutex_t *mutex,
	const struct timespec *abstime);
extern int pthread_mutex_trylock(pthread_mutex_t *mutex);
extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* mutex attribute functions */
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *mutexAttr);
extern int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *mutexAttr,
	int *_priorityCeiling);
extern int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *mutexAttr,
	int *_protocol);
extern int pthread_mutexattr_getpshared(const pthread_mutexattr_t *mutexAttr,
	int *_processShared);
extern int pthread_mutexattr_gettype(const pthread_mutexattr_t *mutexAttr,
	int *_type);
extern int pthread_mutexattr_init(pthread_mutexattr_t *mutexAttr);
extern int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *mutexAttr,
	int priorityCeiling);
extern int pthread_mutexattr_setprotocol(pthread_mutexattr_t *mutexAttr,
	int protocol);
extern int pthread_mutexattr_setpshared(pthread_mutexattr_t *mutexAttr,
	int processShared);
extern int pthread_mutexattr_settype(pthread_mutexattr_t *mutexAttr, int type);

/* barrier functions */
extern int pthread_barrier_init(pthread_barrier_t *barrier,
	const pthread_barrierattr_t *attr, unsigned count);
extern int pthread_barrier_destroy(pthread_barrier_t *barrier);
extern int pthread_barrier_wait(pthread_barrier_t *barrier);

/* barrier attribute functions */
extern int pthread_barrierattr_destroy(pthread_barrierattr_t *attr);
extern int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr,
	int *shared);
extern int pthread_barrierattr_init(pthread_barrierattr_t *attr);
extern int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr,
	int shared);

/* condition variable functions */
extern int pthread_cond_destroy(pthread_cond_t *cond);
extern int pthread_cond_init(pthread_cond_t *cond,
	const pthread_condattr_t *attr);
extern int pthread_cond_broadcast(pthread_cond_t *cond);
extern int pthread_cond_signal(pthread_cond_t *cond);
extern int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
	clockid_t clock_id, const struct timespec *abstime);
extern int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime);
extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/* condition variable attribute functions */
extern int pthread_condattr_destroy(pthread_condattr_t *condAttr);
extern int pthread_condattr_init(pthread_condattr_t *condAttr);
extern int pthread_condattr_getpshared(const pthread_condattr_t *condAttr,
	int *processShared);
extern int pthread_condattr_setpshared(pthread_condattr_t *condAttr,
	int processShared);
extern int pthread_condattr_getclock(const pthread_condattr_t *condAttr,
	clockid_t *clockID);
extern int pthread_condattr_setclock(pthread_condattr_t *condAttr,
	clockid_t clockID);

/* rwlock functions */
extern int pthread_rwlock_init(pthread_rwlock_t *lock,
	const pthread_rwlockattr_t *attr);
extern int pthread_rwlock_destroy(pthread_rwlock_t *lock);
extern int pthread_rwlock_rdlock(pthread_rwlock_t *lock);
extern int pthread_rwlock_tryrdlock(pthread_rwlock_t *lock);
extern int pthread_rwlock_clockrdlock(pthread_rwlock_t* rwlock,
	clockid_t clock_id, const struct timespec* abstime);
extern int pthread_rwlock_timedrdlock(pthread_rwlock_t *lock,
	const struct timespec *abstime);
extern int pthread_rwlock_wrlock(pthread_rwlock_t *lock);
extern int pthread_rwlock_trywrlock(pthread_rwlock_t *lock);
extern int pthread_rwlock_clockwrlock(pthread_rwlock_t* rwlock,
	clockid_t clock_id, const struct timespec* abstime);
extern int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock,
	const struct timespec *abstime);
extern int pthread_rwlock_unlock(pthread_rwlock_t *lock);

/* rwlock attribute functions */
extern int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
extern int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
extern int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr,
	int *shared);
extern int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr,
	int shared);

/* spinlock functions */
extern int pthread_spin_init(pthread_spinlock_t* spinlock, int pshared);
extern int pthread_spin_destroy(pthread_spinlock_t* spinlock);
extern int pthread_spin_lock(pthread_spinlock_t* spinlock);
extern int pthread_spin_trylock(pthread_spinlock_t* spinlock);
extern int pthread_spin_unlock(pthread_spinlock_t* spinlock);

/* misc. functions */
extern int pthread_atfork(void (*prepare)(void), void (*parent)(void),
	void (*child)(void));
extern int pthread_once(pthread_once_t *once_control,
	void (*init_routine)(void));
extern int pthread_getcpuclockid(pthread_t thread_id, clockid_t* clock_id);

/* thread attributes functions */
extern int pthread_attr_destroy(pthread_attr_t *attr);
extern int pthread_attr_init(pthread_attr_t *attr);
extern int pthread_attr_getdetachstate(const pthread_attr_t *attr,
	int *detachstate);
extern int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
extern int pthread_attr_getstacksize(const pthread_attr_t *attr,
	size_t *stacksize);
extern int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
extern int pthread_attr_getscope(const pthread_attr_t *attr,
	int *contentionScope);
extern int pthread_attr_setscope(pthread_attr_t *attr, int contentionScope);

extern int pthread_attr_getschedparam(const pthread_attr_t *attr,
	struct sched_param *param);
extern int pthread_attr_setschedparam(pthread_attr_t *attr,
	const struct sched_param *param);

extern int pthread_attr_getguardsize(const pthread_attr_t *attr,
	size_t *guardsize);
extern int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);

extern int pthread_attr_getstack(const pthread_attr_t *attr,
	void **stackaddr, size_t *stacksize);
extern int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr,
	size_t stacksize);

#if 0   /* Unimplemented attribute functions: */

/* [TPS] */
extern int pthread_attr_getinheritsched(const pthread_attr_t *attr,
	int *inheritsched);
extern int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched);

extern int pthread_attr_getschedpolicy(const pthread_attr_t *attr,
	int *policy);
extern int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);

#endif	/* 0 */

/* thread functions */
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine)(void*), void *arg);
extern int pthread_detach(pthread_t thread);
extern int pthread_equal(pthread_t t1, pthread_t t2);
extern void pthread_exit(void *value_ptr) __attribute__ ((noreturn));
extern int pthread_join(pthread_t thread, void **_value);
extern pthread_t pthread_self(void);
extern int pthread_getconcurrency(void);
extern int pthread_setconcurrency(int newLevel);

extern int pthread_cancel(pthread_t thread);
extern int pthread_setcancelstate(int state, int *_oldState);
extern int pthread_setcanceltype(int type, int *_oldType);
extern void pthread_testcancel(void);

extern int pthread_getschedparam(pthread_t thread, int *policy,
	struct sched_param *param);
extern int pthread_setschedparam(pthread_t thread, int policy,
	const struct sched_param *param);

/* thread specific data functions */
extern int pthread_key_create(pthread_key_t *key,
	void (*destructorFunc)(void*));
extern int pthread_key_delete(pthread_key_t key);
extern void *pthread_getspecific(pthread_key_t key);
extern int pthread_setspecific(pthread_key_t key, const void *value);

/* private functions */
extern void __pthread_cleanup_push_handler(
	struct __pthread_cleanup_handler *handler);
extern struct __pthread_cleanup_handler *__pthread_cleanup_pop_handler(void);

#ifdef __cplusplus
}
#endif

#endif	/* _PTHREAD_ */
