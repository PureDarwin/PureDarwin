/*
 * Copyright 2022-2023, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */

extern "C" {
#include <compat/sys/mutex.h>
#include <compat/sys/kernel.h>
#include <compat/sys/condvar.h>
}

#include <condition_variable.h>


static_assert(sizeof(cv::condition) == sizeof(ConditionVariable));


void
cv_init(struct cv* variable, const char* description)
{
	__cv_ConditionVariable(variable)->Init(NULL, description);
}


void
cv_destroy(struct cv* variable)
{
	cv_broadcast(variable);

	// Nothing else to do.
}


void
cv_signal(struct cv* variable)
{
	__cv_ConditionVariable(variable)->NotifyOne();
}


void
cv_broadcast(struct cv* variable)
{
	__cv_ConditionVariable(variable)->NotifyAll();
}


static int
cv_wait_etc(struct cv* variable, struct mtx* mutex, uint32 flags, bigtime_t timeout)
{
	ConditionVariable* condition = __cv_ConditionVariable(variable);

	int status;
	if (mutex->type == MTX_RECURSE) {
		// Special case: let the ConditionVariable handle switching recursive locks.
		status = condition->Wait(&mutex->u.recursive,
			flags, timeout);
		return status;
	}

	ConditionVariableEntry entry;
	condition->Add(&entry);

	mtx_unlock(mutex);

	status = entry.Wait(flags, timeout);

	mtx_lock(mutex);
	return status;
}


int
cv_timedwait(struct cv* variable, struct mtx* mutex, int timeout)
{
	const uint32 flags = timeout ? B_RELATIVE_TIMEOUT : 0;
	const bigtime_t bigtimeout = TICKS_2_USEC(timeout);
	return cv_wait_etc(variable, mutex, flags, bigtimeout);
}


void
cv_wait(struct cv* variable, struct mtx* mutex)
{
	cv_wait_etc(variable, mutex, 0, 0);
}


int
cv_wait_sig(struct cv* variable, struct mtx* mutex)
{
	return cv_wait_etc(variable, mutex, B_CAN_INTERRUPT, 0);
}
