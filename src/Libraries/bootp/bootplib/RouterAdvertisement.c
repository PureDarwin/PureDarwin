/*
 * Copyright (c) 2010-2020 Apple Inc. All rights reserved.
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
 * RouterAdvertisement.c
 * - CF object to encapulate an IPv6 ND Router Advertisement
 */

/*
 * Modification History
 *
 * April 15, 2020		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include "symbol_scope.h"
#include "RouterAdvertisement.h"
#include "DNSNameList.h"
#include "ptrlist.h"
#include "cfutil.h"
#include "util.h"

#define RA_OPT_INFINITE_LIFETIME		((uint32_t)0xffffffff)

#ifndef ND_OPT_CAPTIVE_PORTAL
#define ND_OPT_CAPTIVE_PORTAL 37
#endif // ND_OPT_CAPTIVE_PORTAL

struct __RouterAdvertisement {
	CFRuntimeBase	cf_base;

	/*
	 * NOTE: if you add a field, add a line to initialize it.
	 */
	struct in6_addr	source_ip;
	CFStringRef	source_ip_str;
	CFAbsoluteTime	receive_time;
	ptrlist_t	options;
	size_t		ndra_length;
	uint8_t		ndra_buf[1]; /* variable length */
};

struct _nd_opt_linkaddr {
	u_int8_t		nd_opt_linkaddr_type;
	u_int8_t		nd_opt_linkaddr_len;
	u_int8_t		nd_opt_linkaddr_data[1];
} __attribute__((__packed__));

#define ND_OPT_PREFIX_INFORMATION_LENGTH sizeof(struct nd_opt_prefix_info)

#define ND_OPT_ROUTE_INFORMATION_LENGTH sizeof(struct nd_opt_route_info)

#define ND_OPT_LINKADDR_HEADER_LENGTH				\
	offsetof(struct _nd_opt_linkaddr, nd_opt_linkaddr_data)

#define ND_OPT_RDNSS_MIN_LENGTH		sizeof(struct nd_opt_rdnss)
#define ND_OPT_RDNSS_HEADER_LENGTH	offsetof(struct nd_opt_rdnss,	\
						 nd_opt_rdnss_addr)

#define ND_OPT_DNSSL_MIN_LENGTH		sizeof(struct nd_opt_dnssl)
#define ND_OPT_DNSSL_HEADER_LENGTH	offsetof(struct nd_opt_dnssl,	\
						 nd_opt_dnssl_domains)

#define ND_OPT_CAPPORT_HEADER_LENGTH	sizeof(struct nd_opt_hdr)

STATIC void
RouterAdvertisementAppendDescription(RouterAdvertisementRef ra,
				     CFMutableStringRef str);

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__RouterAdvertisementCopyDebugDesc(CFTypeRef cf);
STATIC void		__RouterAdvertisementDeallocate(CFTypeRef cf);

STATIC CFTypeID __kRouterAdvertisementTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __RouterAdvertisementClass
= {
   0,	/* version */
   "RouterAdvertisement",		/* className */
   NULL,				/* init */
   NULL,				/* copy */
   __RouterAdvertisementDeallocate,	/* deallocate */
   NULL,				/* equal */
   NULL,				/* hash */
   NULL,				/* copyFormattingDesc */
   __RouterAdvertisementCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__RouterAdvertisementCopyDebugDesc(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	RouterAdvertisementRef	ra = (RouterAdvertisementRef)cf;
	CFMutableStringRef	str;

	str = CFStringCreateMutable(allocator, 0);
	STRING_APPEND(str, "<RouterAdvertisement %p [%p]> { ",
		      cf, allocator);
	RouterAdvertisementAppendDescription(ra, str);
	STRING_APPEND_STR(str, "}");
	return (str);
}


STATIC void
__RouterAdvertisementDeallocate(CFTypeRef cf)
{
	RouterAdvertisementRef 	ra = (RouterAdvertisementRef)cf;

	ptrlist_free(&ra->options);
	my_CFRelease(&ra->source_ip_str);
	return;
}

STATIC void
__RouterAdvertisementInitialize(void)
{
	/* initialize runtime */
	__kRouterAdvertisementTypeID
		= _CFRuntimeRegisterClass(&__RouterAdvertisementClass);
	return;
}

STATIC void
__RouterAdvertisementRegisterClass(void)
{
	STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

	pthread_once(&initialized, __RouterAdvertisementInitialize);
	return;
}

#define RouterAdvertisementAllocSize(length)				\
	(offsetof(struct __RouterAdvertisement, ndra_buf) + length)

STATIC RouterAdvertisementRef
__RouterAdvertisementAllocate(CFAllocatorRef allocator, size_t ndra_length)
{
	RouterAdvertisementRef	ra;
	int			size;

	__RouterAdvertisementRegisterClass();

	size = RouterAdvertisementAllocSize(ndra_length) - sizeof(CFRuntimeBase);
	ra = (RouterAdvertisementRef)
		_CFRuntimeCreateInstance(allocator,
					 __kRouterAdvertisementTypeID,
					 size,
					 NULL);
	return (ra);
}

/**
 ** Utilities
 **/

STATIC bool
parse_nd_options(ptrlist_t * options_p, const char * buf, int len)
{
	int				left = len;
	const struct nd_opt_hdr *	opt;
	int				opt_len = 0;
	const char *			scan;

	ptrlist_init(options_p);
	for (scan = buf; left >= sizeof(*opt); ) {
		opt = (struct nd_opt_hdr *)scan;
		opt_len = opt->nd_opt_len * ND_OPT_ALIGN;
		if (opt_len == 0 || opt_len > left) {
			/* truncated packet */
			ptrlist_free(options_p);
			return (false);
		}
		ptrlist_add(options_p, (void *)opt);
		scan += opt_len;
		left -= opt_len;
	}
	return (true);
}

STATIC const char *
S_nd_opt_name(int nd_opt)
{
	const char *	str;

	switch (nd_opt) {
	case ND_OPT_SOURCE_LINKADDR:
		str = "source link-address";
		break;
	case ND_OPT_TARGET_LINKADDR:
		str = "target link-address";
		break;
	case ND_OPT_PREFIX_INFORMATION:
		str = "prefix info";
		break;
	case ND_OPT_REDIRECTED_HEADER:
		str = "redirected header";
		break;
	case ND_OPT_MTU:
		str = "mtu";
		break;
	case ND_OPT_ROUTE_INFO:
		str = "route info";
		break;
	case ND_OPT_RDNSS:
		str = "rdnss";
		break;
	case ND_OPT_DNSSL:
		str = "dnssl";
		break;
	case ND_OPT_CAPTIVE_PORTAL:
		str = "captive portal";
		break;
	default:
		str = "<unknown>";
		break;
	}
	return (str);
}

STATIC const uint8_t *
find_source_link_address(ptrlist_t * options_p, int * ret_len)
{
	int		count;
	int		i;

	count = ptrlist_count(options_p);
	*ret_len = 0;
	for (i = 0; i < count; i++) {
		const struct _nd_opt_linkaddr *	opt;

		opt = (const struct _nd_opt_linkaddr *)
			ptrlist_element(options_p, i);
		if (opt->nd_opt_linkaddr_type == ND_OPT_SOURCE_LINKADDR) {
			int	opt_len;

			opt_len = opt->nd_opt_linkaddr_len * ND_OPT_ALIGN;
			if (opt_len <
			    (ND_OPT_LINKADDR_HEADER_LENGTH + ETHER_ADDR_LEN)) {
				/* invalid option */
				break;
			}
			*ret_len = opt_len - ND_OPT_LINKADDR_HEADER_LENGTH;
			return (opt->nd_opt_linkaddr_data);
		}
	}
	return (NULL);
}

STATIC const struct in6_addr *
find_rdnss(ptrlist_t * options_p, int * rdnss_count_p, uint32_t * lifetime_p)
{
	int			count;
	int			i;
	uint32_t		lifetime = 0;
	const struct in6_addr *	rdnss = NULL;
	int			rdnss_count = 0;

	count = ptrlist_count(options_p);
	for (i = 0; i < count; i++) {
		const struct nd_opt_rdnss *	opt;

		opt = (const struct nd_opt_rdnss *)ptrlist_element(options_p, i);
		if (opt->nd_opt_rdnss_type == ND_OPT_RDNSS) {
			int	opt_len = opt->nd_opt_rdnss_len * ND_OPT_ALIGN;

			if (opt_len < ND_OPT_RDNSS_MIN_LENGTH) {
				/* invalid option */
				break;
			}
			if (opt->nd_opt_rdnss_lifetime == 0) {
				/* skip it */
				continue;
			}
			rdnss = opt->nd_opt_rdnss_addr;
			lifetime = ntohl(opt->nd_opt_rdnss_lifetime);
			rdnss_count = (opt_len - ND_OPT_RDNSS_HEADER_LENGTH)
				/ sizeof(struct in6_addr);
			break;
		}
	}
	if (rdnss_count_p != NULL) {
		*rdnss_count_p = rdnss_count;
	}
	if (lifetime_p != NULL) {
		*lifetime_p = lifetime;
	}
	return (rdnss);
}

STATIC const uint8_t *
get_dnssl_domains(const struct nd_opt_dnssl * dnssl, int opt_len,
		  int * domains_length_p)
{
	const uint8_t *	domains;
	int		domains_length;

	domains = dnssl->nd_opt_dnssl_domains;
	domains_length = opt_len - ND_OPT_DNSSL_HEADER_LENGTH;

	/* remove all but the last trailing nul */
	while (domains_length >= 2
	       && domains[domains_length - 2] == 0) {
		domains_length--;
	}
	if (domains_length == 0) {
		domains = NULL;
	}
	*domains_length_p = domains_length;
	return (domains);
}

STATIC const uint8_t *
find_dnssl(ptrlist_t * options_p, int * domains_length_p, uint32_t * lifetime_p)
{
	int		count;
	const uint8_t *	domains = NULL;
	int		domains_length = 0;
	int		i;
	uint32_t	lifetime = 0;

	count = ptrlist_count(options_p);
	for (i = 0; i < count; i++) {
		const struct nd_opt_dnssl * 	dnssl;
		int				opt_len;

		dnssl = (const struct nd_opt_dnssl *)
			ptrlist_element(options_p, i);
		if (dnssl->nd_opt_dnssl_type != ND_OPT_DNSSL) {
			continue;
		}
		opt_len = dnssl->nd_opt_dnssl_len * ND_OPT_ALIGN;
		if (opt_len < ND_OPT_DNSSL_MIN_LENGTH) {
			/* no domain names */
			break;
		}
		if (dnssl->nd_opt_dnssl_lifetime == 0) {
			/* skip it */
			continue;
		}
		domains = get_dnssl_domains(dnssl, opt_len, &domains_length);
		lifetime = ntohl(dnssl->nd_opt_dnssl_lifetime);
		break;
	}
	if (domains_length_p != NULL) {
		*domains_length_p = domains_length;
	}
	if (lifetime_p != NULL) {
		*lifetime_p = lifetime;
	}
	return (domains);
}

STATIC const uint8_t *
get_captive_portal(const struct nd_opt_hdr * opt, int opt_len, int * ret_len)
{
	const uint8_t *	uri;
	int		uri_length;

	if (opt_len <= ND_OPT_CAPPORT_HEADER_LENGTH) {
		return (NULL);
	}
	uri_length = opt_len - ND_OPT_CAPPORT_HEADER_LENGTH;
	uri = ((const uint8_t *)opt) + ND_OPT_CAPPORT_HEADER_LENGTH;
	if (*uri == 0) {
		/* starts with a nul character */
		return (NULL);
	}

	/* remove all but the last trailing nul */
	while (uri_length >= 2
	       && uri[uri_length - 2] == 0) {
		uri_length--;
	}
	*ret_len = uri_length;
	return (uri);
}

STATIC const uint8_t *
find_captive_portal(ptrlist_t * options_p, int * ret_len)
{
	int		count;
	int		i;

	count = ptrlist_count(options_p);
	for (i = 0; i < count; i++) {
		const struct nd_opt_hdr * opt;

		opt = (const struct nd_opt_hdr *)ptrlist_element(options_p, i);
		if (opt->nd_opt_type == ND_OPT_CAPTIVE_PORTAL) {
			int		opt_len = opt->nd_opt_len * ND_OPT_ALIGN;
			const uint8_t *	uri;
			int		uri_length;

			uri = get_captive_portal(opt, opt_len, &uri_length);
			if (uri != NULL) {
				*ret_len = uri_length;
				return (uri);
			}
			break;
		}
	}
	*ret_len = 0;
	return (NULL);
}

STATIC uint32_t
find_prefix_lifetimes(ptrlist_t * options_p,
		      uint32_t * valid_lifetime)
{
	int					count;
	int					i;

	count = ptrlist_count(options_p);
	for (i = 0; i < count; i++) {
		const struct nd_opt_prefix_info * 	opt;
		int					opt_len;
		opt = (const struct nd_opt_prefix_info *)
			ptrlist_element(options_p, i);
		if (opt->nd_opt_pi_type != ND_OPT_PREFIX_INFORMATION) {
			continue;
		}
		opt_len = opt->nd_opt_pi_len * ND_OPT_ALIGN;
		if (opt_len < ND_OPT_PREFIX_INFORMATION_LENGTH) {
			break;
		}
		if (opt->nd_opt_pi_valid_time == 0
		    || opt->nd_opt_pi_preferred_time == 0) {
			continue;
		}
		*valid_lifetime = ntohl(opt->nd_opt_pi_valid_time);
		return (ntohl(opt->nd_opt_pi_preferred_time));
	}
	*valid_lifetime = 0;
	return (0);
}

STATIC void
AppendTruncatedOptionDescription(CFMutableStringRef str,
				 const struct nd_opt_hdr * opt,
				 int opt_len, int min_len)
{
	STRING_APPEND(str, "truncated (%d < %d) ",
		      opt_len, min_len);
	print_bytes_sep_cfstr(str,
			      (uint8_t *)(opt + 1),
			      opt_len - sizeof(*opt), ':');
}

STATIC void
AppendRDNSSOptionDescription(CFMutableStringRef str,
			     const struct nd_opt_hdr * opt,
			     int opt_len)
{
	int				count;
	uint32_t			lifetime;
	char 				ntopbuf[INET6_ADDRSTRLEN];
	const struct nd_opt_rdnss *	rdnss;
	const struct in6_addr *		servers;

	if (opt_len < ND_OPT_RDNSS_MIN_LENGTH) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_RDNSS_MIN_LENGTH);
		return;
	}
	rdnss = (const struct nd_opt_rdnss *)opt;
	lifetime = ntohl(rdnss->nd_opt_rdnss_lifetime);
	STRING_APPEND_STR(str, " lifetime ");
	if (lifetime == RA_OPT_INFINITE_LIFETIME) {
		STRING_APPEND_STR(str, "infinite");
	}
	else {
		STRING_APPEND(str, "%us", lifetime);
	}
	STRING_APPEND_STR(str, ", addr: ");
	count = (opt_len - ND_OPT_RDNSS_HEADER_LENGTH)
		/ sizeof(struct in6_addr);
	servers = (const struct in6_addr *)rdnss->nd_opt_rdnss_addr;
	for (int i = 0; i < count; i++) {
		STRING_APPEND(str, "%s%s",
			      (i == 0) ? "" : ", ",
			      inet_ntop(AF_INET6, servers + i,
					ntopbuf, sizeof(ntopbuf)));
	}
	return;
}

STATIC void
AppendDNSSLOptionDescription(CFMutableStringRef str,
			     const struct nd_opt_hdr * opt,
			     int opt_len)
{
	int 				count = 0;
	int 				i;
	const char * *			list;
	uint32_t			lifetime;
	const struct nd_opt_dnssl *	dnssl;
	const uint8_t *			domains;
	int				domains_length;

	if (opt_len < ND_OPT_DNSSL_MIN_LENGTH) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_DNSSL_MIN_LENGTH);
		return;
	}
	dnssl = (const struct nd_opt_dnssl *)opt;
	lifetime = ntohl(dnssl->nd_opt_dnssl_lifetime);
	STRING_APPEND_STR(str, " lifetime ");
	if (lifetime == RA_OPT_INFINITE_LIFETIME) {
		STRING_APPEND_STR(str, "infinite");
	}
	else {
		STRING_APPEND(str, "%us", lifetime);
	}
	STRING_APPEND_STR(str, ", domain(s): ");
	domains = get_dnssl_domains(dnssl, opt_len, &domains_length);
	if (domains == NULL) {
		STRING_APPEND(str, "no domains ");
		print_bytes_sep_cfstr(str,
				      (uint8_t *)(opt + 1),
				      opt_len - sizeof(*opt), ':');
		return;
	}
	list = DNSNameListCreate(domains, domains_length, &count);
	for (i = 0; i < count; i++) {
		STRING_APPEND(str, "%s%s",
			      (i == 0) ? "" : ", ",
			      list[i]);
	}
	if (list != NULL) {
		free(list);
	}
	return;
}

STATIC void
AppendPrefixInformationOptionDescription(CFMutableStringRef str,
					 const struct nd_opt_hdr * opt,
					 int opt_len)
{
	char	 				ntopbuf[INET6_ADDRSTRLEN];
	uint32_t				p_lifetime;
	const struct nd_opt_prefix_info * 	pi;
	uint32_t				v_lifetime;

	pi = (const struct nd_opt_prefix_info *)opt;
	if (opt_len < ND_OPT_PREFIX_INFORMATION_LENGTH) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_PREFIX_INFORMATION_LENGTH);
		return;
	}
	STRING_APPEND(str, " %s/%d, Flags [",
		      inet_ntop(AF_INET6, &pi->nd_opt_pi_prefix,
				ntopbuf, sizeof(ntopbuf)),
		      pi->nd_opt_pi_prefix_len);
	if ((pi->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_ONLINK) != 0) {
		STRING_APPEND_STR(str, " onlink");
	}
	if ((pi->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) != 0) {
		STRING_APPEND_STR(str, " auto");
	}
	STRING_APPEND_STR(str, " ], valid time ");
	v_lifetime = ntohl(pi->nd_opt_pi_valid_time);
	p_lifetime = ntohl(pi->nd_opt_pi_preferred_time);
	if (v_lifetime == RA_OPT_INFINITE_LIFETIME) {
		STRING_APPEND(str, "infinite");
	}
	else {
		STRING_APPEND(str, "%us", v_lifetime);
	}
	STRING_APPEND_STR(str, ", pref. time ");
	if (p_lifetime == RA_OPT_INFINITE_LIFETIME) {
		STRING_APPEND(str, "infinite");
	}
	else {
		STRING_APPEND(str, "%us",
			      ntohl(pi->nd_opt_pi_preferred_time));
	}
	return;
}

STATIC const char *
get_nd_ra_rtpref_string(uint8_t flags)
{
	const char *	str;

	switch (flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		str = "high";
		break;
	case ND_RA_FLAG_RTPREF_MEDIUM:
		str = "medium";
		break;
	case ND_RA_FLAG_RTPREF_LOW:
		str = "low";
		break;
	case ND_RA_FLAG_RTPREF_RSV:
		str = "reserved";
		break;
	default:
		str = "?";
		break;
	}
	return (str);
}

STATIC void
AppendRouteInformationOptionDescription(CFMutableStringRef str,
					const struct nd_opt_hdr * opt,
					int opt_len)
{
	uint8_t					last_byte_bits;
	uint32_t				lifetime;
	uint8_t					n_prefix_bytes;
	char	 				ntopbuf[INET6_ADDRSTRLEN];
	struct in6_addr				prefix;
	int					prefix_length;
	const struct nd_opt_route_info * 	ri;

	if (opt_len < ND_OPT_ROUTE_INFORMATION_LENGTH) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_ROUTE_INFORMATION_LENGTH);
		return;
	}
	ri = (const struct nd_opt_route_info *)opt;
#define MAX_PREFIX_LENGTH	128
	prefix_length = ri->nd_opt_rti_prefixlen;
	if (prefix_length > MAX_PREFIX_LENGTH) {
		STRING_APPEND(str, "invalid prefix length %d > %d",
			      prefix_length, MAX_PREFIX_LENGTH);
		print_bytes_sep_cfstr(str,
				      (uint8_t *)(opt + 1),
				      opt_len - sizeof(*opt), ':');
		return;
	}
#define N_BITS_PER_BYTE		8
	n_prefix_bytes = prefix_length / N_BITS_PER_BYTE;
	last_byte_bits = prefix_length & (N_BITS_PER_BYTE - 1);
	if (last_byte_bits != 0) {
		n_prefix_bytes++;
	}
	if (opt_len < (ND_OPT_ROUTE_INFORMATION_LENGTH + n_prefix_bytes)) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_ROUTE_INFORMATION_LENGTH
						 + n_prefix_bytes);
		return;
	}
	memset(&prefix, 0, sizeof(prefix));
	memcpy(&prefix, (const uint8_t *)(ri + 1), n_prefix_bytes);
	if (last_byte_bits != 0) {
		/* mask out the unwanted bits */
		uint8_t *masked_byte;
		uint8_t	 mask = (uint8_t)~((1 << (8 - last_byte_bits)) - 1);

		masked_byte = (((uint8_t *)&prefix)) + (n_prefix_bytes - 1);
#if 0
		if ((*masked_byte & mask) != 0) {
			printf("masked byte 0x%x => 0x%x\n",
			       *masked_byte, *masked_byte & mask);
		}
#endif
		*masked_byte &= mask;
	}
	STRING_APPEND(str, " %s/%d, pref=%s, lifetime ",
		      inet_ntop(AF_INET6, &prefix, ntopbuf, sizeof(ntopbuf)),
		      prefix_length,
		      get_nd_ra_rtpref_string(ri->nd_opt_rti_flags));
	lifetime = ntohl(ri->nd_opt_rti_lifetime);
	if (lifetime == RA_OPT_INFINITE_LIFETIME) {
		STRING_APPEND_STR(str, "infinite");
	}
	else {
		STRING_APPEND(str, "%us", lifetime);
	}
	return;
}

STATIC void
AppendCaptivePortalOptionDescription(CFMutableStringRef str,
				     const struct nd_opt_hdr * opt,
				     int opt_len)
{
	const uint8_t *	uri;
	int		uri_length;

	if (opt_len <= ND_OPT_CAPPORT_HEADER_LENGTH) {
		AppendTruncatedOptionDescription(str, opt, opt_len,
						 ND_OPT_CAPPORT_HEADER_LENGTH + 1);
		return;
	}
	uri = get_captive_portal(opt, opt_len, &uri_length);
	if (uri == NULL) {
		STRING_APPEND_STR(str, "empty uri: ");
		print_bytes_sep_cfstr(str,
				      (uint8_t *)(opt + 1),
				      opt_len - sizeof(*opt), ':');
		return;
	}
	STRING_APPEND(str, "uri=%.*s", uri_length, uri);
	return;
}

STATIC void
AppendOptionDescriptions(CFMutableStringRef str, ptrlist_t * options_p)
{
	int		count;
	int		i;

	count = ptrlist_count(options_p);
	if (count == 0) {
		return;
	}
	for (i = 0; i < count; i++) {
		const struct nd_opt_hdr *	opt;
		int				opt_len;
		const char *			opt_name;

		opt = (const struct nd_opt_hdr *)ptrlist_element(options_p, i);
		opt_name = S_nd_opt_name(opt->nd_opt_type);
		opt_len = opt->nd_opt_len * ND_OPT_ALIGN;
		STRING_APPEND(str, "\t%s option (%d), length %d (%d): ",
			      opt_name,
			      opt->nd_opt_type,
			      opt_len,
			      opt->nd_opt_len);
		switch (opt->nd_opt_type) {
		case ND_OPT_RDNSS:
			AppendRDNSSOptionDescription(str, opt, opt_len);
			break;
		case ND_OPT_DNSSL:
			AppendDNSSLOptionDescription(str, opt, opt_len);
			break;
		case ND_OPT_PREFIX_INFORMATION:
			AppendPrefixInformationOptionDescription(str, opt,
								 opt_len);
			break;
		case ND_OPT_ROUTE_INFO:
			AppendRouteInformationOptionDescription(str, opt,
								opt_len);
			break;
		case ND_OPT_CAPTIVE_PORTAL:
			AppendCaptivePortalOptionDescription(str, opt, opt_len);
			break;
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_SOURCE_LINKADDR:
		default:
			print_bytes_sep_cfstr(str,
					      (uint8_t *)(opt + 1),
					      opt_len - sizeof(*opt), ':');
			break;
		}
		STRING_APPEND_STR(str, "\n");
	}
	return;
}

STATIC const struct nd_router_advert *
RouterAdvertisementGetData(RouterAdvertisementRef ra)
{
	return ((const struct nd_router_advert *)ra->ndra_buf);
}

STATIC void
RouterAdvertisementAppendDescription(RouterAdvertisementRef ra,
				     CFMutableStringRef str)
{
	uint8_t				flags;
	uint32_t			lifetime;
	const char *			lifetime_string;
	const struct nd_router_advert * ndra;

	ndra = RouterAdvertisementGetData(ra);
	flags = RouterAdvertisementGetFlags(ra);
	lifetime = RouterAdvertisementGetRouterLifetime(ra);
	lifetime_string
		= (lifetime == ROUTER_LIFETIME_MAXIMUM) ? " (max)" : "";
	STRING_APPEND(str,
		      "from %@, length %ld, hop limit %d, lifetime %us%s, "
		      "reachable %dms, retransmit %dms, flags 0x%x",
		      RouterAdvertisementGetSourceIPAddressAsString(ra),
		      ra->ndra_length,
		      ndra->nd_ra_curhoplimit,
		      lifetime, lifetime_string,
		      ntohl(ndra->nd_ra_reachable),
		      ntohl(ndra->nd_ra_retransmit),
		      flags);
	if (flags == 0) {
		STRING_APPEND_STR(str, "\n");
	}
	else {
		STRING_APPEND_STR(str, "=[");
		if ((flags & ND_RA_FLAG_MANAGED) != 0) {
			STRING_APPEND_STR(str, " managed");
		}
		if ((flags & ND_RA_FLAG_OTHER) != 0) {
			STRING_APPEND_STR(str, " other");
		}
		if ((flags & ND_RA_FLAG_HA) != 0) {
			STRING_APPEND_STR(str, " home-agent");
		}
		if ((flags & ND_RA_FLAG_PROXY) != 0) {
			STRING_APPEND_STR(str, " proxy");
		}
		STRING_APPEND_STR(str, " ]");
		STRING_APPEND_STR(str, ", pref=");
		STRING_APPEND(str, "%s\n",
			      get_nd_ra_rtpref_string(flags));
	}
	AppendOptionDescriptions(str, &ra->options);
	return;
}

/**
 ** API
 **/
PRIVATE_EXTERN RouterAdvertisementRef
RouterAdvertisementCreate(const struct nd_router_advert * ndra,
			  size_t ndra_length,
			  const struct in6_addr * source_ip,
			  CFAbsoluteTime receive_time)
{
	struct nd_router_advert *	ndra_p;
	RouterAdvertisementRef		ra;

	ra = __RouterAdvertisementAllocate(NULL, ndra_length);
	/*
	 * NOTE: if you add a field, ensure that it gets initialized here.
	 */
	memcpy(ra->ndra_buf, ndra, ndra_length);
	ra->ndra_length = ndra_length;
	ra->source_ip = *source_ip;
	ra->source_ip_str = my_CFStringCreateWithIPv6Address(source_ip);
	ra->receive_time = receive_time;
	ndra_p = (struct nd_router_advert *)ra->ndra_buf;
	if (!parse_nd_options(&ra->options, (char *)(ndra_p + 1),
			      ra->ndra_length - sizeof(*ndra_p))) {
		CFRelease(ra);
		return (NULL);
	}
	return (ra);
}

PRIVATE_EXTERN CFAbsoluteTime
RouterAdvertisementGetReceiveTime(RouterAdvertisementRef ra)
{
	return (ra->receive_time);
}

PRIVATE_EXTERN bool
RouterAdvertisementLifetimeHasExpired(RouterAdvertisementRef ra,
				      CFAbsoluteTime now,
				      uint32_t lifetime)
{
	bool		expired;

	if (lifetime == 0) {
		expired = true;
	}
	else if (lifetime == RA_OPT_INFINITE_LIFETIME) {
		expired = false;
	}
	else {
		CFAbsoluteTime	start_time;

		start_time = RouterAdvertisementGetReceiveTime(ra);
		if (start_time > now) {
			/* time went backwards */
			expired = true;
		}
		else {
			expired = ((now - start_time) >= lifetime);
		}
	}
	return (expired);
}

PRIVATE_EXTERN CFStringRef
RouterAdvertisementGetSourceIPAddressAsString(RouterAdvertisementRef ra)
{
	return (ra->source_ip_str);
}

PRIVATE_EXTERN const struct in6_addr *
RouterAdvertisementGetSourceIPAddress(RouterAdvertisementRef ra)
{
	return (&ra->source_ip);
}

PRIVATE_EXTERN CFStringRef
RouterAdvertisementCopyDescription(RouterAdvertisementRef ra)
{
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	RouterAdvertisementAppendDescription(ra, str);
	return (str);
}

PRIVATE_EXTERN uint8_t
RouterAdvertisementGetFlags(RouterAdvertisementRef ra)
{
	return (RouterAdvertisementGetData(ra)->nd_ra_flags_reserved);
}

PRIVATE_EXTERN uint16_t
RouterAdvertisementGetRouterLifetime(RouterAdvertisementRef ra)
{
	uint16_t	lifetime;

	lifetime = RouterAdvertisementGetData(ra)->nd_ra_router_lifetime;
	return (ntohs(lifetime));
}

PRIVATE_EXTERN const uint8_t *
RouterAdvertisementGetSourceLinkAddress(RouterAdvertisementRef ra,
					int * ret_len)
{
	return (find_source_link_address(&ra->options, ret_len));
}

PRIVATE_EXTERN uint32_t
RouterAdvertisementGetPrefixLifetimes(RouterAdvertisementRef ra,
				      uint32_t * valid_lifetime)
{
	return (find_prefix_lifetimes(&ra->options, valid_lifetime));
}

PRIVATE_EXTERN const struct in6_addr *
RouterAdvertisementGetRDNSS(RouterAdvertisementRef ra,
			    int * dns_servers_count_p,
			    uint32_t * lifetime_p)
{
	return (find_rdnss(&ra->options, dns_servers_count_p, lifetime_p));
}

PRIVATE_EXTERN const uint8_t *
RouterAdvertisementGetDNSSL(RouterAdvertisementRef ra,
			    int * domains_length_p,
			    uint32_t * lifetime_p)
{
	return (find_dnssl(&ra->options, domains_length_p, lifetime_p));
}

PRIVATE_EXTERN CFAbsoluteTime
RouterAdvertisementGetDNSExpirationTime(RouterAdvertisementRef ra,
					CFAbsoluteTime now)
{
	const uint8_t *		dnssl;
	int			dnssl_length;
	uint32_t 		dnssl_lifetime;
	CFAbsoluteTime		expiration = 0;
	uint32_t 		lifetime;
	const struct in6_addr *	rdnss;
	int			rdnss_count;
	uint32_t		rdnss_lifetime;

	rdnss = RouterAdvertisementGetRDNSS(ra, &rdnss_count, &rdnss_lifetime);
	if (rdnss == NULL
	    || RouterAdvertisementLifetimeHasExpired(ra, now, rdnss_lifetime)) {
		goto done;
	}
	dnssl = RouterAdvertisementGetDNSSL(ra, &dnssl_length, &dnssl_lifetime);
	if (dnssl != NULL
	    && !RouterAdvertisementLifetimeHasExpired(ra, now, dnssl_lifetime)
	    && dnssl_lifetime < rdnss_lifetime) {
		/* use DNSSL lifetime since it is shorter */
		lifetime = dnssl_lifetime;
	}
	else {
		/* use RDNSS lifetime */
		lifetime = rdnss_lifetime;
	}
	if (lifetime != RA_OPT_INFINITE_LIFETIME) {
		expiration = RouterAdvertisementGetReceiveTime(ra)
			+ lifetime;
	}

 done:
	return (expiration);
}

PRIVATE_EXTERN CFStringRef
RouterAdvertisementCopyCaptivePortal(RouterAdvertisementRef ra)
{
	const uint8_t *	url;
	int		url_length;

	url = find_captive_portal(&ra->options, &url_length);
	return (my_CFStringCreateWithBytes(url, url_length));
}

#define kPacket			CFSTR("Packet")
#define kSourceIPAddress	CFSTR("SourceIPAddress")
#define kReceiveDate		CFSTR("ReceiveDate")

PRIVATE_EXTERN CFDictionaryRef
RouterAdvertisementCopyDictionary(RouterAdvertisementRef ra)
{
	CFDictionaryRef	dict;
	const void *	keys[3] = {
				   kPacket,
				   kSourceIPAddress,
				   kReceiveDate
	};
	CFDataRef	packet;
	CFDateRef	receive_date;
	const void *	values[3];

	packet = CFDataCreate(NULL, ra->ndra_buf, ra->ndra_length);
	receive_date = CFDateCreate(NULL, ra->receive_time);
	values[0] = packet;
	values[1] = RouterAdvertisementGetSourceIPAddressAsString(ra);
	values[2] = receive_date;
	dict = CFDictionaryCreate(NULL,
				  keys,
				  values,
				  countof(keys),
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	CFRelease(packet);
	CFRelease(receive_date);
	return (dict);
}

PRIVATE_EXTERN RouterAdvertisementRef
RouterAdvertisementCreateWithDictionary(CFDictionaryRef dict)
{
	const struct icmp6_hdr *	icmp_hdr;
	const struct nd_router_advert * ndra_p;
	int				ndra_length;
	CFDataRef			packet;
	RouterAdvertisementRef		ra = NULL;
	CFDateRef			receive_date;
	struct in6_addr			source_ip;
	CFStringRef			source_ip_str;

	/* packet */
	packet = CFDictionaryGetValue(dict, kPacket);
	if (isA_CFData(packet) == NULL) {
		goto done;
	}
	ndra_p = (const struct nd_router_advert *)CFDataGetBytePtr(packet);
	ndra_length = CFDataGetLength(packet);
	if (ndra_length < sizeof(*ndra_p)) {
		goto done;
	}
	icmp_hdr = &ndra_p->nd_ra_hdr;
	if (icmp_hdr->icmp6_type != ND_ROUTER_ADVERT) {
		goto done;
	}
	if (icmp_hdr->icmp6_code != 0) {
		goto done;
	}

	/* receive date */
	receive_date = CFDictionaryGetValue(dict, kReceiveDate);
	if (isA_CFDate(receive_date) == NULL) {
		goto done;
	}

	/* source IP */
	source_ip_str = CFDictionaryGetValue(dict, kSourceIPAddress);
	if (isA_CFString(source_ip_str) == NULL) {
		goto done;
	}
	if (!my_CFStringToIPv6Address(source_ip_str, &source_ip)) {
		goto done;
	}
	ra = RouterAdvertisementCreate(ndra_p, ndra_length, &source_ip,
				       CFDateGetAbsoluteTime(receive_date));
 done:
	return (ra);
}
