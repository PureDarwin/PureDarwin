/*
 * Copyright 2005-2011, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _TIME_H_
#define _TIME_H_


#include <locale_t.h>
#include <sys/types.h>


struct sigevent;	/* defined in <signal.h> */


typedef __haiku_int32 clock_t;
typedef __haiku_int32 suseconds_t;
typedef __haiku_uint32 useconds_t;

#if defined(__i386__) && !defined(__x86_64__)
typedef __haiku_int32 time_t;
#else
typedef __haiku_int64 time_t;
#endif


#define CLOCKS_PER_SEC	1000000
#define CLK_TCK			CLOCKS_PER_SEC
#define TIME_UTC		1

#define MAX_TIMESTR		70
	/* maximum length of a string returned by asctime(), and ctime() */

#define CLOCK_MONOTONIC				((clockid_t)0)
	/* system-wide monotonic clock (aka system time) */
#define CLOCK_REALTIME				((clockid_t)-1)
	/* system-wide real time clock */
#define CLOCK_PROCESS_CPUTIME_ID	((clockid_t)-2)
	/* clock measuring the used CPU time of the current process */
#define CLOCK_THREAD_CPUTIME_ID		((clockid_t)-3)
	/* clock measuring the used CPU time of the current thread */

#define TIMER_ABSTIME				1	/* absolute timer flag */


struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* and nanoseconds */
};

struct itimerspec {
	struct timespec it_interval;
	struct timespec it_value;
};

#ifndef PD_LINUXKPI
struct tm {
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;	/* day of month (1 to 31) */
	int	tm_mon;		/* months since January (0 to 11) */
	int	tm_year;	/* years since 1900 */
	int	tm_wday;	/* days since Sunday (0 to 6, Sunday = 0, ...) */
	int	tm_yday;	/* days since January 1 (0 to 365) */
	int	tm_isdst;	/* daylight savings time (0 == no, >0 == yes, <0 == has to be calculated */
	int tm_gmtoff;	/* timezone offset to GMT */
	char *tm_zone;	/* timezone name */
};
#endif


/* special timezone support */
extern char *tzname[2];
extern int 	daylight;
extern long	timezone;


#ifdef __cplusplus
extern "C" {
#endif

extern clock_t		clock(void);
extern double		difftime(time_t time1, time_t time2);
extern time_t		mktime(struct tm *tm);
extern time_t		time(time_t *timer);
extern char			*asctime(const struct tm *tm);
extern char			*asctime_r(const struct tm *timep, char *buffer);
extern char			*ctime(const time_t *timer);
extern char			*ctime_r(const time_t *timer, char *buffer);
extern struct tm	*gmtime(const time_t *timer);
extern struct tm	*gmtime_r(const time_t *timer, struct tm *tm);
extern struct tm	*localtime(const time_t *timer);
extern struct tm	*localtime_r(const time_t *timer, struct tm *tm);
extern int			nanosleep(const struct timespec *, struct timespec *);
extern size_t		strftime(char *buffer, size_t maxSize, const char *format,
						const struct tm *tm);
extern size_t		strftime_l(char *buffer, size_t maxSize, const char *format,
						const struct tm *tm, locale_t locale);
extern char 		*strptime(const char *buf, const char *format, struct tm *tm);

/* clock functions */
int		clock_getres(clockid_t clockID, struct timespec* resolution);
int		clock_gettime(clockid_t clockID, struct timespec* _time);
int		clock_settime(clockid_t clockID, const struct timespec* _time);
int		clock_nanosleep(clockid_t clockID, int flags,
			const struct timespec* _time, struct timespec* remainingTime);
int		clock_getcpuclockid(pid_t pid, clockid_t* _clockID);

/* timer functions */
int		timer_create(clockid_t clockID, struct sigevent* event,
			timer_t* timerID);
#ifndef PD_LINUXKPI
int		timer_delete(timer_t timerID);
#endif
int		timer_gettime(timer_t timerID, struct itimerspec* value);
int		timer_settime(timer_t timerID, int flags,
			const struct itimerspec* value, struct itimerspec* oldValue);
int		timer_getoverrun(timer_t timerID);

/* C11 timespec */
int		timespec_get(struct timespec *ts, int base);

/* special timezone support */
extern void tzset(void);

/* non-POSIX */
extern int	stime(const time_t *t);

#ifdef __cplusplus
}
#endif


#endif	/* _TIME_H_ */
