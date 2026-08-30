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

T_GLOBAL_META(
	T_META_RUN_CONCURRENTLY(true),
	T_META_NAMESPACE("xnu.vsock")
	);

T_DECL(vsock_private_connect_with_entitlement, "vsock private connect should succeed with entitlement")
{
	const uint32_t port = 1234;

	struct sockaddr_vm listen_addr;
	int listen_socket;
	int result = vsock_private_listen(VMADDR_CID_ANY, port, &listen_addr, 1, &listen_socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock listen with entitlement");

	const uint32_t connection_cid = vsock_get_local_cid(listen_socket);

	int connected_socket = vsock_private_new_socket();
	struct sockaddr_vm addr = (struct sockaddr_vm) {
		.svm_cid = connection_cid,
		.svm_port = port,
	};
	result = connect(connected_socket, (struct sockaddr *)&addr, sizeof(addr));
	T_ASSERT_POSIX_SUCCESS(result, "vsock connection  with entitlement");

	vsock_close(connected_socket);
	vsock_close(listen_socket);
}

T_DECL(vsock_private_bind_with_entitlement, "vsock private bind should succeed with entitlement")
{
	int socket;
	struct sockaddr_vm addr;
	int result = vsock_private_bind(VMADDR_CID_ANY, 1234, &addr, &socket);
	T_ASSERT_POSIX_SUCCESS(result, "vsock bind with entitlement");
}
