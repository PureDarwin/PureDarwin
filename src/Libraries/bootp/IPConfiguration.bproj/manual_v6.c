/*
 * Copyright (c) 2003-2017 Apple Inc. All rights reserved.
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
 * manual_v6.c
 * - manual IPv6 configuration thread manual_v6_thread()
 * - assigns the address to the interface, waits for the address to appear
 *   on the interface; once it appears, if it's a duplicated IP, report the
 *   conflict; otherwise publish success
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <ctype.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#define KERNEL_PRIVATE
#include <netinet6/in6_var.h>
#undef KERNEL_PRIVATE
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <CoreFoundation/CFSocket.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include "ipconfigd_threads.h"
#include "FDSet.h"
#include "globals.h"
#include "timer.h"
#include "ifutil.h"
#include "sysconfig.h"
#include "util.h"
#include "cfutil.h"
#include "symbol_scope.h"

static void
manual_v6_publish(ServiceRef service_p)
{
    inet6_addrinfo_t		info;

    /* publish our address */
    info.addr_flags = 0;
    ServiceGetRequestedIPv6Address(service_p, &info.addr, &info.prefix_length);
    ServicePublishSuccessIPv6(service_p, &info, 1, NULL, 0, NULL, NULL);
    return;
}

static void
manual_v6_inactive(ServiceRef service_p)
{
    struct in6_addr	addr;
    int			prefix_length;

    ServiceGetRequestedIPv6Address(service_p, 
				   &addr, &prefix_length); 
    ServiceRemoveIPv6Address(service_p, &addr, prefix_length);
    service_publish_failure(service_p,
			    ipconfig_status_media_inactive_e);
    return;
}


static void
manual_v6_set_address(ServiceRef service_p)
{
    struct in6_addr		addr;
    u_int32_t			flags;
    interface_t *		if_p = service_interface(service_p);
    int				prefix_length;

    /* get the requested IP/prefix */
    ServiceGetRequestedIPv6Address(service_p, &addr, &prefix_length);

    /* set the new address */
    if (if_ift_type(if_p) == IFT_CELLULAR && ServiceDADIsEnabled(service_p)) {
	    flags = IN6_IFF_OPTIMISTIC;
    } else {
	    flags = 0;
    }
    ServiceSetIPv6Address(service_p, &addr, prefix_length, flags,
			  ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME);
    return;
}

static void
manual_v6_start(ServiceRef service_p)
{
    link_status_t	link_status;
    interface_t *	if_p = service_interface(service_p);

    if (if_ift_type(if_p) == IFT_LOOP) {
	manual_v6_set_address(service_p);
	manual_v6_publish(service_p);
	return;
    }
    link_status = service_link_status(service_p);
    if (link_status.valid == TRUE && link_status.active == FALSE) {
	manual_v6_inactive(service_p);
    }
    else {
	manual_v6_set_address(service_p);
	/* wait to publish once the address flags say DaD has completed */
    }
    return;
}

static void
manual_v6_address_changed(ServiceRef service_p,
			  inet6_addrlist_t * addr_list_p)
{
    struct in6_addr	addr;
    int			i;
    interface_t *	if_p = service_interface(service_p);
    int			prefix_length;
    inet6_addrinfo_t *	scan;

    if (addr_list_p == NULL || addr_list_p->count == 0) {
	/* no addresses configured, nothing to do */
	return;
    }

    /* get our address */
    ServiceGetRequestedIPv6Address(service_p, &addr, &prefix_length);

    /* find it in the list of IP addresses */
    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0
	    || IN6_ARE_ADDR_EQUAL(&scan->addr, &addr) == FALSE) {
	    continue;
	}
	/* found a match */
	if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
	    char	ntopbuf[INET6_ADDRSTRLEN];
	    /* DaD found a conflict, report it */
	    my_log(LOG_NOTICE,
		   "%s %s: IPv6 address %s is in use by another host",
		   ServiceGetMethodString(service_p),
		   if_name(if_p), 
		   inet_ntop(AF_INET6, &addr, ntopbuf, sizeof(ntopbuf)));
	    ServiceReportIPv6AddressConflict(service_p, &addr);
	    ServiceRemoveIPv6Address(service_p, &addr, prefix_length);
	    service_publish_failure(service_p,
				    ipconfig_status_address_in_use_e);
	}
	else if ((scan->addr_flags & IN6_IFF_TENTATIVE) == 0) {
	    /* DaD complete */
	    manual_v6_publish(service_p);
	}
	break;
    }
    return;
}

STATIC void
manual_v6_simulate_address_changed(ServiceRef service_p)
{
    inet6_addrlist_t	addrs;

    inet6_addrlist_copy(&addrs, if_link_index(service_interface(service_p)));
    manual_v6_address_changed(service_p, &addrs);
    inet6_addrlist_free(&addrs);
    return;
}

PRIVATE_EXTERN ipconfig_status_t
manual_v6_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    
    switch (evid) {
    case IFEventID_start_e: {
	ipconfig_method_data_t	method_data;

	my_log(LOG_INFO, "%s %s: starting", ServiceGetMethodString(service_p),
	       if_name(if_p));
	method_data = (ipconfig_method_data_t)event_data;
	ServiceSetRequestedIPv6Address(service_p,
				       &method_data->manual_v6.addr,
				       method_data->manual_v6.prefix_length);
	manual_v6_start(service_p);
	manual_v6_simulate_address_changed(service_p);
	break;
    }
    case IFEventID_stop_e: {
	struct in6_addr		addr;
	int			prefix_length;

	ServiceGetRequestedIPv6Address(service_p, &addr, &prefix_length);
	my_log(LOG_INFO, "%s %s: stop", ServiceGetMethodString(service_p),
	       if_name(if_p));
	ServiceRemoveIPv6Address(service_p, &addr, prefix_length);
	break;
    }
    case IFEventID_change_e: {
	struct in6_addr		addr;
	change_event_data_t * 	change;
	int			prefix_length;
	ipconfig_method_data_t	method_data;

	ServiceGetRequestedIPv6Address(service_p, &addr, &prefix_length);
	change = ((change_event_data_t *)event_data);
	method_data = change->method_data;
	if (IN6_ARE_ADDR_EQUAL(&method_data->manual_v6.addr, &addr) == FALSE
	    || method_data->manual_v6.prefix_length != prefix_length) {
	    change->needs_stop = TRUE;
	}
	break;
    }
    case IFEventID_wake_e:
    case IFEventID_renew_e:
    case IFEventID_link_status_changed_e: {
	link_status_t	link_status;

	link_status = service_link_status(service_p);
	if (link_status.valid == TRUE) {
	    if (link_status.active == TRUE
		&& (evid != IFEventID_wake_e
		    || ServiceIsPublished(service_p) == FALSE)) {
		manual_v6_start(service_p);
	    }
	}
	break;
    }
    case IFEventID_link_timer_expired_e:
	manual_v6_inactive(service_p);
	break;

    case IFEventID_ipv6_address_changed_e:
	manual_v6_address_changed(service_p, event_data);
	break;

    default:
	break;
    } /* switch */

    return (status);
}
