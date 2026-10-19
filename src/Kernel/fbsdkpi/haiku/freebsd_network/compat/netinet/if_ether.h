/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_NETINET_IF_ETHER_H_
#define _FBSD_COMPAT_NETINET_IF_ETHER_H_


/* Haiku does it's own ARP management */

#define arp_ifinit(ifp, ifa)

#endif
