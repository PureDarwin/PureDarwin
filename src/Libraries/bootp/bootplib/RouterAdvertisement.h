/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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
 * RouterAdvertisement.h
 * - CF object to encapulate an IPv6 ND Router Advertisement
 */

/*
 * Modification History
 *
 * April 15, 2020		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_ROUTERADVERTISEMENT_H
#define _S_ROUTERADVERTISEMENT_H

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFString.h>
#include "symbol_scope.h"

#ifndef ND_OPT_ALIGN
#define ND_OPT_ALIGN			8
#endif /* ND_OPT_ALIGN */

#ifndef ND_RA_FLAG_PROXY
#define ND_RA_FLAG_PROXY	0x04
#endif /* ND_RA_FLAGS_PROXY */

#define ROUTER_LIFETIME_MAXIMUM		((uint16_t)0xffff)

typedef struct __RouterAdvertisement * RouterAdvertisementRef;

RouterAdvertisementRef
RouterAdvertisementCreate(const struct nd_router_advert * ndra,
			  size_t ndra_length, const struct in6_addr * from,
			  CFAbsoluteTime receive_time);

CFAbsoluteTime
RouterAdvertisementGetReceiveTime(RouterAdvertisementRef ra);

bool
RouterAdvertisementLifetimeHasExpired(RouterAdvertisementRef ra,
				      CFAbsoluteTime now,
				      uint32_t lifetime);
CFStringRef
RouterAdvertisementGetSourceIPAddressAsString(RouterAdvertisementRef ra);

const struct in6_addr *
RouterAdvertisementGetSourceIPAddress(RouterAdvertisementRef ra);

CFStringRef
RouterAdvertisementCopyDescription(RouterAdvertisementRef ra);

uint16_t
RouterAdvertisementGetRouterLifetime(RouterAdvertisementRef ra);

const uint8_t *
RouterAdvertisementGetSourceLinkAddress(RouterAdvertisementRef ra,
					int * ret_len);

uint32_t
RouterAdvertisementGetPrefixLifetimes(RouterAdvertisementRef ra,
				      uint32_t * valid_lifetime);

uint8_t
RouterAdvertisementGetFlags(RouterAdvertisementRef ra);

INLINE bool
RouterAdvertisementFlagsGetIsManaged(RouterAdvertisementRef ra)
{
	uint8_t	flags = RouterAdvertisementGetFlags(ra);
	return ((flags & ND_RA_FLAG_MANAGED) != 0);
}

INLINE bool
RouterAdvertisementFlagsGetIsOther(RouterAdvertisementRef ra)
{
	uint8_t	flags = RouterAdvertisementGetFlags(ra);
	return ((flags & ND_RA_FLAG_OTHER) != 0);
}

const struct in6_addr *
RouterAdvertisementGetRDNSS(RouterAdvertisementRef ra,
			    int * dns_servers_count_p,
			    uint32_t * lifetime_p);

const uint8_t *
RouterAdvertisementGetDNSSL(RouterAdvertisementRef ra,
			    int * domains_length_p,
			    uint32_t * lifetime_p);

CFAbsoluteTime
RouterAdvertisementGetDNSExpirationTime(RouterAdvertisementRef ra,
					CFAbsoluteTime now);

CFStringRef
RouterAdvertisementCopyCaptivePortal(RouterAdvertisementRef ra);

RouterAdvertisementRef
RouterAdvertisementCreateWithDictionary(CFDictionaryRef dict);

CFDictionaryRef
RouterAdvertisementCopyDictionary(RouterAdvertisementRef ra);

#endif /* _S_ROUTERADVERTISEMENT_H */
