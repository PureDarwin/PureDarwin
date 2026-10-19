/*
 * Copyright 2009, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H


#include <OS.h>


/*!
	To get a good thread priority, call suggest_thread_priority() with the
	following information:
		\a what is a bit mask describing what you're doing in the thread.
		\a period is how many times a second your thread needs to run
			(-1 if you're running continuously.)
		\a jitter is an estimate (in us) of how much that period can vary,
			as long as it stays centered on the average.
		\a length is how long (in us) you expect to run for each invocation.
			"Invocation" means, typically, receiving a message, dispatching
			it, and then returning to reading a message.
		MANIPULATION means both filtering and compression/decompression.
		PLAYBACK and RECORDING means threads feeding/reading ACTUAL
		HARDWARE ONLY.
		0 means don't care
*/

/* bitmasks for suggest_thread_priority() */
enum be_task_flags {
	B_DEFAULT_MEDIA_PRIORITY	= 0x000,
	B_OFFLINE_PROCESSING		= 0x001,
	B_STATUS_RENDERING			= 0x002,	/* can also use this for */
											/* "preview" type things */
	B_USER_INPUT_HANDLING		= 0x004,
	B_LIVE_VIDEO_MANIPULATION	= 0x008,	/* non-live processing is */
											/* OFFLINE_PROCESSING */

	B_VIDEO_PLAYBACK			= 0x010,	/* feeding hardware */
	B_VIDEO_RECORDING			= 0x020,	/* grabbing from hardware */
	B_LIVE_AUDIO_MANIPULATION	= 0x040,	/* non-live processing is */
											/* OFFLINE_PROCESSING */

	B_AUDIO_PLAYBACK			= 0x080,	/* feeding hardware */
	B_AUDIO_RECORDING			= 0x100,	/* grabbing from hardware */
	B_LIVE_3D_RENDERING			= 0x200,	/* non-live rendering is */
											/* OFFLINE_PROCESSING */
	B_NUMBER_CRUNCHING			= 0x400,
	B_MIDI_PROCESSING			= 0x800
};

enum scheduler_mode {
	SCHEDULER_MODE_LOW_LATENCY,
	SCHEDULER_MODE_POWER_SAVING,
};

#if defined(__cplusplus)
extern "C" {

int32 suggest_thread_priority(uint32 task_flags = B_DEFAULT_MEDIA_PRIORITY, 
	int32 period = 0, bigtime_t jitter = 0, bigtime_t length = 0);

bigtime_t estimate_max_scheduling_latency(thread_id th = -1);
	/* default is current thread */

status_t set_scheduler_mode(int32 mode);
int32 get_scheduler_mode(void);

}
#else

int32 suggest_thread_priority(uint32 what, int32 period, bigtime_t jitter,
	bigtime_t length);

bigtime_t estimate_max_scheduling_latency(thread_id th);
	/* default is current thread */

status_t set_scheduler_mode(int32 mode);
int32 get_scheduler_mode(void);

#endif

#endif // SCHEDULER_H

