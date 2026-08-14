/*
 * Copyright (c) 2003-2016 Apple Inc. All rights reserved.
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
 * stf.c
 * - Six to Four (6to4) configuration
 * - monitors the Primary IPv4 address, and it it's globally routable,
 *   maps it to the 6to4 address and assigns it to the stf0 interface
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

typedef struct {
    struct in_addr		local_ip;	/* IPv4 address we're mapping */
    struct in6_addr		relay;		/* relay we're using */
    char *			relay_hostname; /* DNS hostname of relay */
    SCNetworkReachabilityRef	reach;		/* resolves DNS name */
    CFStringRef			signature;	/* signature of IPv4 service */
    SCDynamicStoreRef		store;		/* notify on primary changes */
    CFRunLoopSourceRef		store_rls;	
} Service_stf_t;


#define STF_PREFIX_LENGTH	16

static const struct in6_addr stf_anycast_relay = { /* RFC3068 */
    { 
	{ 0x20, 0x02,			/* 2002/16 prefix = 6to4 */
	  0xc0, 0x58, 0x63, 0x01,	/* 192.88.99.1 */
	  0x00, 0x00,			/* 16-bit subnet */
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /* 64-bit host */
	}
    }
};

typedef struct {
    uint8_t		prefix[2];	/* 16 bits = { 0x20, 0x02 } */
    uint8_t		ipv4_address[4];/* 32 bits */
    uint8_t		net[2];		/* 16 bits */
    uint8_t		host[8];	/* 64 bits */
} in6_6to4_addr_t;

static void
make_6to4_addr(struct in_addr ip_addr, struct in6_addr * ip6_addr,
	       boolean_t is_host)
{
    in6_6to4_addr_t *	addr = (in6_6to4_addr_t *)ip6_addr;

    bzero(ip6_addr, sizeof(*ip6_addr));
    addr->prefix[0] = 0x20;
    addr->prefix[1] = 0x02;
    bcopy((void *)&ip_addr.s_addr, (void *)&addr->ipv4_address,
	  sizeof(ip_addr.s_addr));
    if (is_host) {
	/* for host address, set the network and host values to 1 */
	addr->net[1] = 0x01;	/* net = 0x0001 */
	addr->host[7] = 0x01;	/* host = 0x0000000000000001 */
    }
    return;
}

static void
stf_publish(ServiceRef service_p)
{
    inet6_addrinfo_t	info;
    CFStringRef		our_signature = NULL;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    if (IN6_IS_ADDR_UNSPECIFIED(&stf->relay)
	|| stf->local_ip.s_addr == 0) {
	/* don't have both the Relay and the IPv4 address */
	return;
    }
    if (stf->signature != NULL) {
	our_signature = CFStringCreateWithFormat(NULL, NULL,
						 CFSTR("IPv6.6to4=(%@)"),
						 stf->signature);
    }
    make_6to4_addr(stf->local_ip, &info.addr, TRUE);
    info.addr_flags = 0;
    info.prefix_length = STF_PREFIX_LENGTH;
    ServicePublishSuccessIPv6(service_p, &info, 1, &stf->relay, 1, NULL,
			      our_signature);
    my_CFRelease(&our_signature);
    return;
}

static void
stf_reachability_callback(SCNetworkReachabilityRef target,
			  SCNetworkReachabilityFlags flags,
			  void * info)
{
    CFIndex		count;
    int			error;
    int			i;
    interface_t *	if_p;
    ServiceRef		service_p = (ServiceRef)info;
    CFArrayRef		relay_addrs = NULL;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);
    
    if_p = service_interface(service_p);
    if ((flags & kSCNetworkReachabilityFlagsReachable) == 0
	|| (flags & kSCNetworkReachabilityFlagsConnectionRequired) != 0) {
	/* relay is not yet reachable */
	my_log(LOG_NOTICE, "6TO4 %s: can't resolve %s",
	       if_name(if_p), stf->relay_hostname);
	return;
    }
    relay_addrs = SCNetworkReachabilityCopyResolvedAddress(stf->reach, &error);
    if (relay_addrs == NULL) {
	return;
    }
    count = CFArrayGetCount(relay_addrs);
    for (i = 0; i < count; i++) {
	CFDataRef		data = CFArrayGetValueAtIndex(relay_addrs, i);
	struct sockaddr_in *	sin;
	
	/* ALIGN: CFDataGetBytePtr returns alignment to at 
	 * least sizeof(uint64) bytes */	
	sin = (struct sockaddr_in *)(void *)CFDataGetBytePtr(data);
	if (sin->sin_family == AF_INET && sin->sin_addr.s_addr != 0) {
	    struct in6_addr	relay;

	    my_log(LOG_INFO, "6TO4 %s: resolved %s to " IP_FORMAT,
		   if_name(if_p), stf->relay_hostname, 
		   IP_LIST(&sin->sin_addr));

	    /* don't need the reachability any longer */
	    SCNetworkReachabilityUnscheduleFromRunLoop(stf->reach,
						       CFRunLoopGetCurrent(),
						       kCFRunLoopDefaultMode);
	    my_CFRelease(&stf->reach);
	    make_6to4_addr(sin->sin_addr, &relay, FALSE);
	    if (IN6_ARE_ADDR_EQUAL(&stf->relay, &relay) == FALSE) {
		stf->relay = relay;
		stf_publish(service_p);
	    }
	    break;
	}
    }
    CFRelease(relay_addrs);
    return;
}

static void
stf_set_relay_hostname(ServiceRef service_p, const char * relay_hostname)
{
    SCNetworkReachabilityContext context = { 0, NULL, NULL, NULL, NULL };
    interface_t *	if_p = service_interface(service_p);
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    if (stf->relay_hostname != NULL) {
	free(stf->relay_hostname);
	stf->relay_hostname = NULL;
    }
    if (stf->reach != NULL) {
	SCNetworkReachabilityUnscheduleFromRunLoop(stf->reach,
						   CFRunLoopGetCurrent(),
						   kCFRunLoopDefaultMode);
	my_CFRelease(&stf->reach);
    }
    if (relay_hostname == NULL) {
	/* no more relay hostname to resolve */
	return;
    }
    stf->reach = SCNetworkReachabilityCreateWithName(NULL, relay_hostname);
    if (stf->reach == NULL) {
	my_log(LOG_NOTICE,
	       "6TO4 %s:SCNetworkReachabilityCreateWithName failed, %s",
	       if_name(if_p),
	       SCErrorString(SCError()));
	return;
    }
    context.info = service_p;
    if (SCNetworkReachabilitySetCallback(stf->reach,
					 stf_reachability_callback,
					 &context) == FALSE) {
	my_log(LOG_NOTICE,
	       "6TO4 %s: SCNetworkReachabilitySetCallback failed, %s",
	       if_name(if_p),
	       SCErrorString(SCError()));
	my_CFRelease(&stf->reach);
	
    }
    SCNetworkReachabilityScheduleWithRunLoop(stf->reach,
					     CFRunLoopGetCurrent(),
					     kCFRunLoopDefaultMode);
    my_log(LOG_INFO, "6TO4 %s: resolving %s", if_name(if_p),
	   relay_hostname);
    stf->relay_hostname = strdup(relay_hostname);
    return;
}

static CFStringRef
copy_state_global_ipv4_key(void)
{
    return (SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						       kSCDynamicStoreDomainState,
						       kSCEntNetIPv4));
}

static struct in_addr
copy_primary_routable_ipv4_info(CFDictionaryRef info,
				CFStringRef ipv4_global_key,
				CFStringRef * ret_signature)
{
    CFArrayRef		addresses;
    CFDictionaryRef	dict = NULL;
    CFDictionaryRef 	ipv4_global_dict = NULL;
    struct in_addr	ret_ip;
    CFStringRef		serviceID = NULL;
    CFStringRef		signature = NULL;

    if (info != NULL) {
	ipv4_global_dict = CFDictionaryGetValue(info, ipv4_global_key);
	ipv4_global_dict = isA_CFDictionary(ipv4_global_dict);
    }

    ret_ip.s_addr = 0;
    if (ipv4_global_dict != NULL) {
	serviceID = CFDictionaryGetValue(ipv4_global_dict,
					 kSCDynamicStorePropNetPrimaryService);
    }
    if (isA_CFString(serviceID) == NULL) {
	goto done;
    }
    if (info != NULL) {
	CFStringRef		key;

	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID,
							  kSCEntNetIPv4);
	dict = CFDictionaryGetValue(info, key);
	dict = isA_CFDictionary(dict);
	CFRelease(key);
    }
    if (dict == NULL) {
	goto done;
    }
    signature = CFDictionaryGetValue(dict, kNetworkSignature);
    signature = isA_CFString(signature);
    addresses = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
    if (isA_CFArray(addresses) != NULL && CFArrayGetCount(addresses) > 0) {
	CFStringRef	addr = CFArrayGetValueAtIndex(addresses, 0);
	struct in_addr	ip;

	if (my_CFStringToIPAddress(addr, &ip) 
	    && ip.s_addr != 0
	    && ip_is_linklocal(ip) == FALSE
	    && ip_is_private(ip) == FALSE) {
	    if (ret_signature != NULL && signature != NULL) {
		*ret_signature = CFRetain(signature);
	    }
	    ret_ip = ip;
	}
    }

 done:
    return (ret_ip);
}

static void
stf_remove_all_addresses(ServiceRef service_p)
{
    int				i;
    interface_t *		if_p = service_interface(service_p);
    inet6_addrlist_t	 	list;
    int				s;

    inet6_addrlist_copy(&list, if_link_index(if_p));
    if (list.count == 0) {
	return;
    }
    s = inet6_dgram_socket();
    if (s < 0) {
	goto done;
    }
    for (i = 0; i < list.count; i++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	my_log(LOG_DEBUG, "6TO4 %s: removing %s/%d",
	       if_name(if_p),
	       inet_ntop(AF_INET6, &list.list[i].addr,
			 ntopbuf, sizeof(ntopbuf)),
	       list.list[i].prefix_length);
	inet6_difaddr(s, if_name(if_p), &list.list[i].addr);
    }
    close(s);
 done:
    inet6_addrlist_free(&list);
    return;
}

static void
stf_update_address(ServiceRef service_p, CFDictionaryRef info,
		   CFStringRef ipv4_global_key)
{
    struct in_addr	local_ip;
    CFStringRef		signature = NULL;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    local_ip = copy_primary_routable_ipv4_info(info,
					       ipv4_global_key,
					       &signature);

    /* if there is no primary IPv4 address, or it has changed since last time */
    if (local_ip.s_addr == 0
	|| local_ip.s_addr != stf->local_ip.s_addr) {
	interface_t *	if_p;
	struct in6_addr	local_ip6;


	if_p = service_interface(service_p);
	if (local_ip.s_addr == 0) {
	    my_log(LOG_INFO, "6TO4 %s: no primary IPv4 address",
		   if_name(if_p));
	}
	else {
	    my_log(LOG_INFO,
		   "6TO4 %s: primary IPv4 address changed to " IP_FORMAT,
		   if_name(if_p), IP_LIST(&local_ip));
	}

	/* clear the old address */
	if (stf->local_ip.s_addr != 0) {
	    /* remove the address */
	    make_6to4_addr(stf->local_ip, &local_ip6, TRUE);
	    ServiceRemoveIPv6Address(service_p, &local_ip6, STF_PREFIX_LENGTH);
	}
	
	/* clear the old signature */
	my_CFRelease(&stf->signature);

	stf->local_ip = local_ip;
	if (local_ip.s_addr != 0) {
	    /* set the new address and publish */
	    make_6to4_addr(local_ip, &local_ip6, TRUE);
	    ServiceSetIPv6Address(service_p, &local_ip6, STF_PREFIX_LENGTH, 0,
				  ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME);
	    if (isA_CFString(signature) != NULL) {
		stf->signature = CFRetain(signature);
	    }
	    stf_publish(service_p);
	}
	else {
	    /* unpublish */
	    service_publish_failure(service_p,
				    ipconfig_status_resource_unavailable_e);
	}
    }
    my_CFRelease(&signature);
    return;
}

static CFDictionaryRef
copy_service_information(SCDynamicStoreRef store,
			 CFStringRef global_ipv4_key)
{
    CFStringRef		all_ipv4_services_pattern;
    CFArrayRef		keys;
    CFArrayRef		patterns;
    CFDictionaryRef	info;

    keys = CFArrayCreate(NULL, 
			 (const void **)&global_ipv4_key, 1,
			 &kCFTypeArrayCallBacks);
    all_ipv4_services_pattern
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainState,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);
    patterns = CFArrayCreate(NULL,
			     (const void **)&all_ipv4_services_pattern, 1,
			     &kCFTypeArrayCallBacks);
    CFRelease(all_ipv4_services_pattern);
    info = SCDynamicStoreCopyMultiple(store, keys, patterns);
    CFRelease(keys);
    CFRelease(patterns);

    return (info);
}

static void
stf_global_ipv4_changed(SCDynamicStoreRef store,
			CFArrayRef changes,
			void * info)
{
    CFDictionaryRef	dict;
    CFStringRef		key;

    if (changes == NULL || CFArrayGetCount(changes) == 0) {
	return;
    }
    key = CFArrayGetValueAtIndex(changes, 0);
    dict = copy_service_information(store, key);
    stf_update_address((ServiceRef)info, dict, key);
    my_CFRelease(&dict);
    return;
}


/*
 * Function: stf_configure_address
 * Purpose:
 *   Called once to install the notification source and to configure the
 *   address of the stf0 interface.
 */
static void
stf_configure_address(ServiceRef service_p)
{
    CFArrayRef		array;
    CFDictionaryRef	dict;
    SCDynamicStoreContext context = { 0, NULL, NULL, NULL, NULL };
    CFStringRef		key;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    /* create the notification source */
    context.info = service_p;
    stf->store = SCDynamicStoreCreate(NULL, 
				      CFSTR("IPConfiguration:STF"),
				      stf_global_ipv4_changed, &context);
    key = copy_state_global_ipv4_key();
    array = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
    SCDynamicStoreSetNotificationKeys(stf->store, array, NULL);
    CFRelease(array);
    stf->store_rls = SCDynamicStoreCreateRunLoopSource(NULL, stf->store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), stf->store_rls,
		       kCFRunLoopDefaultMode);

    /* grab the current value */
    dict = copy_service_information(stf->store, key);
    stf_update_address(service_p, dict, key);
    CFRelease(key);
    my_CFRelease(&dict);
    return;
}

static void
stf_set_relay(ServiceRef service_p, ipconfig_method_data_stf_t method_data)
{
    interface_t *	if_p;
    Boolean		publish_new = FALSE;
    address_type_t	relay_type = address_type_none_e;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    if_p = service_interface(service_p);
    if (method_data != NULL) {
	relay_type = method_data->relay_addr_type;
    }
    switch (relay_type) {
    case address_type_none_e:
	stf_set_relay_hostname(service_p, NULL);
	if (IN6_ARE_ADDR_EQUAL(&stf->relay, &stf_anycast_relay)) {
	    /* we're currently using the anycast relay, nothing to do */
	    return;
	}
	my_log(LOG_INFO, "6TO4 %s: using default anycast relay",
	       if_name(if_p));
	stf->relay = stf_anycast_relay;
	publish_new = TRUE;
	break;

    case address_type_dns_e:
	if (stf->relay_hostname != NULL
	    && strcmp(stf->relay_hostname,
		      method_data->relay_addr.dns) == 0) {
	    /* the same DNS server address, nothing to do */
	    return;
	}
	my_log(LOG_INFO, "6TO4 %s: specified DNS relay %s",
	       if_name(if_p), method_data->relay_addr.dns);
	stf_set_relay_hostname(service_p, method_data->relay_addr.dns);
	break;

    case address_type_ipv4_e: {
	struct in6_addr	requested_ip;

	make_6to4_addr(method_data->relay_addr.v4, &requested_ip, FALSE);
	stf_set_relay_hostname(service_p, NULL);
	if (IN6_ARE_ADDR_EQUAL(&requested_ip, &stf->relay)) {
	    /* new relay same as old, nothing to do */
	    return;
	}
	my_log(LOG_INFO, "6TO4 %s: specified IPv4 relay " IP_FORMAT,
	       if_name(if_p), IP_LIST(&method_data->relay_addr.v4));
	stf->relay = requested_ip;
	publish_new = TRUE;
	break;
    }
    case address_type_ipv6_e:
	stf_set_relay_hostname(service_p, NULL);
	if (IN6_ARE_ADDR_EQUAL(&method_data->relay_addr.v6, &stf->relay)) {
	    /* new relay same as old */
	    return;
	}
	{
	    char 	ntopbuf[INET6_ADDRSTRLEN];

	    my_log(LOG_INFO, "6TO4 %s: specified IPv6 relay %s",
		   if_name(if_p), 
		   inet_ntop(AF_INET6, &method_data->relay_addr.v6,
			     ntopbuf, sizeof(ntopbuf)));
	}
	stf->relay = method_data->relay_addr.v6;
	publish_new = TRUE;
	break;
    default:
	my_log(LOG_NOTICE, "6TO4 %s: specified unknown relay type %d",
	       if_name(if_p), relay_type);
	return;
    }
    if (publish_new) {
	stf_publish(service_p);
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
stf_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    ipconfig_method_data_t method_data;
    Service_stf_t *	stf = (Service_stf_t *)ServiceGetPrivate(service_p);

    switch (evid) {
    case IFEventID_start_e:
	if (if_flags(if_p) & IFF_LOOPBACK) {
	    status = ipconfig_status_invalid_operation_e;
	    break;
	}
	if (stf != NULL) {
	    my_log(LOG_DEBUG, "6TO4 %s: re-entering start state",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	stf = malloc(sizeof(*stf));
	if (stf == NULL) {
	    my_log(LOG_NOTICE, "6TO4 %s: malloc failed", if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    break;
	}
	bzero(stf, sizeof(*stf));
	ServiceSetPrivate(service_p, stf);
	/* scrub all IP addresses - in case we crashed */
	stf_remove_all_addresses(service_p);
	stf_configure_address(service_p);
	method_data = (ipconfig_method_data_t)event_data;
	stf_set_relay(service_p, &method_data->stf);
	break;
    case IFEventID_change_e: {
	change_event_data_t * change_event;

	change_event = ((change_event_data_t *)event_data);
	stf_set_relay(service_p, &change_event->method_data->stf);
	break;
    }
    case IFEventID_stop_e: {
	struct in6_addr		local_ip6;

	if (stf == NULL) {
	    my_log(LOG_DEBUG, "6TO4 %s: already stopped",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	my_log(LOG_DEBUG, "6TO4 %s: stop", if_name(if_p));
	stf_set_relay_hostname(service_p, NULL);
	if (stf->store_rls != NULL) {
	    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), stf->store_rls,
				  kCFRunLoopDefaultMode);
	    my_CFRelease(&stf->store_rls);
	}
	if (stf->local_ip.s_addr != 0) {
	    /* remove the address */
	    make_6to4_addr(stf->local_ip, &local_ip6, TRUE);
	    ServiceRemoveIPv6Address(service_p, &local_ip6, STF_PREFIX_LENGTH);
	}
	my_CFRelease(&stf->store);
	my_CFRelease(&stf->signature);
	ServiceSetPrivate(service_p, NULL);
	free(stf);
	break;
    }
    default:
	break;
    } /* switch */

    return (status);
}
