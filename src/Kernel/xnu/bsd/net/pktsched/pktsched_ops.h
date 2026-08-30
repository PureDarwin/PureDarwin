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

#ifndef _PKTSCHED_PKTSCHED_OPS_H_
#define _PKTSCHED_PKTSCHED_OPS_H_

#ifdef PRIVATE
#ifdef __cplusplus
extern "C" {
#endif

#include <net/classq/if_classq.h>

typedef int (*pktsched_setup_t)(struct ifclassq *ifcq, u_int32_t flags,
    classq_pkt_type_t ptype);
typedef void (*pktsched_teardown_t)(struct ifclassq *ifcq);
typedef int (*pktsched_request_t)(struct ifclassq *ifcq, enum cqrq, void *arg);
typedef boolean_t (*pktsched_allow_dequeue_t)(struct ifclassq *ifcq);
typedef int (*pktsched_stats_t)(struct ifclassq *ifcq, uint8_t gid,
    u_int32_t qid, struct if_ifclassq_stats *ifqs);
typedef int (*pktsched_enq_t)(struct ifclassq *ifq, classq_pkt_t *head,
    classq_pkt_t *tail, uint32_t cnt, uint32_t bytes, boolean_t *pdrop);
typedef int  (*pktsched_deq_t)(struct ifclassq *ifq, u_int32_t maxpktcnt,
    u_int32_t maxbytecnt, classq_pkt_t *first_packet, classq_pkt_t *last_packet,
    u_int32_t *retpktcnt, u_int32_t *retbytecnt, uint8_t grp_idx);
typedef int (*pktsched_deq_sc_t)(struct ifclassq *ifq, mbuf_svc_class_t svc,
    u_int32_t maxpktcnt, u_int32_t maxbytecnt, classq_pkt_t *first_packet,
    classq_pkt_t *last_packet, u_int32_t *retpktcnt, u_int32_t *retbytecnt,
    uint8_t grp_idx);

typedef struct pktsched_ops {
	uint8_t                         ps_id;
#define PKTSCHED_OPS_LOCKLESS    0x1
	uint8_t                         ps_ops_flags;
	pktsched_setup_t                ps_setup;
	pktsched_teardown_t             ps_teardown;
	pktsched_enq_t                  ps_enq;
	pktsched_deq_t                  ps_deq;
	pktsched_deq_sc_t               ps_deq_sc;
	pktsched_request_t              ps_req;
	pktsched_stats_t                ps_stats;
	pktsched_allow_dequeue_t        ps_allow_dequeue;
	LIST_ENTRY(pktsched_ops)        ps_ops_link;
}pktsched_ops_t;

typedef LIST_HEAD(, pktsched_ops) pktsched_ops_list_t;

void
pktsched_ops_register(pktsched_ops_t *new_ops);

pktsched_ops_t *
pktsched_ops_find(uint8_t ps_id);

#ifdef __cplusplus
}
#endif
#endif /* PRIVATE */
#endif /* _PKTSCHED_PKTSCHED_OPS_H_ */
