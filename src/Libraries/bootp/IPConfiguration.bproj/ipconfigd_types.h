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

#ifndef _S_IPCONFIGD_TYPES_H
#define _S_IPCONFIGD_TYPES_H

#include <mach/boolean.h>
#include <netinet/in.h>
#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include "timer.h"
#include "dhcp_options.h"
#include "symbol_scope.h"
#include "RouterAdvertisement.h"

#define IPV4_METHOD_BIT		0x100
#define IPV6_METHOD_BIT		0x200

typedef enum {
    ipconfig_method_none_e 	= 0x0,

    /* IPv4 */
    ipconfig_method_none_v4_e	= 0x100,
    ipconfig_method_manual_e 	= 0x101,
    ipconfig_method_bootp_e 	= 0x102,
    ipconfig_method_dhcp_e 	= 0x103,
    ipconfig_method_inform_e 	= 0x104,
    ipconfig_method_linklocal_e = 0x105,
    ipconfig_method_failover_e 	= 0x106,

    /* IPv6 */
    ipconfig_method_none_v6_e	= 0x200,
    ipconfig_method_manual_v6_e	= 0x201,
    ipconfig_method_automatic_v6_e = 0x202,
    ipconfig_method_rtadv_e 	= 0x203,
    ipconfig_method_stf_e 	= 0x204,
    ipconfig_method_linklocal_v6_e = 0x205,
} ipconfig_method_t;

const char *
ipconfig_method_string(ipconfig_method_t m);

INLINE boolean_t
ipconfig_method_is_dhcp_or_bootp(ipconfig_method_t method)
{
    if (method == ipconfig_method_dhcp_e
	|| method == ipconfig_method_bootp_e) {
	return (TRUE);
    }
    return (FALSE);
}

INLINE boolean_t
ipconfig_method_is_manual(ipconfig_method_t method)
{
    if (method == ipconfig_method_manual_e
	|| method == ipconfig_method_failover_e
	|| method == ipconfig_method_inform_e) {
	return (TRUE);
    }
    return (FALSE);
}

INLINE boolean_t
ipconfig_method_is_v4(ipconfig_method_t method)
{
    return ((method & IPV4_METHOD_BIT) != 0);
}

INLINE boolean_t
ipconfig_method_is_v6(ipconfig_method_t method)
{
    return ((method & IPV6_METHOD_BIT) != 0);
}

typedef struct {
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	router;
    boolean_t		ignore_link_status;
    int32_t		failover_timeout;
} ipconfig_method_data_manual, *ipconfig_method_data_manual_t;

typedef struct {
    int			client_id_len;
    char *		client_id;	/* malloc'd */
} ipconfig_method_data_dhcp, *ipconfig_method_data_dhcp_t;

#define LINKLOCAL_ALLOCATE	TRUE
#define LINKLOCAL_NO_ALLOCATE	FALSE
typedef struct {
    boolean_t	allocate;
} ipconfig_method_data_linklocal, *ipconfig_method_data_linklocal_t;

typedef struct {
    struct in6_addr	addr;
    int			prefix_length;
} ipconfig_method_data_manual_v6, *ipconfig_method_data_manual_v6_t;

typedef enum {
    address_type_none_e,
    address_type_ipv4_e,
    address_type_ipv6_e,
    address_type_dns_e
} address_type_t;

typedef struct {
    address_type_t	relay_addr_type;
    union {
	struct in_addr	v4;
	struct in6_addr	v6;
	char * 		dns;	/* malloc'd */
    } relay_addr;
} ipconfig_method_data_stf, *ipconfig_method_data_stf_t;

/*
 * Type: ipconfig_method_data
 * Purpose:
 *   Used internally by IPConfiguration to communicate the config to
 *   the various methods.
 */
typedef union {
    ipconfig_method_data_manual		manual;
    ipconfig_method_data_dhcp		dhcp;
    ipconfig_method_data_linklocal	linklocal;
    ipconfig_method_data_manual_v6	manual_v6;
    ipconfig_method_data_stf		stf;
} ipconfig_method_data, * ipconfig_method_data_t;

typedef struct {
    ipconfig_method_t			method;
    ipconfig_method_data		method_data;
    boolean_t				disable_cga;
    struct in6_addr			ipv6_linklocal;
} ipconfig_method_info, * ipconfig_method_info_t;

INLINE void
ipconfig_method_info_init(ipconfig_method_info_t info)
{
    bzero(info, sizeof(*info));
}

INLINE void
ipconfig_method_info_free(ipconfig_method_info_t info)
{
    switch (info->method) {
    case ipconfig_method_dhcp_e:
	if (info->method_data.dhcp.client_id != NULL) {
	    free(info->method_data.dhcp.client_id);
	    info->method_data.dhcp.client_id = NULL;
	}
	break;
    case ipconfig_method_stf_e:
	if (info->method_data.stf.relay_addr_type == address_type_dns_e
	    && info->method_data.stf.relay_addr.dns != NULL) {
	    free(info->method_data.stf.relay_addr.dns);
	    info->method_data.stf.relay_addr.dns = NULL;
	}
	break;
    default:
	break;
    }
}

/*
 * Types to hold DHCP/auto-configuration information
 */
typedef struct {
    const uint8_t *		pkt;
    int				pkt_size;
    dhcpol_t *			options;
    absolute_time_t		lease_start;
    absolute_time_t		lease_expiration;
} dhcp_info_t;

typedef struct {
    DHCPv6PacketRef		pkt;
    int				pkt_len;
    DHCPv6OptionListRef		options;
    RouterAdvertisementRef	ra;
    boolean_t			perform_plat_discovery;
    CFDictionaryRef		ipv4_dict;
} ipv6_info_t;

typedef struct {
    boolean_t			requested;
    boolean_t			supported;
} active_during_sleep_t;

typedef struct {
    int				prefix_count;
    int				router_count;
} ipv6_router_prefix_counts_t;
#endif /* _S_IPCONFIGD_TYPES_H */
