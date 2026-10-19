/*
 * Copyright 2010, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_CALLOUT_H_
#define _FBSD_COMPAT_SYS_CALLOUT_H_


#include <sys/haiku-module.h>
#include <sys/_callout.h>


#define	CALLOUT_MPSAFE			0x0008 /* deprecated */
#define	CALLOUT_RETURNUNLOCKED	0x0010 /* handler returns with mtx unlocked */


void callout_init(struct callout *c, int mpsafe);
void callout_init_mtx(struct callout *c, struct mtx *mutex, int flags);
int callout_schedule(struct callout *c, int _ticks);
int callout_reset(struct callout *c, int _ticks, void (*func)(void *), void *arg);
int callout_pending(struct callout *c);
int callout_active(struct callout *c);

#define callout_reset_on(c, to_ticks, fn, arg, cpu) \
	callout_reset(c, to_ticks, fn, arg)

#define	callout_drain(c)	_callout_stop_safe(c, 1)
#define	callout_stop(c)		_callout_stop_safe(c, 0)
int	_callout_stop_safe(struct callout *c, int safe);



#ifdef PD_LINUXKPI
#include <sys/kernel.h>

// sbintime callouts round to the tick, at least one
static inline int
callout_reset_sbt(struct callout *c, long long sbt, long long pr,
	void (*func)(void *), void *arg, int flags)
{
	long long t = (sbt * hz) >> 32;
	return callout_reset(c, t > 0 ? (int)t : 1, func, arg);
}

#define callout_schedule_sbt(c, sbt, pr, flags) \
	callout_schedule(c, (int)((((long long)(sbt) * hz) >> 32) > 0 ? (((long long)(sbt) * hz) >> 32) : 1))
// haiku derives active from the running callout, so there is no flag to clear
#define callout_deactivate(c) do { } while (0)
#endif

#endif	/* _FBSD_COMPAT_SYS_CALLOUT_H_ */
