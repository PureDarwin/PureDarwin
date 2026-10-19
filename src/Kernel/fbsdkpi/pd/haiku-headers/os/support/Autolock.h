/*
 * Copyright 2001-2009, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_AUTOLOCK_H
#define	_AUTOLOCK_H


#include <Locker.h>
#include <Looper.h>


class BAutolock {
public:
	inline						BAutolock(BLooper* looper);
	inline						BAutolock(BLocker* locker);
	inline						BAutolock(BLocker& locker);
	inline						~BAutolock();

	inline	bool				IsLocked();

	inline	bool				Lock();
	inline	void				Unlock();

private:
			BLocker*			fLocker;
			BLooper*			fLooper;
			bool				fIsLocked;
};


inline
BAutolock::BAutolock(BLooper *looper)
	:
	fLocker(NULL),
	fLooper(looper),
	fIsLocked(looper->Lock())
{
}


inline
BAutolock::BAutolock(BLocker *locker)
	:
	fLocker(locker),
	fLooper(NULL),
	fIsLocked(locker->Lock())
{
}


inline
BAutolock::BAutolock(BLocker &locker)
	:
	fLocker(&locker),
	fLooper(NULL),
	fIsLocked(locker.Lock())
{
}


inline
BAutolock::~BAutolock()
{
	Unlock();
}


inline bool
BAutolock::IsLocked()
{
	return fIsLocked;
}


inline bool
BAutolock::Lock()
{
	if (fIsLocked)
		return true;

	if (fLooper != NULL)
		fIsLocked = fLooper->Lock();
	else
		fIsLocked = fLocker->Lock();

	return fIsLocked;
}


inline void
BAutolock::Unlock()
{
	if (!fIsLocked)
		return;

	fIsLocked = false;
	if (fLooper != NULL)
		fLooper->Unlock();
	else
		fLocker->Unlock();
}


#endif	// _AUTOLOCK_H
