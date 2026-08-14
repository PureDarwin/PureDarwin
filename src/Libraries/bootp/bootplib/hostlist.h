/*
 * Copyright (c) 1999 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * hostlist.h
 * - definitions for host list structures and functions
 */

#ifndef _S_HOSTLIST_H
#define _S_HOSTLIST_H

#include <netinet/in.h>

struct hosts {
	struct hosts	*next;
	struct hosts	*prev;
	struct in_addr	iaddr;		/* internet address */
	u_char		htype;		/* hardware type */
	u_char		hlen;		/* hardware length */
	union {				/* hardware address */
	    struct ether_addr 	en;
	    u_char		generic[256];
	} haddr;
	char *		hostname;	/* host name (and suffix) */
	char *		bootfile;	/* default boot file name */
	struct timeval	tv;		/* time-in */

        u_long		lease;		/* lease (dhcp only) */
};

struct hosts * 	hostadd(struct hosts * * hosts, struct timeval * tv_p, 
			int htype, char * haddr, int hlen, 
			struct in_addr * iaddr_p, char * host_name,
			char * bootfile);
void		hostfree(struct hosts * * hosts, struct hosts * hp);
void		hostinsert(struct hosts * * hosts, struct hosts * hp);
void		hostprint(struct hosts * hp);
void		hostremove(struct hosts * * hosts, struct hosts * hp);

typedef boolean_t subnet_match_func_t(void * arg, struct in_addr iaddr);

static __inline__ struct hosts *
hostbyip(struct hosts * hosts, struct in_addr iaddr)
{
    struct hosts * hp;
    for (hp = hosts; hp; hp = hp->next) {
	if (iaddr.s_addr == hp->iaddr.s_addr)
	    return (hp);
    }
    return (NULL);
}

static __inline__ struct hosts *
hostbyaddr(struct hosts * hosts, u_char hwtype, void * hwaddr, int hwlen,
	   subnet_match_func_t * func, void * arg)
{
    struct hosts * hp;

    for (hp = hosts; hp; hp = hp->next) {
	if (hwtype == hp->htype 
	    && hwlen == hp->hlen
	    && bcmp(hwaddr, &hp->haddr, hwlen) == 0) {
	    if (func == NULL
		|| (*func)(arg, hp->iaddr)) {
		return (hp);
	    }
	}
    }
    return (NULL);
}


#endif /* _S_HOSTLIST_H */
