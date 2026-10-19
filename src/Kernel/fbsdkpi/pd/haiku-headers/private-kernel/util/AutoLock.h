/*
 * Copyright 2008-2009, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2005-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 *
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_UTIL_AUTO_LOCKER_H
#define KERNEL_UTIL_AUTO_LOCKER_H


#include <KernelExport.h>

#include <shared/AutoLocker.h>

#include <interrupts.h>
#include <lock.h>


namespace BPrivate {


class MutexLocking {
public:
	inline bool Lock(mutex *lockable)
	{
		return mutex_lock(lockable) == B_OK;
	}

	inline void Unlock(mutex *lockable)
	{
		mutex_unlock(lockable);
	}
};

typedef AutoLocker<mutex, MutexLocking> MutexLocker;


class MutexTryLocking {
public:
	inline bool Lock(mutex *lockable)
	{
		return mutex_trylock(lockable) == B_OK;
	}

	inline void Unlock(mutex *lockable)
	{
		mutex_unlock(lockable);
	}
};

typedef AutoLocker<mutex, MutexTryLocking> MutexTryLocker;


class RecursiveLockLocking {
public:
	inline bool Lock(recursive_lock *lockable)
	{
		return recursive_lock_lock(lockable) == B_OK;
	}

	inline void Unlock(recursive_lock *lockable)
	{
		recursive_lock_unlock(lockable);
	}
};

typedef AutoLocker<recursive_lock, RecursiveLockLocking> RecursiveLocker;


class ReadWriteLockReadLocking {
public:
	inline bool Lock(rw_lock *lockable)
	{
		return rw_lock_read_lock(lockable) == B_OK;
	}

	inline void Unlock(rw_lock *lockable)
	{
		rw_lock_read_unlock(lockable);
	}
};

class ReadWriteLockWriteLocking {
public:
	inline bool Lock(rw_lock *lockable)
	{
		return rw_lock_write_lock(lockable) == B_OK;
	}

	inline void Unlock(rw_lock *lockable)
	{
		rw_lock_write_unlock(lockable);
	}
};

typedef AutoLocker<rw_lock, ReadWriteLockReadLocking> ReadLocker;
typedef AutoLocker<rw_lock, ReadWriteLockWriteLocking> WriteLocker;


class InterruptsLocking {
public:
	inline bool Lock(int* lockable)
	{
		*lockable = disable_interrupts();
		return true;
	}

	inline void Unlock(int* lockable)
	{
		restore_interrupts(*lockable);
	}
};


class InterruptsLocker : public AutoLocker<int, InterruptsLocking> {
public:
	inline InterruptsLocker(bool alreadyLocked = false,
		bool lockIfNotLocked = true)
		: AutoLocker<int, InterruptsLocking>(&fState, alreadyLocked,
			lockIfNotLocked)
	{
	}

private:
	int	fState;
};


class SpinLocking {
public:
	inline bool Lock(spinlock* lockable)
	{
		acquire_spinlock(lockable);
		return true;
	}

	inline void Unlock(spinlock* lockable)
	{
		release_spinlock(lockable);
	}
};

typedef AutoLocker<spinlock, SpinLocking> SpinLocker;


class InterruptsSpinLocking {
public:
// NOTE: work-around for annoying GCC 4+ "fState may be used uninitialized"
// warning.
#if __GNUC__ >= 4
	InterruptsSpinLocking()
		:
		fState(0)
	{
	}
#endif

	inline bool Lock(spinlock* lockable)
	{
		fState = disable_interrupts();
		acquire_spinlock(lockable);
		return true;
	}

	inline void Unlock(spinlock* lockable)
	{
		release_spinlock(lockable);
		restore_interrupts(fState);
	}

private:
	int	fState;
};

typedef AutoLocker<spinlock, InterruptsSpinLocking> InterruptsSpinLocker;


class ReadSpinLocking {
public:
	inline bool Lock(rw_spinlock* lockable)
	{
		acquire_read_spinlock(lockable);
		return true;
	}

	inline void Unlock(rw_spinlock* lockable)
	{
		release_read_spinlock(lockable);
	}
};

typedef AutoLocker<rw_spinlock, ReadSpinLocking> ReadSpinLocker;


class InterruptsReadSpinLocking {
public:
	InterruptsReadSpinLocking()
		:
		fState(0)
	{
	}

	inline bool Lock(rw_spinlock* lockable)
	{
		fState = disable_interrupts();
		acquire_read_spinlock(lockable);
		return true;
	}

	inline void Unlock(rw_spinlock* lockable)
	{
		release_read_spinlock(lockable);
		restore_interrupts(fState);
	}

private:
	int	fState;
};

typedef AutoLocker<rw_spinlock, InterruptsReadSpinLocking>
	InterruptsReadSpinLocker;


class WriteSpinLocking {
public:
	inline bool Lock(rw_spinlock* lockable)
	{
		acquire_write_spinlock(lockable);
		return true;
	}

	inline void Unlock(rw_spinlock* lockable)
	{
		release_write_spinlock(lockable);
	}
};

typedef AutoLocker<rw_spinlock, WriteSpinLocking> WriteSpinLocker;


class InterruptsWriteSpinLocking {
public:
	InterruptsWriteSpinLocking()
		:
		fState(0)
	{
	}

	inline bool Lock(rw_spinlock* lockable)
	{
		fState = disable_interrupts();
		acquire_write_spinlock(lockable);
		return true;
	}

	inline void Unlock(rw_spinlock* lockable)
	{
		release_write_spinlock(lockable);
		restore_interrupts(fState);
	}

private:
	int	fState;
};

typedef AutoLocker<rw_spinlock, InterruptsWriteSpinLocking>
	InterruptsWriteSpinLocker;


class WriteSequentialLocking {
public:
	inline bool Lock(seqlock* lockable)
	{
		acquire_write_seqlock(lockable);
		return true;
	}

	inline void Unlock(seqlock* lockable)
	{
		release_write_seqlock(lockable);
	}
};

typedef AutoLocker<seqlock, WriteSequentialLocking> WriteSequentialLocker;


class InterruptsWriteSequentialLocking {
public:
	InterruptsWriteSequentialLocking()
		:
		fState(0)
	{
	}

	inline bool Lock(seqlock* lockable)
	{
		fState = disable_interrupts();
		acquire_write_seqlock(lockable);
		return true;
	}

	inline void Unlock(seqlock* lockable)
	{
		release_write_seqlock(lockable);
		restore_interrupts(fState);
	}

private:
	int	fState;
};

typedef AutoLocker<seqlock, InterruptsWriteSequentialLocking>
	InterruptsWriteSequentialLocker;


}	// namespace BPrivate

using BPrivate::AutoLocker;
using BPrivate::MutexLocker;
using BPrivate::MutexTryLocker;
using BPrivate::RecursiveLocker;
using BPrivate::ReadLocker;
using BPrivate::WriteLocker;
using BPrivate::InterruptsLocker;
using BPrivate::SpinLocker;
using BPrivate::InterruptsSpinLocker;
using BPrivate::ReadSpinLocker;
using BPrivate::InterruptsReadSpinLocker;
using BPrivate::WriteSpinLocker;
using BPrivate::InterruptsWriteSpinLocker;
using BPrivate::WriteSequentialLocker;
using BPrivate::InterruptsWriteSequentialLocker;


#endif	// KERNEL_UTIL_AUTO_LOCKER_H
