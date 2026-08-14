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

/*
 * ifutil.h
 * - network interface utility routines
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#ifndef _S_IFUTIL_H
#define _S_IFUTIL_H

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <mach/boolean.h>
#include <CoreFoundation/CFString.h>

#define s6_addr16 __u6_addr.__u6_addr16

int
interface_set_mtu(const char * ifname, int mtu);

int
interface_up_down(const char * ifname, boolean_t up);

int
interface_get_eflags(int sockfd, const char * name, uint64_t * ret_eflags);

int
inet_dgram_socket(void);

int
inet_attach_interface(const char * ifname, boolean_t set_iff_up);

int
inet_detach_interface(const char * ifname);

int
inet_aifaddr(int s, const char * name, struct in_addr addr,
	     const struct in_addr * mask,
	     const struct in_addr * broadcast);
int
inet_difaddr(int s, const char * name, struct in_addr addr);

int
inet_set_autoaddr(const char * ifname, int val);

typedef struct {
    struct in6_addr		addr;
    int				prefix_length;
    int				addr_flags;	/* from SIOCGIFAFLAG_IN6 */
    u_int32_t			valid_lifetime;	/* from SIOCGALIFETIME_IN6 */
    u_int32_t			preferred_lifetime;
} inet6_addrinfo_t;

#define INET6_ADDRLIST_N_STATIC		5

typedef struct {
    inet6_addrinfo_t *		list;
    int				count;
    inet6_addrinfo_t		list_static[INET6_ADDRLIST_N_STATIC];
    inet6_addrinfo_t *		linklocal;
} inet6_addrlist_t;


int	inet6_dgram_socket();
int	inet6_attach_interface(const char * ifname, boolean_t set_iff_up);
int	inet6_detach_interface(const char * ifname);
boolean_t inet6_is_attached(const char * ifname);

void
in6_netaddr(struct in6_addr * addr, int len);

int
inet6_get_prefix_length(const struct in6_addr * addr, int if_index);

int
inet6_difaddr(int s, const char * name, const struct in6_addr * addr);

int
inet6_aifaddr(int s, const char * name,
	      const struct in6_addr * addr,
	      const struct in6_addr * dstaddr,
	      int prefix_length,
	      int flags,
	      u_int32_t valid_lifetime, 
	      u_int32_t preferred_lifetime);

int	inet6_rtadv_enable(const char * if_name, boolean_t use_cga);
int	inet6_rtadv_disable(const char * if_name);

int	inet6_linklocal_start(const char * ifname,
			      const struct in6_addr * v6_ll,
			      boolean_t perform_nud,
			      boolean_t use_cga,
			      boolean_t enable_dad,
			      uint8_t collision_count);

int	inet6_linklocal_stop(const char * ifname);

int	inet6_flush_prefixes(const char * ifname);
int	inet6_flush_routes(const char * ifname);

boolean_t
inet6_forwarding_is_enabled(void);

int
inet6_ifstat(const char * if_name, struct in6_ifstat * stat);

boolean_t
inet6_get_linklocal_address(int if_index, struct in6_addr * ret_addr);

int
inet6_router_and_prefix_count(int if_index, int * ret_prefix_count);

boolean_t
inet6_has_nat64_prefixlist(const char * if_name);

int
inet6_clat46_start(const char * if_name);

int
inet6_clat46_stop(const char * if_name);

void
inet6_addrlist_init(inet6_addrlist_t * addr_list_p);

void
inet6_addrlist_free(inet6_addrlist_t * addr_list_p);

void
inet6_addrlist_copy(inet6_addrlist_t * addr_list_p, int if_index);

void
inet6_addrlist_print(const inet6_addrlist_t * addr_list_p);

CFStringRef
inet6_addrlist_copy_description(const inet6_addrlist_t * addr_list_p);

boolean_t
inet6_addrlist_contains_address(const inet6_addrlist_t * addr_list_p,
				const inet6_addrinfo_t * addr);

boolean_t
inet6_addrlist_in6_addr_is_ready(const inet6_addrlist_t * addr_list_p,
				 const struct in6_addr * addr);

inet6_addrinfo_t *
inet6_addrlist_get_linklocal(const inet6_addrlist_t * addr_list_p);

#endif /* _S_IFUTIL_H */
