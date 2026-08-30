/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#ifndef _NET_AOP_FLOW_STATS_H_
#define _NET_AOP_FLOW_STATS_H_

#include <net/ntstat.h>
#include <netinet/tcp_private.h>

typedef struct aop_buffer {
	uint32_t bufsize;       /* Transport buffer size */
	uint32_t bufused;       /* Transport buffer used count */
} aop_buffer_t;

typedef struct aop_tcp_info {
	struct tcp_info tcp_info;       /* TCP information */
	uint8_t tcp_cc_algo;            /* TCP congestion control algo from tcp_cc.h */
} aop_tcp_info_t;

struct aop_flow_stats {
	uint32_t flow_id;                       /* Flow ID */
	uint32_t reserved;
	uint64_t rxbytes;                       /* Total Rx bytes */
	uint64_t txbytes;                       /* Total Tx bytes */
	uint64_t rxpkts;                        /* Total Rx packets */
	uint64_t txpkts;                        /* Total Tx packets */
	aop_buffer_t tx_buffer_stats;           /* Transport Tx buffer stats */
	aop_buffer_t rx_buffer_stats;           /* Transport Rx buffer stats */
	activity_bitmap_t activity_bitmap;      /* Activity bitmap */
	union {
		aop_tcp_info_t tcp_stats;       /* TCP stats */
	} transport;
};

#endif /* _NET_AOP_FLOW_STATS_H_ */
