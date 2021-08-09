//
//  eventlink_internal.h
//  libdispatch
//
//  Created by Rokhini Prabhu on 12/13/19.
//

#ifndef __OS_EVENTLINK_INTERNAL__
#define __OS_EVENTLINK_INTERNAL__

#if OS_EVENTLINK_USE_MACH_EVENTLINK
#include <mach/mach_eventlink.h>
#endif

#define OS_EVENTLINK_LABEL_NEEDS_FREE 0x1ull
#define OS_EVENTLINK_CANCELLED 0x2ull

union eventlink_internal {
	mach_port_t pair[2];
	uint64_t desc;
};

struct os_eventlink_s {
	struct _os_object_s _as_os_obj[0];
	OS_OBJECT_STRUCT_HEADER(eventlink);

	const char *name;
	uint64_t ev_state;

	/* Note: We use the union which allows us to write to both local and remote
	 * port atomically during activate and cancellation APIs. The combination of
	 * the state of the local_port as well as the ev_state tells us the state of
	 * the eventlink
	 *
	 * local_port = EVENTLINK_INACTIVE_PORT means that it hasn't been created yet.
	 * local_port = a valid mach port means that it has been created.
	 *
	 * If the OS_EVENTLINK_CANCELLED bit is set, that means that the port does
	 * not point to a valid kernel eventlink object.
	 *
	 * The ref of the ports are only dropped when the last external ref is
	 * dropped.
	 */
	union eventlink_internal port_pair;

	uint64_t local_count;
};

#define EVENTLINK_INACTIVE_PORT ((uint64_t) 0)
#define EVENTLINK_CLEARED_PORT ((uint64_t) 0)

static inline bool
_os_eventlink_inactive(mach_port_t port)
{
	return port == EVENTLINK_INACTIVE_PORT;
}

static inline bool
_os_eventlink_is_cancelled(uint64_t ev_state)
{
	return (ev_state & OS_EVENTLINK_CANCELLED) == OS_EVENTLINK_CANCELLED;
}

void _os_eventlink_xref_dispose(os_eventlink_t ev);
void _os_eventlink_dispose(os_eventlink_t ev);

#endif /* __OS_EVENTLINK_INTERNAL */
