/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
 * bootp_session.c
 * - maintain BOOTP client socket session
 * - maintain list of BOOTP clients
 * - distribute packet reception to enabled clients
 */
/* 
 * Modification History
 *
 * May 10, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
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
#include "dhcp_options.h"
#include "util.h"
#include <syslog.h>
#include <sys/uio.h>
#include "dhcplib.h"
#include "dynarray.h"
#include "bootp_session.h"
#include "bootp_transmit.h"
#include "timer.h"

static bool S_verbose;

struct bootp_session {
    dynarray_t			clients;
    FDCalloutRef		read_fd;
    int				read_fd_refcount;
    uint16_t			client_port;
    timer_callout_t *		timer_callout;
    bool			verbose;
};

struct bootp_client {
    bootp_session_t *		session; /* pointer to parent */
    interface_t *		if_p;
    boolean_t			fd_open;
    bootp_receive_func_t *	receive;
    void *			receive_arg1;
    void *			receive_arg2;
};


static void
bootp_session_read(void * arg1, void * arg2);

static int
S_open_bootp_socket(uint16_t client_port)
{
    struct sockaddr_in 		me;
    int 			status;
    int 			opt;
    int				sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	my_log(LOG_ERR, "socket failed, %s",
	       strerror(errno));
	return (-1);
    }
    bzero((char *)&me, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(client_port);
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
    if (status != 0) {
	my_log(LOG_ERR, "bootp_session: bind port %d failed, %s",
	       client_port, strerror(errno));
	goto failed;
    }
    opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt SO_BROADCAST failed, %s",
	       strerror(errno));
	goto failed;
    }
    status = ioctl(sockfd, FIONBIO, &opt);
    if (status < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed, %s", strerror(errno));
	goto failed;
    }
    status = setsockopt(sockfd, IPPROTO_IP, IP_RECVIF, &opt, sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt IP_RECVIF failed, %s", 
	       strerror(errno));
	goto failed;
    }
#if defined(SO_RECV_ANYIF)
    status = setsockopt(sockfd, SOL_SOCKET, SO_RECV_ANYIF, (caddr_t)&opt,
			sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt(SO_RECV_ANYIF) failed, %s",
	       strerror(errno));
    }
#endif /* SO_RECV_ANYIF */

#if defined(SO_TRAFFIC_CLASS)
    opt = SO_TC_CTL;
    /* set traffic class */
    status = setsockopt(sockfd, SOL_SOCKET, SO_TRAFFIC_CLASS, &opt,
			sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt(SO_TRAFFIC_CLASS) failed, %s",
	       strerror(errno));
    }
#endif /* SO_TRAFFIC_CLASS */

#if defined(SO_DEFUNCTOK)
    opt = 0;
    /* ensure that our socket can't be defunct'd */
    status = setsockopt(sockfd, SOL_SOCKET, SO_DEFUNCTOK, &opt,
			sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt(SO_DEFUNCTOK) failed, %s",
	       strerror(errno));
    }
#endif /* SO_DEFUNCTOK */
    return sockfd;

 failed:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (-1);
}

bootp_client_t *
bootp_client_init(bootp_session_t * session, interface_t * if_p)
{
    bootp_client_t *	client;

    client = malloc(sizeof(*client));
    if (client == NULL)
	return (NULL);
    bzero(client, sizeof(*client));
    if (dynarray_add(&session->clients, client) == FALSE) {
	free(client);
	return (NULL);
    }
    client->session = session;
    client->if_p = if_p;
    return (client);
}

static void
bootp_client_free_element(void * arg)
{
    bootp_client_t * 	client = (bootp_client_t *)arg;

    bootp_client_disable_receive(client);
    free(client);
    return;
}

void
bootp_client_free(bootp_client_t * * client_p)
{
    bootp_client_t * 	client = *client_p;
    int 		i;
    bootp_session_t * 	session;

    if (client == NULL) {
	return;
    }
    session = client->session;
    i = dynarray_index(&session->clients, client);
    if (i != -1) {
	dynarray_remove(&session->clients, i, NULL);
    }
    else {
	my_log(LOG_ERR, "bootp_client_free(%s) not in list?",
	       if_name(client->if_p));
    }
    bootp_client_free_element(client);
    *client_p = NULL;
    return;
}

static void
bootp_session_delayed_close(void * arg1, void * arg2, void * arg3)
{
    bootp_session_t * 	session = (bootp_session_t *)arg1;

    if (session->read_fd == NULL) {
	my_log(LOG_ERR, 
	       "bootp_session_delayed_close(): socket is already closed");
	return;
    }
    if (session->read_fd_refcount > 0) {
	my_log(LOG_ERR, 
	       "bootp_session_delayed_close(): called when socket in use");
	return;
    }
    my_log(LOG_DEBUG,
	   "bootp_session_delayed_close(): closing bootp socket %d",
	   FDCalloutGetFD(session->read_fd));

    /* this closes the file descriptor */
    FDCalloutRelease(&session->read_fd);
    return;
}

static boolean_t
bootp_session_open_socket(bootp_session_t * session)
{
    int	sockfd;

    sockfd =  S_open_bootp_socket(session->client_port);
    if (sockfd < 0) {
	my_log(LOG_ERR,
	       "bootp_session_open_socket: S_open_bootp_socket() failed, %s",
	       strerror(errno));
	return (FALSE);
    }
    my_log(LOG_DEBUG,
	   "bootp_session_open_socket(): opened bootp socket %d",
	   sockfd);
    /* register as a reader */
    session->read_fd = FDCalloutCreate(sockfd,
				       bootp_session_read,
				       session, NULL);
    return (TRUE);
}

static void
bootp_client_close_socket(bootp_client_t * client)
{
    bootp_session_t * 	session = client->session;

    if (client->fd_open == FALSE) {
	return;
    }
    if (session->read_fd_refcount <= 0) {
	my_log(LOG_NOTICE, "bootp_client_close_socket(%s): refcount %d",
	       if_name(client->if_p), session->read_fd_refcount);
	return;
    }
    session->read_fd_refcount--;
    my_log(LOG_DEBUG, "bootp_client_close_socket(%s): refcount %d",
	   if_name(client->if_p), session->read_fd_refcount);
    client->fd_open = FALSE;
    if (session->read_fd_refcount == 0) {
	struct timeval tv;

	my_log(LOG_DEBUG,
	       "bootp_client_close_socket(): scheduling delayed close");

	tv.tv_sec = 1; /* close it after 1 second of non-use */
	tv.tv_usec = 0;
	timer_set_relative(session->timer_callout, tv,
			   bootp_session_delayed_close,
			   session, NULL, NULL);
    }
    return;
}

static boolean_t
bootp_client_open_socket(bootp_client_t * client)
{
    bootp_session_t *	session = client->session;

    if (client->fd_open) {
	return (TRUE);
    }
    timer_cancel(session->timer_callout);
    session->read_fd_refcount++;
    my_log(LOG_DEBUG, "bootp_client_open_socket (%s): refcount %d",
	   if_name(client->if_p), session->read_fd_refcount);
    client->fd_open = TRUE;
    if (session->read_fd_refcount > 1) {
	/* already open */
	return (TRUE);
    }
    if (session->read_fd != NULL) {
	my_log(LOG_DEBUG, "bootp_client_open_socket(): socket is still open");
    }
    else if (!bootp_session_open_socket(session)) {
	my_log(LOG_NOTICE, "bootp_session_open_socket failed");
	goto failed;
    }
    return (TRUE);

 failed:
    bootp_client_close_socket(client);
    return (FALSE);
}

void
bootp_client_enable_receive(bootp_client_t * client,
			    bootp_receive_func_t * func, 
			    void * arg1, void * arg2)
{
    client->receive = func;
    client->receive_arg1 = arg1;
    client->receive_arg2 = arg2;
    if (bootp_client_open_socket(client) == FALSE) {
	my_log(LOG_ERR, "bootp_client_enable_receive(%s): failed",
	       if_name(client->if_p));
    }
    return;
}

void
bootp_client_disable_receive(bootp_client_t * client)
{
    client->receive = NULL;
    client->receive_arg1 = NULL;
    client->receive_arg2 = NULL;
    bootp_client_close_socket(client);
    return;
}

static void
bootp_client_bind_socket_to_if(bootp_client_t * client, int opt)
{
    bootp_session_t *	session = client->session;
    int			fd = -1;

    if (session->read_fd != NULL) {
	fd = FDCalloutGetFD(session->read_fd);
    }
    if (fd < 0) {
	my_log(LOG_ERR, 
	       "bootp_client_bind_socket_to_if(%s, %d):"
	       " session socket isn't open", if_name(client->if_p), opt);
    }
    else if (setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &opt, sizeof(opt)) < 0) {
	my_log(LOG_ERR, 
	       "bootp_client_bind_socket_to_if(%s, %d):"
	       " setsockopt IP_BOUND_IF failed %s",
	       if_name(client->if_p), opt, strerror(errno));
    }
    return;
}

int
bootp_client_transmit(bootp_client_t * client,
		      struct in_addr dest_ip,
		      struct in_addr src_ip,
		      uint16_t dest_port,
		      uint16_t src_port,
		      void * data, int len)
{
    int			error;
    int			if_index = 0;
    boolean_t		needs_close = FALSE;
    /*
     * send_buf is cast to some struct types containing short fields;
     * force it to be aligned as much as an int
     */
    int 		send_buf_aligned[512];
    char * 		send_buf = (char *)send_buf_aligned;
    bootp_session_t *	session = client->session;
    int			sockfd = -1;

    /* if we're not broadcasting, bind the socket to the interface */
    if (dest_ip.s_addr != INADDR_BROADCAST) {
	if (client->fd_open == FALSE) {
	    /* open the BOOTP socket in case it's needed */
	    (void)bootp_client_open_socket(client);
	    needs_close = TRUE;
	}
	if_index = if_link_index(client->if_p);
	if (if_index != 0) {
	    bootp_client_bind_socket_to_if(client, if_index);
	}
    }
    if (S_verbose) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	dhcp_packet_print_cfstr(str, (struct dhcp *)data, len);
	if (if_index != 0) {
	    my_log(~LOG_INFO,
		   "[%s] Transmit %d byte packet dest "
		   IP_FORMAT " scope %d\n%@",
		   if_name(client->if_p), len,
		   IP_LIST(&dest_ip), if_index, str);
	}
	else {
	    my_log(~LOG_INFO,
		   "[%s] Transmit %d byte packet\n%@",
		   if_name(client->if_p), len, str);
	}
	CFRelease(str);
    }
    else {
	my_log(LOG_INFO,
	       "[%s] Transmit %d byte packet xid 0x%lx to "
	       IP_FORMAT " [scope=%d]",
	       if_name(client->if_p), len,
	       (unsigned long)ntohl(((struct dhcp *)data)->dp_xid),
	       IP_LIST(&dest_ip),
	       if_index);
    }
    if (session->read_fd != NULL) {
	sockfd = FDCalloutGetFD(session->read_fd);
    }
    error = bootp_transmit(sockfd, send_buf,
			   if_name(client->if_p),
			   if_link_arptype(client->if_p), NULL,
			   dest_ip, src_ip, dest_port, src_port, data, len);
    if (needs_close) {
	bootp_client_close_socket(client);
    }
    if (if_index != 0) {
	bootp_client_bind_socket_to_if(client, 0);
    }
    return (error);
}
		       


bootp_session_t *
bootp_session_init(uint16_t client_port)
{
    bootp_session_t * session = malloc(sizeof(*session));
    if (session == NULL)
	return (NULL);
    bzero(session, sizeof(*session));
    dynarray_init(&session->clients, bootp_client_free_element, NULL);
    session->client_port = client_port;
    session->timer_callout = timer_callout_init("bootp_session");

    return (session);
}

void
bootp_session_free(bootp_session_t * * session_p)
{
    bootp_session_t * session = *session_p;

    dynarray_free(&session->clients);
    FDCalloutRelease(&session->read_fd);
    timer_callout_free(&session->timer_callout);
    bzero(session, sizeof(*session));
    free(session);
    *session_p = NULL;
    return;
}

static void
bootp_session_deliver(bootp_session_t * session, const char * ifname,
		      const struct in_addr * from_ip,
		      void * data, int size)
{
    bootp_receive_data_t	event;
    int 			i;

    if (size < sizeof(struct dhcp)) {
	return;
    }

    bzero(&event, sizeof(event));
    event.data = (struct dhcp *)data;
    event.size = size;
    dhcpol_parse_packet(&event.options, event.data, size, NULL);
    if (S_verbose) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	dhcp_packet_with_options_print_cfstr(str, event.data, size,
					     &event.options);
	my_log(~LOG_INFO,
	       "[%s] Receive %d byte packet from " IP_FORMAT "\n%@",
	       ifname, size, IP_LIST(from_ip), str);
	CFRelease(str);
    }
    else {
	my_log(LOG_INFO, "[%s] Receive %d byte packet xid 0x%lx from "
	       IP_FORMAT,
	       ifname, size,
	       (unsigned long)ntohl(event.data->dp_xid),
	       IP_LIST(from_ip));
    }

    for (i = 0; i < dynarray_count(&session->clients); i++) {
	bootp_client_t *	client;

	client = dynarray_element(&session->clients, i);
	if (strcmp(if_name(client->if_p), ifname) != 0) {
	    continue;
	}
	if (client->receive) {
	    (*client->receive)(client->receive_arg1, client->receive_arg2,
			       &event);
	}
    }
    dhcpol_free(&event.options); /* free malloc'd data */
    return;
}

static void *
msghdr_parse_control(struct msghdr * msg_p, int level, int type, int * len)
{
    struct cmsghdr *	cmsg;

    *len = 0;
    for (cmsg = CMSG_FIRSTHDR(msg_p); cmsg; cmsg = CMSG_NXTHDR(msg_p, cmsg)) {
	if (cmsg->cmsg_level == level 
	    && cmsg->cmsg_type == type) {
	    if (cmsg->cmsg_len < sizeof(*cmsg))
		return (NULL);
	    *len = cmsg->cmsg_len - sizeof(*cmsg);
	    return (CMSG_DATA(cmsg));
	}
    }
    return (NULL);
}

static boolean_t
msghdr_copy_ifname(struct msghdr * msg_p, char * ifname, int ifname_size)
{
    int 			len = 0;
    struct sockaddr_dl *	dl_p;

    dl_p = (struct sockaddr_dl *)msghdr_parse_control(msg_p,
						      IPPROTO_IP, IP_RECVIF, 
						      &len);
    if (dl_p == NULL || len == 0 || dl_p->sdl_nlen >= ifname_size) {
	return (FALSE);
    }
    bcopy(dl_p->sdl_data, ifname, dl_p->sdl_nlen);
    ifname[dl_p->sdl_nlen] = '\0';
    return (TRUE);
}

static void
bootp_session_read(void * arg1, void * arg2)
{
    char	 		control[512];
    struct sockaddr_in 		from;
    char			ifname[IFNAMSIZ + 1];
    struct iovec 	 	iov;
    struct msghdr 		msg;
    ssize_t 			n;
    uint32_t 			receive_buf[2048/(sizeof(uint32_t))];
    bootp_session_t * 		session = (bootp_session_t *)arg1;

    msg.msg_name = (caddr_t)&from;
    msg.msg_namelen = sizeof(from);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    msg.msg_flags = 0;
    iov.iov_base = (caddr_t)receive_buf;
    iov.iov_len = sizeof(receive_buf);
    n = recvmsg(FDCalloutGetFD(session->read_fd), &msg, 0);
    if (n > 0) {
	if (msghdr_copy_ifname(&msg, ifname, sizeof(ifname))) {
	    /* ALIGN: receive_buf aligned to uint64_t bytes */
	    bootp_session_deliver(session, ifname, &from.sin_addr,
				  (void *)receive_buf, (int)n);
	}
    }
    else if (n < 0) {
	int	error = errno;

	if (error != EAGAIN) {
	    my_log(LOG_ERR, "bootp_session_read(%d): recvmsg failed, %s",
		   FDCalloutGetFD(session->read_fd), strerror(error));
	    if (error == ENOTCONN) {
		/* close and re-open */
		FDCalloutRelease(&session->read_fd);
		bootp_session_open_socket(session);
	    }
	}
    }
    return;
}

void
bootp_session_set_verbose(bool verbose)
{
    S_verbose = verbose;
    return;
}
