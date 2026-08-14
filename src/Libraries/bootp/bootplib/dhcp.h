/*
 * Copyright (c) 1999, 2020 Apple Inc. All rights reserved.
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
 * dhcp.h
 * - definitions for DHCP (as specified in RFC2132)
 */

#ifndef _S_DHCP_H
#define _S_DHCP_H

#include <stddef.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdint.h>

struct dhcp {
    u_char		dp_op;		/* packet opcode type */
    u_char		dp_htype;	/* hardware addr type */
    u_char		dp_hlen;	/* hardware addr length */
    u_char		dp_hops;	/* gateway hops */
    u_int32_t		dp_xid;		/* transaction ID */
    u_int16_t		dp_secs;	/* seconds since boot began */	
    u_int16_t		dp_flags;	/* flags */
    struct in_addr	dp_ciaddr;	/* client IP address */
    struct in_addr	dp_yiaddr;	/* 'your' IP address */
    struct in_addr	dp_siaddr;	/* server IP address */
    struct in_addr	dp_giaddr;	/* gateway IP address */
    u_char		dp_chaddr[16];	/* client hardware address */
    u_char		dp_sname[64];	/* server host name */
    u_char		dp_file[128];	/* boot file name */
    u_char		dp_options[0];	/* variable-length options field */
};

struct dhcp_packet {
    struct ip 		ip;
    struct udphdr 	udp;
    struct dhcp 	dhcp;
};

#define DHCP_PACKET_OPTIONS_MIN	312
#define DHCP_PACKET_MIN		(sizeof(struct dhcp) + DHCP_PACKET_OPTIONS_MIN)
#define DHCP_PACKET_OVERHEAD	(offsetof(struct dhcp_packet, dhcp))

/* dhcp message types */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

typedef enum {
    dhcp_msgtype_none_e		= 0,
    dhcp_msgtype_discover_e 	= DHCPDISCOVER,
    dhcp_msgtype_offer_e	= DHCPOFFER,
    dhcp_msgtype_request_e	= DHCPREQUEST,
    dhcp_msgtype_decline_e	= DHCPDECLINE,
    dhcp_msgtype_ack_e		= DHCPACK,
    dhcp_msgtype_nak_e		= DHCPNAK,
    dhcp_msgtype_release_e	= DHCPRELEASE,
    dhcp_msgtype_inform_e	= DHCPINFORM,
} dhcp_msgtype_t;

static __inline__ const char *
dhcp_msgtype_names(dhcp_msgtype_t type)
{
    const char * names[] = {
	"<none>",
	"DISCOVER",
	"OFFER",
	"REQUEST",
	"DECLINE",
	"ACK",
	"NAK",
	"RELEASE",
	"INFORM",
    };
    if (type >= dhcp_msgtype_none_e && type <= dhcp_msgtype_inform_e)
	return (names[type]);
    return ("<unknown>");
}

/* overloaded option values */
#define DHCP_OVERLOAD_FILE	1
#define DHCP_OVERLOAD_SNAME	2
#define DHCP_OVERLOAD_BOTH	3

typedef uint32_t 		dhcptag_t;
typedef uint32_t		dhcp_lease_time_t;

#define dhcp_time_hton		htonl
#define dhcp_time_ntoh		ntohl
#define dhcp_lease_hton		htonl
#define dhcp_lease_ntoh		ntohl	

#define DHCP_INFINITE_LEASE	((dhcp_lease_time_t)-1)

#define DHCP_FLAGS_BROADCAST	((u_short)0x8000)

typedef enum {
    dhcp_cstate_inactive_e 	= 0,
    dhcp_cstate_decline_e,
    dhcp_cstate_unbound_e,
    dhcp_cstate_init_e,
    dhcp_cstate_select_e,
    dhcp_cstate_bound_e,
    dhcp_cstate_init_reboot_e,
    dhcp_cstate_renew_e,
    dhcp_cstate_rebind_e,
} dhcp_cstate_t;

static __inline__ const char *
dhcp_cstate_str(dhcp_cstate_t state)
{
    static const char * list[] = {"INACTIVE",
				  "DECLINE",
				  "UNBOUND",
				  "INIT",
				  "SELECT",
				  "BOUND",
				  "INIT/REBOOT",
				  "RENEW",
				  "REBIND"};
    const int list_count = sizeof(list) / sizeof(list[0]);

    if (state >= 0 && state < list_count) {
	return list[state];
    }
    return ("<undefined>");
}

#endif /* _S_DHCP_H */
