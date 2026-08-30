/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#include <vsock_helpers.h>

static int
_vsock_new_socket(uint16_t protocol)
{
	int sock = socket(AF_VSOCK, SOCK_STREAM, protocol);
	if (sock < 0 && errno == ENODEV) {
		T_SKIP("no vsock transport available");
	}
	return sock;
}

int
vsock_new_socket(void)
{
	int sock = _vsock_new_socket(VSOCK_PROTO_STANDARD);
	T_ASSERT_GT(sock, 0, "create new vsock socket");
	return sock;
}

int
vsock_private_new_socket(void)
{
	int sock = _vsock_new_socket(VSOCK_PROTO_PRIVATE);
	T_ASSERT_GT(sock, 0, "create new private vsock socket");
	return sock;
}

uint32_t
vsock_get_local_cid(int socket)
{
	uint32_t cid = 0;
	int result = ioctl(socket, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &cid);
	T_ASSERT_POSIX_SUCCESS(result, "vsock ioctl cid successful");
	T_ASSERT_GT(cid, VMADDR_CID_HOST, "cid is set");
	T_ASSERT_NE(cid, VMADDR_CID_ANY, "cid is valid");

	return cid;
}

static int
_vsock_bind(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int *socket, uint16_t protocol)
{
	int sock = _vsock_new_socket(protocol);
	if (sock < 0) {
		return sock;
	}
	T_ASSERT_GT(sock, 0, "create new vsock socket");
	*socket = sock;

	bzero(addr, sizeof(*addr));
	addr->svm_port = port;
	addr->svm_cid = cid;

	return bind(*socket, (struct sockaddr *) addr, sizeof(*addr));
}

int
vsock_bind(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int *socket)
{
	return _vsock_bind(cid, port, addr, socket, VSOCK_PROTO_STANDARD);
}

int
vsock_private_bind(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int *socket)
{
	return _vsock_bind(cid, port, addr, socket, VSOCK_PROTO_PRIVATE);
}

int
vsock_listen(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int backlog, int *socket)
{
	int result = vsock_bind(cid, port, addr, socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock bind");
	return listen(*socket, backlog);
}

int
vsock_private_listen(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int backlog, int *socket)
{
	int result = vsock_private_bind(cid, port, addr, socket);
	T_ASSERT_POSIX_SUCCESS(result, "private vsock bind");
	return listen(*socket, backlog);
}

static int
_vsock_connect(uint32_t cid, uint32_t port, int *socket, uint16_t protocol)
{
	int sock = _vsock_new_socket(protocol);
	if (sock < 0) {
		return sock;
	}
	T_ASSERT_GT(sock, 0, "create new vsock socket");
	*socket = sock;

	struct sockaddr_vm addr = (struct sockaddr_vm) {
		.svm_cid = cid,
		.svm_port = port,
	};
	return connect(*socket, (struct sockaddr *)&addr, sizeof(addr));
}

int
vsock_connect(uint32_t cid, uint32_t port, int *socket)
{
	return _vsock_connect(cid, port, socket, VSOCK_PROTO_STANDARD);
}

int
vsock_private_connect(uint32_t cid, uint32_t port, int *socket)
{
	return _vsock_connect(cid, port, socket, VSOCK_PROTO_PRIVATE);
}

struct sockaddr_vm
vsock_getsockname(int socket)
{
	struct sockaddr_vm addr;
	socklen_t length = sizeof(addr);
	int result = getsockname(socket, (struct sockaddr *)&addr, &length);
	T_ASSERT_POSIX_SUCCESS(result, "vsock getsockname");
	T_ASSERT_EQ_INT((int) sizeof(addr), length, "correct address length");
	T_ASSERT_GT(addr.svm_port, 0, "bound to non-zero local port");
	return addr;
}

void
vsock_close(int socket)
{
	int result = close(socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock close");
}

void
vsock_connect_peers(uint32_t cid, uint32_t port, int backlog, int *socketA, int *socketB)
{
	// Listen.
	struct sockaddr_vm addr;
	int listen_socket;
	int result = vsock_listen(cid, port, &addr, backlog, &listen_socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock listen");

	const uint32_t connection_cid = vsock_get_local_cid(listen_socket);

	// Connect.
	int connect_socket;
	result = vsock_connect(connection_cid, addr.svm_port, &connect_socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock connect");

	// Accept.
	struct sockaddr_vm accepted_addr;
	socklen_t addrlen = sizeof(accepted_addr);
	int accepted_socket = accept(listen_socket, (struct sockaddr *)&accepted_addr, &addrlen);
	T_ASSERT_GT(accepted_socket, 0, "accepted socket");
	T_ASSERT_EQ_INT((int) sizeof(accepted_addr), addrlen, "correct address length");
	T_ASSERT_EQ_INT(connection_cid, accepted_addr.svm_cid, "same cid");
	T_ASSERT_NE_INT(VMADDR_CID_ANY, accepted_addr.svm_port, "some valid port");
	T_ASSERT_NE_INT(0, accepted_addr.svm_port, "some non-zero port");

	*socketA = connect_socket;
	*socketB = accepted_socket;
}

void
vsock_send(int socket, char *msg)
{
	T_ASSERT_NOTNULL(msg, "send message is not null");
	ssize_t sent_bytes = send(socket, msg, strlen(msg), 0);
	T_ASSERT_EQ_LONG(strlen(msg), (unsigned long)sent_bytes, "sent all bytes");
}

void
vsock_disable_sigpipe(int socket)
{
	int on = 1;
	int result = setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
	T_ASSERT_POSIX_SUCCESS(result, "vsock disable SIGPIPE");
}

bool
vsock_address_exists(struct xvsockpgen *buffer, struct sockaddr_vm addr)
{
	struct xvsockpgen *xvg = buffer;
	struct xvsockpgen *oxvg = buffer;

	bool found = false;
	for (xvg = (struct xvsockpgen *)((char *)xvg + xvg->xvg_len);
	    xvg->xvg_len > sizeof(struct xvsockpgen);
	    xvg = (struct xvsockpgen *)((char *)xvg + xvg->xvg_len)) {
		struct xvsockpcb *xpcb = (struct xvsockpcb *)xvg;

		/* Ignore PCBs which were freed during copyout. */
		if (xpcb->xvp_gencnt > oxvg->xvg_gen) {
			continue;
		}

		if (xpcb->xvp_local_cid == addr.svm_cid && xpcb->xvp_remote_cid == VMADDR_CID_ANY &&
		    xpcb->xvp_local_port == addr.svm_port && xpcb->xvp_remote_port == VMADDR_PORT_ANY) {
			found = true;
			break;
		}
	}

	T_ASSERT_NE(xvg, oxvg, "first and last xvsockpgen were returned");

	return found;
}

uint32_t
vsock_get_available_port(void)
{
	int socket;
	struct sockaddr_vm addr;
	int result = vsock_bind(VMADDR_CID_ANY, VMADDR_PORT_ANY, &addr, &socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock bind to any port");

	const struct sockaddr_vm bound_addr = vsock_getsockname(socket);
	const uint32_t port = bound_addr.svm_port;
	T_ASSERT_NE_INT(port, VMADDR_PORT_ANY, "port is specified");

	vsock_close(socket);

	return port;
}

int
vsock_bind_family(sa_family_t family)
{
	int socket = vsock_new_socket();
	const uint32_t port = vsock_get_available_port();

	struct sockaddr_vm addr = (struct sockaddr_vm) {
		.svm_family = family,
		.svm_cid = VMADDR_CID_ANY,
		.svm_port = port,
	};

	return bind(socket, (struct sockaddr *) &addr, sizeof(addr));
}
