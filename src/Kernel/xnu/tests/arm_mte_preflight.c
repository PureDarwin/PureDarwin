/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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
#include <darwintest.h>
#include <signal.h>
#include <spawn_private.h>
#include <stdlib.h>
#include <sysexits.h>

#include "arm_mte_utilities.h"
#include "test_utils.h"

static void
do_preflight_spawn_test(
	char *binary_to_launch,
	bool expects_mte,
	bool expects_soft)
{
	char *expectation_arg = expects_mte ? EXPECT_MTE : DO_NOT_EXPECT_MTE;
	char *next_test_arg = MTE_ENABLEMENT_TEST_DONE_STR;
	char *argv[] = {
		binary_to_launch,
		expectation_arg,
		next_test_arg,
		NULL
	};
	pid_t child_pid = 0;
	int waitpid_result = 0;
	int status = 0;
	posix_spawn_secflag_options flags = (POSIX_SPAWN_SECFLAG_EXPLICIT_ENABLE |
	    POSIX_SPAWN_SECFLAG_EXPLICIT_PREFLIGHT);
	posix_spawnattr_t attr = NULL;

	/* Initialize spawnattr */
	errno_t ret = posix_spawnattr_init(&attr);
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_init");

	/* Request the process to be suspended upon starting */
	ret = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawnattr_setflags");

	/* Request soft-mode preflighting */
	ret = posix_spawnattr_set_use_sec_transition_shims_np(&attr, flags);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_set_use_sec_transition_shims_np");

	/* Spawn the process (suspended) */
	ret = posix_spawn(&child_pid, argv[0], NULL, &attr, argv, NULL);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn");

	/* Validate whether soft mode is enabled or not */
	validate_proc_pidinfo_mte_soft_mode_status(child_pid, expects_soft);

	ret = posix_spawnattr_destroy(&attr);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_destroy");

	/* Let the child continue */
	ret = kill(child_pid, SIGCONT);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "kill(SIGCONT)");

	/* Handle termination of the child */
	waitpid_result = waitpid(child_pid, &status, 0);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(waitpid_result, "waitpid");
	T_QUIET;
	T_ASSERT_EQ(WIFEXITED(status), 1, "child should have exited normally");
	T_QUIET;
	T_ASSERT_EQ(WEXITSTATUS(status), EX_OK, "child should have exited with success");
}

T_DECL(mte_preflight,
    "Test that we can enable preflighting on non-entitled binaries",
    T_META_REQUIRES_SYSCTL_EQ("hw.optional.arm.FEAT_MTE4", 1),
    XNU_T_META_SOC_SPECIFIC)
{
	do_preflight_spawn_test(SPAWN_HELPER_WITHOUT_ENTITLEMENT, true, true);
}

T_DECL(mte_preflight_no_downgrade,
    "Test that we cannot downgrade entitled binaries to soft mode",
    T_META_REQUIRES_SYSCTL_EQ("hw.optional.arm.FEAT_MTE4", 1),
    XNU_T_META_SOC_SPECIFIC)
{
	do_preflight_spawn_test(SPAWN_HELPER_WITH_ENTITLEMENT, true, false);
}
#endif /* __arm64__ */
