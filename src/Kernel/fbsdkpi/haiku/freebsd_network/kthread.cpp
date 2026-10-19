/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <thread.h>

extern "C" {
#include <sys/systm.h>
#include <sys/kthread.h>
}


int
kthread_add(void (*func)(void *), void *arg, void* p,
	struct thread **newtdp, int flags, int pages, const char *fmt, ...)
{
	va_list ap;
	char name[B_OS_NAME_LENGTH];
	va_start(ap, fmt);
	vsnprintf(name, sizeof(name), fmt, ap);
	va_end(ap);

	thread_id id = spawn_kernel_thread((status_t (*)(void *))func, /* HACK! */
		name, B_NORMAL_PRIORITY, arg);
	if (id < 0)
		return id;
	if (newtdp != NULL) {
		intptr_t thread = id;
		*newtdp = (struct thread*)thread;
	}
	return 0;
}


void
sched_prio(struct thread* td, u_char prio)
{
	uintptr_t tdi = (uintptr_t)td;
	set_thread_priority((thread_id)tdi, prio);
}


void
sched_add(struct thread* td, int /* flags */)
{
	uintptr_t tdi = (uintptr_t)td;
	resume_thread((thread_id)tdi);
}


void
kthread_exit()
{
	thread_exit();
}
