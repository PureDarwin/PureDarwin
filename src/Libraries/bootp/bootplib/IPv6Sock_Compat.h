/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
 * IPv6Sock_Compat.h
 * - shim file for facilitating the transition of RFC 2292 to RFC 3542 
 *   socket options
 */
/* 
 * Modification History
 *
 * February 1, 2011		dX (wxie@apple.com)
 * - initial version
 */

#ifndef _S_IPV6SOCKCOMPAT_H
#define _S_IPV6SOCKCOMPAT_H

#include <netinet/in.h>

#ifdef IPV6_RECVPKTINFO
#define IPCONFIG_SOCKOPT_PKTINFO IPV6_RECVPKTINFO
#else
#define IPCONFIG_SOCKOPT_PKTINFO IPV6_PKTINFO
#endif

#ifdef IPV6_RECVHOPLIMIT
#define IPCONFIG_SOCKOPT_HOPLIMIT IPV6_RECVHOPLIMIT
#else
#define IPCONFIG_SOCKOPT_HOPLIMIT IPV6_HOPLIMIT 
#endif


#endif /* _S_IPV6SOCKCOMPAT_H */
