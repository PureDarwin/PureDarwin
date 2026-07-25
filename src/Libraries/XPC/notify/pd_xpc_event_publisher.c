/*
 * PureDarwin: real xpc_event_publisher_* is Apple's private XPC event
 * streaming/matching API - never open-sourced anywhere
 *
 * The XPC_EVENT_PUBLISHER_ACTION_INITIAL_BARRIER handler is
 * NOT cosmetic: it's where the daemon's actual Mach IPC channels
 * (mach_channel/mach_notifs_channel) get connected via
 * dispatch_mach_connect(). A pure no-op stub would leave notifyd's real
 * client-facing Mach service never listening. This minimal implementation
 * preserves that real behavior: activate() immediately fires the
 * INITIAL_BARRIER action (so the Mach channels connect for real), and
 * simply never fires ADD/REMOVE (no real XPC-event-matching subscribers
 * ever register - degrades gracefully, same as real notifyd would if the
 * matching subsystem had zero subscribers).
 */
#include <stdlib.h>
#include <Block.h>
#include "libnotify.h"
#include "pd_notifyd_compat.h"

struct xpc_event_publisher_s {
	xpc_event_publisher_handler_t handler;
	xpc_event_publisher_error_handler_t error_handler;
};
typedef struct xpc_event_publisher_s *xpc_event_publisher_t;

xpc_event_publisher_t
xpc_event_publisher_create(const char *name, dispatch_queue_t queue)
{
	(void)name;
	(void)queue;
	return (xpc_event_publisher_t)calloc(1, sizeof(struct xpc_event_publisher_s));
}

void
xpc_event_publisher_set_handler(xpc_event_publisher_t publisher, xpc_event_publisher_handler_t handler)
{
	if (publisher == NULL) {
		return;
	}
	publisher->handler = Block_copy(handler);
}

void
xpc_event_publisher_set_error_handler(xpc_event_publisher_t publisher, xpc_event_publisher_error_handler_t handler)
{
	if (publisher == NULL) {
		return;
	}
	publisher->error_handler = Block_copy(handler);
}

void
xpc_event_publisher_activate(xpc_event_publisher_t publisher)
{
	if (publisher == NULL || publisher->handler == NULL) {
		return;
	}
	publisher->handler(XPC_EVENT_PUBLISHER_ACTION_INITIAL_BARRIER, 0, NULL);
}

int
xpc_event_publisher_get_subscriber_asid(xpc_event_publisher_t publisher, uint64_t event_token)
{
	/* Unreachable in this configuration: only called from
	 * notifyd_matching_register(), itself only reachable via a real
	 * ADD action this stub never fires. */
	(void)publisher;
	(void)event_token;
	return -1;
}
