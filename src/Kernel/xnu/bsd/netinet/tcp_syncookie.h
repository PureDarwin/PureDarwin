/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET_TCP_SYNCOOKIE_H_
#define _NETINET_TCP_SYNCOOKIE_H_

#include <netinet/tcp_var.h>
#include <sys/types.h>

#ifdef KERNEL_PRIVATE

void tcp_syncookie_init(void);
void tcp_syncookie_syn(struct tcp_inp *tpi, struct sockaddr *local, struct sockaddr *remote);
bool tcp_syncookie_ack(struct tcp_inp *tpi, struct socket **so2, int* dropsocket);

/*
 * Flags for the Accurate ECN setup
 */
#define SC_ECN_SETUP            0x01  /* send classic ECN setup */
#define SC_ACE_SETUP_NOT_ECT    0x02  /* send ACE not-ECT setup */
#define SC_ACE_SETUP_ECT1       0x04  /* send ACE ECT1 setup */
#define SC_ACE_SETUP_ECT0       0x08  /* send ACE ECT0 setup */
#define SC_ACE_SETUP_CE         0x10  /* send ACE CE setup */


#define SYNCOOKIE_SECRET_SIZE   16
#define SYNCOOKIE_LIFETIME      15              /* seconds */

struct syncookie_secret {
	volatile u_int oddeven;
	uint8_t key[2][SYNCOOKIE_SECRET_SIZE];
	uint32_t last_updated;
};

typedef union {
	uint8_t cookie;
	struct {
		uint8_t odd_even:1,
		    sack_ok:1,
		    ecn_ok:1,              /* Only needed for classic ECN */
		    wscale_idx:3,
		    mss_idx:2;
	} flags;
} syncookie;
#endif /* KERNEL_PRIVATE */

#endif /* _NETINET_TCP_SYNCOOKIE_H_ */
