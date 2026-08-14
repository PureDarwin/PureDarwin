/*
 * Copyright (c) 2013-2018 Apple Inc. All rights reserved.
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
 * ICMPv6Socket.c
 * - common functions for creating/sending packets over ICMPv6 sockets
 */

/* 
 * Modification History
 *
 * May 24, 2013		Dieter Siegmund (dieter@apple.com)
 * - created (based on RTADVSocket.c)
 */

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <sys/uio.h>
#include "IPConfigurationLog.h"
#include "ICMPv6Socket.h"
#include "IPv6Socket.h"
#include "IPv6Sock_Compat.h"
#include "symbol_scope.h"

#define ND_OPT_ALIGN		8
#define BUF_SIZE_NO_HLIM 	CMSG_SPACE(sizeof(struct in6_pktinfo))
#define BUF_SIZE		(BUF_SIZE_NO_HLIM + CMSG_SPACE(sizeof(int)))


PRIVATE_EXTERN int
ICMPv6SocketOpen(bool receive_too)
{
    int		opt = 1;
    int		sockfd;

    sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sockfd < 0) {
	IPConfigLogFL(LOG_ERR, "error opening socket: %s",
		      strerror(errno));
	goto failed;
    }

    /* set to non-blocking */
    if (ioctl(sockfd, FIONBIO, &opt) < 0) {
	IPConfigLogFL(LOG_ERR, "ioctl FIONBIO failed %s",
		      strerror(errno));
	goto failed;
    }

#if defined(SO_RECV_ANYIF)
    if (setsockopt(sockfd, SOL_SOCKET, SO_RECV_ANYIF, (caddr_t)&opt,
		   sizeof(opt)) < 0) {
	IPConfigLogFL(LOG_ERR, "setsockopt(SO_RECV_ANYIF) failed, %s",
		      strerror(errno));
    }
#endif /* SO_RECV_ANYIF */

    if (receive_too) {
	/* ask for packet info */
	if (setsockopt(sockfd, IPPROTO_IPV6, 
		       IPCONFIG_SOCKOPT_PKTINFO, &opt, sizeof(opt)) < 0) {
	    IPConfigLogFL(LOG_ERR, "IPV6_PKTINFO: %s",
			  strerror(errno));
	    goto failed;
	}
	/* ... and hop limit */
	if (setsockopt(sockfd, IPPROTO_IPV6, 
		       IPCONFIG_SOCKOPT_HOPLIMIT, &opt, sizeof(opt)) < 0) {
	    IPConfigLogFL(LOG_ERR, "IPV6_HOPLIMIT: %s",
			  strerror(errno));
	    goto failed;
	}
    }
#if defined(SO_TRAFFIC_CLASS)
    opt = SO_TC_CTL;
    /* set traffic class */
    if (setsockopt(sockfd, SOL_SOCKET, SO_TRAFFIC_CLASS, &opt,
		   sizeof(opt)) < 0) {
	IPConfigLogFL(LOG_ERR, "setsockopt(SO_TRAFFIC_CLASS) failed, %s",
		      strerror(errno));
    }
#endif /* SO_TRAFFIC_CLASS */

#if defined(SO_DEFUNCTOK)
    opt = 0;
    /* ensure that our socket can't be defunct'd */
    if (setsockopt(sockfd, SOL_SOCKET, SO_DEFUNCTOK, &opt,
		   sizeof(opt)) < 0) {
	IPConfigLogFL(LOG_ERR, "setsockopt(SO_DEFUNCTOK) failed, %s",
		      strerror(errno));
    }
#endif /* SO_DEFUNCTOK */

    return (sockfd);

 failed:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (-1);

}

STATIC int
NeighborAdvertTXBufInit(uint32_t * txbuf,
			const void * link_addr,
			int link_addr_length,
			const struct in6_addr * target_ipaddr)
{
    struct nd_neighbor_advert *	ndna;
    int				txbuf_used;

    ndna = (struct nd_neighbor_advert *)txbuf;
    ndna->nd_na_type = ND_NEIGHBOR_ADVERT;
    ndna->nd_na_code = 0;
    ndna->nd_na_cksum = 0;
    ndna->nd_na_flags_reserved = ND_NA_FLAG_OVERRIDE;
    bcopy(target_ipaddr, &ndna->nd_na_target, sizeof(ndna->nd_na_target));
    txbuf_used = sizeof(struct nd_neighbor_advert);
    if (link_addr != NULL) {
	int			opt_len;
	struct nd_opt_hdr *	ndopt;

	opt_len = roundup(link_addr_length + sizeof(*ndopt), ND_OPT_ALIGN);
	ndopt = (struct nd_opt_hdr *)(ndna + 1);
	ndopt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
	ndopt->nd_opt_len = (opt_len / ND_OPT_ALIGN);
	bcopy(link_addr, (ndopt + 1), link_addr_length);
	txbuf_used += opt_len;
    }
    return (txbuf_used);
}

STATIC const struct sockaddr_in6 sin6_allnodes = {
    sizeof(sin6_allnodes),
    AF_INET6, 0, 0,
    IN6ADDR_LINKLOCAL_ALLNODES_INIT, 0
};

/*
 * Define: NDADVERT_PACKET_MAX
 * Purpose:
 *   The maximum size of the payload for the ND Neighbor Advertisement
 *   ICMPv6 packet. That includes the nd_neighbor_advert structure and
 *   optionally, the link address option, which must be aligned to an
 *   ND_OPT_ALIGN boundary.
 */
#include "interfaces.h"
#define NDADVERT_PACKET_MAX (sizeof(struct nd_neighbor_advert)		\
			     + roundup(sizeof(struct nd_opt_hdr)	\
				       + MAX_LINK_ADDR_LEN, ND_OPT_ALIGN))

PRIVATE_EXTERN int
ICMPv6SocketSendNeighborAdvertisement(int sockfd,
				      int if_index,
				      const void * link_addr,
				      int link_addr_length,
				      const struct in6_addr * target_ipaddr)
{
    uint32_t		txbuf[NDADVERT_PACKET_MAX / sizeof(uint32_t)];
    int			txbuf_used;	/* amount actually used */
    
    txbuf_used = NeighborAdvertTXBufInit(txbuf, link_addr, link_addr_length,
					 target_ipaddr);
    return (IPv6SocketSend(sockfd, if_index, &sin6_allnodes,
			   (void *)txbuf, txbuf_used, IPV6_MAXHLIM));
}


#ifdef TEST_NEIGHBOR_ADVERT
#include "interfaces.h"
#include "ifutil.h"
boolean_t G_is_netboot;

int
main(int argc, char * argv[])
{
    inet6_addrlist_t	addr_list;
    int			count;
    int			i;
    interface_t *	if_p;
    const char *	ifname;
    interface_list_t *	interfaces = NULL;
    const void *	link_addr = NULL;
    int			link_addr_length = 0;
    inet6_addrinfo_t *	scan;
    int			sockfd;

    (void) openlog("ndadvert", LOG_PERROR | LOG_PID, LOG_DAEMON);
    interfaces = ifl_init();
    if (argc != 2) {
	fprintf(stderr, "ndadvert <ifname>\n");
	exit(1);
    }
    ifname = argv[1];
    if_p = ifl_find_name(interfaces, ifname);
    if (if_p == NULL) {
	fprintf(stderr, "no such interface '%s'\n", ifname);
	exit(2);
    }
    sockfd = ICMPv6SocketOpen(FALSE);
    if (sockfd < 0) {
	perror("socket");
	exit(2);
    }
    inet6_addrlist_init(&addr_list);
    inet6_addrlist_copy(&addr_list, if_link_index(if_p));
    if (if_link_type(if_p) == IFT_ETHER) {
	link_addr = if_link_address(if_p);
	link_addr_length = if_link_length(if_p);
    }
    for (i = 0, count = 0, scan = addr_list.list; 
	 i < addr_list.count; i++, scan++) {
	int			error;

	if ((scan->addr_flags & IN6_IFF_NOTREADY) != 0) {
	    continue;
	}
	error = ICMPv6SocketSendNeighborAdvertisement(sockfd,
						      if_link_index(if_p),
						      link_addr,
						      link_addr_length,
						      &scan->addr);
	if (error != 0) {
	    fprintf(stderr, "failed to send neighbor advert, %s\n",
		    strerror(error));
	}
    }
    inet6_addrlist_free(&addr_list);
    close(sockfd);
    exit(0);
    return (0);
}

#endif /*  TEST_NEIGHBOR_ADVERT */
