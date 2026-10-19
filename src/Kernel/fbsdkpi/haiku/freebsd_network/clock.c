/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <OS.h>
#include <compat/sys/kernel.h>
#include <compat/sys/time.h>


int32_t
_get_ticks()
{
	return USEC_2_TICKS(system_time());
}


void
getmicrouptime(struct timeval *tvp)
{
	bigtime_t usecs = system_time();
	tvp->tv_sec = usecs / 1000000;
	tvp->tv_usec = usecs % 1000000;
}
