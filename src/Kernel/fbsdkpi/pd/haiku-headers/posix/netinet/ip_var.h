/*
 * Copyright (c) 1982, 1986, 1993
 *      The Regents of the University of California.  All rights reserved.
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

#ifndef NETINET_IP_VAR_H
#define NETINET_IP_VAR_H

#include <netinet/in.h>
#include <sys/socket.h>

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly {
	char *   ih_next;
	char *   ih_prev;
	uint8_t   ih_x1;        /* (unused) */
	uint8_t   ih_pr;           /* protocol */
	uint16_t  ih_len;          /* protocol length */
	struct    in_addr ih_src;  /* source internet address */
	struct    in_addr ih_dst;  /* destination internet address */
};

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN    40

struct ipoption {
	struct  in_addr ipopt_dst;         /* first-hop dst if source routed */
	int8_t  ipopt_list[MAX_IPOPTLEN];  /* options proper */
};

/*
 * Structure attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 */
struct ip_moptions {
	struct    ifnet *imo_multicast_ifp; /* ifp for outgoing multicasts */
	uint8_t  imo_multicast_ttl;           /* TTL for outgoing multicasts */
	uint8_t  imo_multicast_loop;          /* 1 => here sends if a member */
	uint16_t imo_num_memberships;         /* no. memberships this socket */
	struct    in_multi *imo_membership[IP_MAX_MEMBERSHIPS];
};

struct ipasfrag {
#if B_HOST_IS_BENDIAN
	uint8_t  ip_v:4;
	uint8_t  ip_hl:4;
#else
	uint8_t  ip_hl:4;
	uint8_t  ip_v:4;
#endif
	uint8_t  ipf_mff;
	int16_t  ip_len;
	uint16_t ip_id;
	int16_t  ip_off;
	uint8_t  ip_ttl;
	uint8_t  ip_p;
	struct ipasfrag *ipf_next;
	struct ipasfrag *ipf_prev;
};

struct ipq {
	struct ipq *next, *prev;
	uint8_t  ipq_ttl;
	uint8_t  ipq_p;
	uint16_t ipq_id;
	struct ipasfrag *ipq_next, *ipq_prev;
	struct in_addr ipq_src, ipq_dst;
};

struct  ipstat {
	int32_t  ips_total;              /* total packets received */
	int32_t  ips_badsum;             /* checksum bad */
	int32_t  ips_tooshort;           /* packet too short */
	int32_t  ips_toosmall;           /* not enough data */
	int32_t  ips_badhlen;            /* ip header length < data size */
	int32_t  ips_badlen;             /* ip length < ip header length */
	int32_t  ips_fragments;          /* fragments received */
	int32_t  ips_fragdropped;        /* frags dropped (dups, out of space) */
	int32_t  ips_fragtimeout;        /* fragments timed out */
	int32_t  ips_forward;            /* packets forwarded */
	int32_t  ips_cantforward;        /* packets rcvd for unreachable dest */
	int32_t  ips_redirectsent;       /* packets forwarded on same net */
	int32_t  ips_noproto;            /* unknown or unsupported protocol */
	int32_t  ips_delivered;          /* datagrams delivered to upper level*/
	int32_t  ips_localout;           /* total ip packets generated here */
	int32_t  ips_odropped;           /* lost packets due to nobufs, etc. */
	int32_t  ips_reassembled;        /* total packets reassembled ok */
	int32_t  ips_fragmented;         /* datagrams sucessfully fragmented */
	int32_t  ips_ofragments;         /* output fragments created */
	int32_t  ips_cantfrag;           /* don't fragment flag was set, etc. */
	int32_t  ips_badoptions;         /* error in option processing */
	int32_t  ips_noroute;            /* packets discarded due to no route */
	int32_t  ips_badvers;            /* ip version != 4 */
	int32_t  ips_rawout;             /* total raw ip packets generated */
	int32_t  ips_badfrags;           /* malformed fragments (bad length) */
	int32_t  ips_rcvmemdrop;         /* frags dropped for lack of memory */
	int32_t  ips_toolong;            /* ip length > max ip packet size */
	int32_t  ips_nogif;              /* no match gif found */
	int32_t  ips_badaddr;            /* invalid address on header */
	int32_t  ips_inhwcsum;           /* hardware checksummed on input */
	int32_t  ips_outhwcsum;          /* hardware checksummed on output */
};

#endif /* NETINET_IP_VAR_H */
