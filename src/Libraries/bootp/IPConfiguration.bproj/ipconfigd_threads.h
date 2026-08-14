/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
 * ipconfigd_threads.h
 * - definitions required by the configuration threads
 */

#ifndef _S_IPCONFIGD_THREADS_H
#define _S_IPCONFIGD_THREADS_H

/* 
 * Modification History
 *
 * May 10, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <mach/boolean.h>
#include "dhcp.h"
#include "ipconfig_types.h"
#include "globals.h"
#include "bootp_session.h"
#include "arp_session.h"
#include "timer.h"
#include "interfaces.h"
#include "ipconfigd.h"
#include "dhcp_thread.h"
#include "DHCPv6Options.h"
#include "ifutil.h"
#include "symbol_scope.h"

#define kNetworkSignature	CFSTR("NetworkSignature")

typedef enum {
    IFEventID_start_e = 0,		/* start the configuration method */
    IFEventID_stop_e, 			/* stop/clean-up */
    IFEventID_timeout_e,
    IFEventID_link_status_changed_e,	/* link status changed */
    IFEventID_link_timer_expired_e,	/* link inactive for timeout period */
    IFEventID_data_e,			/* server data to process */
    IFEventID_arp_e,			/* ARP check results */
    IFEventID_change_e,			/* ask config method to change */
    IFEventID_renew_e,			/* ask config method to renew */
    IFEventID_arp_collision_e,		/* there was an ARP collision */
    IFEventID_sleep_e,			/* system will sleep */
    IFEventID_wake_e,			/* system has awoken */
    IFEventID_power_off_e,		/* system is powering off */
    IFEventID_get_dhcp_info_e,		/* event_data is (dhcp_info_t *) */
    IFEventID_get_ipv6_info_e,		/* event_data is (ipv6_info_t *) */
    IFEventID_ipv6_address_changed_e,	/* IPv6 address changed on interface */
    IFEventID_bssid_changed_e, 		/* BSSID has changed */
    IFEventID_active_during_sleep_e,	/* (active_during_sleep_t) */
    IFEventID_ipv6_router_expired_e,	/* (ipv6_router_prefix_counts_t) */
    IFEventID_plat_discovery_complete_e,/* (boolean_t *) */
    IFEventID_forget_ssid_e,		/* forget SSID */
} IFEventID_t;

typedef struct ServiceInfo * ServiceRef;
typedef ipconfig_status_t (IPConfigFunc)
(ServiceRef service_p, 
 IFEventID_t evid, void * evdata);

typedef IPConfigFunc * IPConfigFuncRef;

typedef struct {
    ipconfig_method_data_t	method_data;
    boolean_t			needs_stop;
} change_event_data_t;

typedef struct {
    struct in_addr		ip_addr;
    void *			hwaddr;
    int				hwlen;
    boolean_t			is_sleep_proxy;
    struct in_addr		sleep_proxy_ip;
    CFDataRef			opt_record;
} arp_collision_data_t;

typedef enum {
    kLinkFlagsSSIDChanged	= 0x1,
    kLinkFlagsBSSIDChanged	= 0x2,
} LinkFlags;

typedef struct {
    LinkFlags			flags;
} link_event_data, *link_event_data_t;

/*
 * Function: ip_valid
 * Purpose:
 *   Perform some cursory checks on the IP address
 *   supplied by the server
 */
INLINE boolean_t
ip_valid(struct in_addr ip)
{
    if (ip.s_addr == 0
	|| ip.s_addr == INADDR_BROADCAST)
	return (FALSE);
    return (TRUE);
}

extern void *
find_option_with_length(dhcpol_t * options, dhcptag_t tag, int min_length);

extern char *	computer_name();

void
linklocal_service_change(ServiceRef parent_service_p, boolean_t allocate);

void
linklocal_set_needs_attention(void);

void
linklocal_set_address(ServiceRef ll_service_p, struct in_addr ll_addr);

struct in_addr
linklocal_get_address(ServiceRef ll_service_p);

void
netboot_addresses(struct in_addr * ip, struct in_addr * server_ip);

/* 
 * interface configuration "threads" 
 */
IPConfigFunc bootp_thread;
IPConfigFunc dhcp_thread;
IPConfigFunc manual_thread;
IPConfigFunc inform_thread;
IPConfigFunc linklocal_thread;
IPConfigFunc failover_thread;
IPConfigFunc rtadv_thread;
IPConfigFunc stf_thread;
IPConfigFunc manual_v6_thread;
IPConfigFunc linklocal_v6_thread;

/*
 * more globals
 */
extern bootp_session_t *	G_bootp_session;
extern arp_session_t *		G_arp_session;


/**
 ** ServiceRef accessor functions
 **/

const char *
ServiceGetMethodString(ServiceRef service_p);

boolean_t
ServiceIsIPv4(ServiceRef service_p);

boolean_t
ServiceIsIPv6(ServiceRef service_p);

boolean_t
ServiceIsNetBoot(ServiceRef service_p);

void *
ServiceGetPrivate(ServiceRef service_p);

void
ServiceSetPrivate(ServiceRef service_p, void * private);

void
ServiceSetStatus(ServiceRef service_p, ipconfig_status_t status);

struct in_addr
ServiceGetActiveIPAddress(ServiceRef service_p);

struct in_addr
ServiceGetActiveSubnetMask(ServiceRef service_p);

CFStringRef
ServiceGetSSID(ServiceRef service_p);

void
ServiceSetActiveDuringSleepNeedsAttention(ServiceRef service_p);

CFStringRef
ServiceCopyWakeID(ServiceRef service_p);

CFStringRef
ServiceGetAPNName(ServiceRef service_p);

boolean_t
ServiceIsCGAEnabled(ServiceRef service_p);

boolean_t
ServiceShouldSupplyHostName(ServiceRef service_p);

void
service_set_requested_ip_addr(ServiceRef service_p, struct in_addr ip);

struct in_addr
service_requested_ip_addr(ServiceRef service_p);

void
service_set_requested_ip_mask(ServiceRef service_p, struct in_addr mask);

struct in_addr
service_requested_ip_mask(ServiceRef service_p);

interface_t *
service_interface(ServiceRef service_p);

link_status_t
service_link_status(ServiceRef service_p);

bool
service_is_address_set(ServiceRef service_p);

ServiceRef
service_parent_service(ServiceRef service_p);

int
service_enable_autoaddr(ServiceRef service_p);

int
service_disable_autoaddr(ServiceRef service_p);

int
service_set_address(ServiceRef service_p, struct in_addr ip, 
		    struct in_addr mask, struct in_addr  broadcast);

int
service_remove_address(ServiceRef service_p);

void
ServicePublishSuccessIPv4(ServiceRef service_p, dhcp_info_t * dhcp_info_p);

boolean_t
ServiceDefendIPv4Address(ServiceRef service_p, arp_collision_data_t * arpc);

void
ServicePublishSuccessIPv6(ServiceRef service_p,
			  inet6_addrinfo_t * addr, int addr_count,
			  const struct in6_addr * router, int router_count,
			  ipv6_info_t * dhcp_info_p,
			  CFStringRef signature);
boolean_t
ServiceIsPublished(ServiceRef service_p);

void
ServiceSetRequestedIPv6Address(ServiceRef service_p,
			       const struct in6_addr * addr_p,
			       int prefix_length);
void
ServiceGetRequestedIPv6Address(ServiceRef service_p, 
			       struct in6_addr * addr_p,
			       int * prefix_length);
int
ServiceSetIPv6Address(ServiceRef service_p, const struct in6_addr * addr_p,
		      int prefix_length,
		      u_int32_t flags,
		      u_int32_t valid_lifetime,
		      u_int32_t preferred_lifetime);

void
ServiceRemoveIPv6Address(ServiceRef service_p,
			 const struct in6_addr * addr_p, int prefix_length);


boolean_t
ServiceDADIsEnabled(ServiceRef service_p);

void
ServiceGenerateFailureSymptom(ServiceRef service_p);

void
ServiceSetBusy(ServiceRef service_p, boolean_t busy);

void
service_publish_failure(ServiceRef service_p, ipconfig_status_t status);

void
service_publish_failure_sync(ServiceRef service_p, ipconfig_status_t status,
			     boolean_t sync);

#if TARGET_OS_OSX
void
ServiceReportIPv4AddressConflict(ServiceRef service_p, struct in_addr ip);

void
ServiceRemoveAddressConflict(ServiceRef service_p);

void
ServiceReportIPv6AddressConflict(ServiceRef service_p,
				 const struct in6_addr * addr_p);

#else /* TARGET_OS_OSX */

INLINE void
ServiceReportIPv4AddressConflict(ServiceRef service_p, struct in_addr ip)
{
    /* nothing to do */
}

INLINE void
ServiceRemoveAddressConflict(ServiceRef service_p)
{
    /* nothing to do */
}

INLINE void
ServiceReportIPv6AddressConflict(ServiceRef service_p,
				 const struct in6_addr * addr_p)
{
    /* nothing to do */
}

#endif /* TARGET_OS_OSX */

CFStringRef
ServiceGetInterfaceName(ServiceRef service_p);

/**
 ** 464XLAT routines
 **/
boolean_t	service_clat46_is_enabled(ServiceRef service_p);
boolean_t	service_nat64_prefix_available(ServiceRef service_p);
boolean_t	service_plat_discovery_failed(ServiceRef service_p);


/**
 ** router_arp routines
 **/

/* Router IP Address */
boolean_t	service_router_is_hwaddr_valid(ServiceRef service_p);
void		service_router_set_hwaddr_valid(ServiceRef service_p);
void		service_router_clear_hwaddr_valid(ServiceRef service_p);
uint8_t *	service_router_hwaddr(ServiceRef service_p);
int		service_router_hwaddr_size(ServiceRef service_p);

/* Router HW Address */
boolean_t	service_router_is_iaddr_valid(ServiceRef service_p);
void		service_router_set_iaddr_valid(ServiceRef service_p);
void		service_router_clear_iaddr_valid(ServiceRef service_p);
struct in_addr	service_router_iaddr(ServiceRef service_p);
void		service_router_set_iaddr(ServiceRef service_p,
					 struct in_addr iaddr);

/* Router ARP verified */
boolean_t	service_router_is_arp_verified(ServiceRef service_p);
void		service_router_set_arp_verified(ServiceRef service_p);
void		service_router_clear_arp_verified(ServiceRef service_p);

/* Router all (IP, HWAddr, ARP verified) */
void		service_router_clear(ServiceRef service_p);
boolean_t	service_router_all_valid(ServiceRef service_p);
void		service_router_set_all_valid(ServiceRef service_p);

typedef enum {
    router_arp_status_success_e = 0,
    router_arp_status_no_response_e = 1,
    router_arp_status_failed_e = 99,
} router_arp_status_t;

typedef void (service_resolve_router_callback_t)(ServiceRef service_p,
						 router_arp_status_t status);

boolean_t
service_resolve_router(ServiceRef service_p, arp_client_t * arp,
		       service_resolve_router_callback_t * callback_func,
		       struct in_addr our_ip);
boolean_t
service_update_router_address(ServiceRef service_p,
			      dhcpol_t * options_p, struct in_addr our_ip);

boolean_t
service_populate_router_arpinfo(ServiceRef service_p,
				arp_address_info_t * info_p);

boolean_t
service_is_using_ip(ServiceRef exclude_service_p, struct in_addr iaddr);

interface_list_t *
get_interface_list(void);

#endif /* _S_IPCONFIGD_THREADS_H */
