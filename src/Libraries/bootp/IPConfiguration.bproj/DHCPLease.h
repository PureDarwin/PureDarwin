/*
 * Copyright (c) 2000-2015 Apple Inc. All rights reserved.
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
 * DHCPLease.h
 * - in-memory and persistent DHCP lease support
 */

#ifndef _S_DHCPLEASE_H
#define _S_DHCPLEASE_H

/* 
 * Modification History
 *
 * June 11, 2009		Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "dhcplib.h"
#include "timer.h"
#include "interfaces.h"
#include "arp_session.h"

#define DHCP_LEASE_NOT_FOUND	(-1)

/**
 ** DHCPLease, DHCPLeaseList
 **/
typedef struct {
    bool			tentative;
    bool			nak;
    struct in_addr		our_ip;
    absolute_time_t		lease_start;
    dhcp_lease_time_t		lease_length;
    struct in_addr		router_ip;
    uint8_t			router_hwaddr[MAX_LINK_ADDR_LEN];
    int				router_hwaddr_length;
    CFStringRef			ssid;
    int				pkt_length;
    uint8_t			pkt[1];
} DHCPLease, * DHCPLeaseRef;

typedef dynarray_t DHCPLeaseList, * DHCPLeaseListRef;

void
DHCPLeaseSetNAK(DHCPLeaseRef lease_p, int nak);


void
DHCPLeaseListInit(DHCPLeaseListRef list_p);

void
DHCPLeaseListFree(DHCPLeaseListRef list_p);

void
DHCPLeaseListClear(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length);
void
DHCPLeaseListRemoveLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length);
void
DHCPLeaseListUpdateLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length,
			 absolute_time_t lease_start,
			 dhcp_lease_time_t lease_length,
			 const uint8_t * pkt, int pkt_length,
			 CFStringRef ssid);
arp_address_info_t *
DHCPLeaseListCopyARPAddressInfo(DHCPLeaseListRef list_p,
				CFStringRef ssid,
				absolute_time_t * start_threshold_p,
				bool tentative_ok,
				int * ret_count);


void
DHCPLeaseListWrite(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length);
void
DHCPLeaseListRead(DHCPLeaseListRef list_p,
		  const char * ifname, bool is_wifi,
		  uint8_t cid_type, const void * cid, int cid_length);

int
DHCPLeaseListFindLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
		       struct in_addr router_ip,
		       const uint8_t * router_hwaddr, int router_hwaddr_length);

int
DHCPLeaseListFindLeaseWithSSID(DHCPLeaseListRef list_p, CFStringRef ssid);

void
DHCPLeaseListRemoveLeaseWithSSID(DHCPLeaseListRef list_p, CFStringRef ssid);

static __inline__ int
DHCPLeaseListCount(DHCPLeaseListRef list_p)
{
    return (dynarray_count(list_p));
}

static __inline__ DHCPLeaseRef
DHCPLeaseListElement(DHCPLeaseListRef list_p, int i)
{
    return (dynarray_element(list_p, i));
}

void
DHCPLeaseListRemoveAllButLastLease(DHCPLeaseListRef list_p);

#endif /* _S_DHCPLEASE_H */
