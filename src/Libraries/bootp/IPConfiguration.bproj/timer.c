/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * timer.c
 * - timer functions
 */

/* 
 * Modification History
 *
 * May 8, 2000	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <syslog.h>
#include <notify.h>
#include <notify_keys.h>
#ifndef kNotifyClockSet
#define kNotifyClockSet "com.apple.system.clock_set"
#endif
#include "globals.h"
#include "util.h"
#include "timer.h"

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFDate.h>

struct timer_callout {
    char *		name;
    timer_func_t *	func;
    void *		arg1;
    void *		arg2;
    void *		arg3;
    CFRunLoopTimerRef	timer_source;
    boolean_t		enabled;
    uint32_t		time_generation;
};

#ifdef TEST_ARP_SESSION
#undef my_log
#define my_log	syslog
#endif /* TEST_ARP_SESSION */

/*
 * Use notify(3) APIs to detect date/time changes
 */

static boolean_t	S_time_change_registered;
static int		S_time_change_token;
static uint32_t		S_time_generation;

static void
timer_cancel_common(timer_callout_t * callout);

static void
timer_notify_check(void)
{
    int		check = 0;
    int		status;

    if (S_time_change_registered == FALSE) {
	return;
    }
    status = notify_check(S_time_change_token, &check);
    if (status != NOTIFY_STATUS_OK) {
	my_log(LOG_NOTICE, "timer: notify_check failed with %d", status);
    }
    else if (check != 0) {
	S_time_generation++;
    }
    return;
}

static void
timer_register_time_change(void)
{
    int		status;

    if (S_time_change_registered) {
	return;
    }
    status = notify_register_check(kNotifyClockSet, &S_time_change_token);
    if (status != NOTIFY_STATUS_OK) {
	my_log(LOG_NOTICE, "timer: notify_register_check(%s) failed, %d",
	       kNotifyClockSet, status);
	return;
    }
    S_time_change_registered = TRUE;

    /* throw away the first check, since it always says check=1 */
    timer_notify_check();
    return;
}

/**
 ** Timer callout functions
 **/
static const char *
timer_name(timer_callout_t * callout)
{
    return (callout->name);
}

static void 
timer_callout_process(CFRunLoopTimerRef timer_source, void * info)
{
    timer_callout_t *	callout = (timer_callout_t *)info;

    if (callout->func && callout->enabled) {
	callout->enabled = FALSE;
	(*callout->func)(callout->arg1, callout->arg2, callout->arg3);
    }
    return;
}

timer_callout_t *
timer_callout_init(const char * name)
{
    timer_callout_t * callout;

    callout = malloc(sizeof(*callout));
    if (callout == NULL)
	return (NULL);
    bzero(callout, sizeof(*callout));
    callout->name = strdup(name);
    timer_register_time_change();
    callout->time_generation = S_time_generation;
    return (callout);
}

void
timer_callout_free(timer_callout_t * * callout_p)
{
    timer_callout_t * callout = *callout_p;

    if (callout == NULL)
	return;

    timer_cancel_common(callout);
    free(callout->name);
    free(callout);
    *callout_p = NULL;
    return;
}

static int
timer_callout_set_common(timer_callout_t * callout,
			 CFAbsoluteTime wakeup_time, timer_func_t * func,
			 void * arg1, void * arg2, void * arg3)
{
    CFRunLoopTimerContext 	context =  { 0, NULL, NULL, NULL, NULL };

    if (callout == NULL) {
	return (0);
    }
    timer_cancel_common(callout);
    if (func == NULL) {
	return (0);
    }
    callout->func = func;
    callout->arg1 = arg1;
    callout->arg2 = arg2;
    callout->arg3 = arg3;
    callout->enabled = TRUE;
    callout->time_generation = S_time_generation;
    context.info = callout;
    callout->timer_source 
	= CFRunLoopTimerCreate(NULL, wakeup_time,
			       0.0, 0, 0,
			       timer_callout_process,
			       &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), callout->timer_source,
		      kCFRunLoopDefaultMode);
    return (1);
}

int
timer_callout_set_absolute(timer_callout_t * callout,
			   CFAbsoluteTime wakeup_time, timer_func_t * func,
			   void * arg1, void * arg2, void * arg3)
{
    CFAbsoluteTime	now = CFAbsoluteTimeGetCurrent();

    my_log(LOG_DEBUG, "timer(%s): wakeup time is (%0.09g) %0.09g",
	   timer_name(callout),
	   wakeup_time - now, wakeup_time);
    return (timer_callout_set_common(callout, wakeup_time,
				     func, arg1, arg2, arg3));
}

int
timer_callout_set(timer_callout_t * callout,
		  CFAbsoluteTime relative_time, timer_func_t * func,
		  void * arg1, void * arg2, void * arg3)
{
    CFAbsoluteTime 		wakeup_time;

    wakeup_time = CFAbsoluteTimeGetCurrent() + relative_time;
    my_log(LOG_DEBUG, "timer(%s): wakeup time is (%0.09g) %0.09g",
	   timer_name(callout),
	   relative_time, wakeup_time);
    return (timer_callout_set_common(callout, wakeup_time,
				     func, arg1, arg2, arg3));
}

int
timer_set_relative(timer_callout_t * callout, 
		   struct timeval rel_time, timer_func_t * func, 
		   void * arg1, void * arg2, void * arg3)
{
    CFTimeInterval	relative_time;

    if (rel_time.tv_sec < 0) {
	rel_time.tv_sec = 0;
	rel_time.tv_usec = 1;
    }
    relative_time = rel_time.tv_sec 
	+ ((double)rel_time.tv_usec / USECS_PER_SEC);
    return (timer_callout_set(callout, relative_time, func, arg1, arg2, arg3));
}

static void
timer_cancel_common(timer_callout_t * callout)
{
    callout->enabled = FALSE;
    callout->func = NULL;
    callout->time_generation = S_time_generation;
    if (callout->timer_source) {
	CFRunLoopTimerInvalidate(callout->timer_source);
	CFRelease(callout->timer_source);
	callout->timer_source = NULL;
    }
    return;
}

void
timer_cancel(timer_callout_t * callout)
{
    if (callout == NULL) {
	return;
    }
    if (callout->enabled) {
	my_log(LOG_DEBUG, "timer(%s): cancelled", timer_name(callout));
    }
    timer_cancel_common(callout);
    return;
}


/**
 ** timer functions
 **/
long
timer_current_secs()
{
    return ((long)CFAbsoluteTimeGetCurrent());
}

boolean_t
timer_time_changed(timer_callout_t * entry)
{
    timer_notify_check();
    return (entry->time_generation != S_time_generation);
}

boolean_t
timer_still_pending(timer_callout_t * entry)
{
    return (entry->enabled);
}
