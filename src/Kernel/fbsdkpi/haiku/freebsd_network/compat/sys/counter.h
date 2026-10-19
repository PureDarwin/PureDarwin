/*
 * Copyright 2017-2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_COUNTER_H_
#define _FBSD_COMPAT_SYS_COUNTER_H_

#include <machine/atomic.h>
#include <sys/malloc.h>


/* FreeBSD does not use atomics: it has a per-CPU data storage structure
 * that it adds to whenever someone calls add(), and then only locks and
 * coalesces it whenever fetch() is called. This means that on some
 * architectures (e.g. x86_64), adding to the counter is one instruction.
 *
 * However, this seems to be for the most part overengineering, as its
 * only uses seem to be statistical counting in semi-performance-critical paths.
 * Axel noted in #12328 that there's a potential way to implement FreeBSD's
 * method on Haiku using cpu_ent, but that atomics were "perfectly fine",
 * so we will go with that for now.
 */


typedef uint64_t* counter_u64_t;


static inline counter_u64_t
counter_u64_alloc(int wait)
{
	return (counter_u64_t)_kernel_malloc(sizeof(uint64_t), wait | M_ZERO);
}


static inline void
counter_u64_free(counter_u64_t c)
{
	_kernel_free(c);
}


static inline void
counter_u64_add(counter_u64_t c, int64_t v)
{
	atomic_add64((int64*)c, v);
}


static inline uint64_t
counter_u64_fetch(counter_u64_t c)
{
	return atomic_get64((int64*)c);
}


static inline void
counter_u64_zero(counter_u64_t c)
{
	atomic_set64((int64*)c, 0);
}


static inline void
counter_enter()
{
	// unneeded; counters are atomic
}


static inline void
counter_exit()
{
	// unneeded; counters are atomic
}


static inline void
counter_u64_add_protected(counter_u64_t c, int64_t v)
{
	// counters are atomic
	counter_u64_add(c, v);
}


#endif
