/*
 * Copyright 2012, Jérôme Duval, korli@users.berlios.de.
 * Copyright 2018, Haiku, Inc.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <thread.h>

extern "C" {
#include <compat/sys/systm.h>
#include <compat/sys/kernel.h>
}


int
_pause(const char* waitMessage, int timeout)
{
	KASSERT(timeout != 0, ("pause: timeout required"));
	return snooze(TICKS_2_USEC(timeout));
}


void
critical_enter(void)
{
	thread_pin_to_current_cpu(thread_get_current_thread());
}


void
critical_exit(void)
{
	thread_unpin_from_current_cpu(thread_get_current_thread());
}


void
freeenv(char *env)
{
}


char *
getenv(const char *name)
{
	return NULL;
}


char *
kern_getenv(const char *name)
{
	return NULL;
}

