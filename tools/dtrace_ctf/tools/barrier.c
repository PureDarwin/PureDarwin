/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file implements a barrier, a synchronization primitive designed to allow
 * threads to wait for each other at given points.  Barriers are initialized
 * with a given number of threads, n, using barrier_init().  When a thread calls
 * barrier_wait(), that thread blocks until n - 1 other threads reach the
 * barrier_wait() call using the same barrier_t.  When n threads have reached
 * the barrier, they are all awakened and sent on their way.  One of the threads
 * returns from barrier_wait() with a return code of 1; the remaining threads
 * get a return code of 0.
 */

#include <pthread.h>

#include "barrier.h"

#if !__APPLE__
#include <stdatomic.h>
#include <stdlib.h>

struct dispatch_semaphore_s {
    atomic_int count;
};

static dispatch_semaphore_t dispatch_semaphore_create(int count) {
    dispatch_semaphore_t sema = (dispatch_semaphore_t) calloc(1, sizeof(struct dispatch_semaphore_s));
    sema->count = count;
    return sema;
}

#define DISPATCH_TIME_FOREVER 0
static void dispatch_semaphore_wait(dispatch_semaphore_t sema, int unused) {
    int old_count = atomic_fetch_sub(&sema->count, 1) - 1;
    if (old_count <= 0) {
        for (int value = atomic_read(&sema->count); ; value = atomic_read(&sema->count)) {
            if (value >= 0) break;
            thread_yield();
        }
    }
}

static void dispatch_semaphore_signal(dispatch_semaphore_t sema) {
    atomic_fetch_add(&sema->count, 1);
}
#endif

void
barrier_init(barrier_t *bar, int nthreads)
{
	pthread_mutex_init(&bar->bar_lock, NULL);
	bar->bar_sem = dispatch_semaphore_create(0);

	bar->bar_numin = 0;
	bar->bar_nthr = nthreads;
}

int
barrier_wait(barrier_t *bar)
{
	pthread_mutex_lock(&bar->bar_lock);

	if (++bar->bar_numin < bar->bar_nthr) {
		pthread_mutex_unlock(&bar->bar_lock);
		dispatch_semaphore_wait(bar->bar_sem, DISPATCH_TIME_FOREVER);

		return (0);

	} else {
		int i;

		/* reset for next use */
		bar->bar_numin = 0;
		for (i = 1; i < bar->bar_nthr; i++)
			dispatch_semaphore_signal(bar->bar_sem);
		pthread_mutex_unlock(&bar->bar_lock);

		return (1);
	}
}
