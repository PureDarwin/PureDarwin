/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __LIBPTHREAD_PROTOTYPES_INTERNAL_H__
#define __LIBPTHREAD_PROTOTYPES_INTERNAL_H__

/*!
 * @file prototypes_internal.h
 *
 * @brief
 * This file has prototypes for symbols / functions private to libpthread.
 */

#define PTHREAD_NOEXPORT __attribute__((visibility("hidden")))
#define PTHREAD_NOEXPORT_VARIANT


#pragma GCC visibility push(hidden)

/*!
 * @macro main_thread()
 *
 * @brief
 * Returns a pointer to the main thread.
 *
 * @discussion
 * The main thread structure really lives in dyld,
 * and when __pthread_init() is called, its pointer will be discovered
 * and stashed in _main_thread_ptr which libpthread uses.
 */
#if VARIANT_DYLD
extern struct pthread_s _main_thread;
#define main_thread() (&_main_thread)
#define __pthread_mutex_default_opt_policy _PTHREAD_MTX_OPT_POLICY_DEFAULT
#define __pthread_mutex_use_ulock _PTHREAD_MTX_OPT_ULOCK_DEFAULT
#define __pthread_mutex_ulock_adaptive_spin _PTHREAD_MTX_OPT_ADAPTIVE_DEFAULT
#else // VARIANT_DYLD
extern pthread_t _main_thread_ptr;
#define main_thread() (_main_thread_ptr)
extern void *(*_pthread_malloc)(size_t);
extern void (*_pthread_free)(void *);
extern int __pthread_mutex_default_opt_policy;
extern bool __pthread_mutex_use_ulock;
extern bool __pthread_mutex_ulock_adaptive_spin;
#endif // VARIANT_DYLD

extern struct __pthread_list __pthread_head; // List of all pthreads in the process.
extern _pthread_lock _pthread_list_lock;     // Lock protects access to above list.
extern uint32_t _main_qos;
extern uintptr_t _pthread_ptr_munge_token;

#if PTHREAD_DEBUG_LOG
#include <mach/mach_time.h>
extern int _pthread_debuglog;
extern uint64_t _pthread_debugstart;
#endif

/* pthread.c */
void _pthread_deallocate(pthread_t t, bool from_mach_thread);
void _pthread_main_thread_init(pthread_t p);
void _pthread_main_thread_postfork_init(pthread_t p);
void _pthread_bsdthread_init(struct _pthread_registration_data *data);
void *_pthread_atomic_xchg_ptr(void **p, void *v);
uint32_t _pthread_atomic_xchg_uint32_relaxed(uint32_t *p, uint32_t v);

/* pthread_cancelable.c */
void _pthread_markcancel_if_canceled(pthread_t thread, mach_port_t kport);
void _pthread_setcancelstate_exit(pthread_t self, void *value_ptr);
semaphore_t _pthread_joiner_prepost_wake(pthread_t thread);
int _pthread_join(pthread_t thread, void **value_ptr, pthread_conformance_t);

/* pthread_cond.c */
int _pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		const struct timespec *abstime, int isRelative, pthread_conformance_t);
int _pthread_mutex_droplock(pthread_mutex_t *mutex, uint32_t *flagp,
		uint32_t **pmtxp, uint32_t *mgenp, uint32_t *ugenp);

/* pthread_dependency.c */
void _pthread_dependency_fulfill_slow(pthread_dependency_t *pr, uint32_t old);

/* pthread_mutex.c */
OS_COLD OS_NORETURN
int _pthread_mutex_corruption_abort(pthread_mutex_t *mutex);
int _pthread_mutex_fairshare_lock_slow(pthread_mutex_t *mutex, bool trylock);
int _pthread_mutex_fairshare_unlock_slow(pthread_mutex_t *mutex);
int _pthread_mutex_ulock_lock(pthread_mutex_t *mutex, bool trylock);
int _pthread_mutex_ulock_unlock(pthread_mutex_t *mutex);
int _pthread_mutex_firstfit_lock_slow(pthread_mutex_t *mutex, bool trylock);
int _pthread_mutex_firstfit_unlock_slow(pthread_mutex_t *mutex);
int _pthread_mutex_lock_init_slow(pthread_mutex_t *mutex, bool trylock);
void _pthread_mutex_global_init(const char *envp[], struct _pthread_registration_data *registration_data);

/* pthread_rwlock.c */
enum rwlock_seqfields;
int _pthread_rwlock_lock_slow(pthread_rwlock_t *rwlock, bool readlock, bool trylock);
int _pthread_rwlock_unlock_slow(pthread_rwlock_t *rwlock, enum rwlock_seqfields updated_seqfields);

/* pthread_tsd.c */
void _pthread_tsd_cleanup(pthread_t self);
void _pthread_key_global_init(const char *envp[]);

/* qos.c */
thread_qos_t _pthread_qos_class_to_thread_qos(qos_class_t qos);
void _pthread_set_main_qos(pthread_priority_t qos);

#pragma GCC visibility pop

#endif // __LIBPTHREAD_PROTOTYPES_INTERNAL_H__
