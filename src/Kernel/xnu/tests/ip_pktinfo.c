/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#define __APPLE_USE_RFC_3542 1

#include <darwintest.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "net_test_lib.h"

#define MAX_IPv4_STR_LEN        16
#define MAX_IPv6_STR_LEN        64

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.net"),
	T_META_ASROOT(true),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("networking"),
	T_META_CHECK_LEAKS(false));

static char *ifname1;
static char *ifname2;

#define IPV4_MULTICAST_ADDR_STR "239.1.2.3"
#define IPV6_MULTICAST_ADDR_STR "FF12:0:0:0:0:0:0:FC"

#define TEN_NET                 0x0a000000
#define TEN_1_NET               (TEN_NET | 0x010000)
#define TEN_1_BROADCAST         (TEN_1_NET | 0xff)

static network_interface_pair_list_t    S_feth_pairs;


static char *data = "hello\n";

static bool success = false;

static void
get_ipv4_address(u_int unit, u_int addr_index, struct in_addr *ip)
{
	/* up to 255 units, 255 addresses */
	ip->s_addr = htonl(TEN_1_NET | (unit << 8) | addr_index);
	return;
}

static void
network_interface_assign_address(network_interface_t netif,
    unsigned int unit, unsigned int address_index)
{
	get_ipv4_address(unit, address_index, &netif->ip);
	ifnet_add_ip_address(netif->if_name, netif->ip,
	    inet_class_c_subnet_mask);
	route_add_inet_scoped_subnet(netif->if_name, netif->if_index,
	    netif->ip, inet_class_c_subnet_mask);
	ifnet_start_ipv6(netif->if_name);
	T_ASSERT_EQ(inet6_get_linklocal_address(netif->if_index, &netif->ip6), 1, NULL);
}

static void
initialize_feth_pairs(u_int n, bool need_address)
{
	network_interface_pair_t        scan;

	S_feth_pairs = network_interface_pair_list_alloc(n);
	scan = S_feth_pairs->list;
	for (unsigned int i = 0; i < n; i++, scan++) {
		network_interface_create(&scan->one, FETH_NAME);
		network_interface_create(&scan->two, FETH_NAME);
		if (need_address) {
			network_interface_assign_address(&scan->one, i, 1);
			network_interface_assign_address(&scan->two, i, 2);
		}
		fake_set_peer(scan->one.if_name, scan->two.if_name);
	}

	ifname1 = S_feth_pairs->list->one.if_name;
	ifname2 = S_feth_pairs->list->two.if_name;
}

static void
cleanup(void)
{
	network_interface_pair_list_destroy(S_feth_pairs);
	/* allow for the detach to be final before the next test */
	usleep(100000);
}

static void
init(void)
{
	T_ATEND(cleanup);

	success = false;
	initialize_feth_pairs(1, true);
}

static int
setup_receiver(char *bind_to_ifname, bool bind_to_port, in_addr_t bind_to_addr, in_port_t *bound_port)
{
	int receiver_fd;
	socklen_t solen;
	struct sockaddr_in sin = {};
	char ifname[IFNAMSIZ];
	char laddr_str[MAX_IPv4_STR_LEN];
	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	int optval;

	/*
	 * Setup receiver bound to ifname1
	 */
	T_ASSERT_POSIX_SUCCESS(receiver_fd = socket(AF_INET, SOCK_DGRAM, 0), NULL);

	T_ASSERT_POSIX_SUCCESS(setsockopt(receiver_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receiver_fd, SOL_SOCKET, SO_DEBUG, &optval, sizeof(int)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receiver_fd, IPPROTO_IP, IP_RECVPKTINFO, &optval, sizeof(int)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receiver_fd, IPPROTO_UDP, UDP_NOCKSUM, &optval, sizeof(int)), NULL);

	if (bind_to_ifname != NULL) {
		solen = strlen(bind_to_ifname);
		T_ASSERT_POSIX_SUCCESS(setsockopt(receiver_fd, SOL_SOCKET, SO_BINDTODEVICE, bind_to_ifname, solen), NULL);
	}

	if (bind_to_port || bind_to_addr != INADDR_ANY) {
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr.s_addr = bind_to_addr;
		T_ASSERT_POSIX_SUCCESS(bind(receiver_fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)), NULL);
	}
	solen = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(getsockname(receiver_fd, (struct sockaddr *)&sin, &solen), NULL);
	inet_ntop(AF_INET, &sin.sin_addr, laddr_str, sizeof(laddr_str));

	solen = sizeof(ifname);
	T_ASSERT_POSIX_SUCCESS(getsockopt(receiver_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, &solen), NULL);

	T_LOG("receiver bound to %s:%u over '%s'", laddr_str, ntohs(sin.sin_port), ifname);

	*bound_port = sin.sin_port;
	return receiver_fd;
}

int
setup_sender(char *bind_to_ifname, in_addr_t connect_to_addr, in_port_t connect_to_port)
{
	int sender_fd;
	struct sockaddr_in connect_to_sin = {};
	struct sockaddr_in sin = {};
	socklen_t solen;
	char laddr_str[MAX_IPv4_STR_LEN];
	char faddr_str[MAX_IPv4_STR_LEN];
	char ifname[IFNAMSIZ];
	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	int optval;

	T_ASSERT_POSIX_SUCCESS(sender_fd = socket(AF_INET, SOCK_DGRAM, 0), NULL);

	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_DEBUG, &optval, sizeof(int)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_IP, IP_RECVPKTINFO, &optval, sizeof(int)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_UDP, UDP_NOCKSUM, &optval, sizeof(int)), NULL);

	if (bind_to_ifname != NULL) {
		solen = strlen(bind_to_ifname);
		T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_BINDTODEVICE, bind_to_ifname, solen), NULL);
	}

	connect_to_sin.sin_family = AF_INET;
	connect_to_sin.sin_len = sizeof(struct sockaddr_in);
	connect_to_sin.sin_port = connect_to_port;
	connect_to_sin.sin_addr.s_addr = connect_to_addr;

	T_ASSERT_POSIX_SUCCESS(connect(sender_fd, (struct sockaddr *)&connect_to_sin, sizeof(struct sockaddr_in)), NULL);

	solen = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(getsockname(sender_fd, (struct sockaddr *)&sin, &solen), NULL);
	inet_ntop(AF_INET, &sin.sin_addr, laddr_str, sizeof(laddr_str));
	inet_ntop(AF_INET, &connect_to_sin.sin_addr, faddr_str, sizeof(faddr_str));

	solen = sizeof(ifname);
	T_ASSERT_POSIX_SUCCESS(getsockopt(sender_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, &solen), NULL);

	T_LOG("sender_fd connected from %s:%u to %s:%u over '%s'",
	    laddr_str, ntohs(sin.sin_port), faddr_str, ntohs(connect_to_sin.sin_port),
	    ifname);

	return sender_fd;
}


static void
echo(int receiver_fd, bool by_ip_addr)
{
	struct msghdr recvmsghdr = {};
	char control_space[CMSG_SPACE(128)] = {};
	char packet_space[1500] = {};
	struct cmsghdr *cmsg;
	ssize_t retval;
	struct iovec recv_iov = {};
	struct sockaddr_in peer_addr;
	struct in_pktinfo recv_in_pktinfo = {};
	struct in_pktinfo send_in_pktinfo = {};
	char ifname[IFNAMSIZ] = {};
	struct msghdr reply_msg = {};
	struct iovec reply_iov = {};
	char reply_control_space[CMSG_SPACE(128)] = {};
	char spec_dst_str[MAX_IPv4_STR_LEN];
	char addr_str[MAX_IPv4_STR_LEN];
	char peer_addr_str[MAX_IPv4_STR_LEN];

	T_LOG("%s(by_ip_addr: %s)", __func__, by_ip_addr ? "true" : "false");

	recv_iov.iov_len = sizeof(packet_space);
	recv_iov.iov_base = &packet_space;

	recvmsghdr.msg_name = &peer_addr;
	recvmsghdr.msg_namelen = sizeof(struct sockaddr_in);
	recvmsghdr.msg_iov = &recv_iov;
	recvmsghdr.msg_iovlen = 1;
	recvmsghdr.msg_control = &control_space;
	recvmsghdr.msg_controllen = sizeof(control_space);
	recvmsghdr.msg_flags = 0;

	T_ASSERT_POSIX_SUCCESS(retval = recvmsg(receiver_fd, &recvmsghdr, 0), NULL);

	for (cmsg = CMSG_FIRSTHDR(&recvmsghdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&recvmsghdr, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVPKTINFO) {
			T_ASSERT_EQ(CMSG_LEN(sizeof(struct in_pktinfo)), (size_t)cmsg->cmsg_len,
			    "CMSG_LEN(struct in_pktinfo), (size_t)cmsg->cmsg_len");
			memcpy(&recv_in_pktinfo, CMSG_DATA(cmsg), sizeof(struct in_pktinfo));
		}
	}

	ifname[0] = 0;
	if_indextoname(recv_in_pktinfo.ipi_ifindex, ifname);
	inet_ntop(AF_INET, &recv_in_pktinfo.ipi_spec_dst, spec_dst_str, sizeof(spec_dst_str));
	inet_ntop(AF_INET, &recv_in_pktinfo.ipi_addr, addr_str, sizeof(addr_str));
	inet_ntop(AF_INET, &peer_addr.sin_addr, peer_addr_str, sizeof(peer_addr_str));

	T_LOG("received %ld bytes from %s:%u with IP_RECVPKTINFO ipi_ifindex: %u (%s) ipi_spec_dst: %s ipi_addr: %s",
	    retval, peer_addr_str, ntohs(peer_addr.sin_port),
	    recv_in_pktinfo.ipi_ifindex, ifname, spec_dst_str, addr_str);

	reply_iov.iov_base = packet_space;
	reply_iov.iov_len = retval;

	reply_msg.msg_name = &peer_addr;
	reply_msg.msg_namelen = sizeof(struct sockaddr_in);
	reply_msg.msg_iov = &reply_iov;
	reply_msg.msg_iovlen = 1;
	reply_msg.msg_control = reply_control_space;
	reply_msg.msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));

	send_in_pktinfo.ipi_addr.s_addr = 0;
	if (by_ip_addr) {
		send_in_pktinfo.ipi_ifindex = 0;
		send_in_pktinfo.ipi_spec_dst.s_addr = recv_in_pktinfo.ipi_addr.s_addr;
	} else {
		send_in_pktinfo.ipi_ifindex = recv_in_pktinfo.ipi_ifindex;
		send_in_pktinfo.ipi_spec_dst.s_addr = 0;
	}
	cmsg = CMSG_FIRSTHDR(&reply_msg);
	cmsg->cmsg_level = IPPROTO_IP;
	cmsg->cmsg_type = IP_PKTINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
	memcpy(CMSG_DATA(cmsg), &send_in_pktinfo, sizeof(struct in_pktinfo));

	ifname[0] = 0;
	if_indextoname(send_in_pktinfo.ipi_ifindex, ifname);
	inet_ntop(AF_INET, &send_in_pktinfo.ipi_spec_dst, spec_dst_str, sizeof(spec_dst_str));
	inet_ntop(AF_INET, &send_in_pktinfo.ipi_addr, addr_str, sizeof(addr_str));

	T_LOG("sending %ld bytes to %s:%u with IP_PKTINFO ipi_ifindex: %u (%s) ipi_spec_dst: %s ipi_addr: %s",
	    retval, peer_addr_str, ntohs(peer_addr.sin_port),
	    send_in_pktinfo.ipi_ifindex, ifname, spec_dst_str, addr_str);

	T_ASSERT_POSIX_SUCCESS(retval = sendmsg(receiver_fd, &reply_msg, 0), NULL);
}

static void
echo_and_check(int receiver_fd, bool by_ip_addr)
{
	socklen_t solen;
	struct sockaddr_in before_sin = {};
	char before_ifname[IFNAMSIZ];
	u_int before_ifindex;
	struct sockaddr_in after_sin = {};
	char after_ifname[IFNAMSIZ];
	u_int after_ifindex;
	char before_addr_str[MAX_IPv4_STR_LEN];
	char after_addr_str[MAX_IPv4_STR_LEN];

	T_LOG("%s(by_ip_addr: %s)", __func__, by_ip_addr ? "true" : "false");

	solen = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(getsockname(receiver_fd, (struct sockaddr *)&before_sin, &solen), NULL);
	inet_ntop(AF_INET, &before_sin.sin_addr, before_addr_str, sizeof(before_addr_str));

	solen = sizeof(before_ifname);
	T_ASSERT_POSIX_SUCCESS(getsockopt(receiver_fd, SOL_SOCKET, SO_BINDTODEVICE, before_ifname, &solen), NULL);
	before_ifindex = if_nametoindex(before_ifname);

	echo(receiver_fd, by_ip_addr);

	solen = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(getsockname(receiver_fd, (struct sockaddr *)&after_sin, &solen), NULL);
	inet_ntop(AF_INET, &after_sin.sin_addr, after_addr_str, sizeof(after_addr_str));

	solen = sizeof(after_ifname);
	T_ASSERT_POSIX_SUCCESS(getsockopt(receiver_fd, SOL_SOCKET, SO_BINDTODEVICE, after_ifname, &solen), NULL);
	after_ifindex = if_nametoindex(after_ifname);


	T_LOG("before bound to %s:%u over '%s'/%u", before_addr_str, ntohs(before_sin.sin_port), before_ifname, before_ifindex);
	T_LOG("after bound to %s:%u over '%s'/%u", after_addr_str, ntohs(after_sin.sin_port), after_ifname, after_ifindex);

	T_ASSERT_EQ_USHORT(before_sin.sin_port, after_sin.sin_port, "same port");
	T_ASSERT_EQ_UINT(before_sin.sin_addr.s_addr, after_sin.sin_addr.s_addr, "same IP address");
	T_ASSERT_EQ_UINT(before_ifindex, after_ifindex, "same interface index");
}

static void
do_test_ip_pktinfo(bool bind_to_device, bool bind_to_port, in_addr_t bind_to_addr)
{
	int receiver_fd;
	in_port_t receiver_port = 0;
	int sender_fd;
	ssize_t retval;

	init();

	receiver_fd = setup_receiver(bind_to_device ? ifname1 : NULL,
	    bind_to_port,
	    bind_to_addr ? S_feth_pairs->list->one.ip.s_addr : INADDR_ANY,
	    &receiver_port);
	sender_fd = setup_sender(ifname2, S_feth_pairs->list->one.ip.s_addr, receiver_port);

	T_ASSERT_POSIX_SUCCESS(retval = send(sender_fd, data, strlen(data) + 1, 0), NULL);
	echo_and_check(receiver_fd, true);

	T_ASSERT_POSIX_SUCCESS(retval = send(sender_fd, data, strlen(data) + 1, 0), NULL);
	echo_and_check(receiver_fd, false);

	close(sender_fd);
	close(receiver_fd);

	success = true;
}


T_DECL(ip_pktinfo_010, "IP_PTKINFO bind_to_device=false bind_to_port=true bind_to_addr=false")
{
	do_test_ip_pktinfo(false, true, false);
}

T_DECL(ip_pktinfo_011, "IP_PTKINFO bind_to_device=false bind_to_port=true bind_to_addr=true")
{
	do_test_ip_pktinfo(false, true, true);
}

T_DECL(ip_pktinfo_110, "IP_PTKINFO bind_to_device=true bind_to_port=true bind_to_addr=false")
{
	do_test_ip_pktinfo(true, true, false);
}

T_DECL(ip_pktinfo_111, "IP_PTKINFO bind_to_device=true bind_to_port=true bind_to_addr=true")
{
	do_test_ip_pktinfo(true, true, true);
}
