/*
 * Copyright (c) 2014-2015 Apple Inc. All rights reserved.
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
 * IPv4ClasslessRoute.h
 * - handle IPv4 route lists in DHCP options
 */

/*
 * Modification History
 *
 * June 5, 2014			Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_IPV4CLASSLESSROUTE_H
#define _S_IPV4CLASSLESSROUTE_H


#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include <CoreFoundation/CFArray.h>
#include "symbol_scope.h"

typedef struct {
    struct in_addr	dest;
    int			prefix_length;
    struct in_addr	gate;		/* 0.0.0.0 => direct to interface */
} IPv4ClasslessRoute, * IPv4ClasslessRouteRef;


uint8_t *
IPv4ClasslessRouteListBufferCreate(IPv4ClasslessRouteRef list, int list_count,
				   uint8_t * buffer, int * buffer_size);

IPv4ClasslessRouteRef
IPv4ClasslessRouteListCreate(const uint8_t * buffer, int buffer_size,
			     int * list_count);

IPv4ClasslessRouteRef
IPv4ClasslessRouteListGetDefault(IPv4ClasslessRouteRef list, int list_count);

IPv4ClasslessRouteRef
IPv4ClasslessRouteListCreateWithArray(CFArrayRef string_list,
				      int * ret_count);

#endif /* _S_IPV4CLASSLESSROUTE_H */

