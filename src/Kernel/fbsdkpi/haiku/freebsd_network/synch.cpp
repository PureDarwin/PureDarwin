/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */

extern "C" {
#include <compat/sys/systm.h>
#include <compat/sys/kernel.h>
#include <compat/sys/mutex.h>
#include <compat/sys/condvar.h>
}

#include <condition_variable.h>


int
msleep(void* identifier, struct mtx* mutex, int priority,
	const char* description, int timeout)
{
	struct cv channel;
	__cv_ConditionVariable(&channel)->Publish(identifier, description);

	int status = cv_timedwait(&channel, mutex, timeout);

	__cv_ConditionVariable(&channel)->Unpublish();
	return status;
}


void
wakeup(void* identifier)
{
	ConditionVariable::NotifyAll(identifier, B_OK);
}


void
wakeup_one(void* identifier)
{
	ConditionVariable::NotifyOne(identifier, B_OK);
}
