/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_NET_ROUTE_H_
#define _FBSD_COMPAT_NET_ROUTE_H_


#include <posix/net/route.h>


/*
 * A route consists of a destination address, a reference
 * to a routing entry, and a reference to an llentry.
 * These are often held by protocols in their control
 * blocks, e.g. inpcb.
 */
struct route {
	struct	rtentry *ro_rt;
	struct	llentry *ro_lle;
	struct	sockaddr ro_dst;
};

#endif /* _FBSD_COMPAT_NET_ROUTE_H_ */
