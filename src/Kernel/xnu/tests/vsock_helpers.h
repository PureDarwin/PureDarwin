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

#ifndef VSOCK_HELPERS_H
#define VSOCK_HELPERS_H

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/vsock_private.h>
#include <errno.h>

#include <darwintest.h>
#include <darwintest_utils.h>

#define COUNT_ELEMS(array) (sizeof (array) / sizeof (array[0]))

int
vsock_new_socket(void);

int
vsock_private_new_socket(void);

uint32_t
vsock_get_local_cid(int socket);

int
vsock_bind(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int *socket);

int
vsock_private_bind(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int *socket);

int
vsock_listen(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int backlog, int *socket);

int
vsock_private_listen(uint32_t cid, uint32_t port, struct sockaddr_vm * addr, int backlog, int *socket);

int
vsock_connect(uint32_t cid, uint32_t port, int *socket);

int
vsock_private_connect(uint32_t cid, uint32_t port, int *socket);

struct sockaddr_vm
vsock_getsockname(int socket);

void
vsock_close(int socket);

void
vsock_connect_peers(uint32_t cid, uint32_t port, int backlog, int *socketA, int *socketB);

void
vsock_send(int socket, char *msg);

void
vsock_disable_sigpipe(int socket);

bool
vsock_address_exists(struct xvsockpgen *buffer, struct sockaddr_vm addr);

uint32_t
vsock_get_available_port(void);

int
vsock_bind_family(sa_family_t family);

#endif /* VSOCK_HELPERS_H */
