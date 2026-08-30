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

#if __arm64__
#include <arm_acle.h>
#include <darwintest.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "arm_mte_utilities.h"
#include "test_utils.h"

/*
 * This binary is code signed with the signing ID com.apple.internal.arm_mte_soft_mode_test
 * and includes ptrace entitlements.
 * On internal builds, AMFI contains this ID on the MTE soft mode list.
 *
 * Test that soft mode is disabled when a process becomes traced (rdar://156025403).
 */
T_DECL(mte_soft_mode_disabled_when_trace_me,
    "Test that soft mode is disabled when process is traced",
    T_META_REQUIRES_SYSCTL_EQ("hw.optional.arm.FEAT_MTE2", 1),
    XNU_T_META_REQUIRES_DEVELOPMENT_KERNEL,
    XNU_T_META_SOC_SPECIFIC,
    T_META_ASROOT(true),
    T_META_ENABLED(false) /* rdar://142784868 */)
{
	int ret;

	pid_t child_pid = fork();
	T_ASSERT_NE(child_pid, -1, "fork");

	if (child_pid == 0) {
		/* Child */
		validate_proc_pidinfo_mte_soft_mode_status(getpid(), true);
		ret = ptrace(PT_TRACE_ME, 0, NULL, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "ptrace(PT_TRACE_ME)");
		validate_proc_pidinfo_mte_soft_mode_status(getpid(), false);

		/* Allocate tagged memory */
		vm_size_t alloc_size = 16 * 1024;
		vm_address_t address = allocate_and_tag_range(alloc_size, TAG_RANDOM);

		T_LOG("Child %d triggering MTE fault (should crash in hard mode)", getpid());
		char *incorrectly_tagged_ptr = __arm_mte_increment_tag((char *)address, 1);
		*incorrectly_tagged_ptr = 'X';

		T_FAIL("Survived TCF -- soft mode didn't work");
		exit(1);
	} else {
		/* Parent */
		int status = 0;
		pid_t waited_pid = waitpid(child_pid, &status, 0);
		T_ASSERT_EQ(waited_pid, child_pid, "waitpid()");

		/* Since we're ptrace'd, the process should be stopped. */
		T_ASSERT_TRUE(WIFSTOPPED(status), "WIFSTOPPED(status)");

		int sig = WSTOPSIG(status);
		T_LOG("Child stopped with %s (%d)", strsignal(sig), sig);
		T_ASSERT_EQ(sig, SIGBUS, "Child stopped with SIGBUS");
	}
}

/*
 * Test that soft mode is disabled when a process is attached via PT_ATTACH (rdar://156025403).
 */
T_DECL(mte_soft_mode_disabled_when_attached,
    "Test that soft mode is disabled when process is attached via PT_ATTACH",
    T_META_REQUIRES_SYSCTL_EQ("hw.optional.arm.FEAT_MTE2", 1),
    XNU_T_META_REQUIRES_DEVELOPMENT_KERNEL,
    XNU_T_META_SOC_SPECIFIC,
    T_META_ASROOT(true),
    T_META_ENABLED(false) /* rdar://142784868 */)
{
	int ret;

	pid_t child_pid = fork();
	T_ASSERT_NE(child_pid, -1, "fork");

	if (child_pid == 0) {
		/* Child */
		validate_proc_pidinfo_mte_soft_mode_status(getpid(), true);
		T_LOG("Child %d sleeping to allow attach", getpid());
		sleep(5); /* Hack: give the parent a moment to attach */
		validate_proc_pidinfo_mte_soft_mode_status(getpid(), false);

		/* Allocate tagged memory */
		vm_size_t alloc_size = 16 * 1024;
		vm_address_t address = allocate_and_tag_range(alloc_size, TAG_RANDOM);

		T_LOG("Child %d triggering MTE fault (should crash in hard mode)", getpid());
		char *incorrectly_tagged_ptr = __arm_mte_increment_tag((char *)address, 1);
		*incorrectly_tagged_ptr = 'X';

		T_FAIL("Survived TCF -- soft mode didn't work");
		exit(1);
	} else {
		/* Parent */
		int status = 0;

		/* Hack: give the child a moment to come up */
		usleep(100000);

		/* Attach to the child */
		T_LOG("Parent attaching to child %d", child_pid);
		ret = ptrace(PT_ATTACH, child_pid, NULL, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "ptrace(PT_ATTACH)");

		pid_t waited_pid = waitpid(child_pid, &status, 0);
		T_ASSERT_EQ(waited_pid, child_pid, "waitpid() for attach");
		T_ASSERT_TRUE(WIFSTOPPED(status), "Child stopped after attach");

		/* Continue the child so it can check soft mode status and trigger the fault */
		T_LOG("Parent continuing child");
		ret = ptrace(PT_CONTINUE, child_pid, (caddr_t)1, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "ptrace(PT_CONTINUE)");

		/* Wait for the child to crash */
		waited_pid = waitpid(child_pid, &status, 0);
		T_ASSERT_EQ(waited_pid, child_pid, "waitpid() for crash");
		T_ASSERT_TRUE(WIFSTOPPED(status), "WIFSTOPPED(status)");

		int sig = WSTOPSIG(status);
		T_LOG("Child stopped with %s (%d)", strsignal(sig), sig);
		T_ASSERT_EQ(sig, SIGBUS, "Child stopped with SIGBUS");
	}
}
#endif /* __arm64__ */
