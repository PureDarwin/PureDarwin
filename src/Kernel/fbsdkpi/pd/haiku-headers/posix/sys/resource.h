/*
 * Copyright 2003-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H


#include <config/types.h>

#include <sys/cdefs.h>
#include <sys/time.h>


/* getrlimit()/setrlimit() definitions */

typedef __haiku_addr_t rlim_t;

struct rlimit {
	rlim_t	rlim_cur;		/* current soft limit */
	rlim_t	rlim_max;		/* hard limit */
};

/* ToDo: the only supported mode is RLIMIT_NOFILE right now */
#define RLIMIT_CORE		0	/* size of the core file */
#define RLIMIT_CPU		1	/* CPU time per team */
#define RLIMIT_DATA		2	/* data segment size */
#define RLIMIT_FSIZE	3	/* file size */
#define RLIMIT_NOFILE	4	/* number of open files */
#define RLIMIT_STACK	5	/* stack size */
#define RLIMIT_AS		6	/* address space size */
/* Haiku-specifics */
#define RLIMIT_NOVMON	7	/* number of open vnode monitors */

#define RLIM_NLIMITS	8	/* number of resource limits */

#define RLIM_INFINITY	(0xffffffffUL)
#define RLIM_SAVED_MAX	RLIM_INFINITY
#define RLIM_SAVED_CUR	RLIM_INFINITY


/* getrusage() definitions */

struct rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */

	/* unused, only for compatibility with other systems */
	long	ru_maxrss;
	long	ru_ixrss;
	long	ru_idrss;
	long	ru_isrss;
	long	ru_minflt;
	long	ru_majflt;
	long	ru_nswap;
	long	ru_inblock;
	long	ru_oublock;
	long	ru_msgsnd;
	long	ru_msgrcv;
	long	ru_nsignals;
	long	ru_nvcsw;
	long	ru_nivcsw;
};

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN -1


/* getpriority()/setpriority() definitions */

#define PRIO_PROCESS	0
#define PRIO_PGRP		1
#define PRIO_USER		2


__BEGIN_DECLS

extern int getrusage(int who, struct rusage *rusage);

extern int getrlimit(int resource, struct rlimit * rlp);
extern int setrlimit(int resource, const struct rlimit * rlp);

extern int getpriority(int which, id_t who);
extern int setpriority(int which, id_t who, int priority);

__END_DECLS

#endif	/* _SYS_RESOURCE_H */
