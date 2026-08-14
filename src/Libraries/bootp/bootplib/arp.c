/*
 * Copyright (c) 2000-2011 Apple Inc. All rights reserved.
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
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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

/*
 * Modification History:
 *
 * 25 Feb 1998	Dieter Siegmund (dieter@apple.com)
 * - significantly restructured to make it useful for inclusion
 *   in a library
 *   - define MAIN to generate the arp command
 * - removed use of most global variables, abstracted
 *   out the ability to call arp_set() as a library
 *   routine, moved error messages to error codes
 *
 * 22 Sep 2000	Dieter Siegmund (dieter@apple.com)
 * - added arp_flush()
 * 
 * 30 Apr 2010	Dieter Siegmund (dieter@apple.com)
 * - eliminated unnecessary functions/API
 * - making remaining functions less susceptible to failures at the routing
 *   socket layer
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "arp.h"

#ifdef MAIN
#include <err.h>
static int	delete __P((int, int, char * *));
static int	dump __P((u_long));
static void	ether_print __P((u_char *));
static int	get __P((char *));
static void	usage __P((void));
static void	dump_entry(struct rt_msghdr * rtm);

static int nflag;
#else /* MAIN */
#define err(a, b)	return (ARP_RETURN_FAILURE)
#endif /* MAIN */

static const struct sockaddr_inarp blank_sin = {sizeof(blank_sin), AF_INET };

#ifdef MAIN

static const char * arperrors[] = {
    "success",
    "failure",
    "internal error",
    "write to routing socket failed",
    "read to routing socket failed",
    "can't locate",
    0
};

const char *
arp_strerror(int err)
{
    if (err < ARP_RETURN_LAST && err >= ARP_RETURN_SUCCESS)
	return (arperrors[err]);
    return ("unknown error");
}

typedef enum {
    command_none_e = 0,
    command_dump_e,
    command_delete_e,
    command_flush_e,
} command_t;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	int s;
	int aflag = 0;
	command_t cmd = command_none_e;
	int ret = 0;

	while ((ch = getopt(argc, argv, "andF")) != EOF) {
		switch((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			if (nflag || aflag 
			    || cmd != command_none_e) {
				usage();
			}
			cmd = command_delete_e;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'F':
			if (cmd != command_none_e)
				usage();
			cmd = command_flush_e;
			break;
		case '?':
		default:
			usage();
		}
	}
	if (cmd == command_none_e) {
		cmd = command_dump_e;
	}
	switch (cmd) {
	case command_dump_e:
		if (aflag)
			dump(0);
		else {
			if ((argc - optind) != 1)
				usage();
			ret = get(argv[optind]) ? 1 : 0;
		}
		break;
	case command_delete_e:
		if ((argc - optind) < 1 || (argc - optind) > 2)
			usage();
		s = arp_open_routing_socket();
		ret = delete(s, argc - optind, &argv[optind]) ? 1 : 0;
		break;
	case command_flush_e:
		s = arp_open_routing_socket();
		arp_flush(s, aflag, 0);
		break;
	default:
		break;
	}
	exit(ret);
	return (0);
}

/*
 * Display an individual arp entry
 */
int
get(host)
	char *host;
{
	struct hostent *hp;
	struct	sockaddr_inarp sin_m;
	struct sockaddr_inarp *sin = &sin_m;
	route_msg msg;

	sin_m = blank_sin;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	if (arp_get(arp_open_routing_socket(), &msg, sin->sin_addr, 0) 
	    != ARP_RETURN_SUCCESS) {
		printf("%s (%s) -- no entry\n",
		    host, inet_ntoa(sin->sin_addr));
		return (1);
	}
	else {
	    dump_entry(&msg.m_rtm);
	}
	return (0);
}

/*
 * Delete an arp entry 
 */
int
delete(int s, int argc, char * * argv)
{
	char * host;
	struct hostent *hp;
	struct in_addr iaddr;

	host = argv[0];
	iaddr.s_addr = inet_addr(host);
	if (iaddr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&iaddr,
		      sizeof (iaddr));
	}
	{
	    int ret;

	    errno = 0;
	    ret = arp_delete(s, iaddr, 0);
	    if (ret == ARP_RETURN_SUCCESS) {
		printf("%s (%s) deleted\n", host, inet_ntoa(iaddr));
		return (0);
	    }
	    printf("delete: %s %s", arp_strerror(ret), host);
	    if (errno)
		printf(": %s\n", strerror(errno));
	    else
		printf("\n");
	}
	return (1);
}

/*
 * Dump the entire arp table
 */

static void
dump_entry(struct rt_msghdr * rtm)
{
    char *			host;
    struct hostent *		hp;
    struct sockaddr_inarp *	sin;
    struct sockaddr_dl *	sdl;

    sin = (struct sockaddr_inarp *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(sin + 1);
    if (nflag == 0) {
	hp = gethostbyaddr((caddr_t)&(sin->sin_addr),
			   sizeof(sin->sin_addr), AF_INET);
    }
    else {
	hp = NULL;
    }

    if (hp != NULL) {
	host = hp->h_name;
    }
    else {
	host = "?";
    }
    if (h_errno == TRY_AGAIN) {
	nflag = 1;
    }
    printf("%s (%s) at ", host, inet_ntoa(sin->sin_addr));
    if (sdl->sdl_alen != 0) {
	ether_print((u_char *)LLADDR(sdl));
    }
    else {
	printf("(incomplete)");
    }
    if (rtm->rtm_rmx.rmx_expire == 0) {
	printf(" permanent");
    }
    if (sin->sin_other & SIN_PROXY) {
	printf(" published (proxy only)");
    }
    if (rtm->rtm_addrs & RTA_NETMASK) {
	sin = (struct sockaddr_inarp *)
	    (sdl->sdl_len + (char *)sdl);
	if (sin->sin_addr.s_addr == 0xffffffff)
	    printf(" published");
	if (sin->sin_len != 8)
	    printf("(weird)");
    }
    printf("\n");
    return;
}

int
dump(addr)
	u_long addr;
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;
	extern int h_errno;
	int found_entry = 0;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	    	free(buf);
		err(1, "actual retrieval of routing table");
	}
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		/* ALIGN: trust that the kernel has taken care of alignment */
		rtm = (struct rt_msghdr *)(void *)next;
		sin = (struct sockaddr_inarp *)(void *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(void *)(sin + 1);

		if (addr) {
			if (addr != sin->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		dump_entry(rtm);
	}
	return (found_entry);
}

void
ether_print(cp)
	u_char *cp;
{
	printf("%02x:%02x:%02x:%02x:%02x:%02x",
	       cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

void
usage()
{
	printf("usage: arp hostname\n");
	printf("       arp -a [-n]\n");
	printf("       arp -d hostname\n");
	printf("       arp -f filename\n");
	printf("       arp -F [-a]\n");
	exit(1);
}

#endif /* MAIN */

int
arp_open_routing_socket(void)
{
    int		opt;
    int 	s;

    s = socket(AF_ROUTE, SOCK_RAW, AF_ROUTE);
    if (s < 0) {
#ifdef MAIN
	perror("arp: socket");
	exit(1);
#else /* MAIN */
	return (-1);
#endif /* MAIN */
    }
    opt = 1;
    if (ioctl(s, FIONBIO, &opt) < 0) {
#ifdef MAIN
	perror("arp: FIONBIO");
	exit(1);
#else /* MAIN */
	close(s);
	return (-1);
#endif /* MAIN */
    }
    return (s);
}

int
arp_get_next_seq(void)
{
    static int 	rtm_seq;

    return (++rtm_seq);
}

static int
route_get(int s, route_msg * msg_p, struct in_addr iaddr, int if_index)
{
    ssize_t			n;
    int 			pid = getpid();
    struct rt_msghdr *		rtm = &(msg_p->m_rtm);
    struct sockaddr_inarp *	sin;
    int 			rtm_seq;

    bzero((char *)rtm, sizeof(*rtm));
    rtm->rtm_flags = RTF_LLINFO;
    if (if_index != 0) {
	rtm->rtm_index = if_index;
	rtm->rtm_flags |= RTF_IFSCOPE;
    }
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_addrs = RTA_DST;
    sin = (struct sockaddr_inarp *)(rtm + 1);
    *sin = blank_sin;
    sin->sin_addr = iaddr;
    n = rtm->rtm_msglen = sizeof(*rtm) + sizeof(*sin);
    rtm->rtm_seq = rtm_seq = arp_get_next_seq();
    rtm->rtm_type = RTM_GET_SILENT;
    if (write(s, (char *)msg_p, n) != n) {
	return (ARP_RETURN_WRITE_FAILED);
    }
    errno = 0;
    while (1) {
	n = read(s, (char *)msg_p, sizeof(*msg_p));
	if (n <= 0) {
	    break;
	}
	if (rtm->rtm_type == RTM_GET 
	    && rtm->rtm_seq == rtm_seq && rtm->rtm_pid == pid) {
	    return (ARP_RETURN_SUCCESS);
	}
    }
    return (ARP_RETURN_READ_FAILED);
}

static __inline__ int
is_arp_sdl_type(int sdl_type)
{
    switch (sdl_type) {
    case IFT_ETHER:
    case IFT_FDDI:
    case IFT_ISO88023:
    case IFT_ISO88024:
    case IFT_ISO88025:
    case IFT_IEEE1394:
	return (1);
    }
    return (0);
}

int
arp_get(int s, route_msg * msg_p, struct in_addr iaddr, int if_index)
{
    int				ret;
    struct rt_msghdr *		rtm = &(msg_p->m_rtm);
    struct sockaddr_inarp *	sin;
    struct sockaddr_dl *	sdl;

    ret = route_get(s, msg_p, iaddr, if_index);
    if (ret) {
	goto done;
    }
#define WHICH_RTA	(RTA_DST | RTA_GATEWAY)
    ret = ARP_RETURN_HOST_NOT_FOUND;
    if ((rtm->rtm_addrs & (WHICH_RTA)) != WHICH_RTA
	|| (rtm->rtm_flags & RTF_LLINFO) == 0
	|| (rtm->rtm_flags & RTF_GATEWAY) != 0) {
	goto done;
    }
    sin = (struct sockaddr_inarp *)msg_p->m_space;
    if (sin->sin_addr.s_addr != iaddr.s_addr) {
	goto done;
    }

    /* ALIGN: msg_p->m_space is aligned sufficiently to dereference 
     * sdl safely */
    sdl = (struct sockaddr_dl *)(void *)(sin->sin_len + (char *)sin);
    if (sdl->sdl_family == AF_LINK && is_arp_sdl_type(sdl->sdl_type)) {
	ret = ARP_RETURN_SUCCESS;
    }
 done:
    return (ret);
}

/*
 * Function: arp_delete
 *
 * Purpose:
 *   Delete the arp entry for the given host.
 * Assumes:
 *   s is an open routing socket
 */
int 
arp_delete(int s, struct in_addr iaddr, int if_index)
{
    route_msg			msg;
    int 			ret;
    struct rt_msghdr *		rtm = &msg.m_rtm;

    ret = arp_get(s, &msg, iaddr, if_index);
    if (ret) {
	goto done;
    }
    /* turn the RTM_GET into an RTM_DELETE */
    rtm->rtm_seq = arp_get_next_seq();
    rtm->rtm_type = RTM_DELETE;
    if (write(s, (char *)rtm, rtm->rtm_msglen) < 0) {
	ret = ARP_RETURN_FAILURE;
    }
    else {
	ret = ARP_RETURN_SUCCESS;
    }
 done:
    return (ret);
}

int
arp_flush(int s, int all, int if_index)
{
    char * 			buf;
    char *			lim;
    int 			mib[6];
    size_t 			needed;
    char *			next;
    struct rt_msghdr *		rtm;
    struct sockaddr_dl *	sdl;
    struct sockaddr_inarp *	sin;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_LLINFO;
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
	err(1, "route-sysctl-estimate");
    }
    if ((buf = malloc(needed)) == NULL) {
	err(1, "malloc");
    }
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	free(buf);
	err(1, "actual retrieval of routing table");
    }
    lim = buf + needed;
    for (next = buf; next < lim; next += rtm->rtm_msglen) {
	/* ALIGN: trust that the kernel has taken care of alignment */
	rtm = (struct rt_msghdr *)(void *)next;
	sin = (struct sockaddr_inarp *)(void *)(rtm + 1);
	
	if (all == 0 && rtm->rtm_rmx.rmx_expire == 0) {
	    /* skip permanent entries */
	    continue;
	}
	if (ip_is_linklocal(sin->sin_addr)
	    && (if_index != 0 && if_index != rtm->rtm_index)) {
	    /* IPv4 LL ARP entry doesn't match specified interface */
	    continue;
	}

	 /* ALIGN: sin aligned sufficiently to dereference sdl safely */
	sdl = (struct sockaddr_dl *)(void *)(sin->sin_len + (char *)sin);
	if (sdl->sdl_family != AF_LINK) {
	    continue;
	}
	/* turn the RTM_GET into an RTM_DELETE */
	rtm->rtm_seq = arp_get_next_seq();
	rtm->rtm_type = RTM_DELETE;
	if (write(s, (char *)rtm, rtm->rtm_msglen) < 0) {
#ifdef MAIN
	    fprintf(stderr, "flush: delete %s failed, %s\n",
		    inet_ntoa(sin->sin_addr), strerror(errno));
#endif /* MAIN */
	}
    }
    free(buf);
    return (0);
}
