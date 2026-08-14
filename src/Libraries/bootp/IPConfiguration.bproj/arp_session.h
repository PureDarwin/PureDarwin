/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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
 * arp_session.h
 */

/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */


#ifndef _S_ARP_SESSION_H
#define _S_ARP_SESSION_H

#include "FDSet.h"
#include "interfaces.h"
#include <CoreFoundation/CFDate.h>

#define ARP_PROBE_COUNT				3
#define ARP_GRATUITOUS_COUNT			2
#define ARP_RETRY_SECS				0.32
#define ARP_DETECT_COUNT			6
#define ARP_DETECT_RETRY_SECS			0.02
#define ARP_CONFLICT_RETRY_COUNT		2
#define ARP_CONFLICT_RETRY_DELAY_SECS		0.75

typedef struct arp_session_values {
    int * 		probe_count;
    int * 		probe_gratuitous_count;
    CFTimeInterval *	probe_interval;
    int * 		detect_count;
    CFTimeInterval * 	detect_interval;
    int *		conflict_retry_count;
    CFTimeInterval *	conflict_delay_interval;
} arp_session_values_t;

typedef struct arp_client arp_client_t;

typedef struct {
    struct in_addr		sender_ip;
    struct in_addr		target_ip;
    uint8_t			target_hardware[MAX_LINK_ADDR_LEN];
} arp_address_info_t;

typedef struct {
    arp_client_t *		client;	
    boolean_t			error;
    boolean_t			in_use;
    arp_address_info_t		addr;
} arp_result_t;

/*
 * Type: arp_result_func_t
 * Purpose:
 *   Called to send results back to the caller.  The first two args are
 *   supplied by the client.
 */
typedef void (arp_result_func_t)(void * arg1, void * arg2, 
				 const arp_result_t * result);

/*
 * Type: arp_our_address_func_t
 * Purpose:
 *   Called to check whether a given hardware address matches any
 *   of this system's hardware addresses.  A return value of TRUE
 *   implies the hardware address corresponds to a physical interface on
 *   this system, and should not be reported as a conflict.  A return of
 *   FALSE implies the converse, and a conflict should be reported.
 */
typedef boolean_t (arp_our_address_func_t)(interface_t * if_p, 
					   int hwtype, void * hwaddr,
					   int hwlen);

typedef struct arp_session arp_session_t;

arp_session_t *
arp_session_init(arp_our_address_func_t * func,
		 arp_session_values_t * values);
void
arp_session_free(arp_session_t * * session_p);

void
arp_session_set_debug(arp_session_t * session, int debug);


/**
 ** arp client functions
 **/
void
arp_client_set_probes_are_collisions(arp_client_t * client, 
				     boolean_t probes_are_collisions);

arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p);

void
arp_client_free(arp_client_t * * client_p);

const char *
arp_client_errmsg(arp_client_t * client);

void
arp_client_probe(arp_client_t * client,
		 arp_result_func_t * func, void * arg1, void * arg2,
		 struct in_addr sender_ip, struct in_addr target_ip);

void
arp_client_resolve(arp_client_t * client,
		   arp_result_func_t * func, void * arg1, void * arg2,
		   struct in_addr sender_ip, struct in_addr target_ip,
		   uint32_t resolve_secs);

void
arp_client_detect(arp_client_t * client,
		  arp_result_func_t * func, void * arg1, void * arg2,
		  const arp_address_info_t * list, int list_count);

void
arp_client_cancel(arp_client_t * client);

boolean_t
arp_client_defend(arp_client_t * client, struct in_addr our_ip);

boolean_t
arp_client_is_active(arp_client_t * client);

void
arp_client_announce(arp_client_t * client,
                    arp_result_func_t * func, void * arg1, void * arg2,
                    struct in_addr sender_ip, struct in_addr target_ip,
                    boolean_t skip);

#endif /* _S_ARP_SESSION_H */
