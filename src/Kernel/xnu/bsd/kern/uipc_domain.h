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

#ifndef _KERN_UIPC_DOMAIN_H
#define _KERN_UIPC_DOMAIN_H

#ifdef XNU_KERNEL_PRIVATE


#include <kern/mem_acct.h>
#include <kern/uipc_socket.h>

#include <sys/protosw.h>

static inline void
proto_memacct_add(struct protosw *proto, unsigned int size)
{
	if (proto->pr_mem_acct) {
		mem_acct_add(proto->pr_mem_acct, size);
	} else {
		socket_memacct_add(size);
	}
}

static inline void
proto_memacct_sub(struct protosw *proto, unsigned int size)
{
	if (proto->pr_mem_acct) {
		mem_acct_sub(proto->pr_mem_acct, size);
	} else {
		socket_memacct_sub(size);
	}
}

static inline bool
proto_memacct_hardlimit(const struct protosw *proto)
{
	if (proto->pr_mem_acct) {
		return mem_acct_limited(proto->pr_mem_acct) == MEMACCT_HARDLIMIT;
	} else {
		return socket_memacct_hardlimit();
	}
}

static inline bool
proto_memacct_limited(const struct protosw *proto)
{
	if (proto->pr_mem_acct) {
		return mem_acct_limited(proto->pr_mem_acct) != 0;
	} else {
		return socket_memacct_limited();
	}
}

extern uint64_t _net_uptime;
extern uint64_t _net_uptime_ms;
extern uint64_t _net_uptime_us;

extern void net_update_uptime(void);
extern void net_update_uptime_with_time(const struct timeval *);

/*
 * ToDo - we could even replace all callers of net_uptime* by a direct access
 * to _net_uptime*
 */
static inline uint64_t
net_uptime(void)
{
	return _net_uptime;
}
static inline uint64_t
net_uptime_ms(void)
{
	return _net_uptime_ms;
}
static inline uint64_t
net_uptime_us(void)
{
	return _net_uptime_us;
}

extern void net_uptime2timeval(struct timeval *);

#endif /* XNU_KERNEL_PRIVATE */

#endif /*_KERN_UIPC_DOMAIN_H */
