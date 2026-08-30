/*
 * Copyright (c) 2015-2025 Apple Inc. All rights reserved.
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

/* TCP-cache to store and retrieve TCP-related information */

#ifndef _NETINET_TCP_CACHE_H
#define _NETINET_TCP_CACHE_H

#include <net/if_private.h>
#include <netinet/tcp_var.h>
#include <netinet/in.h>

#ifdef PRIVATE

#define TCP_HEURISTICS_LIST_ENTITLEMENT "com.apple.private.tcp.heuristics_list"
#define TCP_CACHE_LIST_ENTITLEMENT "com.apple.private.tcp.cache_list"

typedef union {
	struct in_addr addr;
	struct in6_addr addr6;
} in_4_6_addr;

struct tcp_heuristic_key {
	union {
		uint8_t thk_net_signature[IFNET_SIGNATURELEN];
		in_4_6_addr thk_ip;
	};
	sa_family_t     thk_family;
};

/* Data structure for sysctl export - same as tcp_heuristic but without list field */
struct tcp_heuristics_data {
	uint32_t        th_last_access;

	struct tcp_heuristic_key        th_key;

	uint8_t         th_tfo_data_loss; /* The number of times a SYN+data has been lost */
	uint8_t         th_tfo_req_loss; /* The number of times a SYN+cookie-req has been lost */
	uint8_t         th_tfo_data_rst; /* The number of times a SYN+data has received a RST */
	uint8_t         th_tfo_req_rst; /* The number of times a SYN+cookie-req has received a RST */
	uint8_t         th_mptcp_loss; /* The number of times a SYN+MP_CAPABLE has been lost */
	uint8_t         th_mptcp_success; /* The number of times MPTCP-negotiation has been successful */
	uint8_t         th_ecn_droprst; /* The number of times ECN connections received a RST after first data pkt */
	uint8_t         th_ecn_synrst;  /* number of times RST was received in response to an ECN enabled SYN */
	uint32_t        th_tfo_enabled_time; /* The moment when we reenabled TFO after backing off */
	uint32_t        th_tfo_backoff_until; /* Time until when we should not try out TFO */
	uint32_t        th_tfo_backoff; /* Current backoff timer */
	uint32_t        th_mptcp_backoff; /* Time until when we should not try out MPTCP */
	uint32_t        th_ecn_backoff; /* Time until when we should not try out ECN */

	uint8_t         th_tfo_in_backoff:1, /* Are we avoiding TFO due to the backoff timer? */
	    th_mptcp_in_backoff:1,             /* Are we avoiding MPTCP due to the backoff timer? */
	    th_mptcp_heuristic_disabled:1;             /* Are heuristics disabled? */
};

struct tcp_cache_key {
	sa_family_t     tck_family;

	struct tcp_heuristic_key tck_src;
	in_4_6_addr tck_dst;
};

#define TFO_COOKIE_LEN_MAX      16

/* Data structure for sysctl export - same as tcp_cache but without list field */
struct tcp_cache_data {
	uint32_t       tc_last_access;

	struct tcp_cache_key tc_key;

	uint8_t        tc_tfo_cookie[TFO_COOKIE_LEN_MAX];
	uint8_t        tc_tfo_cookie_len;

	uint8_t        tc_mptcp_version_confirmed:1;
	uint8_t        tc_mptcp_version; /* version to use right now */
	uint32_t       tc_mptcp_next_version_try; /* Time, until we try preferred version again */
};

#define MPTCP_VERSION_SUPPORTED 1
#define MPTCP_VERSION_UNSUPPORTED -1
#define MPTCP_VERSION_SUPPORTED_UNKNOWN 0

#endif /* PRIVATE */

#ifdef KERNEL_PRIVATE

#define ECN_MIN_CE_PROBES       (20) /* Probes are basically the number of incoming packets */
#define ECN_MAX_CE_RATIO        (18) /* Ratio is the maximum number of E/CE-packets we accept per incoming "probe" */

extern void tcp_cache_set_cookie(struct tcpcb *tp, u_char *__counted_by(len) cookie, u_int8_t len);
extern int tcp_cache_get_cookie(struct tcpcb *tp, u_char *__counted_by(buflen) cookie, uint8_t buflen, u_int8_t *len);
extern unsigned int tcp_cache_get_cookie_len(struct tcpcb *tp);
extern uint8_t tcp_cache_get_mptcp_version(struct sockaddr* dst);
extern void tcp_cache_update_mptcp_version(struct tcpcb *tp, boolean_t succeeded);

extern void tcp_heuristic_tfo_loss(struct tcpcb *tp);
extern void tcp_heuristic_tfo_rst(struct tcpcb *tp);
extern void tcp_heuristic_mptcp_loss(struct tcpcb *tp);
extern void tcp_heuristic_ecn_loss(struct tcpcb *tp);
extern void tcp_heuristic_ecn_aggressive(struct tcpcb *tp);
extern void tcp_heuristic_tfo_middlebox(struct tcpcb *tp);
extern void tcp_heuristic_tfo_success(struct tcpcb *tp);
extern void tcp_heuristic_mptcp_success(struct tcpcb *tp);
extern void tcp_heuristic_ecn_success(struct tcpcb *tp);
extern boolean_t tcp_heuristic_do_tfo(struct tcpcb *tp);
extern int tcp_heuristic_do_mptcp(struct tcpcb *tp);
extern boolean_t tcp_heuristic_do_ecn(struct tcpcb *tp);
extern void tcp_heuristic_ecn_droprst(struct tcpcb *tp);
extern void tcp_heuristic_ecn_synrst(struct tcpcb *tp);

extern boolean_t tcp_heuristic_do_ecn_with_address(struct ifnet *ifp,
    union sockaddr_in_4_6 *local_address);
extern void tcp_heuristics_ecn_update(struct necp_tcp_ecn_cache *necp_buffer,
    struct ifnet *ifp, union sockaddr_in_4_6 *local_address);
extern boolean_t tcp_heuristic_do_tfo_with_address(struct ifnet *ifp,
    union sockaddr_in_4_6 *local_address, union sockaddr_in_4_6 *remote_address,
    u_int8_t *__counted_by(maxlen) cookie, u_int8_t maxlen, u_int8_t *cookie_len);
extern void tcp_heuristics_tfo_update(struct necp_tcp_tfo_cache *necp_buffer,
    struct ifnet *ifp, union sockaddr_in_4_6 *local_address,
    union sockaddr_in_4_6 *remote_address);

extern void tcp_cache_init(void);

#endif /* KERNEL_PRIVATE */
#endif /* _NETINET_TCP_CACHE_H */
