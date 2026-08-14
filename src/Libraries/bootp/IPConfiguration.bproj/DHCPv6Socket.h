/*
 * Copyright (c) 2009-2018 Apple Inc. All rights reserved.
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
 * DHCPv6Socket.h
 * - maintain list of DHCPv6 clients
 * - distribute packet reception to enabled clients
 */
/* 
 * Modification History
 *
 * September 30, 2009		Dieter Siegmund (dieter@apple.com)
 * - created (based on bootp_session.h)
 */

#ifndef _S_DHCPV6SOCKET_H
#define _S_DHCPV6SOCKET_H

#include <stdint.h>
#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include "FDSet.h"
#include "interfaces.h"

typedef struct {
    DHCPv6PacketRef		pkt;
    int				pkt_len;
    DHCPv6OptionListRef		options;
} DHCPv6SocketReceiveData, * DHCPv6SocketReceiveDataRef;

/*
 * Type: DHCPv6SocketReceiveFunc
 * Purpose:
 *   Called to deliver data to the client.  The first two args are
 *   supplied by the client, the third is a DHCPv6ReceiveDataRef.
 */
typedef void (DHCPv6SocketReceiveFunc)(void * arg1, void * arg2, void * arg3);
typedef DHCPv6SocketReceiveFunc * DHCPv6SocketReceiveFuncPtr;

typedef struct DHCPv6Socket * DHCPv6SocketRef;

void
DHCPv6SocketSetVerbose(bool verbose);

void
DHCPv6SocketSetPorts(uint16_t client_port, uint16_t server_port);

DHCPv6SocketRef
DHCPv6SocketCreate(interface_t * if_p);

interface_t *
DHCPv6SocketGetInterface(DHCPv6SocketRef sock);

void
DHCPv6SocketRelease(DHCPv6SocketRef * sock);

void
DHCPv6SocketEnableReceive(DHCPv6SocketRef sock,
			  DHCPv6SocketReceiveFuncPtr func, 
			  void * arg1, void * arg2);

bool
DHCPv6SocketReceiveIsEnabled(DHCPv6SocketRef sock);

void
DHCPv6SocketDisableReceive(DHCPv6SocketRef sock);

int
DHCPv6SocketTransmit(DHCPv6SocketRef sock,
		     DHCPv6PacketRef pkt, int pkt_len);

#endif /* _S_DHCPV6SOCKET_H */
