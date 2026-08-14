/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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
 * bootp_session.h
 * - maintain BOOTP client socket session
 * - maintain list of BOOTP clients
 * - distribute packet reception to enabled clients
 */

/* 
 * Modification History
 *
 * May 10, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_BOOTP_SESSION_H
#define _S_BOOTP_SESSION_H

#include <stdint.h>
#include "dhcp_options.h"
#include "FDSet.h"
#include "interfaces.h"

typedef struct {
    struct dhcp  *		data;
    int				size;
    dhcpol_t			options;
} bootp_receive_data_t;

/*
 * Type: bootp_receive_func_t
 * Purpose:
 *   Called to deliver data to the client.  The first two args are
 *   supplied by the client, the third is a pointer to a bootp_receive_data_t.
 */
typedef void (bootp_receive_func_t)(void * arg1, void * arg2, void * arg3);

typedef struct bootp_session bootp_session_t;
typedef struct bootp_client bootp_client_t;

bootp_client_t *
bootp_client_init(bootp_session_t * slist, interface_t * if_p);

void
bootp_client_free(bootp_client_t * * session);

void
bootp_client_enable_receive(bootp_client_t * client,
			    bootp_receive_func_t * func, 
			    void * arg1, void * arg2);

void
bootp_client_disable_receive(bootp_client_t * client);

int
bootp_client_transmit(bootp_client_t * client,
		      struct in_addr dest_ip,
		      struct in_addr src_ip,
		      uint16_t dest_port,
		      uint16_t src_port,
		      void * data, int len);

bootp_session_t * 
bootp_session_init(uint16_t client_port);

void
bootp_session_set_verbose(bool verbose);

void
bootp_session_free(bootp_session_t * * slist);

#endif /* _S_BOOTP_SESSION_H */
