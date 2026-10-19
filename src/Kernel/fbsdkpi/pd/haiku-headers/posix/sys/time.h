/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_TIME_H
#define _SYS_TIME_H


#include <sys/types.h>


struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* microseconds */
};

#include <sys/select.h>
	/* circular dependency: fd_set needs to be defined and the
	 * select prototype exported by this file, but <sys/select.h>
	 * needs struct timeval.
	 */

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

struct itimerval {
	struct timeval it_interval;
	struct timeval it_value;
};

#define ITIMER_REAL		1	/* real time */
#define ITIMER_VIRTUAL	2	/* process virtual time */
#define ITIMER_PROF		3	/* both */


#ifdef __cplusplus
extern "C" {
#endif

extern int	getitimer(int which, struct itimerval *value);
extern int	setitimer(int which, const struct itimerval *value, struct itimerval *oldValue);
extern int	gettimeofday(struct timeval *tv, void *tz);

extern int	utimes(const char *path, const struct timeval times[2]);
	/* legacy */

#ifdef __cplusplus
}
#endif

/* BSDish macros operating on timeval structs */
#define timeradd(a, b, res)								\
	do {												\
		(res)->tv_sec = (a)->tv_sec + (b)->tv_sec;		\
		(res)->tv_usec = (a)->tv_usec + (b)->tv_usec;	\
		if ((res)->tv_usec >= 1000000) {				\
			(res)->tv_usec -= 1000000;					\
			(res)->tv_sec++;							\
		}												\
	} while (0)
#define timersub(a, b, res)								\
	do {												\
		(res)->tv_sec = (a)->tv_sec - (b)->tv_sec;		\
		(res)->tv_usec = (a)->tv_usec - (b)->tv_usec;	\
		if ((res)->tv_usec < 0) {						\
			(res)->tv_usec += 1000000;					\
			(res)->tv_sec--;							\
		}												\
	} while (0)
#define timerclear(a)	((a)->tv_sec = (a)->tv_usec = 0)
#define timerisset(a)	((a)->tv_sec != 0 || (a)->tv_usec != 0)
#define timercmp(a, b, cmp)	((a)->tv_sec == (b)->tv_sec \
		? (a)->tv_usec cmp (b)->tv_usec : (a)->tv_sec cmp (b)->tv_sec)

#endif	/* _SYS_TIME_H */
