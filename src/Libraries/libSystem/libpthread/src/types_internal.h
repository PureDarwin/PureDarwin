/*
 * Copyright (c) 2003-2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef __LIBPTHREAD_TYPES_INTERNAL_H__
#define __LIBPTHREAD_TYPES_INTERNAL_H__

/*!
 * @file types_internal.h
 *
 * @brief
 * This file exposes all the internal pthread types used by the library.
 *
 * @discussion
 * This header must be included first, as it masks the opaque definitions
 * exposed to libpthread clients in the SDK.
 */

#define _PTHREAD_ONCE_T
typedef struct pthread_once_s pthread_once_t;

#define _PTHREAD_MUTEX_T
#define _PTHREAD_MUTEXATTR_T
typedef struct pthread_mutex_s pthread_mutex_t;
typedef struct pthread_mutexattr_s pthread_mutexattr_t;

#define _PTHREAD_COND_T
#define _PTHREAD_CONDATTR_T
typedef struct pthread_cond_s pthread_cond_t;
typedef struct pthread_condattr_s pthread_condattr_t;

#define _PTHREAD_RWLOCK_T
#define _PTHREAD_RWLOCKATTR_T
typedef struct pthread_rwlock_s pthread_rwlock_t;
typedef struct pthread_rwlockattr_s pthread_rwlockattr_t;

#define _PTHREAD_T
#define _PTHREAD_ATTR_T
typedef struct pthread_s *pthread_t;
typedef struct pthread_attr_s pthread_attr_t;

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/param.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <os/base_private.h>
#include <os/once_private.h>
#include <os/lock.h>

#include "pthread/posix_sched.h"
#include "pthread/workqueue_private.h"
#include "sys/_pthread/_pthread_types.h"

#pragma mark - constants

#define _PTHREAD_NO_SIG                         0x00000000
#define _PTHREAD_MUTEX_ATTR_SIG                 0x4D545841  /* 'MTXA' */
#define _PTHREAD_MUTEX_SIG                      0x4D555458  /* 'MUTX' */
#define _PTHREAD_MUTEX_SIG_fast                 0x4D55545A  /* 'MUTZ' */
#define _PTHREAD_MUTEX_SIG_MASK                 0xfffffffd
#define _PTHREAD_MUTEX_SIG_CMP                  0x4D555458  /* _PTHREAD_MUTEX_SIG & _PTHREAD_MUTEX_SIG_MASK */
#define _PTHREAD_MUTEX_SIG_init                 0x32AAABA7  /* [almost] ~'MUTX' */
#define _PTHREAD_ERRORCHECK_MUTEX_SIG_init      0x32AAABA1
#define _PTHREAD_RECURSIVE_MUTEX_SIG_init       0x32AAABA2
#define _PTHREAD_FIRSTFIT_MUTEX_SIG_init        0x32AAABA3
#define _PTHREAD_MUTEX_SIG_init_MASK            0xfffffff0
#define _PTHREAD_MUTEX_SIG_init_CMP             0x32AAABA0
#define _PTHREAD_COND_ATTR_SIG                  0x434E4441  /* 'CNDA' */
#define _PTHREAD_COND_SIG_init                  0x3CB0B1BB  /* [almost] ~'COND' */
#define _PTHREAD_COND_SIG_pristine              0x434F4E44  /* 'COND' */
#define _PTHREAD_COND_SIG_psynch                0x434F4E45  /* 'COND' + 0b01: 'CONE' */
#define _PTHREAD_COND_SIG_ulock                 0x434F4E46  /* 'COND' + 0b10: 'CONF' */
#define _PTHREAD_ATTR_SIG                       0x54484441  /* 'THDA' */
#define _PTHREAD_ONCE_SIG                       0x4F4E4345  /* 'ONCE' */
#define _PTHREAD_ONCE_SIG_init                  0x30B1BCBA  /* [almost] ~'ONCE' */
#define _PTHREAD_SIG                            0x54485244  /* 'THRD' */
#define _PTHREAD_RWLOCK_ATTR_SIG                0x52574C41  /* 'RWLA' */
#define _PTHREAD_RWLOCK_SIG                     0x52574C4B  /* 'RWLK' */
#define _PTHREAD_RWLOCK_SIG_init                0x2DA8B3B4  /* [almost] ~'RWLK' */

__enum_closed_decl(pthread_conformance_t, unsigned, {
	PTHREAD_CONFORM_UNIX03_NOCANCEL    = 1,
	PTHREAD_CONFORM_UNIX03_CANCELABLE  = 2,
});

/* Pull the pthread_t into the same page as the top of the stack so we dirty one less page.
 * <rdar://problem/19941744> The pthread_s struct at the top of the stack shouldn't be page-aligned
 */
#if defined(__arm64__)
#define PTHREAD_T_OFFSET (12*1024)
#else
#define PTHREAD_T_OFFSET 0
#endif

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#define _EXTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_END 512
#else
#define _EXTERNAL_POSIX_THREAD_KEYS_MAX 512
#define _INTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_END 768
#endif

#define PTHREAD_ATFORK_INLINE_MAX       10

#define MAXTHREADNAMESIZE               64

#define _PTHREAD_DEFAULT_INHERITSCHED   PTHREAD_INHERIT_SCHED
#define _PTHREAD_DEFAULT_PROTOCOL       PTHREAD_PRIO_NONE
#define _PTHREAD_DEFAULT_PRIOCEILING    0
#define _PTHREAD_DEFAULT_POLICY         SCHED_OTHER
#define _PTHREAD_DEFAULT_STACKSIZE      0x80000    /* 512K */
#define _PTHREAD_DEFAULT_PSHARED        PTHREAD_PROCESS_PRIVATE

#define _PTHREAD_CANCEL_STATE_MASK      0x01
#define _PTHREAD_CANCEL_TYPE_MASK       0x02
#define _PTHREAD_CANCEL_PENDING         0x10  /* pthread_cancel() has been called for this thread */
#define _PTHREAD_CANCEL_EXITING         0x20

#define pthread_assert_type_size(type) \
	static_assert(sizeof(struct type##_s) == sizeof(struct _opaque_##type##_t), "")
#define pthread_assert_type_alias(type, f1, f2) \
	static_assert(offsetof(struct type##_s, f1) == offsetof(struct _opaque_##type##_t, f2), "")

typedef os_unfair_lock _pthread_lock;
struct _pthread_registration_data;


#pragma mark - pthread_once_t

struct pthread_once_s {
	long sig;
	os_once_t once;
};

pthread_assert_type_size(pthread_once);
pthread_assert_type_alias(pthread_once, sig, __sig);

#pragma mark - pthread_mutex_t, pthread_mutexattr_t

#define _PTHREAD_MUTEX_POLICY_LAST          (PTHREAD_MUTEX_POLICY_FIRSTFIT_NP + 1)
#define _PTHREAD_MTX_OPT_POLICY_FAIRSHARE   1
#define _PTHREAD_MTX_OPT_POLICY_FIRSTFIT    2
#define _PTHREAD_MTX_OPT_POLICY_DEFAULT     _PTHREAD_MTX_OPT_POLICY_FIRSTFIT
// The following pthread_mutex_options_s defintions exist in synch_internal.h
// such that the kernel extension can test for flags. They must be kept in
// sync with the bit values in the struct above.
// _PTHREAD_MTX_OPT_PSHARED 0x010
// _PTHREAD_MTX_OPT_NOTIFY 0x1000
// _PTHREAD_MTX_OPT_MUTEX 0x2000

#define _PTHREAD_MTX_OPT_ULOCK_DEFAULT false
#define _PTHREAD_MTX_OPT_ADAPTIVE_DEFAULT false

// The fixed mask is used to mask out portions of the mutex options that
// change on a regular basis (notify, lock_count).
#define _PTHREAD_MTX_OPT_FIXED_MASK         0x27ff

struct pthread_mutex_options_s {
	uint32_t
		protocol:2,
		type:2,
		pshared:2,
		policy:3,
		hold:2,
		misalign:1,
		notify:1,
		mutex:1,
		ulock:1,
		unused:1,
		lock_count:16;
};

#define _PTHREAD_MUTEX_ULOCK_OWNER_MASK 0xfffffffcu
#define _PTHREAD_MUTEX_ULOCK_WAITERS_BIT 0x00000001u
#define _PTHREAD_MUTEX_ULOCK_UNLOCKED_VALUE 0x0u
#define _PTHREAD_MUTEX_ULOCK_UNLOCKED \
		((struct _pthread_mutex_ulock_s){0})

typedef struct _pthread_mutex_ulock_s {
	uint32_t uval;
} *_pthread_mutex_ulock_t;

struct pthread_mutex_s {
	long sig;
	_pthread_lock lock;
	union {
		uint32_t value;
		struct pthread_mutex_options_s options;
	} mtxopts;
	int16_t prioceiling;
	int16_t priority;
#if defined(__LP64__)
	uint32_t _pad;
#endif
	union {
		struct {
			uint32_t m_tid[2]; // thread id of thread that has mutex locked
			uint32_t m_seq[2]; // mutex sequence id
			uint32_t m_mis[2]; // for misaligned locks m_tid/m_seq will span into here
		} psynch;
		struct _pthread_mutex_ulock_s ulock;
	};
#if defined(__LP64__)
	uint32_t _reserved[4];
#else
	uint32_t _reserved[1];
#endif
};

pthread_assert_type_size(pthread_mutex);
pthread_assert_type_alias(pthread_mutex, sig, __sig);

struct pthread_mutexattr_s {
	long sig;
	int prioceiling;
	uint32_t
		protocol:2,
		type:2,
		pshared:2,
		opt:3,
		unused:23;
};

pthread_assert_type_size(pthread_mutexattr);
pthread_assert_type_alias(pthread_mutexattr, sig, __sig);

#pragma mark - pthread_rwlock_t, pthread_rwlockattr_t

struct pthread_rwlock_s {
	long sig;
	_pthread_lock lock;
	uint32_t
		unused:29,
		misalign:1,
		pshared:2;
	uint32_t rw_flags;
#if defined(__LP64__)
	uint32_t _pad;
#endif
	uint32_t rw_tid[2]; // thread id of thread that has exclusive (write) lock
	uint32_t rw_seq[4]; // rw sequence id (at 128-bit aligned boundary)
	uint32_t rw_mis[4]; // for misaligned locks rw_seq will span into here
#if defined(__LP64__)
	uint32_t _reserved[34];
#else
	uint32_t _reserved[18];
#endif
};

pthread_assert_type_size(pthread_rwlock);
pthread_assert_type_alias(pthread_rwlock, sig, __sig);

struct pthread_rwlockattr_s {
	long sig;
	int pshared;
#if defined(__LP64__)
	uint32_t _reserved[3];
#else
	uint32_t _reserved[2];
#endif
};

pthread_assert_type_size(pthread_rwlockattr);
pthread_assert_type_alias(pthread_rwlockattr, sig, __sig);

#pragma mark - pthread_cond_t, pthread_condattr_t

struct pthread_cond_s {
	struct {
		uint32_t val;
#if defined(__LP64__)
		uint32_t _pad;
#endif
	} sig;
	_pthread_lock lock;
	uint32_t
		unused:29,
		misalign:1,
		pshared:2;
	pthread_mutex_t *busy;
	uint32_t c_seq[3];
#if defined(__LP64__)
	uint32_t _reserved[3];
#endif
};

pthread_assert_type_size(pthread_cond);
pthread_assert_type_alias(pthread_cond, sig, __sig);

struct pthread_condattr_s {
	long sig;
	uint32_t
		pshared:2,
		unsupported:30;
};

pthread_assert_type_size(pthread_condattr);
pthread_assert_type_alias(pthread_condattr, sig, __sig);

#pragma mark - pthread_t, pthread_attr_t

typedef struct pthread_join_context_s {
	pthread_t   waiter;
	void      **value_ptr;
	mach_port_t kport;
	semaphore_t custom_stack_sema;
	bool        detached;
} pthread_join_context_s, *pthread_join_context_t;

#define MAXTHREADNAMESIZE               64

struct pthread_s {
	long sig;
	struct __darwin_pthread_handler_rec *__cleanup_stack;

	//
	// Fields protected by _pthread_list_lock
	//

	TAILQ_ENTRY(pthread_s) tl_plist;              // global thread list [aligned]
	struct pthread_join_context_s *tl_join_ctx;
	void *tl_exit_value;
	uint8_t tl_policy;
	// pthread knows that tl_joinable bit comes immediately after tl_policy
	uint8_t
		tl_joinable:1,
		tl_joiner_cleans_up:1,
		tl_has_custom_stack:1,
		__tl_pad:5;
	uint16_t introspection;
	// MACH_PORT_NULL if no joiner
	// tsd[_PTHREAD_TSD_SLOT_MACH_THREAD_SELF] when has a joiner
	// MACH_PORT_DEAD if the thread exited
	uint32_t tl_exit_gate;
	struct sched_param tl_param;
	void *__unused_padding;

	//
	// Fields protected by pthread_t::lock
	//

	_pthread_lock lock;
	uint16_t max_tsd_key;
	uint16_t
		inherit:8,
		kernalloc:1,
		schedset:1,
		wqthread:1,
		wqkillset:1,
		__flags_pad:4;

	char pthread_name[MAXTHREADNAMESIZE];   // includes NUL [aligned]

	void  *(*fun)(void *);  // thread start routine
	void    *arg;           // thread start routine argument
	int      wq_nevents;    // wqthreads (workloop / kevent)
	bool     wq_outsideqos;
	uint8_t  canceled;      // 4597450 set if conformant cancelation happened
	uint16_t cancel_state;  // whether the thread can be canceled [atomic]
	errno_t  cancel_error;
	errno_t  err_no;        // thread-local errno

	void    *stackaddr;     // base of the stack (page aligned)
	void    *stackbottom;   // stackaddr - stacksize
	void    *freeaddr;      // stack/thread allocation base address
	size_t   freesize;      // stack/thread allocation size
	size_t   guardsize;     // guard page size in bytes

	// tsd-base relative accessed elements
	__attribute__((aligned(8)))
	uint64_t thread_id;     // 64-bit unique thread id

	/* Thread Specific Data slots
	 *
	 * The offset of this field from the start of the structure is difficult to
	 * change on OS X because of a thorny bitcompat issue: mono has hard coded
	 * the value into their source.  Newer versions of mono will fall back to
	 * scanning to determine it at runtime, but there's lots of software built
	 * with older mono that won't.  We will have to break them someday...
	 */
	__attribute__ ((aligned (16)))
	void *tsd[_EXTERNAL_POSIX_THREAD_KEYS_MAX + _INTERNAL_POSIX_THREAD_KEYS_MAX];
};

TAILQ_HEAD(__pthread_list, pthread_s);

#if 0 // pthread_t is never stack-allocated, so it doesn't matter
pthread_assert_type_size(pthread);
#endif
pthread_assert_type_alias(pthread, sig, __sig);
pthread_assert_type_alias(pthread, __cleanup_stack, __cleanup_stack);
#if __LP64__
static_assert(offsetof(struct pthread_s, tsd) == 224, "TSD LP64 offset");
#else
static_assert(offsetof(struct pthread_s, tsd) == 176, "TSD ILP32 offset");
#endif

#define _PTHREAD_ATTR_REFILLMS_MAX ((2<<24) - 1)

struct pthread_attr_s {
	long   sig;
	size_t guardsize; // size in bytes of stack overflow guard area
	void  *stackaddr; // stack base; vm_page_size aligned
	size_t stacksize; // stack size; multiple of vm_page_size and >= PTHREAD_STACK_MIN
	union {
		struct sched_param param; // [aligned]
		unsigned long qosclass; // pthread_priority_t
	};
	uint32_t
		detached:8,
		inherit:8,
		policy:8,
		schedset:1,
		qosset:1,
		policyset:1,
		cpupercentset:1,
		defaultguardpage:1,
		unused:3;
	uint32_t
		cpupercent:8,
		refillms:24;
#if defined(__LP64__)
	uint32_t _reserved[4];
#else
	uint32_t _reserved[2];
#endif
};

pthread_assert_type_size(pthread_attr);
pthread_assert_type_alias(pthread_attr, sig, __sig);

#pragma mark - atfork / qos

struct pthread_atfork_entry {
	void (*prepare)(void);
	void (*parent)(void);
	void (*child)(void);
};

#define PTHREAD_ATFORK_INLINE_MAX 10
#if defined(__arm__)
// Hack. We don't want to depend on libcompiler_rt. armv7 implements integer
// division by calling into compiler_rt. vm_page_size isn't a constant and
// pthread_atfork_entry is 12 bytes so the compiler can't strength-reduce the
// division, so it generates a call into compiler_rt.
// So let's just use PAGE_MAX_SIZE on armv7, which is a constant. At worst
// this wastes a maybe dozen K if we are actaully running on a smaller page
// size than the max.
// At the time of this writing we don't have any supported iOS armv7 hardware
// that has different vm_page_size and PAGE_MAX_SIZE.
#define PTHREAD_ATFORK_MAX (PAGE_MAX_SIZE/sizeof(struct pthread_atfork_entry))
#else // defined(__arm__)
#define PTHREAD_ATFORK_MAX (vm_page_size/sizeof(struct pthread_atfork_entry))
#endif // defined(__arm__)

struct pthread_globals_s {
	// atfork.c
	pthread_t psaved_self;
	_pthread_lock psaved_self_global_lock;
	_pthread_lock pthread_atfork_lock;

	size_t atfork_count;
	struct pthread_atfork_entry atfork_storage[PTHREAD_ATFORK_INLINE_MAX];
	struct pthread_atfork_entry *atfork;
	uint16_t qmp_logical[THREAD_QOS_LAST];
	uint16_t qmp_physical[THREAD_QOS_LAST];

};
typedef struct pthread_globals_s *pthread_globals_t;

#endif // __LIBPTHREAD_TYPES_INTERNAL_H__
