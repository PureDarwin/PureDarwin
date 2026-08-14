/*
 * Copyright (c) 2009-2013 Apple Inc. All rights reserved.
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
 * DHCPv6.c
 * - implementation of DHCPv6-specific functions (packet field set, get)
 */

/* 
 * Modification History
 *
 * September 16, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include <stddef.h>
#include "util.h"
#include "cfutil.h"
#include <SystemConfiguration/SCPrivate.h>

const char *
DHCPv6MessageName(int message)
{
    const char	*	str;

    switch (message) {
    case kDHCPv6MessageNone:
	str = "None";
	break;
    case kDHCPv6MessageSOLICIT:
	str = "SOLICIT";
	break;
    case kDHCPv6MessageADVERTISE:
	str = "ADVERTISE";
	break;
    case kDHCPv6MessageREQUEST:
	str = "REQUEST";
	break;
    case kDHCPv6MessageCONFIRM:
	str = "CONFIRM";
	break;
    case kDHCPv6MessageRENEW:
	str = "RENEW";
	break;
    case kDHCPv6MessageREBIND:
	str = "REBIND";
	break;
    case kDHCPv6MessageREPLY:
	str = "REPLY";
	break;
    case kDHCPv6MessageRELEASE:
	str = "RELEASE";
	break;
    case kDHCPv6MessageDECLINE:
	str = "DECLINE";
	break;
    case kDHCPv6MessageRECONFIGURE:
	str = "RECONFIGURE";
	break;
    case kDHCPv6MessageINFORMATION_REQUEST:
	str = "INFORMATION_REQUEST";
	break;
    case kDHCPv6MessageRELAY_FORW:
	str = "RELAY_FORW";
	break;
    case kDHCPv6MessageRELAY_REPL:
	str = "RELAY_REPL";
	break;
    default:
	str = "<unknown";
	break;
    }
    return (str);
}

void
DHCPv6PacketSetMessageType(DHCPv6PacketRef pkt, int msg_type)
{
    pkt->msg_type = msg_type;
    return;
}

/*
 * Function: DHCPv6PacketSetTransactionID
 * Purpose:
 *   Set the transaction id in the given DHCPv6 packet using the given 
 *   transaction id.   Convert the transaction id to network byte order, then
 *   take the bottom 3 bytes and place them in the DHCPv6 packet transaction_id
 *   field.
 */
void
DHCPv6PacketSetTransactionID(DHCPv6PacketRef pkt, uint32_t transaction_id)
{
    uint32_t	r = htonl(transaction_id);
    uint8_t *	v;

    /* grab the lower 24 bits */
    v = (uint8_t *)&r;
    pkt->transaction_id[0] = v[1];
    pkt->transaction_id[1] = v[2];
    pkt->transaction_id[2] = v[3];
    return;
}

/*
 * Function: DHCPv6PacketGetTransactionID
 * Purpose:
 *   Return the transaction id from the given DHCPv6 packet in host byte order.
 *   The transaction id is 3 bytes long, so stuff the bytes into the lower 
 *   3 bytes of the uint32_t, and convert to host byte order.
 * Returns:
 *   Transaction id from DHCPv6 packet in host byte order as a 32-bit quantity.
 */
uint32_t
DHCPv6PacketGetTransactionID(const DHCPv6PacketRef pkt)
{
    uint32_t	r;
    uint8_t *	v;

    /* grab the lower 24 bits */
    v = (uint8_t *)&r;
    v[0] = 0;
    v[1] = pkt->transaction_id[0];
    v[2] = pkt->transaction_id[1];
    v[3] = pkt->transaction_id[2];
    return (ntohl(r));
}

void
DHCPv6PacketPrintToString(CFMutableStringRef str,
			  const DHCPv6PacketRef pkt, int pkt_len)
{
    if (pkt_len < DHCPV6_PACKET_HEADER_LENGTH) {
	STRING_APPEND(str, "Packet too short %d < %d\n",
		      pkt_len, DHCPV6_PACKET_HEADER_LENGTH);
    }
    else {
	STRING_APPEND(str, "DHCPv6 %s (%d) Transaction ID 0x%06x Length %d\n",
		      DHCPv6MessageName(pkt->msg_type), pkt->msg_type,
		      DHCPv6PacketGetTransactionID(pkt), pkt_len);
    }
    return;
}

void
DHCPv6PacketFPrint(FILE * file, const DHCPv6PacketRef pkt, int pkt_len)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    DHCPv6PacketPrintToString(str, pkt, pkt_len);
    SCPrint(TRUE, file, CFSTR("%@"), str);
    CFRelease(str);
    return;
}
