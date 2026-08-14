/*
 * Copyright (c) 1999-2016 Apple Inc. All rights reserved.
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
 * udp_transmit.h
 * - send a UDP packet using a socket or BPF
 */

#ifndef _S_UDP_TRANSMIT_H
#define _S_UDP_TRANSMIT_H

/* 
 * Modification History
 *
 * March 30, 2016		Dieter Siegmund (dieter@apple.com)
 * - created
 */

int
udpv4_transmit(int sockfd, void * sendbuf,
	       const char * if_name, 
	       int hwtype, const void * hwaddr,
	       struct in_addr dest_ip,
	       struct in_addr src_ip,
	       u_short dest_port,
	       u_short src_port,
	       const void * data, int len);


#endif /* _S_UDP_TRANSMIT_H */
