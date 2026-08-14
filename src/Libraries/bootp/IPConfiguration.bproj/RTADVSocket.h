/*
 * Copyright (c) 2011-2020 Apple Inc. All rights reserved.
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
 * RTADVSocket.h
 * - maintain list of Router Advertisement client "sockets"
 * - distribute packet reception to enabled "sockets"
 */

/* 
 * Modification History
 *
 * June 4, 2010		Dieter Siegmund (dieter@apple.com)
 * - created (based on DHCPv6Socket.h)
 */

#ifndef _S_RTADVSOCKET_H
#define _S_RTADVSOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include "FDSet.h"
#include "interfaces.h"
#include "RouterAdvertisement.h"

/*
 * Type: RTADVSocketReceiveFunc
 * Purpose:
 *   Called to deliver data to the client.  The first two args are
 *   supplied by the client, the third is a RouterAdvertisementRef.
 */
typedef void (RTADVSocketReceiveFunc)(void * arg1, void * arg2, void * arg3);
typedef RTADVSocketReceiveFunc * RTADVSocketReceiveFuncPtr;

typedef struct RTADVSocket * RTADVSocketRef;

RTADVSocketRef
RTADVSocketCreate(interface_t * if_p);

interface_t *
RTADVSocketGetInterface(RTADVSocketRef sock);

void
RTADVSocketRelease(RTADVSocketRef * sock);

void
RTADVSocketEnableReceive(RTADVSocketRef sock,
			 RTADVSocketReceiveFuncPtr func, 
			 void * arg1, void * arg2);

void
RTADVSocketDisableReceive(RTADVSocketRef sock);

int
RTADVSocketSendSolicitation(RTADVSocketRef sock, bool lladdr_ok);

#endif /* _S_RTADVSOCKET_H */
