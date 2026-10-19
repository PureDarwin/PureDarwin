/*
 * Copyright 2009-2020, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Copyright (c) 1983, 1987, 1989
 *    The Regents of the University of California.  All rights reserved.
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

#ifndef _RESOLV_H_
#define	_RESOLV_H_

#include <stdint.h>
#include <stdio.h>
#include <arpa/nameser.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	__RES	19991006

struct res_sym {
	int			number;	   /* Identifying number, like T_MX */
	const char *name;	   /* Its symbolic name, like "MX" */
	const char *humanname; /* Its fun name, like "mail exchanger" */
};

/*
 * Global defines and variables for resolver stub.
 */
#define	MAXNS				3	/* max # name servers we'll track */
#define	MAXDFLSRCH			3	/* # default domain levels to try */
#define	MAXDNSRCH			6	/* max # domains in search path */
#define	LOCALDOMAINPARTS	2	/* min levels in name that is "local" */
#define	RES_TIMEOUT			5	/* min. seconds between retries */
#define	MAXRESOLVSORT		10	/* number of net to sort on */
#define	RES_MAXNDOTS		15	/* should reflect bit field size */
#define	RES_MAXRETRANS		30	/* only for resolv.conf/RES_OPTIONS */
#define	RES_MAXRETRY		5	/* only for resolv.conf/RES_OPTIONS */
#define	RES_DFLRETRY		2	/* Default #/tries. */
#define	RES_MAXTIME			65535	/* Infinity, in milliseconds. */

struct __res_state {
	int	retrans;		 	/* retransmission time interval */
	int	retry;				/* number of times to retransmit */
	unsigned long options;	/* option flags - see below. */
	int	nscount;			/* number of name servers */
	struct sockaddr_in
		nsaddr_list[MAXNS];	/* address of name server */
	unsigned short	id;		/* current message id */
	char	*dnsrch[MAXDNSRCH+1];	/* components of domain to search */
	char	defdname[256];	/* default domain (deprecated) */
	unsigned long	pfcode;	/* RES_PRF_ flags - see below. */
	unsigned ndots:4;		/* threshold for initial abs. query */
	unsigned nsort:4;		/* number of elements in sort_list[] */
	char	unused[3];
	struct {
		struct in_addr	addr;
		uint32_t	mask;
	} sort_list[MAXRESOLVSORT];
	void* qhook;	/* query hook */
	void* rhook;	/* response hook */
	int	res_h_errno;		/* last one set for this context */
	int	_vcsock;			/* PRIVATE: for res_send VC i/o */
	unsigned int	_flags;			/* PRIVATE: see below */
	unsigned char	_rnd[16];		/* PRIVATE: random state */
	unsigned int	_pad;			/* make _u 64 bit aligned */
	union {
		/* On an 32-bit arch this means 512b total. */
		char	pad[56 - 4*sizeof (int) - 2*sizeof (void *)];
		struct {
			uint16_t		nscount;
			uint16_t		nstimes[MAXNS];	/* ms. */
			int			nssocks[MAXNS];
			struct __res_state_ext *ext;	/* extention for IPv6 */
		} _ext;
	} _u;
};

typedef struct __res_state *res_state;

/* resolver flags; for _flags in __res_state. */
#define	RES_F_VC		0x00000001	/* socket is TCP */
#define	RES_F_CONN		0x00000002	/* socket is connected */
#define	RES_F_EDNS0ERR	0x00000004	/* EDNS0 caused errors */

/* res_findzonecut2() options */
#define	RES_EXHAUSTIVE	0x00000001	/* always do all queries */
#define	RES_IPV4ONLY	0x00000002	/* IPv4 only */
#define	RES_IPV6ONLY	0x00000004	/* IPv6 only */

/* resolver options */
#define RES_INIT		(1 << 0)	/* address initialized */
#define RES_DEBUG		(1 << 1)	/* print debug messages */
#define RES_AAONLY		(1 << 2)	/* authoritative answers only (!IMPL)*/
#define RES_USEVC		(1 << 3)	/* use virtual circuit */
#define RES_PRIMARY		(1 << 4)	/* query primary server only (!IMPL) */
#define RES_IGNTC		(1 << 5)	/* ignore trucation errors */
#define RES_RECURSE		(1 << 6)	/* recursion desired */
#define RES_DEFNAMES	(1 << 7)	/* use default domain name */
#define RES_STAYOPEN	(1 << 8)	/* Keep TCP socket open */
#define RES_DNSRCH		(1 << 9)	/* search up local domain tree */
#define	RES_INSECURE1	(1 << 10)	/* type 1 security disabled */
#define	RES_INSECURE2	(1 << 11)	/* type 2 security disabled */
#define	RES_NOALIASES	(1 << 12)	/* shuts off HOSTALIASES feature */
#define	RES_USE_INET6	(1 << 13)	/* use/map IPv6 in gethostbyname() */
#define RES_ROTATE		(1 << 14)	/* rotate ns list after each query */
#define	RES_NOCHECKNAME	(1 << 15)	/* do not check names for sanity. */
#define	RES_KEEPTSIG	(1 << 16)	/* do not strip TSIG records */
#define	RES_BLAST		(1 << 17)	/* blast all recursive servers */

#define RES_NOTLDQUERY	(1 << 20)	/* don't unqualified name as a tld */
#define RES_USE_DNSSEC	(1 << 21)	/* use DNSSEC using OK bit in OPT */

#define RES_USE_INET4	(1 << 23)	/* use IPv4 in gethostbyname() */

/* KAME extensions: use higher bit to avoid conflict with ISC use */
#define RES_USE_EDNS0	0x40000000	/* use EDNS0 if configured */
#define RES_NO_NIBBLE2	0x80000000	/* disable alternate nibble lookup */

#define RES_DEFAULT	\
	(RES_RECURSE | RES_DEFNAMES | RES_DNSRCH | RES_NO_NIBBLE2)

/* resolver "pfcode" values, used by dig. */
#define RES_PRF_STATS	0x00000001
#define RES_PRF_UPDATE	0x00000002
#define RES_PRF_CLASS   0x00000004
#define RES_PRF_CMD		0x00000008
#define RES_PRF_QUES	0x00000010
#define RES_PRF_ANS		0x00000020
#define RES_PRF_AUTH	0x00000040
#define RES_PRF_ADD		0x00000080
#define RES_PRF_HEAD1	0x00000100
#define RES_PRF_HEAD2	0x00000200
#define RES_PRF_TTLID	0x00000400
#define RES_PRF_HEADX	0x00000800
#define RES_PRF_QUERY	0x00001000
#define RES_PRF_REPLY	0x00002000
#define RES_PRF_INIT	0x00004000

/* Things involving an internal (static) resolver context. */
extern struct __res_state *__res_state(void) __attribute__ ((__const__));
#define _res (*__res_state())

#define fp_nquery		__fp_nquery
#define fp_query		__fp_query
#define hostalias		__hostalias
#define p_query			__p_query
#define res_close		__res_close
#define res_init		__res_init
#define res_isourserver	__res_isourserver
#define res_mkquery		__res_mkquery
#define res_query		__res_query
#define res_querydomain	__res_querydomain
#define res_search		__res_search
#define res_send		__res_send

void		fp_nquery(const unsigned char *, int, FILE *);
void		fp_query(const unsigned char *, FILE *);
const char *hostalias(const char *);
void		p_query(const unsigned char *);
void		res_close(void);
int			res_init(void);
int			res_isourserver(const struct sockaddr_in *);
int			res_mkquery(int, const char *, int, int, const unsigned char *,
				 int, const unsigned char *, unsigned char *, int);
int			res_query(const char *, int, int, unsigned char *, int);
int			res_querydomain(const char *, const char *, int, int,
				unsigned char *, int);
int			res_search(const char *, int, int, unsigned char *, int);
int			res_send(const unsigned char *, int, unsigned char *, int);

#define b64_ntop			__b64_ntop
#define b64_pton			__b64_pton
#define dn_comp				__dn_comp
#define dn_count_labels		__dn_count_labels
#define dn_expand			__dn_expand
#define dn_skipname			__dn_skipname
#define fp_resstat			__fp_resstat
#define loc_aton			__loc_aton
#define loc_ntoa			__loc_ntoa
#define p_cdname			__p_cdname
#define p_cdnname			__p_cdnname
#define p_class				__p_class
#define p_fqname			__p_fqname
#define p_fqnname			__p_fqnname
#define p_option			__p_option
#define p_time				__p_time
#define p_type				__p_type
#define p_rcode				__p_rcode
#define res_dnok			__res_dnok
#define res_hnok			__res_hnok
#define res_hostalias		__res_hostalias
#define res_mailok			__res_mailok
#define res_nameinquery		__res_nameinquery
#define res_nclose			__res_nclose
#define res_ninit			__res_ninit
#define res_nmkquery		__res_nmkquery
#define res_nquery			__res_nquery
#define res_nquerydomain	__res_nquerydomain
#define res_nsearch			__res_nsearch
#define res_nsend			__res_nsend
#define res_ownok			__res_ownok
#define res_queriesmatch	__res_queriesmatch
#define res_randomid		__res_randomid
#define sym_ntop			__sym_ntop
#define sym_ntos			__sym_ntos
#define sym_ston			__sym_ston

int				res_hnok(const char *);
int				res_ownok(const char *);
int				res_mailok(const char *);
int				res_dnok(const char *);
int				sym_ston(const struct res_sym *, const char *, int *);
const char *	sym_ntos(const struct res_sym *, int, int *);
const char *	sym_ntop(const struct res_sym *, int, int *);
int				b64_ntop(unsigned char const *, size_t, char *, size_t);
int				b64_pton(char const *, unsigned char *, size_t);
int				loc_aton(const char *, unsigned char *);
const char *	loc_ntoa(const unsigned char *, char *);
int				dn_skipname(const unsigned char *, const unsigned char *);
const char *	p_class(int);
const char *	p_time(uint32_t);
const char *	p_type(int);
const char *	p_rcode(int);
const unsigned char * p_cdnname(const unsigned char *, const unsigned char *,
						int, FILE *);
const unsigned char * p_cdname(const unsigned char *, const unsigned char *,
						FILE *);
const unsigned char * p_fqnname(const unsigned char *, const unsigned char *,
						int, char *, int);
const unsigned char * p_fqname(const unsigned char *, const unsigned char *,
						FILE *);
const char *	p_option(unsigned long);
int				dn_count_labels(const char *);
int				dn_comp(const char *, unsigned char *, int, unsigned char **,
					unsigned char **);
int				dn_expand(const unsigned char *, const unsigned char *,
					const unsigned char *, char *, int);
unsigned int	res_randomid(void);
int				res_nameinquery(const char *, int, int, const unsigned char *,
					const unsigned char *);
int				res_queriesmatch(const unsigned char *, const unsigned char *,
					const unsigned char *, const unsigned char *);

/* Things involving a resolver context. */
int				res_ninit(res_state);
void			fp_resstat(const res_state, FILE *);
const char *	res_hostalias(const res_state, const char *, char *, size_t);
int				res_nquery(res_state, const char *, int, int, unsigned char *,
					int);
int				res_nsearch(res_state, const char *, int, int, unsigned char *,
					int);
int				res_nquerydomain(res_state, const char *, const char *, int,
					int, unsigned char *, int);
int				res_nmkquery(res_state, int, const char *, int, int,
					const unsigned char *, int, const unsigned char *,
					unsigned char *, int);
int				res_nsend(res_state, const unsigned char *, int, unsigned char *,
					int);
void			res_nclose(res_state);

#ifdef __cplusplus
}
#endif

#endif /* !_RESOLV_H_ */
