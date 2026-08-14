/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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

#ifndef _S_UTIL_H
#define _S_UTIL_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <mach/boolean.h>
#include <string.h>
#include <ctype.h>
#include <net/ethernet.h>
#include <sys/time.h>
#include <net/route.h>
#include <CoreFoundation/CFString.h>
#include "symbol_scope.h"

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip, i)	(((uint8_t *)(ip))[i])
#define IP_LIST(ip)	IP_CH(ip,0),IP_CH(ip,1),IP_CH(ip,2),IP_CH(ip,3)

#define EA_FORMAT	"%02x:%02x:%02x:%02x:%02x:%02x"
#define EA_CH(e, i)	((uint8_t)((uint8_t *)(e))[(i)])
#define EA_LIST(ea)	EA_CH(ea,0),EA_CH(ea,1),EA_CH(ea,2),EA_CH(ea,3),EA_CH(ea,4),EA_CH(ea,5)

#define FWA_FORMAT	"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define FWA_CH(e, i) 	((uint8_t)((uint8_t *)(e))[(i)])
#define FWA_LIST(ea) 	FWA_CH(ea,0),FWA_CH(ea,1),FWA_CH(ea,2),FWA_CH(ea,3),FWA_CH(ea,4),FWA_CH(ea,5),FWA_CH(ea,6),FWA_CH(ea,7)

INLINE in_addr_t 
iptohl(struct in_addr ip)
{
    return (ntohl(ip.s_addr));
}

INLINE struct in_addr 
hltoip(in_addr_t l)
{
    struct in_addr ip;

    ip.s_addr = htonl(l);
    return (ip);
}

INLINE boolean_t
in_subnet(struct in_addr netaddr, struct in_addr netmask, struct in_addr ip)
{
    return ((ip.s_addr & netmask.s_addr) == netaddr.s_addr);
}

/*
 * Taken from RFC 1918.  Private IP address ranges are:
 * 10.0.0.0        -   10.255.255.255  (10/8 prefix)
 * 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
 * 192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
 */

#define IN_PRIVATE_10		((u_int32_t)0x0a000000)
#define IN_PRIVATE_10_NET	((u_int32_t)IN_CLASSA_NET)

#define IN_PRIVATE_172_16	((u_int32_t)0xac100000)
#define IN_PRIVATE_172_16_NET	((u_int32_t)0xfff00000)

#define IN_PRIVATE_192_168	((u_int32_t)0xc0a80000)
#define IN_PRIVATE_192_168_NET	((u_int32_t)IN_CLASSB_NET)

INLINE boolean_t
ip_is_private(struct in_addr iaddr)
{
    u_int32_t	val = ntohl(iaddr.s_addr);

    if ((val & IN_PRIVATE_10_NET) == IN_PRIVATE_10
	|| (val & IN_PRIVATE_172_16_NET) == IN_PRIVATE_172_16
	|| (val & IN_PRIVATE_192_168_NET) == IN_PRIVATE_192_168) {
	return (TRUE);
    }
    return (FALSE);
}

INLINE boolean_t
ip_is_linklocal(struct in_addr iaddr)
{
    u_int32_t	val = ntohl(iaddr.s_addr);

    return (IN_LINKLOCAL(val));
}

INLINE uint32_t
prefix_to_mask32(unsigned int prefix_length)
{
    if (prefix_length > 32 || prefix_length == 0) {
	return (0);
    }
    return (0xffffffff << (32 - prefix_length));
}

/*
 * IPv4 Service Continuity Prefix (RFC 7335)
 * 192.0.0.0/29
 */
#define IN_SERVICE_CONTINUITY		((u_int32_t)0xc0000000)
#define IN_SERVICE_CONTINUITY_NET	((u_int32_t)0xfffffff8)


int	nbits_host(struct in_addr mask);

char *	inet_nettoa(struct in_addr addr, struct in_addr mask);

long	random_range(long bottom, long top);

#define USECS_PER_SEC	1000000
void	timeval_subtract(struct timeval tv1, struct timeval tv2, 
			 struct timeval * result);

void	timeval_add(struct timeval tv1, struct timeval tv2,
		    struct timeval * result);

int	timeval_compare(struct timeval tv1, struct timeval tv2);

void	print_data_cfstr(CFMutableStringRef str, const uint8_t * data_p,
			 int n_bytes);
void	print_data(const uint8_t * data_p, int n_bytes);
void	fprint_data(FILE * f, const uint8_t * data_p, int n_bytes);

void	print_bytes_sep_cfstr(CFMutableStringRef str, uint8_t * data, int len,
			      char separator);
void	print_bytes_cfstr(CFMutableStringRef str, uint8_t * data, int len);
void	print_bytes(uint8_t * data, int len);
void	print_bytes_sep(uint8_t * data, int len, char separator);
void	fprint_bytes(FILE * out_f, uint8_t * data_p, int n_bytes);
void	fprint_bytes_sep(FILE * out_f, uint8_t * data_p, int n_bytes,
			 char separator);

int	create_path(const char * dirname, mode_t mode);

char *  tagtext_get(char * data, char * data_end, char * tag, char * * end_p);

int	ether_cmp(struct ether_addr * e1, struct ether_addr * e2);

void
link_addr_to_string(char * string_buffer, int string_buffer_length,
		    const uint8_t * hwaddr, int hwaddr_len);

void
fill_with_random(void * buf, uint32_t len);

int
rt_xaddrs(const char * cp, const char * cplim, struct rt_addrinfo * rtinfo);

#ifndef countof
#define countof(__an_array)	(sizeof(__an_array) / sizeof((__an_array)[0]))
#endif

#endif /* _S_UTIL_H */
