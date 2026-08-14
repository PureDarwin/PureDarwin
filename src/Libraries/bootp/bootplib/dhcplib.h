/*
 * Copyright (c) 2000-2015 Apple Inc. All rights reserved.
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


#ifndef _S_DHCPLIB_H
#define _S_DHCPLIB_H

#include <mach/boolean.h>
#include <sys/time.h>
#import <netinet/bootp.h>
#include "dhcp_options.h"
#include "gen_dhcp_tags.h"

void	dhcp_packet_print_cfstr(CFMutableStringRef str, 
				struct dhcp * dp, int pkt_len);
void	dhcp_packet_with_options_print_cfstr(CFMutableStringRef str,
					     struct dhcp * dp, int pkt_len,
					     dhcpol_t * options);
void	dhcp_packet_fprint(FILE * f, struct dhcp * dp, int pkt_len);
void	dhcp_packet_print(struct dhcp * dp, int pkt_len);

/*
 * Function: is_dhcp_packet
 *
 * Purpose:
 *   Return whether packet is a DHCP packet.
 *   If the packet contains DHCP message ids, then its a DHCP packet.
 */
static __inline__ boolean_t
is_dhcp_packet(dhcpol_t * options, dhcp_msgtype_t * msgtype)
{
    if (options) {
	u_char * opt;
	int opt_len;

	opt = dhcpol_find(options, dhcptag_dhcp_message_type_e,
			  &opt_len, NULL);
	if (opt != NULL) {
	    if (msgtype)
		*msgtype = *opt;
	    return (TRUE);
	}
    }
    return (FALSE);
}

boolean_t
dhcp_packet_match(struct bootp * packet, u_int32_t xid, 
		  u_char hwtype, void * hwaddr, int hwlen);

#endif /* _S_DHCPLIB_H */
