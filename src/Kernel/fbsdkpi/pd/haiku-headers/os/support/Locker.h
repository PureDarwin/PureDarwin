/*
 * Copyright 2001-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_LOCKER_H
#define	_LOCKER_H


#include <OS.h>


class BLocker {
public:
								BLocker();
								BLocker(const char* name);
								BLocker(bool benaphoreStyle);
								BLocker(const char* name, bool benaphoreStyle);
	virtual						~BLocker();

			status_t			InitCheck() const;

			bool				Lock();
			status_t			LockWithTimeout(bigtime_t timeout);
			void				Unlock();

			thread_id			LockingThread() const;
			bool				IsLocked() const;
			int32				CountLocks() const;
			int32				CountLockRequests() const;
			sem_id				Sem() const;

private:
								BLocker(const char* name, bool benaphoreStyle,
									bool _ignored);
								BLocker(const BLocker&);
								BLocker& operator=(const BLocker&);
			void				InitLocker(const char* name,
									bool benaphoreStyle);
			bool				AcquireLock(bigtime_t timeout, status_t* error);

			int32				fBenaphoreCount;
			sem_id				fSemaphoreID;
			thread_id			fLockOwner;
			int32				fRecursiveCount;

			int32				_reserved[4];
};


#endif	// _LOCKER_H
