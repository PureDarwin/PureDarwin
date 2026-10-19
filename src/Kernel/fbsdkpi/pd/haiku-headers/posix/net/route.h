/*
 * Copyright 2006-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NET_ROUTE_H
#define _NET_ROUTE_H


#include <sys/socket.h>


#define RTF_UP			0x00000001
#define RTF_GATEWAY		0x00000002
#define RTF_HOST		0x00000004
#define RTF_REJECT		0x00000008
#define RTF_DYNAMIC		0x00000010
#define RTF_MODIFIED	0x00000020
#define RTF_DEFAULT		0x00000080
#define RTF_STATIC		0x00000800
#define RTF_BLACKHOLE	0x00001000
#define RTF_LOCAL		0x00200000

/* This structure is used to pass routes to and from the network stack
 * (via struct ifreq) */

struct route_entry {
	struct sockaddr	*destination;
	struct sockaddr	*mask;
	struct sockaddr	*gateway;
	struct sockaddr	*source;
	uint32_t		flags;
	uint32_t		mtu;
};

#endif	/* _NET_ROUTE_H */
