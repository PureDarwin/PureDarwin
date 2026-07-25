/*
 * Minimal notify_post() shim.
 *
 * The real Libnotify client (notify_client.c) depends on XPC (<xpc/private.h>),
 * dispatch-private, os private headers, bootstrap and a MIG-generated IPC layer, plus
 * a running notifyd -- none of which exist in PureDarwin yet. notify_post only
 * leaks into the libSystem export surface transitively, so until XPC + notifyd
 * are up, posting a notification is a no-op that reports success. Swap this for
 * the real Libnotify client once XPC lands.
 */
#include <stdint.h>

#define NOTIFY_STATUS_OK 0

uint32_t
notify_post(const char *name)
{
	(void)name;
	return NOTIFY_STATUS_OK;
}
