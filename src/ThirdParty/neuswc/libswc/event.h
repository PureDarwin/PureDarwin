#ifndef SWC_EVENT_H
#define SWC_EVENT_H

#include <stdint.h>
#include <wayland-server.h>

/**
 * An event is the data passed to the listeners of the event_signals of various
 * objects.
 */
struct event {
	/**
	 * The type of event that was sent.
	 *
	 * The meaning of this field depends on the type of object containing the
	 * event_signal that passed this event.
	 */
	uint32_t type;

	/**
	 * Data specific to the event type.
	 *
	 * Unless explicitly stated in the description of the event type, this value
	 * is undefined.
	 */
	void *data;
};

static inline void
send_event(struct wl_signal *signal, uint32_t type, void *event_data)
{
	struct event event = {.type = type, .data = event_data};
	wl_signal_emit(signal, &event);
}

#endif
