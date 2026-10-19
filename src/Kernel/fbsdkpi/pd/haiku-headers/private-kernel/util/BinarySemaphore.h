/*
 * Copyright 2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_UTIL_BINARY_SEMAPHORE_H
#define KERNEL_UTIL_BINARY_SEMAPHORE_H


#include <condition_variable.h>
#include <util/AutoLock.h>


struct BinarySemaphore {
	void Init(const char* name)
	{
		mutex_init(&fLock, "binary semaphore");
		fCondition.Init(this, name);
		fActivated = false;
	}

	bool Lock()
	{
		return mutex_lock(&fLock) == B_OK;
	}

	void Unlock()
	{
		mutex_unlock(&fLock);
	}

	bool Wait(bigtime_t timeout, bool clearActivated)
	{
		MutexLocker locker(fLock);
		if (clearActivated)
			fActivated = false;
		else if (fActivated)
			return true;

		ConditionVariableEntry entry;
		fCondition.Add(&entry);

		locker.Unlock();

		return entry.Wait(B_RELATIVE_TIMEOUT, timeout) == B_OK;
	}

	void WakeUp()
	{
		if (fActivated)
			return;

		MutexLocker locker(fLock);
		fActivated = true;
		fCondition.NotifyOne();
	}

	void ClearActivated()
	{
		MutexLocker locker(fLock);
		fActivated = false;
	}

private:
	mutex				fLock;
	ConditionVariable	fCondition;
	bool				fActivated;
};


#endif	// KERNEL_UTIL_BINARY_SEMAPHORE_H
