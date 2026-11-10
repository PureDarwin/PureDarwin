/*
 * Copyright (c) 2008-2019 Apple Inc. All rights reserved.
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
 * Copyright (c) 1983, 1988, 1993
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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/kern_control.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_mib.h>
#include <net/if_llreach.h>
#include <net/ethernet.h>
#include <net/route.h>
#include <net/ntstat.h>
#include <net/pktsched/pktsched_fq_codel.h>

#include <skywalk/os_channel.h>

#include <net/pktsched/pktsched.h>
#include <net/classq/if_classq.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>

#include <assert.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

#define ROUNDUP(a, size) (((a) & ((size) - 1)) ? (1 + ((a)|(size - 1))) : (a))

#define NEXT_SA(p) (struct sockaddr *) \
    ((caddr_t)p + (p->sa_len ? ROUNDUP(p->sa_len, sizeof(uint32_t)) : \
    sizeof(uint32_t)))

#ifndef FQ_IF_STATS_MAX_GROUPS
#define FQ_IF_STATS_MAX_GROUPS 16
#endif

static void sidewaysintpr ();
static void catchalarm (int);
static char *sec2str(time_t);
static int llreach_sysctl(struct netstat_parameters *, uint32_t);
static char *nsec_to_str(unsigned long long);
static char *sched2str(unsigned int);
static char *pri2str(unsigned int i);

static void print_fq_codel_stats(struct netstat_parameters *, int slot, struct fq_codel_classstats *, uint8_t);

#ifdef INET6
static char ntop_buf[INET6_ADDRSTRLEN];		/* for inet_ntop() */
#endif

/*
 * Display a formatted value, or a '-' in the same space.
 */
#define show_stat(fmt, width,  value, showvalue) do { \
	if (showvalue) { \
		printf("%*" fmt, width, value); \
	} else { \
		printf("%*s", width, "-"); \
	} \
} while (0)

size_t
get_rti_info(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
    int			i;
    size_t		len = 0;

    for (i = 0; i < RTAX_MAX; i++) {
        if (addrs & (1 << i)) {
            rti_info[i] = sa;
            if (sa->sa_len < sizeof(struct sockaddr))
                len += sizeof(struct sockaddr);
            else
                len += sa->sa_len;
            sa = NEXT_SA(sa);
        } else {
            rti_info[i] = NULL;
        }
    }
    return len;
}

static void
multipr(struct netstat_parameters *params, int family, char *buf, char *lim)
{
    char  *next;

    for (next = buf; next < lim; ) {
		struct ifma_msghdr2	*ifmam = (struct ifma_msghdr2 *)next;
		struct sockaddr *rti_info[RTAX_MAX];
		struct sockaddr *sa;
		const char *fmt = 0;

		next += ifmam->ifmam_msglen;
		if (ifmam->ifmam_type == RTM_IFINFO2)
			break;
		else if (ifmam->ifmam_type != RTM_NEWMADDR2)
			continue;
		get_rti_info(ifmam->ifmam_addrs, (struct sockaddr*)(ifmam + 1), rti_info);
		sa = rti_info[RTAX_IFA];

		if (sa->sa_family != family)
			continue;
		switch (sa->sa_family) {
			case AF_INET: {
				struct sockaddr_in *sin = (struct sockaddr_in *)sa;

				fmt = routename(params, sin->sin_addr.s_addr);
				break;
			}
	#ifdef INET6
			case AF_INET6: {
				struct sockaddr_in6 sin6;

				memcpy(&sin6, sa, sizeof(struct sockaddr_in6));

				if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
					IN6_IS_ADDR_MC_NODELOCAL(&sin6.sin6_addr) ||
					IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr)) {
					sin6.sin6_scope_id = ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
					sin6.sin6_addr.s6_addr[2] = 0;
					sin6.sin6_addr.s6_addr[3] = 0;
				}

				printf("%23s %-19.19s(refs: %d)\n", "",
				    inet_ntop(AF_INET6, &sin6.sin6_addr,
				    ntop_buf, sizeof(ntop_buf)),
						ifmam->ifmam_refcount);
				break;
			}
	#endif /* INET6 */
			case AF_LINK: {
				struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

				switch (sdl->sdl_type) {
				case IFT_ETHER:
				case IFT_FDDI:
					fmt = ether_ntoa((struct ether_addr *)
						LLADDR(sdl));
					break;
				}
				break;
			}
		}
		if (fmt)
			printf("%23s %s\n", "", fmt);
	}
}

static void
print_intpr_header(struct netstat_parameters *params, bool first)
{
		if (first == false ) {
			putchar('\n');
		}

		if (params->lflag) {
			printf("%-14.14s %-5.5s %-39.39s %-39.39s %8.8s %5.5s",
				   "Name", "Mtu", "Network", "Address", "Ipkts", "Ierrs");
		} else {
			printf("%-10.10s %-5.5s %-13.13s %-15.15s %8.8s %5.5s",
				   "Name", "Mtu", "Network", "Address", "Ipkts", "Ierrs");
		}
		if (params->prioflag >= 0)
			printf(" %8.8s %8.8s", "Itcpkts", "Ipvpkts");
		if (params->bflag) {
			printf(" %10.10s","Ibytes");
			if (params->prioflag >= 0)
				printf(" %8.8s %8.8s", "Itcbytes", "Ipvbytes");
		}
		printf(" %8.8s %5.5s", "Opkts", "Oerrs");
		if (params->prioflag >= 0)
			printf(" %8.8s %8.8s", "Otcpkts", "Opvpkts");
		if (params->bflag) {
			printf(" %10.10s","Obytes");
			if (params->prioflag >= 0)
				printf(" %8.8s %8.8s", "Otcbytes", "Opvbytes");
		}
		printf(" %5s", "Coll");
		if (params->tflag)
			printf(" %s", "Time");
		if (params->dflag)
			printf(" %s", "Drop");
		if (params->Fflag) {
			printf(" %8.8s", "Fpkts");
			if (params->bflag)
				printf(" %10.10s", "Fbytes");
		}
		putchar('\n');
}

/*
 * Print a description of the network interfaces.
 */
int
intpr(struct netstat_parameters *params, void (*pfunc)(struct netstat_parameters *, char *))
{
	u_int64_t opackets = 0;
	u_int64_t ipackets = 0;
	u_int64_t obytes = 0;
	u_int64_t ibytes = 0;
	u_int64_t oerrors = 0;
	u_int64_t ierrors = 0;
	u_int64_t collisions = 0;
	u_int64_t fpackets = 0;
	u_int64_t fbytes = 0;
	uint32_t mtu = 0;
	int timer = 0;
	int drops = 0;
	struct sockaddr *sa = NULL;
	char name[32];
	short network_layer;
	short link_layer;
	int mib[6];
	char *buf = NULL, *lim, *next;
	size_t len;
	struct if_msghdr *ifm;
	struct sockaddr *rti_info[RTAX_MAX];
	unsigned int ifindex = 0;
	char last_ifname[32] = { 0 };
	int retval = 0;

	if (params->interval) {
		sidewaysintpr(params);
		goto done;
	}

	if (params->interface != 0)
		ifindex = if_nametoindex(params->interface);

	mib[0]	= CTL_NET;			// networking subsystem
	mib[1]	= PF_ROUTE;			// type of information
	mib[2]	= 0;				// protocol (IPPROTO_xxx)
	mib[3]	= 0;				// address family
	mib[4]	= NET_RT_IFLIST2;	// operation
	mib[5]	= 0;
	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
		goto done;
	}
	if ((buf = malloc(len)) == NULL) {
		snprintf(params->errbuf, sizeof(params->errbuf), "malloc failed");
		retval = -1;
		goto done;
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		goto done;
	}

	print_intpr_header(params, true);

	lim = buf + len;
	for (next = buf; next < lim; ) {
		char *cp;
		int n, m;
		struct ifmibdata_supplemental ifmsupp;
		u_int64_t	ift_itcp = 0;	/* input tc packets */
		u_int64_t	ift_itcb = 0;	/* input tc bytes */
		u_int64_t	ift_otcp = 0;	/* output tc packets */
		u_int64_t	ift_otcb = 0;	/* output tc bytes */
		u_int64_t	ift_ipvp = 0;	/* input priv tc packets */
		u_int64_t	ift_ipvb = 0;	/* input priv tc bytes */
		u_int64_t	ift_opvp = 0;	/* output priv tc packets */
		u_int64_t	ift_opvb = 0;	/* output priv tc bytes */

		bzero(&ifmsupp, sizeof(struct ifmibdata_supplemental));

		network_layer = 0;
		link_layer = 0;
		ifm = (struct if_msghdr *)next;
		next += ifm->ifm_msglen;

		if (ifm->ifm_type == RTM_IFINFO2) {
			struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
			struct sockaddr_dl *sdl =
			    (struct sockaddr_dl *)(if2m + 1);
			int mibname[6];
			size_t miblen = sizeof(struct ifmibdata_supplemental);

			if (params->interface != NULL && if2m->ifm_index != ifindex)
				continue;

			/* The interface name is not a zero-ended string */
			memcpy(name, sdl->sdl_data, MIN(sizeof(name) - 1, sdl->sdl_nlen));
			name[MIN(sizeof(name) - 1, sdl->sdl_nlen)] = 0;

			cp = index(name, '\0');
			if ((if2m->ifm_flags & IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';

			/*
			 * Get the interface stats.  These may get
			 * overriden below on a per interface basis.
			 */
			opackets = if2m->ifm_data.ifi_opackets;
			ipackets = if2m->ifm_data.ifi_ipackets;
			obytes = if2m->ifm_data.ifi_obytes;
			ibytes = if2m->ifm_data.ifi_ibytes;
			oerrors =if2m->ifm_data.ifi_oerrors;
			ierrors = if2m->ifm_data.ifi_ierrors;
			collisions = if2m->ifm_data.ifi_collisions;
			timer = if2m->ifm_timer;
			drops = if2m->ifm_snd_drops;
			mtu = if2m->ifm_data.ifi_mtu;

			/* Common OID prefix */
			mibname[0] = CTL_NET;
			mibname[1] = PF_LINK;
			mibname[2] = NETLINK_GENERIC;
			mibname[3] = IFMIB_IFDATA;
			mibname[4] = if2m->ifm_index;
			mibname[5] = IFDATA_SUPPLEMENTAL;
			if (sysctl(mibname, 6, &ifmsupp, &miblen, NULL, 0) == -1) {
				warn("sysctl IFDATA_SUPPLEMENTAL");
				retval = -1;
				goto done;
			}
			fpackets = ifmsupp.ifmd_data_extended.ifi_fpackets;
			fbytes = ifmsupp.ifmd_data_extended.ifi_fbytes;

			if (params->prioflag >= 0) {
				switch (params->prioflag) {
				case SO_TC_BE:
					ift_itcp = ifmsupp.ifmd_traffic_class.ifi_ibepackets;
					ift_itcb = ifmsupp.ifmd_traffic_class.ifi_ibebytes;
					ift_otcp = ifmsupp.ifmd_traffic_class.ifi_obepackets;
					ift_otcb = ifmsupp.ifmd_traffic_class.ifi_obebytes;
					break;
				case SO_TC_BK:
					ift_itcp = ifmsupp.ifmd_traffic_class.ifi_ibkpackets;
					ift_itcb = ifmsupp.ifmd_traffic_class.ifi_ibkbytes;
					ift_otcp = ifmsupp.ifmd_traffic_class.ifi_obkpackets;
					ift_otcb = ifmsupp.ifmd_traffic_class.ifi_obkbytes;
					break;
				case SO_TC_VI:
					ift_itcp = ifmsupp.ifmd_traffic_class.ifi_ivipackets;
					ift_itcb = ifmsupp.ifmd_traffic_class.ifi_ivibytes;
					ift_otcp = ifmsupp.ifmd_traffic_class.ifi_ovipackets;
					ift_otcb = ifmsupp.ifmd_traffic_class.ifi_ovibytes;
					break;
				case SO_TC_VO:
					ift_itcp = ifmsupp.ifmd_traffic_class.ifi_ivopackets;
					ift_itcb = ifmsupp.ifmd_traffic_class.ifi_ivobytes;
					ift_otcp = ifmsupp.ifmd_traffic_class.ifi_ovopackets;
					ift_otcb = ifmsupp.ifmd_traffic_class.ifi_ovobytes;
					break;
				default:
					ift_itcp = 0;
					ift_itcb = 0;
					ift_otcp = 0;
					ift_otcb = 0;
					ift_ipvp = 0;
					ift_ipvb = 0;
					ift_opvp = 0;
					ift_opvb = 0;
					break;
				}
				ift_ipvp = ifmsupp.ifmd_traffic_class.ifi_ipvpackets;
				ift_ipvb = ifmsupp.ifmd_traffic_class.ifi_ipvbytes;
				ift_opvp = ifmsupp.ifmd_traffic_class.ifi_opvpackets;
				ift_opvb = ifmsupp.ifmd_traffic_class.ifi_opvbytes;
			}

			get_rti_info(if2m->ifm_addrs,
			    (struct sockaddr*)(if2m + 1), rti_info);
			sa = rti_info[RTAX_IFP];


		} else if (ifm->ifm_type == RTM_NEWADDR) {
			struct ifa_msghdr *ifam = (struct ifa_msghdr *)ifm;

			if (params->interface != NULL && ifam->ifam_index != ifindex)
				continue;
			get_rti_info(ifam->ifam_addrs,
			    (struct sockaddr*)(ifam + 1), rti_info);
			sa = rti_info[RTAX_IFA];
		} else {
			continue;
		}

		/*
		 * When printing all the interfaces:
		 * - print the per inteface protocol stats of the previous interface
		 * - print the header again for the current interface
		 */
		if (strcmp(last_ifname, name) != 0) {
			if (params->sflag && last_ifname[0] != 0) {
				printprotoifstats(params, last_ifname);

				print_intpr_header(params, false);
			}
			strncpy(last_ifname, name, sizeof(last_ifname));
		}

		if (params->lflag) {
			printf("%-14.14s %-5u ", name, mtu);
		} else {
			printf("%-10.10s %-5u ", name, mtu);
		}

		if (sa == 0) {
			printf(params->lflag ? "%-39.39s " : "%-13.13s ", "none");
			printf(params->lflag ? "%-39.39s " : "%-15.15s ", "none");
		} else {
			switch (sa->sa_family) {
			case AF_UNSPEC:
				printf(params->lflag ? "%-39.39s " : "%-13.13s ", "none");
				printf(params->lflag ? "%-39.39s " : "%-15.15s ", "none");
				break;

			case AF_INET: {
				struct sockaddr_in *sin =
				    (struct sockaddr_in *)sa;
				struct sockaddr_in mask;

				mask.sin_addr.s_addr = 0;
				memcpy(&mask, rti_info[RTAX_NETMASK],
				    ((struct sockaddr_in *)
				    rti_info[RTAX_NETMASK])->sin_len);

				printf(params->lflag ? "%-39.39s " : "%-13.13s ",
				    netname(params, sin->sin_addr.s_addr &
				    mask.sin_addr.s_addr,
				    ntohl(mask.sin_addr.s_addr)));

				printf(params->lflag ? "%-39.39s " : "%-15.15s ",
				    routename(params, sin->sin_addr.s_addr));

				network_layer = 1;
				break;
			}
#ifdef INET6
			case AF_INET6: {
				struct sockaddr_in6 *sin6 =
				    (struct sockaddr_in6 *)sa;
				struct sockaddr *mask =
				    (struct sockaddr *)rti_info[RTAX_NETMASK];

				printf(params->lflag ? "%-39.39s " : "%-11.11s ", netname6(params, sin6, mask));
				printf(params->lflag ? "%-39.39s " : "%-17.17s ", (char *)inet_ntop(AF_INET6,
				    &sin6->sin6_addr, ntop_buf,
				    sizeof(ntop_buf)));

				network_layer = 1;
				break;
			}
#endif /*INET6*/
			case AF_LINK: {
				struct sockaddr_dl *sdl =
				    (struct sockaddr_dl *)sa;
				char linknum[10];
				cp = (char *)LLADDR(sdl);
				n = sdl->sdl_alen;
				snprintf(linknum, sizeof(linknum),
				    "<Link#%d>", sdl->sdl_index);
				m = printf(params->lflag ? "%-39.39s " : "%-11.11s ", linknum);
				goto hexprint;
			}

			default:
				m = printf("(%d)", sa->sa_family);
				for (cp = sa->sa_len + (char *)sa;
					--cp > sa->sa_data && (*cp == 0);) {}
				n = cp - sa->sa_data + 1;
				cp = sa->sa_data;
			hexprint:
				while (--n >= 0)
					m += printf("%02x%c", *cp++ & 0xff,
						    n > 0 ? ':' : ' ');
				m = (params->lflag ? 80 : 30) - m;
				while (m-- > 0)
					putchar(' ');

				link_layer = 1;
				break;
			}
		}

		show_stat("llu", 8, ipackets, link_layer|network_layer);
		printf(" ");
		show_stat("llu", 5, ierrors, link_layer);
		printf(" ");
		if (params->prioflag >= 0) {
			show_stat("llu", 8, ift_itcp, link_layer|network_layer);
			printf(" ");
			show_stat("llu", 8, ift_ipvp, link_layer|network_layer);
			printf(" ");
		}
		if (params->bflag) {
			show_stat("llu", 10, ibytes, link_layer|network_layer);
			printf(" ");
			if (params->prioflag >= 0) {
				show_stat("llu", 8, ift_itcb, link_layer|network_layer);
				printf(" ");
				show_stat("llu", 8, ift_ipvb, link_layer|network_layer);
				printf(" ");
			}
		}
		show_stat("llu", 8, opackets, link_layer|network_layer);
		printf(" ");
		show_stat("llu", 5, oerrors, link_layer);
		printf(" ");
		if (params->prioflag >= 0) {
			show_stat("llu", 8, ift_otcp, link_layer|network_layer);
			printf(" ");
			show_stat("llu", 8, ift_opvp, link_layer|network_layer);
			printf(" ");
		}
		if (params->bflag) {
			show_stat("llu", 10, obytes, link_layer|network_layer);
			printf(" ");
			if (params->prioflag >= 0) {
				show_stat("llu", 8, ift_otcb, link_layer|network_layer);
				printf(" ");
				show_stat("llu", 8, ift_opvb, link_layer|network_layer);
				printf(" ");
			}
		}
		show_stat("llu", 5, collisions, link_layer);
		if (params->tflag) {
			printf(" ");
			show_stat("d", 3, timer, link_layer);
		}
		if (params->dflag) {
			printf(" ");
			show_stat("d", 3, drops, link_layer);
		}
		if (params->Fflag) {
			printf(" ");
			show_stat("llu", 8, fpackets, link_layer|network_layer);
			if (params->bflag) {
				printf(" ");
				show_stat("llu", 10, fbytes,
				    link_layer|network_layer);
			}
		}
		putchar('\n');

		if (params->aflag)
			multipr(params, sa->sa_family, next, lim);
	}

	/*
	 * Finally print the statistics of the last interface
	 */
	if (params->sflag && last_ifname[0] != 0) {
		printprotoifstats(params, last_ifname);
	}
done:
	free(buf);

	return retval;
}

struct	iftot {
	SLIST_ENTRY(iftot) chain;
	char		ift_name[16];	/* interface name */
	u_int64_t	ift_ip;		/* input packets */
	u_int64_t	ift_ie;		/* input errors */
	u_int64_t	ift_op;		/* output packets */
	u_int64_t	ift_oe;		/* output errors */
	u_int64_t	ift_co;		/* collisions */
	u_int64_t	ift_dr;		/* drops */
	u_int64_t	ift_ib;		/* input bytes */
	u_int64_t	ift_ob;		/* output bytes */
	u_int64_t	ift_itcp;	/* input tc packets */
	u_int64_t	ift_itcb;	/* input tc bytes */
	u_int64_t	ift_otcp;	/* output tc packets */
	u_int64_t	ift_otcb;	/* output tc bytes */
	u_int64_t	ift_ipvp;	/* input priv tc packets */
	u_int64_t	ift_ipvb;	/* input priv tc bytes */
	u_int64_t	ift_opvp;	/* output priv tc packets */
	u_int64_t	ift_opvb;	/* output priv tc bytes */
	u_int64_t	ift_fp;		/* forwarded packets */
	u_int64_t	ift_fb;		/* forwarded bytes */
};

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 * XXX - should be rewritten to use ifmib(4).
 */
static void
sidewaysintpr(struct netstat_parameters *params)
{
	struct iftot *total, *sum, *interesting;
	register int line;
	int first;
	int name[6];
	size_t len;
	unsigned int ifcount, i;
	struct ifmibdata *ifmdall = 0;
	int interesting_row;
	sigset_t sigset, oldsigset;
	struct itimerval timer_interval;

	/* Common OID prefix */
	name[0] = CTL_NET;
	name[1] = PF_LINK;
	name[2] = NETLINK_GENERIC;

	len = sizeof(int);
	name[3] = IFMIB_SYSTEM;
	name[4] = IFMIB_IFCOUNT;
	if (sysctl(name, 5, &ifcount, &len, 0, 0) == 1)
		err(1, "sysctl IFMIB_IFCOUNT");

	len = ifcount * sizeof(struct ifmibdata);
	ifmdall = malloc(len);
	if (ifmdall == 0)
		err(1, "malloc failed");
	name[3] = IFMIB_IFALLDATA;
	name[4] = 0;
	name[5] = IFDATA_GENERAL;
	if (sysctl(name, 6, ifmdall, &len, (void *)0, 0) == -1)
		err(1, "sysctl IFMIB_IFALLDATA");

	interesting = NULL;
	interesting_row = 0;
	for (i = 0; i < ifcount; i++) {
		struct ifmibdata *ifmd = ifmdall + i;

		if (params->interface && strcmp(ifmd->ifmd_name, params->interface) == 0) {
			if ((interesting = calloc(ifcount,
			    sizeof(struct iftot))) == NULL)
				err(1, "malloc failed");
			interesting_row = if_nametoindex(params->interface);
			snprintf(interesting->ift_name, 16, "(%s)",
			    ifmd->ifmd_name);;
		}
	}
	if ((total = calloc(1, sizeof(struct iftot))) == NULL)
		err(1, "malloc failed");

	if ((sum = calloc(1, sizeof(struct iftot))) == NULL)
		err(1, "malloc failed");

	/* create a timer that fires repeatedly every interval seconds */
	timer_interval.it_value.tv_sec = params->interval;
	timer_interval.it_value.tv_usec = 0;
	timer_interval.it_interval.tv_sec = params->interval;
	timer_interval.it_interval.tv_usec = 0;
	(void)signal(SIGALRM, catchalarm);
	signalled = NO;
	(void)setitimer(ITIMER_REAL, &timer_interval, NULL);
	first = 1;
banner:
	if (params->vflag > 0)
		printf("%9s", " ");

	if (params->prioflag >= 0)
		printf("%39s %39s %36s", "input",
		    interesting ? interesting->ift_name : "(Total)", "output");
	else
		printf("%17s %14s %16s", "input",
		    interesting ? interesting->ift_name : "(Total)", "output");
	putchar('\n');

	if (params->vflag > 0)
		printf("%9s", " ");

	printf("%10s %5s %10s ", "packets", "errs", "bytes");
	if (params->prioflag >= 0)
		printf(" %10s %10s %10s %10s",
		    "tcpkts", "tcbytes", "pvpkts", "pvbytes");
	printf("%10s %5s %10s %5s", "packets", "errs", "bytes", "colls");
	if (params->dflag)
		printf(" %5.5s", "drops");
	if (params->prioflag >= 0)
		printf(" %10s %10s %10s %10s",
		    "tcpkts", "tcbytes", "pvpkts", "pvbytes");
	if (params->Fflag)
		printf(" %10s %10s", "fpackets", "fbytes");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	if (params->vflag && !first)
		print_time();

	if (interesting != NULL) {
		struct ifmibdata ifmd;
		struct ifmibdata_supplemental ifmsupp;

		len = sizeof(struct ifmibdata);
		name[3] = IFMIB_IFDATA;
		name[4] = interesting_row;
		name[5] = IFDATA_GENERAL;
		if (sysctl(name, 6, &ifmd, &len, (void *)0, 0) == -1)
			err(1, "sysctl IFDATA_GENERAL %d", interesting_row);

		len = sizeof(struct ifmibdata_supplemental);
		name[3] = IFMIB_IFDATA;
		name[4] = interesting_row;
		name[5] = IFDATA_SUPPLEMENTAL;
		if (sysctl(name, 6, &ifmsupp, &len, (void *)0, 0) == -1)
			err(1, "sysctl IFDATA_SUPPLEMENTAL %d",
			    interesting_row);

		if (!first) {
			printf("%10llu %5llu %10llu ",
			    ifmd.ifmd_data.ifi_ipackets - interesting->ift_ip,
			    ifmd.ifmd_data.ifi_ierrors - interesting->ift_ie,
			    ifmd.ifmd_data.ifi_ibytes - interesting->ift_ib);
			switch (params->prioflag) {
			case SO_TC_BE:
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_ibepackets -
				    interesting->ift_itcp,
				    ifmsupp.ifmd_traffic_class.ifi_ibebytes -
				    interesting->ift_itcb);
				break;
			case SO_TC_BK:
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_ibkpackets -
				    interesting->ift_itcp,
				    ifmsupp.ifmd_traffic_class.ifi_ibkbytes -
				    interesting->ift_itcb);
				break;
			case SO_TC_VI:
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_ivipackets -
				    interesting->ift_itcp,
				    ifmsupp.ifmd_traffic_class.ifi_ivibytes -
				    interesting->ift_itcb);
				break;
			case SO_TC_VO:
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_ivopackets -
				    interesting->ift_itcp,
				    ifmsupp.ifmd_traffic_class.ifi_ivobytes -
				    interesting->ift_itcb);
				break;
			default:
				break;
			}
			if (params->prioflag >= 0) {
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_ipvpackets -
				    interesting->ift_ipvp,
				    ifmsupp.ifmd_traffic_class.ifi_ipvbytes -
				    interesting->ift_ipvb);
			}
			printf("%10llu %5llu %10llu %5llu",
			    ifmd.ifmd_data.ifi_opackets - interesting->ift_op,
			    ifmd.ifmd_data.ifi_oerrors - interesting->ift_oe,
			    ifmd.ifmd_data.ifi_obytes - interesting->ift_ob,
			    ifmd.ifmd_data.ifi_collisions - interesting->ift_co);
			if (params->dflag)
				printf(" %5llu",
				    ifmd.ifmd_snd_drops - interesting->ift_dr);
			switch (params->prioflag) {
			case SO_TC_BE:
				printf(" %10llu %10llu",
				    ifmsupp.ifmd_traffic_class.ifi_obepackets -
				    interesting->ift_otcp,
				    ifmsupp.ifmd_traffic_class.ifi_obebytes -
				    interesting->ift_otcb);
				break;
			case SO_TC_BK:
				printf(" %10llu %10llu",
				    ifmsupp.ifmd_traffic_class.ifi_obkpackets -
				    interesting->ift_otcp,
				    ifmsupp.ifmd_traffic_class.ifi_obkbytes -
				    interesting->ift_otcb);
				break;
			case SO_TC_VI:
				printf(" %10llu %10llu",
				    ifmsupp.ifmd_traffic_class.ifi_ovipackets -
				    interesting->ift_otcp,
				    ifmsupp.ifmd_traffic_class.ifi_ovibytes -
				    interesting->ift_otcb);
				break;
			case SO_TC_VO:
				printf(" %10llu %10llu",
				    ifmsupp.ifmd_traffic_class.ifi_ovopackets -
				    interesting->ift_otcp,
				    ifmsupp.ifmd_traffic_class.ifi_ovobytes -
				    interesting->ift_otcb);
				break;
			default:
				break;
			}
			if (params->prioflag >= 0) {
				printf("%10llu %10llu ",
				    ifmsupp.ifmd_traffic_class.ifi_opvpackets -
				    interesting->ift_opvp,
				    ifmsupp.ifmd_traffic_class.ifi_opvbytes -
				    interesting->ift_opvb);
			}
			if (params->Fflag) {
				printf("%10llu %10llu",
				    ifmsupp.ifmd_data_extended.ifi_fpackets -
				    interesting->ift_fp,
				    ifmsupp.ifmd_data_extended.ifi_fbytes -
				    interesting->ift_fb);
			}
		}
		interesting->ift_ip = ifmd.ifmd_data.ifi_ipackets;
		interesting->ift_ie = ifmd.ifmd_data.ifi_ierrors;
		interesting->ift_ib = ifmd.ifmd_data.ifi_ibytes;
		interesting->ift_op = ifmd.ifmd_data.ifi_opackets;
		interesting->ift_oe = ifmd.ifmd_data.ifi_oerrors;
		interesting->ift_ob = ifmd.ifmd_data.ifi_obytes;
		interesting->ift_co = ifmd.ifmd_data.ifi_collisions;
		interesting->ift_dr = ifmd.ifmd_snd_drops;

		/* private counters */
		switch (params->prioflag) {
		case SO_TC_BE:
			interesting->ift_itcp =
			    ifmsupp.ifmd_traffic_class.ifi_ibepackets;
			interesting->ift_itcb =
			    ifmsupp.ifmd_traffic_class.ifi_ibebytes;
			interesting->ift_otcp =
			    ifmsupp.ifmd_traffic_class.ifi_obepackets;
			interesting->ift_otcb =
			    ifmsupp.ifmd_traffic_class.ifi_obebytes;
			break;
		case SO_TC_BK:
			interesting->ift_itcp =
			    ifmsupp.ifmd_traffic_class.ifi_ibkpackets;
			interesting->ift_itcb =
			    ifmsupp.ifmd_traffic_class.ifi_ibkbytes;
			interesting->ift_otcp =
			    ifmsupp.ifmd_traffic_class.ifi_obkpackets;
			interesting->ift_otcb =
			    ifmsupp.ifmd_traffic_class.ifi_obkbytes;
			break;
		case SO_TC_VI:
			interesting->ift_itcp =
			    ifmsupp.ifmd_traffic_class.ifi_ivipackets;
			interesting->ift_itcb =
			    ifmsupp.ifmd_traffic_class.ifi_ivibytes;
			interesting->ift_otcp =
			    ifmsupp.ifmd_traffic_class.ifi_ovipackets;
			interesting->ift_otcb =
			    ifmsupp.ifmd_traffic_class.ifi_ovibytes;
			break;
		case SO_TC_VO:
			interesting->ift_itcp =
			    ifmsupp.ifmd_traffic_class.ifi_ivopackets;
			interesting->ift_itcb =
			    ifmsupp.ifmd_traffic_class.ifi_ivobytes;
			interesting->ift_otcp =
			    ifmsupp.ifmd_traffic_class.ifi_ovopackets;
			interesting->ift_otcb =
			    ifmsupp.ifmd_traffic_class.ifi_ovobytes;
			break;
		default:
			break;
		}
		if (params->prioflag >= 0) {
			interesting->ift_ipvp =
			    ifmsupp.ifmd_traffic_class.ifi_ipvpackets;
			interesting->ift_ipvb =
			    ifmsupp.ifmd_traffic_class.ifi_ipvbytes;
			interesting->ift_opvp =
			    ifmsupp.ifmd_traffic_class.ifi_opvpackets;
			interesting->ift_opvb =
			    ifmsupp.ifmd_traffic_class.ifi_opvbytes;
		}
		interesting->ift_fp = ifmsupp.ifmd_data_extended.ifi_fpackets;
		interesting->ift_fb = ifmsupp.ifmd_data_extended.ifi_fbytes;
	} else {
		unsigned int latest_ifcount;
		struct ifmibdata_supplemental *ifmsuppall = NULL;

		len = sizeof(int);
		name[3] = IFMIB_SYSTEM;
		name[4] = IFMIB_IFCOUNT;
		if (sysctl(name, 5, &latest_ifcount, &len, 0, 0) == 1)
			err(1, "sysctl IFMIB_IFCOUNT");
		if (latest_ifcount > ifcount) {
			ifcount = latest_ifcount;
			len = ifcount * sizeof(struct ifmibdata);
			free(ifmdall);
			ifmdall = malloc(len);
			if (ifmdall == 0)
				err(1, "malloc ifmdall failed");
		} else if (latest_ifcount > ifcount) {
			ifcount = latest_ifcount;
			len = ifcount * sizeof(struct ifmibdata);
		}
		len = ifcount * sizeof(struct ifmibdata);
		name[3] = IFMIB_IFALLDATA;
		name[4] = 0;
		name[5] = IFDATA_GENERAL;
		if (sysctl(name, 6, ifmdall, &len, (void *)0, 0) == -1)
			err(1, "sysctl IFMIB_IFALLDATA");

		len = ifcount * sizeof(struct ifmibdata_supplemental);
		ifmsuppall = malloc(len);
		if (ifmsuppall == NULL)
			err(1, "malloc ifmsuppall failed");
		name[3] = IFMIB_IFALLDATA;
		name[4] = 0;
		name[5] = IFDATA_SUPPLEMENTAL;
		if (sysctl(name, 6, ifmsuppall, &len, (void *)0, 0) == -1)
			err(1, "sysctl IFMIB_IFALLDATA SUPPLEMENTAL");

		sum->ift_ip = 0;
		sum->ift_ie = 0;
		sum->ift_ib = 0;
		sum->ift_op = 0;
		sum->ift_oe = 0;
		sum->ift_ob = 0;
		sum->ift_co = 0;
		sum->ift_dr = 0;
		sum->ift_itcp = 0;
		sum->ift_itcb = 0;
		sum->ift_otcp = 0;
		sum->ift_otcb = 0;
		sum->ift_ipvp = 0;
		sum->ift_ipvb = 0;
		sum->ift_opvp = 0;
		sum->ift_opvb = 0;
		sum->ift_fp = 0;
		sum->ift_fb = 0;
		for (i = 0; i < ifcount; i++) {
			struct ifmibdata *ifmd = ifmdall + i;
			struct ifmibdata_supplemental *ifmsupp = ifmsuppall + i;

			sum->ift_ip += ifmd->ifmd_data.ifi_ipackets;
			sum->ift_ie += ifmd->ifmd_data.ifi_ierrors;
			sum->ift_ib += ifmd->ifmd_data.ifi_ibytes;
			sum->ift_op += ifmd->ifmd_data.ifi_opackets;
			sum->ift_oe += ifmd->ifmd_data.ifi_oerrors;
			sum->ift_ob += ifmd->ifmd_data.ifi_obytes;
			sum->ift_co += ifmd->ifmd_data.ifi_collisions;
			sum->ift_dr += ifmd->ifmd_snd_drops;
			/* private counters */
			if (params->prioflag >= 0) {
				switch (params->prioflag) {
				case SO_TC_BE:
					sum->ift_itcp += ifmsupp->ifmd_traffic_class.ifi_ibepackets;
					sum->ift_itcb += ifmsupp->ifmd_traffic_class.ifi_ibebytes;
					sum->ift_otcp += ifmsupp->ifmd_traffic_class.ifi_obepackets;
					sum->ift_otcb += ifmsupp->ifmd_traffic_class.ifi_obebytes;
					break;
				case SO_TC_BK:
					sum->ift_itcp += ifmsupp->ifmd_traffic_class.ifi_ibkpackets;
					sum->ift_itcb += ifmsupp->ifmd_traffic_class.ifi_ibkbytes;
					sum->ift_otcp += ifmsupp->ifmd_traffic_class.ifi_obkpackets;
					sum->ift_otcb += ifmsupp->ifmd_traffic_class.ifi_obkbytes;
					break;
				case SO_TC_VI:
					sum->ift_itcp += ifmsupp->ifmd_traffic_class.ifi_ivipackets;
					sum->ift_itcb += ifmsupp->ifmd_traffic_class.ifi_ivibytes;
					sum->ift_otcp += ifmsupp->ifmd_traffic_class.ifi_ovipackets;
					sum->ift_otcb += ifmsupp->ifmd_traffic_class.ifi_ovibytes;
					break;
				case SO_TC_VO:
					sum->ift_itcp += ifmsupp->ifmd_traffic_class.ifi_ivopackets;
					sum->ift_itcb += ifmsupp->ifmd_traffic_class.ifi_ivobytes;
					sum->ift_otcp += ifmsupp->ifmd_traffic_class.ifi_ovopackets;
					sum->ift_otcb += ifmsupp->ifmd_traffic_class.ifi_ovobytes;
					break;
				default:
					break;
				}
				sum->ift_ipvp += ifmsupp->ifmd_traffic_class.ifi_ipvpackets;
				sum->ift_ipvb += ifmsupp->ifmd_traffic_class.ifi_ipvbytes;
				sum->ift_opvp += ifmsupp->ifmd_traffic_class.ifi_opvpackets;
				sum->ift_opvb += ifmsupp->ifmd_traffic_class.ifi_opvbytes;
			}
			sum->ift_fp += ifmsupp->ifmd_data_extended.ifi_fpackets;
			sum->ift_fb += ifmsupp->ifmd_data_extended.ifi_fbytes;
		}
		if (!first) {
			printf("%10llu %5llu %10llu ",
				sum->ift_ip - total->ift_ip,
				sum->ift_ie - total->ift_ie,
				sum->ift_ib - total->ift_ib);
			if (params->prioflag >= 0)
				printf(" %10llu %10llu %10llu %10llu",
				    sum->ift_itcp - total->ift_itcp,
				    sum->ift_itcb - total->ift_itcb,
				    sum->ift_ipvp - total->ift_ipvp,
				    sum->ift_ipvb - total->ift_ipvb);
			printf("%10llu %5llu %10llu %5llu",
				sum->ift_op - total->ift_op,
				sum->ift_oe - total->ift_oe,
				sum->ift_ob - total->ift_ob,
				sum->ift_co - total->ift_co);
			if (params->dflag)
				printf(" %5llu", sum->ift_dr - total->ift_dr);
			if (params->prioflag >= 0)
				printf(" %10llu %10llu %10llu %10llu",
				    sum->ift_otcp - total->ift_otcp,
				    sum->ift_otcb - total->ift_otcb,
				    sum->ift_opvp - total->ift_opvp,
				    sum->ift_opvb - total->ift_opvb);
			if (params->Fflag)
				printf(" %10llu %10llu",
				    sum->ift_fp - total->ift_fp,
				    sum->ift_fb - total->ift_fb);
		}
		*total = *sum;

		free(ifmsuppall);
	}
	if (!first)
		putchar('\n');
	fflush(stdout);
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	(void)sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
	if (!signalled) {
	    sigemptyset(&sigset);
	    sigsuspend(&sigset);
	}
	(void)sigprocmask(SIG_SETMASK, &oldsigset, NULL);

	signalled = NO;
	line++;
	first = 0;
	if (line == 21)
		goto banner;
	else
		goto loop;
	/*NOTREACHED*/
}

void
intervalpr(struct netstat_parameters *params, int (*pr)(struct netstat_parameters *, uint32_t, char *, int), uint32_t off, char *name, int af)
{
	struct itimerval timer_interval;
	sigset_t sigset, oldsigset;

	/* create a timer that fires repeatedly every interval seconds */
	timer_interval.it_value.tv_sec = params->interval;
	timer_interval.it_value.tv_usec = 0;
	timer_interval.it_interval.tv_sec = params->interval;
	timer_interval.it_interval.tv_usec = 0;
	(void) signal(SIGALRM, catchalarm);
	signalled = NO;
	(void) setitimer(ITIMER_REAL, &timer_interval, NULL);

	for (;;) {
		pr(params, off, name, af);

		fflush(stdout);
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		(void) sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		(void) sigprocmask(SIG_SETMASK, &oldsigset, NULL);
		signalled = NO;
	}
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
static void
catchalarm(int signo )
{
	signalled = YES;
}

static char *
sec2str(total)
	time_t total;
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	days = total / 3600 / 24;
	hours = (total / 3600) % 24;
	mins = (total / 60) % 60;
	secs = total % 60;

	if (days) {
		first = 0;
		p += snprintf(p, sizeof(result) - (p - result), "%dd", days);
	}
	if (!first || hours) {
		first = 0;
		p += snprintf(p, sizeof(result) - (p - result), "%dh", hours);
	}
	if (!first || mins) {
		first = 0;
		p += snprintf(p, sizeof(result) - (p - result), "%dm", mins);
	}
	snprintf(p, sizeof(result) - (p - result), "%ds", secs);

	return(result);
}

int
intpr_ri(struct netstat_parameters *params, void (*pfunc)(struct netstat_parameters *, char *))
{
	int mib[6];
	char *buf = NULL, *lim, *next;
	size_t len;
	unsigned int ifindex = 0;
	struct if_msghdr2 *if2m;
	int retval = 0;

	if (params->interface != 0) {
		ifindex = if_nametoindex(params->interface);
		if (ifindex == 0) {
			snprintf(params->errbuf, sizeof(params->errbuf), "interface name is not valid: %s", params->interface);
			retval = -1;
			goto done;
		}
	}

	mib[0]	= CTL_NET;		/* networking subsystem */
	mib[1]	= PF_ROUTE;		/* type of information */
	mib[2]	= 0;			/* protocol (IPPROTO_xxx) */
	mib[3]	= 0;			/* address family */
	mib[4]	= NET_RT_IFLIST2;	/* operation */
	mib[5]	= 0;
	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		goto done;
	if ((buf = malloc(len)) == NULL) {
		snprintf(params->errbuf, sizeof(params->errbuf), "malloc failed");
		retval = -1;
		goto done;
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		goto done;
	}

	printf("%-6s %-17s %14.14s %-9.9s %4s %4s",
	       "Proto", "Linklayer Address", "Netif", "Expire", "Refs",
	       "Prbs");
	if (params->xflag)
		printf(" %7s %7s %7s", "RSSI", "LQM", "NPM");
	printf("\n");

	lim = buf + len;
	if2m = (struct if_msghdr2 *)buf;

	for (next = buf; next < lim; ) {
		if2m = (struct if_msghdr2 *)next;
		next += if2m->ifm_msglen;

		if (if2m->ifm_type != RTM_IFINFO2)
			continue;
		else if (params->interface != 0 && if2m->ifm_index != ifindex)
			continue;

		retval = llreach_sysctl(params, if2m->ifm_index);
		if (retval == -1) {
			break;
		}
	}
done:
	free(buf);

	return retval;
}

static int
llreach_sysctl(struct netstat_parameters *params, uint32_t ifindex)
{
#define	MAX_SYSCTL_TRY	5
	int mib[6], i, ntry = 0;
	size_t mibsize, len, needed, cnt;
	struct if_llreach_info *lri;
	struct timeval time;
	char *buf = NULL;
	char ifname[IF_NAMESIZE];
	int retval = 0;

	bzero(&mib, sizeof (mib));
	mibsize = sizeof (mib) / sizeof (mib[0]);
	if (sysctlnametomib("net.link.generic.system.llreach_info", mib,
	    &mibsize) == -1) {
		perror("sysctlnametomib");
		retval = -1;
		goto out_free;
	}

	needed = 0;
	mib[5] = ifindex;

	mibsize = sizeof (mib) / sizeof (mib[0]);
	do {
		if (sysctl(mib, mibsize, NULL, &needed, NULL, 0) == -1) {
			perror("sysctl net.link.generic.system.llreach_info");
			retval = -1;
			goto out_free;
		}
		if ((buf = malloc(needed)) == NULL) {
			perror("malloc");
			retval = -1;
			goto out_free;
		}
		if (sysctl(mib, mibsize, buf, &needed, NULL, 0) == -1) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				perror("sysctl");
				retval = -1;
				goto out_free;
			}
			free(buf);
			buf = NULL;
		}
	} while (buf == NULL);

	len = needed;
	cnt = len / sizeof (*lri);
	lri = (struct if_llreach_info *)buf;

	gettimeofday(&time, 0);
	if (if_indextoname(ifindex, ifname) == NULL)
		snprintf(ifname, sizeof (ifname), "%s", "?");

	for (i = 0; i < cnt; i++, lri++) {
		printf("0x%-4x %-17s %14.14s ", lri->lri_proto,
		    ether_ntoa((struct ether_addr *)lri->lri_addr), ifname);

		if (lri->lri_expire > time.tv_sec)
			printf("%-9.9s", sec2str(lri->lri_expire - time.tv_sec));
		else if (lri->lri_expire == 0)
			printf("%-9.9s", "permanent");
		else
			printf("%-9.9s", "expired");

		printf(" %4d", lri->lri_refcnt);
		if (lri->lri_probes)
			printf(" %4d", lri->lri_probes);

		if (params->xflag) {
			if (!lri->lri_probes)
				printf(" %-4.4s", "none");

			if (lri->lri_rssi != IFNET_RSSI_UNKNOWN)
				printf(" %7d", lri->lri_rssi);
			else
				printf(" %-7.7s", "unknown");

			switch (lri->lri_lqm)
			{
			case IFNET_LQM_THRESH_OFF:
				printf(" %-7.7s", "off");
				break;
			case IFNET_LQM_THRESH_UNKNOWN:
				printf(" %-7.7s", "unknown");
				break;
			case IFNET_LQM_THRESH_POOR:
				printf(" %-7.7s", "poor");
				break;
			case IFNET_LQM_THRESH_GOOD:
				printf(" %-7.7s", "good");
				break;
			default:
				printf(" %7d", lri->lri_lqm);
				break;
			}

			switch (lri->lri_npm)
			{
			case IFNET_NPM_THRESH_UNKNOWN:
				printf(" %-7.7s", "unknown");
				break;
			case IFNET_NPM_THRESH_NEAR:
				printf(" %-7.7s", "near");
				break;
			case IFNET_NPM_THRESH_GENERAL:
				printf(" %-7.7s", "general");
				break;
			case IFNET_NPM_THRESH_FAR:
				printf(" %-7.7s", "far");
				break;
			default:
				printf(" %7d", lri->lri_npm);
				break;
			}
		}

		printf("\n");
		len -= sizeof (*lri);
	}

	if (len > 0) {
		fprintf(stderr, "warning: %u trailing bytes from %s\n",
		    (unsigned int)len, "net.link.generic.system.llreach_info");
	}

out_free:
	free(buf);
#undef	MAX_SYSCTL_TRY

	return retval;
}

int
aqstatpr(struct netstat_parameters *params)
{
	struct itimerval timer_interval;
	struct if_qstatsreq ifqr;
	struct if_ifclassq_stats *ifcqs;
	sigset_t sigset, oldsigset;
	u_int32_t scheduler;
	int s = -1, n;
	struct ifaddrs *ifap = NULL, *ifa;
	const char *last_ifname = NULL;
	int retval = 0;

	if (params->cq < -1 || params->cq >= IFCQ_SC_MAX) {
		fprintf(stderr, "Invalid classq index (range is 0-%d)\n",
		     IFCQ_SC_MAX-1);
		return -1;
	}

	ifcqs = malloc(sizeof (*ifcqs));
	if (ifcqs == NULL) {
		fprintf(stderr, "Unable to allocate memory\n");
		retval = -1;
		goto done;
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET)");
		retval = -1;
		goto done;
	}
	if (getifaddrs(&ifap) != 0) {
		perror("getifaddrs");
		retval = -1;
		goto done;
    }

loop:
	if (params->interval > 0) {
		/* create a timer that fires repeatedly every interval seconds */
		timer_interval.it_value.tv_sec = params->interval;
		timer_interval.it_value.tv_usec = 0;
		timer_interval.it_interval.tv_sec = params->interval;
		timer_interval.it_interval.tv_usec = 0;
		(void) signal(SIGALRM, catchalarm);
		signalled = NO;
		(void) setitimer(ITIMER_REAL, &timer_interval, NULL);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct ifreq ifr;

		if (params->interface != NULL &&
			strcmp(ifa->ifa_name, params->interface) != 0) {
			continue;
		}

		if (last_ifname != NULL && strcmp(ifa->ifa_name, last_ifname) == 0) {
			continue;
		}
		last_ifname = ifa->ifa_name;

		bzero(&ifr, sizeof (struct ifreq));
		strlcpy(ifr.ifr_name, ifa->ifa_name, sizeof (ifr.ifr_name));

		if (ioctl(s, SIOCGIFEFLAGS, (caddr_t)&ifr) != 0) {
			perror("ioctl(SIOCGIFEFLAGS");
			continue;
		}

		if ((ifr.ifr_eflags & IFEF_TXSTART) == 0) {
			continue;
		}

		bzero(&ifqr, sizeof (ifqr));
		strlcpy(ifqr.ifqr_name, ifa->ifa_name, sizeof (ifqr.ifqr_name));
		ifqr.ifqr_buf = ifcqs;
		ifqr.ifqr_len = sizeof (*ifcqs);

		ifqr.ifqr_slot = 0;
		if (ioctl(s, SIOCGIFQUEUESTATS, (char *)&ifqr) < 0) {
			if (errno == ENXIO) {
				if (params->interface == NULL) {
					continue;
				}
				printf("Queue statistics are not available on %s\n",
					   ifa->ifa_name);
			} else {
				perror("Warning: ioctl(SIOCGIFQUEUESTATS)");
			}
			goto done;
		}
		scheduler = ifcqs->ifqs_scheduler;
		if (scheduler == PKTSCHEDT_NONE) {
			fprintf(stderr, "Invalid scheduler type\n");
			retval = -1;
			goto done;
		}

		if (params->interface == NULL) {
			printf("\n");
		}

		printf("%s:\n"
			   "     [ sched: %18s  qlength:  %3d/%3d ]\n",
			   ifa->ifa_name, sched2str(ifcqs->ifqs_scheduler),
			   ifcqs->ifqs_len, ifcqs->ifqs_maxlen);
		printf("     [ dequeued pkts: %10llu  bytes: %10llu "
			   " dropped pkts: %6llu bytes: %6llu ]\n",
			   ifcqs->ifqs_xmitcnt.packets, ifcqs->ifqs_xmitcnt.bytes,
			   ifcqs->ifqs_dropcnt.packets, ifcqs->ifqs_dropcnt.bytes);

		for (uint8_t grp = 0; grp < FQ_IF_STATS_MAX_GROUPS; grp++) {
			ifqr.ifqr_grp_idx = grp;
			for (n = 0; n < IFCQ_SC_MAX; n++) {
				if (params->cq >= 0 && params->cq != n){
					continue;
				}

				ifqr.ifqr_slot = n;
				errno = 0;
				if (ioctl(s, SIOCGIFQUEUESTATS, (char *)&ifqr) < 0) {
					if (errno == ENXIO && grp > 0 && grp < FQ_IF_STATS_MAX_GROUPS) {
						// try the next AQM group (qset) if the current is not configured
						// grp 0 should always exist as the default one
						break;
					}
					perror("Warning: ioctl(SIOCGIFQUEUESTATS)");
					retval = -1;
					goto done;
				}

				switch (scheduler) {
					case PKTSCHEDT_FQ_CODEL:
					case PKTSCHEDT_FQ_CODEL_NEW:
						print_fq_codel_stats(params, n,
											 &ifcqs->ifqs_fq_codel_stats,
											 grp);
						break;
					case PKTSCHEDT_NONE:
					default:
						break;
				}
			}
		}

		fflush(stdout);
	}
	if (params->interval > 0) {
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		(void) sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		(void) sigprocmask(SIG_SETMASK, &oldsigset, NULL);

		signalled = NO;
		goto loop;
	}

done:
	freeifaddrs(ifap);
	free(ifcqs);
	close(s);

	return retval;
}

static void
print_fq_codel_stats(struct netstat_parameters *params, int pri, struct fq_codel_classstats *fqst, uint8_t grp)
{
	int i = 0;

	if (fqst->fcls_service_class == 0 && fqst->fcls_pri == 0)
		return;
	printf("=====================================================\n");
	printf("     [ pri: %s (%d)\tsrv_cl: 0x%x\tquantum: %d\tdrr_max: %d\tqset idx: %hu ]\n",
	    pri2str(fqst->fcls_pri), fqst->fcls_pri,
	    fqst->fcls_service_class, fqst->fcls_quantum,
	    fqst->fcls_drr_max, grp);
	printf("     [ queued pkts: %llu\tbytes: %llu ]\n",
	    fqst->fcls_pkt_cnt, fqst->fcls_byte_cnt);
	printf("     [ dequeued pkts: %llu\tbytes: %llu ]\n",
	    fqst->fcls_dequeue, fqst->fcls_dequeue_bytes);
	printf("     [ budget: %lld\ttarget qdelay: %10s\t",
	    fqst->fcls_budget, nsec_to_str(fqst->fcls_target_qdelay));
	printf("update interval:%10s ]\n",
	    nsec_to_str(fqst->fcls_update_interval));
	printf("     [ flow control: %u\tfeedback: %u\tstalls: %u\tfailed: %u \toverwhelming: %u ]\n",
	    fqst->fcls_flow_control, fqst->fcls_flow_feedback,
	    fqst->fcls_dequeue_stall, fqst->fcls_flow_control_fail, fqst->fcls_overwhelming);
	printf("     [ drop overflow: %llu\tearly: %llu\tmemfail: %u\tduprexmt:%u ]\n",
	    fqst->fcls_drop_overflow, fqst->fcls_drop_early,
	    fqst->fcls_drop_memfailure, fqst->fcls_dup_rexmts);
#ifdef AQM_CONGESTION_FEEDBACK
    printf("     [ congestion feedback: %llu\t congestion drops: %llu ]\n",
        fqst->fcls_congestion_feedback, fqst->fcls_high_delay_drop);
#endif /* AQM_CONGESTION_FEEDBACK */
    printf("     [ L4S target qdelay: %10s ]\n", nsec_to_str(fqst->fcls_l4s_target_qdelay));
    printf("     [ CE marked:%llu\tCE marking failures:%llu\tL4S pkts:%llu   ]\n",
           fqst->fcls_ce_marked, fqst->fcls_ce_mark_failures, fqst->fcls_l4s_pkts);
    printf("     [ paced pkts:%llu\tpacemaker needed:%llu ]\n", fqst->fcls_paced_pkts, fqst->fcls_fcl_pacing_needed);

	printf("     [ flows total: %u\tnew: %u\told: %u ]\n",
	    fqst->fcls_flows_cnt,
	    fqst->fcls_newflows_cnt, fqst->fcls_oldflows_cnt);
	printf("     [ throttle on: %u\toff: %u\tdrop: %u ]\n",
	    fqst->fcls_throttle_on, fqst->fcls_throttle_off,
	    fqst->fcls_throttle_drops);
	printf("     [ compressible pkts: %u compressed pkts: %u]\n",
	    fqst->fcls_pkts_compressible, fqst->fcls_pkts_compressed);

	if (params->qflag < 2)
		return;

	if (fqst->fcls_flowstats_cnt > 0) {
		printf("Flowhash\tBytes\tMin qdelay\tFlags\t\n");
		for (i = 0; i < fqst->fcls_flowstats_cnt; i++) {
			printf("%x\t%u\t%14s\t",
			    fqst->fcls_flowstats[i].fqst_flowhash,
			    fqst->fcls_flowstats[i].fqst_bytes,
			    nsec_to_str(fqst->fcls_flowstats[i].fqst_min_qdelay));
			if (fqst->fcls_flowstats[i].fqst_flags &
			    FQ_FLOWSTATS_OLD_FLOW)
				printf("O");
			if (fqst->fcls_flowstats[i].fqst_flags &
			    FQ_FLOWSTATS_NEW_FLOW)
				printf("N");
			if (fqst->fcls_flowstats[i].fqst_flags &
			    FQ_FLOWSTATS_LARGE_FLOW)
				printf("L");
			if (fqst->fcls_flowstats[i].fqst_flags &
			    FQ_FLOWSTATS_DELAY_HIGH)
				printf("D");
			if (fqst->fcls_flowstats[i].fqst_flags &
			    FQ_FLOWSTATS_FLOWCTL_ON)
				printf("F");
			printf("\n");
		}
	}
}

#define NSEC_PER_SEC    1000000000      /* nanoseconds per second */
#define USEC_PER_SEC    1000000		/* microseconds per second */
#define MSEC_PER_SEC    1000		/* milliseconds per second */

static char *
nsec_to_str(unsigned long long nsec)
{
	static char buf[32];
	const char *u;
	long double n = nsec, t;

	if (nsec >= NSEC_PER_SEC) {
		t = n / NSEC_PER_SEC;
		u = "sec ";
	} else if (n >= USEC_PER_SEC) {
		t = n / USEC_PER_SEC;
		u = "msec";
	} else if (n >= MSEC_PER_SEC) {
		t = n / MSEC_PER_SEC;
		u = "usec";
	} else {
		t = n;
		u = "nsec";
	}

	snprintf(buf, sizeof (buf), "%-4.2Lf %4s", t, u);
	return (buf);
}

static char *
sched2str(unsigned int s)
{
	char *c;

	switch (s) {
	case PKTSCHEDT_NONE:
		c = "NONE";
		break;
	case PKTSCHEDT_FQ_CODEL:
		c = "FQ_CODEL";
		break;
#ifdef PKTSCHEDT_FQ_CODEL_NEW
    case PKTSCHEDT_FQ_CODEL_NEW:
        c = "NEW FQ_CODEL";
        break;
#endif /* PKTSCHEDT_FQ_CODEL_NEW */
	default:
		c = "UNKNOWN";
		break;
	}

	return (c);
}

static char *
pri2str(unsigned int i)
{
	char *c;
	switch (i) {
	case 9:
		c = "BK_SYS";
		break;
	case 8:
		c = "BK";
		break;
	case 7:
		c = "BE";
		break;
	case 6:
		c = "RD";
		break;
	case 5:
		c = "OAM";
		break;
	case 4:
		c = "AV";
		break;
	case 3:
		c = "RV";
		break;
	case 2:
		c = "VI";
		break;
	case 1:
		c = "VO";
		break;
	case 0:
		c = "CTL";
		break;
	default:
		c = "?";
		break;
	}
	return (c);
}

int
rxpollstatpr(struct netstat_parameters *params)
{
	struct ifmibdata_supplemental ifmsupp;
	size_t miblen = sizeof (ifmsupp);
	struct itimerval timer_interval;
	struct if_rxpoll_stats *sp;
	struct if_netif_stats *np;
	sigset_t sigset, oldsigset;
	unsigned int ifindex;
	int name[6];
	struct ifaddrs *ifap = NULL, *ifa;
	const char *last_ifname = NULL;
	int s = -1;
	int retval = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET)");
		retval = -1;
		goto done;
	}
	if (getifaddrs(&ifap) != 0) {
		perror("getifaddrs");
		retval = -1;
		goto done;
    }

loop:
	if (params->interval > 0) {
		/* create a timer that fires repeatedly every interval seconds */
		timer_interval.it_value.tv_sec = params->interval;
		timer_interval.it_value.tv_usec = 0;
		timer_interval.it_interval.tv_sec = params->interval;
		timer_interval.it_interval.tv_usec = 0;
		(void) signal(SIGALRM, catchalarm);
		signalled = NO;
		(void) setitimer(ITIMER_REAL, &timer_interval, NULL);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct ifreq ifr;

		if (params->interface != NULL &&
			strcmp(ifa->ifa_name, params->interface) != 0) {
			continue;
		}

		if (last_ifname != NULL && strcmp(ifa->ifa_name, last_ifname) == 0) {
			continue;
		}
		last_ifname = ifa->ifa_name;

		bzero(&ifr, sizeof (struct ifreq));
		strlcpy(ifr.ifr_name, ifa->ifa_name, sizeof (ifr.ifr_name));

		if (ioctl(s, SIOCGIFEFLAGS, (caddr_t)&ifr) != 0) {
			perror("ioctl(SIOCGIFEFLAGS");
			retval = -1;
			goto done;
		}

		if ((ifr.ifr_eflags & IFEF_RXPOLL) == 0) {
			continue;
		}

		ifindex = if_nametoindex(ifa->ifa_name);
		if (ifindex == 0) {
			fprintf(stderr, "Invalid interface name\n");
			retval = -1;
			goto done;
		}

		bzero(&ifmsupp, sizeof (struct ifmibdata_supplemental));

		/* Common OID prefix */
		name[0] = CTL_NET;
		name[1] = PF_LINK;
		name[2] = NETLINK_GENERIC;
		name[3] = IFMIB_IFDATA;
		name[4] = ifindex;
		name[5] = IFDATA_SUPPLEMENTAL;
		if (sysctl(name, 6, &ifmsupp, &miblen, NULL, 0) == -1) {
			perror("sysctl IFDATA_SUPPLEMENTAL");
			goto done;
		}
		sp = &ifmsupp.ifmd_rxpoll_stats;

		if (params->interface == NULL) {
			printf("\n");
		}

		printf("%-4s [ poll on requests:  %15u  errors: %27u ]\n",
			   ifa->ifa_name, sp->ifi_poll_on_req, sp->ifi_poll_on_err);
		printf("     [ poll off requests: %15u  errors: %27u ]\n",
			   sp->ifi_poll_off_req, sp->ifi_poll_off_err);
		printf("     [ polled packets: %18llu  per poll limit: %19u ]\n",
			   sp->ifi_poll_packets, sp->ifi_poll_packets_limit);
		printf("     [ polled bytes: %20llu ]\n", sp->ifi_poll_bytes);
		printf("     [ poll interval: %14llu nsec ]\n",
			   sp->ifi_poll_interval_time);
		printf("     [ sampled packets avg/min/max: %12u / %12u / %12u ]\n",
			   sp->ifi_poll_packets_avg, sp->ifi_poll_packets_min,
			   sp->ifi_poll_packets_max);
		printf("     [ sampled bytes avg/min/max:   %12u / %12u / %12u ]\n",
			   sp->ifi_poll_bytes_avg, sp->ifi_poll_bytes_min,
			   sp->ifi_poll_bytes_max);
		printf("     [ sampled wakeups avg:         %12u ]\n",
			   sp->ifi_poll_wakeups_avg);
		printf("     [ packets lowat/hiwat threshold: %10u / %10u ]\n",
			   sp->ifi_poll_packets_lowat, sp->ifi_poll_packets_hiwat);
		printf("     [ bytes lowat/hiwat threshold:   %10u / %10u ]\n",
			   sp->ifi_poll_bytes_lowat, sp->ifi_poll_bytes_hiwat);
		printf("     [ wakeups lowat/hiwat threshold: %10u / %10u ]\n",
			   sp->ifi_poll_wakeups_lowat, sp->ifi_poll_wakeups_hiwat);

		np = &ifmsupp.ifmd_netif_stats;
		printf("     [ mit mode: %24U  cfg idx: %26u ]\n",
			   np->ifn_rx_mit_mode, np->ifn_rx_mit_cfg_idx);
		printf("     [ cfg packets lo/hi threshold: %12u / %12u ]\n",
			   np->ifn_rx_mit_cfg_packets_lowat, np->ifn_rx_mit_cfg_packets_hiwat);
		printf("     [ cfg bytes lo/hi threshold:   %12u / %12u ]\n",
			   np->ifn_rx_mit_cfg_bytes_lowat, np->ifn_rx_mit_cfg_bytes_hiwat);
		printf("     [ cfg interval: %15u nsec ]\n",
			   np->ifn_rx_mit_cfg_interval);
		printf("     [ mit interval: %15llu nsec ]\n",
			   np->ifn_rx_mit_interval);
		printf("     [ mit packets avg/min/max:    %12u / %12u / %12u ]\n",
			   np->ifn_rx_mit_packets_avg, np->ifn_rx_mit_packets_min,
			   np->ifn_rx_mit_packets_max);
		printf("     [ mit bytes avg/min/max:      %12u / %12u / %12u ]\n",
			   np->ifn_rx_mit_bytes_avg, np->ifn_rx_mit_bytes_min,
			   np->ifn_rx_mit_bytes_max);

		fflush(stdout);
	}
	if (params->interval > 0) {
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		(void) sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		(void) sigprocmask(SIG_SETMASK, &oldsigset, NULL);

		signalled = NO;
		goto loop;
	}
done:
	freeifaddrs(ifap);
	close(s);

	return retval;
}

static int
create_control_socket(struct netstat_parameters *params, const char *control_name)
{
	struct sockaddr_ctl sc;
	struct ctl_info	ctl;
	int fd;

	fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (fd == -1) {
		perror("socket(PF_SYSTEM)");
		return fd;
	}

	/* Get the control ID for statistics */
	bzero(&ctl, sizeof(ctl));
	strlcpy(ctl.ctl_name, control_name, sizeof(ctl.ctl_name));
	if (ioctl(fd, CTLIOCGINFO, &ctl) == -1)
	{
		perror("ioctl(CTLIOCGINFO)");
		close(fd);
		return -1;
	}

	/* Connect to the statistics control */
	bzero(&sc, sizeof(sc));
	sc.sc_len = sizeof(sc);
	sc.sc_family = AF_SYSTEM;
	sc.ss_sysaddr = SYSPROTO_CONTROL;
	sc.sc_id = ctl.ctl_id;
	sc.sc_unit = 0;
	if (connect(fd, (struct sockaddr*)&sc, sc.sc_len) != 0)
	{
		perror("connect(SYSPROTO_CONTROL)");
		close(fd);
		return -1;
	}

	/* Set socket to non-blocking operation */
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
		perror("fcnt(F_SETFL,O_NONBLOCK)");
		close(fd);
		return -1;
	}
	return fd;
}

static int
add_nstat_src(int fd, const nstat_ifnet_add_param *ifparam,
		nstat_src_ref_t *outsrc)
{
	nstat_msg_add_src_req *addreq;
	nstat_msg_src_added *addedmsg;
	nstat_ifnet_add_param *param;
	char buffer[sizeof(*addreq) + sizeof(*param)];
	ssize_t result;
	const u_int32_t	addreqsize =
		offsetof(struct nstat_msg_add_src, param) + sizeof(*param);

	/* Setup the add source request */
	addreq = (nstat_msg_add_src_req *)buffer;
	param = (nstat_ifnet_add_param*)addreq->param;
	bzero(addreq, addreqsize);
	addreq->hdr.context = (uintptr_t)&buffer;
	addreq->hdr.type = NSTAT_MSG_TYPE_ADD_SRC;
	addreq->provider = NSTAT_PROVIDER_IFNET;
	bzero(param, sizeof(*param));
	param->ifindex = ifparam->ifindex;
	param->threshold = ifparam->threshold;

	/* Send the add source request */
	result = send(fd, addreq, addreqsize, 0);
	if (result != addreqsize)
	{
		if (result == -1)
			perror("send(NSTAT_ADD_SRC_REQ)");
		else
			fprintf(stderr, "%s: could only sent %ld out of %d\n",
				__func__, result, addreqsize);
		return -1;
	}

	/* Receive the response */
	addedmsg = (nstat_msg_src_added *)buffer;
	result = recv(fd, addedmsg, sizeof(buffer), 0);
	if (result < sizeof(*addedmsg))
	{
		if (result == -1)
			perror("recv(NSTAT_ADD_SRC_RSP)");
		else
			fprintf(stderr, "%s: recv too small, received %ld, "
				"expected %lu\n", __func__, result,
				sizeof(*addedmsg));
		return -1;
	}

	if (addedmsg->hdr.type != NSTAT_MSG_TYPE_SRC_ADDED)
	{
		fprintf(stderr, "%s: received wrong message type, received %u "
			"expected %u\n", __func__, addedmsg->hdr.type,
			NSTAT_MSG_TYPE_SRC_ADDED);
		return -1;
	}

	if (addedmsg->hdr.context != (uintptr_t)&buffer)
	{
		fprintf(stderr, "%s: received wrong context, received %llu "
			"expected %lu\n", __func__, addedmsg->hdr.context,
			(uintptr_t)&buffer);
		return -1;
	}
	*outsrc = addedmsg->srcref;
	return 0;
}

static int
rem_nstat_src(int fd, nstat_src_ref_t sref)
{
	nstat_msg_rem_src_req *remreq;
	nstat_msg_src_removed *remrsp;
	char buffer[sizeof(*remreq)];
	ssize_t result;

	/* Setup the add source request */
	remreq = (nstat_msg_rem_src_req *)buffer;
	bzero(remreq, sizeof(*remreq));
	remreq->hdr.type = NSTAT_MSG_TYPE_REM_SRC;
	remreq->srcref = sref;

	/* Send the remove source request */
	result = send(fd, remreq, sizeof(*remreq), 0);
	if (result != sizeof(*remreq)) {
		if (result == -1)
			perror("send(NSTAT_REM_SRC_REQ)");
		else
			fprintf(stderr, "%s: could only sent %ld out of %lu\n",
				__func__, result, sizeof(*remreq));
		return -1;
	}

	/* Receive the response */
	remrsp = (nstat_msg_src_removed *)buffer;
	result = recv(fd, remrsp, sizeof(buffer), 0);
	if (result < sizeof(*remrsp)) {
		if (result == -1)
			perror("recv(NSTAT_REM_SRC_RSP)");
		else
			fprintf(stderr, "%s: recv too small, received %ld, "
				"expected %lu\n", __func__, result,
				sizeof(*remrsp));
		return -1;
	}

	if (remrsp->hdr.type != NSTAT_MSG_TYPE_SRC_REMOVED) {
		fprintf(stderr, "%s: received wrong message type, received %u "
			"expected %u\n", __func__, remrsp->hdr.type,
			NSTAT_MSG_TYPE_SRC_REMOVED);
		return -1;
	}

	if (remrsp->srcref != sref) {
		fprintf(stderr, "%s: received invalid srcref, received %llu "
			"expected %llu\n", __func__, remrsp->srcref, sref);
	}
	return 0;
}

static int
get_src_decsription(int fd, nstat_src_ref_t srcref,
			struct nstat_ifnet_descriptor *ifdesc)
{
	nstat_msg_get_src_description *dreq;
	nstat_msg_src_description *drsp;
	char buffer[sizeof(*drsp) + sizeof(*ifdesc)];
	ssize_t result;
	const u_int32_t	descsize =
		offsetof(struct nstat_msg_src_description, data) +
		sizeof(nstat_ifnet_descriptor);

	dreq = (nstat_msg_get_src_description *)buffer;
	bzero(dreq, sizeof(*dreq));
	dreq->hdr.type = NSTAT_MSG_TYPE_GET_SRC_DESC;
	dreq->srcref = srcref;
	result = send(fd, dreq, sizeof(*dreq), 0);
	if (result != sizeof(*dreq))
	{
		if (result == -1)
			perror("send(NSTAT_GET_SRC_DESC_REQ)");
		else
			fprintf(stderr, "%s: sent %ld out of %lu\n",
				__func__, result, sizeof(*dreq));
		return -1;
	}

	/* Receive the source description response */
	drsp = (nstat_msg_src_description *)buffer;
	result = recv(fd, drsp, sizeof(buffer), 0);
	if (result < descsize)
	{
		if (result == -1)
			perror("recv(NSTAT_GET_SRC_DESC_RSP");
		else
			fprintf(stderr, "%s: recv too small, received %ld, "
				"expected %u\n", __func__, result, descsize);
		return -1;
	}

	if (drsp->hdr.type != NSTAT_MSG_TYPE_SRC_DESC)
	{
		fprintf(stderr, "%s: received wrong message type, received %u "
			"expected %u\n", __func__, drsp->hdr.type,
			NSTAT_MSG_TYPE_SRC_DESC);
		return -1;
	}

	if (drsp->srcref != srcref)
	{
		fprintf(stderr, "%s: received message for wrong source, "
			"received 0x%llx expected 0x%llx\n",
			__func__, drsp->srcref, srcref);
		return -1;
	}

	bcopy(drsp->data, ifdesc, sizeof(*ifdesc));
	return 0;
}

static void
print_wifi_status(nstat_ifnet_desc_wifi_status *status)
{
	int tmp;
#define val(x, f)	\
	((status->valid_bitmask & NSTAT_IFNET_DESC_WIFI_ ## f ## _VALID) ?\
	 status->x : -1)
#define parg(n, un) #n, val(n, un)
#define pretxtl(n, un) \
	(((tmp = val(n, un)) == -1) ? "(not valid)" : \
	((tmp == NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_NONE) ? "(none)" : \
	((tmp == NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_LOW) ? "(low)" : \
	((tmp == NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_MEDIUM) ? "(medium)" : \
	((tmp == NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_HIGH) ? "(high)" : \
	"(?)")))))

	printf("wifi status:\n");
	printf(
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t\t%d%s\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t\t%d\n",
	    parg(link_quality_metric, LINK_QUALITY_METRIC),
	    parg(ul_effective_bandwidth, UL_EFFECTIVE_BANDWIDTH),
	    parg(ul_max_bandwidth, UL_MAX_BANDWIDTH),
	    parg(ul_min_latency, UL_MIN_LATENCY),
	    parg(ul_effective_latency, UL_EFFECTIVE_LATENCY),
	    parg(ul_max_latency, UL_MAX_LATENCY),
	    parg(ul_retxt_level, UL_RETXT_LEVEL),
	    pretxtl(ul_retxt_level, UL_RETXT_LEVEL),
	    parg(ul_bytes_lost, UL_BYTES_LOST),
	    parg(ul_error_rate, UL_ERROR_RATE),
	    parg(dl_effective_bandwidth, DL_EFFECTIVE_BANDWIDTH),
	    parg(dl_max_bandwidth, DL_MAX_BANDWIDTH),
	    parg(dl_min_latency, DL_MIN_LATENCY),
	    parg(dl_effective_latency, DL_EFFECTIVE_LATENCY),
	    parg(dl_max_latency, DL_MAX_LATENCY),
	    parg(dl_error_rate, DL_ERROR_RATE),
	    parg(config_frequency, CONFIG_FREQUENCY),
	    parg(config_multicast_rate, CONFIG_MULTICAST_RATE),
	    parg(scan_count, CONFIG_SCAN_COUNT),
	    parg(scan_duration, CONFIG_SCAN_DURATION)
	    );
#undef pretxtl
#undef parg
#undef val
}

static void
print_cellular_status(nstat_ifnet_desc_cellular_status *status)
{
	int tmp, tmp_mss;
#define val(x, f)	\
	((status->valid_bitmask & NSTAT_IFNET_DESC_CELL_ ## f ## _VALID) ?\
	 status->x : -1)
#define parg(n, un) #n, val(n, un)
#define pretxtl(n, un) \
	(((tmp = val(n, un)) == -1) ? "(not valid)" : \
	((tmp == NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_NONE) ? "(none)" : \
	((tmp == NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_LOW) ? "(low)" : \
	((tmp == NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_MEDIUM) ? "(medium)" : \
	((tmp == NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_HIGH) ? "(high)" : \
	"(?)")))))
#define pretxtm(n, un) \
	(((tmp_mss = val(n,un)) == -1) ? "(not valid)" : \
	((tmp_mss == NSTAT_IFNET_DESC_MSS_RECOMMENDED_NONE) ? "(none)" : \
	((tmp_mss == NSTAT_IFNET_DESC_MSS_RECOMMENDED_MEDIUM) ? "(medium)" : \
	((tmp_mss == NSTAT_IFNET_DESC_MSS_RECOMMENDED_LOW) ? "(low)" : \
	"(?)"))))

	printf("cellular status:\n");
	printf(
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t\t%d%s\n"
	    "\t%s:\t\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d\n"
	    "\t%s:\t%d %s\n",
	    parg(link_quality_metric, LINK_QUALITY_METRIC),
	    parg(ul_effective_bandwidth, UL_EFFECTIVE_BANDWIDTH),
	    parg(ul_max_bandwidth, UL_MAX_BANDWIDTH),
	    parg(ul_min_latency, UL_MIN_LATENCY),
	    parg(ul_effective_latency, UL_EFFECTIVE_LATENCY),
	    parg(ul_max_latency, UL_MAX_LATENCY),
	    parg(ul_retxt_level, UL_RETXT_LEVEL),
	    pretxtl(ul_retxt_level, UL_RETXT_LEVEL),
	    parg(ul_bytes_lost, UL_BYTES_LOST),
	    parg(ul_min_queue_size, UL_MIN_QUEUE_SIZE),
	    parg(ul_avg_queue_size, UL_AVG_QUEUE_SIZE),
	    parg(ul_max_queue_size, UL_MAX_QUEUE_SIZE),
	    parg(dl_effective_bandwidth, DL_EFFECTIVE_BANDWIDTH),
	    parg(dl_max_bandwidth, DL_MAX_BANDWIDTH),
	    parg(config_inactivity_time, CONFIG_INACTIVITY_TIME),
	    parg(config_backoff_time, CONFIG_BACKOFF_TIME),
	    parg(mss_recommended, MSS_RECOMMENDED),
	    pretxtm(mss_recommended, MSS_RECOMMENDED)
	    );
#undef pretxtl
#undef parg
#undef val
}

static int
get_interface_state(int fd, const char *ifname, struct ifreq *ifr)
{
	bzero(ifr, sizeof(*ifr));
	snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%s", ifname);

	if (ioctl(fd, SIOCGIFINTERFACESTATE, ifr) == -1) {
		perror("ioctl(CTLIOCGINFO)");
		return -1;
	}
	return 0;
}

static void
print_interface_state(struct ifreq *ifr)
{
	int lqm, rrc, avail;

	printf("\ninterface state:\n");

	if (ifr->ifr_interface_state.valid_bitmask &
	    IF_INTERFACE_STATE_LQM_STATE_VALID) {
		printf("\tlqm: ");
		lqm = ifr->ifr_interface_state.lqm_state;
		if (lqm == IFNET_LQM_THRESH_GOOD)
			printf("\"good\"");
		else if (lqm == IFNET_LQM_THRESH_POOR)
			printf("\"poor\"");
		else if (lqm == IFNET_LQM_THRESH_BAD)
			printf("\"bad\"");
		else if (lqm == IFNET_LQM_THRESH_UNKNOWN)
			printf("\"unknown\"");
		else if (lqm == IFNET_LQM_THRESH_OFF)
			printf("\"off\"");
		else
			printf("invalid(%d)", lqm);
	}

	if (ifr->ifr_interface_state.valid_bitmask &
	    IF_INTERFACE_STATE_RRC_STATE_VALID) {
		printf("\trrc: ");
		rrc = ifr->ifr_interface_state.rrc_state;
		if (rrc == IF_INTERFACE_STATE_RRC_STATE_CONNECTED)
			printf("\"connected\"");
		else if (rrc == IF_INTERFACE_STATE_RRC_STATE_IDLE)
			printf("\"idle\"");
		else
			printf("\"invalid(%d)\"", rrc);
	}

	if (ifr->ifr_interface_state.valid_bitmask &
	    IF_INTERFACE_STATE_INTERFACE_AVAILABILITY_VALID) {
		printf("\tavailability: ");
		avail = ifr->ifr_interface_state.interface_availability;
		if (avail == IF_INTERFACE_STATE_INTERFACE_AVAILABLE)
			printf("\"true\"");
		else if (rrc == IF_INTERFACE_STATE_INTERFACE_UNAVAILABLE)
			printf("\"false\"");
		else
			printf("\"invalid(%d)\"", avail);
	}
    printf("\n");
}

void
print_link_status(struct netstat_parameters *params, const char *ifname)
{
	unsigned int ifindex;
	struct itimerval timer_interval;
	sigset_t sigset, oldsigset;
	struct nstat_ifnet_descriptor ifdesc;
	nstat_ifnet_add_param ifparam;
	nstat_src_ref_t	sref = 0;
	int ctl_fd = -1;
	struct ifaddrs *ifap = NULL, *ifa;
	const char *last_ifname = NULL;
	int s = -1;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET)");
		goto done;
	}
	if (getifaddrs(&ifap) != 0) {
		perror("getifaddrs");
		goto done;
    }
loop:
	if (params->interval > 0) {
		/* create a timer that fires repeatedly every interval
		 * seconds */
		timer_interval.it_value.tv_sec = params->interval;
		timer_interval.it_value.tv_usec = 0;
		timer_interval.it_interval.tv_sec = params->interval;
		timer_interval.it_interval.tv_usec = 0;
		(void) signal(SIGALRM, catchalarm);
		signalled = NO;
		(void) setitimer(ITIMER_REAL, &timer_interval, NULL);
	}
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct ifreq ifr;

		if (ifname != NULL &&
			strcmp(ifa->ifa_name, ifname) != 0) {
			continue;
		}

		if (last_ifname != NULL && strcmp(ifa->ifa_name, last_ifname) == 0) {
			continue;
		}
		last_ifname = ifa->ifa_name;

		ifindex = if_nametoindex(last_ifname);
		if (ifindex == 0) {
			fprintf(stderr, "Invalid interface name\n");
			return;
		}

		if ((ctl_fd = create_control_socket(params, NET_STAT_CONTROL_NAME)) < 0) {
			perror("create_control_socket");
			goto done;
		}

		bzero(&ifparam, sizeof(nstat_ifnet_add_param));
		ifparam.ifindex = ifindex;
		ifparam.threshold = UINT64_MAX;
		if (add_nstat_src(ctl_fd, &ifparam, &sref))
			goto done;

		/* get interface state */
		if (get_interface_state(ctl_fd, last_ifname, &ifr))
			goto done;

		/* get ntstat interface description */
		if (get_src_decsription(ctl_fd, sref, &ifdesc))
			goto done;

		/* print time */
		printf("\n%s: ", last_ifname);
		print_time();

		/* print interface state */
		print_interface_state(&ifr);

		/* print ntsat interface link status */
		if (ifdesc.link_status.link_status_type ==
			NSTAT_IFNET_DESC_LINK_STATUS_TYPE_CELLULAR)
			print_cellular_status(&ifdesc.link_status.u.cellular);
		else if (ifdesc.link_status.link_status_type ==
				 NSTAT_IFNET_DESC_LINK_STATUS_TYPE_WIFI)
			print_wifi_status(&ifdesc.link_status.u.wifi);

		fflush(stdout);
		if (sref) {
			rem_nstat_src(ctl_fd, sref);
			sref = 0;
		}
		close(ctl_fd);
		ctl_fd = -1;
	}
	if (params->interval > 0) {
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		(void) sigprocmask(SIG_BLOCK, &sigset, &oldsigset);
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		(void) sigprocmask(SIG_SETMASK, &oldsigset, NULL);

		signalled = NO;
		goto loop;
	}
done:
	if (sref)
		rem_nstat_src(ctl_fd, sref);
	close(ctl_fd);
	freeifaddrs(ifap);
	close(s);
}
