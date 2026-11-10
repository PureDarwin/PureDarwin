/*
 * Copyright (c) 2008-2020 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

#include <TargetConditionals.h>

#include "network_cmds_lib.h"

struct netstat_parameters {
	int	Aflag;		/* show addresses of protocol control block */
	int	aflag;		/* show all sockets (including servers) */
	int	Bflag;		/* show information about BPF */
	int	bflag;		/* show i/f total bytes in/out */
	int	cflag;		/* show specific classq */
	int	dflag;		/* show i/f dropped packets */
	int	Fflag;		/* show i/f forwarded packets */
	int	gflag;		/* show group (multicast) routing or stats */
	int	iflag;		/* show interfaces */
	int	lflag;		/* show routing table with use and ref */
	int	Lflag;		/* show size of listen queues */
	int	mflag;		/* show memory stats */
	int	nflag;		/* show addresses numerically */
	int	pflag;		/* show given protocol */
	int	Rflag;		/* show reachability information */
	int	rflag;		/* show routing tables (or routing stats) */
	int	sflag;		/* show protocol statistics */
	int	Sflag;		/* show additional i/f link status */
	int	prioflag;	/* show packet priority  statistics */
	int	tflag;		/* show i/f watchdog timers */
	int	vflag;		/* more verbose */
	int	Wflag;		/* wide display */
	int	qflag;		/* Display ifclassq stats */
	int	Qflag;		/* Display opportunistic polling stats */
	int	xflag;		/* show extended link-layer reachability information */
	int	zflag;		/* show only entries with non zero rtt metrics */

	int	cq;			/* send classq index (-1 for all) */
	int	interval;	/* repeat interval for i/f stats */

	char *interface; /* desired i/f for stats, or NULL for all i/fs */
	int	unit;		/* unit number for above */

	int	af;			/* address family */

	char proto_name[32]; /* protocol name */

	char errbuf[256]; /* error buffer */

	int print_banner;
	char cmd_args[256];
	size_t cmd_len;
};

extern char	*plural(int);
extern char	*plurales(int);
extern char	*pluralies(int);

extern int	protopr(struct netstat_parameters *, uint32_t, char *, int);
extern int	mptcppr(struct netstat_parameters *, uint32_t, char *, int);
extern int	tcp_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	mptcp_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	udp_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	ip_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	icmp_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	igmp_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	arp_stats(struct netstat_parameters *, uint32_t, char *, int);
#ifdef IPSEC
extern int	ipsec_stats(struct netstat_parameters *, uint32_t, char *, int);
#endif

extern int tcp_ifstats(struct netstat_parameters *, char *);
extern int udp_ifstats(struct netstat_parameters *, char *);

extern int tcp_reinit(struct netstat_parameters *, uint32_t, char *, int);
extern int udp_reinit(struct netstat_parameters *, uint32_t, char *, int);
extern int mptcp_reinit(struct netstat_parameters *, uint32_t, char *, int);

struct in_addr;
void in_addr_print(struct netstat_parameters *params, struct in_addr *in);

#ifdef INET6
extern int	ip6_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	ip6_ifstats(struct netstat_parameters *, char *);
extern int	icmp6_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	icmp6_ifstats(struct netstat_parameters *, char *);
extern int	rip6_stats(struct netstat_parameters *, uint32_t, char *, int);

/* forward references */
struct sockaddr_in6;
struct in6_addr;
struct sockaddr;

extern char	*routename6(struct netstat_parameters *, struct sockaddr_in6 *);
extern char	*netname6(struct netstat_parameters *, struct sockaddr_in6 *, struct sockaddr *);

void in6_addr_print(struct netstat_parameters *params, struct in6_addr *in6);
#endif /*INET6*/

#ifdef IPSEC
extern int	pfkey_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int keysockpr(struct netstat_parameters *, uint32_t, char *, int);
#endif


extern int	systmpr(struct netstat_parameters *, uint32_t, char *, int);
extern int	kctl_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	kevt_stats(struct netstat_parameters *, uint32_t, char *, int);

extern int	mbpr(struct netstat_parameters *);

extern int	intpr(struct netstat_parameters *, void (*)(struct netstat_parameters *, char *));
extern int	intpr_ri(struct netstat_parameters *, void (*)(struct netstat_parameters *, char *));
extern void	intervalpr(struct netstat_parameters *, int (*)(struct netstat_parameters *, uint32_t, char *, int), uint32_t,
		    char *, int);

extern void	pr_rthdr(struct netstat_parameters *, int);
extern void	pr_family(int);
extern void	rt_stats(struct netstat_parameters *);
extern void	upHex(char *);
extern char	*routename(struct netstat_parameters *, uint32_t);
extern char	*netname(struct netstat_parameters *, uint32_t, uint32_t);
extern int	routepr(struct netstat_parameters *);
extern int	rtsock_pcblist(struct netstat_parameters *, uint32_t, char *, int);

extern int	unixpr(struct netstat_parameters *, uint32_t, char *, int);
extern int	unixstats(struct netstat_parameters *, uint32_t, char *, int);
extern int	aqstatpr(struct netstat_parameters *);
extern int	rxpollstatpr(struct netstat_parameters *);
extern int	vsockpr(struct netstat_parameters *, uint32_t, char *, int);
extern int	vsockstats(struct netstat_parameters *, uint32_t, char *, int);

extern int	ifmalist_dump(struct netstat_parameters *);

extern int print_time(void);
extern void	print_link_status(struct netstat_parameters *, const char *);

extern int	print_extbkidle_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	print_nstat_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	print_net_api_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	print_if_ports_used_stats(struct netstat_parameters *, uint32_t, char *, int);
extern int	print_if_link_heuristics_stats(struct netstat_parameters *, char *);
extern int	print_if_lpw_stats(struct netstat_parameters *, char *);

extern int bpf_stats(struct netstat_parameters *, char *);
extern void bpf_help(void);

extern void print_socket_stats_format(struct netstat_parameters *);

struct xsocket_n;
struct xsockbuf_n;
struct xsockstat_n;
extern void print_socket_stats_data(struct netstat_parameters *, struct xsocket_n *, struct xsockbuf_n *, struct xsockbuf_n *, struct xsockstat_n *);

extern void printprotoifstats(struct netstat_parameters *, char *ifname);
