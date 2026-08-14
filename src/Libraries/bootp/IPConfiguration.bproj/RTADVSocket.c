/*
 * Copyright (c) 2010-2020 Apple Inc. All rights reserved.
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
 * RTADVSocket.c
 * - maintain list of Router Advertisement client "sockets"
 * - distribute packet reception to enabled "sockets"
 */
/* 
 * Modification History
 *
 * June 4, 2010		Dieter Siegmund (dieter@apple.com)
 * - created (based on DHCPv6Socket.c and rtadv.c)
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
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <syslog.h>
#include <sys/uio.h>
#include "ipconfigd_globals.h"
#include "RTADVSocket.h"
#include "util.h"
#include "interfaces.h"
#include "FDSet.h"
#include "dynarray.h"
#include "symbol_scope.h"
#include "timer.h"
#include "IPv6Sock_Compat.h"
#include "IPv6Socket.h"
#include "ICMPv6Socket.h"
#include "ifutil.h"

STATIC const struct sockaddr_in6 sin6_allrouters = {
    sizeof(sin6_allrouters),
    AF_INET6, 0, 0,
    IN6ADDR_LINKLOCAL_ALLROUTERS_INIT, 0
};

#define RECEIVE_CMSG_BUF_SIZE 	(CMSG_SPACE(sizeof(struct in6_pktinfo))	\
				 + CMSG_SPACE(sizeof(int)))
#define SEND_CMSG_BUF_SIZE	RECEIVE_CMSG_BUF_SIZE

#define ND_RTADV_HOP_LIMIT		IPV6_MAXHLIM

/*
 * Define: RTSOL_PACKET_MAX
 * Purpose:
 *   The maximum size of the payload for the ND Router Solicit ICMPv6 packet.
 *   That includes the nd_router_solicit structure and optionally, the 
 *   link address option, which must be aligned to an ND_OPT_ALIGN boundary.
 */
#define RTSOL_PACKET_MAX (sizeof(struct nd_router_solicit) \
			  + roundup(sizeof(struct nd_opt_hdr) \
				    + MAX_LINK_ADDR_LEN, ND_OPT_ALIGN))

typedef struct RTADVSocketGlobals {
    dynarray_t			sockets;
    FDCalloutRef		read_fd;
    int				read_fd_refcount;
    timer_callout_t *		timer_callout;
} RTADVSocketGlobals, * RTADVSocketGlobalsRef;

struct RTADVSocket {
    interface_t *		if_p;
    struct in6_addr		linklocal_addr;
    boolean_t			fd_open;
    RTADVSocketReceiveFuncPtr	receive_func;
    void *			receive_arg1;
    void *			receive_arg2;
};

STATIC RTADVSocketGlobalsRef	S_globals;

STATIC int
open_rtadv_socket(void)
{
    struct icmp6_filter	filt;
    int			sockfd;

    /* open socket */
    sockfd = ICMPv6SocketOpen(TRUE);
    if (sockfd < 0) {
	my_log_fl(LOG_ERR, "error opening socket: %s",
		  strerror(errno));
	goto failed;
    }

    /* accept only router advertisement messages */
    ICMP6_FILTER_SETBLOCKALL(&filt);
    ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
    if (setsockopt(sockfd, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		   sizeof(filt)) == -1) {
	my_log_fl(LOG_ERR, "setsockopt(ICMP6_FILTER): %s",
		  strerror(errno));
	goto failed;
    }
    return (sockfd);

 failed:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (-1);
}

STATIC void
RTADVSocketFreeElement(void * arg);

STATIC RTADVSocketGlobalsRef
RTADVSocketCreateGlobals(void)
{
    RTADVSocketGlobalsRef	globals = malloc(sizeof(*globals));

    if (globals == NULL) {
	return (NULL);
    }
    bzero(globals, sizeof(*globals));
    dynarray_init(&globals->sockets, RTADVSocketFreeElement, NULL);
    globals->timer_callout = timer_callout_init("RTADVSocket");
    return (globals);
}

STATIC void
RTADVSocketReleaseGlobals(RTADVSocketGlobalsRef * globals_p)
{
    RTADVSocketGlobalsRef	globals;

    if (globals_p == NULL) {
	return;
    }
    globals = *globals_p;
    if (globals == NULL) {
	return;
    }
    *globals_p = NULL;
    dynarray_free(&globals->sockets);
    FDCalloutRelease(&globals->read_fd);
    timer_callout_free(&globals->timer_callout);
    bzero(globals, sizeof(*globals));
    free(globals);
    return;
}

STATIC RTADVSocketGlobalsRef
RTADVSocketGetGlobals(void)
{
    if (S_globals != NULL) {
	return (S_globals);
    }
    S_globals = RTADVSocketCreateGlobals();
    return (S_globals);
}

STATIC RTADVSocketRef
RTADVSocketFind(int if_index)
{
    int 			i;

    for (i = 0; i < dynarray_count(&S_globals->sockets); i++) {
	interface_t *		if_p;
	RTADVSocketRef		sock;

	sock = dynarray_element(&S_globals->sockets, i);
	if_p = RTADVSocketGetInterface(sock);
	if (if_index == if_link_index(if_p)) {
	    return (sock);
	}
    }
    return (NULL);
}

STATIC void
RTADVSocketDelayedClose(void * arg1, void * arg2, void * arg3)
{
    if (S_globals->read_fd == NULL) {
	my_log(LOG_NOTICE,
	       "RTADVSocketDelayedClose(): socket is already closed");
	return;
    }
    if (S_globals->read_fd_refcount > 0) {
	my_log(LOG_NOTICE,
	       "RTADVSocketDelayedClose(): called when socket in use");
	return;
    }
    my_log(LOG_DEBUG,
	   "RTADVSocketDelayedClose(): closing RTADV socket %d",
	   FDCalloutGetFD(S_globals->read_fd));

    /* this closes the file descriptor */
    FDCalloutRelease(&S_globals->read_fd);
    return;
}

STATIC void
RTADVSocketDemux(int if_index, const struct in6_addr * from,
		 const struct nd_router_advert * ndra_p, ssize_t ndra_length)
{
    RTADVSocketRef		sock;

    sock = RTADVSocketFind(if_index);
    if (sock == NULL) {
	return;
    }
    if (sock->receive_func != NULL) {
	RouterAdvertisementRef		ra;

	ra = RouterAdvertisementCreate(ndra_p, (size_t)ndra_length,
				       from, CFAbsoluteTimeGetCurrent());
	if (ra == NULL) {
	    my_log(LOG_NOTICE,
		   "%s: failed to create RouterAdvertisement",
		   if_name(sock->if_p));
	    return;
	}
	(*sock->receive_func)(sock->receive_arg1, sock->receive_arg2, ra);
    }
    return;
}

STATIC void
RTADVSocketRead(void * arg1, void * arg2)
{
    struct cmsghdr *			cm;
    char				cmsgbuf[RECEIVE_CMSG_BUF_SIZE];
    int *				hop_limit_p;
    struct icmp6_hdr *			icmp_hdr;
    struct iovec 			iov;
    struct sockaddr_in6 		from;
    struct msghdr 			mhdr;
    ssize_t				n;
    struct nd_router_advert * 		ndra_p;
    char 				ntopbuf[INET6_ADDRSTRLEN];
    struct in6_pktinfo *		pktinfo = NULL;
    uint32_t				receive_buf[1500 / sizeof(uint32_t)];

    /* initialize msghdr for receiving packets */
    iov.iov_base = (caddr_t)receive_buf;
    iov.iov_len = sizeof(receive_buf);
    mhdr.msg_name = (caddr_t)&from;
    mhdr.msg_namelen = sizeof(from);
    mhdr.msg_iov = &iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = (caddr_t)cmsgbuf;
    mhdr.msg_controllen = sizeof(cmsgbuf);
    mhdr.msg_flags = 0;

    /* get message */
    n = recvmsg(FDCalloutGetFD(S_globals->read_fd), &mhdr, 0);
    if (n < 0) {
	if (errno != EAGAIN) {
	    my_log(LOG_ERR, "RTADVSocketRead: recvfrom failed %s (%d)",
		   strerror(errno), errno);
	}
	return;
    }
    if (n == 0) {
	/* nothing to read */
	return;
    }
    if (n < sizeof(struct nd_router_advert)) {
	my_log(LOG_NOTICE, "RTADVSocketRead: packet size(%ld) is too short", n);
	return;
    }
    if (!IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
	my_log(LOG_NOTICE,
	       "RTADVSocketRead: RA has non link-local source address %s",
	       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)));
	return;
    }

    /* process the packet */
    pktinfo = NULL;
    hop_limit_p = NULL;
    for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&mhdr);
	 cm != NULL;
	 cm = (struct cmsghdr *)CMSG_NXTHDR(&mhdr, cm)) {
	if (cm->cmsg_level != IPPROTO_IPV6) {
	    continue;
	}
	switch (cm->cmsg_type) {
	case IPV6_PKTINFO:
	    if (cm->cmsg_len < CMSG_LEN(sizeof(struct in6_pktinfo))) {
		continue;
	    }
	    /* ALIGN: CMSG_DATA returns aligned data, cast ok. */
	    pktinfo = (struct in6_pktinfo *)(void *)(CMSG_DATA(cm));
	    break;
	case IPV6_HOPLIMIT:
	    if (cm->cmsg_len < CMSG_LEN(sizeof(int))) {
		continue;
	    }
	    /* ALIGN: CMSG_DATA returns aligned data, cast ok. */
	    hop_limit_p = (int *)(void *)CMSG_DATA(cm);
	    break;
	default:
	    /* this should never occur */
	    my_log(LOG_NOTICE, "RTADVSocketRead: why control message type %d?",
		   cm->cmsg_type);
	    break;
	}
    }
    if (pktinfo == NULL) {
	my_log(LOG_NOTICE,
	       "RTADVSocketRead: missing IPV6_PKTINFO");
	return;
    }
    if (hop_limit_p == NULL) {
	my_log(LOG_NOTICE, "RTADVSocketRead: missing IPV6_HOPLIMIT");
	return;
    }
    ndra_p = (struct nd_router_advert *)receive_buf;
    icmp_hdr = &ndra_p->nd_ra_hdr;
    if (icmp_hdr->icmp6_type != ND_ROUTER_ADVERT) {
	/* this should not happen, the kernel filters for us */
	my_log(LOG_NOTICE, "RTADVSocket: received unexpected ND packet type %d",
	       icmp_hdr->icmp6_type);
	return;
    }
    if (icmp_hdr->icmp6_code != 0) {
	my_log(LOG_NOTICE, "RTADVSocket: invalid icmp code %d",
	       icmp_hdr->icmp6_code);
	return;
    }
    if (*hop_limit_p != ND_RTADV_HOP_LIMIT) {
	my_log(LOG_NOTICE, "RTADVSocket: invalid RA hop limit %d from %s",
	       *hop_limit_p, 
	       inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)));
	return;
    }
    RTADVSocketDemux(pktinfo->ipi6_ifindex, &from.sin6_addr, ndra_p, n);
    return;
}

PRIVATE_EXTERN interface_t *
RTADVSocketGetInterface(RTADVSocketRef sock)
{
    return (sock->if_p);
}

STATIC int
RTADVSocketInitTXBuf(RTADVSocketRef sock, uint32_t * txbuf, bool lladdr_ok)
{
    interface_t *		if_p = RTADVSocketGetInterface(sock);
    struct nd_router_solicit *	ndrs;
    int				txbuf_used;

    ndrs = (struct nd_router_solicit *)txbuf;
    ndrs->nd_rs_type = ND_ROUTER_SOLICIT;
    ndrs->nd_rs_code = 0;
    ndrs->nd_rs_cksum = 0;
    ndrs->nd_rs_reserved = 0;
    txbuf_used = sizeof(struct nd_router_solicit);

    /* if it's OK to include [RFC 4429], add source link-layer address option */
    if (lladdr_ok && if_link_type(if_p) == IFT_ETHER) {
	int			opt_len;
	struct nd_opt_hdr *	ndopt;

	opt_len = roundup(if_link_length(if_p) + sizeof(*ndopt), ND_OPT_ALIGN);
	ndopt = (struct nd_opt_hdr *)(ndrs + 1);
	ndopt->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	ndopt->nd_opt_len = (opt_len / ND_OPT_ALIGN);
	bcopy(if_link_address(if_p), (ndopt + 1), if_link_length(if_p));
	txbuf_used += opt_len;
    }
    return (txbuf_used);
}

PRIVATE_EXTERN RTADVSocketRef
RTADVSocketCreate(interface_t * if_p)
{
    RTADVSocketRef		sock;
    RTADVSocketGlobalsRef	globals;

    globals = RTADVSocketGetGlobals();
    if (globals == NULL) {
	my_log(LOG_NOTICE, "RTADVSocketCreate: could not allocate globals");
	return (NULL);
    }
    sock = RTADVSocketFind(if_link_index(if_p));
    if (sock != NULL) {
	my_log(LOG_NOTICE, "RTADVSocketCreate(%s): socket already allocated",
	       if_name(if_p));
	return (NULL);
    }
    sock = malloc(sizeof(*sock));
    if (sock == NULL) {
	return (NULL);
    }
    bzero(sock, sizeof(*sock));
    if (dynarray_add(&globals->sockets, sock) == FALSE) {
	free(sock);
	return (NULL);
    }
    sock->if_p = if_p;
    return (sock);
}

STATIC void
RTADVSocketFreeElement(void * arg)
{
    RTADVSocketRef 	sock = (RTADVSocketRef)arg;

    RTADVSocketDisableReceive(sock);
    free(sock);
    return;
}

PRIVATE_EXTERN void
RTADVSocketRelease(RTADVSocketRef * sock_p)
{
    RTADVSocketRef 	sock = *sock_p;
    int 		i;

    if (sock == NULL) {
	return;
    }
    i = dynarray_index(&S_globals->sockets, sock);
    if (i != -1) {
	dynarray_remove(&S_globals->sockets, i, NULL);
    }
    else {
	my_log(LOG_NOTICE, "RTADVSocketRelease: %s not in list?",
	       if_name(sock->if_p));
    }
    RTADVSocketFreeElement(sock);
    *sock_p = NULL;
    if (dynarray_count(&S_globals->sockets) == 0) {
	RTADVSocketReleaseGlobals(&S_globals);
    }
    return;
}

STATIC void
RTADVSocketCloseSocket(RTADVSocketRef sock)
{
    if (sock->fd_open == FALSE) {
	return;
    }
    if (S_globals->read_fd_refcount <= 0) {
	my_log(LOG_NOTICE, "RTADVSocketCloseSocket(%s): refcount %d",
	       if_name(sock->if_p), S_globals->read_fd_refcount);
	return;
    }
    S_globals->read_fd_refcount--;
    my_log(LOG_DEBUG, "RTADVSocketCloseSocket(%s): refcount %d",
	   if_name(sock->if_p), S_globals->read_fd_refcount);
    sock->fd_open = FALSE;
    if (S_globals->read_fd_refcount == 0) {
	struct timeval tv;

	my_log(LOG_DEBUG,
	       "RTADVSocketCloseSocket(): scheduling delayed close");

	tv.tv_sec = 1; /* close it after 1 second of non-use */
	tv.tv_usec = 0;
	timer_set_relative(S_globals->timer_callout, tv,
			   RTADVSocketDelayedClose,
			   NULL, NULL, NULL);
    }
    return;
}

STATIC bool
RTADVSocketOpenSocket(RTADVSocketRef sock)
{
    if (sock->fd_open) {
	return (TRUE);
    }
    timer_cancel(S_globals->timer_callout);
    S_globals->read_fd_refcount++;
    my_log(LOG_DEBUG, "RTADVSocketOpenSocket (%s): refcount %d",
	   if_name(sock->if_p), S_globals->read_fd_refcount);
    sock->fd_open = TRUE;
    if (S_globals->read_fd_refcount > 1) {
	/* already open */
	return (TRUE);
    }
    if (S_globals->read_fd != NULL) {
	my_log(LOG_INFO, "RTADVSocketOpenSocket(): socket is still open");
    }
    else {
	int	sockfd;

	sockfd = open_rtadv_socket();
	if (sockfd < 0) {
	    my_log(LOG_ERR, 
		   "RTADVSocketOpenSocket: socket() failed, %s",
		   strerror(errno));
	    goto failed;
	}
	my_log(LOG_DEBUG,
	       "RTADVSocketOpenSocket(): opened RTADV socket %d",
	       sockfd);
	/* register as a reader */
	S_globals->read_fd = FDCalloutCreate(sockfd,
					     RTADVSocketRead,
					     NULL, NULL);
    }
    return (TRUE);

 failed:
    RTADVSocketCloseSocket(sock);
    return (FALSE);
}

PRIVATE_EXTERN void
RTADVSocketEnableReceive(RTADVSocketRef sock,
			 RTADVSocketReceiveFuncPtr func, 
			 void * arg1, void * arg2)
{
    sock->receive_func = func;
    sock->receive_arg1 = arg1;
    sock->receive_arg2 = arg2;
    if (RTADVSocketOpenSocket(sock) == FALSE) {
	my_log_fl(LOG_NOTICE, "%s: failed", if_name(sock->if_p));
    }
    return;
}

PRIVATE_EXTERN void
RTADVSocketDisableReceive(RTADVSocketRef sock)
{
    sock->receive_func = NULL;
    sock->receive_arg1 = NULL;
    sock->receive_arg2 = NULL;
    bzero(&sock->linklocal_addr, sizeof(sock->linklocal_addr));
    RTADVSocketCloseSocket(sock);
    return;
}

PRIVATE_EXTERN int
RTADVSocketSendSolicitation(RTADVSocketRef sock, bool lladdr_ok)
{
    interface_t *	if_p = RTADVSocketGetInterface(sock);
    boolean_t		needs_close = FALSE;
    int			ret;
    uint32_t		txbuf[RTSOL_PACKET_MAX / sizeof(uint32_t)];
    int			txbuf_used;	/* amount actually used */


    if (sock->fd_open == FALSE) {
	/* open the RTADV socket in case it's needed */
	if (RTADVSocketOpenSocket(sock) == FALSE) {
	    my_log(LOG_NOTICE, "RTADVSocket: failed to open socket");
	    return (FALSE);
	}
	needs_close = TRUE;
    }
    txbuf_used = RTADVSocketInitTXBuf(sock, txbuf, lladdr_ok);
    ret = IPv6SocketSend(FDCalloutGetFD(S_globals->read_fd),
			 if_link_index(if_p),
			 &sin6_allrouters,
			 (void *)txbuf, txbuf_used,
			 ND_RTADV_HOP_LIMIT);
    if (needs_close) {
	RTADVSocketCloseSocket(sock);
    }
    return (ret);
}

#ifdef TEST_RTADVSOCKET

#include <CoreFoundation/CFRunLoop.h>
#include "ipconfigd_threads.h"
#include "DNSNameList.h"

Boolean G_IPConfiguration_verbose = TRUE;

typedef struct {
    RTADVSocketRef	sock;
    timer_callout_t *	timer;
    int			try;
} RTADVInfo, * RTADVInfoRef;

STATIC void
start_rtadv(RTADVInfoRef rtadv, IFEventID_t event_id, void * event_data)
{
    CFStringRef			captive_portal;
    CFStringRef			description;
    const uint8_t *		dns_search_list;
    int				dns_search_list_length;
    uint32_t 			dns_search_list_lifetime;
    const struct in6_addr *	dns_servers;
    int				dns_servers_count;
    uint32_t 			dns_servers_lifetime;
    int				error;
    interface_t *		if_p = RTADVSocketGetInterface(rtadv->sock);
    char			link_addr_buf[MAX_LINK_ADDR_LEN * 3 + 1];
    const uint8_t *		lladdr;
    int				lladdr_length;
    char 			ntopbuf[INET6_ADDRSTRLEN];
    CFAbsoluteTime		now;
    RouterAdvertisementRef	ra;
    struct timeval		tv;

    switch (event_id) {
    case IFEventID_start_e:
	rtadv->try = 1;
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)start_rtadv,
				 rtadv, (void *)IFEventID_data_e);
	/* fall through */
    case IFEventID_timeout_e:
	if (rtadv->try > MAX_RTR_SOLICITATIONS) {
	    break;
	}
	my_log(LOG_NOTICE,
	       "RTADV %s: sending Router Solicitation (%d of %d)",
	       if_name(if_p), rtadv->try, MAX_RTR_SOLICITATIONS);
	error = RTADVSocketSendSolicitation(rtadv->sock, TRUE);
	switch (error) {
	case 0:
	case ENXIO:
	case ENETDOWN:
	case EADDRNOTAVAIL:
	    break;
	default:
	    my_log(LOG_NOTICE,
		   "RTADV %s: send Router Solicitation: failed, %s",
		   if_name(if_p), strerror(error));
	    break;
	}
	rtadv->try++;
	tv.tv_sec = RTR_SOLICITATION_INTERVAL;
	tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	timer_set_relative(rtadv->timer, tv,
			   (timer_func_t *)start_rtadv,
			   rtadv, (void *)IFEventID_timeout_e, NULL);

	break;

    case IFEventID_data_e:
	now = CFAbsoluteTimeGetCurrent();
	ra = (RouterAdvertisementRef)event_data;
	description = RouterAdvertisementCopyDescription(ra);
	my_log(LOG_NOTICE,
	       "RTADV %s: Received RA %@", if_name(if_p), description);
	CFRelease(description);
	dns_servers
	    = RouterAdvertisementGetRDNSS(ra, &dns_servers_count,
					  &dns_servers_lifetime);
	if (dns_servers != NULL) {
	    int		i;

	    my_log(LOG_NOTICE, "DNS servers (lifetime %d):",
		   dns_servers_lifetime);
	    for (i = 0; i < dns_servers_count; i++) {
		my_log(LOG_NOTICE,
		       "%s",
		       inet_ntop(AF_INET6, dns_servers + i,
				 ntopbuf, sizeof(ntopbuf)));
	    }
	}
	dns_search_list
	    = RouterAdvertisementGetDNSSL(ra, &dns_search_list_length,
					  &dns_search_list_lifetime);
	if (dns_search_list != NULL) {
	    int 		count = 0;
	    int 		i;
	    const char * *	list;

	    my_log(LOG_NOTICE, "DNS search list (lifetime %d):",
		   dns_search_list_lifetime);
	    list = DNSNameListCreate(dns_search_list,
				     dns_search_list_length,
				     &count);
	    for (i = 0; i < count; i++) {
		my_log(LOG_NOTICE, "%s", list[i]);
	    }
	    if (list != NULL) {
		free(list);
	    }
	}
	captive_portal = RouterAdvertisementCopyCaptivePortal(ra);
	if (captive_portal != NULL) {
	    my_log(LOG_NOTICE,
		   "Captive Portal: %@", captive_portal);
	    CFRelease(captive_portal);
	}
	lladdr = RouterAdvertisementGetSourceLinkAddress(ra,
							 &lladdr_length);
	if (lladdr != NULL) {

	    link_addr_to_string(link_addr_buf, sizeof(link_addr_buf),
				lladdr, lladdr_length);
	    my_log(LOG_NOTICE, "Router Link Address %s",
		   link_addr_buf);
	}
	timer_cancel(rtadv->timer);
	break;
    default:
	break;
    }
    return;
}

int
main(int argc, char * argv[])
{
    os_log_t 		handle;
    interface_t *	if_p;
    const char *	ifname;
    interface_list_t *	interfaces = NULL;
    RTADVInfo		rtadv;

    handle = os_log_create(kIPConfigurationLogSubsystem,
			   kIPConfigurationLogCategoryServer);
    IPConfigLogSetHandle(handle);
    interfaces = ifl_init();
    if (argc != 2) {
	fprintf(stderr, "rtadv <ifname>\n");
	exit(1);
    }
    ifname = argv[1];
    if_p = ifl_find_name(interfaces, ifname);
    if (if_p == NULL) {
	fprintf(stderr, "no such interface '%s'\n", ifname);
	exit(2);
    }
    rtadv.sock = RTADVSocketCreate(if_p);
    rtadv.timer = timer_callout_init("test-RTADVSocket");
    start_rtadv(&rtadv, IFEventID_start_e, NULL);
    CFRunLoopRun();
    exit(0);
    return (0);
}

#endif /*  TEST_RTADVSOCKET */
