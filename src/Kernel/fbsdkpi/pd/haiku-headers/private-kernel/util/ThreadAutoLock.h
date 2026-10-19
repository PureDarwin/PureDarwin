/*
 * Copyright 2021, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_UTIL_THREAD_AUTO_LOCKER_H
#define KERNEL_UTIL_THREAD_AUTO_LOCKER_H


#include <shared/AutoLocker.h>

#include <thread.h>


namespace BPrivate {


class ThreadCPUPinLocking {
public:
	inline bool Lock(Thread* thread)
	{
		thread_pin_to_current_cpu(thread);
		return true;
	}

	inline void Unlock(Thread* thread)
	{
		thread_unpin_from_current_cpu(thread);
	}
};

typedef AutoLocker<Thread, ThreadCPUPinLocking> ThreadCPUPinner;
typedef AutoLocker<Team> TeamLocker;
typedef AutoLocker<Thread> ThreadLocker;


}	// namespace BPrivate

using BPrivate::ThreadCPUPinner;
using BPrivate::TeamLocker;
using BPrivate::ThreadLocker;


#endif	// KERNEL_UTIL_THREAD_AUTO_LOCKER_H
