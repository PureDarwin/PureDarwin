/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_PCPU_H_
#define _FBSD_COMPAT_SYS_PCPU_H_


#include <OS.h>
#include <sys/smp.h>


struct thread;

#ifdef PD_LINUXKPI
// linuxkpi keeps per-thread state in struct thread, so give each thread one
struct thread *pd_curthread(void);
#define curthread pd_curthread()
#else
#define curthread ((struct thread*)NULL)
	/* NOTE: Dereferencing curthread will crash, which is intentional. There is
	   no FreeBSD compatible struct thread and Haiku's should not be used as it
	   is only valid for the current thread or with proper locking. Currently
	   only priv_check() expects a struct thread parameter and ignores it. Using
	   NULL will show us when other uses appear. */
#endif


#endif /* _FBSD_COMPAT_SYS_PCPU_H_ */
