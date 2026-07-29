/*
 * os/activity.h - minimal PureDarwin declarations for the os_activity(3) bits
 * libsystem_asl references.
 *
 * Activity tracing is part of Apple's closed libsystem_trace; the vendored
 * ravynOS implementation covers os_log/os_signpost but not activities. asl.c
 * uses this in exactly one place, to annotate a message with the current
 * activity ID, and skips the annotation when the ID is 0 - so reporting "no
 * current activity" simply omits an optional key rather than losing a message.
 */

#ifndef _PUREDARWIN_OS_ACTIVITY_H_
#define _PUREDARWIN_OS_ACTIVITY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t os_activity_id_t;
typedef void *os_activity_t;

#define OS_ACTIVITY_CURRENT ((os_activity_t)0)

static inline os_activity_id_t
os_activity_get_identifier(os_activity_t activity, os_activity_id_t *parent_id)
{
	(void)activity;
	if (parent_id != NULL) {
		*parent_id = 0;
	}
	return 0;   /* no activity tracing: asl.c omits the OSActivityID key */
}

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_OS_ACTIVITY_H_ */
