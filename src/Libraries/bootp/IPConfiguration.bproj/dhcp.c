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
 * dhcp.c
 * - DHCP configuration threads
 * - contains dhcp_thread() and inform_thread()
 */
/* 
 * Modification History
 *
 * May 16, 2000		Dieter Siegmund (dieter@apple.com)
 * - reworked to fit within the new event-driven framework
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
 * - modified INFORM to process link change events as well
 *
 * February 1, 2002	Dieter Siegmund (dieter@apple.com)
 * - changes for NetBoot
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
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include "rfc_options.h"
#include "dhcp_options.h"
#include "dhcp.h"
#include "interfaces.h"
#include "util.h"
#include <net/if_types.h>
#include "host_identifier.h"
#include "dhcplib.h"

#include "ipconfigd_threads.h"
#include "DHCPLease.h"
#include "symbol_scope.h"
#include "cfutil.h"

#define SUGGESTED_LEASE_LENGTH		(60 * 60 * 24 * 30 * 3) /* 3 months */

typedef struct {
    boolean_t			valid;
    absolute_time_t		expiration;
    dhcp_lease_time_t		length;
    absolute_time_t		start;
    absolute_time_t		t1;
    absolute_time_t		t2;
    boolean_t			needs_write;
    CFStringRef			ssid;
} lease_info_t;

typedef struct {
    boolean_t		allow_wake_with_short_lease;
    arp_client_t *	arp;
    bootp_client_t *	client;
    void *		client_id;
    int			client_id_len;
    struct in_addr	conflicting_address;
    boolean_t		disable_arp_collision_detection;
    boolean_t		gathering;
    boolean_t		got_nak;
    lease_info_t	lease;
    DHCPLeaseList	lease_list;
    boolean_t		must_broadcast;
    struct dhcp * 	request;
    int			request_size;
    absolute_time_t	renew_rebind_time;
    boolean_t		resolve_router_timed_out;
    struct saved_pkt	saved;
    dhcp_cstate_t	state;
    absolute_time_t	start_secs;
    timer_callout_t *	timer;
    int			try;
    uint32_t		txbuf[DHCP_PACKET_MIN/sizeof(uint32_t)];
    u_int32_t		xid;
    boolean_t		user_warned;
    int			wait_secs;
    absolute_time_t	wake_time;
} Service_dhcp_t;

typedef struct {
    arp_client_t *	arp;
    bootp_client_t *	client;
    boolean_t		gathering;
    struct in_addr	our_mask;
    struct dhcp * 	request;
    int			request_size;
    boolean_t		resolve_router_timed_out;
    struct saved_pkt	saved;
    absolute_time_t	start_secs;
    timer_callout_t *	timer;
    int			try;
    uint32_t		txbuf[DHCP_PACKET_MIN/sizeof(uint32_t)];
    u_int32_t		xid;
    boolean_t		user_warned;
    int			wait_secs;
} Service_inform_t;

#define ROUTER_INVALID	-1

INLINE boolean_t
router_is_valid(struct in_addr router)
{
    return (router.s_addr != ROUTER_INVALID && router.s_addr != 0);
}

INLINE void
router_set_invalid(struct in_addr * router_p)
{
    router_p->s_addr = ROUTER_INVALID;
    return;
}

STATIC struct in_addr
dhcp_get_router_from_options(dhcpol_t * options_p, struct in_addr our_ip);

static void
dhcp_init(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_init_reboot(ServiceRef service_p, IFEventID_t event_id, 
		 void * event_data);

static void
dhcp_arp_router(ServiceRef service_p, IFEventID_t event_id, 
		void * event_data);

static void
dhcp_select(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_bound(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_renew_rebind(ServiceRef service_p, IFEventID_t event_id, 
		  void * event_data);

static void
dhcp_unbound(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_decline(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_release(ServiceRef service_p);

static void
dhcp_no_server(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static boolean_t
dhcp_check_lease(ServiceRef service_p, absolute_time_t current_time);

static void
dhcp_check_router(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static void
inform_no_server(ServiceRef service_p, IFEventID_t event_id, void * event_data);

static boolean_t
get_server_identifier(dhcpol_t * options, struct in_addr * server_ip)
{
    struct in_addr * 	ipaddr_p;

    ipaddr_p = (struct in_addr *) 
	dhcpol_find_with_length(options, dhcptag_server_identifier_e,
				sizeof(*ipaddr_p));
    if (ipaddr_p != NULL) {
	*server_ip = *ipaddr_p;
    }
    return (ipaddr_p != NULL);
}

#define DHCP_ADDRESS_RATING_POOR        0
#define DHCP_ADDRESS_RATING_FAIR        100
#define DHCP_ADDRESS_RATING_GOOD        1000

static
int
get_rating_for_ip_address(struct in_addr * iaddr)
{
    if (ip_is_private(*iaddr)) {
	return (DHCP_ADDRESS_RATING_FAIR);
    }

    if (ip_is_linklocal(*iaddr)) {
	return (DHCP_ADDRESS_RATING_POOR);
    }

    return (DHCP_ADDRESS_RATING_GOOD);
}

static const uint8_t dhcp_static_default_params[] = {
    dhcptag_subnet_mask_e, 
    dhcptag_classless_static_route_e,
    dhcptag_router_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_domain_search_e,
    dhcptag_proxy_auto_discovery_url_e,
    dhcptag_captive_portal_url_e,
#if TARGET_OS_OSX
    dhcptag_ldap_url_e,
    dhcptag_nb_over_tcpip_name_server_e,
    dhcptag_nb_over_tcpip_node_type_e,
#endif /* TARGET_OS_OSX */
};
#define	N_DHCP_STATIC_DEFAULT_PARAMS 	(sizeof(dhcp_static_default_params) / sizeof(dhcp_static_default_params[0]))

static uint8_t * dhcp_default_params = (uint8_t *)dhcp_static_default_params;
static int	n_dhcp_default_params = N_DHCP_STATIC_DEFAULT_PARAMS;

static uint8_t * dhcp_params = (uint8_t *)dhcp_static_default_params;
static int	n_dhcp_params = N_DHCP_STATIC_DEFAULT_PARAMS;

void
dhcp_set_default_parameters(uint8_t * params, int n_params)
{
    if (params && n_params) {
	dhcp_default_params = params;
	n_dhcp_default_params = n_params;
    }
    else {
	dhcp_default_params = (uint8_t *)dhcp_static_default_params;
	n_dhcp_default_params = N_DHCP_STATIC_DEFAULT_PARAMS;
    }
    dhcp_params = dhcp_default_params;
    n_dhcp_params = n_dhcp_default_params;
    return;
}

static uint8_t *
S_merge_parameters(uint8_t * params, int n_params, int * n_ret)
{
    int		i;
    uint8_t *	ret = dhcp_default_params;
    uint8_t *	new = NULL;
    int		new_end = 0;

    *n_ret = n_dhcp_default_params;
    if (params == NULL || n_params == 0) {
	goto done;
    }
    /* allocate the worst case size ie. no duplicates */
    new = (uint8_t *)malloc(n_dhcp_default_params + n_params);
    if (new == NULL) {
	goto done;
    }
    bcopy(dhcp_default_params, new, n_dhcp_default_params);
    for (i = 0, new_end = n_dhcp_default_params; i < n_params; i++) {
	boolean_t	already_there = FALSE;
	int 		j;

	for (j = 0; j < new_end; j++) {
	    if (new[j] == params[i]) {
		/* already in requested parameters list, ignore it */
		already_there = TRUE;
		break;
	    }
	}
	if (already_there == FALSE) {
	    new[new_end++] = params[i];
	}
    }
    if (new_end > n_dhcp_default_params) {
	ret = new;
	*n_ret = new_end;
    }
    else {
	free(new);
	new = NULL;
    }
 done:
    return (ret);
}

static void
S_print_char_array(CFMutableStringRef str, uint8_t * params, int n_params)
{
    int i;

    for (i = 0; i < n_params; i++) {
	if (i == 0)
	    STRING_APPEND(str, "%d", params[i]);
	else
	    STRING_APPEND(str, ", %d", params[i]);
    }
    return;
}

void
dhcp_set_additional_parameters(uint8_t * params, int n_params)
{
    if (dhcp_params && dhcp_params != dhcp_default_params) {
	free(dhcp_params);
    }
    dhcp_params = S_merge_parameters(params, n_params, &n_dhcp_params);
    if (params) {
	free(params);
    }
    if (G_IPConfiguration_verbose) {
	CFMutableStringRef	str;
	
	str = CFStringCreateMutable(NULL, 0);
	S_print_char_array(str, dhcp_params, n_dhcp_params);
	my_log(LOG_DEBUG, "DHCP requested parameters = { %@ }", str);
	CFRelease(str);
    }
    return;
}

bool
dhcp_parameter_is_ok(uint8_t param)
{
    int i;

    switch (param) {
    case dhcptag_lease_time_e:
    case dhcptag_dhcp_message_type_e:
    case dhcptag_server_identifier_e:
    case dhcptag_message_e:
    case dhcptag_renewal_t1_time_value_e:
    case dhcptag_rebinding_t2_time_value_e:
    case dhcptag_vendor_class_identifier_e:
    case dhcptag_client_identifier_e:
	return (TRUE);
    default:
	break;
    }
    for (i = 0; i < n_dhcp_params; i++) {
	if (dhcp_params[i] == param) {
	    return (TRUE);
	}
    }
    return (FALSE);
}


static void
add_computer_name(ServiceRef service_p, dhcpoa_t * options_p)
{
    char * name;

    if (!ServiceShouldSupplyHostName(service_p)) {
	return;
    }
    /* add the computer name as the host_name option */
    name = computer_name();
    if (name) {
	if (dhcpoa_add(options_p, dhcptag_host_name_e, (int)strlen(name), name)
	    != dhcpoa_success_e) {
	    my_log(LOG_NOTICE, "make_dhcp_request: couldn't add host_name, %s",
		   dhcpoa_err(options_p));
	}
    }
    return;
}

static struct dhcp * 
make_dhcp_request(struct dhcp * request, int pkt_size,
		  dhcp_msgtype_t msg, 
		  const uint8_t * hwaddr, uint8_t hwtype, uint8_t hwlen, 
		  const void * cid, int cid_len, boolean_t must_broadcast,
		  dhcpoa_t * options_p)
{
    char * 	buf = NULL;
    uint8_t 	cid_type = 0;

    /* if no client id was specified, use the hardware address */
    if (cid == NULL || cid_len == 0) {
	cid = hwaddr;
	cid_len = hwlen;
	cid_type = hwtype;
    }

    bzero(request, pkt_size);
    request->dp_htype = hwtype;
    request->dp_op = BOOTREQUEST;

    switch (hwtype) {
    default:
    case ARPHRD_ETHER:
	request->dp_hlen = hwlen;
	bcopy(hwaddr, request->dp_chaddr, hwlen);
	break;
    case ARPHRD_IEEE1394:
	request->dp_hlen = 0; /* RFC 2855 */
	if (cid == hwaddr) {
	    /* if client id is the hardware address, set the right type */
	    cid_type = ARPHRD_IEEE1394_EUI64;
	}
	break;
    }
    if (must_broadcast || G_must_broadcast) {
	request->dp_flags = htons(DHCP_FLAGS_BROADCAST);
    }
    bcopy(G_rfc_magic, request->dp_options, sizeof(G_rfc_magic));
    dhcpoa_init(options_p, request->dp_options + sizeof(G_rfc_magic),
		pkt_size - sizeof(struct dhcp) - sizeof(G_rfc_magic));
    
    /* make the request a dhcp message */
    if (dhcpoa_add_dhcpmsg(options_p, msg) != dhcpoa_success_e) {
	my_log(LOG_NOTICE,
	       "make_dhcp_request: couldn't add dhcp message tag %d, %s", msg,
	       dhcpoa_err(options_p));
	goto err;
    }

    if (msg != dhcp_msgtype_decline_e && msg != dhcp_msgtype_release_e) {
	u_int16_t	max_message_size = htons(1500); /* max receive size */

	/* add the list of required parameters */
	if (dhcpoa_add(options_p, dhcptag_parameter_request_list_e,
			n_dhcp_params, dhcp_params)
	    != dhcpoa_success_e) {
	    my_log(LOG_NOTICE, "make_dhcp_request: "
		   "couldn't add parameter request list, %s",
		   dhcpoa_err(options_p));
	    goto err;
	}
	/* add the max message size */
	if (dhcpoa_add(options_p, dhcptag_max_dhcp_message_size_e,
		       sizeof(max_message_size), &max_message_size)
	    != dhcpoa_success_e) {
	    my_log(LOG_NOTICE, "make_dhcp_request: "
		    "couldn't add max message size, %s",
		    dhcpoa_err(options_p));
	    goto err;
	}
    }

    /* add the client identifier to the request packet */
    buf = malloc(cid_len + 1);
    if (buf == NULL) {
	my_log(LOG_NOTICE, "make_dhcp_request: malloc failed, %s (%d)",
	       strerror(errno), errno);
	goto err;
    }
    *buf = cid_type;
    bcopy(cid, buf + 1, cid_len);
    if (dhcpoa_add(options_p, dhcptag_client_identifier_e, cid_len + 1, buf)
	!= dhcpoa_success_e) {
	free(buf);
	my_log(LOG_NOTICE, "make_dhcp_request: "
	       "couldn't add client identifier, %s",
	       dhcpoa_err(options_p));
	goto err;
    }
    free(buf);
    return (request);
  err:
    return (NULL);
}

/*
 * Function: verify_packet
 * Purpose:
 */
static boolean_t
verify_packet(bootp_receive_data_t * pkt, u_int32_t xid, interface_t * if_p, 
	      dhcp_msgtype_t * msgtype_p, struct in_addr * server_ip)
{
    if (dhcp_packet_match((struct bootp *)pkt->data, xid, 
			  (uint8_t) if_link_arptype(if_p),
			  if_link_address(if_p),
			  if_link_length(if_p))) {
	/* 
	 * A BOOTP packet should be one that doesn't contain
	 * a dhcp message.  Unfortunately, some stupid BOOTP servers
	 * are unaware of DHCP and RFC-standard options, and simply 
         * echo back what we sent in the options area.  This is the 
	 * reason for checking for DISCOVER, REQUEST and INFORM: they are
	 * invalid responses in the DHCP protocol, so we assume that 
	 * the server is blindly echoing what we send.
	 */
	if (is_dhcp_packet(&pkt->options, msgtype_p) == FALSE
	    || *msgtype_p == dhcp_msgtype_discover_e
	    || *msgtype_p == dhcp_msgtype_request_e
	    || *msgtype_p == dhcp_msgtype_inform_e) {
	    /* BOOTP packet */
	    return (FALSE);
	}
	server_ip->s_addr = 0;
	(void)get_server_identifier(&pkt->options, server_ip);
	/* matching DHCP packet */
	return (TRUE);
    }
    return (FALSE);
}

/**
 **
 ** INFORM Functions
 ** 
 */
static void
inform_set_dhcp_info(Service_inform_t * inform, dhcp_info_t * dhcp_info_p)
{
    bzero(dhcp_info_p, sizeof(*dhcp_info_p));
    if (inform->saved.pkt_size != 0) {
	dhcp_info_p->pkt = (uint8_t *)inform->saved.pkt;
	dhcp_info_p->pkt_size = inform->saved.pkt_size;
	dhcp_info_p->options = &inform->saved.options;
    }
    return;
}

static void
inform_publish_success(ServiceRef service_p)
{
    dhcp_info_t		dhcp_info;
    Service_inform_t *	inform;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    inform_set_dhcp_info(inform, &dhcp_info);
    ServicePublishSuccessIPv4(service_p, &dhcp_info);
    return;
}


static void
inform_cancel_pending_events(ServiceRef service_p)
{
    Service_inform_t *	inform;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    if (inform == NULL)
	return;
    if (inform->timer) {
	timer_cancel(inform->timer);
    }
    if (inform->client) {
	bootp_client_disable_receive(inform->client);
    }
    if (inform->arp) {
	arp_client_cancel(inform->arp);
    }
    return;
}

static void
inform_inactive(ServiceRef service_p)
{
    Service_inform_t *	inform;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    inform_cancel_pending_events(service_p);
    service_remove_address(service_p);
    dhcpol_free(&inform->saved.options);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e);
    return;
}

static void
inform_failed(ServiceRef service_p, ipconfig_status_t status)
{
    inform_cancel_pending_events(service_p);
    service_publish_failure(service_p, status);
    return;
}

static void
inform_resolve_router_callback(ServiceRef service_p,
			       router_arp_status_t status);

static void
inform_resolve_router_retry(void * arg0, void * arg1, void * arg2)
{
    Service_inform_t *	inform;
    ServiceRef 		service_p = (ServiceRef)arg0;

    inform  = (Service_inform_t *)ServiceGetPrivate(service_p);
    service_resolve_router(service_p, inform->arp,
			   inform_resolve_router_callback,
			   inform->saved.our_ip);
    return;
}

static void
inform_resolve_router_callback(ServiceRef service_p,
			       router_arp_status_t status)
{
    Service_inform_t *	inform;
    struct timeval	tv;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    switch (status) {
    case router_arp_status_no_response_e:
	/* try again in 60 seconds */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	timer_set_relative(inform->timer, tv, 
			   (timer_func_t *)inform_resolve_router_retry,
			   service_p, NULL, NULL);
	if (inform->resolve_router_timed_out) {
	    break;
	}
	/* publish what we have so far */
	inform->resolve_router_timed_out = TRUE;
	inform_publish_success(service_p);
	break;
    case router_arp_status_success_e:
	inform->resolve_router_timed_out = FALSE;
	inform_publish_success(service_p);
	break;
    default:
    case router_arp_status_failed_e:
	break;
    }
}

static void
inform_success(ServiceRef service_p)
{
    Service_inform_t *	inform;
    void *		option;
	
    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    option = dhcpol_find_with_length(&inform->saved.options,
				     dhcptag_subnet_mask_e,
				     sizeof(struct in_addr));
    if (service_requested_ip_mask(service_p).s_addr == 0
	&& option != NULL) {
	inform->our_mask = *((struct in_addr *)option);

	/* reset the interface address with the new mask */
	(void)service_set_address(service_p,
				  service_requested_ip_addr(service_p),
				  inform->our_mask,
				  G_ip_zeroes);
    }
    inform_cancel_pending_events(service_p);
    inform->resolve_router_timed_out = FALSE;
    if (service_update_router_address(service_p, &inform->saved.options,
				      inform->saved.our_ip)
	&& service_resolve_router(service_p, inform->arp,
				  inform_resolve_router_callback,
				  service_requested_ip_addr(service_p))) {
	/* router resolution started */
    }
    else {
	inform_publish_success(service_p);
    }
    return;
}

static void
inform_request(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t	current_time = timer_current_secs();
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform;
    ipconfig_status_t	status = ipconfig_status_success_e;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t		options;
	  
	  inform->start_secs = current_time;

	  /* clean-up anything that might have come before */
	  inform_cancel_pending_events(service_p);
	  /* ALIGN: txbuf is aligned to sizeof(uint32_t) bytes */
	  inform->request = make_dhcp_request((struct dhcp *)(void *)inform->txbuf, 
					      sizeof(inform->txbuf),
					      dhcp_msgtype_inform_e,
					      if_link_address(if_p), 
					      if_link_arptype(if_p),
					      if_link_length(if_p), 
					      NULL, 0,
					      FALSE,
					      &options);
	  if (inform->request == NULL) {
	      my_log(LOG_NOTICE, "INFORM %s: make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  inform->request->dp_ciaddr = service_requested_ip_addr(service_p);
	  add_computer_name(service_p, &options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "INFORM %s: failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  inform->request_size = sizeof(*inform->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (inform->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      inform->request_size = sizeof(struct bootp);
	  }
	  inform->try = 0;
	  inform->gathering = FALSE;
	  inform->wait_secs = G_initial_wait_secs;
	  bootp_client_enable_receive(inform->client,
				      (bootp_receive_func_t *)inform_request,
				      service_p, (void *)IFEventID_data_e);
	  inform->saved.rating = 0;
	  inform->xid++;
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct timeval 	tv;
	  if (inform->gathering == TRUE) {
	      /* done gathering */
	      inform_success(service_p);
	      return;
	  }
	  inform->try++;
	  if (inform->try > 1) {
	      link_status_t	link_status = service_link_status(service_p);

	      if (link_status.valid 
		  && link_status.active == FALSE) {
		  inform_inactive(service_p);
		  break; /* out of switch */
	      }
	  }
	  if (inform->try > (G_max_retries + 1)) {
	      inform_no_server(service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  inform->request->dp_xid = htonl(inform->xid);
	  inform->request->dp_secs 
	      = htons((uint16_t)(current_time - inform->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(inform->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    inform->request, 
				    inform->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "INFORM %s: transmit failed", if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = inform->wait_secs;
	  tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_INFO, "INFORM %s: waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 current_time - inform->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(inform->timer, tv, 
			     (timer_func_t *)inform_request,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  inform->wait_secs *= 2;
	  if (inform->wait_secs > G_max_wait_secs) {
	      inform->wait_secs = G_max_wait_secs;
	  }
	  break;
      }
      case IFEventID_data_e: {
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, inform->xid, if_p, &reply_msgtype,
			    &server_ip) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (reply_msgtype == dhcp_msgtype_ack_e) {
	      int rating = 0;
	      
	      rating = dhcpol_count_params(&pkt->options,
					   dhcp_params, n_dhcp_params);
	      /* 
	       * The new packet is "better" than the saved packet if there
	       * is no saved packet, or it has a higher rating.
	       */
	      if (inform->saved.pkt_size == 0
		  || rating > inform->saved.rating) {
		  dhcpol_free(&inform->saved.options);
		  bcopy(pkt->data, inform->saved.pkt, pkt->size);
		  inform->saved.pkt_size = pkt->size;
		  inform->saved.rating = rating;
		  /* ALIGN: saved.pkt is at least sizeof(uint32_t) aligned, 
		   * cast ok. */
		  (void)dhcpol_parse_packet(&inform->saved.options, 
					    (void *)inform->saved.pkt, 
					    inform->saved.pkt_size, NULL);
		  inform->saved.server_ip = server_ip;
		  if (rating == n_dhcp_params) {
		      inform_success(service_p);
		      return;
		  }
		  if (inform->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_INFO, "INFORM %s: gathering began at %ld",
			     if_name(if_p), 
			     current_time - inform->start_secs);
		      inform->gathering = TRUE;
		      timer_set_relative(inform->timer, t, 
					 (timer_func_t *)inform_request,
					 service_p, (void *)IFEventID_timeout_e,
					 NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;
 error:
    inform_failed(service_p, status);
    return;

}

static void
inform_no_server(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
#define INFORM_RETRY_INTERVAL_SECS      60
    Service_inform_t *	inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    struct timeval 	tv = {INFORM_RETRY_INTERVAL_SECS,0};
    
    switch (event_id) {
        case IFEventID_start_e: {
            inform_success(service_p);
            timer_set_relative(inform->timer, tv,
                               (timer_func_t *)inform_request,
                               service_p, (void *)IFEventID_start_e, NULL);
            break;
        }
        default: {
            break;
        }
    }
    return;
}


static void
inform_start(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  inform_cancel_pending_events(service_p);

	  arp_client_probe(inform->arp, 
			   (arp_result_func_t *)inform_start, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   service_requested_ip_addr(service_p));
	  break;
      }
      case IFEventID_arp_e: {
	  link_status_t		link_status;
	  struct in_addr	mask;
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_NOTICE, "INFORM %s: arp probe failed, %s",
		     if_name(if_p),
		     arp_client_errmsg(inform->arp));
	      inform_failed(service_p, ipconfig_status_internal_error_e);
	      /* hopefully a temporary failure, try again in a bit */
	      timer_callout_set(inform->timer, ARP_PROBE_FAILURE_RETRY_TIME,
				(timer_func_t *)inform_start,
				service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  else {
	      if (result->in_use) {
		  struct in_addr	requested_ip;
		  char			msg[128];
		  struct timeval	tv;

		  requested_ip = service_requested_ip_addr(service_p);
		  snprintf(msg, sizeof(msg),
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(&requested_ip),
			   EA_LIST(result->addr.target_hardware));
		  if (inform->user_warned == FALSE) {
		      inform->user_warned = TRUE;
		      ServiceReportIPv4AddressConflict(service_p,
						       requested_ip);
		  }
		  my_log(LOG_NOTICE, "INFORM %s: %s", if_name(if_p),
			 msg);
		  (void)service_remove_address(service_p);
		  inform_failed(service_p, ipconfig_status_address_in_use_e);
		  /* try again in a bit */
		  if (G_manual_conflict_retry_interval_secs > 0) {
		      tv.tv_sec = G_manual_conflict_retry_interval_secs;
		      tv.tv_usec = 0;
		      timer_set_relative(inform->timer, tv, 
					 (timer_func_t *)inform_start,
					 service_p, IFEventID_start_e, NULL);
		  }
		  break;
	      }
	  }
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE 
	      && link_status.active == FALSE) {
	      inform_inactive(service_p);
	      break;
	  }

	  /* assign the address so that we can send/receive DHCP traffic */
	  mask = (inform->our_mask.s_addr != 0)
	      ? inform->our_mask
	      : G_ip_broadcast;
	  (void)service_set_address(service_p,
				    service_requested_ip_addr(service_p),
				    mask,
				    G_ip_zeroes);
	  ServiceRemoveAddressConflict(service_p);
	  inform_request(service_p, IFEventID_start_e, 0);
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
inform_thread(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform;
    ipconfig_status_t	status = ipconfig_status_success_e;

    inform = (Service_inform_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
      case IFEventID_start_e: {
	  ipconfig_method_data_t	method_data;
	  char				timer_name[32];

	  method_data = (ipconfig_method_data_t)event_data;
	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (inform != NULL) {
	      my_log(LOG_NOTICE, "INFORM %s: re-entering start state",
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  inform = malloc(sizeof(*inform));
	  ServiceSetPrivate(service_p, inform);
	  bzero(inform, sizeof(*inform));
	  dhcpol_init(&inform->saved.options);
	  service_set_requested_ip_addr(service_p, method_data->manual.addr);
	  service_set_requested_ip_mask(service_p, method_data->manual.mask);
	  inform->our_mask = service_requested_ip_mask(service_p);
	  snprintf(timer_name, sizeof(timer_name), "inform-%s",
		   if_name(if_p));
	  inform->timer = timer_callout_init(timer_name);
	  if (inform->timer == NULL) {
	      my_log(LOG_NOTICE, "INFORM %s: timer_callout_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->client = bootp_client_init(G_bootp_session, if_p);
	  if (inform->client == NULL) {
	      my_log(LOG_NOTICE, "INFORM %s: bootp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->arp = arp_client_init(G_arp_session, if_p);
	  if (inform->arp == NULL) {
	      my_log(LOG_NOTICE, "INFORM %s: arp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  my_log(LOG_INFO, "INFORM %s: start", if_name(if_p));
	  inform->xid = arc4random();
	  inform_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_INFO, "INFORM %s: stop", if_name(if_p));
	  if (inform == NULL) { /* already stopped */
	      break;
	  }
	  /* remove IP address */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (inform->timer) {
	      timer_callout_free(&inform->timer);
	  }
	  if (inform->client) {
	      bootp_client_free(&inform->client);
	  }
	  if (inform->arp) {
	      arp_client_free(&inform->arp);
	  }
	  dhcpol_free(&inform->saved.options);
	  if (inform)
	      free(inform);
	  ServiceSetPrivate(service_p, NULL);
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   	change_event;
	  ipconfig_method_data_t 	method_data;

	  if (inform == NULL) {
	      my_log(LOG_INFO, "INFORM %s: private data is NULL", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  change_event = ((change_event_data_t *)event_data);
	  method_data = change_event->method_data;
	  change_event->needs_stop = FALSE;
	  if (method_data->manual.addr.s_addr
	      != service_requested_ip_addr(service_p).s_addr) {
	      change_event->needs_stop = TRUE;
	  }
	  else if (method_data->manual.mask.s_addr != 0
		   && (method_data->manual.mask.s_addr 
		       != service_requested_ip_mask(service_p).s_addr)) {
	      service_set_requested_ip_mask(service_p, 
					    method_data->manual.mask);
	      inform->our_mask = method_data->manual.mask;
	      (void)service_set_address(service_p,
					method_data->manual.addr,
					inform->our_mask, G_ip_zeroes);
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];

	  arpc = (arp_collision_data_t *)event_data;
	  if (inform == NULL) {
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
	  if (inform->user_warned == FALSE) {
	      inform->user_warned = TRUE;
	      ServiceReportIPv4AddressConflict(service_p,
					       arpc->ip_addr);
	  }
	  my_log(LOG_NOTICE, "INFORM %s: %s", if_name(if_p),
		 msg);
	  break;
      }
      case IFEventID_wake_e:
      case IFEventID_renew_e:
      case IFEventID_link_status_changed_e: {
	  link_status_t	link_status;

	  if (inform == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  inform->user_warned = FALSE;
	  link_status = service_link_status(service_p);
	  if (link_status.valid == TRUE && link_status.active == FALSE) {
	      inform_cancel_pending_events(service_p);
	  }
	  else if (event_id != IFEventID_wake_e
		   || ServiceIsPublished(service_p) == FALSE) {
	      inform_start(service_p, IFEventID_start_e, 0);
	  }
	  break;
      }
      case IFEventID_link_timer_expired_e:
	  inform_inactive(service_p);
	  break;
      case IFEventID_get_dhcp_info_e: {
	  if (ServiceGetActiveIPAddress(service_p).s_addr == 0
	      || inform->saved.pkt_size == 0) {
	      break;
	  }
	  inform_set_dhcp_info(inform, (dhcp_info_t *)event_data);
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

/**
 **
 ** DHCP Functions
 ** 
 */

/*
 * Function: S_time_in_future
 * Purpose:
 *   Returns whether the given time is in the future by at least the
 *   time interval specified by 'time_interval'.
 */
INLINE boolean_t
S_time_in_future(absolute_time_t current_time,
		 absolute_time_t the_time,
		 uint32_t time_interval)
{
    return (current_time < the_time
	    && (the_time - current_time) >= time_interval);
}

/*
 * Function: S_min_wake_ok
 * Purpose:
 *   Returns whether the given time is in the future by at least the
 *   the minimum wake interval.
 */
INLINE boolean_t
S_min_wake_ok(absolute_time_t current_time, absolute_time_t the_time)
{
    return (S_time_in_future(current_time, the_time,
			     G_min_wake_interval_secs));
}

/*
 * Function: S_short_wake_ok
 * Purpose:
 *   Returns whether the given time is in the future by at least the minimum
 *   schedulable short wake.
 */
INLINE boolean_t
S_short_wake_ok(absolute_time_t current_time, absolute_time_t the_time)
{
    return (S_time_in_future(current_time, the_time, 
			     G_min_short_wake_interval_secs));
}

static void
dhcp_set_dhcp_info(Service_dhcp_t * dhcp, dhcp_info_t * dhcp_info_p)
{
    dhcp_info_p->pkt = (uint8_t *)dhcp->saved.pkt;
    dhcp_info_p->pkt_size = dhcp->saved.pkt_size;
    dhcp_info_p->options = &dhcp->saved.options;
    dhcp_info_p->lease_start = dhcp->lease.start;
    dhcp_info_p->lease_expiration = dhcp->lease.expiration;
    return;
}

static void
dhcp_publish_success(ServiceRef service_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    dhcp_info_t		dhcp_info;

    dhcp_set_dhcp_info(dhcp, &dhcp_info);
    ServicePublishSuccessIPv4(service_p, &dhcp_info);
    return;
}

static void
dhcp_cancel_pending_events(ServiceRef service_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);

    if (dhcp == NULL)
	return;
    timer_cancel(dhcp->timer);
    bootp_client_disable_receive(dhcp->client);
    arp_client_cancel(dhcp->arp);
    return;
}

static void
dhcp_failed(ServiceRef service_p, ipconfig_status_t status)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);

    dhcp_cancel_pending_events(service_p);

    service_disable_autoaddr(service_p);
    dhcpol_free(&dhcp->saved.options);
    service_remove_address(service_p);
    service_publish_failure(service_p, status);
    dhcp->state = dhcp_cstate_inactive_e;
    dhcp->allow_wake_with_short_lease = FALSE;
    ServiceSetBusy(service_p, FALSE);
    return;
}

static void
dhcp_inactive(ServiceRef service_p)
{
    Service_dhcp_t * 	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);

    dhcp_cancel_pending_events(service_p);
    /*
     * Set the status here so that the link-local service will disappear 
     * when we call service_remove_address.
     */
    ServiceSetStatus(service_p, ipconfig_status_media_inactive_e);
    service_remove_address(service_p);
    service_disable_autoaddr(service_p);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e);
    dhcp->state = dhcp_cstate_inactive_e;
    dhcp->allow_wake_with_short_lease = FALSE;
    ServiceSetBusy(service_p, FALSE);
    return;
}

static void
dhcp_set_lease_params(ServiceRef service_p, char * descr,
		      absolute_time_t lease_start,
		      dhcp_lease_time_t lease, dhcp_lease_time_t t1,
		      dhcp_lease_time_t t2)
{
    Service_dhcp_t * dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *    if_p = service_interface(service_p);

    dhcp->lease.start = lease_start;
    dhcp->lease.length = lease;

    if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
	dhcp->lease.t1 = dhcp->lease.t2 = dhcp->lease.expiration = 0;
    }
    else {
	dhcp->lease.expiration = dhcp->lease.start + dhcp->lease.length;
	dhcp->lease.t1 = dhcp->lease.start + t1;
	dhcp->lease.t2 = dhcp->lease.start + t2;
    }
    my_log(LOG_INFO, 
	   "DHCP %s: %s "
	   "lease = { start 0x%lx, t1 0x%lx, t2 0x%lx, expiration 0x%lx }", 
	   if_name(if_p), descr, dhcp->lease.start, 
	   dhcp->lease.t1, dhcp->lease.t2, dhcp->lease.expiration);
    return;
}

/* 
 * Function: dhcp_adjust_lease_info
 * Purpose:
 *    Apply the delta to the lease information: start, t1, t2, and expiration.
 */
static void
dhcp_adjust_lease_info(ServiceRef service_p, absolute_time_t delta)
{
    Service_dhcp_t * 	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *    	if_p = service_interface(service_p);
    lease_info_t *   	info_p = &dhcp->lease;

    info_p->expiration += delta;
    info_p->start += delta;
    info_p->t1 += delta;
    info_p->t2 += delta;
    my_log(LOG_INFO, 
	   "DHCP %s: adjusted lease by %ld seconds", if_name(if_p), delta);
    return;
}


STATIC void
dhcp_log_lease(int level, const char * ifname, absolute_time_t current_time,
	       lease_info_t * lease_p)

{
    my_log(level, 
	   "DHCP %s: now = 0x%lx, "
	   "lease = { start 0x%lx, t1 0x%lx, t2 0x%lx, expiration 0x%lx }", 
	   ifname, current_time,
	   lease_p->start, lease_p->t1, lease_p->t2, lease_p->expiration);
    return;
}

static void
get_client_id(Service_dhcp_t * dhcp, interface_t * if_p, const void * * cid_p, 
	      uint8_t * cid_type_p, int * cid_length_p)
{
    if (dhcp->client_id != NULL) {
	*cid_type_p = 0;
	*cid_p = dhcp->client_id;
	*cid_length_p = dhcp->client_id_len;
    }
    else {
	*cid_type_p = if_link_arptype(if_p);
	*cid_p = if_link_address(if_p);
	*cid_length_p = if_link_length(if_p);
    }
    return;
}

static void
dhcp_lease_set_ssid(Service_dhcp_t * dhcp, CFStringRef ssid)
{
    if (ssid != NULL) {
	CFRetain(ssid);
    }
    if (dhcp->lease.ssid != NULL) {
	CFRelease(dhcp->lease.ssid);
    }
    dhcp->lease.ssid = ssid;
    return;
}

static void
_dhcp_lease_save(ServiceRef service_p, absolute_time_t lease_start,
		 const uint8_t * pkt, int pkt_size, boolean_t write_lease)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    const void *	cid;
    uint8_t 		cid_type;
    int 		cid_length;
    struct in_addr	router_ip = { 0 };
    const uint8_t * 	router_hwaddr = NULL;
    int 		router_hwaddr_length = 0;

    if (service_router_is_iaddr_valid(service_p)) {
	router_ip = service_router_iaddr(service_p);
	if (service_router_is_hwaddr_valid(service_p)) {
	    /* we have the router h/w address, save it too */
	    router_hwaddr = service_router_hwaddr(service_p);
	    router_hwaddr_length = if_link_length(if_p);
	}
    }
    get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
    DHCPLeaseListUpdateLease(&dhcp->lease_list, dhcp->saved.our_ip,
			     router_ip, router_hwaddr, router_hwaddr_length,
			     dhcp->lease.start, dhcp->lease.length,
			     pkt, pkt_size, dhcp->lease.ssid);
    if (write_lease) {
	DHCPLeaseListWrite(&dhcp->lease_list,
			   if_name(if_p), cid_type, cid, cid_length);
    }
    return;
}

static void
_dhcp_lease_clear(ServiceRef service_p, bool nak)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	router_ip = { 0 };
    const uint8_t * 	router_hwaddr = NULL;
    int 		router_hwaddr_length = 0;

    if (service_router_is_iaddr_valid(service_p)) {
	router_ip = service_router_iaddr(service_p);
	if (service_router_is_hwaddr_valid(service_p)) {
	    router_hwaddr = service_router_hwaddr(service_p);
	    router_hwaddr_length = if_link_length(if_p);
	}
    }
    if (nak == FALSE 
	|| router_ip.s_addr == 0
	|| router_hwaddr_length == 0) {
	const void *	cid;
	uint8_t		cid_type;
	int 		cid_length;

	get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
	DHCPLeaseListRemoveLease(&dhcp->lease_list,
				 dhcp->saved.our_ip, router_ip,
				 router_hwaddr, router_hwaddr_length);
	DHCPLeaseListWrite(&dhcp->lease_list,
			   if_name(if_p), cid_type, cid, cid_length);
    }
    else {
	DHCPLeaseRef	lease_p;
	int		where;

	where = DHCPLeaseListFindLease(&dhcp->lease_list, 
				       dhcp->saved.our_ip,
				       router_ip,
				       router_hwaddr,
				       router_hwaddr_length);
	if (where != DHCP_LEASE_NOT_FOUND) {
	    lease_p = DHCPLeaseListElement(&dhcp->lease_list, where);
	    DHCPLeaseSetNAK(lease_p, TRUE);
	}
    }
    return;
}

static boolean_t
switch_to_lease(ServiceRef service_p, DHCPLeaseRef lease_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    dhcp_lease_time_t	lease_time;
    dhcp_lease_time_t	t1_time;
    dhcp_lease_time_t	t2_time;

    if (lease_p->our_ip.s_addr == dhcp->saved.our_ip.s_addr) {
	if (ip_is_private(lease_p->our_ip) == FALSE) {
	    /* same lease */
	    my_log(LOG_INFO,
		   "DHCP %s: switch_to_lease returns FALSE, "
		   "public IP is the same", if_name(if_p));
	    return (FALSE);
	}
	if (service_router_is_iaddr_valid(service_p)
	    && service_router_is_hwaddr_valid(service_p)
	    && (lease_p->router_ip.s_addr
		== service_router_iaddr(service_p).s_addr)
	    && (bcmp(lease_p->router_hwaddr, 
		     service_router_hwaddr(service_p),
		     if_link_length(if_p)) == 0)) {
	    my_log(LOG_INFO, "DHCP %s: switch_to_lease returns FALSE, "
		   "private IP has same router", if_name(if_p));
	    return (FALSE);
	}
    }
    /* make sure we stop using the old IP address */
    (void)service_remove_address(service_p);
    if (lease_p->pkt_length > sizeof(dhcp->saved.pkt)) {
	dhcp->saved.pkt_size = sizeof(dhcp->saved.pkt);
    }
    else {
	dhcp->saved.pkt_size = lease_p->pkt_length;
    }
    bcopy(lease_p->pkt, dhcp->saved.pkt, dhcp->saved.pkt_size);
    dhcpol_free(&dhcp->saved.options);
    /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
    (void)dhcpol_parse_packet(&dhcp->saved.options, 
			      (void *)dhcp->saved.pkt, 
			      dhcp->saved.pkt_size, NULL);
    dhcp_get_lease_from_options(&dhcp->saved.options, &lease_time, 
				&t1_time, &t2_time);
    dhcp_set_lease_params(service_p, "RECOVERED",
			  lease_p->lease_start, lease_time,
			  t1_time, t2_time);
    dhcp->lease.valid = TRUE;
    dhcp->saved.rating = 0;
    dhcp->saved.our_ip = lease_p->our_ip;
    get_server_identifier(&dhcp->saved.options, &dhcp->saved.server_ip);
    dhcp_lease_set_ssid(dhcp, lease_p->ssid);
    service_router_clear(service_p);
    if (lease_p->router_ip.s_addr != 0) {
	service_router_set_iaddr(service_p, lease_p->router_ip);
	service_router_set_iaddr_valid(service_p);
	if (lease_p->router_hwaddr_length > 0) {
	    int		len;
	    
	    len = lease_p->router_hwaddr_length;
	    if (len > service_router_hwaddr_size(service_p)) {
		len = service_router_hwaddr_size(service_p);
	    }
	    bcopy(lease_p->router_hwaddr,
		  service_router_hwaddr(service_p), len);
	    service_router_set_hwaddr_valid(service_p);
	}
    }
    my_log(LOG_INFO, "DHCP %s: switched to lease for IP " IP_FORMAT,
	   if_name(if_p), IP_LIST(&lease_p->our_ip));
    return (TRUE);
}

static void
recover_lease(ServiceRef service_p)
{
    const void *	cid;
    uint8_t		cid_type;
    int			cid_length;
    int			count;
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    DHCPLeaseRef	lease_p;

    /* read the DHCP lease */
    get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
    DHCPLeaseListRead(&dhcp->lease_list, if_name(if_p),
		      if_is_wireless(if_p),
		      cid_type, cid, cid_length);
    count = DHCPLeaseListCount(&dhcp->lease_list);
    if (count == 0) {
	return;
    }
    lease_p = DHCPLeaseListElement(&dhcp->lease_list, count - 1);
    (void)switch_to_lease(service_p, lease_p);
    my_log(LOG_INFO, "DHCP %s: recovered lease for IP " IP_FORMAT,
	   if_name(if_p), IP_LIST(&lease_p->our_ip));
    return;
}

static boolean_t
dhcp_check_lease(ServiceRef service_p, absolute_time_t current_time)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);

    if (dhcp->lease.valid) {
	if (dhcp->lease.length != DHCP_INFINITE_LEASE
	    && (current_time >= dhcp->lease.expiration)) {
	    dhcp->lease.valid = FALSE;
	    service_router_clear(service_p);
	    (void)service_remove_address(service_p);
	    service_publish_failure(service_p,
				    ipconfig_status_lease_expired_e);
	}
    }
    return (dhcp->lease.valid);
}

static void
dhcp_check_link(ServiceRef service_p, IFEventID_t event_id)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    link_status_t	link_status = service_link_status(service_p);
    boolean_t		wait_for_link_active;

    if (if_is_wireless(if_p)) {
	/*
	 * Wi-Fi should always be reporting accurate link status. If it
	 * does not, it's likely at start-up. Sending a packet at that
	 * time is not desirable since it is inherently stale and not
	 * applicable to the current SSID.
	 */
	wait_for_link_active = !link_status.valid || !link_status.active;
    }
    else {
	wait_for_link_active = link_status.valid && !link_status.active;
    }
    if (wait_for_link_active) {
	/* ensure that we'll retry when the link goes back up */
	dhcp->try = 0;
	dhcp->state = dhcp_cstate_inactive_e;
	dhcp_cancel_pending_events(service_p);
	return;
    }
    dhcp->conflicting_address.s_addr = 0;
    dhcp->user_warned = FALSE;
    if (dhcp->lease.valid && dhcp->lease.ssid != NULL) {
	CFStringRef	ssid = ServiceGetSSID(service_p);

	if (ssid == NULL) {
	    my_log(LOG_INFO, "%s: dhcp_check_link: no SSID",
		   if_name(if_p));
	}
	else if (!CFEqual(dhcp->lease.ssid, ssid)) {
	    DHCPLeaseRef 	lease_p;
	    int			where;

	    /* try to switch to new lease */
	    my_log(LOG_INFO, "%s: SSID is now %@ (was %@)",
		   if_name(if_p), ssid, dhcp->lease.ssid);
	    where = DHCPLeaseListFindLeaseWithSSID(&dhcp->lease_list,
						   ssid);
	    if (where != DHCP_LEASE_NOT_FOUND) {
		lease_p = DHCPLeaseListElement(&dhcp->lease_list, where);
		(void)switch_to_lease(service_p, lease_p);
	    }
	    else {
		/* go back to INIT state */
		my_log(LOG_INFO, "%s: No lease for %@", if_name(if_p), ssid);
		dhcp->lease.valid = FALSE;
	    }
	}
    }
    if (dhcp_check_lease(service_p, current_time)) {
	/* try same address */
	if (event_id == IFEventID_renew_e
	    || dhcp->state != dhcp_cstate_init_reboot_e
	    || dhcp->try != 1) {
	    struct in_addr	our_ip;
	    
	    our_ip = dhcp->saved.our_ip;
	    dhcp_init_reboot(service_p, IFEventID_start_e, 
			     &our_ip);
	}
	/* we're already in the init-reboot state */
	return;
    }
    if (event_id == IFEventID_renew_e
	|| dhcp->state != dhcp_cstate_init_e
	|| dhcp->try != 1) {
	dhcp_init(service_p, IFEventID_start_e, NULL);
	return;
    }
    return;
}

INLINE boolean_t
S_dhcp_cstate_is_bound_renew_or_rebind(dhcp_cstate_t state)
{
    boolean_t	ret;

    switch (state) {
    case dhcp_cstate_bound_e:
    case dhcp_cstate_renew_e:
    case dhcp_cstate_rebind_e:
	ret = TRUE;
	break;
    default:
	ret = FALSE;
	break;
    }
    return (ret);
}

STATIC void
dhcp_handle_active_during_sleep(ServiceRef service_p,
				active_during_sleep_t * active_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    long 		leeway = 0;
    absolute_time_t	wake_time = 0;

    if (active_p->requested
	&& ServiceGetActiveIPAddress(service_p).s_addr != 0
	&& dhcp->lease.valid
	&& dhcp->lease.expiration != 0) {
	/* try to schedule a wake */
	absolute_time_t 	current_time;
	const char *		reason = "";

	current_time = timer_current_secs();
	if (current_time >= dhcp->lease.expiration) {
	    /* expired lease, this should not happen */
	    my_log_fl(LOG_NOTICE,
		      "DHCP %s: lease is already expired",
		      if_name(if_p));
	    reason = "lease is expired";
	}
	else if (S_min_wake_ok(current_time, dhcp->renew_rebind_time)) {
	    wake_time = dhcp->renew_rebind_time;
	    reason = "wake at renew_rebind_time";
	    if (dhcp->lease.t2 > wake_time) {
		leeway = (dhcp->lease.t2 - wake_time)/2;
	    }
	}
	else if (S_min_wake_ok(current_time, dhcp->lease.t2)) {
	    wake_time = dhcp->lease.t2;
	    reason = "wake at t2";
	    if (dhcp->lease.expiration > wake_time) {
		leeway = (dhcp->lease.expiration - wake_time)/2;
	    }
	}
	else if (dhcp->allow_wake_with_short_lease
		 && S_short_wake_ok(current_time, dhcp->lease.t2)) {
	    wake_time = dhcp->lease.t2;
	    reason = "wake at t2 (allow short first wake)";
	}
	else if (S_min_wake_ok(current_time, dhcp->lease.expiration)) {
	    wake_time = current_time + G_min_wake_interval_secs;
	    reason = "wake in min_interval";
	}
	else {
	    reason = "expiration is too soon";
	}
	if (wake_time == 0) {
	    my_log(LOG_NOTICE,
		   "DHCP %s: can't schedule wake-up, %s",
		   if_name(if_p), reason);
	    dhcp_log_lease(LOG_NOTICE, if_name(if_p),
			   current_time, &dhcp->lease);
	    active_p->supported = FALSE;
	    wake_time = 0;
	}
	else {
	    my_log(LOG_INFO, 
		   "DHCP %s: next wake-up at 0x%lx, %s",
		   if_name(if_p), wake_time, reason);
	    dhcp_log_lease(LOG_INFO, if_name(if_p),
			   current_time, &dhcp->lease);
	}
    }
    if (wake_time != 0) {
	if (wake_time == dhcp->wake_time) {
	    /* wake already scheduled */
	    my_log(LOG_INFO,
		   "DHCP %s: wake at 0x%lx already scheduled",
		   if_name(if_p), wake_time);
	}
	else {
	    CFDateRef			date;
	    CFMutableDictionaryRef 	req_dict = NULL;
	    IOReturn			status;
	    CFStringRef			wake_id;

	    wake_id = ServiceCopyWakeID(service_p);
	    if (dhcp->wake_time != 0) {
		/* unschedule old wake up time */
		date = CFDateCreate(NULL, dhcp->wake_time);
		(void)IOPMCancelScheduledPowerEvent(date, wake_id, 
						    CFSTR(kIOPMAutoWake));
		my_log(LOG_INFO,
		       "DHCP %s: unscheduled old wake at %@ (0x%lx)",
		       if_name(if_p), date, dhcp->wake_time);
		CFRelease(date);
	    }

	    /* schedule new wake up time */
	    date = CFDateCreate(NULL, wake_time);
	    req_dict = CFDictionaryCreateMutable(NULL, 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	    CFDictionarySetValue(req_dict, CFSTR(kIOPMPowerEventAppNameKey), wake_id);
	    CFDictionarySetValue(req_dict, CFSTR(kIOPMPowerEventTimeKey), date);
	    if (leeway > 0) {
		CFNumberRef	leewayNumber;

		leewayNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &leeway);
		CFDictionarySetValue(req_dict, CFSTR(kIOPMPowerEventLeewayKey), leewayNumber);
		CFRelease(leewayNumber);
	    }
	    status = IOPMRequestSysWake(req_dict);
	    CFRelease(req_dict);
	    CFRelease(wake_id);
	    if (status != kIOReturnSuccess) {
		my_log(LOG_NOTICE,
		       "DHCP %s: failed to schedule wake at %@ (0x%lx)",
		       if_name(if_p), date, wake_time);
		dhcp->wake_time = 0;
		active_p->supported = FALSE;
	    }
	    else {
		my_log(LOG_INFO,
		       "DHCP %s: scheduled wake at %@ (0x%lx)",
		       if_name(if_p), date, wake_time);
		dhcp->wake_time = wake_time;
	    }
	    CFRelease(date);
	}
    }
    else if (dhcp->wake_time != 0) {
	CFDateRef			date;
	CFStringRef		wake_id;

	/* unschedule existing wake time */
	wake_id = ServiceCopyWakeID(service_p);
	date = CFDateCreate(NULL, dhcp->wake_time);
	(void)IOPMCancelScheduledPowerEvent(date, wake_id, 
					    CFSTR(kIOPMAutoWake));
	CFRelease(wake_id);
	my_log(LOG_INFO,
	       "DHCP %s: unscheduled old wake at %@ (0x%lx)",
	       if_name(if_p), date, dhcp->wake_time);
	CFRelease(date);
	dhcp->wake_time = 0;
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
dhcp_thread(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;


    switch (event_id) {
      case IFEventID_start_e: {
	  ipconfig_method_data_t	method_data;
	  struct in_addr		our_ip;
	  char				timer_name[32];

	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (dhcp != NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: re-entering start state",
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  dhcp = malloc(sizeof(*dhcp));
	  bzero(dhcp, sizeof(*dhcp));
	  dhcp->must_broadcast = (if_link_arptype(if_p) == ARPHRD_IEEE1394);
	  dhcpol_init(&dhcp->saved.options);
	  ServiceSetPrivate(service_p, dhcp);

	  dhcp->lease.valid = FALSE;
	  service_router_clear(service_p);
	  dhcp->state = dhcp_cstate_inactive_e;
	  snprintf(timer_name, sizeof(timer_name), "dhcp-%s",
		   if_name(if_p));
	  dhcp->timer = timer_callout_init(timer_name);
	  if (dhcp->timer == NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: timer_callout_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  (void)service_enable_autoaddr(service_p);
	  dhcp->client = bootp_client_init(G_bootp_session, if_p);
	  if (dhcp->client == NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: bootp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  dhcp->arp = arp_client_init(G_arp_session, if_p);
	  if (dhcp->arp == NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: arp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  method_data = (ipconfig_method_data_t)event_data;
	  if (method_data != NULL && method_data->dhcp.client_id_len > 0) {
	      dhcp->client_id_len = method_data->dhcp.client_id_len;
	      dhcp->client_id = malloc(dhcp->client_id_len);
	      if (dhcp->client_id == NULL) {
		  my_log(LOG_NOTICE, "DHCP %s: malloc client ID failed",
			 if_name(if_p));
		  status = ipconfig_status_allocation_failed_e;
		  goto stop;
	      }
	      bcopy(method_data->dhcp.client_id, 
		    dhcp->client_id, dhcp->client_id_len);
	  }
	  my_log(LOG_INFO, "DHCP %s: start", if_name(if_p));
	  dhcp->xid = arc4random();
	  DHCPLeaseListInit(&dhcp->lease_list);
	  our_ip.s_addr = 0;
	  if (ServiceIsNetBoot(service_p)) {
	      dhcp_init_reboot(service_p, IFEventID_start_e, &our_ip);
	  }
	  else {
	      recover_lease(service_p);
	      dhcp_check_link(service_p, IFEventID_start_e);
	  }
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_INFO, "DHCP %s: stop", if_name(if_p));
	  if (dhcp == NULL) {
	      my_log(LOG_INFO, "DHCP %s: already stopped", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }
	  if (event_id == IFEventID_stop_e) {
	      (void)dhcp_release(service_p);
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  service_disable_autoaddr(service_p);

	  /* clean-up resources */
	  if (dhcp->timer) {
	      timer_callout_free(&dhcp->timer);
	  }
	  if (dhcp->client) {
	      bootp_client_free(&dhcp->client);
	  }
	  if (dhcp->arp) {
	      arp_client_free(&dhcp->arp);
	  }
	  if (dhcp->client_id) {
	      free(dhcp->client_id);
	      dhcp->client_id = NULL;
	  }
	  DHCPLeaseListFree(&dhcp->lease_list);
	  dhcpol_free(&dhcp->saved.options);
	  dhcp_lease_set_ssid(dhcp, NULL);
	  if (dhcp) {
	      free(dhcp);
	  }
	  ServiceSetPrivate(service_p, NULL);
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *		change_event;
	  ipconfig_method_data_t	method_data;

	  if (dhcp == NULL) {
	      my_log(LOG_INFO, "DHCP %s: private data is NULL", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  change_event = (change_event_data_t *)event_data;
	  method_data = change_event->method_data;
	  change_event->needs_stop = FALSE;
	  if (method_data != NULL && method_data->dhcp.client_id_len > 0) {
	      if (dhcp->client_id == NULL 
		  || dhcp->client_id_len != method_data->dhcp.client_id_len
		  || bcmp(dhcp->client_id, method_data->dhcp.client_id,
			  dhcp->client_id_len)) {
		  change_event->needs_stop = TRUE;
	      }
	  }
	  else {
	      if (dhcp->client_id != NULL) {
		  change_event->needs_stop = TRUE;
	      }
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;

	  arpc = (arp_collision_data_t *)event_data;

	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (dhcp->disable_arp_collision_detection
	      || (ServiceGetActiveIPAddress(service_p).s_addr
		  != dhcp->saved.our_ip.s_addr)
	      || arpc->ip_addr.s_addr != dhcp->saved.our_ip.s_addr) {
	      break;
	  }

	  /* defend our address, don't just give it up */
	  if (ServiceDefendIPv4Address(service_p, arpc)) {
	      break;
	  }

	  /* address conflict, give up the address */
	  my_log(LOG_NOTICE, "DHCP %s: "
		 IP_FORMAT " in use by " EA_FORMAT ", DHCP Server " 
		 IP_FORMAT,
		 if_name(if_p),
		 IP_LIST(&dhcp->saved.our_ip),
		 EA_LIST(arpc->hwaddr),
		 IP_LIST(&dhcp->saved.server_ip));
	  _dhcp_lease_clear(service_p, FALSE);
	  service_publish_failure(service_p, 
				  ipconfig_status_address_in_use_e);
	  dhcp_decline(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_renew_e:
      case IFEventID_link_status_changed_e: {
	  link_event_data_t	link_event;

	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  link_event = (link_event_data_t)event_data;
	  if ((link_event->flags & kLinkFlagsSSIDChanged) != 0) {
	      /* switched networks, remove IP address to avoid IP collisions */
	      (void)service_remove_address(service_p);
	      service_publish_failure(service_p,
				      ipconfig_status_network_changed_e);
	      linklocal_service_change(service_p, LINKLOCAL_NO_ALLOCATE);
	  }
	  /* make sure to start DHCP again */
	  dhcp_check_link(service_p, event_id);
	  break;
      }
      case IFEventID_link_timer_expired_e:
	  dhcp_inactive(service_p);
	  break;
      case IFEventID_power_off_e:
      case IFEventID_sleep_e: {
	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (dhcp->lease.valid) {
	      _dhcp_lease_save(service_p, dhcp->lease.start,
			       (uint8_t *)dhcp->saved.pkt, dhcp->saved.pkt_size,
			       TRUE);
	  }
	  break;
      }
      case IFEventID_wake_e: {
	  link_status_t		link_status;
	  link_event_data_t	link_event;

	  if (ServiceIsNetBoot(service_p)) {
	      break;
	  }
	  /*
	   * While asleep, we could have switched networks without knowing it.
	   * Unless we know with some confidence that we're on the same network,
	   * we need to remove the IP address from the interface.
	   *
	   * We remove the IP address if any of the following are true:
	   * - we're not connected to a network (link status is inactive)
	   * - we're on a different Wi-Fi network (the SSID changed)
	   * - we're not on the same ethernet network
	   */
	  link_event = (link_event_data_t)event_data;
	  link_status = service_link_status(service_p);
	  if ((link_status.valid && link_status.active == FALSE)
	      || (if_is_wireless(if_p) 
		  && (link_event->flags & kLinkFlagsSSIDChanged) != 0)
	      || (if_is_wireless(if_p) == FALSE
		  && link_status.wake_on_same_network == FALSE)) {
	      /* make sure there's no IP address assigned when we wake */
	      service_remove_address(service_p);
	      service_publish_failure(service_p,
				      ipconfig_status_network_changed_e);
	      dhcp_check_link(service_p, event_id);
	  }
	  else {
	      absolute_time_t 	current_time;

	      current_time = timer_current_secs();
	      if (dhcp_check_lease(service_p, current_time) == FALSE) {
		  /* no valid lease */
		  dhcp_init(service_p, IFEventID_start_e, NULL);
		  break;
	      }
	      
	      /*
	       * If we're not in RENEW/REBIND/BOUND, we haven't published,
	       * or we couldn't resolve the router's MAC address,
	       * enter INIT-REBOOT.
	       */
	      if (S_dhcp_cstate_is_bound_renew_or_rebind(dhcp->state) == FALSE
		  || ServiceIsPublished(service_p) == FALSE
		  || dhcp->resolve_router_timed_out) {
		  struct in_addr	our_ip;

		  our_ip = dhcp->saved.our_ip;
		  dhcp_init_reboot(service_p, IFEventID_start_e, &our_ip);
		  break;
	      }
	   
	      /* Check if the new bssid router is arp-able. */
	      if ((link_event->flags & kLinkFlagsBSSIDChanged) != 0) {
		  arp_address_info_t info;

		  if (service_populate_router_arpinfo(service_p,
						      &info)) {
		      dhcp_check_router(service_p, IFEventID_start_e,
					(void *)&info);
		      break;
		  }
	      }

	      /* Check the lease time */
	      if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
		  /* infinite lease, no need to do any maintenance */
		  break;
	      }
	      /*
	       * Check the timer we had scheduled. If it is sufficiently in the
	       * future, schedule a new timer to wakeup in RENEW/REBIND then.
	       * Otherwise, proceed to RENEW/REBIND.
	       */
	      if (S_time_in_future(current_time, dhcp->renew_rebind_time,
				   G_wake_skew_secs)) {
		  struct timeval	tv;
		  
		  tv.tv_sec = dhcp->renew_rebind_time - current_time;
		  tv.tv_usec = 0;
		  timer_set_relative(dhcp->timer, tv, 
				     (timer_func_t *)dhcp_renew_rebind,
				     service_p, (void *)IFEventID_start_e,
				     NULL);
	      }
	      else {
		  dhcp_renew_rebind(service_p, IFEventID_start_e, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_get_dhcp_info_e: {
	  dhcp_info_t * dhcp_info_p = ((dhcp_info_t *)event_data);

	  if (ServiceGetActiveIPAddress(service_p).s_addr == 0
	      || dhcp->saved.pkt_size == 0) {
	      break;
	  }
	  dhcp_set_dhcp_info(dhcp, dhcp_info_p);
	  break;
      }
      case IFEventID_bssid_changed_e: {
	  arp_address_info_t info;
	  
	  if (!S_dhcp_cstate_is_bound_renew_or_rebind(dhcp->state)) {
	      /* we aren't in a state where roaming matters */
	      break;
	  }
	  if (!ServiceIsPublished(service_p)) {
	      /* we haven't published yet, ignore the roam event */
	      break;
	  }
	  if (service_populate_router_arpinfo(service_p, &info)) {
	      dhcp_check_router(service_p, IFEventID_start_e, 
	          	        (void *)&info);
	  }
	  break;	
      }
      case IFEventID_active_during_sleep_e:
	  dhcp_handle_active_during_sleep(service_p, 
					  (active_during_sleep_t *)event_data);
	  break;
      case IFEventID_forget_ssid_e: {
	  CFStringRef	ssid = (CFStringRef)event_data;

	  my_log(LOG_NOTICE, "DHCP %s: ForgetSSID %@", if_name(if_p), ssid);
	  DHCPLeaseListRemoveLeaseWithSSID(&dhcp->lease_list, ssid);
	  if (dhcp->lease.ssid != NULL && CFEqual(dhcp->lease.ssid, ssid)) {
	      struct timeval	tv;

	      /* if the lease is current, give it up, and try again */
	      (void)dhcp_release(service_p);
	      service_remove_address(service_p);
	      service_publish_failure(service_p,
				      ipconfig_status_lease_terminated_e);
	      dhcpol_free(&dhcp->saved.options);
	      dhcp_lease_set_ssid(dhcp, NULL);
	      ServiceSetBusy(service_p, FALSE);

	      /* try again in 0.5 seconds */
	      tv.tv_sec = 0;
	      tv.tv_usec = USECS_PER_SEC / 2;
	      timer_set_relative(dhcp->timer, tv,
				 (timer_func_t *)dhcp_check_link,
				 service_p, (void *)event_id,
				 NULL);
	  }
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

static void
dhcp_init(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcp_lease_time_t	lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  dhcpoa_t		options;
	  dhcp_cstate_t		prev_state = dhcp->state;

	  my_log(LOG_INFO, "DHCP %s: INIT", if_name(if_p));
	  dhcp->state = dhcp_cstate_init_e;

	  dhcp->allow_wake_with_short_lease = TRUE;
	  dhcp->resolve_router_timed_out = FALSE;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);
	  
	  /* form the request */
	  /* ALIGN: txbuf is aligned to at least sizeof(uint32) bytes */
	  dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_discover_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p),
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: INIT make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT dhcpoa_add lease time failed, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(service_p, &options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: INIT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  if (prev_state != dhcp_cstate_init_reboot_e) {
	      dhcp->start_secs = current_time;
	  }
	  dhcp->wait_secs = G_initial_wait_secs;
	  dhcp->try = 0;
	  dhcp->xid++;
	  dhcp->gathering = FALSE;
	  dhcp->saved.rating = 0;
	  dhcp->got_nak = FALSE;
	  (void)service_enable_autoaddr(service_p);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init, 
				      service_p, (void *)IFEventID_data_e);
	  ServiceSetBusy(service_p, TRUE);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      dhcp_select(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      link_status_t	link_status = service_link_status(service_p);

	      if (link_status.valid 
		  && link_status.active == FALSE) {
		  dhcp_inactive(service_p);
		  break;
	      }
	  }
	  if (dhcp->try >= (G_dhcp_router_arp_at_retry_count + 1)
	      && service_router_all_valid(service_p) == FALSE) {
	      /* try to confirm the router's address */
	      dhcp_arp_router(service_p, IFEventID_start_e, &current_time);
	  }
	  if (service_router_all_valid(service_p) == FALSE) {
	      if (dhcp->try >= (G_dhcp_allocate_linklocal_at_retry_count + 1)
		  && G_dhcp_failure_configures_linklocal) {
		  ServiceSetStatus(service_p, 
				   ipconfig_status_no_server_e);
		  linklocal_service_change(service_p, LINKLOCAL_ALLOCATE);
	      }
	      if (dhcp->try
		  >= (G_dhcp_generate_failure_symptom_at_retry_count + 1)) {
		  ServiceGenerateFailureSymptom(service_p);
	      }
	  }
	  if (dhcp->try > (G_max_retries + 1)) {
	      /* no server responded */
	      if (service_router_all_valid(service_p)) {
		  /* lease is still valid, router is still available */
		  dhcp_bound(service_p, IFEventID_start_e, NULL);
		  break;
	      }
	      /* DHCP server and router not responding, try again later */
	      dhcp_no_server(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((uint16_t)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT transmit failed", if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_INFO, "DHCP %s: INIT waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  dhcp_lease_time_t 	lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  dhcp_lease_time_t	t1;
	  dhcp_lease_time_t	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip) == FALSE
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (reply_msgtype == dhcp_msgtype_offer_e) {
	      int rating = 0;
	      int dhcp_packet_ideal_rating = n_dhcp_params 
					     + DHCP_ADDRESS_RATING_GOOD;
	      
	      dhcp_get_lease_from_options(&pkt->options, &lease, &t1, &t2);
	      rating = dhcpol_count_params(&pkt->options, 
					   dhcp_params, n_dhcp_params);

	      /* We need to get an augmented rating to account for routable
	       * addresses (Ie, those which are not private or linklocal
	       */
	      rating += 
		 get_rating_for_ip_address(&pkt->data->dp_yiaddr); 

	      /* 
	       * The new packet is "better" than the saved packet if there
	       * is no saved packet, or it has a higher rating.
	       */
	      if (dhcp->saved.pkt_size == 0
		  || rating > dhcp->saved.rating) {
		  service_router_clear(service_p);
		  dhcpol_free(&dhcp->saved.options);
		  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
		  dhcp->saved.pkt_size = pkt->size;
		  dhcp->saved.rating = rating;
		  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
		  (void)dhcpol_parse_packet(&dhcp->saved.options, 
					    (void *)dhcp->saved.pkt, 
					    dhcp->saved.pkt_size, NULL);
		  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
		  dhcp->saved.server_ip = server_ip;
		  dhcp_set_lease_params(service_p, "INIT",
					current_time, lease, t1, t2);
		  if (rating == dhcp_packet_ideal_rating
		      || if_is_expensive(if_p)) {
		      dhcp_select(service_p, IFEventID_start_e, NULL);
		      break; /* out of switch */
		  }
		  if (dhcp->gathering == FALSE) {
		      my_log(LOG_INFO, "DHCP %s: INIT gathering began at %ld",
			     if_name(if_p), 
			     current_time - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_callout_set(dhcp->timer, G_gather_secs,
					(timer_func_t *)dhcp_init,
					service_p, (void *)IFEventID_timeout_e,
					NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;

 error:
    dhcp_failed(service_p, status);
    return;
}


static void
dhcp_init_reboot(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	source_ip = G_ip_zeroes;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;
    struct in_addr 	server_ip = G_ip_broadcast;

    if (ServiceIsNetBoot(service_p)) {
	netboot_addresses(&source_ip, NULL);
    }

    switch (evid) {
      case IFEventID_start_e: {
	  dhcp_lease_time_t	lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  char			ntopbuf[INET_ADDRSTRLEN];
	  dhcpoa_t	 	options;
	  struct in_addr 	our_ip;

	  dhcp->state = dhcp_cstate_init_reboot_e;
	  dhcp->resolve_router_timed_out = FALSE;
	  dhcp->allow_wake_with_short_lease = TRUE;
	  dhcp->start_secs = current_time;
	  dhcp->try = 0;
	  dhcp->wait_secs = G_initial_wait_secs;
	  if (source_ip.s_addr == 0) {
	      our_ip = *((struct in_addr *)event_data);
	  }
	  else {
	      our_ip = source_ip;
	  }
	  my_log(LOG_INFO, "DHCP %s: INIT-REBOOT (%s)",
		 if_name(if_p),
		 inet_ntop(AF_INET, &our_ip, ntopbuf, sizeof(ntopbuf)));

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  /* form the request */
	  /* ALIGN: txbuf is aligned to at least sizeof(uint32_t) bytes */
	  dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf), 
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(our_ip), &our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT-REBOOT add request ip failed, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT-REBOOT add lease time failed, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(service_p, &options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT-REBOOT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->got_nak = FALSE;
	  dhcp->gathering = FALSE;
	  dhcp->xid++;
	  dhcp->saved.our_ip = our_ip;
	  dhcp->saved.rating = 0;
	  (void)service_enable_autoaddr(service_p);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init_reboot, 
				      service_p, (void *)IFEventID_data_e);
	  ServiceSetBusy(service_p, TRUE);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      /* done gathering */
	      dhcp_bound(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      link_status_t	link_status = service_link_status(service_p);

	      if (link_status.valid 
		  && link_status.active == FALSE) {
		  dhcp_inactive(service_p);
		  break;
	      }
	  }
	  /* try to confirm the router's address */
	  dhcp_arp_router(service_p, IFEventID_start_e, &current_time);

	  if (service_router_all_valid(service_p) == FALSE
	      && dhcp->try >= (G_dhcp_allocate_linklocal_at_retry_count + 1)) {
	      if (G_dhcp_failure_configures_linklocal) {
		  ServiceSetStatus(service_p, ipconfig_status_no_server_e);
		  linklocal_service_change(service_p, LINKLOCAL_ALLOCATE);
	      }
	  }
	  if (dhcp->try > (G_dhcp_init_reboot_retry_count + 1)) {
	      my_log(LOG_INFO, "DHCP %s: INIT-REBOOT (" IP_FORMAT 
		     ") timed out", if_name(if_p),
		     IP_LIST(&dhcp->saved.our_ip));
	      (void)service_remove_address(service_p);
	      service_publish_failure_sync(service_p, 
					   ipconfig_status_server_not_responding_e, 
					   FALSE);
	      dhcp->try--;
	      /* go back to the INIT state */
	      dhcp_init(service_p, IFEventID_start_e, NULL);
	      break; /* ouf of case */
	  }

	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((uint16_t)(current_time - dhcp->start_secs));
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    server_ip, source_ip,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: INIT-REBOOT transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);

	  my_log(LOG_INFO, 
		 "DHCP %s: INIT-REBOOT (" IP_FORMAT 
		 ") waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 IP_LIST(&dhcp->saved.our_ip),
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init_reboot,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  dhcp_lease_time_t 	lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  dhcp_lease_time_t 	t1;
	  dhcp_lease_time_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (ServiceIsNetBoot(service_p) == FALSE
	      && reply_msgtype == dhcp_msgtype_nak_e) {
	      my_log(LOG_INFO, "DHCP %s: got DHCP NAK",
		     if_name(if_p));
	      if (arp_client_is_active(dhcp->arp)) {
		  dhcp->got_nak = TRUE;
	      }
	      else {
		  service_publish_failure(service_p, 
					  ipconfig_status_lease_terminated_e);
		  dhcp_unbound(service_p, IFEventID_start_e, (void *)TRUE);
		  break; /* out of switch */
	      }
	  }
	  if (server_ip.s_addr == 0
	      || pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (reply_msgtype == dhcp_msgtype_ack_e) {
	      int rating = 0;
	      
	      dhcp_get_lease_from_options(&pkt->options, &lease, &t1, &t2);

	      rating = dhcpol_count_params(&pkt->options, 
					   dhcp_params, n_dhcp_params);
	      /* 
	       * The new packet is "better" than the saved packet if there
	       * is no saved packet, or it has a higher rating.
	       */
	      if (dhcp->saved.pkt_size == 0
		  || rating > dhcp->saved.rating) {
		  service_router_clear(service_p);
		  dhcpol_free(&dhcp->saved.options);
		  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
		  dhcp->saved.pkt_size = pkt->size;
		  dhcp->saved.rating = rating;
		  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
		  (void)dhcpol_parse_packet(&dhcp->saved.options, 
					    (void *)dhcp->saved.pkt, 
					    dhcp->saved.pkt_size, NULL);
		  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
		  dhcp->saved.server_ip = server_ip;
		  dhcp_set_lease_params(service_p, "INIT-REBOOT",
					current_time, lease, t1, t2);
		  /*
		   * Move to the Bound state if:
		   * - the interface is expensive: there is unlikely to be
		   *   more than a single DHCP server present
		   * - the router responded to ARP and we have all of the
		   *   options we asked for
		   */
		  if (if_is_expensive(if_p)
		      || (rating == n_dhcp_params
			  && service_router_all_valid(service_p))) {
		      dhcp_bound(service_p, IFEventID_start_e, NULL);
		      return;
		  }
		  /* gather responses and give arp detection a chance to work */
		  if (dhcp->gathering == FALSE) {
		      my_log(LOG_INFO,
			     "DHCP %s: INIT-REBOOT ("
			     IP_FORMAT ") gathering began at %ld",
			     if_name(if_p),
			     IP_LIST(&dhcp->saved.our_ip),
			     current_time - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_callout_set(dhcp->timer, G_gather_secs,
					(timer_func_t *)dhcp_init_reboot,
					service_p,
					(void *)IFEventID_timeout_e, 
					NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break; /* shouldn't happen */
    }
    return;

 error:
    dhcp_failed(service_p, status);
    return;
}

static void
dhcp_select(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  my_log(LOG_INFO, "DHCP %s: SELECT", if_name(if_p));
	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  dhcp->state = dhcp_cstate_select_e;

	  /* form the request */
	  /* ALIGN: txbuf is uint32_t aligned, cast ok */
	  dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_NOTICE, "DHCP %s: SELECT make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* insert server identifier and requested ip address */
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: SELECT add requested ip failed, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: SELECT add server ip failed, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(service_p, &options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: SELECT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->try = 0;
	  dhcp->gathering = FALSE;
	  dhcp->wait_secs = G_initial_wait_secs;
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_select, 
				      service_p, (void *)IFEventID_data_e);
      }
      case IFEventID_timeout_e: {
	  dhcp->try++;
	  if (dhcp->try > (G_dhcp_select_retry_count + 1)) {
	      my_log(LOG_INFO, "DHCP %s: SELECT timed out", if_name(if_p));
	      /* go back to INIT and try again */
	      dhcp_init(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((uint16_t)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: SELECT transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = 0;
	  my_log(LOG_INFO, "DHCP %s: SELECT waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_select,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  dhcp_lease_time_t 	lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip = { 0 };
	  dhcp_lease_time_t 	t1;
	  dhcp_lease_time_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      if (server_ip.s_addr == 0 
		  || server_ip.s_addr != dhcp->saved.server_ip.s_addr) {
		  /* reject the packet */
		  break; /* out of switch */
	      }
	      /* clean-up anything that might have come before */
	      dhcp_cancel_pending_events(service_p);

	      /* 
	       * wait to retry INIT just in case there's a misbehaving server
	       * and we get stuck in an INIT-SELECT-NAK infinite loop
	       */
	      tv.tv_sec = 10;
	      tv.tv_usec = 0;
	      timer_set_relative(dhcp->timer, tv, 
				 (timer_func_t *)dhcp_init,
				 service_p, (void *)IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  dhcp_get_lease_from_options(&pkt->options, &lease, &t1, &t2);
	  dhcp_set_lease_params(service_p, "SELECT",
				current_time, lease, t1, t2);
	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
	  if (server_ip.s_addr != 0) {
	      dhcp->saved.server_ip = server_ip;
	  }
	  dhcp_bound(service_p, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;

 error:
    dhcp_failed(service_p, status);
    return;
}

static void
dhcp_check_router(ServiceRef service_p, 
                  IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *    dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *       if_p = service_interface(service_p);

    switch (event_id) {
	case IFEventID_start_e: {
	    arp_address_info_t * info_p = (arp_address_info_t*) event_data;

	    ServiceSetBusy(service_p, TRUE);
	    info_p->sender_ip = dhcp->saved.our_ip;
	    my_log(LOG_INFO,
		   "DHCP %s: sending unicast ARP to gateway "
		   IP_FORMAT " from " IP_FORMAT,
		   if_name(if_p),
		   IP_LIST(&info_p->target_ip),
		   IP_LIST(&info_p->sender_ip));
	    arp_client_detect(dhcp->arp, 
			      (arp_result_func_t *)
			      dhcp_check_router,
			      service_p,
			      (void *)IFEventID_arp_e,
			      info_p, 1);
	    break;
	}
	case IFEventID_arp_e: {
	    arp_result_t *        result = (arp_result_t *)event_data;
	    
	    ServiceSetBusy(service_p, FALSE);
	    if (result->error || result->in_use == FALSE) {
		if (result->error) {
		    my_log(LOG_NOTICE,
			   "DHCP %s: ARP detect gateway failed, %s",
			   if_name(if_p),
			   arp_client_errmsg(dhcp->arp));
                }
		else {
		   my_log(LOG_INFO,
			  "DHCP %s: ARP detect gateway got no response",
			  if_name(if_p));
		}
		/* Try DHCP */ 
	    	dhcp_check_link(service_p, event_id);
	    }
	    else {
		absolute_time_t     current_time = timer_current_secs();

		my_log(LOG_INFO,
		       "DHCP %s: ARP detect gateway got response",
		       if_name(if_p));

		/* Check the lease time */
		if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
		    /* infinite lease, no need to do any maintenance */
		    break;
		}
		/*
		 * Check the timer we had scheduled.  If it is in the
		 * future, schedule a new timer to wakeup in RENEW/REBIND then.
		 * If it is in the past, proceed immediately to RENEW/REBIND.
		 */
		else if (current_time < dhcp->renew_rebind_time) {
		    struct timeval        tv;

		    tv.tv_sec = dhcp->renew_rebind_time - current_time;
		    tv.tv_usec = 0;
		    timer_set_relative(dhcp->timer, tv,
				       (timer_func_t *)dhcp_renew_rebind,
				       service_p, (void *)IFEventID_start_e,
				       NULL);
		}
		else {
		    dhcp_renew_rebind(service_p, IFEventID_start_e, NULL);
		}
	    }
	    break;
	}
	default:
 	    break;
    }
}

static void
dhcp_arp_router(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);

    switch (event_id) {
      case IFEventID_start_e: {
	  int			info_count;
	  arp_address_info_t *	info_p;
	  CFStringRef		ssid;
	  absolute_time_t	start_time_threshold;
	  absolute_time_t *	start_time_threshold_p;
	  bool			tentative_ok;

	  if (G_router_arp == FALSE) {
	      /* don't ARP for the router */
	      break;
	  }
	  if (service_router_is_arp_verified(service_p)) {
	      /* nothing to be done */
	      break;
	  }
	  tentative_ok = (dhcp->state == dhcp_cstate_init_e);
	  if (if_is_wireless(if_p)) {
	      absolute_time_t *	current_time_p;

	      ssid = ServiceGetSSID(service_p);
	      if (ssid == NULL) {
		  /* no SSID, no play */
		  my_log(LOG_NOTICE, "dhcp_arp_router: %s SSID unavailable",
			 if_name(if_p));
		  break;
	      }
	      current_time_p = (absolute_time_t *)event_data;
	      start_time_threshold = *current_time_p
		  - G_router_arp_wifi_lease_start_threshold_secs;
	      start_time_threshold_p = &start_time_threshold;
	  }
	  else {
	      start_time_threshold_p = NULL;
	      ssid = NULL;
	  }
	  info_p = DHCPLeaseListCopyARPAddressInfo(&dhcp->lease_list, 
						   ssid,
						   start_time_threshold_p,
						   tentative_ok,
						   &info_count);
	  if (info_p == NULL) {
	      my_log(LOG_INFO, 
		     "DHCP %s: ARP router: No leases to query for",
		     if_name(if_p));
	      break;
	  }
	  {
	      int	i;

	      my_log(LOG_INFO, "DHCP %s: ARP detect router starting",
		     if_name(if_p));
	      for (i = 0; i < info_count; i++) {
		  my_log(LOG_INFO, "%d. sender "  IP_FORMAT
			 " target " IP_FORMAT "", i + 1,
			 IP_LIST(&info_p[i].sender_ip),
			 IP_LIST(&info_p[i].target_ip));
	      }
	  }
	  arp_client_detect(dhcp->arp,
			    (arp_result_func_t *)
			    dhcp_arp_router,
			    service_p,
			    (void *)IFEventID_arp_e, 
			    info_p, info_count);
	  free(info_p);
	  break;
      }
      case IFEventID_arp_e: {
	  arp_address_info_t *	info_p;
	  DHCPLeaseRef		lease_p;
	  void *		option;
	  struct in_addr	mask = {0};
	  arp_result_t *	result = (arp_result_t *)event_data;
	  int			where;

	  if (result->error || result->in_use == FALSE) {
	      if (result->error) {
		  my_log(LOG_NOTICE, "DHCP %s: ARP detect ROUTER failed, %s",
			 if_name(if_p),
			 arp_client_errmsg(dhcp->arp));
	      }
	      else {
		  my_log(LOG_INFO,
			 "DHCP %s: ARP detect router got no response",
			 if_name(if_p));
	      }
	      if (dhcp->got_nak) {
		  service_publish_failure(service_p, 
					  ipconfig_status_lease_terminated_e);
		  dhcp_unbound(service_p, IFEventID_start_e, (void *)TRUE);
		  break; /* out of switch */
	      }
	      break;
	  }
	  info_p = &result->addr;
	  my_log(LOG_INFO, "DHCP %s: got response for sender "
		 IP_FORMAT " target " IP_FORMAT,
		 if_name(if_p), IP_LIST(&info_p->sender_ip),
		 IP_LIST(&info_p->target_ip));
	  where = DHCPLeaseListFindLease(&dhcp->lease_list, 
					 info_p->sender_ip,
					 info_p->target_ip,
					 info_p->target_hardware,
					 if_link_length(if_p));
	  if (where == DHCP_LEASE_NOT_FOUND) {
	      my_log(LOG_INFO, "DHCP %s: lease for "
		     IP_FORMAT " is no longer available",
		     if_name(if_p), IP_LIST(&info_p->sender_ip));
	      if (dhcp->got_nak) {
		  service_publish_failure(service_p, 
					  ipconfig_status_lease_terminated_e);
		  dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      }
	      break; /* out of switch */
	  }
	  lease_p = DHCPLeaseListElement(&dhcp->lease_list, where);
	  my_log(LOG_INFO, "DHCP %s: identified lease for " IP_FORMAT,
		 if_name(if_p), IP_LIST(&lease_p->our_ip));
	  if (dhcp->state == dhcp_cstate_init_e) {
	      if (dhcp->gathering == TRUE) {
		  /* we got a response, don't bother with old lease */
		  break;
	      }
	      switch_to_lease(service_p, lease_p);
	      service_router_set_all_valid(service_p);
	      break;
	  }
	  if (dhcp->state != dhcp_cstate_init_reboot_e) {
	      /* this can't really happen */
	      break;
	  }
	  if (dhcp->gathering == TRUE) {
	      struct in_addr	router;

	      if (lease_p->our_ip.s_addr != dhcp->saved.our_ip.s_addr) {
		  /* not the same lease */
		  break;
	      }
	      if (dhcp_get_router_address(&dhcp->saved.options,
					  dhcp->saved.our_ip,
					  &router) == FALSE
		  || router.s_addr != lease_p->router_ip.s_addr) {
		  /* router changed or different, do full refresh */
		  break;
	      }
	  }
	  else if (switch_to_lease(service_p, lease_p)) {
	      dhcpol_t		options;
	      struct in_addr *	req_ip_p;

	      dhcp->request->dp_xid = htonl(++dhcp->xid);
	      dhcpol_init(&options);
	      /* ALIGN: txbuf is uint32_t aligned, cast ok */
	      (void)dhcpol_parse_packet(&options,
					(struct dhcp *)(void *)dhcp->txbuf, 
					sizeof(dhcp->txbuf), NULL);
	      req_ip_p 
		  = dhcpol_find_with_length(&options,
					    dhcptag_requested_ip_address_e,
					    sizeof(*req_ip_p));
	      if (req_ip_p != NULL) {
		  /* switch to the new IP address */
		  *req_ip_p = lease_p->our_ip;
		  dhcp->try = 0;
	      }
	      dhcpol_free(&options);
	  }
	  else if (dhcp->got_nak) {
	      /* we got a nak and we didn't switch leases */
	      service_publish_failure(service_p, 
				      ipconfig_status_lease_terminated_e);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  service_router_set_all_valid(service_p);

	  /* lease is still valid, router is still available */
	  option = dhcpol_find_with_length(&dhcp->saved.options,
					   dhcptag_subnet_mask_e, 
					   sizeof(mask));
	  if (option != NULL) {
	      mask = *((struct in_addr *)option);
	  }
	  
	  /* allow user warning to appear */
	  dhcp->conflicting_address.s_addr = 0;
	  dhcp->user_warned = FALSE;
	  
	  /* set our address */
	  (void)service_set_address(service_p, dhcp->saved.our_ip, 
				    mask, G_ip_zeroes);
	  dhcp_publish_success(service_p);

	  /* stop link local if necessary */
	  if (G_dhcp_success_deconfigures_linklocal) {
	      linklocal_service_change(service_p, LINKLOCAL_NO_ALLOCATE);
	  }
	  break;
      }
      default:
	  break;
    }
    return;
}

static void
dhcp_resolve_router_callback(ServiceRef service_p, router_arp_status_t status);


static void
dhcp_resolve_router_retry(void * arg0, void * arg1, void * arg2)
{
    Service_dhcp_t *	dhcp;
    ServiceRef 		service_p = (ServiceRef)arg0;

    dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    service_resolve_router(service_p, dhcp->arp,
			   dhcp_resolve_router_callback,
			   dhcp->saved.our_ip);
    return;
}


static void
dhcp_resolve_router_callback(ServiceRef service_p, router_arp_status_t status)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    ServiceSetBusy(service_p, FALSE);
    switch (status) {
    case router_arp_status_no_response_e:
	/* try again in 60 seconds */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	timer_set_relative(dhcp->timer, tv, 
			   (timer_func_t *)dhcp_resolve_router_retry,
			   service_p, NULL, NULL);
	if (dhcp->resolve_router_timed_out) {
	    break;
	}
	/* publish what we have so far */
	dhcp->resolve_router_timed_out = TRUE;
	dhcp_publish_success(service_p);
	_dhcp_lease_save(service_p, dhcp->lease.start,
			 (uint8_t *)dhcp->saved.pkt,
			 dhcp->saved.pkt_size,
			 TRUE);
	break;
    case router_arp_status_success_e:
	_dhcp_lease_save(service_p, dhcp->lease.start,
			 (uint8_t *)dhcp->saved.pkt,
			 dhcp->saved.pkt_size,
			 dhcp->lease.needs_write);
	dhcp->lease.needs_write = FALSE;
	dhcp_publish_success(service_p);
	break;
    default:
    case router_arp_status_failed_e:
	break;
    }
}

static boolean_t
should_write_lease(Service_dhcp_t * dhcp, absolute_time_t current_time)
{
    if (dhcp->lease.t1 < current_time) {
	/* t1 is already in the past */
	return (FALSE);
    }
    if ((dhcp->lease.t1 - current_time)
	> G_dhcp_lease_write_t1_threshold_secs) {
	/* if T1 is sufficiently far enough into the future, write the lease */
	return (TRUE);
    }
    return (FALSE);
}

static void
dhcp_bound(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    boolean_t		do_arp = TRUE;
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	mask = {0};
    void *		option;
    struct timeval 	tv = {0, 0};

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcp_cstate_t		prev_state = dhcp->state;

	  my_log(LOG_INFO, "DHCP %s: BOUND", if_name(if_p));
	  dhcp->state = dhcp_cstate_bound_e;
	  dhcp->lease.needs_write = TRUE;
	  dhcp->disable_arp_collision_detection = TRUE;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  switch (prev_state) {
	  case dhcp_cstate_renew_e:
	  case dhcp_cstate_rebind_e:
	      dhcp->lease.needs_write
		  = should_write_lease(dhcp, current_time);
	      break;
	  default:
	      /* make sure autoaddr is disabled */
	      service_disable_autoaddr(service_p);
	      break;
	  }

	  /* For renew/rebind and netboot cases, we don't
	   * need to arp  
	   */
	  if (prev_state == dhcp_cstate_rebind_e
	      || prev_state == dhcp_cstate_renew_e
	      || ServiceIsNetBoot(service_p)) {
	     break;
	  }

	  if (!if_is_expensive(if_p)
	      && prev_state == dhcp_cstate_select_e) {
              /* do an ARP probe of the supplied address */
              arp_client_probe(dhcp->arp,
                               (arp_result_func_t *)dhcp_bound, service_p,
                               (void *)IFEventID_arp_e, G_ip_zeroes,
                               dhcp->saved.our_ip);
              return;
	  }

	  if (ServiceGetActiveIPAddress(service_p).s_addr
	      == dhcp->saved.our_ip.s_addr) {
	      /* no need to probe, we're already using it */
	      if (prev_state == dhcp_cstate_init_reboot_e
		  || prev_state == dhcp_cstate_init_e) {
                  arp_client_announce(dhcp->arp,
                                      (arp_result_func_t *)dhcp_bound,
				      service_p,
                                      (void *)IFEventID_arp_e, G_ip_zeroes,
                                      dhcp->saved.our_ip, TRUE);
                  return;
	      }
	  }
	  break;
	}
	case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_NOTICE, "DHCP %s: ARP probe failed, %s",
		     if_name(if_p),
		     arp_client_errmsg(dhcp->arp));
	      dhcp_failed(service_p, ipconfig_status_internal_error_e);
	      return;
	  }
	  if (result->in_use) {
	      char		msg[128];
	      
	      snprintf(msg, sizeof(msg),
		       IP_FORMAT " in use by " EA_FORMAT 
		       ", DHCP Server " 
		       IP_FORMAT, IP_LIST(&dhcp->saved.our_ip),
		       EA_LIST(result->addr.target_hardware),
		       IP_LIST(&dhcp->saved.server_ip));
	      if (dhcp->conflicting_address.s_addr
		  != dhcp->saved.our_ip.s_addr) {
		  /* we got a different address, so allow warning again */
		  dhcp->user_warned = FALSE;
	      }
	      else if (dhcp->user_warned == FALSE) {
		  ServiceReportIPv4AddressConflict(service_p,
						   dhcp->saved.our_ip);
		  dhcp->user_warned = TRUE;
	      }
	      dhcp->conflicting_address = dhcp->saved.our_ip;
	      my_log(LOG_NOTICE, "DHCP %s: %s", if_name(if_p), msg);
	      _dhcp_lease_clear(service_p, FALSE);
	      dhcp->lease.valid = FALSE;
	      service_router_clear(service_p);
	      service_publish_failure(service_p, 
				      ipconfig_status_address_in_use_e);
	      dhcp_decline(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  dhcp_cancel_pending_events(service_p);
	  break;
	}
	default:
	  return;
    }

    /* the lease is valid */
    dhcp->lease.valid = TRUE;
    dhcp_lease_set_ssid(dhcp, ServiceGetSSID(service_p));

    /* allow user warning to appear */
    dhcp->conflicting_address.s_addr = 0;
    dhcp->user_warned = FALSE;

    /* set the interface's address and output the status */
    option = dhcpol_find_with_length(&dhcp->saved.options, 
				     dhcptag_subnet_mask_e, sizeof(mask));
    if (option != NULL) {
	mask = *((struct in_addr *)option);
    }

    /* set our address */
    if ((dhcp->saved.our_ip.s_addr 
	 != ServiceGetActiveIPAddress(service_p).s_addr)
	|| (mask.s_addr 
	    != ServiceGetActiveSubnetMask(service_p).s_addr)) {
	(void)service_set_address(service_p, dhcp->saved.our_ip, 
				  mask, G_ip_zeroes);
    }
    /* stop link local if necessary */
    if (G_dhcp_success_deconfigures_linklocal) {
	linklocal_service_change(service_p, LINKLOCAL_NO_ALLOCATE);
    }

    /* allow us to be called in the event of a subsequent collision */
    dhcp->disable_arp_collision_detection = FALSE;

    if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
	/* don't need to talk to server anymore */
	my_log(LOG_INFO, "DHCP %s: infinite lease", 
	       if_name(if_p));
    }
    else {
	if (dhcp->lease.t1 < current_time) {
	    /* t1 is already in the past, wake up in RENEW momentarily */
	    tv.tv_sec = 0;
	    tv.tv_usec = 1;
	    do_arp = FALSE;
	}
	else {
	    /* wake up in RENEW at t1 */
	    tv.tv_sec = dhcp->lease.t1 - current_time;
	    tv.tv_usec = 0;
	}
	dhcp->renew_rebind_time = dhcp->lease.t1;
	timer_set_relative(dhcp->timer, tv, 
			   (timer_func_t *)dhcp_renew_rebind,
			   service_p, (void *)IFEventID_start_e, NULL);
    }

    /* get the router's MAC address */
    dhcp->resolve_router_timed_out = FALSE;
    if (do_arp
	&& service_update_router_address(service_p, &dhcp->saved.options,
					 dhcp->saved.our_ip)
	&& service_resolve_router(service_p, dhcp->arp,
				  dhcp_resolve_router_callback,
				  dhcp->saved.our_ip)) {
	/* resolving router's MAC address started */
    }
    else {
	/* save the lease now since we won't ARP for the router */
	_dhcp_lease_save(service_p, dhcp->lease.start,
			 (uint8_t *)dhcp->saved.pkt, dhcp->saved.pkt_size,
			 dhcp->lease.needs_write);
	dhcp->lease.needs_write = FALSE;
	dhcp_publish_success(service_p);
	ServiceSetBusy(service_p, FALSE);
    }

    return;
}

static void
dhcp_no_server(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    struct timeval 	tv;
    
    switch (event_id) {
      case IFEventID_start_e: {
	  if (G_dhcp_failure_configures_linklocal) {
	      linklocal_service_change(service_p, LINKLOCAL_ALLOCATE);
	  }
	  dhcp_cancel_pending_events(service_p);
	  service_publish_failure(service_p, ipconfig_status_no_server_e);
	  ServiceSetBusy(service_p, FALSE);
	  
#define INIT_RETRY_INTERVAL_SECS      (1 * 60)
	  tv.tv_sec = INIT_RETRY_INTERVAL_SECS;
	  tv.tv_usec = 0;
	  /* wake up in INIT state after a period of waiting */
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default: {
	break;
      }
    }
    return;
}

static void
dhcp_decline(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  my_log(LOG_INFO, "DHCP %s: DECLINE", if_name(if_p));

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  /* decline the address */
	  dhcp->state = dhcp_cstate_decline_e;
	  /* ALIGN: txbuf is uint32_t aligned, cast ok */
	  dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_decline_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    FALSE,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: DECLINE couldn't add our ip, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: DECLINE couldn't add server ip, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: DECLINE failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: DECLINE transmit failed", 
		     if_name(if_p));
	  }
	  (void)service_remove_address(service_p);
	  dhcp->saved.our_ip.s_addr = 0;
	  dhcp->lease.valid = FALSE;
	  service_router_clear(service_p);
	  service_disable_autoaddr(service_p);
	  ServiceSetBusy(service_p, FALSE);

	  /* retry in a bit */
	  if (if_is_expensive(if_p)) {
	      tv.tv_sec = 1;
	  }
	  else {
	      tv.tv_sec = 10;
	  }
	  tv.tv_usec = 0;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;
 error:
    dhcp_failed(service_p, status);
    return;
}

static void
dhcp_unbound(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    struct timeval 	tv = {0,0};

    switch (event_id) {
      case IFEventID_start_e: {
	  int		nak = (int)(intptr_t)event_data;

	  my_log(LOG_INFO, "DHCP %s: UNBOUND%s", if_name(if_p),
		 nak ? " (NAK)" : "");

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);
	  dhcp->state = dhcp_cstate_unbound_e;

	  /* stop using the IP address immediately */
	  _dhcp_lease_clear(service_p, nak);
	  (void)service_remove_address(service_p);
	  dhcp->saved.our_ip.s_addr = 0;
	  dhcp->lease.valid = FALSE;
	  dhcp->got_nak = FALSE;
	  service_router_clear(service_p);
	  ServiceSetBusy(service_p, FALSE);

	  tv.tv_sec = 0;
	  tv.tv_usec = 1000;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	break;
    }
    return;
}

static void
dhcp_renew_rebind(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcp_lease_time_t	lease_option;
	  dhcpoa_t	 	options;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);
	  dhcp->allow_wake_with_short_lease = FALSE;
	  dhcp->start_secs = current_time;

	  dhcp->state = dhcp_cstate_renew_e;
	  my_log(LOG_INFO, "DHCP %s: RENEW/REBIND", if_name(if_p));
	  /* ALIGN: txbuf is uint32_t aligned, cast ok */
	  dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    FALSE,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->try = 0;
	  dhcp->request->dp_ciaddr = dhcp->saved.our_ip;
	  lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  if (dhcpoa_add(&options, dhcptag_lease_time_e, sizeof(lease_option), 
			 &lease_option) != dhcpoa_success_e) {
	      my_log(LOG_NOTICE, "DHCP %s: RENEW/REBIND couldn't add"
		     " lease time: %s", if_name(if_p),
		     dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(service_p, &options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: RENEW/REBIND failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* enable packet reception */
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_renew_rebind,
				      service_p, (void *)IFEventID_data_e);

	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct in_addr	dest_ip;
	  absolute_time_t	wakeup_time;

	  /* 
	   * If we're running as the result of our timer firing (i.e. not
	   * by the Wake code calling us), check whether the time has changed.
	   * If it has, assume that our timer fired accurately, and compute the
	   * difference between current_time and dhcp->renew_rebind_time.
	   * Assume that the entire delta is due to the time changing, and
	   * apply the delta to the lease information.
	   */
	  if (timer_still_pending(dhcp->timer) == FALSE
	      && timer_time_changed(dhcp->timer)) {
	      dhcp_adjust_lease_info(service_p, 
				     (current_time - dhcp->renew_rebind_time));
	  }

	  if (current_time >= dhcp->lease.expiration) {
	      /* server did not respond */
	      service_publish_failure(service_p, 
				      ipconfig_status_server_not_responding_e);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  if (current_time < dhcp->lease.t2) {
	      dhcp->state = dhcp_cstate_renew_e;
	      wakeup_time = current_time + (dhcp->lease.t2 - current_time) / 2;
	      dest_ip = dhcp->saved.server_ip;
	  }
	  else { /* rebind */
	      dhcp->state = dhcp_cstate_rebind_e;
	      wakeup_time = current_time 
		  + (dhcp->lease.expiration - current_time) / 2;
	      dest_ip = G_ip_broadcast;
	  }
	  dhcp->request->dp_xid = htonl(++dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((uint16_t)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    dest_ip, dhcp->saved.our_ip,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_NOTICE,
		     "DHCP %s: RENEW/REBIND transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
#define RENEW_REBIND_MIN_WAIT_SECS	60
	  if ((wakeup_time - current_time) < RENEW_REBIND_MIN_WAIT_SECS) {
	      tv.tv_sec = RENEW_REBIND_MIN_WAIT_SECS;
	      dhcp->renew_rebind_time = current_time + tv.tv_sec;
	  }
	  else {
	      tv.tv_sec = wakeup_time - current_time;
	      dhcp->renew_rebind_time = wakeup_time;
	  }
	  tv.tv_usec = 0;
	  my_log(LOG_INFO, "DHCP %s: RENEW/REBIND waiting at %ld for %ld.%06d",
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_renew_rebind,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  if (dhcp->wake_time == 0
	      || current_time > dhcp->wake_time
	      || (dhcp->wake_time - current_time) < G_wake_skew_secs) {
	      ServiceSetActiveDuringSleepNeedsAttention(service_p);
	  }
	  break;
      }
      case IFEventID_data_e: {
	  dhcp_lease_time_t	lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  dhcp_lease_time_t	t1;
	  dhcp_lease_time_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip) == FALSE) {
	      /* reject the packet */
	      return;
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      service_publish_failure(service_p, 
				      ipconfig_status_lease_terminated_e);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      return;
	  }
	  dhcp_get_lease_from_options(&pkt->options, &lease, &t1, &t2);

	  /* address has to match, otherwise start over */
	  if (pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      service_publish_failure(service_p, 
				      ipconfig_status_server_error_e);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  dhcp_set_lease_params(service_p, "RENEW/REBIND",
				current_time, lease, t1, t2);
	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  /* ALIGN: saved.pkt is uint32_t aligned, cast ok */
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.server_ip = server_ip;
	  dhcp_bound(service_p, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  return;
    }
    return;

 error:
    dhcp_failed(service_p, status);
    return;
}

#include "arp.h"

static void
dhcp_wait_until_arp_completes(ServiceRef service_p)
{
    struct in_addr		addr_wait = { 0 };
    Service_dhcp_t *		dhcp;
    int				i;
    int				if_index;
    interface_t *		if_p = service_interface(service_p);
    route_msg			msg;
    char			ntopbuf[INET_ADDRSTRLEN];
    struct in_addr		our_mask;
    boolean_t			resolved = FALSE;
    int				s;
    struct sockaddr_dl *	sdl = NULL;
    struct sockaddr_inarp *	sin;
    struct in_addr		subnet;

    dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    our_mask = ServiceGetActiveSubnetMask(service_p);
    subnet.s_addr = dhcp->saved.our_ip.s_addr & our_mask.s_addr;
    if (in_subnet(subnet, our_mask, dhcp->saved.server_ip)) {
	/* we'll ARP for the DHCP server */
	addr_wait = dhcp->saved.server_ip;
    }
    else {
	struct in_addr		router;
	
	router = dhcp_get_router_from_options(&dhcp->saved.options,
					      dhcp->saved.our_ip);
	if (!router_is_valid(router)) {
	    /* all subnet routes are local, so we'll ARP for the DHCP server */
	    addr_wait = dhcp->saved.server_ip;
	}
	else {
	    /* the router is the gateway to the DHCP server */
	    addr_wait = router;
	}
    }
    s = arp_open_routing_socket();
    if (s == -1) {
	my_log(LOG_NOTICE, "DHCP %s: arp_open_routing_socket() failed, %s",
	       if_name(if_p),
	       strerror(errno));
	return;
    }
    /* ALIGN: msg_p->m_space is aligned sufficiently 
     * to dereference sdl safely */
    sin = (struct sockaddr_inarp *)(void *)msg.m_space;
    if_index = if_link_index(if_p);

#define N_ARP_GET_TRIES		5
    
    i = 1;
    while (TRUE) {
	if (arp_get(s, &msg, addr_wait, if_index) != ARP_RETURN_SUCCESS) {
	    goto failed;
	}
	/* ALIGN: msg_p->m_space is aligned sufficiently 
	 * to dereference sdl safely */
	sdl = (struct sockaddr_dl *)(void *)(sin->sin_len + (char *)sin);
	if (sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
	    resolved = TRUE;
	    break;
	}
	if (i == N_ARP_GET_TRIES) {
	    break;
	}
	i++;
	/* sleep for 1 millisecond, and try again */
	usleep(1000);
    }
    if (resolved == FALSE) {
	my_log(LOG_INFO, "DHCP %s: %s was NOT resolved",
	       if_name(if_p),
	       inet_ntop(AF_INET, &addr_wait, ntopbuf, sizeof(ntopbuf)));

    }
    else  {
	  my_log(LOG_INFO,
		 "DHCP %s: %s is resolved, %s after trying %d time(s)",
		 if_name(if_p),
		 inet_ntop(AF_INET, &addr_wait, ntopbuf, sizeof(ntopbuf)),
		 link_ntoa(sdl), i);
    }
 failed:
    close(s);
    return;
}

static void
dhcp_release(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)ServiceGetPrivate(service_p);
    link_status_t	link_status;
    dhcpoa_t	 	options;

    if (dhcp->lease.valid == FALSE) {
	return;
    }

    my_log(LOG_INFO, "DHCP %s: RELEASE", if_name(if_p));
    _dhcp_lease_clear(service_p, FALSE);
    dhcp->lease.valid = FALSE;
    service_router_clear(service_p);

    /* clean-up anything that might have come before */
    dhcp_cancel_pending_events(service_p);

    /* don't bother trying to transmit if the link is down */
    link_status = service_link_status(service_p);
    if (link_status.valid 
	&& link_status.active == FALSE) {
	return;
    }

    /* release the address */
    /* ALIGN: txbuf is aligned to at least sizeof(uint32_t) bytes */
    dhcp->request = make_dhcp_request((struct dhcp *)(void *)dhcp->txbuf, 
				      sizeof(dhcp->txbuf),
				      dhcp_msgtype_release_e,
				      if_link_address(if_p), 
				      if_link_arptype(if_p),
				      if_link_length(if_p), 
				      dhcp->client_id, dhcp->client_id_len,
				      FALSE,
				      &options);
    if (dhcp->request == NULL) {
	return;
    }
    dhcp->request->dp_xid = htonl(++dhcp->xid);
    dhcp->request->dp_ciaddr = dhcp->saved.our_ip;
    if (dhcpoa_add(&options, dhcptag_server_identifier_e,
		   sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	!= dhcpoa_success_e) {
	my_log(LOG_NOTICE, "DHCP %s: RELEASE couldn't add server ip, %s",
	       if_name(if_p), dhcpoa_err(&options));
	return;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	!= dhcpoa_success_e) {
	my_log(LOG_NOTICE, "DHCP %s: RELEASE failed to terminate options",
	       if_name(if_p));
	return;
    }
    if (bootp_client_transmit(dhcp->client,
			      dhcp->saved.server_ip, dhcp->saved.our_ip,
			      G_server_port, G_client_port,
			      dhcp->request, dhcp->request_size) < 0) {
	my_log(LOG_NOTICE,
	       "DHCP %s: RELEASE transmit failed", 
	       if_name(if_p));
	return;
    }
    dhcp_wait_until_arp_completes(service_p);
    dhcp->saved.our_ip.s_addr = 0;
    return;
}

#define DEFAULT_LEASE_LENGTH		(60 * 60)		/* 1 hour */
#define MIN_LEASE_LENGTH		(3) 			/* 3 seconds */
#define MIN_T1_VAL			(2)			/* 2 seconds */
#define MIN_T2_VAL			(2)			/* 2 seconds */

void
dhcp_get_lease_from_options(dhcpol_t * options, dhcp_lease_time_t * lease, 
			    dhcp_lease_time_t * t1, dhcp_lease_time_t * t2)
{
    dhcp_lease_time_t *	lease_opt;
    dhcp_lease_time_t *	t1_opt;
    dhcp_lease_time_t *	t2_opt;

    lease_opt = (dhcp_lease_time_t *)
	dhcpol_find_with_length(options, dhcptag_lease_time_e, 
				sizeof(dhcp_lease_time_t));
    t1_opt = (dhcp_lease_time_t *)
	dhcpol_find_with_length(options, dhcptag_renewal_t1_time_value_e, 
				sizeof(dhcp_lease_time_t));
    t2_opt = (dhcp_lease_time_t *)
	dhcpol_find_with_length(options, dhcptag_rebinding_t2_time_value_e, 
				sizeof(dhcp_lease_time_t));
    if (lease_opt != NULL) {
	*lease = ntohl(*lease_opt);
	if (*lease < MIN_LEASE_LENGTH) {
	    *lease = MIN_LEASE_LENGTH;
	}
    }
    if (t1_opt != NULL) {
	*t1 = ntohl(*t1_opt);
	if (*t1 < MIN_T1_VAL) {
	    *t1 = MIN_T1_VAL;
	}
    }
    if (t2_opt != NULL) {
	*t2 = ntohl(*t2_opt);
	if (*t2 < MIN_T2_VAL) {
	    *t2 = MIN_T2_VAL;
	}
    }
    if (lease_opt == NULL) {
	if (t1_opt != NULL) {
	    *lease = *t1;
	}
	else if (t2_opt != NULL) {
	    *lease = *t2;
	}
	else {
	    *lease = DEFAULT_LEASE_LENGTH;
	}
    }
    if (*lease == DHCP_INFINITE_LEASE) {
	*t1 = *t2 = 0;
    }
    else if (t1_opt == NULL || *t1 >= *lease
	     || t2_opt == NULL || *t2 >= *lease
	     || *t2 < *t1) {
	*t1 = (*lease) / 2;
	*t2 = (dhcp_lease_time_t) ((double)(*lease) * 0.875);
    }
    return;
}

PRIVATE_EXTERN IPv4ClasslessRouteRef
dhcp_copy_classless_routes(dhcpol_t * options_p, int * routes_count_p)
{
    IPv4ClasslessRouteRef	routes = NULL;
    void *			routes_buf;
    int				routes_buf_length;

    routes_buf = dhcpol_option_copy(options_p,
				    dhcptag_classless_static_route_e,
				    &routes_buf_length);
    if (routes_buf != NULL) {
	routes = IPv4ClasslessRouteListCreate(routes_buf,
					      routes_buf_length,
					      routes_count_p);
	free(routes_buf);
    }
    return (routes);
}

/*
 * Function: dhcp_get_router_from_options
 * Purpose:
 *   Retrieve the router from either the router option or the classless
 *   static routes option.  Check whether the router value indicates
 *   proxy ARP, and if so, return IP address 0.0.0.0.
 */
STATIC struct in_addr
dhcp_get_router_from_options(dhcpol_t * options_p, struct in_addr our_ip)
{
    struct in_addr	router;
    struct in_addr *	router_p;
    
    router_set_invalid(&router);
    router_p = dhcpol_find_with_length(options_p, dhcptag_router_e, 
				       sizeof(*router_p));
    if (router_p != NULL) {
	bcopy(router_p, &router, sizeof(router));
	if (router.s_addr == our_ip.s_addr) {
	    router.s_addr = 0;
	}
    }
    else {
	IPv4ClasslessRouteRef	routes;
	int			routes_count;

	routes = dhcp_copy_classless_routes(options_p, &routes_count);
	if (routes != NULL) {
	    IPv4ClasslessRouteRef def_route;

	    def_route = IPv4ClasslessRouteListGetDefault(routes,
							 routes_count);
	    if (def_route != NULL) {
		router = def_route->gate;
	    }
	    free(routes);
	}
    }
    return (router);
}

/*
 * Function: dhcp_get_router_address
 * Purpose:
 *   Find the router IP address to use to detect/identify the network.
 *   Note that if the network is using proxy ARP, this will be the IP address
 *   of the first DNS server.
 *
 * Returns:
 *   TRUE if the router was found and ret_router_p is set,
 *   FALSE otherwise.
 */
PRIVATE_EXTERN boolean_t
dhcp_get_router_address(dhcpol_t * options_p, struct in_addr our_ip,
			struct in_addr * ret_router_p)
{
    boolean_t		is_valid;
    struct in_addr	router;

    router = dhcp_get_router_from_options(options_p, our_ip);
    if (router.s_addr == 0) {
	struct in_addr *	router_p;

	/* proxy arp, use DNS server instead */
	router_p = dhcpol_find_with_length(options_p,
					   dhcptag_domain_name_server_e, 
					   sizeof(*router_p));
	if (router_p != NULL) {
	    bcopy(router_p, &router, sizeof(router));
	}
    }
    else if (router.s_addr == ROUTER_INVALID) {
	struct in_addr *	server_ip_p;

	server_ip_p = dhcpol_find_with_length(options_p,
					      dhcptag_server_identifier_e,
					      sizeof(*server_ip_p));
	if (server_ip_p != NULL) {
	    bcopy(server_ip_p, &router, sizeof(router));
	}
    }
    is_valid = router_is_valid(router);
    if (is_valid) {
	*ret_router_p = router;
    }
    return (is_valid);
}
