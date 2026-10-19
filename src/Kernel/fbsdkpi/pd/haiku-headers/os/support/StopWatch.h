/*
 * Copyright 2001-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STOP_WATCH_H
#define _STOP_WATCH_H


#include <BeBuild.h>
#include <SupportDefs.h>


class BStopWatch {
	public:
		BStopWatch(const char* name, bool silent = false);
		virtual ~BStopWatch();

		void		Suspend();
		void		Resume();
		bigtime_t	Lap();
		bigtime_t	ElapsedTime() const;
		void		Reset();
		const char*	Name() const;

	private:
		virtual	void _ReservedStopWatch1();
		virtual	void _ReservedStopWatch2();

		bigtime_t	fStart;
		bigtime_t	fSuspendTime;
		bigtime_t	fLaps[10];
		int32		fLap;
		const char*	fName;
		uint32		_reserved[2];
		bool		fSilent;
};

#endif // _STOP_WATCH_H
