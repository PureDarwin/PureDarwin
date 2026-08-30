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
 * net_siocdifaddr.c
 * - verify that SIOCDIFADDR succeeds
 */

#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <TargetConditionals.h>
#include <darwintest.h>
#include <darwintest_utils.h>

#include "net_test_lib.h"

T_GLOBAL_META(T_META_NAMESPACE("xnu.net"),
    T_META_RADAR_COMPONENT_NAME("xnu"),
    T_META_RADAR_COMPONENT_VERSION("networking"),
    T_META_ASROOT(true));

static char ifname[IF_NAMESIZE];

static void
fake_set_fail_ioctl(bool fail)
{
	int     error;
	int     val;

	val = fail ? 1 : 0;
#define FAKE_FAIL_IOCTL         "net.link.fake.fail_ioctl"
	error = sysctlbyname(FAKE_FAIL_IOCTL, NULL, 0,
	    &val, sizeof(val));
	T_ASSERT_EQ(error, 0, FAKE_FAIL_IOCTL " %d", val);
}

static void
test_cleanup(void)
{
	if (ifname[0] != '\0') {
		(void)ifnet_destroy(ifname, false);
		T_LOG("ifnet_destroy %s", ifname);
	}
	fake_set_fail_ioctl(false);
}

static void
sigint_cleanup(__unused int sig)
{
	signal(SIGINT, SIG_DFL);
	test_cleanup();
}

static void
test_siocdifaddr(void)
{
	struct in_addr  addr;
	int             error;
	struct in_addr  mask;

	addr.s_addr = htonl(IN_LINKLOCALNETNUM + 1);
	mask.s_addr = htonl(IN_CLASSB_NET);

	signal(SIGINT, sigint_cleanup);
	T_ATEND(test_cleanup);

	strlcpy(ifname, FETH_NAME, sizeof(ifname));
	error = ifnet_create_2(ifname, sizeof(ifname));
	if (error != 0) {
		ifname[0] = '\0';
		T_FAIL("ifnet_create_2 %s", FETH_NAME);
	}
	fake_set_fail_ioctl(true);
	ifnet_add_ip_address(ifname, addr, mask);
	ifnet_remove_ip_address(ifname, addr, mask);
}

T_DECL(siocdifaddr,
    "Verify SIOCDIFADDR succeeds when interface returns failure",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	test_siocdifaddr();
}
