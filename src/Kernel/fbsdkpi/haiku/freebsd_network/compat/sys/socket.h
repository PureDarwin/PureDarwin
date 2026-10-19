/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SOCKET_H_
#define _FBSD_COMPAT_SYS_SOCKET_H_


#include <posix/sys/socket.h>

#include <sys/cdefs.h>
#include <sys/_types.h>


// TODO Should be incorporated into Haikus socket.h with a number below AF_MAX
#define AF_IEEE80211 	(AF_MAX + 1) 	/* IEEE 802.11 protocol */

#endif /* _FBSD_COMPAT_SYS_SOCKET_H_ */
