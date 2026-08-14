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
 * bootp.c
 * - BOOTP configuration thread
 * - contains bootp_thread()
 * - configures an interface's address using BOOTP
 * - once the address is retrieved, the client probes the address using
 *   ARP to ensure that another client isn't already using the address
 */
/* 
 * Modification History
 *
 * May 9, 2000		Dieter Siegmund (dieter@apple.com)
 * - reworked to fit within the new event-driven framework
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
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
#include <net/if_types.h>
#include <syslog.h>

#include "rfc_options.h"
#include "dhcp_options.h"
#include "dhcp.h"
#include "interfaces.h"
#include "util.h"
#include "host_identifier.h"
#include "dhcplib.h"
#include "ipconfigd_threads.h"

typedef struct {
    boolean_t			gathering;
    struct bootp		request;
    struct saved_pkt		saved;
    long			start_secs;
    int				try;
    int				wait_secs;
    u_int32_t			xid;
    timer_callout_t *		timer;
    arp_client_t *		arp;
    bootp_client_t *		client;
    boolean_t			user_warned;
    boolean_t			enable_arp_collision_detection;
    boolean_t			resolve_router_timed_out;
} Service_bootp_t;

/* tags_search: these are the tags we look for using BOOTP */
static const uint8_t       	bootp_params[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
};
#define N_BOOTP_PARAMS	(sizeof(bootp_params) / sizeof(bootp_params[0]))
#define IDEAL_RATING	N_BOOTP_PARAMS

static void 
bootp_request(ServiceRef service_p, IFEventID_t evid, void * event_data);

/*
 * Function: make_bootp_request
 * Purpose:
 *   Create a "blank" bootp packet.
 */
static void
make_bootp_request(struct bootp * pkt, 
		   uint8_t * hwaddr, uint8_t hwtype, uint8_t hwlen)
{
    bzero(pkt, sizeof (*pkt));
    pkt->bp_op = BOOTREQUEST;
    pkt->bp_htype = hwtype;
    pkt->bp_hlen = hwlen;
    if (G_must_broadcast)
	pkt->bp_unused = htons(DHCP_FLAGS_BROADCAST);
    bcopy(hwaddr, pkt->bp_chaddr, hwlen);
    bcopy(G_rfc_magic, pkt->bp_vend, sizeof(G_rfc_magic));
    pkt->bp_vend[4] = dhcptag_end_e;
    return;
}

static void
bootp_set_dhcp_info(Service_bootp_t * bootp, dhcp_info_t * dhcp_info_p)
{
    dhcp_info_p->pkt = (uint8_t *)bootp->saved.pkt;
    dhcp_info_p->pkt_size = bootp->saved.pkt_size;
    dhcp_info_p->options = &bootp->saved.options;
    dhcp_info_p->lease_start = 0;
    dhcp_info_p->lease_expiration = 0;
    return;
}

static void
bootp_publish_success(ServiceRef service_p)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    dhcp_info_t		dhcp_info;

    bootp_set_dhcp_info(bootp, &dhcp_info);
    ServicePublishSuccessIPv4(service_p, &dhcp_info);
    return;
}

static void
S_cancel_pending_events(ServiceRef service_p)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);

    if (bootp == NULL)
	return;
    if (bootp->timer) {
	timer_cancel(bootp->timer);
    }
    if (bootp->client) {
	bootp_client_disable_receive(bootp->client);
    }
    if (bootp->arp) {
	arp_client_cancel(bootp->arp);
    }
    return;
}

static void
bootp_resolve_router_callback(ServiceRef service_p,
			      router_arp_status_t status);

static void
bootp_resolve_router_retry(void * arg0, void * arg1, void * arg2)
{
    Service_bootp_t *	bootp;
    ServiceRef 		service_p = (ServiceRef)arg0;

    bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    service_resolve_router(service_p, bootp->arp,
			   bootp_resolve_router_callback,
			   bootp->saved.our_ip);
    return;
}

static void
bootp_resolve_router_callback(ServiceRef service_p,
			      router_arp_status_t status)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    switch (status) {
    case router_arp_status_no_response_e:
	/* try again in 60 seconds */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	timer_set_relative(bootp->timer, tv, 
			   (timer_func_t *)bootp_resolve_router_retry,
			   service_p, NULL, NULL);
	if (bootp->resolve_router_timed_out) {
	    break;
	}
	/* publish what we have so far */
	bootp->resolve_router_timed_out = TRUE;
	bootp_publish_success(service_p);
	break;
    case router_arp_status_success_e:
	bootp->resolve_router_timed_out = FALSE;
	bootp_publish_success(service_p);
	break;
    default:
    case router_arp_status_failed_e:
	break;
    }
}

static void
bootp_success(ServiceRef service_p)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    struct in_addr	mask = {0};
    void *		option;
    /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
    struct bootp *	reply = (struct bootp *)(void *)bootp->saved.pkt;

    S_cancel_pending_events(service_p);
    option = dhcpol_find_with_length(&bootp->saved.options,
				     dhcptag_subnet_mask_e,
				     sizeof(mask));
    if (option != NULL) {
	mask = *((struct in_addr *)option);
    }
    if (bootp->saved.our_ip.s_addr
	&& reply->bp_yiaddr.s_addr != bootp->saved.our_ip.s_addr) {
	(void)service_remove_address(service_p);
    }
    bootp->try = 0;
    bootp->enable_arp_collision_detection = TRUE;
    bootp->saved.our_ip = reply->bp_yiaddr;
    (void)service_set_address(service_p, bootp->saved.our_ip, 
			      mask, G_ip_zeroes);
    bootp->resolve_router_timed_out = FALSE;
    if (service_update_router_address(service_p, &bootp->saved.options,
				      bootp->saved.our_ip)
	&& service_resolve_router(service_p, bootp->arp,
				  bootp_resolve_router_callback,
				  bootp->saved.our_ip)) {
	/* router resolution started */
    }
    else {
	bootp_publish_success(service_p);
    }
    return;
}

static void
bootp_failed(ServiceRef service_p, ipconfig_status_t status, char * msg)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    S_cancel_pending_events(service_p);
    dhcpol_free(&bootp->saved.options);
    service_remove_address(service_p);
    (void)service_disable_autoaddr(service_p);
    bootp->saved.our_ip.s_addr = 0;
    bootp->try = 0;
    bootp->enable_arp_collision_detection = FALSE;
    service_publish_failure(service_p, status);

    if (status != ipconfig_status_media_inactive_e) {
	/* retry BOOTP again in a bit */
#define RETRY_INTERVAL_SECS      (2 * 60)
	tv.tv_sec = RETRY_INTERVAL_SECS;
	tv.tv_usec = 0;
	timer_set_relative(bootp->timer, tv, 
			   (timer_func_t *)bootp_request,
			   service_p, (void *)IFEventID_start_e, NULL);
    }

    return;
}

static void
bootp_arp_probe(ServiceRef service_p,  IFEventID_t evid, void * event_data)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);

    switch (evid) {
      case IFEventID_start_e: {
	  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
	  struct bootp *	reply = (struct bootp *)(void *)bootp->saved.pkt;

	  my_log(LOG_INFO, "BOOTP %s: ended at %ld", if_name(if_p),
		 timer_current_secs() - bootp->start_secs);
	  (void)service_disable_autoaddr(service_p);
	  bootp_client_disable_receive(bootp->client);
	  timer_cancel(bootp->timer);
	  arp_client_cancel(bootp->arp);
	  arp_client_probe(bootp->arp, 
			   (arp_result_func_t *)bootp_arp_probe, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   reply->bp_yiaddr);
	  return;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_NOTICE, "BOOTP %s: arp probe failed, %s",
		     if_name(if_p),
		     arp_client_errmsg(bootp->arp));
	      bootp_failed(service_p, ipconfig_status_internal_error_e, NULL);
	      return;
	  }
	  else {
	      /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
	      struct bootp *	reply = 
			(struct bootp *)(void *)bootp->saved.pkt;
	      if (result->in_use) {
		  char	msg[128];
		  
		  snprintf(msg, sizeof(msg),
			   IP_FORMAT " in use by " 
			   EA_FORMAT ", BOOTP Server " IP_FORMAT,
			   IP_LIST(&reply->bp_yiaddr),
			   EA_LIST(result->addr.target_hardware),
			   IP_LIST(&reply->bp_siaddr));
		  if (bootp->user_warned == FALSE) {
		      ServiceReportIPv4AddressConflict(service_p,
						       reply->bp_yiaddr);
		      bootp->user_warned = TRUE;
		  }
		  syslog(LOG_NOTICE, "BOOTP %s: %s", if_name(if_p), msg);
		  bootp_failed(service_p, ipconfig_status_address_in_use_e,
			       msg);
		  break;
	      }
	  }
	  bootp_success(service_p);
	  break;
      }
      default:
	  break;
    }
    return;
}

static void 
bootp_request(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: 
	  my_log(LOG_INFO, "BOOTP %s: starting", if_name(if_p));
	  (void)service_enable_autoaddr(service_p);
	  S_cancel_pending_events(service_p);
	  bootp->start_secs = timer_current_secs();
	  bootp->wait_secs = G_initial_wait_secs;
	  bootp->gathering = FALSE;
	  bootp->saved.pkt_size = 0;
	  bootp->saved.rating = 0;
	  bootp->enable_arp_collision_detection = FALSE;
	  dhcpol_free(&bootp->saved.options);
	  make_bootp_request(&bootp->request, if_link_address(if_p), 
			     if_link_arptype(if_p),
			     if_link_length(if_p));
	  bootp->try = 0;
	  bootp->xid++;
	  bootp_client_enable_receive(bootp->client,
				      (bootp_receive_func_t *)bootp_request, 
				      service_p, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      case IFEventID_timeout_e:
	  if (bootp->gathering == TRUE) {
	      bootp_arp_probe(service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  bootp->try++;
	  if (bootp->try > 1) {
	      link_status_t	link_status = service_link_status(service_p);

	      if (link_status.valid 
		  && link_status.active == FALSE) {
		  bootp_failed(service_p, ipconfig_status_media_inactive_e,
			       NULL);
		  break;
	      }
	  }
	  if (bootp->try > (G_max_retries + 1)) {
	      bootp_failed(service_p, ipconfig_status_no_server_e, NULL);
	      break;
	  }
	  bootp->request.bp_secs 
	    = htons((uint16_t)(timer_current_secs() - bootp->start_secs));
	  bootp->request.bp_xid = htonl(bootp->xid);
	  /* send the packet */
	  if (bootp_client_transmit(bootp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    &bootp->request, 
				    sizeof(bootp->request)) < 0) {
	      my_log(LOG_NOTICE,
		     "BOOTP %s: transmit failed", if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = bootp->wait_secs;
	  tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_INFO, "BOOTP %s: waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 timer_current_secs() - bootp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(bootp->timer, tv, 
			     (timer_func_t *)bootp_request,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  bootp->wait_secs = (int)tv.tv_sec * 2;
	  if (bootp->wait_secs > G_max_wait_secs)
	      bootp->wait_secs = G_max_wait_secs;
	  break;

      case IFEventID_data_e: {
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  unsigned 		rating;
	  struct bootp *	reply;

	  reply = (struct bootp *)pkt->data;
	  if ((ip_valid(reply->bp_yiaddr) == FALSE
	       && ip_valid(reply->bp_ciaddr) == FALSE)
	      || dhcp_packet_match(reply, bootp->xid,
				   (uint8_t) if_link_arptype(if_p),
				   if_link_address(if_p),
				   if_link_length(if_p)) == FALSE) {
	      /* not an interesting packet, drop the packet */
	      break; /* out of case */
	  }
	  rating = dhcpol_count_params(&pkt->options, 
				       bootp_params, N_BOOTP_PARAMS);
	  if (bootp->saved.pkt_size == 0
	      || rating > bootp->saved.rating) {
	      dhcpol_free(&bootp->saved.options);
	      bcopy(pkt->data, bootp->saved.pkt, pkt->size);
	      bootp->saved.pkt_size = pkt->size;
	      bootp->saved.rating = rating;
	      dhcpol_parse_packet(&bootp->saved.options, 
				  (void *)bootp->saved.pkt, 
				  bootp->saved.pkt_size,
				  NULL);
	      if (rating == IDEAL_RATING) {
		  bootp_arp_probe(service_p, IFEventID_start_e, NULL);
		  break;
	      }
	      if (bootp->gathering == FALSE) {
		  struct timeval t = {0,0};
		  t.tv_sec = G_gather_secs;
		  my_log(LOG_INFO, "BOOTP %s: gathering began at %ld",
			 if_name(if_p), 
			 timer_current_secs() - bootp->start_secs);
		  bootp->gathering = TRUE;
		  timer_set_relative(bootp->timer, t, 
				     (timer_func_t *)bootp_request,
				     service_p, (void *)IFEventID_timeout_e, 
				     NULL);
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;
}

static void
bootp_inactive(ServiceRef service_p)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);

    S_cancel_pending_events(service_p);
    service_remove_address(service_p);
    (void)service_disable_autoaddr(service_p);
    dhcpol_free(&bootp->saved.options);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e);
    return;
}

ipconfig_status_t
bootp_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    Service_bootp_t *	bootp = (Service_bootp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (evid) {
      case IFEventID_start_e: 
	  if (bootp != NULL) {
	      my_log(LOG_NOTICE, "BOOTP %s: re-entering start state",
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  bootp = malloc(sizeof(*bootp));
	  ServiceSetPrivate(service_p, bootp);
	  bzero(bootp, sizeof(*bootp));
	  dhcpol_init(&bootp->saved.options);
	  bootp->xid = arc4random();
	  bootp->timer = timer_callout_init("bootp");
	  if (bootp->timer == NULL) {
	      my_log(LOG_NOTICE, "BOOTP %s: timer_callout_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  (void)service_enable_autoaddr(service_p);
	  bootp->client = bootp_client_init(G_bootp_session, if_p);
	  if (bootp->client == NULL) {
	      my_log(LOG_NOTICE, "BOOTP %s: bootp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  bootp->arp = arp_client_init(G_arp_session, if_p);
	  if (bootp->arp == NULL) {
	      my_log(LOG_NOTICE, "BOOTP %s: arp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  bootp_request(service_p, IFEventID_start_e, NULL);
	  break;
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_INFO, "BOOTP %s: stop", if_name(if_p));

	  if (bootp == NULL) { /* already stopped */
	      my_log(LOG_INFO, "BOOTP %s: already stopped", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  /* disable reception of packets */
	  (void)service_disable_autoaddr(service_p);

	  /* clean-up resources */
	  if (bootp->timer) {
	      timer_callout_free(&bootp->timer);
	  }
	  if (bootp->client) {
	      bootp_client_free(&bootp->client);
	  }
	  if (bootp->arp) {
	      arp_client_free(&bootp->arp);
	  }
	  dhcpol_free(&bootp->saved.options);
	  if (bootp)
	      free(bootp);
	  ServiceSetPrivate(service_p, NULL);
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];
	  struct bootp *		reply;

	  arpc = (arp_collision_data_t *)event_data;

	  if (bootp == NULL) {
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  if (bootp->saved.pkt_size == 0 
	      || bootp->saved.our_ip.s_addr != arpc->ip_addr.s_addr
	      || bootp->enable_arp_collision_detection == FALSE) {
	      break;
	  }

	  /* defend our address, don't just give it up */
	  if (ServiceDefendIPv4Address(service_p, arpc)) {
	      break;
	  }

	  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
	  reply = (struct bootp *)(void *)bootp->saved.pkt;
	  snprintf(msg, sizeof(msg),
		   IP_FORMAT " in use by " 
		   EA_FORMAT ", BOOTP Server " IP_FORMAT,
		   IP_LIST(&reply->bp_yiaddr),
		   EA_LIST(arpc->hwaddr),
		   IP_LIST(&reply->bp_siaddr));
	  if (bootp->user_warned == FALSE) {
	      ServiceReportIPv4AddressConflict(service_p,
					       reply->bp_yiaddr);
	      bootp->user_warned = TRUE;
	  }
	  syslog(LOG_NOTICE, "BOOTP %s: %s", if_name(if_p), msg);
	  bootp_failed(service_p, ipconfig_status_address_in_use_e,
		       msg);
	  break;
      }
      case IFEventID_wake_e:
      case IFEventID_renew_e:
      case IFEventID_link_status_changed_e: {
	  link_event_data_t	link_event = (link_event_data_t)event_data;
	  link_status_t		link_status;

	  if (bootp == NULL) {
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  if ((link_event->flags & kLinkFlagsSSIDChanged) != 0) {
	      /* switched networks, remove IP address to avoid IP collisions */
	      (void)service_remove_address(service_p);
	  }
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE && link_status.active == FALSE) {
	      /* ensure that we'll retry when the link goes back up */
	      bootp->try = 0;

	      /* disallow collision detection while disconnected */
	      bootp->enable_arp_collision_detection = FALSE;

	      S_cancel_pending_events(service_p);
	  }
	  else {
	      /* confirm an address, get a new one, or timeout */
	      bootp->user_warned = FALSE;
	      if (bootp->try != 1) {
		  bootp_request(service_p, IFEventID_start_e, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_link_timer_expired_e:
	  bootp_inactive(service_p);
	  break;
      case IFEventID_get_dhcp_info_e:
	  if (ServiceGetActiveIPAddress(service_p).s_addr == 0
	      || bootp->saved.pkt_size == 0) {
	      break;
	  }
	  bootp_set_dhcp_info(bootp, (dhcp_info_t *)event_data);
	  break;
      default:
	  break;
    } /* switch */
    return (status);
}
