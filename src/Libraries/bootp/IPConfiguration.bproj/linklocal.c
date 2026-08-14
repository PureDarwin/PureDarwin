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
 * linklocal.c
 * - link-local address configuration thread
 * - contains linklocal_thread()
 */
/* 
 * Modification History
 *
 * September 27, 2001	Dieter Siegmund (dieter@apple.com)
 * - moved ad-hoc processing into its own service
 *
 * January 8, 2002	Dieter Siegmund (dieter@apple.com)
 * - added pseudo-link-local service support i.e. configure the
 *   subnet, but don't configure a link-local address
 * 
 * April 13, 2005	Dieter Siegmund (dieter@apple.com)
 * - for the pseudo-link-local service support, check whether an ARP for
 *   169.254.255.255 (link-local subnet-specific broadcast) is received.
 *   If it is, we can assume that a router is configured for proxy ARP, and
 *   thus link-local to routable communication is not possible, so disable
 *   link-local ARP on this interface.
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
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <net/if_types.h>

#include "dhcp_options.h"
#include "dhcp.h"
#include "interfaces.h"
#include "util.h"
#include "host_identifier.h"
#include "dhcplib.h"

#include "ipconfigd_threads.h"

#define LINKLOCAL_RANGE_START	IN_LINKLOCALNETNUM
#define LINKLOCAL_RANGE_END	((u_int32_t)0xa9feffff) /* 169.254.255.255 */
#define LINKLOCAL_FIRST_USEABLE	(LINKLOCAL_RANGE_START + 256) /* 169.254.1.0 */
#define LINKLOCAL_LAST_USEABLE	(LINKLOCAL_RANGE_END - 256) /* 169.254.254.255 */
#define LINKLOCAL_MASK		IN_CLASSB_NET
#define LINKLOCAL_RANGE		((u_int32_t)(LINKLOCAL_LAST_USEABLE + 1) \
				 - LINKLOCAL_FIRST_USEABLE)

#define	MAX_LINKLOCAL_INITIAL_TRIES	10

/*
 * LINKLOCAL_RETRY_TIME_SECS
 *   After we probe for MAX_LINKLOCAL_INITIAL_TRIES addresses and fail,
 *   wait this amount of time before trying the next one.  This avoids
 *   overwhelming the network with ARP probes in the worst case scenario.
 */
#define LINKLOCAL_RETRY_TIME_SECS	30

typedef struct {
    arp_client_t *	arp;
    timer_callout_t *	timer;
    int			current;
    struct in_addr	our_ip;
    struct in_addr	probe;
    boolean_t		allocate;
    boolean_t		enable_arp_collision_detection;
} Service_linklocal_t;

static int
siocarpipll(int s, const char * name, int val)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    ifr.ifr_intval = val;
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCARPIPLL, &ifr));
}

static void
set_arp_linklocal(const char * name, int val)
{
    int	s;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
	my_log(LOG_NOTICE, "set_arp_linklocal(%s) socket() failed, %s",
	       name, strerror(errno));
	return;
    }
    if (siocarpipll(s, name, val) < 0) {
	if (errno != ENXIO) {
	    my_log(LOG_NOTICE,
		   "set_arp_linklocal(%s) SIOCARPIPLL %d failed, %s",
		   name, val, strerror(errno));
	}
    }
    close(s);
}

static __inline__ void
arp_linklocal_disable(const char * name)
{
    set_arp_linklocal(name, 0);
}

static __inline__ void
arp_linklocal_enable(const char * name)
{
    set_arp_linklocal(name, 1);
}

static boolean_t
parent_service_ip_address(ServiceRef service_p, struct in_addr * ret_ip)
{
    struct in_addr	addr;
    ServiceRef 		parent_service_p = service_parent_service(service_p);

    if (parent_service_p == NULL) {
	return (FALSE);
    }
    addr = ServiceGetActiveIPAddress(parent_service_p);
    if (addr.s_addr == 0) {
	return (FALSE);
    }
    *ret_ip = addr;
    return (TRUE);
}

struct in_addr
S_find_linklocal_address(ServiceRef service_p)
{
    int				count;
    int				i;
    interface_t *		if_p;
    struct in_addr		ll_addr;

    ll_addr = linklocal_get_address(service_p);
    if (ll_addr.s_addr != 0) {
	return (ll_addr);
    }
    if_p = service_interface(service_p);
    count = if_inet_count(if_p);
    for (i = 0; i < count; i++) {
	inet_addrinfo_t * 	info = if_inet_addr_at(if_p, i);

	if (ip_is_linklocal(info->addr)) {
	    my_log(LOG_INFO, "LINKLOCAL %s: found address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&info->addr));
	    return (info->addr);
	}
    }
    return (G_ip_zeroes);
}

static void
linklocal_cancel_pending_events(ServiceRef service_p)
{
    Service_linklocal_t * 	linklocal;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    if (linklocal == NULL)
	return;
    if (linklocal->timer) {
	timer_cancel(linklocal->timer);
    }
    if (linklocal->arp) {
	arp_client_cancel(linklocal->arp);
    }
    return;
}


static void
linklocal_failed(ServiceRef service_p, ipconfig_status_t status)
{
    Service_linklocal_t * linklocal;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    linklocal->enable_arp_collision_detection = FALSE;
    linklocal_cancel_pending_events(service_p);
    arp_linklocal_disable(if_name(service_interface(service_p)));
    service_remove_address(service_p);
    if (status != ipconfig_status_media_inactive_e) {
	linklocal->our_ip = G_ip_zeroes;
    }
    service_publish_failure(service_p, status);
    return;
}

static void
linklocal_inactive(ServiceRef service_p)
{
    linklocal_failed(service_p, ipconfig_status_media_inactive_e);
    return;
}

static void
linklocal_detect_proxy_arp(ServiceRef service_p, IFEventID_t event_id,
			   void * event_data)
{
    interface_t *	  if_p = service_interface(service_p);
    Service_linklocal_t * linklocal;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  struct in_addr	iaddr = { 0 };
	  struct in_addr	llbroadcast;

	  if (if_is_wireless(if_p)) {
		  /* don't send ARP, just assume it will work */
		  arp_linklocal_enable(if_name(if_p));
		  service_publish_failure(service_p,
					  ipconfig_status_success_e);
		  break;
	  }
	  arp_linklocal_disable(if_name(if_p));
	  llbroadcast.s_addr = htonl(LINKLOCAL_RANGE_END);
	  /* clean-up anything that might have come before */
	  linklocal_cancel_pending_events(service_p);
	  if (parent_service_ip_address(service_p, &iaddr) == FALSE) {
	      my_log(LOG_NOTICE, "LINKLOCAL %s: parent has no IP",
		     if_name(if_p));
	      break;
	  }
	  my_log(LOG_INFO,
		 "LINKLOCAL %s: ARP Request: Source " IP_FORMAT
		 " Target 169.254.255.255", if_name(if_p), IP_LIST(&iaddr));
	  arp_client_probe(linklocal->arp,
			   (arp_result_func_t *)linklocal_detect_proxy_arp,
			   service_p, (void *)IFEventID_arp_e, iaddr,
			   llbroadcast);
	  /* wait for the results */
	  break;
      }
      case IFEventID_arp_e: {
	  link_status_t		link_status;
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_INFO, "LINKLOCAL %s: ARP probe failed, %s",
		     if_name(if_p),
		     arp_client_errmsg(linklocal->arp));
	      break;
	  }
	  linklocal_set_needs_attention();
	  if (result->in_use) {
	      my_log(LOG_INFO,
		     "LINKLOCAL %s: ARP response received for 169.254.255.255"
		     " from " EA_FORMAT,
		     if_name(if_p),
		     EA_LIST(result->addr.target_hardware));
	      service_publish_failure(service_p,
				      ipconfig_status_address_in_use_e);
	      break;
	  }
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE
	      && link_status.active == FALSE) {
	      linklocal_failed(service_p,
			       ipconfig_status_media_inactive_e);
	      break;
	  }
	  arp_linklocal_enable(if_name(if_p));
	  service_publish_failure(service_p,
				  ipconfig_status_success_e);
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

static void
linklocal_allocate(ServiceRef service_p, IFEventID_t event_id,
		   void * event_data)
{
    interface_t *	  if_p = service_interface(service_p);
    Service_linklocal_t * linklocal;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  linklocal->enable_arp_collision_detection = FALSE;

	  arp_linklocal_disable(if_name(if_p));

	  /* clean-up anything that might have come before */
	  linklocal_cancel_pending_events(service_p);
	  
	  linklocal->current = 1;
	  if (linklocal->our_ip.s_addr) {
	      /* try to keep the same address */
	      linklocal->probe = linklocal->our_ip;
	  }
	  else {
	      linklocal->probe.s_addr 
		  = htonl(LINKLOCAL_FIRST_USEABLE 
	            + random_range(0, LINKLOCAL_RANGE));
	  }
	  my_log(LOG_INFO, "LINKLOCAL %s: probing " IP_FORMAT, 
		 if_name(if_p), IP_LIST(&linklocal->probe));
	  arp_client_probe(linklocal->arp, 
			   (arp_result_func_t *)linklocal_allocate, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   linklocal->probe);
	  /* wait for the results */
	  return;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_INFO, "LINKLOCAL %s: ARP probe failed, %s", 
		     if_name(if_p),
		     arp_client_errmsg(linklocal->arp));
	      break;
	  }
	  if (result->in_use 
	      || service_is_using_ip(service_p, linklocal->probe)) {
	      if (result->in_use) {
	          my_log(LOG_INFO, "LINKLOCAL %s: IP address " 
		         IP_FORMAT " is in use by " EA_FORMAT, 
		         if_name(if_p), 
		         IP_LIST(&linklocal->probe),
		         EA_LIST(result->addr.target_hardware));
	      }
	      else {
		  my_log(LOG_INFO, "LINKLOCAL %s: IP address "
			 IP_FORMAT " is no longer unique",
			 if_name(if_p),
		         IP_LIST(&linklocal->probe));
	      }
	      if (linklocal->our_ip.s_addr == linklocal->probe.s_addr) {
		  linklocal->our_ip = G_ip_zeroes;
		  (void)service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e);
	      }
	  }
	  else {
	      link_status_t	   link_status = service_link_status(service_p);
	      const struct in_addr linklocal_mask = { htonl(LINKLOCAL_MASK) };

	      if (link_status.valid == TRUE 
		  && link_status.active == FALSE) {
		  linklocal_failed(service_p,
				   ipconfig_status_media_inactive_e);
		  break;
	      }

	      /* ad-hoc IP address is not in use, so use it */
	      (void)service_set_address(service_p, linklocal->probe, 
					linklocal_mask, G_ip_zeroes);
	      linklocal_set_address(service_p, linklocal->probe);
	      arp_linklocal_enable(if_name(if_p));
	      linklocal_cancel_pending_events(service_p);
	      linklocal->our_ip = linklocal->probe;
	      ServicePublishSuccessIPv4(service_p, NULL);
	      linklocal->enable_arp_collision_detection = TRUE;
	      /* we're done */
	      break; /* out of switch */
	  }
	  if (linklocal->current >= MAX_LINKLOCAL_INITIAL_TRIES) {
	      struct timeval tv;
	      /* initial tries threshold reached, try again after a timeout */
	      tv.tv_sec = LINKLOCAL_RETRY_TIME_SECS;
	      tv.tv_usec = 0;
	      timer_set_relative(linklocal->timer, tv, 
				 (timer_func_t *)linklocal_allocate,
				 service_p, (void *)IFEventID_timeout_e, NULL);
	      /* don't fall through, wait for timer */
	      break;
	  }
	  linklocal->current++;
	  /* FALL THROUGH */
      case IFEventID_timeout_e:
	  /* try the next address */
	  linklocal->probe.s_addr 
	      = htonl(LINKLOCAL_FIRST_USEABLE 
		+ random_range(0, LINKLOCAL_RANGE));
	  arp_client_probe(linklocal->arp, 
			   (arp_result_func_t *)linklocal_allocate, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   linklocal->probe);
	  my_log(LOG_INFO, "LINKLOCAL %s probing " IP_FORMAT, 
		 if_name(if_p), IP_LIST(&linklocal->probe));
	  /* wait for the results */
	  break;
      }
      default:
	  break;
    }

    return;
}

static void
linklocal_start(ServiceRef service_p)
{
    Service_linklocal_t *	linklocal;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    if (linklocal->allocate) {
	linklocal_allocate(service_p, IFEventID_start_e, NULL);
    }
    else {
	linklocal_detect_proxy_arp(service_p,
				   IFEventID_start_e, NULL);
    }
    return;
}

ipconfig_status_t
linklocal_thread(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *		if_p = service_interface(service_p);
    Service_linklocal_t *	linklocal;
    ipconfig_status_t		status = ipconfig_status_success_e;

    linklocal = (Service_linklocal_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  ipconfig_method_data_t	method_data;
	  char				timer_name[32];

	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (linklocal != NULL) {
	      my_log(LOG_NOTICE, "LINKLOCAL %s: re-entering start state",
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  my_log(LOG_INFO, "LINKLOCAL %s: start", if_name(if_p));
	  linklocal = malloc(sizeof(*linklocal));
	  bzero(linklocal, sizeof(*linklocal));
	  ServiceSetPrivate(service_p, linklocal);

	  snprintf(timer_name, sizeof(timer_name), "linklocal-%s",
		   if_name(if_p));
	  linklocal->timer = timer_callout_init(timer_name);
	  if (linklocal->timer == NULL) {
	      my_log(LOG_NOTICE, "LINKLOCAL %s: timer_callout_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  linklocal->arp = arp_client_init(G_arp_session, if_p);
	  if (linklocal->arp == NULL) {
	      my_log(LOG_NOTICE, "LINKLOCAL %s: arp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  /* ARP probes count as collisions for link-local address allocation */
	  arp_client_set_probes_are_collisions(linklocal->arp, TRUE);
	  linklocal->allocate = TRUE;

	  method_data = (ipconfig_method_data_t)event_data;
	  if (method_data != NULL 
	      && method_data->linklocal.allocate == FALSE) {
	      /* don't allocate an IP address, just set the subnet */
	      linklocal->allocate = FALSE;
	      linklocal_detect_proxy_arp(service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  linklocal->our_ip = S_find_linklocal_address(service_p);
	  linklocal_allocate(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_INFO, "LINKLOCAL %s: stop", if_name(if_p));
	  if (linklocal == NULL) {
	      my_log(LOG_INFO, "LINKLOCAL %s: already stopped", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }

	  /* remove IP address */
	  arp_linklocal_disable(if_name(if_p));
	  service_remove_address(service_p);

	  /* clean-up the published state */
	  service_publish_failure(service_p, 
				  ipconfig_status_success_e);

	  /* clean-up resources */
	  if (linklocal->timer) {
	      timer_callout_free(&linklocal->timer);
	  }
	  if (linklocal->arp) {
	      arp_client_free(&linklocal->arp);
	  }
	  if (linklocal) {
	      free(linklocal);
	  }
	  ServiceSetPrivate(service_p, NULL);
	  break;
      }
      case IFEventID_change_e: {
	  boolean_t		  	allocate = TRUE;
	  change_event_data_t *   	change_event;
	  ipconfig_method_data_t 	method_data;

	  change_event = (change_event_data_t *)event_data;
	  method_data = change_event->method_data;
	  if (method_data != NULL 
	      && method_data->linklocal.allocate == FALSE) {
	      /* don't allocate an IP address, just set the subnet */
	      allocate = FALSE;
	  }
	  if (linklocal->allocate != allocate) {
	      linklocal->allocate = allocate;
	      if (allocate) {
		  linklocal->our_ip = S_find_linklocal_address(service_p);
		  linklocal_allocate(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  linklocal_failed(service_p, ipconfig_status_success_e);
		  linklocal_detect_proxy_arp(service_p,
					     IFEventID_start_e, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;

	  arpc = (arp_collision_data_t *)event_data;
	  if (linklocal == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (linklocal->allocate == FALSE) {
	      break;
	  }
	  if (linklocal->enable_arp_collision_detection == FALSE
	      || arpc->ip_addr.s_addr != linklocal->our_ip.s_addr) {
	      break;
	  }
	  linklocal->our_ip = G_ip_zeroes;
	  (void)service_remove_address(service_p);
	  service_publish_failure(service_p, 
				  ipconfig_status_address_in_use_e);
	  linklocal_allocate(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_wake_e:
      case IFEventID_link_status_changed_e: {
	  link_status_t	   link_status;

	  if (linklocal == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE && link_status.active == FALSE) {
	      linklocal_cancel_pending_events(service_p);
	      linklocal->enable_arp_collision_detection = FALSE;
	  }
	  else {
	      linklocal_start(service_p);
	  }
	  break;
      }
      case IFEventID_link_timer_expired_e:
	  linklocal_inactive(service_p);
	  break;

      case IFEventID_renew_e: {
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}
