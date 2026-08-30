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
}

static void
init(void)
{
	T_ATEND(cleanup);

	initialize_feth_pairs(1, true);
}

T_DECL(ip_recv_link_addr_type, "IP_RECV_LINK_ADDR_TYPE")
{
	int receive_fd;
	int sender_fd;
	socklen_t solen;
	int optval;
	struct ip_mreq mreq = {};
	struct sockaddr_in sin = {};
	struct in_addr addr;
	char *str;
	ssize_t retval;
	in_port_t port;

	init();

	/*
	 * Setup receiver bound to ifname1
	 */
	T_ASSERT_POSIX_SUCCESS(receive_fd = socket(AF_INET, SOCK_DGRAM, 0), NULL);

	solen = strlen(ifname1);
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname1, solen), NULL);

	/*
	 * Verify the IP_RECV_LINK_ADDR_TYPE option is setable
	 */
	solen = sizeof(int);

	T_ASSERT_POSIX_SUCCESS(getsockopt(receive_fd, IPPROTO_IP, IP_RECV_LINK_ADDR_TYPE, &optval, &solen), NULL);
	T_LOG("IP_RECV_LINK_ADDR_TYPE default: %d", optval);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IP, IP_RECV_LINK_ADDR_TYPE, &optval, solen), NULL);

	T_ASSERT_POSIX_SUCCESS(getsockopt(receive_fd, IPPROTO_IP, IP_RECV_LINK_ADDR_TYPE, &optval, &solen), NULL);
	T_LOG("IP_RECV_LINK_ADDR_TYPE enabled: %d", optval);

	/*
	 * Join multicast group on ifname1
	 */
	inet_aton(IPV4_MULTICAST_ADDR_STR, &mreq.imr_multiaddr);
	mreq.imr_interface = S_feth_pairs->list->one.ip;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)), NULL);

	struct timeval timeo = { .tv_sec = 1, .tv_usec = 0 };
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IP, IP_RECVDSTADDR, &optval, sizeof(optval)), NULL);

	/*
	 * Bind to an ephemeral port
	 */
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(bind(receive_fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)), NULL);

	solen = sizeof(struct sockaddr_in);
	T_ASSERT_POSIX_SUCCESS(getsockname(receive_fd, (struct sockaddr *)&sin, &solen), NULL);

	port = sin.sin_port;
	T_LOG("receiver bound to port %u", ntohs(port));


	/*
	 * Setup receiver bound to ifname2
	 */
	T_ASSERT_POSIX_SUCCESS(sender_fd = socket(AF_INET, SOCK_DGRAM, 0), NULL);

	solen = strlen(ifname2);
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname2, solen), NULL);

	addr = S_feth_pairs->list->two.ip;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)), NULL);

	u_char ttl = 255;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)), NULL);

	/*
	 * Send unicast, broadcast and multicast a few times to allow for ARP to do its job
	 */
	for (int i = 0; i < 3; i++) {
		str = "unicast";
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = S_feth_pairs->list->one.ip;
		sin.sin_port = port;
		T_ASSERT_POSIX_SUCCESS(retval = sendto(sender_fd, str, strlen(str) + 1, 0, (struct sockaddr *)&sin, sin.sin_len), NULL);

		str = "broadcast";
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr.s_addr = htonl(TEN_1_BROADCAST);
		sin.sin_port = port;
		T_ASSERT_POSIX_SUCCESS(retval = sendto(sender_fd, str, strlen(str) + 1, 0, (struct sockaddr *)&sin, sin.sin_len), NULL);

		str = "multicast";
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		inet_aton(IPV4_MULTICAST_ADDR_STR, &sin.sin_addr);
		sin.sin_port = port;
		T_ASSERT_POSIX_SUCCESS(retval = sendto(sender_fd, str, strlen(str) + 1, 0, (struct sockaddr *)&sin, sin.sin_len), NULL);

		usleep(50);
	}

	while (true) {
		char control_space[CMSG_SPACE(8192)] = {};
		struct msghdr recvmsghdr = {};
		char packet_space[1500] = {};
		struct cmsghdr *cmsg;
		int addr_type = -1;

		struct iovec recv_iov;
		recv_iov.iov_len = sizeof(packet_space);
		recv_iov.iov_base = &packet_space;

		recvmsghdr.msg_iov = &recv_iov;
		recvmsghdr.msg_iovlen = 1;
		recvmsghdr.msg_control = &control_space;
		recvmsghdr.msg_controllen = sizeof(control_space);
		recvmsghdr.msg_flags = 0;

		retval = recvmsg(receive_fd, &recvmsghdr, 0);
		if (retval < 0) {
			break;
		}

		for (cmsg = CMSG_FIRSTHDR(&recvmsghdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&recvmsghdr, cmsg)) {
			if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVDSTADDR) {
				addr.s_addr = *(in_addr_t *)CMSG_DATA(cmsg);
			}
			if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECV_LINK_ADDR_TYPE) {
				addr_type = *(int *)CMSG_DATA(cmsg);
			}
		}
		T_LOG("received packet to: %s address type: %d", inet_ntoa(addr), addr_type);

		if (IN_MULTICAST(ntohl(addr.s_addr))) {
			T_ASSERT_EQ(addr_type, IP_RECV_LINK_ADDR_MULTICAST, "multicast");
		} else if ((ntohl(addr.s_addr) & 0x000000ff) == 0x000000ff) {
			T_ASSERT_EQ(addr_type, IP_RECV_LINK_ADDR_BROADCAST, "broadcast");
		} else {
			T_ASSERT_EQ(addr_type, IP_RECV_LINK_ADDR_UNICAST, "unicast");
		}
	}
}

T_DECL(ipv6_recv_link_addr_type, "IPV6_RECV_LINK_ADDR_TYPE")
{
	int receive_fd;
	int sender_fd;
	socklen_t solen;
	int optval;
	struct ipv6_mreq mreq = {};
	struct sockaddr_in6 sin6 = {};
	char *str;
	ssize_t retval;
	in_port_t port;
	char addrstr[INET6_ADDRSTRLEN];

	init();

	inet_ntop(AF_INET6, &S_feth_pairs->list->one.ip6, addrstr, sizeof(addrstr));
	T_LOG("feth one: %s index: %u ip: %s ip6: %s",
	    S_feth_pairs->list->one.if_name,
	    S_feth_pairs->list->one.if_index,
	    inet_ntoa(S_feth_pairs->list->one.ip),
	    addrstr);

	inet_ntop(AF_INET6, &S_feth_pairs->list->two.ip6, addrstr, sizeof(addrstr));
	T_LOG("feth one: %s index: %u ip: %s ip6: %s",
	    S_feth_pairs->list->two.if_name,
	    S_feth_pairs->list->two.if_index,
	    inet_ntoa(S_feth_pairs->list->two.ip),
	    addrstr);


	/*
	 * Setup receiver bound to ifname1
	 */
	T_ASSERT_POSIX_SUCCESS(receive_fd = socket(AF_INET6, SOCK_DGRAM, 0), NULL);

	solen = strlen(ifname1);
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname1, solen), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)), NULL);

	/*
	 * Verify the IP_RECV_LINK_ADDR_TYPE option is setable
	 */
	solen = sizeof(int);

	T_ASSERT_POSIX_SUCCESS(getsockopt(receive_fd, IPPROTO_IPV6, IPV6_RECV_LINK_ADDR_TYPE, &optval, &solen), NULL);
	T_LOG("IPV6_RECV_LINK_ADDR_TYPE default: %d", optval);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IPV6, IPV6_RECV_LINK_ADDR_TYPE, &optval, solen), NULL);

	T_ASSERT_POSIX_SUCCESS(getsockopt(receive_fd, IPPROTO_IPV6, IPV6_RECV_LINK_ADDR_TYPE, &optval, &solen), NULL);
	T_LOG("IPV6_RECV_LINK_ADDR_TYPE enabled: %d", optval);

	/*
	 * Join multicast group on ifname1
	 */
	inet_pton(AF_INET6, IPV6_MULTICAST_ADDR_STR, &mreq.ipv6mr_multiaddr);
	mreq.ipv6mr_interface = S_feth_pairs->list->one.if_index;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)), NULL);

	struct timeval timeo = { .tv_sec = 1, .tv_usec = 0 };
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(receive_fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &optval, sizeof(optval)), NULL);

	/*
	 * Bind to an ephemeral port
	 */
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	T_ASSERT_POSIX_SUCCESS(bind(receive_fd, (struct sockaddr *)&sin6, sizeof(struct sockaddr_in6)), NULL);

	solen = sizeof(struct sockaddr_in6);
	T_ASSERT_POSIX_SUCCESS(getsockname(receive_fd, (struct sockaddr *)&sin6, &solen), NULL);

	port = sin6.sin6_port;
	T_LOG("receiver bound to port %u", ntohs(port));


	/*
	 * Setup receiver bound to ifname2
	 */
	T_ASSERT_POSIX_SUCCESS(sender_fd = socket(AF_INET6, SOCK_DGRAM, 0), NULL);

	solen = strlen(ifname2);
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_BINDTODEVICE, ifname2, solen), NULL);

	optval = S_feth_pairs->list->two.if_index;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &optval, sizeof(optval)), NULL);

	optval = IPV6_DEFHLIM;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &optval, sizeof(optval)), NULL);

	optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sender_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)), NULL);

	/*
	 * Send unicast, broadcast and multicast a few times to allow for ND to do its job
	 */
	for (int i = 0; i < 3; i++) {
		str = "unicast";
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr = S_feth_pairs->list->one.ip6;
		sin6.sin6_port = port;
		sin6.sin6_scope_id = S_feth_pairs->list->two.if_index;
		T_ASSERT_POSIX_SUCCESS(retval = sendto(sender_fd, str, strlen(str) + 1, 0, (struct sockaddr *)&sin6, sin6.sin6_len), NULL);

		str = "multicast";
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		inet_pton(AF_INET6, IPV6_MULTICAST_ADDR_STR, &sin6.sin6_addr);
		sin6.sin6_port = port;
		T_ASSERT_POSIX_SUCCESS(retval = sendto(sender_fd, str, strlen(str) + 1, 0, (struct sockaddr *)&sin6, sin6.sin6_len), NULL);

		usleep(50);
	}

	while (true) {
		char control_space[CMSG_SPACE(8192)] = {};
		struct msghdr recvmsghdr = {};
		char packet_space[1500] = {};
		struct cmsghdr *cmsg;
		int addr_type = -1;
		struct in6_pktinfo pktinfo = {};

		struct iovec recv_iov;
		recv_iov.iov_len = sizeof(packet_space);
		recv_iov.iov_base = &packet_space;

		recvmsghdr.msg_iov = &recv_iov;
		recvmsghdr.msg_iovlen = 1;
		recvmsghdr.msg_control = &control_space;
		recvmsghdr.msg_controllen = sizeof(control_space);
		recvmsghdr.msg_flags = 0;

		retval = recvmsg(receive_fd, &recvmsghdr, 0);
		if (retval < 0) {
			break;
		}

		for (cmsg = CMSG_FIRSTHDR(&recvmsghdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&recvmsghdr, cmsg)) {
			if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
				pktinfo = *(struct in6_pktinfo *)CMSG_DATA(cmsg);
			}
			if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_RECV_LINK_ADDR_TYPE) {
				addr_type = *(int *)CMSG_DATA(cmsg);
			}
		}
		inet_ntop(AF_INET6, &pktinfo.ipi6_addr, addrstr, sizeof(addrstr));
		T_LOG("received packet to: %s address type: %d", addrstr, addr_type);

		if (IN6_IS_ADDR_MULTICAST(&pktinfo.ipi6_addr)) {
			T_ASSERT_EQ(addr_type, IP_RECV_LINK_ADDR_MULTICAST, "multicast");
		} else {
			T_ASSERT_EQ(addr_type, IP_RECV_LINK_ADDR_UNICAST, "unicast");
		}
	}
}
