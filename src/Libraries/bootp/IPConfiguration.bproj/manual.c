/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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
 * manual.c
 * - manual configuration thread manual_thread()
 * - probes the address using ARP to ensure that another client 
 *   isn't already using the address
 */
/* 
 * Modification History
 *
 * May 24, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
 * - added more code to re-arp when the link comes back
 * - forgo configuring the address if the link state is inactive
 *   after we ARP
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <ctype.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <syslog.h>

#include "interfaces.h"
#include "util.h"

#include "dhcp_options.h"
#include "ipconfigd_threads.h"
#include "symbol_scope.h"

typedef struct {
    arp_client_t *		arp;
    timer_callout_t *		timer;
    boolean_t			resolve_router_timed_out;
    boolean_t			ignore_link_status;
    boolean_t			user_warned;
} Service_manual_t;

static void
manual_resolve_router_callback(ServiceRef service_p, router_arp_status_t status);

static void
manual_resolve_router_retry(void * arg0, void * arg1, void * arg2)
{
    Service_manual_t *	manual;
    ServiceRef 		service_p = (ServiceRef)arg0;

    manual = (Service_manual_t *)ServiceGetPrivate(service_p);
    service_resolve_router(service_p, manual->arp,
			   manual_resolve_router_callback,
			   service_requested_ip_addr(service_p));
    return;
}

static void
manual_resolve_router_callback(ServiceRef service_p,
			       router_arp_status_t status)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual;
    struct timeval	tv;

    manual = (Service_manual_t *)ServiceGetPrivate(service_p);
    switch (status) {
    case router_arp_status_no_response_e:
	/* try again in 60 seconds */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	timer_set_relative(manual->timer, tv, 
			   (timer_func_t *)manual_resolve_router_retry,
			   service_p, NULL, NULL);
	if (manual->resolve_router_timed_out) {
	    break;
	}
	manual->resolve_router_timed_out = TRUE;
	/* publish what we have so far */
	ServicePublishSuccessIPv4(service_p, NULL);
	break;
    case router_arp_status_success_e:
	manual->resolve_router_timed_out = FALSE;
	ServicePublishSuccessIPv4(service_p, NULL);
	break;
    case router_arp_status_failed_e:
	my_log(LOG_NOTICE, "MANUAL %s: router arp resolution failed, %s",
	       if_name(if_p), arp_client_errmsg(manual->arp));
	break;
    }
    return;
}

static void
manual_cancel_pending_events(ServiceRef service_p)
{
    Service_manual_t *	manual;

    manual = (Service_manual_t *)ServiceGetPrivate(service_p);
    if (manual == NULL)
	return;
    if (manual->timer) {
	timer_cancel(manual->timer);
    }
    if (manual->arp) {
	arp_client_cancel(manual->arp);
    }
    return;
}

static void
manual_inactive(ServiceRef service_p)
{
    manual_cancel_pending_events(service_p);
    service_remove_address(service_p);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e);
    return;
}

static void
manual_start(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual;

    manual = (Service_manual_t *)ServiceGetPrivate(service_p);
    switch (evid) {
      case IFEventID_start_e: {
	  if (manual->arp == NULL) {
	      link_status_t	link_status = service_link_status(service_p);

	      /* if the link is up, just assign the IP */
	      if (manual->ignore_link_status == FALSE
		  && link_status.valid == TRUE 
		  && link_status.active == FALSE) {
		  manual_inactive(service_p);
		  break;
	      }
	      (void)service_set_address(service_p,
					service_requested_ip_addr(service_p),
					service_requested_ip_mask(service_p),
					G_ip_zeroes);
	      ServicePublishSuccessIPv4(service_p, NULL);
	      break;
	  }
	  manual_cancel_pending_events(service_p);
	  arp_client_probe(manual->arp,
			   (arp_result_func_t *)manual_start, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   service_requested_ip_addr(service_p));
	  break;
      }
      case IFEventID_arp_e: {
	  link_status_t		link_status;
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_NOTICE, "MANUAL %s: arp probe failed, %s",
		     if_name(if_p), arp_client_errmsg(manual->arp));
	      /* hopefully a temporary failure, try again in a bit */
	      timer_callout_set(manual->timer, ARP_PROBE_FAILURE_RETRY_TIME,
				(timer_func_t *)manual_start,
				service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  else {
	      if (result->in_use) {
		  char			msg[128];
		  struct in_addr	requested_ip;
		  struct timeval	tv;

		  requested_ip = service_requested_ip_addr(service_p);
		  snprintf(msg, sizeof(msg), 
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(&requested_ip),
			   EA_LIST(result->addr.target_hardware));
		  if (manual->user_warned == FALSE) {
		      manual->user_warned = TRUE;
		      ServiceReportIPv4AddressConflict(service_p,
						       requested_ip);
		  }
		  my_log(LOG_NOTICE, "MANUAL %s: %s",
			 if_name(if_p), msg);
		  service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e);
		  if (G_manual_conflict_retry_interval_secs > 0) {
		      /* try again in a bit */
		      tv.tv_sec = G_manual_conflict_retry_interval_secs;
		      tv.tv_usec = 0;
		      timer_set_relative(manual->timer, tv, 
					 (timer_func_t *)manual_start,
					 service_p, IFEventID_start_e, NULL);
		  }
		  break;
	      }
	  }
	  link_status = service_link_status(service_p);
	  if (manual->ignore_link_status == FALSE
	      && link_status.valid == TRUE 
	      && link_status.active == FALSE) {
	      manual_inactive(service_p);
	      break;
	  }

	  /* set the new address */
	  (void)service_set_address(service_p,
				    service_requested_ip_addr(service_p),
				    service_requested_ip_mask(service_p),
				    G_ip_zeroes);
	  ServiceRemoveAddressConflict(service_p);
	  if (service_router_is_iaddr_valid(service_p)
	      && service_resolve_router(service_p, manual->arp,
					manual_resolve_router_callback,
					service_requested_ip_addr(service_p))) {
	      /* router ARP resolution started */
	      manual->resolve_router_timed_out = FALSE;
	  }
	  else {
	      ServicePublishSuccessIPv4(service_p, NULL);
	  }
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
manual_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual;
    ipconfig_status_t	status = ipconfig_status_success_e;

    manual = (Service_manual_t *)ServiceGetPrivate(service_p);
    switch (evid) {
      case IFEventID_start_e: {
	  ipconfig_method_data_t 	method_data;
	  char				timer_name[32];

	  method_data = (ipconfig_method_data_t)event_data;
	  if (manual) {
	      my_log(LOG_INFO, "MANUAL %s: re-entering start state", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  manual = malloc(sizeof(*manual));
	  ServiceSetPrivate(service_p, manual);
	  bzero(manual, sizeof(*manual));
	  manual->ignore_link_status = method_data->manual.ignore_link_status;
	  service_set_requested_ip_addr(service_p, method_data->manual.addr);
	  service_set_requested_ip_mask(service_p, method_data->manual.mask);
	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      /* set the new address */
	      (void)service_set_address(service_p, 
					service_requested_ip_addr(service_p),
					service_requested_ip_mask(service_p),
					G_ip_zeroes);
	      ServicePublishSuccessIPv4(service_p, NULL);
	      break;
	  }
	  if (method_data->manual.router.s_addr != 0
	      && (method_data->manual.router.s_addr 
		  != method_data->manual.addr.s_addr)) {
	      service_router_set_iaddr(service_p, method_data->manual.router);
	      service_router_set_iaddr_valid(service_p);
	  }
	  snprintf(timer_name, sizeof(timer_name), "manual-%s",
		   if_name(if_p));
	  manual->timer = timer_callout_init(timer_name);
	  if (manual->timer == NULL) {
	      my_log(LOG_NOTICE, "MANUAL %s: timer_callout_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  manual->arp = arp_client_init(G_arp_session, if_p);
	  if (manual->arp == NULL) {
	      my_log(LOG_NOTICE, "MANUAL %s: arp_client_init failed",
		     if_name(if_p));
	  }
	  my_log(LOG_INFO, "MANUAL %s: starting",
		 if_name(if_p));
	  manual_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      stop:
      case IFEventID_stop_e: {
	  my_log(LOG_INFO, "MANUAL %s: stop", if_name(if_p));
	  if (manual == NULL) {
	      break;
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (manual->arp) {
	      arp_client_free(&manual->arp);
	  }
	  if (manual->timer) {
	      timer_callout_free(&manual->timer);
	  }
	  if (manual) {
	      free(manual);
	  }
	  ServiceSetPrivate(service_p, NULL);
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   	change_event;
	  ipconfig_method_data_t 	method_data;

	  if (manual == NULL) {
	      my_log(LOG_INFO, "MANUAL %s: private data is NULL", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  change_event = ((change_event_data_t *)event_data);
	  method_data = change_event->method_data;
	  change_event->needs_stop = FALSE;
	  if (method_data->manual.addr.s_addr
	      != service_requested_ip_addr(service_p).s_addr
	      || (service_router_is_iaddr_valid(service_p)
		  && (method_data->manual.router.s_addr 
		      != service_router_iaddr(service_p).s_addr))
	      || (method_data->manual.ignore_link_status
		  != manual->ignore_link_status)) {
	      change_event->needs_stop = TRUE;
	  }
	  else if (method_data->manual.mask.s_addr
		   != service_requested_ip_mask(service_p).s_addr) {
	      service_set_requested_ip_mask(service_p,
					    method_data->manual.mask);
	      (void)service_set_address(service_p, 
					method_data->manual.addr,
					method_data->manual.mask,
					G_ip_zeroes);
	      /* publish new mask */
	      ServicePublishSuccessIPv4(service_p, NULL);
	  }
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];

	  arpc = (arp_collision_data_t *)event_data;

	  if (manual == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (arpc->ip_addr.s_addr 
	      != service_requested_ip_addr(service_p).s_addr) {
	      break;
	  }
	  /* defend our address, don't just give it up */
	  if (ServiceDefendIPv4Address(service_p, arpc)) {
	      break;
	  }
	  snprintf(msg, sizeof(msg), 
		   IP_FORMAT " in use by " EA_FORMAT,
		   IP_LIST(&arpc->ip_addr), 
		   EA_LIST(arpc->hwaddr));
	  if (manual->user_warned == FALSE) {
	      manual->user_warned = TRUE;
	      ServiceReportIPv4AddressConflict(service_p,
					       arpc->ip_addr);
	  }
	  my_log(LOG_NOTICE, "MANUAL %s: %s",
		 if_name(if_p), msg);
	  break;
      }
      case IFEventID_wake_e:
      case IFEventID_renew_e:
      case IFEventID_link_status_changed_e: {
	  link_status_t		link_status;

	  if (manual == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (manual->ignore_link_status) {
	      break;
	  }
	  manual->user_warned = FALSE;
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE && link_status.active == FALSE) {
	      manual_cancel_pending_events(service_p);
	  }
	  else if (evid != IFEventID_wake_e
		   || ServiceIsPublished(service_p) == FALSE) {
	      manual_start(service_p, IFEventID_start_e, NULL);
	  }
	  break;
      }
      case IFEventID_link_timer_expired_e:
	  if (manual->ignore_link_status) {
	      break;
	  }
	  manual_inactive(service_p);
	  break;
      default:
	  break;
    } /* switch */

    return (status);
}
