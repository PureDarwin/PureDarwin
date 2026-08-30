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

#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/socketvar.h>
#include <sys/unpcb.h>

#include <err.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <darwintest/darwintest.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.net"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("networking"),
	T_META_CHECK_LEAKS(false));

static void
do_test(int type)
{
	ssize_t retval;
	int sock[2];

	T_LOG("test socket type %d", type);

	T_ASSERT_POSIX_SUCCESS(socketpair(PF_LOCAL, type, 0, sock), "socketpair()");

	T_LOG("socketpair: [%d, %d]", sock[0], sock[1]);

	int optval = 1;
	T_ASSERT_POSIX_SUCCESS(setsockopt(sock[0], SOL_SOCKET, SO_DEBUG, &optval, sizeof(optval)), "setsockopt(SO_DEBUG)");

	struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
	T_ASSERT_POSIX_SUCCESS(setsockopt(sock[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)), "setsockopt(SO_RCVTIMEO)");

	struct iovec iov0 = { .iov_base = NULL, .iov_len = 0 };

	char cmsg_buf[CMSG_SPACE(sizeof(int))];
	struct msghdr msghdr1 = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov0,
		.msg_iovlen = 1,
		.msg_control = cmsg_buf,
		.msg_controllen = sizeof(cmsg_buf),
		.msg_flags = 0
	};

	struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msghdr1);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmsg)) = sock[0];

	retval = sendmsg(sock[1], &msghdr1, 0);
	if (retval == -1) {
		T_LOG("sendmsg(msghdr1) error: %s", strerror(errno));
	} else {
		T_LOG("sendmsg msghdr1 %ld", retval);
	}

	struct iovec iov1 = { .iov_base = NULL, .iov_len = 0 };
	struct msghdr msghdr2 = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov1,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0
	};

	retval = recvmsg(sock[0], &msghdr2, MSG_WAITALL);
	if (retval == -1) {
		T_LOG("recvmsg(msghdr2) error: %s", strerror(errno));
	} else {
		T_LOG("recvmsg msghdr2 %ld", retval);
	}

	char * buf[0x10] = { 0 };
	struct iovec iov2 = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};

	struct msghdr msghdr3 = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov2,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0
	};

	retval = recvmsg(sock[0], &msghdr3, MSG_WAITALL);
	if (retval == -1) {
		T_LOG("recvmsg(msghdr3) error: %s", strerror(errno));
	} else {
		T_LOG("recvmsg msghdr3 %ld", retval);
	}

	close(sock[0]);
	close(sock[1]);

	T_PASS("%s", __func__);
}

T_DECL(send_zero_payload_dgram, "repro-124040738 SOCK_DGRAM", T_META_ASROOT(true))
{
	do_test(SOCK_DGRAM);
}

T_DECL(send_zero_payload_stream, "repro-124040738 SOCK_STREAM", T_META_ASROOT(true))
{
	do_test(SOCK_STREAM);
}
