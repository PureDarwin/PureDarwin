/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <string.h>
#include "dhcplib.h"
#include "cfutil.h"
#include "symbol_scope.h"

PRIVATE_EXTERN void
dhcp_packet_print_cfstr(CFMutableStringRef str, struct dhcp * dp, int pkt_len)
{
    dhcpol_t 		options;

    if (pkt_len < sizeof(struct dhcp)) {
	STRING_APPEND(str, "Packet is too short %d < %d\n", pkt_len,
		(int)sizeof(struct dhcp));
	return;
    }
    dhcpol_init(&options);
    dhcpol_parse_packet(&options, dp, pkt_len, NULL);
    dhcp_packet_with_options_print_cfstr(str, dp, pkt_len, &options);
    dhcpol_free(&options);
    return;
}

PRIVATE_EXTERN void
dhcp_packet_with_options_print_cfstr(CFMutableStringRef str,
				     struct dhcp * dp, int pkt_len,
				     dhcpol_t * options)
{
    int 		i;
    int			j;
    int			len;

    STRING_APPEND(str, "op = ");
    if (dp->dp_op == BOOTREQUEST) {
	STRING_APPEND(str, "BOOTREQUEST\n");
    }
    else if (dp->dp_op == BOOTREPLY) {
	STRING_APPEND(str, "BOOTREPLY\n");
    }
    else {
	i = dp->dp_op;
	STRING_APPEND(str, "OP(%d)\n", i);
    }
    
    i = dp->dp_htype;
    STRING_APPEND(str, "htype = %d\n", i);
    
    STRING_APPEND(str, "flags = %x\n", ntohs(dp->dp_flags));
    len = dp->dp_hlen;
    STRING_APPEND(str, "hlen = %d\n", len);
    
    i = dp->dp_hops;
    STRING_APPEND(str, "hops = %d\n", i);
    
    STRING_APPEND(str, "xid = 0x%lx\n", (u_long)ntohl(dp->dp_xid));
    
    STRING_APPEND(str, "secs = %hu\n", ntohs(dp->dp_secs));
    
    STRING_APPEND(str, "ciaddr = %s\n", inet_ntoa(dp->dp_ciaddr));
    STRING_APPEND(str, "yiaddr = %s\n", inet_ntoa(dp->dp_yiaddr));
    STRING_APPEND(str, "siaddr = %s\n", inet_ntoa(dp->dp_siaddr));
    STRING_APPEND(str, "giaddr = %s\n", inet_ntoa(dp->dp_giaddr));
    
    STRING_APPEND(str, "chaddr = ");
    for (j = 0; j < len; j++) {
	i = dp->dp_chaddr[j];
	STRING_APPEND(str, "%0x", i);
	if (j < (len - 1)) STRING_APPEND(str, ":");
    }
    STRING_APPEND(str, "\n");
    
    STRING_APPEND(str, "sname = %s\n", dp->dp_sname);
    STRING_APPEND(str, "file = %s\n", dp->dp_file);

    if (options != NULL && dhcpol_count(options) > 0) {
	STRING_APPEND(str, "options:\n");
	dhcpol_print_cfstr(str, options);
    }
    return;
}

PRIVATE_EXTERN void
dhcp_packet_fprint(FILE * f, struct dhcp * dp, int pkt_len)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    dhcp_packet_print_cfstr(str, dp, pkt_len);
    my_CFStringPrint(f, str);
    CFRelease(str);
    fflush(f);
    return;
}

PRIVATE_EXTERN void
dhcp_packet_print(struct dhcp *dp, int pkt_len)
{
    dhcp_packet_fprint(stdout, dp, pkt_len);
    return;
}

PRIVATE_EXTERN boolean_t
dhcp_packet_match(struct bootp * packet, u_int32_t xid, 
		  u_char hwtype, void * hwaddr, int hwlen)
{
    int		check_len;

    switch (hwtype) {
    default:
    case ARPHRD_ETHER:
	check_len = hwlen;
	break;
    case ARPHRD_IEEE1394:
	check_len = 0;
	break;
    }
    if (packet->bp_op != BOOTREPLY
	|| ntohl(packet->bp_xid) != xid
	|| (packet->bp_htype != hwtype)
	|| (packet->bp_hlen != check_len)
	|| (check_len != 0 && bcmp(packet->bp_chaddr, hwaddr, check_len))) {
	return (FALSE);
    }
    return (TRUE);
}
