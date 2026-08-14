/*
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * linklocal_v6.c
 * - this service only gets instantiated if the ConfigMethod is set explicitly
 *   to LinkLocal
 * - its only purpose is to act as a placeholder and to publish the
 *   corresponding IPv6 link-local address for the service
 * - the rest of linklocal is handled by the service management code in
 *   ipconfigd.c
 */

/* 
 * Modification History
 *
 * October 6, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "ipconfigd_threads.h"
#include "globals.h"
#include "symbol_scope.h"

STATIC void
linklocal_v6_address_changed(ServiceRef service_p,
			     inet6_addrlist_t * addr_list_p)
{
    inet6_addrinfo_t *	linklocal_p = NULL;

    linklocal_p = inet6_addrlist_get_linklocal(addr_list_p);
    if (linklocal_p != NULL) {
	if ((linklocal_p->addr_flags & IN6_IFF_NOTREADY) != 0) {
	    linklocal_p = NULL;
	}
    }
    if (linklocal_p == NULL) {
	service_publish_failure(service_p,
				ipconfig_status_resource_unavailable_e);
    }
    else {
	ServicePublishSuccessIPv6(service_p, linklocal_p, 1, NULL, 0, NULL,
				  NULL);
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
linklocal_v6_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);

    switch (evid) {
    case IFEventID_start_e: {
	inet6_addrlist_t	addrs;

	my_log(LOG_DEBUG, "%s %s: START", ServiceGetMethodString(service_p),
	       if_name(if_p));
	inet6_addrlist_copy(&addrs, if_link_index(if_p));
	linklocal_v6_address_changed(service_p, &addrs);
	inet6_addrlist_free(&addrs);
	break;
    }
    case IFEventID_stop_e:
	my_log(LOG_DEBUG, "%s %s: STOP", ServiceGetMethodString(service_p),
	       if_name(if_p));
	break;
    case IFEventID_ipv6_address_changed_e:
	linklocal_v6_address_changed(service_p, event_data);
	break;
    default:
	break;
    }

    return (ipconfig_status_success_e);
}
