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
 * Copyright (c) 1983, 1988, 1993
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
 */

#ifndef lint
char const copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sys_domain.h>
#include <sys/vsock_private.h>

#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include "netstat.h"
#include <sys/types.h>
#include <sys/sysctl.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: main.c,v 1.8 2004/10/14 22:24:09 lindak Exp $
 *
 */

struct protox {
	int	(*pr_cblocks)(struct netstat_parameters *, uint32_t, char *, int);
					/* control blocks printing routine */
	int	(*pr_stats)(struct netstat_parameters *, uint32_t, char *, int);
					/* statistics printing routine */
	int	(*pr_istats)(struct netstat_parameters *, char *);	/* per/if statistics printing routine */
	int	(*pr_reinit)(struct netstat_parameters *, uint32_t, char *, int);	/* per/if statistics printing routine */
	char	*pr_name;		/* well-known name */
	int	pr_protocol;
} protox[] = {
	{ protopr,	tcp_stats,	tcp_ifstats,	tcp_reinit,		"tcp",	IPPROTO_TCP },
	{ protopr,	udp_stats,	udp_ifstats,	udp_reinit,		"udp",	IPPROTO_UDP },
	{ protopr,	NULL,		NULL,	NULL,		"divert", IPPROTO_DIVERT },
	{ protopr,	ip_stats,	NULL,	NULL,		"ip",	IPPROTO_RAW },
	{ protopr,	icmp_stats,	NULL,	NULL,		"icmp",	IPPROTO_ICMP },
	{ protopr,	igmp_stats,	NULL,	NULL,		"igmp",	IPPROTO_IGMP },
#ifdef IPSEC
	{ NULL,		ipsec_stats,	NULL,	NULL,		"ipsec", IPPROTO_ESP},
#endif
	{ NULL,		arp_stats,	NULL,	NULL,		"arp",	0 },
	{ mptcppr,	mptcp_stats,	NULL,	mptcp_reinit,		"mptcp", IPPROTO_TCP },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

#ifdef INET6
struct protox ip6protox[] = {
	{ protopr,	tcp_stats,	NULL,	NULL,		"tcp",	IPPROTO_TCP },
	{ protopr,	udp_stats,	NULL,	NULL,		"udp",	IPPROTO_UDP },
	{ protopr,	ip6_stats,	ip6_ifstats,	NULL,		"ip6",	IPPROTO_RAW },
	{ protopr,	icmp6_stats,	icmp6_ifstats,	NULL,		"icmp6",IPPROTO_ICMPV6 },
#ifdef IPSEC
	{ NULL,		ipsec_stats,	NULL,	NULL,		"ipsec6", IPPROTO_ESP },
#endif
	{ NULL,		rip6_stats,	NULL,	NULL,		"rip6",	IPPROTO_RAW },
	{ mptcppr,	mptcp_stats,	NULL,	NULL,		"mptcp", IPPROTO_TCP },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};
#endif /*INET6*/

#ifdef IPSEC
struct protox pfkeyprotox[] = {
	{ keysockpr,		pfkey_stats,	NULL,	NULL,		"pfkey", PF_KEY_V2 },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};
#endif

struct protox systmprotox[] = {
	{ systmpr,	NULL,		NULL,	NULL,		"reg", 0 },
	{ systmpr,	kevt_stats,		NULL,	NULL,		"kevt", SYSPROTO_EVENT },
	{ systmpr,	kctl_stats,	NULL,	NULL,		"kctl", SYSPROTO_CONTROL },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

struct protox nstatprotox[] = {
	{ NULL,		print_nstat_stats,	NULL,	NULL,		"nstat", 0 },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

struct protox ipcprotox[] = {
	{ NULL,		print_extbkidle_stats,	NULL,	NULL,		"xbkidle", 0 },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

struct protox kernprotox[] = {
	{ NULL,		print_net_api_stats,	NULL,	NULL,		"net_api", 0 },
	{ NULL,		print_if_ports_used_stats,	NULL,	NULL,		"if_ports_used", 0 },
	{ NULL,		NULL,	print_if_link_heuristics_stats,	NULL,		"link_heuristics", 0 },
	{ NULL,		NULL,	print_if_lpw_stats,	NULL,		"lpw", 0 },
	{ NULL,		NULL,		NULL,	NULL,	0 }
};

#ifdef AF_VSOCK
struct protox vsockprotox[] = {
	{ vsockpr,	vsockstats,	NULL,	NULL,		"vsock",   VSOCK_PROTO_STANDARD },
    { vsockpr,  vsockstats, NULL,   NULL,		"vsock_private", VSOCK_PROTO_PRIVATE },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};
#endif

struct protox unixprotox[] = {
	{ unixpr,		unixstats,	NULL,	NULL,		"unix", 0 },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

struct protox routeprotox[] = {
	{ rtsock_pcblist,		NULL,	NULL,	NULL,		"route", 0 },
	{ NULL,		NULL,		NULL,	NULL,		NULL,	0 }
};

struct protox *protoprotox[] = {
	protox,
#ifdef INET6
	ip6protox,
#endif
#ifdef IPSEC
	pfkeyprotox,
#endif
	systmprotox,
	nstatprotox,
	ipcprotox,
	kernprotox,
	routeprotox,
#ifdef AF_VSOCK
	vsockprotox,
#endif
	NULL
};

static void printproto (struct netstat_parameters *, struct protox *, char *);
static void usage (void);
static struct protox *name2protox (char *);
static struct protox *knownname (char *);
#ifdef SRVCACHE
extern void _serv_cache_close();
#endif
static void reinitalize_protocols(struct netstat_parameters *);


const static struct netstat_parameters netstat_params_initializer = {
	.Aflag = 0,
	.aflag = 0,
	.Bflag = 0,
	.bflag = 0,
	.cflag = 0,
	.dflag = 0,
	.Fflag = 0,
	.gflag = 0,
	.iflag = 0,
	.lflag = 0,
	.Lflag = 0,
	.mflag = 0,
	.nflag = 0,
	.pflag = 0,
	.Rflag = 0,
	.rflag = 0,
	.sflag = 0,
	.Sflag = 0,
	.prioflag = -1,
	.tflag = 0,
	.vflag = 0,
	.Wflag = 0,
	.qflag = 0,
	.Qflag = 0,
	.xflag = 0,
	.zflag = 0,
	.cq = -1,
	.interval = 0,
	.interface = NULL,
	.unit = 0,
	.af = AF_UNSPEC,
	.proto_name[0] = 0,
	.print_banner = 0,
	.cmd_args[0] = 0,
	.cmd_len = 0
};

void
netstat_init_parameters(struct netstat_parameters *params, size_t params_size)
{
	/*
	 * Note: We can re-use the same struct netstat_parameters over and over
	 * as long as "interface" is a plain pointer and not allocated
	 */
	bzero(params, params_size);
	if (params != NULL && params_size > 0) {
		memcpy(params, &netstat_params_initializer, params_size);
	}
}

static void
netstat_copy_args(struct netstat_parameters *params)
{
	if (optopt != '%') {
		int retval;

		retval = snprintf(params->cmd_args + params->cmd_len, sizeof(params->cmd_args) - params->cmd_len, "-%c ", optopt);
		if (retval > 0) {
			params->cmd_len += retval;
		}
		if (optarg != NULL) {
			retval = snprintf(params->cmd_args + params->cmd_len, sizeof(params->cmd_args) - params->cmd_len, "%s ", optarg);
			if (retval > 0) {
				params->cmd_len += retval;
			}
		}
	}
}

int
netstat_parse_parameters(int argc, char *argv[], struct netstat_parameters *params)
{
	struct protox *tp = NULL;  /* for printing cblocks & stats */
	int ch;
	int retval = 0;

	while ((ch = getopt(argc, argv, "AaBbc:dFf:gI:ikLlmnP:p:qQrRsStuvWw:xz%:")) != -1 && retval == 0) {
		netstat_copy_args(params);

		switch(ch) {
		case 'A':
			params->Aflag = 1;
			break;
		case 'a':
			params->aflag = 1;
			break;
		case 'B':
			if (optind < argc) {
				if (strcmp(argv[optind], "help") == 0) {
					bpf_help();
					optind++;
					continue;
				}
			}
			params->Bflag = 1;
			break;
		case 'b':
			params->bflag = 1;
			break;
		case 'c':
			params->cflag = 1;
			params->cq = atoi(optarg);
			break;
		case 'd':
			params->dflag = 1;
			break;
		case 'F':
			params->Fflag = 1;
			break;
		case 'f':
			if (strcmp(optarg, "ipx") == 0)
				params->af = AF_IPX;
			else if (strcmp(optarg, "inet") == 0)
				params->af = AF_INET;
#ifdef INET6
			else if (strcmp(optarg, "inet6") == 0)
				params->af = AF_INET6;
#endif /*INET6*/
#ifdef IPSEC
			else if (strcmp(optarg, "pfkey") == 0)
				params->af = PF_KEY;
#endif /*IPSEC */
			else if (strcmp(optarg, "unix") == 0)
				params->af = AF_UNIX;
			else if (strcmp(optarg, "systm") == 0)
				params->af = AF_SYSTEM;
			else if (strcmp(optarg, "route") == 0)
				params->af = AF_ROUTE;
#ifdef AF_VSOCK
			else if (strcmp(optarg, "vsock") == 0)
				params->af = AF_VSOCK;
#endif /*AF_VSOCK*/
			else {
				snprintf(params->errbuf, sizeof(params->errbuf), "%s: unknown address family", optarg);
				retval = -1;
				goto done;
			}
			break;
		case 'g':
			params->gflag = 1;
			break;
		case 'I': {
			char *cp;

			if (optarg[0] == '-') {
				warnx("# option -I requires an interface name");
				retval = -2;
				goto done;
			}

			params->iflag = 1;
			for (cp = params->interface = optarg; isalpha(*cp); cp++)
				continue;
			params->unit = atoi(cp);
			break;
		}
		case 'i':
			params->iflag = 1;
			break;
		case 'l':
			params->lflag += 1;
			break;
		case 'L':
			params->Lflag = 1;
			break;
		case 'm':
			params->mflag++;
			break;
		case 'n':
			params->nflag = 1;
			break;
		case 'P':
			params->prioflag = atoi(optarg);
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL) {
				warn("%s: unknown or uninstrumented protocol",
				     optarg);
				goto done;
			}
			snprintf(params->proto_name, sizeof(params->proto_name), "%s", optarg);
			params->pflag = 1;
			break;
		case 'q':
			params->qflag++;
			break;
		case 'Q':
			params->Qflag++;
			break;
		case 'R':
			params->Rflag = 1;
			break;
		case 'r':
			params->rflag = 1;
			break;
		case 's':
			++params->sflag;
			break;
		case 'S':
			params->Sflag = 1;
			break;
		case 't':
			params->tflag = 1;
			break;
		case 'u':
			params->af = AF_UNIX;
			break;
		case 'v':
			params->vflag++;
			break;
		case 'W':
			params->Wflag = 1;
			break;
		case 'w':
			params->interval = atoi(optarg);
			params->iflag = 1;
			break;
		case 'x':
			params->xflag = 1;
			params->Rflag = 1;
			break;
		case 'z':
			params->zflag = 1;
			break;
		case '%':
			params->print_banner = atoi(optarg);
			break;
		default:
			fprintf(stderr, "unexpected: '%c' (optind: %d)\n", optopt, optind);
		case '?':
			retval = -2;
			goto done;
		}
	}
done:
	return retval;
}

void
netstat_print_banner(struct netstat_parameters *params)
{
	if (params->print_banner != 0) {
		printf("#\n# %s %s\n#\n", getprogname(), params->cmd_args);
	}
}

int
netstat_run(struct netstat_parameters *params)
{
	struct protox *tp = NULL;  /* for printing cblocks & stats */

	if (params->print_banner == 1) {
		netstat_print_banner(params);
	}

	if (params->proto_name[0] != 0) {
		tp = name2protox(params->proto_name);
		if (tp == NULL) {
			snprintf(params->errbuf, sizeof(params->errbuf), "%s: unknown or uninstrumented protocol", params->proto_name);
			return -1;
		}
	}

	if (params->mflag) {
		mbpr(params);
		return(0);
	}
	if (params->Bflag) {
		bpf_stats(params, params->interface);
		return(0);
	}
	if (params->iflag && !params->sflag && !params->Sflag && !params->gflag && !params->qflag && !params->Qflag) {
		if (params->Rflag)
			return intpr_ri(params, NULL);
		else
			return intpr(params, NULL);
	}
	if (params->rflag) {
		if (params->sflag)
			rt_stats(params);
		else
			routepr(params);
		return(0);
	}
	if (params->qflag || params->Qflag) {
		if (params->qflag) {
			return aqstatpr(params);
		} else {
			return rxpollstatpr(params);
		}
	}
	if (params->Sflag) {
		print_link_status(params, params->interface);
		return(0);
	}

	if (params->gflag) {
		ifmalist_dump(params);
		return(0);
	}

	/* TCP/IP protocols */
	reinitalize_protocols(params);

	if (tp) {
		printproto(params, tp, tp->pr_name);
		return(0);
	}
	/*
	 * Avoid printing the interface statistics for each prototocol
	 */
	if (params->iflag && !params->pflag) {
		return intpr(params, NULL);
	}
	/*
	 * Go through all the protocols and address families
	 */
	if (params->af == AF_INET || params->af == AF_UNSPEC)
		for (tp = protox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == AF_INET6 || params->af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if ((params->af == AF_UNIX || params->af == AF_UNSPEC) && !params->Lflag) {
		for (tp = unixprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);
	}

	if ((params->af == AF_SYSTEM || params->af == AF_UNSPEC) && !params->Lflag)
		for (tp = systmprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == AF_UNSPEC && !params->Lflag)
		for (tp = nstatprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == AF_UNSPEC && !params->Lflag)
		for (tp = ipcprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == AF_UNSPEC && !params->Lflag)
		for (tp = kernprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == PF_KEY || params->af == AF_UNSPEC)
		for (tp = pfkeyprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if (params->af == PF_ROUTE || params->af == AF_UNSPEC)
		for (tp = routeprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);

	if ((params->af == AF_VSOCK || params->af == AF_UNSPEC) && !params->Lflag) {
		for (tp = vsockprotox; tp->pr_name; tp++)
			printproto(params, tp, tp->pr_name);
	}

#ifdef SRVCACHE
	_serv_cache_close();
#endif

	return 0;
}

int
main(int argc, char *argv[])
{
	struct netstat_parameters params = { 0 };
	int retval;
	int print_banner = 0;

	/* Skip the program name */
	argv += 1;
	argc -= 1;
	optind--;

again:
	netstat_init_parameters(&params, sizeof(struct netstat_parameters));

	/* The -% option is sticky */
	params.print_banner = -1;
	retval = netstat_parse_parameters(argc, argv, &params);
	if (retval == -2) {
			usage();
			exit(EX_USAGE);
	}
	if (params.print_banner != -1) {
		print_banner = params.print_banner;
	} else {
		params.print_banner = print_banner;
	}

	retval = netstat_run(&params);
	if (retval != 0) {
		warnx("%s", params.errbuf);
	}

	while (optind < argc) {
		if (strcmp(argv[optind], ",") == 0) {
			argv += optind + 1;
			argc -= optind + 1;
			optind = 0;
			goto again;
		}
		optind += 1;
	}

	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct netstat_parameters *params, register struct protox *tp, char *name)
{
	int (*pr)(struct netstat_parameters *, uint32_t, char *, int);
	uint32_t off;

	if (params->sflag) {
		pr = tp->pr_stats;
		if (!pr) {
			if (params->pflag && params->vflag)
				printf("%s: no stats routine\n",
					tp->pr_name);
			return;
		}
		off = tp->pr_protocol;
	} else {
		pr = tp->pr_cblocks;
		if (!pr) {
			if (params->pflag && params->vflag)
				printf("%s: no PCB routine\n", tp->pr_name);
			return;
		}
		off = tp->pr_protocol;
	}
	if (pr != NULL) {
		if (params->sflag && params->iflag && params->pflag && params->interval)
			intervalpr(params, pr, off, name, params->af);
		else
			(*pr)(params, off, name, params->af);
	} else {
		printf("### no stats for %s\n", name);
	}
}

void
printprotoifstats(struct netstat_parameters *params, char *ifname)
{
	struct protox **proto_table;

	for (proto_table = protoprotox; proto_table != NULL && *proto_table != NULL; proto_table++) {
		struct protox *tp;

		for (tp = *proto_table; tp->pr_name != NULL; tp++) {
			if (tp->pr_istats != NULL) {
				tp->pr_istats(params, ifname);
			}
		}
	}
}

char *
plural(int n)
{
	return (n > 1 ? "s" : "");
}

char *
plurales(int n)
{
	return (n > 1 ? "es" : "");
}

char *
pluralies(int n)
{
	return (n > 1 ? "ies" : "y");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

static void
reinitalize_protocols(struct netstat_parameters *params)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++) {
		for (tp = *tpp; tp->pr_name; tp++) {
			if (tp->pr_reinit != NULL) {
				tp->pr_reinit(params, tp->pr_name, tp->pr_protocol, AF_UNSPEC);
			}
		}
	}
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)) != NULL)
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent()) != NULL) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				tp = knownname(p->p_name);
				endprotoent();
				return tp;
			}
	}
	endprotoent();
	return (NULL);
}

#define	NETSTAT_USAGE "\
Usage:	netstat [-AaLlnW] [-f address_family | -p protocol]\n\
	netstat [-gilns] [-f address_family]\n\
	netstat -i | -I interface [-w wait] [-abdgRtS]\n\
	netstat -s [-s] [-f address_family | -p protocol] [-w wait]\n\
	netstat -i | -I interface -s [-f address_family | -p protocol]\n\
	netstat -m [-m]\n\
	netstat -r [-Aaln] [-f address_family]\n\
	netstat -rs [-s]\n\
"

static void
usage(void)
{
	(void) fprintf(stderr, "%s\n", NETSTAT_USAGE);
}

int
print_time(void)
{
    time_t now;
    struct tm tm;
    int num_written = 0;
    
    (void) time(&now);
    (void) localtime_r(&now, &tm);
    
    num_written += printf("%02d:%02d:%02d ", tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    return (num_written);
}


void
print_socket_stats_format(struct netstat_parameters *params)
{
	if (params->bflag > 0 || params->vflag > 0) {
		if (params->vflag > 1) {
			printf(" %12.12s/%-9.9s %12.12s/%-9.9s", "rxbytes", "packets", "txbytes", "packets");
		} else {
			printf(" %12.12s %12.12s", "rxbytes", "txbytes");
		}
		if (params->dflag > 0) {
			if (params->vflag > 1) {
				printf(" %12.12s/%-9.9s", "rxdrops", "packets");
			} else {
				printf(" %12.12s", "rxdrops");
			}
		}
	}
	if (params->vflag > 0) {
		if (params->vflag > 1) {
			printf(" %7.7s %7.7s %16s:%-6s %16s:%-6s",
			       "rhiwat", "shiwat", "process", "pid", "eprocess", "epid");
		} else {
			printf(" %7.7s %7.7s %16s:%-6s",
			       "rhiwat", "shiwat", "process", "pid");
		}

		printf(" %5.5s %8.8s %16.16s %8.8s %8.8s %6s %6s %5s",
		       "state", "options", "gencnt", "flags", "flags1", "usecnt", "rtncnt", "fltrs");
	}

}

void
print_socket_stats_data(struct netstat_parameters *params, struct xsocket_n *so, struct xsockbuf_n *so_rcv, struct xsockbuf_n *so_snd, struct xsockstat_n *so_stat)
{
	if (params->bflag > 0 || params->vflag > 0) {
		if (params->vflag > 1) {
			printf(" %12llu/%-9llu %12llu/%-9llu",
			       so_stat->xst_tc_stats[SO_STATS_DATA].rxbytes,
			       so_stat->xst_tc_stats[SO_STATS_DATA].rxpackets,
			       so_stat->xst_tc_stats[SO_STATS_DATA].txbytes,
			       so_stat->xst_tc_stats[SO_STATS_DATA].txpackets);
		} else {
			printf(" %12llu %12llu",
			       so_stat->xst_tc_stats[SO_STATS_DATA].rxbytes,
			       so_stat->xst_tc_stats[SO_STATS_DATA].txbytes);
		}
		if (params->dflag > 0) {
			if (params->vflag > 1) {
				printf(" %12llu/%-9llu",
				       so_stat->xst_tc_stats[SO_STATS_SBNOSPACE].rxbytes,
				       so_stat->xst_tc_stats[SO_STATS_SBNOSPACE].rxpackets);
			} else {
				printf(" %12llu",
				       so_stat->xst_tc_stats[SO_STATS_SBNOSPACE].rxbytes);
			}
		}
	}
	if (params->vflag > 0) {
		char namebuf[32] = { 0 };

		proc_name(so->so_last_pid, namebuf, sizeof(namebuf));

		if (params->vflag > 1) {
			char epidbuf[16] = { 0 };
			char enamebuf[32] = { 0 };

			if (so->so_e_pid != 0) {
				snprintf(epidbuf, sizeof(epidbuf),"%d", so->so_e_pid);
				proc_name(so->so_e_pid, enamebuf, sizeof(enamebuf));
			}
			printf(" %7u %7u %16s:%-6u %16s:%-6s",
			       so_rcv->sb_hiwat,
			       so_snd->sb_hiwat,
			       namebuf,
			       so->so_last_pid,
			       enamebuf,
			       epidbuf);
		} else {
			printf(" %7u %7u %16s:%-6u",
			       so_rcv->sb_hiwat,
			       so_snd->sb_hiwat,
			       namebuf,
			       so->so_last_pid);
		}

		printf(" %05x %08x %016llx %08x %08x %6d %6d %06x",
		       so->so_state,
		       so->so_options,
		       so->so_gencnt,
		       so->so_flags,
		       so->so_flags1,
		       so->so_usecount,
		       so->so_retaincnt,
		       so->xso_filter_flags);
	}
}
