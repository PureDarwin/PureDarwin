/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_IFLIB_SYS_KTHREAD_H_
#define _FBSD_COMPAT_IFLIB_SYS_KTHREAD_H_

#include <sys/pcpu.h>


#define SRQ_BORING 0

void sched_prio(struct thread* td, u_char prio);
void sched_add(struct thread* td, int flags);

int kthread_add(void (*func)(void *), void *arg, void* p,
	struct thread** newtdp, int flags, int pages, const char* fmt, ...);
void kthread_exit();

#define thread_lock(td)
#define thread_unlock(td)


#endif /* _FBSD_COMPAT_IFLIB_SYS_KTHREAD_H_ */
