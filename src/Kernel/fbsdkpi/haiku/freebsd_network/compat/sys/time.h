/*
 * Copyright 2020-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 * $FreeBSD$
 */
#ifndef _FBSD_COMPAT_SYS_TIME_H_
#define _FBSD_COMPAT_SYS_TIME_H_


#include <posix/sys/time.h>

#include <sys/_timeval.h>
#include <sys/types.h>


#define TICKS_2_USEC(t) ((1000000*(bigtime_t)t) / hz)
#define USEC_2_TICKS(t) (((bigtime_t)t*hz) / 1000000)

#define TICKS_2_MSEC(t) ((1000*(bigtime_t)t) / hz)
#define MSEC_2_TICKS(t) (((bigtime_t)t*hz) / 1000)

#define time_uptime		(system_time()/(1000*1000))


/* Operations on timevals. */
#define	timevalclear(tvp)		((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define	timevalisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timevalcmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
		((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
		((tvp)->tv_sec cmp (uvp)->tv_sec))


void	getmicrouptime(struct timeval *tvp);

int	ppsratecheck(struct timeval *, int *, int);
int	ratecheck(struct timeval *, const struct timeval *);
void	timevaladd(struct timeval *t1, const struct timeval *t2);
void	timevalsub(struct timeval *t1, const struct timeval *t2);


#endif /* _FBSD_COMPAT_SYS_TIME_H_ */
