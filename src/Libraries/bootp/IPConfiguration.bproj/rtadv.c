/*
 * Copyright (c) 2003-2021 Apple Inc. All rights reserved.
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
 * rtadv.c
 * - IPv6 Router Advertisement
 * - sends router solicitation requests, and waits for responses
 * - reads router advertisement messages, and from it, gleans the source
 *   IPv6 address to use as the Router
 */

/* 
 * Modification History
 *
 * October 6, 2009		Dieter Siegmund (dieter@apple.com)
 * - added support for DHCPv6
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

#include "cfutil.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"
#include "globals.h"
#include "timer.h"
#include "ifutil.h"
#include "rtutil.h"
#include "util.h"
#include "symbol_scope.h"
#include "RouterAdvertisement.h"
#include "DHCPv6Client.h"
#include "RTADVSocket.h"
#include "DNSNameList.h"
#include "IPv6AWDReport.h"


typedef struct {
    RTADVSocketRef		sock;
    timer_callout_t *		timer;
    RouterAdvertisementRef	ra;
    DHCPv6ClientRef		dhcp_client;
    int				try;
    CFAbsoluteTime		start;
    CFAbsoluteTime		dhcpv6_complete;
    boolean_t			success_report_submitted;
    boolean_t			lladdr_ok; /* ok to send link-layer address */
    boolean_t			renew;
    uint32_t			restart_count;
    boolean_t			has_autoconf_address;
    boolean_t			clat46_address_set;
    boolean_t			router_lifetime_zero;
} Service_rtadv_t;

STATIC void
rtadv_address_changed(ServiceRef service_p);

STATIC struct in_addr
S_get_clat46_address(void)
{
    struct in_addr	clat46_address;

    /* CLAT46 IPv4 address: 192.0.0.1 */
    clat46_address.s_addr = htonl(IN_SERVICE_CONTINUITY + 1);
    return (clat46_address);
}

STATIC CFDictionaryRef
S_ipv4_clat46_dict_copy(CFStringRef ifname)
{
    struct in_addr 		clat46_address;
    CFMutableDictionaryRef	ipv4_dict;

    clat46_address = S_get_clat46_address();
    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* Addresses */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4Addresses,
					    clat46_address);
    /* Router */
    my_CFDictionarySetIPAddressAsString(ipv4_dict,
					kSCPropNetIPv4Router,
					clat46_address);

    /* InterfaceName */
    CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, ifname);

    /* CLAT46 */
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4CLAT46, kCFBooleanTrue);

    return (ipv4_dict);
}


STATIC void
rtadv_set_clat46_address(ServiceRef service_p)
{
    struct in_addr	addr;
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	mask;
    int			ret = 0;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    int			s;

    if (rtadv->clat46_address_set) {
	return;
    }
    s = inet_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR, "socket failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    addr = S_get_clat46_address();
    mask.s_addr = INADDR_BROADCAST;
    ret = inet_aifaddr(s, if_name(if_p), addr, &mask, &addr);
    if (ret == 0) {
	uint64_t	eflags = 0;

	(void)interface_get_eflags(s, if_name(if_p), &eflags);
	if ((eflags & IFEF_CLAT46) != 0
	    || inet6_clat46_start(if_name(if_p)) == 0) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: CLAT46 enabled using address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&addr));
	    rtadv->clat46_address_set = TRUE;
	}
	else {
	    my_log(LOG_ERR,
		   "RTADV %s: failed to enable CLAT46",
		   if_name(if_p));
	    (void)inet_difaddr(s, if_name(if_p), addr);
	}
	flush_routes(if_link_index(if_p), G_ip_zeroes, addr);
    }
    else {
	my_log(LOG_NOTICE,
	       "RTADV %s: set CLAT46 address " IP_FORMAT " failed, %s (%d)",
	       if_name(if_p), IP_LIST(&addr), strerror(ret), ret);
    }
    close(s);
    return;
}

STATIC void
rtadv_remove_clat46_address(ServiceRef service_p)
{
    struct in_addr	addr;
    uint64_t		eflags = 0;
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    int			s;

    s = inet_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR, "socket failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    addr = S_get_clat46_address();
    if (inet_difaddr(s, if_name(if_p), addr) == 0) {
	my_log(LOG_NOTICE,
	       "RTADV %s: removed CLAT46 address " IP_FORMAT,
	       if_name(if_p), IP_LIST(&addr));
	flush_routes(if_link_index(if_p), G_ip_zeroes, addr);
    }
    else if (rtadv->clat46_address_set) {
	int	error = errno;

	my_log(LOG_NOTICE,
	       "RTADV %s: remove CLAT46 address " IP_FORMAT " failed, %s (%d)",
	       if_name(if_p), IP_LIST(&addr), strerror(error), error);
    }
    (void)interface_get_eflags(s, if_name(if_p), &eflags);
    if ((eflags & IFEF_CLAT46) != 0) {
	inet6_clat46_stop(if_name(if_p));
    }
    rtadv->clat46_address_set = FALSE;
    close(s);
    return;
}

STATIC void
rtadv_cancel_pending_events(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    timer_cancel(rtadv->timer);
    RTADVSocketDisableReceive(rtadv->sock);
    return;
}

STATIC boolean_t
rtadv_router_lladdr_collision(ServiceRef service_p)
{
    boolean_t			collision = FALSE;
    interface_t *		if_p = service_interface(service_p);
    struct in6_addr		linklocal_addr;
    Service_rtadv_t *		rtadv;
    const struct in6_addr *	router_p;

    if (if_ift_type(if_p) != IFT_CELLULAR) {
	/* don't care except for cellular */
	goto done;
    }
    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    if (rtadv->ra == NULL) {
	goto done;
    }
    if (!inet6_get_linklocal_address(if_link_index(if_p), &linklocal_addr)) {
	/* couldn't get our link-local address */
	goto done;
    }
    router_p = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
    if (IN6_ARE_ADDR_EQUAL(router_p, &linklocal_addr)) {
	collision = TRUE;
    }
 done:
    return (collision);
}

STATIC void
rtadv_submit_awd_report(ServiceRef service_p, boolean_t success)
{
    inet6_addrlist_t	addrs;
    CFStringRef		apn_name = NULL;
    boolean_t		autoconf_active = FALSE;
    boolean_t		dns_complete = FALSE;
    bool		has_search = FALSE;
    int			i;
    interface_t *	if_p = service_interface(service_p);
    int			prefix_count = 0;
    IPv6AWDReportRef	report;
    int			router_count = 0;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct in6_ifstat	stats;
    inet6_addrinfo_t *	scan;
    InterfaceType	type;

    if (success) {
	if (rtadv->success_report_submitted) {
	    return;
	}
	if (rtadv->ra == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: success yet ra is NULL?",
		   if_name(if_p));
	    return;
	}
    }
    switch (if_ift_type(if_p)) {
    case IFT_CELLULAR:
	type = kInterfaceTypeCellular;
	apn_name = ServiceGetAPNName(service_p);
	break;
    case IFT_ETHER:
	type = if_is_wireless(if_p) ? kInterfaceTypeWiFi : kInterfaceTypeWired;
	break;
    default:
	type = kInterfaceTypeOther;
	break;
    }

    switch (G_awd_interface_types) {
    case kIPConfigurationInterfaceTypesCellular:
	if (type != kInterfaceTypeCellular) {
	    /* ignore non-cellular interface */
	    return;
	}
	break;
    case kIPConfigurationInterfaceTypesAll:
	break;
    default:
	/* don't generate awd reports */
	return;
    }

    report = IPv6AWDReportCreate(type);
    if (report == NULL) {
	return;
    }

    /* get the addresses and set whether flags are set or not */
    inet6_addrlist_copy(&addrs, if_link_index(if_p));
    for (i = 0, scan = addrs.list; i < addrs.count; i++, scan++) {
	if (IN6_IS_ADDR_LINKLOCAL(&scan->addr)) {
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		IPv6AWDReportSetLinkLocalAddressDuplicated(report);
	    }
	}
	else if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0) {
	    IPv6AWDReportSetAutoconfAddressAcquired(report);
	    autoconf_active = TRUE;
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		IPv6AWDReportSetAutoconfAddressDuplicated(report);
	    }
	    if ((scan->addr_flags & IN6_IFF_DEPRECATED) != 0) {
		IPv6AWDReportSetAutoconfAddressDeprecated(report);
	    }
	    if ((scan->addr_flags & IN6_IFF_DETACHED) != 0) {
		IPv6AWDReportSetAutoconfAddressDetached(report);
	    }
	}
    }
    inet6_addrlist_free(&addrs);

    if (apn_name != NULL) {
	IPv6AWDReportSetAPNName(report, apn_name);
    }
    if (autoconf_active && rtadv->ra != NULL) {
	uint32_t	prefix_preferred_lifetime;
	uint32_t	prefix_valid_lifetime;
	uint32_t	router_lifetime;

	router_lifetime = RouterAdvertisementGetRouterLifetime(rtadv->ra);
	prefix_preferred_lifetime
	    = RouterAdvertisementGetPrefixLifetimes(rtadv->ra,
						    &prefix_valid_lifetime);
	IPv6AWDReportSetRouterLifetime(report, router_lifetime);
	IPv6AWDReportSetPrefixPreferredLifetime(report,
						prefix_preferred_lifetime);
	IPv6AWDReportSetPrefixValidLifetime(report, prefix_valid_lifetime);

	/* set cellular-specific properties */
	if (type == kInterfaceTypeCellular) {
	    if (router_lifetime != ROUTER_LIFETIME_MAXIMUM) {
		IPv6AWDReportSetRouterLifetimeNotMaximum(report);
	    }
	    if (prefix_valid_lifetime != ND6_INFINITE_LIFETIME) {
		IPv6AWDReportSetPrefixLifetimeNotInfinite(report);
	    }
	}
    }
    /* 464XLAT */
    if (service_clat46_is_enabled(service_p)) {
	IPv6AWDReportSetXLAT464Enabled(report);
    }

    /* DNS options from RA */
    if (rtadv->ra != NULL
	&& RouterAdvertisementGetRDNSS(rtadv->ra, NULL, NULL) != NULL) {
	IPv6AWDReportSetAutoconfRDNSS(report);
	dns_complete = TRUE;
	if (RouterAdvertisementGetDNSSL(rtadv->ra, NULL, NULL) != NULL) {
	    IPv6AWDReportSetAutoconfDNSSL(report);
	}
    }

    /* DNS options from DHCPv6 */
    if (rtadv->dhcp_client != NULL
	&& DHCPv6ClientHasDNS(rtadv->dhcp_client, &has_search)) {
	dns_complete = TRUE;
	IPv6AWDReportSetDHCPv6DNSServers(report);
	if (has_search) {
	    IPv6AWDReportSetDHCPv6DNSDomainList(report);
	}
    }

    if (success) {
	/* success report */
	CFAbsoluteTime	complete;
	CFTimeInterval	delta;

	complete = RouterAdvertisementGetReceiveTime(rtadv->ra);
	if (complete > rtadv->start) {
	    delta = complete - rtadv->start;
	    IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(report, delta);
	}
	if (rtadv->dhcpv6_complete != 0
	    && rtadv->dhcpv6_complete > rtadv->start) {
	    delta = rtadv->dhcpv6_complete - rtadv->start;
	    IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(report, delta);
	}
	if (dns_complete) {
	    CFAbsoluteTime	now = timer_get_current_time();

	    if (now > rtadv->start) {
		delta = now - rtadv->start;
		IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(report,
								   delta);
	    }
	}
	rtadv->success_report_submitted = TRUE;
    }
    else {
	/* failure report */
	if (rtadv->router_lifetime_zero) {
	    IPv6AWDReportSetRouterLifetimeZero(report);
	}
#if 0
	/* TBD metric to handle */
	IPv6AWDReportSetControlQueueUnsentCount(report, count);
#endif
	if (service_plat_discovery_failed(service_p)) {
	    IPv6AWDReportSetXLAT464PLATDiscoveryFailed(report);
	}
	rtadv->success_report_submitted = FALSE;
    }
    if (rtadv_router_lladdr_collision(service_p)) {
	IPv6AWDReportSetRouterSourceAddressCollision(report);
    }

    if (rtadv->try >= MAX_RTR_SOLICITATIONS) {
	IPv6AWDReportSetRouterSolicitationCount(report, MAX_RTR_SOLICITATIONS);
    }
    else {
	IPv6AWDReportSetRouterSolicitationCount(report, rtadv->try);
    }
    if (inet6_ifstat(if_name(if_p), &stats) == 0) {
	if (stats.ifs6_pfx_expiry_cnt != 0) {
	    IPv6AWDReportSetExpiredPrefixCount(report,
					       stats.ifs6_pfx_expiry_cnt);
	}
	if (stats.ifs6_defrtr_expiry_cnt != 0) {
	    IPv6AWDReportSetExpiredDefaultRouterCount(report,
						      stats.ifs6_defrtr_expiry_cnt);
	}
    }
    router_count = inet6_router_and_prefix_count(if_link_index(if_p),
						 &prefix_count);
    if (router_count > 0) {
	IPv6AWDReportSetDefaultRouterCount(report, router_count);
	IPv6AWDReportSetPrefixCount(report, prefix_count);
    }
    if (rtadv->restart_count != 0) {
	IPv6AWDReportSetAutoconfRestarted(report);
    }
#if 0
    /* TBD metric to handle */
    IPv6AWDReportSetManualAddressConfigured(report); /* need in manual_v6.c */
#endif

    IPv6AWDReportSubmit(report);
    my_log(LOG_NOTICE, "%s: submitted AWD %s report %@", if_name(if_p),
	   success ? "success" : "failure", report);

    CFRelease(report);
    return;
}

STATIC void
rtadv_submit_awd_success_report(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    if (rtadv->dhcp_client != NULL
	&& DHCPv6ClientIsActive(rtadv->dhcp_client)) {
	/* waiting for DHCPv6 to complete */
	return;
    }
    rtadv_submit_awd_report(service_p, TRUE);
}

STATIC void
rtadv_failed(ServiceRef service_p, ipconfig_status_t status)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->try = 0;
    my_CFRelease(&rtadv->ra);
    rtadv_cancel_pending_events(service_p);
    inet6_rtadv_disable(if_name(service_interface(service_p)));
    if (service_clat46_is_enabled(service_p)) {
	rtadv_remove_clat46_address(service_p);
    }
    service_publish_failure(service_p, status);
    rtadv->router_lifetime_zero = FALSE;
    ServiceSetBusy(service_p, FALSE);
    return;
}

STATIC void
rtadv_inactive(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv;

    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    inet6_flush_prefixes(if_name(if_p));
    inet6_flush_routes(if_name(if_p));
    rtadv_failed(service_p, ipconfig_status_media_inactive_e);
    rtadv->restart_count = 0;
    return;
}

static void
rtadv_log_ra(const char * ifname, RouterAdvertisementRef ra)
{
    CFStringRef	description;

    description = RouterAdvertisementCopyDescription(ra);
    my_log(LOG_INFO, "RTADV %s: received RA %@", ifname, description);
    CFRelease(description);
}

STATIC void
rtadv_acquired(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    CFAbsoluteTime		dns_expiration = 0;
    interface_t *		if_p = service_interface(service_p);
    CFAbsoluteTime		now;
    RouterAdvertisementRef	ra;
    Service_rtadv_t *		rtadv;

    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
    case IFEventID_start_e:
	/* no more retries */
	timer_cancel(rtadv->timer);
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)rtadv_acquired,
				 service_p, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_data_e:
	ra = (RouterAdvertisementRef)event_data;
	rtadv_log_ra(if_name(if_p), ra);
	if (rtadv->ra != NULL) {
	    const struct in6_addr *	current;
	    const struct in6_addr *	this;

	    /* XXX only track state of one router right now */
	    current = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
	    this = RouterAdvertisementGetSourceIPAddress(ra);
	    if (!IN6_ARE_ADDR_EQUAL(current, this)) {
		my_log(LOG_INFO, "RTADV %s: ignoring RA (not from %@)",
		       if_name(if_p),
		       RouterAdvertisementGetSourceIPAddressAsString(rtadv->ra));
		CFRelease(ra);
		break;
	    }
	}
	else if (RouterAdvertisementGetRouterLifetime(ra) == 0) {
	    rtadv->router_lifetime_zero = TRUE;
	    my_log(LOG_INFO,
		   "RTADV %s: ignoring RA (lifetime zero)", if_name(if_p));
	    CFRelease(ra);
	    break;
	}

	/* toss old state */
	timer_cancel(rtadv->timer);
	my_CFRelease(&rtadv->ra);

	/* abandon RA if the lifetime is now zero */
	if (RouterAdvertisementGetRouterLifetime(ra) == 0) {
	    /* router is no longer eligible to be primary */
	    my_log(LOG_INFO,
		   "RTADV %s: router lifetime became zero",
		   if_name(if_p));
	    my_CFRelease(&ra);
	}
	else {
	    /* save the new RA */
	    rtadv->ra = ra;
	}

	/* kick off DHCP if needed */
	if (rtadv->dhcp_client != NULL) {
	    uint8_t	flags = 0;

	    if (rtadv->ra != NULL) {
		flags = RouterAdvertisementGetFlags(rtadv->ra);
	    }
	    if ((flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER)) != 0) {
		DHCPv6ClientMode	expected_mode;
		DHCPv6ClientMode	mode;

		expected_mode = (flags & ND_RA_FLAG_MANAGED) != 0
		    ? kDHCPv6ClientModeStateful : kDHCPv6ClientModeStateless;
		mode = DHCPv6ClientGetMode(rtadv->dhcp_client);
		if (mode != expected_mode) {
		    bool stateful;

		    stateful = (expected_mode == kDHCPv6ClientModeStateful);
		    DHCPv6ClientStart(rtadv->dhcp_client,
				      (G_dhcpv6_stateful_enabled
				       && stateful));
		}
	    }
	    else if (DHCPv6ClientIsActive(rtadv->dhcp_client)) {
		DHCPv6ClientStop(rtadv->dhcp_client, FALSE);
	    }
	}
	if (rtadv->ra != NULL) {
	    /* if needed, set a DNS expiration timer */
	    now = RouterAdvertisementGetReceiveTime(rtadv->ra);
	    dns_expiration
		= RouterAdvertisementGetDNSExpirationTime(rtadv->ra, now);
	}
	if (dns_expiration != 0) {
	    CFDateRef	date;

	    date = CFDateCreate(NULL, dns_expiration);
	    my_log(LOG_NOTICE, "RTADV %s: DNS expiration timeout %@",
		   if_name(if_p), date);
	    CFRelease(date);
	    timer_callout_set_absolute(rtadv->timer,
				       dns_expiration,
				       (timer_func_t *)rtadv_acquired,
				       service_p,
				       (void *)IFEventID_timeout_e,
				       NULL);
	}
	/* DNS may have been updated, force publish */
	rtadv_address_changed(service_p);
	break;
    case IFEventID_timeout_e:
	if (rtadv->ra == NULL) {
	    break;
	}
	/* DNS expired, force publish */
	rtadv_address_changed(service_p);

	/* check again, rtadv->ra could have been released */
	if (rtadv->ra == NULL) {
	    break;
	}

	/* check if we need to set another timer */
	now = timer_get_current_time();
	dns_expiration
	    = RouterAdvertisementGetDNSExpirationTime(rtadv->ra, now);
	if (dns_expiration != 0) {
	    CFDateRef	date;

	    date = CFDateCreate(NULL, dns_expiration);
	    my_log(LOG_NOTICE, "RTADV %s: DNS expiration timeout %@",
		   if_name(if_p), date);
	    CFRelease(date);
	    timer_callout_set_absolute(rtadv->timer,
				       dns_expiration,
				       (timer_func_t *)rtadv_acquired,
				       service_p,
				       (void *)IFEventID_timeout_e,
				       NULL);
	}
	break;
    default:
	break;
    }
}

STATIC void
rtadv_solicit(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    int			error;
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    switch (event_id) {
    case IFEventID_start_e:
	my_log(LOG_INFO, "RTADV %s: solicit", if_name(if_p));
	rtadv->start = timer_get_current_time();
	rtadv->dhcpv6_complete = 0;
	rtadv->success_report_submitted = FALSE;
	rtadv->router_lifetime_zero = FALSE;
	rtadv_remove_clat46_address(service_p);
	rtadv_cancel_pending_events(service_p);
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)rtadv_solicit,
				 service_p, (void *)IFEventID_data_e);
	if (inet6_rtadv_enable(if_name(if_p),
			       ServiceIsCGAEnabled(service_p)) != 0) {
	    rtadv_failed(service_p, ipconfig_status_internal_error_e);
	    return;
	}
	rtadv->try = 0;
	my_CFRelease(&rtadv->ra);
	rtadv->has_autoconf_address = FALSE;

	/* FALL THROUGH */

    case IFEventID_timeout_e:
	rtadv->try++;
	if (rtadv->try > 1) {
	    link_status_t	link_status = service_link_status(service_p);

	    if (link_status.valid == TRUE
		&& link_status.active == FALSE) {
		rtadv_inactive(service_p);
		return;
	    }
	}
	if (rtadv->try > MAX_RTR_SOLICITATIONS) {
	    if (rtadv->try == (MAX_RTR_SOLICITATIONS + 1)) {
		/* set a timer that fires if we don't get an address */
#define ADDRESS_ACQUISITION_FAILURE_TIMEOUT	20
		tv.tv_sec = ADDRESS_ACQUISITION_FAILURE_TIMEOUT;
		tv.tv_usec = 0;
		timer_set_relative(rtadv->timer, tv,
				   (timer_func_t *)rtadv_solicit,
				   service_p,
				   (void *)IFEventID_timeout_e, NULL);
	    }
	    else if (rtadv->try == (MAX_RTR_SOLICITATIONS + 2)) {
		/* generate a symptom if we don't get an address */
		ServiceGenerateFailureSymptom(service_p);
		ServiceSetBusy(service_p, FALSE);
	    }
	    /* now we just wait to see if something comes in */
	    return;
	}
	my_log(LOG_INFO, 
	       "RTADV %s: sending Router Solicitation (%d of %d)",
	       if_name(if_p), rtadv->try, MAX_RTR_SOLICITATIONS);
	error = RTADVSocketSendSolicitation(rtadv->sock,
					    rtadv->lladdr_ok);
	switch (error) {
	case 0:
	case ENXIO:
	case ENETDOWN:
	case EADDRNOTAVAIL:
	    break;
	default:
	    my_log(LOG_ERR, "RTADV %s: send Router Solicitation: failed, %s",
		   if_name(if_p), strerror(error));
	    break;
	}
	
	/* set timer values and wait for responses */
	tv.tv_sec = RTR_SOLICITATION_INTERVAL;

	/* don't add random fuzz for cellular rdar://problem/31542146 */
	tv.tv_usec = (if_ift_type(if_p) == IFT_CELLULAR)
	    ? 0
	    : (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	timer_set_relative(rtadv->timer, tv,
			   (timer_func_t *)rtadv_solicit,
			   service_p, (void *)IFEventID_timeout_e, NULL);
	break;

    case IFEventID_data_e:
	rtadv_acquired(service_p, IFEventID_start_e, event_data);
	break;
    default:
	break;
    }
    return;
}

STATIC void
rtadv_flush(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    /* flush IPv6 configuration from the interface and unpublish */
    inet6_flush_prefixes(if_name(if_p));
    inet6_flush_routes(if_name(if_p));
    inet6_rtadv_disable(if_name(if_p));
    if (rtadv->dhcp_client != NULL) {
	DHCPv6ClientStop(rtadv->dhcp_client, TRUE);
    }
    rtadv_failed(service_p, ipconfig_status_network_changed_e);
    return;
}

STATIC void
rtadv_flush_and_restart(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->restart_count++;
    my_log(LOG_NOTICE, "RTADV %s: flushing and restarting (count %u)",
	   if_name(if_p), rtadv->restart_count);
    rtadv_flush(service_p);
    rtadv_solicit(service_p, IFEventID_start_e, NULL);
    return;
}

STATIC void
rtadv_router_expired(ServiceRef service_p,
		     ipv6_router_prefix_counts_t * event)
{
    interface_t *	if_p = service_interface(service_p);

    my_log(LOG_NOTICE, "RTADV %s: router expired", if_name(if_p));

    /* generate metric when the router expires */
    rtadv_submit_awd_report(service_p, FALSE);

    if (event->router_count == 0) {
	/* no more default routers, flush and start over */
	rtadv_flush_and_restart(service_p);
    }
    else {
	/* the router expired but we still have a router */
	rtadv_solicit(service_p, IFEventID_start_e, NULL);
    }
    return;
}

STATIC CFStringRef
create_signature(RouterAdvertisementRef ra,
		 inet6_addrinfo_t * list_p, int list_count)
{
    const uint8_t *	lladdr;
    int			lladdr_length;
    struct in6_addr	netaddr;
    char 		ntopbuf[INET6_ADDRSTRLEN];
    CFMutableStringRef	sig_str;

    if (list_p == NULL || list_count == 0 || ra == NULL) {
	return (NULL);
    }

    lladdr = RouterAdvertisementGetSourceLinkAddress(ra, &lladdr_length);
    if (lladdr == NULL) {
	return (NULL);
    }
    netaddr = list_p[0].addr;
    in6_netaddr(&netaddr, list_p[0].prefix_length);
    sig_str = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sig_str, NULL, 
			 CFSTR("IPv6.Prefix=%s/%d;IPv6.RouterHardwareAddress="),
			 inet_ntop(AF_INET6, &netaddr,
				   ntopbuf, sizeof(ntopbuf)),
			 list_p[0].prefix_length);
    my_CFStringAppendBytesAsHex(sig_str, lladdr, lladdr_length, ':');
    return (sig_str);
}

#ifndef IN6_IFF_SWIFTDAD
#define IN6_IFF_SWIFTDAD	0x0800  /* DAD with no delay */
#endif /* IN6_IFF_SWIFTDAD */

STATIC void
rtadv_trigger_dad(ServiceRef service_p, inet6_addrinfo_t * list, int count)
{
    int			i;
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	scan;
    int			sockfd;

    sockfd = inet6_dgram_socket();
    if (sockfd < 0) {
	my_log(LOG_ERR,
	       "RTADV %s: failed to open socket, %s",
	       if_name(if_p), strerror(errno));
	return;
    }
    for (i = 0, scan = list; i < count; scan++, i++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	if (inet6_aifaddr(sockfd, if_name(if_p),
			  &scan->addr, NULL, scan->prefix_length,
			  scan->addr_flags | IN6_IFF_SWIFTDAD,
			  scan->valid_lifetime,
			  scan->preferred_lifetime) < 0) {
	    my_log(LOG_ERR,
		   "RTADV %s: inet6_aifaddr(%s/%d) failed, %s",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length, strerror(errno));
	}
	else {
	    my_log(LOG_INFO,
		   "RTADV %s: Re-assigned %s/%d",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length);
	}
    }
    close(sockfd);
    return;
}

STATIC void
rtadv_address_changed_common(ServiceRef service_p,
			     inet6_addrlist_t * addr_list_p)
{
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	linklocal;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    boolean_t		try_was_zero = FALSE;

    linklocal = inet6_addrlist_get_linklocal(addr_list_p);
    if (linklocal == NULL) {
	/* no linklocal address assigned, nothing to do */
	my_log(LOG_INFO,
	       "RTADV %s: link-local address not present",
	       if_name(if_p));
	return;
    }
    if ((linklocal->addr_flags & IN6_IFF_NOTREADY) != 0) {
	if ((linklocal->addr_flags & IN6_IFF_DUPLICATED) != 0) {
	    /* address conflict occurred */
	    my_log(LOG_NOTICE,
		   "RTADV %s: link-local address is duplicated",
		   if_name(if_p));
	    return;
	}
	/* linklocal address isn't ready */
	my_log(LOG_INFO,
	       "RTADV %s: link-local address is not ready",
	       if_name(if_p));
	return;
    }
    rtadv->lladdr_ok = (linklocal->addr_flags & IN6_IFF_DADPROGRESS) == 0;
    my_log(LOG_INFO,
	   "RTADV %s: link-layer option in Router Solicitation is %sOK",
	   if_name(if_p), rtadv->lladdr_ok ? "" : "not ");
    if (rtadv->try == 0) {
	link_status_t	link_status = service_link_status(service_p);

	try_was_zero = TRUE;
	if (link_status.valid == FALSE
	    || link_status.active == TRUE) {
	    my_log(LOG_INFO,
		   "RTADV %s: link-local address is ready, starting",
		   if_name(if_p));
	    rtadv_solicit(service_p, IFEventID_start_e, NULL);
	}
    }
    if (rtadv->renew || !try_was_zero) {
	int			count;
	boolean_t		dhcp_has_address = FALSE;
	inet6_addrlist_t	dhcp_addr_list;
	uint32_t		autoconf_count = 0;
	uint32_t		deprecated_count = 0;
	uint32_t		detached_count = 0;
	int			i;
	ipv6_info_t		info;
	inet6_addrinfo_t	list[addr_list_p->count];
	const struct in6_addr *	router_p = NULL;
	int			router_count = 0;
	inet6_addrinfo_t *	scan;
	CFStringRef		signature = NULL;

	inet6_addrlist_init(&dhcp_addr_list);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientCopyAddresses(rtadv->dhcp_client, &dhcp_addr_list);
	    dhcp_has_address = (dhcp_addr_list.count != 0);
	}

	/* only copy autoconf and DHCP addresses */
	for (i = 0, count = 0, scan = addr_list_p->list;
	     i < addr_list_p->count;
	     i++, scan++) {
	    boolean_t	autoconf_address = FALSE;

	    if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0) {
		autoconf_count++;
		autoconf_address = TRUE;
	    }
	    if ((scan->addr_flags & IN6_IFF_NOTREADY) != 0) {
		if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		    my_log(LOG_NOTICE, "RTADV %s: duplicated address",
			   if_name(if_p));
		}
		continue;
	    }
	    if (autoconf_address) {
		if ((scan->addr_flags & IN6_IFF_DEPRECATED) != 0) {
		    deprecated_count++;
		}
		if ((scan->addr_flags & IN6_IFF_DETACHED) != 0) {
		    detached_count++;
		}
	    }
	    if (autoconf_address
		|| inet6_addrlist_contains_address(&dhcp_addr_list, scan)) {
		list[count++] = *scan;
	    }
	}
	inet6_addrlist_free(&dhcp_addr_list);
	if (autoconf_count == 0 && rtadv->has_autoconf_address) {
	    my_log(LOG_NOTICE, "RTADV %s: autoconf addresses expired",
		   if_name(if_p));
	    rtadv_submit_awd_report(service_p, FALSE);
	    rtadv_flush_and_restart(service_p);
	    return;
	}
	if (count == 0) {
	    return;
	}
	if (autoconf_count != 0
	    && (detached_count + deprecated_count) == autoconf_count) {
	    my_log(LOG_INFO,
		   "RTADV %s: all autoconf addresses detached/deprecated",
		   if_name(if_p));
	}
	rtadv->has_autoconf_address = (autoconf_count != 0);

	bzero(&info, sizeof(info));
	if (rtadv->ra != NULL) {
	    /* fill in information from DHCPv6 */
	    if (rtadv->dhcp_client != NULL
		&& DHCPv6ClientGetInfo(rtadv->dhcp_client, &info)) {
		if (dhcp_has_address && rtadv->dhcpv6_complete == 0) {
		    rtadv->dhcpv6_complete = timer_get_current_time();
		}
	    }
	    info.ra = rtadv->ra;
	    if (service_clat46_is_enabled(service_p)) {
		info.perform_plat_discovery = TRUE;
		if (service_nat64_prefix_available(service_p)) {
		    CFStringRef	ifname = ServiceGetInterfaceName(service_p);

		    rtadv_set_clat46_address(service_p);
		    info.ipv4_dict = S_ipv4_clat46_dict_copy(ifname);
		}
		else {
		    rtadv_remove_clat46_address(service_p);
		}
	    }
	    else if (if_ift_type(if_p) != IFT_CELLULAR) {
		info.perform_plat_discovery = TRUE;
	    }
	    router_p = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
	    router_count = 1;
	    signature = create_signature(rtadv->ra, list, count);
	}
	ServicePublishSuccessIPv6(service_p, list, count,
				  router_p, router_count,
				  &info, signature);
	my_CFRelease(&signature);
	my_CFRelease(&info.ipv4_dict);
	if (rtadv->ra != NULL) {
	    rtadv_submit_awd_success_report(service_p);
	    if (rtadv->dhcp_client == NULL
		|| !DHCPv6ClientIsActive(rtadv->dhcp_client)) {
		ServiceSetBusy(service_p, FALSE);
	    }
	    else {
		my_log(LOG_NOTICE, "%s: DHCPv6 client still active",
		       if_name(if_p));
	    }
	}
	if (rtadv->renew) {
	    /* re-assign address to trigger DAD */
	    rtadv_trigger_dad(service_p, list, count);
	    rtadv->renew = FALSE;
	}
    }
    return;
}

STATIC void
rtadv_address_changed(ServiceRef service_p)
{
    inet6_addrlist_t	addrs;

    inet6_addrlist_copy(&addrs,
			if_link_index(service_interface(service_p)));
    rtadv_address_changed_common(service_p, &addrs);
    inet6_addrlist_free(&addrs);
}

STATIC void
rtadv_dhcp_callback(DHCPv6ClientRef client, void * callback_arg,
		    DHCPv6ClientNotificationType type)
{
    ServiceRef		service_p = (ServiceRef)callback_arg;

    switch (type) {
    case kDHCPv6ClientNotificationTypeStatusChanged:
	rtadv_address_changed(service_p);
	break;
    case kDHCPv6ClientNotificationTypeGenerateSymptom:
	ServiceGenerateFailureSymptom(service_p);
	break;
    default:
	break;
    }
    return;
}

STATIC void
rtadv_init(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->try = 0;
    rtadv_address_changed(service_p);
    ServiceSetBusy(service_p, TRUE);
    return;
}

PRIVATE_EXTERN ipconfig_status_t
rtadv_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    char		timer_name[32];

    switch (evid) {
    case IFEventID_start_e:
	if (if_flags(if_p) & IFF_LOOPBACK) {
	    status = ipconfig_status_invalid_operation_e;
	    break;
	}
	if (rtadv != NULL) {
	    my_log(LOG_INFO, "RTADV %s: re-entering start state",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	rtadv = malloc(sizeof(*rtadv));
	if (rtadv == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: malloc failed", if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    break;
	}
	bzero(rtadv, sizeof(*rtadv));
	ServiceSetPrivate(service_p, rtadv);
	snprintf(timer_name, sizeof(timer_name), "rtadv-%s",
		 if_name(if_p));
	rtadv->timer = timer_callout_init(timer_name);
	if (rtadv->timer == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: timer_callout_init failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	rtadv->sock = RTADVSocketCreate(if_p);
	if (rtadv->sock == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: RTADVSocketCreate failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	if (G_dhcpv6_enabled) {
	    rtadv->dhcp_client = DHCPv6ClientCreate(if_p);
	    DHCPv6ClientSetNotificationCallBack(rtadv->dhcp_client,
						rtadv_dhcp_callback,
						service_p);
	}
	rtadv_init(service_p);
	break;

    stop:
    case IFEventID_stop_e:
	if (rtadv == NULL) {
	    my_log(LOG_INFO, "RTADV %s: already stopped",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	my_log(LOG_INFO, "RTADV %s: stop", if_name(if_p));

	/* generate/submit AWD report */
	if (!ServiceIsPublished(service_p)) {
	    rtadv_submit_awd_report(service_p, FALSE);
	}

	/* close/release the RTADV socket */
	RTADVSocketRelease(&rtadv->sock);

	/* stop DHCPv6 client */
	DHCPv6ClientRelease(&rtadv->dhcp_client);

	/* this flushes the addresses */
	(void)inet6_rtadv_disable(if_name(if_p));

	/* remove any CLAT46 address */
	if (service_clat46_is_enabled) {
	    rtadv_remove_clat46_address(service_p);
	}

	/* clean-up resources */
	if (rtadv->timer) {
	    timer_callout_free(&rtadv->timer);
	}
	my_CFRelease(&rtadv->ra);
	inet6_flush_prefixes(if_name(if_p));
	inet6_flush_routes(if_name(if_p));
	ServiceSetPrivate(service_p, NULL);
	free(rtadv);
	break;

    case IFEventID_ipv6_address_changed_e:
	if (rtadv == NULL) {
	    my_log(LOG_INFO, "RTADV %s: private data is NULL",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientAddressChanged(rtadv->dhcp_client, event_data);
	}
	rtadv_address_changed_common(service_p, event_data);
	break;

    case IFEventID_wake_e:
    case IFEventID_renew_e:
    case IFEventID_link_status_changed_e: {
	link_status_t		link_status;

	if (rtadv == NULL) {
	    return (ipconfig_status_internal_error_e);
	}
	link_status = service_link_status(service_p);
	if (link_status.valid == FALSE
	    || link_status.active == TRUE) {
	    link_event_data_t	link_event = (link_event_data_t)event_data;
	    boolean_t		ssid_changed;

	    ssid_changed = (link_event->flags & kLinkFlagsSSIDChanged) != 0;
	    if (ssid_changed) {
		rtadv->restart_count = 0;
		rtadv_flush(service_p);
	    }
	    else if (evid != IFEventID_renew_e
		     && rtadv->try == 1
		     && rtadv->ra == NULL) {
		/* we're already on it */
		break;
	    }
	    if (evid == IFEventID_renew_e 
		&& if_ift_type(if_p) == IFT_CELLULAR) {
		rtadv->renew = TRUE;
	    }
	    if (evid != IFEventID_wake_e || ssid_changed) {
		rtadv_init(service_p);
	    }
	}
	else {
	    rtadv->try = 0;
	}
	break;
    }
    case IFEventID_link_timer_expired_e:
	rtadv_inactive(service_p);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientStop(rtadv->dhcp_client, FALSE);
	}
	break;

    case IFEventID_get_ipv6_info_e: {
	ipv6_info_t *		info_p = (ipv6_info_t *)event_data;

	if (rtadv->dhcp_client != NULL) {
	    (void)DHCPv6ClientGetInfo(rtadv->dhcp_client, info_p);
	}
	if (rtadv->ra != NULL) {
	    info_p->ra = rtadv->ra;
	}
	break;
    }
    case IFEventID_ipv6_router_expired_e:
	rtadv_router_expired(service_p,
			     (ipv6_router_prefix_counts_t *)event_data);
	break;

    case IFEventID_plat_discovery_complete_e: {
	boolean_t	success;

	if (!service_clat46_is_enabled(service_p)) {
	    break;
	}
	if (rtadv == NULL) {
	    my_log(LOG_INFO, "RTADV %s: private data is NULL",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	if (event_data != NULL) {
	    success = *((boolean_t *)event_data);
	}
	else {
	    success = FALSE;
	}
	if (success) {
	    rtadv_address_changed(service_p);
	}
	else {
	    /* generate failure metric */
	    my_log(LOG_NOTICE, "RTADV %s: PLAT discovery failed",
		   if_name(if_p));
	    rtadv_submit_awd_report(service_p, FALSE);
	}
	break;
    }
    default:
	break;
    } /* switch */

    return (status);
}
