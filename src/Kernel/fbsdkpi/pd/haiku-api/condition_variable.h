// SPDX-License-Identifier: MIT
// haiku's ConditionVariable, on the seam's global wait lock
#ifndef _PD_HAIKU_CONDITION_VARIABLE_H
#define _PD_HAIKU_CONDITION_VARIABLE_H

#include <OS.h>
#include <lock.h>

struct ConditionVariable;

struct ConditionVariableEntry {
	ConditionVariableEntry()
		: fVariable(NULL), fNext(NULL), fWaitStatus(0), fWaiting(false) {}
	~ConditionVariableEntry();

	status_t Wait(uint32 flags = 0, bigtime_t timeout = 0);
	ConditionVariable* Variable() const { return fVariable; }

	ConditionVariable*		fVariable;
	ConditionVariableEntry*	fNext;
	status_t				fWaitStatus;
	bool					fWaiting;
};

struct ConditionVariable {
	void Init(const void* object, const char* objectType);
	void Publish(const void* object, const char* objectType);
	void Unpublish();

	void NotifyOne(status_t result = B_OK);
	void NotifyAll(status_t result = B_OK);
	static void NotifyOne(const void* object, status_t result);
	static void NotifyAll(const void* object, status_t result);

	void Add(ConditionVariableEntry* entry);
	status_t Wait(uint32 flags = 0, bigtime_t timeout = 0);
	status_t Wait(mutex* lock, uint32 flags = 0, bigtime_t timeout = 0);
	status_t Wait(recursive_lock* lock, uint32 flags = 0, bigtime_t timeout = 0);

	const void* Object() const { return fObject; }

	// keep the size fixed at five pointers plus padding, as struct cv reserves
	const void*				fObject;
	const char*				fObjectType;
	ConditionVariableEntry*	fEntries;
	ConditionVariable*		fNextPublished;
	void*					fReserved;
	int32					fPad[2];
};

#endif
