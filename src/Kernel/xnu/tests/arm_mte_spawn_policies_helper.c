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

#include <darwintest.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "arm_mte_utilities.h"

/*
 * The goal of this helper is to help ensure that MTE enablement
 * rules work correctly in face of entitlements and spawn policies.
 * We build several copies of this same file, one for each case that we
 * currently support w.r.t. the hardened-process entitlement and the AMFI opt-out
 * list.
 */
int
main(int argc, char **argv)
{
	/* Let's start by validating our own state. */
	bool should_expect_mte = (strcmp(argv[1], "YES") == 0);
	assert(validate_proc_pidinfo_mte_status(getpid(), should_expect_mte));

	/* Extract the operation we are supposed to perform. */
	int test_to_perform = (int)argv[2][0];

	/* If we are the last process in the tree, just bail out. */
	if (test_to_perform == MTE_ENABLEMENT_TEST_DONE) {
		return 0;
	}
	/* We need to execute again, argv[3] contains the expectation. */
	char *next_test_should_expect_mte = argv[3];

	char *next_test_path;

	switch (test_to_perform) {
	case MTE_ENABLEMENT_TEST_HARDENED_PROCESS:
		next_test_path = SPAWN_HELPER_WITH_ENTITLEMENT;
		break;
	case MTE_ENABLEMENT_TEST_VANILLA_PROCESS:
		next_test_path = SPAWN_HELPER_WITHOUT_ENTITLEMENT;
		break;
	case MTE_ENABLEMENT_TEST_OPTED_OUT_PROCESS:
		next_test_path = HARDENED_PROCESS_TOP_LEVEL_ONLY_AND_IN_AMFI_MTE_OPT_OUT_HELPER;
		break;
	default:
		T_FAIL("Unexpected MTE enablement operation passed");
		return 1;
	}

	/* We never recurse more than once as a two level dependency already exercises all our paths. */
	char *next_test_to_perform = MTE_ENABLEMENT_TEST_DONE_STR;

	/* Create the next set of arguments. */
	char *next_test_argv[] = {
		next_test_path,
		next_test_should_expect_mte,
		next_test_to_perform,
		NULL, /* Change this if we ever need to recurse more than once. */
	};

	/*
	 * Rules are identical for both fork()/exec() and posix_spawn() with no extra flags.
	 */
	T_ASSERT_TRUE(fork_and_exec_new_process(next_test_argv), "fork/exec matches expectations");
	T_ASSERT_TRUE(posix_spawn_then_perform_action_from_process(next_test_argv, MTE_SPAWN_USE_VANILLA, 0), "posix_spawn matches expectations");

	return 0;
}
