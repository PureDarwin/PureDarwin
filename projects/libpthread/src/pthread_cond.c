/*
 * Copyright (c) 2000-2003, 2007, 2008 Apple Inc. All rights reserved.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * MkLinux
 */

/*
 * POSIX Pthread Library
 */

#include "resolver.h"
#include "internal.h"
#include <sys/time.h>	      /* For struct timespec and getclock(). */

#ifdef PLOCKSTAT
#include "plockstat.h"
#else /* !PLOCKSTAT */
#define PLOCKSTAT_MUTEX_RELEASE(x, y)
#endif /* PLOCKSTAT */

typedef union {
	uint64_t val;
	struct {
		uint32_t seq;
		uint16_t waiters;
		uint16_t signal;
	};
} pthread_ulock_cond_state_u;

#define _PTHREAD_COND_WAITERS_INC \
		(1ull << (offsetof(pthread_ulock_cond_state_u, waiters) * CHAR_BIT))

OS_ALWAYS_INLINE
static inline void
COND_GETSEQ_ADDR(pthread_cond_t *cond,
		volatile uint64_t **c_lsseqaddr,
		volatile uint32_t **c_lseqcnt,
		volatile uint32_t **c_useqcnt,
		volatile uint32_t **c_sseqcnt)
{
	if (cond->misalign) {
		*c_lseqcnt = &cond->c_seq[1];
		*c_sseqcnt = &cond->c_seq[2];
		*c_useqcnt = &cond->c_seq[0];
	} else {
		*c_lseqcnt = &cond->c_seq[0];
		*c_sseqcnt = &cond->c_seq[1];
		*c_useqcnt = &cond->c_seq[2];
	}
	*c_lsseqaddr = (volatile uint64_t *)*c_lseqcnt;
}

OS_ALWAYS_INLINE
static inline pthread_ulock_cond_state_u *
_pthread_ulock_cond_state(pthread_cond_t *cond)
{
	return (pthread_ulock_cond_state_u *)&cond->c_seq[cond->misalign];
}

#ifndef BUILDING_VARIANT /* [ */

static void _pthread_psynch_cond_cleanup(void *arg);
static void _pthread_cond_updateval(pthread_cond_t *cond, pthread_mutex_t *mutex,
		int error, uint32_t updateval);

static int
_pthread_ulock_cond_wait_complete(pthread_ulock_cond_state_u *state,
		pthread_mutex_t *mutex, int rc);
static void
_pthread_ulock_cond_cleanup(void *arg);


int
pthread_condattr_init(pthread_condattr_t *attr)
{
	attr->sig = _PTHREAD_COND_ATTR_SIG;
	attr->pshared = _PTHREAD_DEFAULT_PSHARED;
	return 0;
}

int
pthread_condattr_destroy(pthread_condattr_t *attr)
{
	attr->sig = _PTHREAD_NO_SIG;
	return 0;
}

int
pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_COND_ATTR_SIG) {
		*pshared = (int)attr->pshared;
		res = 0;
	}
	return res;
}

int
pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
	int res = EINVAL;
	if (attr->sig == _PTHREAD_COND_ATTR_SIG) {
		if (pshared == PTHREAD_PROCESS_PRIVATE || pshared == PTHREAD_PROCESS_SHARED) {
			attr->pshared = pshared;
			res = 0;
		}
	}
	return res;
}

int
pthread_cond_timedwait_relative_np(pthread_cond_t *cond, pthread_mutex_t *mutex,
		const struct timespec *abstime)
{
	return _pthread_cond_wait(cond, mutex, abstime, 1,
			PTHREAD_CONFORM_UNIX03_NOCANCEL);
}

#endif /* !BUILDING_VARIANT ] */

OS_ALWAYS_INLINE
static inline int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr,
		uint32_t sig)
{
	volatile uint64_t *c_lsseqaddr;
	volatile uint32_t *c_lseqcnt, *c_useqcnt, *c_sseqcnt;

	cond->busy = NULL;
	cond->c_seq[0] = 0;
	cond->c_seq[1] = 0;
	cond->c_seq[2] = 0;
	cond->unused = 0;

	// TODO: PTHREAD_STRICT candidate
	cond->misalign = (((uintptr_t)&cond->c_seq[0]) & 0x7) != 0;
	COND_GETSEQ_ADDR(cond, &c_lsseqaddr, &c_lseqcnt, &c_useqcnt, &c_sseqcnt);
	*c_sseqcnt = PTH_RWS_CV_CBIT; // set Sword to 0c

	if (attr) {
		cond->pshared = attr->pshared;
	} else {
		cond->pshared = _PTHREAD_DEFAULT_PSHARED;
	}

	// Ensure all contents are properly set before setting signature.
#if defined(__LP64__)
	// For binary compatibility reasons we cannot require natural alignment of
	// the 64bit 'sig' long value in the struct. rdar://problem/21610439
	cond->sig._pad = 0;
#endif
	os_atomic_store(&cond->sig.val, sig, release);

	return 0;
}

#ifndef BUILDING_VARIANT /* [ */

OS_ALWAYS_INLINE
static int
_pthread_cond_check_signature(pthread_cond_t *cond, uint32_t sig_current,
		uint32_t *sig_inout)
{
	int res = 0;
	switch (sig_current) {
	case _PTHREAD_COND_SIG_init:
		__builtin_unreachable();
		break;
	case _PTHREAD_COND_SIG_pristine:
		if (*sig_inout != _PTHREAD_COND_SIG_pristine) {
			os_atomic_store(&cond->sig.val, *sig_inout, relaxed);
		}
		break;
	case _PTHREAD_COND_SIG_psynch:
	case _PTHREAD_COND_SIG_ulock:
		if (*sig_inout == _PTHREAD_COND_SIG_pristine) {
			*sig_inout = sig_current;
		} else if (*sig_inout != sig_current) {
			PTHREAD_INTERNAL_CRASH(0, "Mixed ulock and psych condvar use");
		}
		break;
	default:
		// TODO: PTHREAD_STRICT candidate
		res = EINVAL;
		break;
	}

	return res;
}

OS_NOINLINE
static int
_pthread_cond_check_init_slow(pthread_cond_t *cond, uint32_t *sig_inout)
{
	int res;
	_pthread_lock_lock(&cond->lock);

	uint32_t sig_current = os_atomic_load(&cond->sig.val, relaxed);
	if (sig_current == _PTHREAD_COND_SIG_init) {
		res = _pthread_cond_init(cond, NULL, *sig_inout);
	} else {
		res = _pthread_cond_check_signature(cond, sig_current, sig_inout);
	}

	_pthread_lock_unlock(&cond->lock);
	return res;
}

/*
 * These routines maintain the signature of the condition variable, which
 * encodes a small state machine:
 * - a statically initialized condvar begins with SIG_init
 * - explicit initialization via _cond_init() and implicit initialization
 *   transition to SIG_pristine, as there have been no waiters so we don't know
 *   what kind of mutex we'll be used with
 * - the first _cond_wait() transitions to one of SIG_psynch or SIG_ulock
 *   according to the mutex being waited on
 *
 * On entry, *sig_inout is the furthest state we can transition to given the
 * calling context.  On exit, it is the actual state we observed, after any
 * possible advancement.
 */
OS_ALWAYS_INLINE
static inline int
_pthread_cond_check_init(pthread_cond_t *cond, uint32_t *sig_inout)
{
	uint32_t sig_current = os_atomic_load(&cond->sig.val, relaxed);
	if (sig_current == _PTHREAD_COND_SIG_init) {
		return _pthread_cond_check_init_slow(cond, sig_inout);
	} else {
		return _pthread_cond_check_signature(cond, sig_current, sig_inout);
	}
}

PTHREAD_NOEXPORT_VARIANT
int
pthread_cond_destroy(pthread_cond_t *cond)
{
	int res = EINVAL;
	uint32_t sig = os_atomic_load(&cond->sig.val, relaxed);
	switch (sig) {
	case _PTHREAD_COND_SIG_psynch:
		_pthread_lock_lock(&cond->lock);

		uint64_t oldval64, newval64;
		uint32_t lcntval, ucntval, scntval;
		volatile uint64_t *c_lsseqaddr;
		volatile uint32_t *c_lseqcnt, *c_useqcnt, *c_sseqcnt;

		COND_GETSEQ_ADDR(cond, &c_lsseqaddr, &c_lseqcnt, &c_useqcnt, &c_sseqcnt);

		do {
			lcntval = *c_lseqcnt;
			ucntval = *c_useqcnt;
			scntval = *c_sseqcnt;

			// validate it is not busy
			if ((lcntval & PTHRW_COUNT_MASK) != (scntval & PTHRW_COUNT_MASK)) {
				//res = EBUSY;
				break;
			}
			oldval64 = (((uint64_t)scntval) << 32);
			oldval64 |= lcntval;
			newval64 = oldval64;
		} while (!os_atomic_cmpxchg(c_lsseqaddr, oldval64, newval64, seq_cst));

		// <rdar://problem/13782056> Need to clear preposts.
		uint32_t flags = 0;
		bool needclearpre = ((scntval & PTH_RWS_CV_PBIT) != 0);
		if (needclearpre && cond->pshared == PTHREAD_PROCESS_SHARED) {
			flags |= _PTHREAD_MTX_OPT_PSHARED;
		}

		os_atomic_store(&cond->sig.val, _PTHREAD_NO_SIG, relaxed);
		res = 0;

		_pthread_lock_unlock(&cond->lock);

		if (needclearpre) {
			(void)__psynch_cvclrprepost(cond, lcntval, ucntval, scntval, 0, lcntval, flags);
		}
		break;
	case _PTHREAD_COND_SIG_init:
		// Compatibility for misbehaving applications that attempt to
		// destroy a statically initialized condition variable.
		//
		// fall through
	case _PTHREAD_COND_SIG_pristine:
	case _PTHREAD_COND_SIG_ulock:
		os_atomic_store(&cond->sig.val, _PTHREAD_NO_SIG, relaxed);
		res = 0;
		break;
	default:
		// TODO: PTHREAD_STRICT candidate
		break;
	}
	return res;
}

OS_ALWAYS_INLINE
static inline int
_pthread_psynch_cond_signal(pthread_cond_t *cond, bool broadcast,
		mach_port_t thread)
{
	uint32_t updateval;
	uint32_t diffgen;
	uint32_t ulval;

	uint64_t oldval64, newval64;
	uint32_t lcntval, ucntval, scntval;
	volatile uint64_t *c_lsseqaddr;
	volatile uint32_t *c_lseqcnt, *c_useqcnt, *c_sseqcnt;

	int retry_count = 0, uretry_count = 0;
	int ucountreset = 0;

	COND_GETSEQ_ADDR(cond, &c_lsseqaddr, &c_lseqcnt, &c_useqcnt, &c_sseqcnt);

	bool retry;
	do {
		retry = false;

		lcntval = *c_lseqcnt;
		ucntval = *c_useqcnt;
		scntval = *c_sseqcnt;
		diffgen = 0;
		ulval = 0;

		if (((lcntval & PTHRW_COUNT_MASK) == (scntval & PTHRW_COUNT_MASK)) ||
		    (thread == MACH_PORT_NULL && ((lcntval & PTHRW_COUNT_MASK) == (ucntval & PTHRW_COUNT_MASK)))) {
			/* validate it is spurious and return */
			oldval64 = (((uint64_t)scntval) << 32);
			oldval64 |= lcntval;
			newval64 = oldval64;

			if (!os_atomic_cmpxchg(c_lsseqaddr, oldval64, newval64, seq_cst)) {
				retry = true;
				continue;
			} else {
				return 0;
			}
		}

		if (thread) {
			break;
		}

		/* validate to eliminate spurious values, race snapshots */
		if (is_seqhigher((scntval & PTHRW_COUNT_MASK), (lcntval & PTHRW_COUNT_MASK))) {
			/* since ucntval may be newer, just redo */
			retry_count++;
			if (retry_count > 8192) {
				return EAGAIN;
			} else {
				sched_yield();
				retry = true;
				continue;
			}
		} else if (is_seqhigher((ucntval & PTHRW_COUNT_MASK), (lcntval & PTHRW_COUNT_MASK))) {
			/* since ucntval may be newer, just redo */
			uretry_count++;
			if (uretry_count > 8192) {
				/*
				 * U value if not used for a while can go out of sync
				 * set this to S value and try one more time.
				 */
				if (ucountreset != 0) {
					return EAGAIN;
				} else if (os_atomic_cmpxchg(c_useqcnt, ucntval, (scntval & PTHRW_COUNT_MASK), seq_cst)) {
					/* now the U is reset to S value */
					ucountreset = 1;
					uretry_count = 0;
				}
			}
			sched_yield();
			retry = true;
			continue;
		}

		if (is_seqlower(ucntval & PTHRW_COUNT_MASK, scntval & PTHRW_COUNT_MASK) != 0) {
			/* If U < S, set U = S+diff due to intr's TO, etc */
			ulval = (scntval & PTHRW_COUNT_MASK);
		} else {
			/* If U >= S, set U = U+diff due to intr's TO, etc */
			ulval = (ucntval & PTHRW_COUNT_MASK);
		}

		if (broadcast) {
			diffgen = diff_genseq(lcntval, ulval);
			// Set U = L
			ulval = (lcntval & PTHRW_COUNT_MASK);
		} else {
			ulval += PTHRW_INC;
		}

	} while (retry || !os_atomic_cmpxchg(c_useqcnt, ucntval, ulval, seq_cst));

	uint32_t flags = 0;
	if (cond->pshared == PTHREAD_PROCESS_SHARED) {
		flags |= _PTHREAD_MTX_OPT_PSHARED;
	}

	uint64_t cvlsgen = ((uint64_t)scntval << 32) | lcntval;

	if (broadcast) {
		// pass old U val so kernel will know the diffgen
		uint64_t cvudgen = ((uint64_t)ucntval << 32) | diffgen;
		updateval = __psynch_cvbroad(cond, cvlsgen, cvudgen, flags, NULL, 0, 0);
	} else {
		updateval = __psynch_cvsignal(cond, cvlsgen, ucntval, thread, NULL, 0, 0, flags);
	}

	if (updateval != (uint32_t)-1 && updateval != 0) {
		_pthread_cond_updateval(cond, NULL, 0, updateval);
	}

	return 0;
}

OS_ALWAYS_INLINE
static inline int
_pthread_ulock_cond_signal(pthread_cond_t *cond, bool broadcast,
		mach_port_t thread)
{
	pthread_ulock_cond_state_u *state = _pthread_ulock_cond_state(cond);

	pthread_ulock_cond_state_u oldstate, newstate;
	// release to pair with acquire after wait
	os_atomic_rmw_loop(&state->val, oldstate.val, newstate.val, release, {
		if (!oldstate.waiters || oldstate.waiters == oldstate.signal) {
			os_atomic_rmw_loop_give_up(return 0);
		}

		newstate = (pthread_ulock_cond_state_u){
			.seq = oldstate.seq + 1,
			.waiters = oldstate.waiters,
			.signal = broadcast ? oldstate.waiters :
					MIN(oldstate.signal + 1, oldstate.waiters),
		};
	});

	PTHREAD_TRACE(ulcond_signal, cond, oldstate.val, newstate.val, broadcast);

	// Priority hole: if we're pre-empted here, nobody else can signal the
	// waiter we took responsibility for signaling by incrementing the signal
	// count.

	if (oldstate.signal < oldstate.waiters) {
		uint32_t wake_op = UL_COMPARE_AND_WAIT | ULF_NO_ERRNO;
		if (broadcast) {
			wake_op |= ULF_WAKE_ALL;
		} else if (thread) {
			wake_op |= ULF_WAKE_THREAD;
		}

		for (;;) {
			int rc = __ulock_wake(wake_op, &state->seq, thread);
			if (rc < 0) {
				switch (-rc) {
				case EINTR:
					continue;
				case ENOENT:
					break;
				case EALREADY:
					if (!thread) {
						PTHREAD_INTERNAL_CRASH(0, "EALREADY from ulock_wake");
					}
					// Compatibility with psynch: promote to broadcast
					return pthread_cond_broadcast(cond);
				default:
					PTHREAD_INTERNAL_CRASH(-rc, "ulock_wake failure");
				}
			}
			break;
		}
	}

	return 0;
}

OS_ALWAYS_INLINE
static inline int
_pthread_cond_signal(pthread_cond_t *cond, bool broadcast, mach_port_t thread)
{
	uint32_t sig = _PTHREAD_COND_SIG_pristine;
	int res = _pthread_cond_check_init(cond, &sig);
	if (res != 0 || sig == _PTHREAD_COND_SIG_pristine) {
		return res;
	}

	switch (sig) {
	case _PTHREAD_COND_SIG_psynch:
		return _pthread_psynch_cond_signal(cond, broadcast, thread);
	case _PTHREAD_COND_SIG_ulock:
		return _pthread_ulock_cond_signal(cond, broadcast, thread);
	default:
		PTHREAD_INTERNAL_CRASH(sig, "impossible cond signature");
	}
}

/*
 * Signal a condition variable, waking up all threads waiting for it.
 */
PTHREAD_NOEXPORT_VARIANT
int
pthread_cond_broadcast(pthread_cond_t *cond)
{
	return _pthread_cond_signal(cond, true, MACH_PORT_NULL);
}

/*
 * Signal a condition variable, waking a specified thread.
 */
PTHREAD_NOEXPORT_VARIANT
int
pthread_cond_signal_thread_np(pthread_cond_t *cond, pthread_t thread)
{
	mach_port_t mp = MACH_PORT_NULL;
	if (thread) {
		mp = pthread_mach_thread_np((_Nonnull pthread_t)thread);
	}
	return _pthread_cond_signal(cond, false, mp);
}

/*
 * Signal a condition variable, waking only one thread.
 */
PTHREAD_NOEXPORT_VARIANT
int
pthread_cond_signal(pthread_cond_t *cond)
{
	return _pthread_cond_signal(cond, false, MACH_PORT_NULL);
}

static int
_pthread_psynch_cond_wait(pthread_cond_t *cond,
			pthread_mutex_t *mutex,
			const struct timespec *then,
			pthread_conformance_t conforming)
{
	uint32_t mtxgen, mtxugen, flags=0, updateval;
	uint32_t lcntval, ucntval, scntval;
	uint32_t nlval, ulval, savebits;
	volatile uint64_t *c_lsseqaddr;
	volatile uint32_t *c_lseqcnt, *c_useqcnt, *c_sseqcnt;
	uint64_t oldval64, newval64, mugen, cvlsgen;
	uint32_t *npmtx = NULL;

	COND_GETSEQ_ADDR(cond, &c_lsseqaddr, &c_lseqcnt, &c_useqcnt, &c_sseqcnt);

	do {
		lcntval = *c_lseqcnt;
		ucntval = *c_useqcnt;
		scntval = *c_sseqcnt;

		oldval64 = (((uint64_t)scntval) << 32);
		oldval64 |= lcntval;

		/* remove c and p bits on S word */
		savebits = scntval & PTH_RWS_CV_BITSALL;
		ulval = (scntval & PTHRW_COUNT_MASK);
		nlval = lcntval + PTHRW_INC;
		newval64 = (((uint64_t)ulval) << 32);
		newval64 |= nlval;
	} while (!os_atomic_cmpxchg(c_lsseqaddr, oldval64, newval64, seq_cst));

	cond->busy = mutex;

	int res = _pthread_mutex_droplock(mutex, &flags, &npmtx, &mtxgen, &mtxugen);

	/* TBD: cases are for normal (non owner for recursive mutex; error checking)*/
	if (res != 0) {
		return EINVAL;
	}
	if ((flags & _PTHREAD_MTX_OPT_NOTIFY) == 0) {
		npmtx = NULL;
		mugen = 0;
	} else {
		mugen = ((uint64_t)mtxugen << 32) | mtxgen;
	}
	flags &= ~_PTHREAD_MTX_OPT_MUTEX;	/* reset the mutex bit as this is cvar */

	cvlsgen = ((uint64_t)(ulval | savebits)<< 32) | nlval;

	// SUSv3 requires pthread_cond_wait to be a cancellation point
	if (conforming == PTHREAD_CONFORM_UNIX03_CANCELABLE) {
		pthread_cleanup_push(_pthread_psynch_cond_cleanup, (void *)cond);
		updateval = __psynch_cvwait(cond, cvlsgen, ucntval, (pthread_mutex_t *)npmtx, mugen, flags, (int64_t)(then->tv_sec), (int32_t)(then->tv_nsec));
		pthread_testcancel();
		pthread_cleanup_pop(0);
	} else {
		updateval = __psynch_cvwait(cond, cvlsgen, ucntval, (pthread_mutex_t *)npmtx, mugen, flags, (int64_t)(then->tv_sec), (int32_t)(then->tv_nsec));
	}

	if (updateval == (uint32_t)-1) {
		int err = errno;
		switch (err & 0xff) {
		case ETIMEDOUT:
			res = ETIMEDOUT;
			break;
		case EINTR:
			// spurious wakeup (unless canceled)
			res = 0;
			break;
		default:
			res = EINVAL;
			break;
		}

		// add unlock ref to show one less waiter
		_pthread_cond_updateval(cond, mutex, err, 0);
	} else if (updateval != 0) {
		// Successful wait
		// The return due to prepost and might have bit states
		// update S and return for prepo if needed
		_pthread_cond_updateval(cond, mutex, 0, updateval);
	}

	pthread_mutex_lock(mutex);

	return res;
}

struct pthread_ulock_cond_cancel_ctx_s {
	pthread_cond_t *cond;
	pthread_mutex_t *mutex;
};

static int
_pthread_ulock_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		const struct timespec *then, pthread_conformance_t conforming)
{
	bool cancelable = (conforming == PTHREAD_CONFORM_UNIX03_CANCELABLE);

	uint64_t timeout_ns = 0;
	if (then->tv_sec || then->tv_nsec) {
		// psynch compatibility: cast and bitwise-truncate tv_nsec
		uint64_t fraction_ns = ((uint32_t)then->tv_nsec) & 0x3fffffff;
		if (os_mul_and_add_overflow(then->tv_sec, NSEC_PER_SEC, fraction_ns,
				&timeout_ns)) {
			// saturate (can't wait longer than 584 years...)
			timeout_ns = UINT64_MAX;
		}
	}

	pthread_ulock_cond_state_u *state = _pthread_ulock_cond_state(cond);

	pthread_ulock_cond_state_u origstate = {
		.val = os_atomic_add(&state->val, _PTHREAD_COND_WAITERS_INC, relaxed)
	};

	int rc = _pthread_mutex_ulock_unlock(mutex);
	if (rc) {
		return _pthread_ulock_cond_wait_complete(state, NULL, rc);
	}

	PTHREAD_TRACE(ulcond_wait, cond, origstate.val, timeout_ns, 0);

	do {
		const uint32_t wait_op = UL_COMPARE_AND_WAIT | ULF_NO_ERRNO;
		if (cancelable) {
			struct pthread_ulock_cond_cancel_ctx_s ctx = {
				.cond = cond,
				.mutex = mutex,
			};
			pthread_cleanup_push(_pthread_ulock_cond_cleanup, &ctx);
			rc = __ulock_wait2(wait_op | ULF_WAIT_CANCEL_POINT, &state->seq,
					origstate.seq, timeout_ns, 0);
			pthread_testcancel();
			pthread_cleanup_pop(0);
		} else {
			rc = __ulock_wait2(wait_op, &state->seq, origstate.seq, timeout_ns, 0);
		}
		if (rc < 0) {
			switch (-rc) {
			case EFAULT:
				continue;
			case EINTR:
				// "These functions shall not return an error code of [EINTR]."
				// => promote to spurious wake-up
				rc = 0;
				goto out;
			case ETIMEDOUT:
				rc = ETIMEDOUT;
				goto out;
			default:
				PTHREAD_INTERNAL_CRASH(-rc, "ulock_wait failure");
			}
		} else {
			// XXX for now don't care about other waiters
			rc = 0;
		}
	} while (os_atomic_load(&state->seq, relaxed) == origstate.seq);

out:
	return _pthread_ulock_cond_wait_complete(state, mutex, rc);
}

static int
_pthread_ulock_cond_wait_complete(pthread_ulock_cond_state_u *state,
		pthread_mutex_t *mutex, int rc)
{
	if (mutex) {
		// XXX Check this return value?  Historically we haven't, but if rc == 0
		// we could promote the return value to this one.
		_pthread_mutex_ulock_lock(mutex, false);
	}

	pthread_ulock_cond_state_u oldstate, newstate;
	// acquire to pair with release upon signal
	os_atomic_rmw_loop(&state->val, oldstate.val, newstate.val, acquire, {
		newstate = (pthread_ulock_cond_state_u){
			.seq = oldstate.seq,
			.waiters = oldstate.waiters - 1,
			.signal = oldstate.signal ? oldstate.signal - 1 : 0,
		};
	});

	return rc;
}

/*
 * Suspend waiting for a condition variable.
 * If conformance is not cancelable, we skip the pthread_testcancel(),
 * but keep the remaining conforming behavior.
 */
PTHREAD_NOEXPORT OS_NOINLINE
int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			const struct timespec *abstime, int isRelative,
			pthread_conformance_t conforming)
{
	int res;
	struct timespec then = { 0, 0 };
	bool timeout_elapsed = false;

	if (!_pthread_mutex_check_signature(mutex) &&
			!_pthread_mutex_check_signature_init(mutex)) {
		return EINVAL;
	}

	bool ulock = _pthread_mutex_uses_ulock(mutex);
	uint32_t sig = ulock ? _PTHREAD_COND_SIG_ulock : _PTHREAD_COND_SIG_psynch;
	res = _pthread_cond_check_init(cond, &sig);
	if (res != 0) {
		return res;
	}

	if (conforming == PTHREAD_CONFORM_UNIX03_CANCELABLE) {
		pthread_testcancel();
	}

	/* send relative time to kernel */
	if (abstime) {
		if (abstime->tv_nsec < 0 || abstime->tv_nsec >= NSEC_PER_SEC) {
			// TODO: PTHREAD_STRICT candidate
			return EINVAL;
		}

		if (isRelative == 0) {
			struct timespec now;
			struct timeval tv;
			__gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &now);

			if ((abstime->tv_sec == now.tv_sec) ?
				(abstime->tv_nsec <= now.tv_nsec) :
				(abstime->tv_sec < now.tv_sec)) {
				timeout_elapsed = true;
			} else {
				/* Compute relative time to sleep */
				then.tv_nsec = abstime->tv_nsec - now.tv_nsec;
				then.tv_sec = abstime->tv_sec - now.tv_sec;
				if (then.tv_nsec < 0) {
					then.tv_nsec += NSEC_PER_SEC;
					then.tv_sec--;
				}
			}
		} else {
			then.tv_sec = abstime->tv_sec;
			then.tv_nsec = abstime->tv_nsec;
			if ((then.tv_sec == 0) && (then.tv_nsec == 0)) {
				timeout_elapsed = true;
			}
		}
	}

	if (!ulock && cond->busy != NULL && cond->busy != mutex) {
		// TODO: PTHREAD_STRICT candidate
		return EINVAL;
	}

	/*
	 * If timeout is known to have elapsed, we still need to unlock and
	 * relock the mutex to allow other waiters to get in line and
	 * modify the condition state.
	 */
	if (timeout_elapsed) {
		res = pthread_mutex_unlock(mutex);
		if (res != 0) {
			return res;
		}
		res = pthread_mutex_lock(mutex);
		if (res != 0) {
			return res;
		}

		return ETIMEDOUT;
	}

	if (ulock) {
		return _pthread_ulock_cond_wait(cond, mutex, &then, conforming);
	} else {
		return _pthread_psynch_cond_wait(cond, mutex, &then, conforming);
	}
}

static void
_pthread_ulock_cond_cleanup(void *arg)
{
	struct pthread_ulock_cond_cancel_ctx_s *ctx = arg;
	pthread_ulock_cond_state_u *state = _pthread_ulock_cond_state(ctx->cond);

	(void)_pthread_ulock_cond_wait_complete(state, ctx->mutex, 0);

	// "A thread that has been unblocked because it has been canceled while
	// blocked in a call to pthread_cond_timedwait() or pthread_cond_wait()
	// shall not consume any condition signal that may be directed concurrently
	// at the condition variable if there are other threads blocked on the
	// condition variable."
	//
	// Since we have no way to know if we've eaten somebody else's signal, just
	// signal again pessimistically.
	pthread_cond_signal(ctx->cond);
}

static void
_pthread_psynch_cond_cleanup(void *arg)
{
	pthread_cond_t *cond = (pthread_cond_t *)arg;
	pthread_t thread = pthread_self();
	pthread_mutex_t *mutex;

// 4597450: begin
	if (!thread->canceled) {
		return;
	}
// 4597450: end

	mutex = cond->busy;

	// add unlock ref to show one less waiter
	_pthread_cond_updateval(cond, mutex, thread->cancel_error, 0);

	/*
	** Can't do anything if this fails -- we're on the way out
	*/
	if (mutex != NULL) {
		(void)pthread_mutex_lock(mutex);
	}
}

static void
_pthread_cond_updateval(pthread_cond_t *cond, pthread_mutex_t *mutex,
		int error, uint32_t updateval)
{
	int needclearpre;

	uint32_t diffgen, nsval;
	uint64_t oldval64, newval64;
	uint32_t lcntval, ucntval, scntval;
	volatile uint64_t *c_lsseqaddr;
	volatile uint32_t *c_lseqcnt, *c_useqcnt, *c_sseqcnt;

	if (error != 0) {
		updateval = PTHRW_INC;
		if (error & ECVCLEARED) {
			updateval |= PTH_RWS_CV_CBIT;
		}
		if (error & ECVPREPOST) {
			updateval |= PTH_RWS_CV_PBIT;
		}
	}

	COND_GETSEQ_ADDR(cond, &c_lsseqaddr, &c_lseqcnt, &c_useqcnt, &c_sseqcnt);

	do {
		lcntval = *c_lseqcnt;
		ucntval = *c_useqcnt;
		scntval = *c_sseqcnt;
		nsval = 0;
		needclearpre = 0;

		diffgen = diff_genseq(lcntval, scntval); // pending waiters

		oldval64 = (((uint64_t)scntval) << 32);
		oldval64 |= lcntval;

		PTHREAD_TRACE(psynch_cvar_updateval | DBG_FUNC_START, cond, oldval64,
				updateval, 0);

		if (diffgen <= 0 && !is_rws_pbit_set(updateval)) {
			/* TBD: Assert, should not be the case */
			/* validate it is spurious and return */
			newval64 = oldval64;
		} else {
			// update S by one

			// update scntval with number of expected returns and bits
			nsval = (scntval & PTHRW_COUNT_MASK) + (updateval & PTHRW_COUNT_MASK);
			// set bits
			nsval |= ((scntval & PTH_RWS_CV_BITSALL) | (updateval & PTH_RWS_CV_BITSALL));

			// if L==S and c&p bits are set, needs clearpre
			if (((nsval & PTHRW_COUNT_MASK) == (lcntval & PTHRW_COUNT_MASK)) &&
			    ((nsval & PTH_RWS_CV_BITSALL) == PTH_RWS_CV_BITSALL)) {
				// reset p bit but retain c bit on the sword
				nsval &= PTH_RWS_CV_RESET_PBIT;
				needclearpre = 1;
			}

			newval64 = (((uint64_t)nsval) << 32);
			newval64 |= lcntval;
		}
	} while (!os_atomic_cmpxchg(c_lsseqaddr, oldval64, newval64, seq_cst));

	PTHREAD_TRACE(psynch_cvar_updateval | DBG_FUNC_END, cond, newval64,
			(uint64_t)diffgen << 32 | needclearpre, 0);

	if (diffgen > 0) {
		// if L == S, then reset associated mutex
		if ((nsval & PTHRW_COUNT_MASK) == (lcntval & PTHRW_COUNT_MASK)) {
			cond->busy = NULL;
		}
	}

	if (needclearpre) {
		uint32_t flags = 0;
		if (cond->pshared == PTHREAD_PROCESS_SHARED) {
			flags |= _PTHREAD_MTX_OPT_PSHARED;
		}
		(void)__psynch_cvclrprepost(cond, lcntval, ucntval, nsval, 0, lcntval, flags);
	}
}

#endif /* !BUILDING_VARIANT ] */

PTHREAD_NOEXPORT_VARIANT
int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	_pthread_lock_init(&cond->lock);
	return _pthread_cond_init(cond, attr, _PTHREAD_COND_SIG_pristine);
}

