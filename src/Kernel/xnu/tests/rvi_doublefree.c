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

/*
 * rvi_doublefree.c
 *
 * Test for mbuf_pullup() double-free vulnerability in
 * rvi_ctl_send -> rvi_bpf_tap
 *
 * This test verifies that the Remote VIF (RVI) control interface
 * properly handles malformed pktap headers without triggering
 * a double-free condition.
 */

#include <darwintest.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/bpf.h>
#include <pcap/dlt.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.net"),
	T_META_ASROOT(true),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("networking"),
	T_META_CHECK_LEAKS(false),
	T_META_ENABLED(false)
	);

/* ------ minimal pktap header -------- */
struct pktap_header {
	uint32_t pth_length;
	uint32_t pth_type_next;
	uint32_t pth_dlt;
	char     pth_ifname[64];
	uint32_t pth_flags;
	uint32_t pth_protocol_family;
	uint32_t pth_frame_pre_length;
	uint32_t pth_frame_post_length;
};
/* -------------------------------------------------------------- */

/* - Remote-VIF control constants (snippets from remote_vif.h) -- */
#define RVI_CTL_NAME                "com.apple.net.rvi_control"
#define RVI_COMMAND_GET_INTERFACE   0x20    /* getsockopt -> rviN */

static int
open_rvi_socket(void)
{
	/* open PF_SYSTEM / SYSPROTO_CONTROL and auto-create a fresh rviN */
	int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	T_ASSERT_POSIX_SUCCESS(fd, "socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL)");

	struct ctl_info ci = {0};
	strlcpy(ci.ctl_name, RVI_CTL_NAME, sizeof(ci.ctl_name));
	T_ASSERT_POSIX_SUCCESS(ioctl(fd, CTLIOCGINFO, &ci), "ioctl(CTLIOCGINFO)");

	struct sockaddr_ctl sc = {
		.sc_len     = sizeof(sc),
		.sc_family  = AF_SYSTEM,
		.ss_sysaddr = AF_SYS_CONTROL,
		.sc_id      = ci.ctl_id,
		.sc_unit    = 0                    /* allocate new rviN */
	};
	T_ASSERT_POSIX_SUCCESS(connect(fd, (struct sockaddr *)&sc, sizeof(sc)),
	    "connect to RVI control socket");

	return fd;
}

static void
get_rvi_ifname(int kctl_fd, char ifname[IFNAMSIZ])
{
	/* find out what interface was created (rviN) */
	socklen_t len = IFNAMSIZ;
	T_ASSERT_POSIX_SUCCESS(getsockopt(kctl_fd, SYSPROTO_CONTROL,
	    RVI_COMMAND_GET_INTERFACE, ifname, &len),
	    "getsockopt(RVI_COMMAND_GET_INTERFACE)");
}

static int
bump_raw_count(const char *ifname)
{
	/* open /dev/bpf, attach DLT_RAW -> _raw_count++ in rvi_set_bpf_tap */
	int bpf_fd = open("/dev/bpf1", O_RDONLY);
	T_ASSERT_POSIX_SUCCESS(bpf_fd, "open(/dev/bpf1)");

	T_ASSERT_POSIX_SUCCESS(ioctl(bpf_fd, BIOCSETIF, (void *)ifname),
	    "ioctl(BIOCSETIF, %s)", ifname);

	u_int dlt = DLT_RAW;
	T_ASSERT_POSIX_SUCCESS(ioctl(bpf_fd, BIOCSDLT, &dlt),
	    "ioctl(BIOCSDLT, DLT_RAW)");

	/*
	 * CRITICAL: Do NOT close the BPF file descriptor here.
	 * The vulnerability requires the BPF descriptor to remain open
	 * to maintain _raw_count > 0 for the rvi_bpf_tap code path.
	 */
	return bpf_fd;  /* Return FD for cleanup later */
}

T_DECL(rvi_doublefree_test,
    "Test RVI control interface resilience against malformed pktap headers",
    T_META_ASROOT(true))
{
	char ifname[IFNAMSIZ] = {0};
	int rvifd = -1;
	int bpf_fd = -1;

	rvifd = open_rvi_socket();
	get_rvi_ifname(rvifd, ifname);
	T_LOG("Created RVI interface: %s", ifname);

	/* Increase _raw_count to trigger rvi_bpf_tap code path */
	/* CRITICAL: Keep BPF FD open to maintain vulnerability conditions */
	bpf_fd = bump_raw_count(ifname);

	/* ------------------- craft the malicious mbuf --------------------- */
	size_t hdr_len = 256;                     /* > sizeof(pktap_header)   */
	char *hdr = calloc(1, hdr_len);           /* zero-initialised         */
	T_ASSERT_NOTNULL(hdr, "allocate pktap header buffer");

	/* set pth_protocol_family (offset 40) = AF_INET                      */
	*(uint32_t *)(hdr + 40) = AF_INET;

	/* set pth_frame_pre_length (offset 44) = huge value -> pullup fails  */
	*(uint32_t *)(hdr + 44) = 0x10000000;

	const char payload[] = "A";               /* 1-byte dummy "IP packet" */
	size_t plen = hdr_len + sizeof(payload);

	char *buf = malloc(plen);
	T_ASSERT_NOTNULL(buf, "allocate send buffer");

	memcpy(buf, hdr, hdr_len);
	memcpy(buf + hdr_len, payload, sizeof(payload));

	/*
	 * This send operation should not cause a kernel panic or double-free.
	 * The kernel should gracefully handle the malformed pktap header.
	 */
	ssize_t sent = send(rvifd, buf, plen, 0);

	/*
	 * We expect this to either succeed (if the vulnerability is fixed)
	 * or fail gracefully (without a panic). The key is that we shouldn't
	 * crash the system.
	 */
	if (sent < 0) {
		T_LOG("send() failed as expected: %s", strerror(errno));
	} else {
		T_LOG("send() succeeded, sent %zd bytes", sent);
	}

	/* If we reach this point, the kernel didn't panic - test passes */
	T_PASS("RVI interface handled malformed pktap header without crashing");

	/* Clean up resources */
	free(hdr);
	free(buf);
	if (bpf_fd >= 0) {
		close(bpf_fd);
	}
	if (rvifd >= 0) {
		close(rvifd);
	}
}
