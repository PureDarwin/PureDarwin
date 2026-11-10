/*
 * Copyright (c) 2008-2012, 2021 Apple Inc. All rights reserved.
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
/*	$FreeBSD: src/usr.bin/netstat/ipsec.c,v 1.1.2.3 2001/08/10 09:07:09 ru Exp $	*/
/*	$NetBSD: inet.c,v 1.35.2.1 1999/04/29 14:57:08 perry Exp $	*/
/*	$KAME: ipsec.c,v 1.25 2001/03/12 09:04:39 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/keysock.h>
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#if defined(__APPLE__) && !defined(__unused)
#define __unused
#endif

/*
 * portability issues:
 * - bsdi[34] uses PLURAL(), not plural().
 * - freebsd2 can't print "unsigned long long" properly.
 */
/*
 * XXX see PORTABILITY for the twist
 */
#define LLU	"%llu"
#define CAST	unsigned long long

#ifdef IPSEC

#ifdef XSO_KEYPCB
#include <netkey/keysock_private.h>

#define ROUNDUP64(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint64_t) - 1))) : sizeof(uint64_t))
#define ADVANCE64(x, n) (((char *)x) += ROUNDUP64(n))

#define ALL_XGN_KIND_ROUTEPCB (XSO_SOCKET | XSO_RCVBUF | XSO_SNDBUF | XSO_STATS | XSO_KEYPCB)

struct xgen_n {
	u_int32_t	xgn_len;	/* length of this structure */
	u_int32_t	xgn_kind;	/* type of structure */
};
#endif /* XSO_KEYPCB */

struct val2str {
	int val;
	const char *str;
};

static struct val2str ipsec_ahnames[] = {
	{ SADB_AALG_NONE, "none", },
	{ SADB_AALG_MD5HMAC, "hmac-md5", },
	{ SADB_AALG_SHA1HMAC, "hmac-sha1", },
	{ SADB_X_AALG_MD5, "md5", },
	{ SADB_X_AALG_SHA, "sha", },
	{ SADB_X_AALG_NULL, "null", },
#ifdef SADB_X_AALG_SHA2_256
	{ SADB_X_AALG_SHA2_256, "hmac-sha2-256", },
#endif
#ifdef SADB_X_AALG_SHA2_384
	{ SADB_X_AALG_SHA2_384, "hmac-sha2-384", },
#endif
#ifdef SADB_X_AALG_SHA2_512
	{ SADB_X_AALG_SHA2_512, "hmac-sha2-512", },
#endif
	{ -1, NULL },
};

static struct val2str ipsec_espnames[] = {
	{ SADB_EALG_NONE, "none", },
	{ SADB_EALG_DESCBC, "des-cbc", },
	{ SADB_EALG_3DESCBC, "3des-cbc", },
	{ SADB_EALG_NULL, "null", },
#ifdef SADB_X_EALG_RC5CBC
	{ SADB_X_EALG_RC5CBC, "rc5-cbc", },
#endif
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
#ifdef SADB_X_EALG_RIJNDAELCBC
	{ SADB_X_EALG_RIJNDAELCBC, "rijndael-cbc", },
#endif
#ifdef SADB_X_EALG_AES_GCM
	{ SADB_X_EALG_AES_GCM, "aes-gcm", },
#endif
#ifdef SADB_X_EALG_CHACHA20POLY1305
	{ SADB_X_EALG_CHACHA20POLY1305, "chacha20-poly1305", },
#endif
#ifdef SADB_X_EALG_AES_GMAC
    { SADB_X_EALG_AES_GMAC, "aes-gmac", },
#endif
	{ -1, NULL },
};

static struct val2str ipsec_compnames[] = {
	{ SADB_X_CALG_NONE, "none", },
	{ SADB_X_CALG_OUI, "oui", },
	{ SADB_X_CALG_DEFLATE, "deflate", },
	{ SADB_X_CALG_LZS, "lzs", },
	{ -1, NULL },
};

static const char *pfkey_msgtypenames[] = {
	"reserved", "getspi", "update", "add", "delete",
	"get", "acquire", "register", "expire", "flush",
	"dump", "x_promisc", "x_pchange", "x_spdupdate", "x_spdadd",
	"x_spddelete", "x_spdget", "x_spdacquire", "x_spddump", "x_spdflush",
	"x_spdsetidx", "x_spdexpire", "x_spddelete2"
};

static struct ipsecstat pipsecstat;
static struct ipsecstat ipsecstat;

static void print_ipsecstats (struct netstat_parameters *params);
static const char *pfkey_msgtype_names (int);
static void ipsec_hist (const u_quad_t *, const u_quad_t *, size_t,
    const struct val2str *, const char *);

/*
 * Dump IPSEC statistics structure.
 */
static void
ipsec_hist(const u_quad_t *hist,
	   const u_quad_t *phist,
	   size_t histmax,
	   const struct val2str *name,
	   const char *title)
{
	int first;
	size_t proto;
	const struct val2str *p;

	first = 1;
	for (proto = 0; proto < histmax; proto++) {
		if ((hist[proto] - phist[proto]) <= 0)
			continue;
		if (first) {
			printf("\t%s histogram:\n", title);
			first = 0;
		}
		for (p = name; p && p->str; p++) {
			if (p->val == (int)proto)
				break;
		}
		if (p && p->str) {
			printf("\t\t%s: " LLU "\n", p->str,
			    (CAST)hist[proto] - (CAST)phist[proto]);
		} else {
			printf("\t\t#%ld: " LLU "\n", (long)proto,
			    (CAST)hist[proto] - (CAST)phist[proto]);
		}
	}
}

static void
print_ipsecstats(struct netstat_parameters *params)
{
#define	IPSECDIFF(f) (ipsecstat.f - pipsecstat.f)
#define	p(f, m) if (IPSECDIFF(f) || params->sflag <= 1) \
    printf(m, (CAST)IPSECDIFF(f), plural(IPSECDIFF(f)))
#define hist(f, n, t) \
    ipsec_hist(ipsecstat.f, pipsecstat.f, \
        sizeof(ipsecstat.f)/sizeof(ipsecstat.f[0]), (n), (t));

    if (params->interval && params->vflag > 0)
        print_time();

	p(in_success, "\t" LLU " inbound packet%s processed successfully\n");
	p(in_polvio, "\t" LLU " inbound packet%s violated process security "
		"policy\n");
	p(in_nosa, "\t" LLU " inbound packet%s with no SA available\n");
	p(in_inval, "\t" LLU " invalid inbound packet%s\n");
	p(in_nomem, "\t" LLU " inbound packet%s failed due to insufficient memory\n");
	p(in_badspi, "\t" LLU " inbound packet%s failed getting SPI\n");
	p(in_ahreplay, "\t" LLU " inbound packet%s failed on AH replay check\n");
	p(in_espreplay, "\t" LLU " inbound packet%s failed on ESP replay check\n");
	p(in_ahauthsucc, "\t" LLU " inbound packet%s considered authentic by AH\n");
	p(in_ahauthfail, "\t" LLU " inbound packet%s failed on AH authentication\n");
	p(in_espauthsucc, "\t" LLU " inbound packet%s considered authentic by ESP\n");
	p(in_espauthfail, "\t" LLU " inbound packet%s failed on ESP authentication\n");
	hist(in_ahhist, ipsec_ahnames, "AH input");
	hist(in_esphist, ipsec_espnames, "ESP input");
	hist(in_comphist, ipsec_compnames, "IPComp input");

	p(out_success, "\t" LLU " outbound packet%s processed successfully\n");
	p(out_polvio, "\t" LLU " outbound packet%s violated process security "
		"policy\n");
	p(out_nosa, "\t" LLU " outbound packet%s with no SA available\n");
	p(out_inval, "\t" LLU " invalid outbound packet%s\n");
	p(out_nomem, "\t" LLU " outbound packet%s failed due to insufficient memory\n");
	p(out_noroute, "\t" LLU " outbound packet%s with no route\n");
	hist(out_ahhist, ipsec_ahnames, "AH output");
	hist(out_esphist, ipsec_espnames, "ESP output");
	hist(out_comphist, ipsec_compnames, "IPComp output");
#undef IPSECDIFF
#undef p
#undef hist
}

int
ipsec_stats(struct netstat_parameters *params, uint32_t off __unused, char *name, int af __unused)
{
	size_t len;
	
	len = sizeof(struct ipsecstat);
	if (strcmp(name, "ipsec") == 0) {
		if (sysctlbyname("net.inet.ipsec.stats", &ipsecstat, &len, 0, 0) == -1)
			return 0;
	} else if (strcmp(name, "ipsec6") == 0) {
		if (sysctlbyname("net.inet6.ipsec6.stats", &ipsecstat, &len, 0, 0) == -1)
			return 0;
	} else
		return 0;
	printf ("%s:\n", name);

	print_ipsecstats(params);

	if (params->interval > 0)
		bcopy(&ipsecstat, &pipsecstat, len);

	return 0;
}

static const char *
pfkey_msgtype_names(int x)
{
	const int max =
	    sizeof(pfkey_msgtypenames)/sizeof(pfkey_msgtypenames[0]);
	static char buf[10];

	if (x < max && pfkey_msgtypenames[x])
		return pfkey_msgtypenames[x];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

int
pfkey_stats(struct netstat_parameters *params, uint32_t off __unused, char *name, int af __unused)
{
	static struct pfkeystat ppfkeystat;
	struct pfkeystat pfkeystat;
	u_int pcbcount;
	unsigned first, type;
	size_t len;
	
	len = sizeof(struct pfkeystat);
	if (sysctlbyname("net.key.pfkeystat", &pfkeystat, &len, 0, 0) == -1)
		return -1;

    if (params->interval && params->vflag > 0)
        print_time();
	printf ("%s:\n", name);

#define	PFKEYDIFF(f) (pfkeystat.f - ppfkeystat.f)
#define	p(f, m) if (PFKEYDIFF(f) || params->sflag <= 1) \
    printf(m, (CAST)PFKEYDIFF(f), plural(PFKEYDIFF(f)))

	/* kernel -> userland */
	p(out_total, "\t" LLU " request%s sent to userland\n");
	p(out_bytes, "\t" LLU " byte%s sent to userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.out_msgtype)/sizeof(pfkeystat.out_msgtype[0]);
	     type++) {
		if (PFKEYDIFF(out_msgtype[type]) <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: " LLU "\n", pfkey_msgtype_names(type),
			(CAST)PFKEYDIFF(out_msgtype[type]));
	}
	p(out_invlen, "\t" LLU " message%s with invalid length field\n");
	p(out_invver, "\t" LLU " message%s with invalid version field\n");
	p(out_invmsgtype, "\t" LLU " message%s with invalid message type field\n");
	p(out_tooshort, "\t" LLU " message%s too short\n");
	p(out_nomem, "\t" LLU " message%s with memory allocation failure\n");
	p(out_dupext, "\t" LLU " message%s with duplicate extension\n");
	p(out_invexttype, "\t" LLU " message%s with invalid extension type\n");
	p(out_invsatype, "\t" LLU " message%s with invalid sa type\n");
	p(out_invaddr, "\t" LLU " message%s with invalid address extension\n");

	/* userland -> kernel */
	p(in_total, "\t" LLU " request%s sent from userland\n");
	p(in_bytes, "\t" LLU " byte%s sent from userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.in_msgtype)/sizeof(pfkeystat.in_msgtype[0]);
	     type++) {
		if (PFKEYDIFF(in_msgtype[type]) <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: " LLU "\n", pfkey_msgtype_names(type),
			(CAST)PFKEYDIFF(in_msgtype[type]));
	}
	p(in_msgtarget[KEY_SENDUP_ONE],
	    "\t" LLU " message%s toward single socket\n");
	p(in_msgtarget[KEY_SENDUP_ALL],
	    "\t" LLU " message%s toward all sockets\n");
	p(in_msgtarget[KEY_SENDUP_REGISTERED],
	    "\t" LLU " message%s toward registered sockets\n");
	p(in_nomem, "\t" LLU " message%s with memory allocation failure\n");

	if (params->interval > 0)
		bcopy(&pfkeystat, &ppfkeystat, len);
#undef PFKEYDIFF
#undef p

	len = sizeof(pcbcount);
	if (sysctlbyname("net.key.pcbcount", &pcbcount, &len, 0, 0) == 0) {
		if (pcbcount != 0 || params->sflag <= 1 ) {
			printf("\t%u open pfkey socket%s\n", pcbcount, plural(pcbcount));
		}
	}

	return 0;
}

int
keysockpr(struct netstat_parameters *params, uint32_t proto, char *name, int af)
{
	int error = 0;

	printf("Active pfkey sockets\n");

#ifdef XSO_KEYPCB
	char *buf = NULL, *next;
	size_t len;
	struct xrtsockgen *xrg, *oxrg;
	struct xgen_n *xgn;
	struct xrtsockpcb *xrp = NULL;
	struct xsocket_n *so = NULL;
	struct xsockbuf_n *so_rcv = NULL;
	struct xsockbuf_n *so_snd = NULL;
	struct xsockstat_n *so_stat = NULL;
	const char *mibvar = "net.route.pcblist";
	int which = 0;
	int first = 1;

	len = 0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		warn("sysctl: %s for len", mibvar);
		return -1;
	}
	if ((buf = malloc(len)) == 0) {
		warn("malloc %lu bytes", (u_long)len);
		return -1;
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		warn("sysctl: %s for buf len %zd", mibvar, len);
		free(buf);
		return -1;
	}

	oxrg = xrg = (struct xrtsockgen *)buf;
	xgn = (struct xgen_n*)buf;

	for (next = buf + ROUNDUP64(xgn->xgn_len); next < buf + len; next += ROUNDUP64(xgn->xgn_len)) {
		xgn = (struct xgen_n*)next;

		if (xgn->xgn_len <= sizeof(struct xgen_n))
			break;
		if ((which & xgn->xgn_kind) == 0) {
			which |= xgn->xgn_kind;
			switch (xgn->xgn_kind) {
				case XSO_SOCKET:
					so = (struct xsocket_n *)xgn;
					break;
				case XSO_RCVBUF:
					so_rcv = (struct xsockbuf_n *)xgn;
					break;
				case XSO_SNDBUF:
					so_snd = (struct xsockbuf_n *)xgn;
					break;
				case XSO_STATS:
					so_stat = (struct xsockstat_n *)xgn;
					break;
				case XSO_KEYPCB:
					xrp = (struct xrtsockpcb *)xgn;
					break;
				default:
					/* It's OK to have some extra bytes at the end */
					if (which != 0 && params->vflag > 2) {
						printf("unexpected kind %d which 0x%x\n", xgn->xgn_kind, which);
					}
					break;
			}
		}
		if (first) {
			printf("%-6.6s %-6.6s %-6.6s ",
				   "Proto", "Recv-Q", "Send-Q");
			if (params->lflag) {
				printf("%-40.40s %-40.40s ",
					   "Local Address", "Foreign Address");
			} else {
				printf("%-18.18s %-18.18s ",
					   "Local Address", "Foreign Address");
			}
			print_socket_stats_format(params);

			printf("\n");
			first = 0;
		}
		if (which == ALL_XGN_KIND_ROUTEPCB) {
			which = 0;
			char buffer[10];

			if (xrp->xrp_laddr.sa_family == PF_KEY_V2) {
				snprintf(buffer, sizeof(buffer), "%s", "v2");
			} else {
				snprintf(buffer, sizeof(buffer), "%d", xrp->xrp_laddr.sa_family);
			}

			printf("%-6s %6u %6u ", buffer, so_rcv->sb_cc, so_snd->sb_cc);

#define SIN(x) ((struct sockaddr_in *)(x))
#define SIN6(x) ((struct sockaddr_in6 *)(x))
			if (xrp->xrp_laddr.sa_family == AF_INET) {
				in_addr_print(params, &SIN(&xrp->xrp_laddr)->sin_addr);
			} else if (xrp->xrp_laddr.sa_family == AF_INET6) {
				in6_addr_print(params, &SIN6(&xrp->xrp_laddr)->sin6_addr);
			} else {
				if (params->lflag) {
					printf("%-40.40s ",
						   "");
				} else {
					printf("%-18.18s ",
						   "");
				}
			}

			if (xrp->xrp_faddr.sa_family == AF_INET) {
				in_addr_print(params, &SIN(&xrp->xrp_faddr)->sin_addr);
			} else if (xrp->xrp_faddr.sa_family == AF_INET6) {
				in6_addr_print(params, &SIN6(&xrp->xrp_faddr)->sin6_addr);
			} else {
				if (params->lflag) {
					printf("%-40.40s ",
						   "");
				} else {
					printf("%-18.18s ",
						   "");
				}
			}
#undef SIN
#undef SIN6

			print_socket_stats_data(params, so, so_rcv, so_snd, so_stat);
			printf("\n");
		}
	}
	free(buf);
#endif /* XSO_KEYPCB */
	return error;
}
#endif /*IPSEC*/
