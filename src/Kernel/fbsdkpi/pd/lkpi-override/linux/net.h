// SPDX-License-Identifier: MIT
// linux/net.h, only the ratelimit helper
#ifndef _LINUXKPI_LINUX_NET_H_
#define _LINUXKPI_LINUX_NET_H_

// kernel sockets are not offered here, only the ratelimit helper
#include <sys/types.h>
#include <sys/errno.h>

int linuxkpi_net_ratelimit(void);

static inline int
net_ratelimit(void)
{
	return (linuxkpi_net_ratelimit());
}

#endif
