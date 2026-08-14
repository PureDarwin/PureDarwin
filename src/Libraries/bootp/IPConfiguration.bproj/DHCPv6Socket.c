/*
 * Copyright (c) 2009-2020 Apple Inc. All rights reserved.
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
 * DHCPv6Socket.c
 * - maintain list of DHCPv6 client "sockets"
 * - distribute packet reception to enabled "sockets"
 */
/* 
 * Modification History
 *
 * September 30, 2009		Dieter Siegmund (dieter@apple.com)
 * - created (based on bootp_session.c)
 */


#include <stdlib.h>
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
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include "ipconfigd_globals.h"
#include "DHCPv6.h"
#include "DHCPv6Socket.h"
#include "DHCPv6Options.h"
#include "IPv6Socket.h"
#include "util.h"
#include <syslog.h>
#include <sys/uio.h>
#include "interfaces.h"
#include "FDSet.h"
#include "dynarray.h"
#include "symbol_scope.h"
#include "timer.h"
#include "IPv6Sock_Compat.h"

typedef struct DHCPv6SocketGlobals {
    dynarray_t			sockets;
    FDCalloutRef		read_fd;
    int				read_fd_refcount;
    timer_callout_t *		timer_callout;
} DHCPv6SocketGlobals, * DHCPv6SocketGlobalsRef;

struct DHCPv6Socket {
    interface_t *		if_p;
    boolean_t			fd_open;
    DHCPv6SocketReceiveFuncPtr	receive_func;
    void *			receive_arg1;
    void *			receive_arg2;
};

STATIC bool			S_verbose;
STATIC uint16_t			S_client_port = DHCPV6_CLIENT_PORT;
STATIC uint16_t			S_server_port = DHCPV6_SERVER_PORT;
STATIC DHCPv6SocketGlobalsRef	S_globals;

STATIC const struct sockaddr_in6 dhcpv6_all_servers_and_relay_agents = {
    sizeof(dhcpv6_all_servers_and_relay_agents),
    AF_INET6, 0, 0,
    All_DHCP_Relay_Agents_and_Servers_INIT, 0
};

STATIC int
open_dhcpv6_socket(uint16_t client_port)
{
    struct sockaddr_in6		me;
    int				opt = 1;
    int 			sockfd;

    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	my_log(LOG_ERR, "DHCPv6Socket: socket failed, %s",
	       strerror(errno));
	return (sockfd);
    }
    bzero(&me, sizeof(me));
    me.sin6_family = AF_INET6;
    me.sin6_port = htons(client_port);

    if (bind(sockfd, (struct sockaddr *)&me, sizeof(me)) != 0) {
	my_log(LOG_ERR, "DHCPv6Socket: bind failed, %s",
	       strerror(errno));
	goto failed;
    }
    /* set non-blocking I/O */
    if (ioctl(sockfd, FIONBIO, &opt) < 0) {
	my_log(LOG_ERR, "DHCPv6Socket: ioctl FIONBIO failed, %s",
	       strerror(errno));
	goto failed;
    }
    /* ask for packet info */
    if (setsockopt(sockfd, IPPROTO_IPV6, 
		   IPCONFIG_SOCKOPT_PKTINFO, &opt, sizeof(opt)) < 0) {
	my_log(LOG_ERR, "DHCPv6Socket: setsockopt(IPV6_PKTINFO) failed, %s",
	       strerror(errno));
	goto failed;
    }

#if defined(SO_RECV_ANYIF)
    if (setsockopt(sockfd, SOL_SOCKET, SO_RECV_ANYIF, (caddr_t)&opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(SO_RECV_ANYIF) failed, %s",
	       strerror(errno));
    }
#endif /* SO_RECV_ANYIF */

#if defined(SO_TRAFFIC_CLASS)
    opt = SO_TC_CTL;
    /* set traffic class */
    if (setsockopt(sockfd, SOL_SOCKET, SO_TRAFFIC_CLASS, &opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(SO_TRAFFIC_CLASS) failed, %s",
	       strerror(errno));
    }
#endif /* SO_TRAFFIC_CLASS */

#if defined(SO_DEFUNCTOK)
    opt = 0;
    /* ensure that our socket can't be defunct'd */
    if (setsockopt(sockfd, SOL_SOCKET, SO_DEFUNCTOK, &opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(SO_DEFUNCTOK) failed, %s",
	       strerror(errno));
    }
#endif /* SO_DEFUNCTOK */

    opt = 0;
    /* don't loop our multicast packets back (rdar://problem/44307441) */
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(IPV6_MULTICAST_LOOP) failed, %s",
	       strerror(errno));
    }
    return (sockfd);

 failed:
    close(sockfd);
    return (-1);
}

STATIC void
DHCPv6SocketFreeElement(void * arg);

STATIC DHCPv6SocketGlobalsRef
DHCPv6SocketCreateGlobals(void)
{
    DHCPv6SocketGlobalsRef	globals = malloc(sizeof(*globals));

    if (globals == NULL) {
	return (NULL);
    }
    bzero(globals, sizeof(*globals));
    dynarray_init(&globals->sockets, DHCPv6SocketFreeElement, NULL);
    globals->timer_callout = timer_callout_init("DHCPv6Socket");
    return (globals);
}

STATIC void
DHCPv6SocketReleaseGlobals(DHCPv6SocketGlobalsRef * globals_p)
{
    DHCPv6SocketGlobalsRef	globals;

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

STATIC DHCPv6SocketGlobalsRef
DHCPv6SocketGetGlobals(void)
{
    if (S_globals != NULL) {
	return (S_globals);
    }
    S_globals = DHCPv6SocketCreateGlobals();
    return (S_globals);
}

STATIC void
DHCPv6SocketDelayedClose(void * arg1, void * arg2, void * arg3)
{
    if (S_globals->read_fd == NULL) {
	my_log(LOG_NOTICE,
	       "DHCPv6SocketDelayedClose(): socket is already closed");
	return;
    }
    if (S_globals->read_fd_refcount > 0) {
	my_log(LOG_NOTICE,
	       "DHCPv6SocketDelayedClose(): called when socket in use");
	return;
    }
    my_log(LOG_DEBUG,
	   "DHCPv6SocketDelayedClose(): closing DHCPv6 socket %d",
	   FDCalloutGetFD(S_globals->read_fd));

    /* this closes the file descriptor */
    FDCalloutRelease(&S_globals->read_fd);
    return;
}

STATIC void
DHCPv6SocketDemux(int if_index, const struct in6_addr * server_p,
		  const DHCPv6PacketRef pkt, int pkt_len)
{
    DHCPv6SocketReceiveData	data;
    DHCPv6OptionErrorString 	err;
    int 			i;

    if (pkt_len < DHCPV6_PACKET_HEADER_LENGTH) {
	return;
    }
    data.pkt = pkt;
    data.pkt_len = pkt_len;
    data.options = DHCPv6OptionListCreateWithPacket(pkt, pkt_len, &err);
    if (data.options == NULL) {
	my_log(LOG_NOTICE, "DHCPv6Socket: options parse failed, %s",
	       err.str);
	return;
    }
    for (i = 0; i < dynarray_count(&S_globals->sockets); i++) {
	DHCPv6SocketRef	client;
	char 		ntopbuf[INET6_ADDRSTRLEN];

	client = dynarray_element(&S_globals->sockets, i);
	if (if_index != if_link_index(DHCPv6SocketGetInterface(client))) {
	    continue;
	}
	if (S_verbose) {
	    CFMutableStringRef	str;
	    
	    str = CFStringCreateMutable(NULL, 0);
	    DHCPv6PacketPrintToString(str, pkt, pkt_len);
	    if (data.options != NULL) {
		DHCPv6OptionListPrintToString(str, data.options);
	    }
	    my_log(~LOG_INFO, "[%s] Receive from %s %@",
		   if_name(DHCPv6SocketGetInterface(client)),
		   inet_ntop(AF_INET6, server_p, ntopbuf, sizeof(ntopbuf)),
		   str);
	    CFRelease(str);
	}
	else {
	    my_log(LOG_INFO,"[%s] Receive %s (%d) [%d bytes] from %s",
		   if_name(DHCPv6SocketGetInterface(client)),
		   DHCPv6MessageName(pkt->msg_type), pkt->msg_type, pkt_len,
		   inet_ntop(AF_INET6, server_p, ntopbuf, sizeof(ntopbuf)));
	}
	if (client->receive_func != NULL) {
	    (*client->receive_func)(client->receive_arg1, client->receive_arg2,
				    &data);
	}
	break;
    }
    DHCPv6OptionListRelease(&data.options);
    return;
}

void
DHCPv6SocketSetVerbose(bool verbose)
{
    S_verbose = verbose;
    return;
}

PRIVATE_EXTERN void
DHCPv6SocketSetPorts(uint16_t client_port, uint16_t server_port)
{
    S_client_port = client_port;
    S_server_port = server_port;
    return;
}

PRIVATE_EXTERN interface_t *
DHCPv6SocketGetInterface(DHCPv6SocketRef sock)
{
    return (sock->if_p);
}

PRIVATE_EXTERN DHCPv6SocketRef
DHCPv6SocketCreate(interface_t * if_p)
{
    DHCPv6SocketRef		sock;
    DHCPv6SocketGlobalsRef	globals;

    globals = DHCPv6SocketGetGlobals();
    if (globals == NULL) {
	my_log(LOG_NOTICE, "DHCPv6SocketCreate: could not allocate globals");
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
DHCPv6SocketFreeElement(void * arg)
{
    DHCPv6SocketRef 	sock = (DHCPv6SocketRef)arg;

    DHCPv6SocketDisableReceive(sock);
    free(sock);
    return;
}

PRIVATE_EXTERN void
DHCPv6SocketRelease(DHCPv6SocketRef * sock_p)
{
    DHCPv6SocketRef 	sock = *sock_p;
    int 		i;

    if (sock == NULL) {
	return;
    }
    i = dynarray_index(&S_globals->sockets, sock);
    if (i != -1) {
	dynarray_remove(&S_globals->sockets, i, NULL);
    }
    else {
	my_log(LOG_NOTICE, "DHCPv6SocketRelease: %s not in list?",
	       if_name(sock->if_p));
    }
    DHCPv6SocketFreeElement(sock);
    *sock_p = NULL;
    if (dynarray_count(&S_globals->sockets) == 0) {
	DHCPv6SocketReleaseGlobals(&S_globals);
    }
    return;
}

STATIC void
DHCPv6SocketCloseSocket(DHCPv6SocketRef sock)
{
    if (sock->fd_open == FALSE) {
	return;
    }
    if (S_globals->read_fd_refcount <= 0) {
	my_log(LOG_NOTICE, "DHCPv6SocketCloseSocket(%s): refcount %d",
	       if_name(sock->if_p), S_globals->read_fd_refcount);
	return;
    }
    S_globals->read_fd_refcount--;
    my_log(LOG_DEBUG, "DHCPv6SocketCloseSocket(%s): refcount %d",
	   if_name(sock->if_p), S_globals->read_fd_refcount);
    sock->fd_open = FALSE;
    if (S_globals->read_fd_refcount == 0) {
	struct timeval tv;

	my_log(LOG_DEBUG,
	       "DHCPv6SocketCloseSocket(): scheduling delayed close");

	tv.tv_sec = 1; /* close it after 1 second of non-use */
	tv.tv_usec = 0;
	timer_set_relative(S_globals->timer_callout, tv,
			   DHCPv6SocketDelayedClose,
			   NULL, NULL, NULL);
    }
    return;
}

STATIC void
DHCPv6SocketRead(void * arg1, void * arg2)
{
    struct cmsghdr *	cm;
    char		cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    struct iovec 	iov;
    struct sockaddr_in6 from;
    struct msghdr 	mhdr;
    ssize_t		n;
    struct in6_pktinfo *pktinfo = NULL;
    char	 	receive_buf[1500];

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
	    my_log(LOG_ERR, "DHCPv6SocketRead: recvfrom failed %s (%d)",
		   strerror(errno), errno);
	}
	return;
    }
    if (n == 0) {
	/* nothing to read */
	return;
    }

    /* get the control message that has the interface index */
    pktinfo = NULL;
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
	    /* ALIGN: CMSG_DATA is should return aligned data */ 
	    pktinfo = (struct in6_pktinfo *)(void *)(CMSG_DATA(cm));
	    break;
	default:
	    /* this should never occur */
	    my_log(LOG_NOTICE, "Why did we get control message type %d?",
		   cm->cmsg_type);
	    break;
	}
    }
    if (pktinfo == NULL) {
	my_log(LOG_NOTICE,
	       "DHCPv6SocketRead: missing IPV6_PKTINFO");
	return;
    }
    DHCPv6SocketDemux(pktinfo->ipi6_ifindex, &from.sin6_addr,
		      (const DHCPv6PacketRef)receive_buf, (int)n);
    return;
}

STATIC boolean_t
DHCPv6SocketOpenSocket(DHCPv6SocketRef sock)
{
    if (sock->fd_open) {
	return (TRUE);
    }
    timer_cancel(S_globals->timer_callout);
    S_globals->read_fd_refcount++;
    my_log(LOG_DEBUG, "DHCPv6SocketOpenSocket (%s): refcount %d",
	   if_name(sock->if_p), S_globals->read_fd_refcount);
    sock->fd_open = TRUE;
    if (S_globals->read_fd_refcount > 1) {
	/* already open */
	return (TRUE);
    }
    if (S_globals->read_fd != NULL) {
	my_log(LOG_INFO, "DHCPv6SocketOpenSocket(): socket is still open");
    }
    else {
	int	sockfd;

	sockfd = open_dhcpv6_socket(S_client_port);
	if (sockfd < 0) {
	    my_log(LOG_NOTICE,
		   "DHCPv6SocketOpenSocket: socket() failed, %s",
		   strerror(errno));
	    goto failed;
	}
	my_log(LOG_DEBUG,
	       "DHCPv6SocketOpenSocket(): opened DHCPv6 socket %d",
	       sockfd);
	/* register as a reader */
	S_globals->read_fd = FDCalloutCreate(sockfd,
					     DHCPv6SocketRead,
					     NULL, NULL);
    }
    return (TRUE);

 failed:
    DHCPv6SocketCloseSocket(sock);
    return (FALSE);
}

PRIVATE_EXTERN void
DHCPv6SocketEnableReceive(DHCPv6SocketRef sock,
			  DHCPv6SocketReceiveFuncPtr func, 
			  void * arg1, void * arg2)
{
    sock->receive_func = func;
    sock->receive_arg1 = arg1;
    sock->receive_arg2 = arg2;
    if (DHCPv6SocketOpenSocket(sock) == FALSE) {
	my_log(LOG_NOTICE, "DHCPv6SocketEnableReceive(%s): failed",
	       if_name(sock->if_p));
    }
    return;
}

PRIVATE_EXTERN bool
DHCPv6SocketReceiveIsEnabled(DHCPv6SocketRef sock)
{
    return (sock->receive_func != NULL);
}

PRIVATE_EXTERN void
DHCPv6SocketDisableReceive(DHCPv6SocketRef sock)
{
    sock->receive_func = NULL;
    sock->receive_arg1 = NULL;
    sock->receive_arg2 = NULL;
    DHCPv6SocketCloseSocket(sock);
    return;
}

STATIC int
S_send_packet(int sockfd, int ifindex, DHCPv6PacketRef pkt, int pkt_size)
{
    struct sockaddr_in6	dst;

    dst = dhcpv6_all_servers_and_relay_agents;
    dst.sin6_port = htons(S_server_port);
    return (IPv6SocketSend(sockfd, ifindex, &dst, pkt, pkt_size, -1));
}

PRIVATE_EXTERN int
DHCPv6SocketTransmit(DHCPv6SocketRef sock,
		     DHCPv6PacketRef pkt, int pkt_len)
{
    DHCPv6OptionErrorString	err;
    int				ret;
    boolean_t			needs_close = FALSE;

    if (sock->fd_open == FALSE) {
	/* open the DHCPv6 socket in case it's needed */
	if (DHCPv6SocketOpenSocket(sock) == FALSE) {
	    my_log(LOG_NOTICE, "DHCPv6Socket: failed to open socket");
	    return (FALSE);
	}
	needs_close = TRUE;
    }
    if (S_verbose) {
	DHCPv6OptionListRef	options;
	CFMutableStringRef	str;
	
	str = CFStringCreateMutable(NULL, 0);
	DHCPv6PacketPrintToString(str, pkt, pkt_len);
	options = DHCPv6OptionListCreateWithPacket(pkt, pkt_len, &err);
	if (options == NULL) {
	    my_log_fl(LOG_DEBUG, "parse options failed, %s",
		      err.str);
	}
	else {
	    DHCPv6OptionListPrintToString(str, options);
	    DHCPv6OptionListRelease(&options);
	}
	my_log(~LOG_INFO, "[%s] Transmit %@", if_name(sock->if_p), str);
	CFRelease(str);
    }
    else {
	my_log(LOG_INFO, "[%s] Transmit %s (%d) [%d bytes]",
	       if_name(sock->if_p),
	       DHCPv6MessageName(pkt->msg_type),
	       pkt->msg_type,
	       pkt_len);
    }
    ret = S_send_packet(FDCalloutGetFD(S_globals->read_fd),
			if_link_index(sock->if_p), pkt, pkt_len);
    if (needs_close) {
	DHCPv6SocketCloseSocket(sock);
    }
    return (ret);
}
