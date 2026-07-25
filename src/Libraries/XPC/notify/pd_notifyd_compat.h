/*
 * PureDarwin: os/trace_private.h itself now lives at compat/os/trace_private.h
 * (a real includable file, since notifyd.c has its own #include for it)
 */
#ifndef PD_NOTIFYD_COMPAT_H
#define PD_NOTIFYD_COMPAT_H

#include <xpc/xpc.h>
static inline xpc_object_t
xpc_copy_entitlement_for_token(const char *key, void *token)
{
	(void)key;
	(void)token;
	return NULL;
}

/*
 * PureDarwin: real Apple private XPC event streaming/matching API - see
 * pd_xpc_event_publisher.c for why these are implemented there (not a
 * pure stub - the INITIAL_BARRIER action drives notifyd's real Mach
 * channel connection).
 */
#include <dispatch/dispatch.h>

typedef struct xpc_event_publisher_s *xpc_event_publisher_t;
typedef int xpc_event_publisher_action_t;
#define XPC_EVENT_PUBLISHER_ACTION_ADD 0
#define XPC_EVENT_PUBLISHER_ACTION_REMOVE 1
#define XPC_EVENT_PUBLISHER_ACTION_INITIAL_BARRIER 2

typedef void (^xpc_event_publisher_handler_t)(xpc_event_publisher_action_t action, uint64_t event_token, xpc_object_t descriptor);
typedef void (^xpc_event_publisher_error_handler_t)(int error);

xpc_event_publisher_t xpc_event_publisher_create(const char *name, dispatch_queue_t queue);
void xpc_event_publisher_set_handler(xpc_event_publisher_t publisher, xpc_event_publisher_handler_t handler);
void xpc_event_publisher_set_error_handler(xpc_event_publisher_t publisher, xpc_event_publisher_error_handler_t handler);
void xpc_event_publisher_activate(xpc_event_publisher_t publisher);
int xpc_event_publisher_get_subscriber_asid(xpc_event_publisher_t publisher, uint64_t event_token);

static inline int
xpc_event_publisher_fire_noboost(void *publisher, uint64_t token, xpc_object_t payload)
{
	(void)publisher;
	(void)token;
	(void)payload;
	return 1;
}

#endif /* PD_NOTIFYD_COMPAT_H */
