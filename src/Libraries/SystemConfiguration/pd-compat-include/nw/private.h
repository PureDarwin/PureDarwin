/*
 * <nw/private.h> is the private header for Apple's libnetwork (the code behind
 * Network.framework), which is not open source. PureDarwin has no libnetwork.
 *
 * Most includers of this header reference nothing from it - ip_plugin.c and
 * nat64-configuration.c's siblings include it out of habit. The sources that
 * genuinely need the nw_* object API (SCNetworkReachability.c,
 * SCNetworkSignature.c, IPMonitor's nat64-configuration.c) are left out of the
 * build entirely rather than linked against a fake nw_* API.
 *
 * The one exception is below.
 */

#ifndef _PUREDARWIN_NW_PRIVATE_H_
#define _PUREDARWIN_NW_PRIVATE_H_

#include <xpc/xpc.h>

/*
 * KernelEventMonitor's config_new_interface() calls this to tell libnetwork's
 * configuration machinery that an interface appeared and its settings should be
 * re-examined. It is a notification to another subsystem, not a query: it
 * returns nothing and KernelEventMonitor does not depend on any effect of it.
 *
 * With no libnetwork there is nothing to notify, so this does nothing. That
 * makes KernelEventMonitor's own job - watching PF_SYSTEM events and publishing
 * State:/Network/Interface keys into the dynamic store - work normally, while
 * the interface-settings re-check simply does not happen because nothing
 * implements it.
 */
static inline void
network_config_check_interface_settings(xpc_object_t if_list)
{
	(void)if_list;
}

/*
 * Opaque forward declarations for the nw_* object types, so headers that merely
 * name them in struct fields parse. SCNetworkReachabilityInternal.h is the case
 * that matters: ip_plugin.c includes it for the SCNetworkReachabilityFlags
 * constants and never touches the reachability struct's nw_* members.
 *
 * Types only - deliberately no function declarations. Anything that actually
 * calls into libnetwork therefore fails to link, which is the correct outcome
 * rather than silently binding to a stub.
 */
typedef struct nw_endpoint        *nw_endpoint_t;
typedef struct nw_parameters      *nw_parameters_t;
typedef struct nw_path            *nw_path_t;
typedef struct nw_path_evaluator  *nw_path_evaluator_t;
typedef struct nw_resolver        *nw_resolver_t;
typedef struct nw_array           *nw_array_t;
typedef struct nw_interface       *nw_interface_t;
typedef int                        nw_resolver_status_t;

#endif /* _PUREDARWIN_NW_PRIVATE_H_ */
