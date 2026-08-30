/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifdef XNU_KERNEL_PRIVATE

#ifndef _KERN_UIPC_SOCKET_H
#define _KERN_UIPC_SOCKET_H

#include <kern/mem_acct.h>

#include <sys/socketvar.h>

extern struct mem_acct *socket_memacct;

static inline void
socket_memacct_add(unsigned int size)
{
	mem_acct_add(socket_memacct, size);
}

static inline void
socket_memacct_sub(unsigned int size)
{
	mem_acct_sub(socket_memacct, size);
}

static inline bool
socket_memacct_hardlimit()
{
	return mem_acct_limited(socket_memacct) == MEMACCT_HARDLIMIT;
}

static inline bool
socket_memacct_limited()
{
	return mem_acct_limited(socket_memacct) != 0;
}

struct sock_cm_info {
	int sotc;
	int netsvctype;
	uint64_t tx_time;
};

static inline void
sock_init_cm_info(struct sock_cm_info *sockcminfo, const struct socket *so)
{
	sockcminfo->sotc = so->so_traffic_class;
	sockcminfo->netsvctype = so->so_netsvctype;
	sockcminfo->tx_time = 0;
}

extern void sock_parse_cm_info(struct mbuf *control, struct sock_cm_info *sockcminfo);

#endif /*_KERN_UIPC_SOCKET_H */

#endif /* XNU_KERNEL_PRIVATE */
