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

#ifndef __LIBPTHREAD_INLINE_INTERNAL_H__
#define __LIBPTHREAD_INLINE_INTERNAL_H__

/*!
 * @file inline_internal.h
 *
 * @brief
 * This file exposes inline helpers that are generally useful in libpthread.
 */

#define PTHREAD_INTERNAL_CRASH(c, x) OS_BUG_INTERNAL(c, "LIBPTHREAD", x)
#define PTHREAD_CLIENT_CRASH(c, x)   OS_BUG_CLIENT(c, "LIBPTHREAD", x)
#ifdef DEBUG
#define PTHREAD_DEBUG_ASSERT(b) \
	do { \
		if (os_unlikely(!(b))) { \
			PTHREAD_INTERNAL_CRASH(0, "Assertion failed: " #b); \
		} \
	} while (0)
#else
#define PTHREAD_DEBUG_ASSERT(b) ((void)0)
#endif

#pragma mark _pthread_mutex_check_signature

OS_ALWAYS_INLINE
static inline bool
_pthread_mutex_check_signature_fast(pthread_mutex_t *mutex)
{
	return (mutex->sig == _PTHREAD_MUTEX_SIG_fast);
}

OS_ALWAYS_INLINE
static inline bool
_pthread_mutex_check_signature(const pthread_mutex_t *mutex)
{
	// TODO: PTHREAD_STRICT candidate
	return ((mutex->sig & _PTHREAD_MUTEX_SIG_MASK) == _PTHREAD_MUTEX_SIG_CMP);
}

OS_ALWAYS_INLINE
static inline bool
_pthread_mutex_check_signature_init(const pthread_mutex_t *mutex)
{
	return ((mutex->sig & _PTHREAD_MUTEX_SIG_init_MASK) ==
			_PTHREAD_MUTEX_SIG_init_CMP);
}

#pragma mark pthread mutex accessors

OS_ALWAYS_INLINE
static inline bool
_pthread_mutex_uses_ulock(pthread_mutex_t *mutex)
{
	return mutex->mtxopts.options.ulock;
}

#pragma mark _pthread_rwlock_check_signature

OS_ALWAYS_INLINE
static inline bool
_pthread_rwlock_check_signature(const pthread_rwlock_t *rwlock)
{
	return (rwlock->sig == _PTHREAD_RWLOCK_SIG);
}

OS_ALWAYS_INLINE
static inline bool
_pthread_rwlock_check_signature_init(const pthread_rwlock_t *rwlock)
{
	return (rwlock->sig == _PTHREAD_RWLOCK_SIG_init);
}

#pragma mark unfair lock wrappers

#define _PTHREAD_LOCK_INITIALIZER OS_UNFAIR_LOCK_INIT
#define _PTHREAD_LOCK_OPTIONS \
		(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN)

OS_ALWAYS_INLINE
static inline void
_pthread_lock_init(os_unfair_lock_t lock)
{
	*lock = _PTHREAD_LOCK_INITIALIZER;
}

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
_pthread_lock_lock(os_unfair_lock_t lock)
{
#if OS_UNFAIR_LOCK_INLINE
	os_unfair_lock_lock_with_options_inline(lock, _PTHREAD_LOCK_OPTIONS);
#else
	os_unfair_lock_lock_with_options(lock, _PTHREAD_LOCK_OPTIONS);
#endif
}

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
_pthread_lock_lock(os_unfair_lock_t lock, mach_port_t mts)
{
#if OS_UNFAIR_LOCK_INLINE
	os_unfair_lock_lock_no_tsd_inline(lock, _PTHREAD_LOCK_OPTIONS, mts);
#else
	os_unfair_lock_lock_no_tsd(lock, _PTHREAD_LOCK_OPTIONS, mts);
#endif
}

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
_pthread_lock_unlock(os_unfair_lock_t lock)
{
#if OS_UNFAIR_LOCK_INLINE
	os_unfair_lock_unlock_inline(lock);
#else
	os_unfair_lock_unlock(lock);
#endif
}

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
_pthread_lock_unlock(os_unfair_lock_t lock, mach_port_t mts)
{
#if OS_UNFAIR_LOCK_INLINE
	os_unfair_lock_unlock_no_tsd_inline(lock, mts);
#else
	os_unfair_lock_unlock_no_tsd(lock, mts);
#endif
}

#pragma mark pthread accessors

// Internal references to pthread_self() use TSD slot 0 directly.
#define pthread_self() _pthread_self_direct()

// Internal references to errno use TSD slot 1 directly.
#undef  errno
#define errno          (*_pthread_errno_address_direct())

#define _pthread_tsd_slot(th, name) \
		(*(_PTHREAD_TSD_SLOT_##name##_TYPE *)(uintptr_t *)&(th)->tsd[_PTHREAD_TSD_SLOT_##name])

OS_ALWAYS_INLINE
static inline void
_pthread_validate_signature(pthread_t thread)
{
	pthread_t th  = (pthread_t)(thread->sig ^ _pthread_ptr_munge_token);
#if __has_feature(ptrauth_calls)
	th = ptrauth_auth_data(th, ptrauth_key_process_dependent_data,
			ptrauth_string_discriminator("pthread.signature"));
#endif
	if (os_unlikely(th != thread)) {
		/* OS_REASON_LIBSYSTEM_CODE_PTHREAD_CORRUPTION == 4 */
		abort_with_reason(OS_REASON_LIBSYSTEM, 4, "pthread_t was corrupted", 0);
	}
}

OS_ALWAYS_INLINE
static inline void
_pthread_init_signature(pthread_t thread)
{
	pthread_t th = thread;
#if __has_feature(ptrauth_calls)
	th = ptrauth_sign_unauthenticated(th, ptrauth_key_process_dependent_data,
			ptrauth_string_discriminator("pthread.signature"));
#endif
	thread->sig = (uintptr_t)th ^ _pthread_ptr_munge_token;
}

/*
 * ALWAYS called without list lock and return with list lock held on success
 *
 * This weird calling convention exists because this function will sometimes
 * drop the lock, and it's best callers don't have to remember this.
 */
OS_ALWAYS_INLINE
static inline bool
_pthread_validate_thread_and_list_lock(pthread_t thread)
{
	pthread_t p;
	if (thread == NULL) return false;
	_pthread_lock_lock(&_pthread_list_lock);
	TAILQ_FOREACH(p, &__pthread_head, tl_plist) {
		if (p != thread) continue;
		_pthread_validate_signature(p);
		return true;
	}
	_pthread_lock_unlock(&_pthread_list_lock);

	return false;
}

OS_ALWAYS_INLINE
static inline bool
_pthread_is_valid(pthread_t thread, mach_port_t *portp)
{
	mach_port_t kport = MACH_PORT_NULL;
	bool valid;

	if (thread == pthread_self()) {
		_pthread_validate_signature(thread);
		valid = true;
		kport = _pthread_tsd_slot(thread, MACH_THREAD_SELF);
	} else if (!_pthread_validate_thread_and_list_lock(thread)) {
		valid = false;
	} else {
		kport = _pthread_tsd_slot(thread, MACH_THREAD_SELF);
		valid = true;
		_pthread_lock_unlock(&_pthread_list_lock);
	}

	if (portp != NULL) {
		*portp = kport;
	}
	return valid;
}

OS_ALWAYS_INLINE OS_CONST
static inline pthread_globals_t
_pthread_globals(void)
{
	return os_alloc_once(OS_ALLOC_ONCE_KEY_LIBSYSTEM_PTHREAD,
			sizeof(struct pthread_globals_s), NULL);
}

#endif // __LIBPTHREAD_INLINE_INTERNAL_H__
