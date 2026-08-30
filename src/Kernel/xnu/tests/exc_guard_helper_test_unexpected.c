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

/*
 * exc_guard_helper_test_unexpected.c
 *
 * Test the testing helper functions in exc_guard_helper.h.
 * The exception handler used by block_raise_exc_guard_of_type()
 * should allow other exceptions to continue to a crash.
 */

#include "test_utils.h"
#include "exc_guard_helper.h"

#include <darwintest.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/task_info.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vm"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true),

	T_META_IGNORECRASHES(".*exc_guard_helper_test_unexpected.*")
	);

T_DECL(exc_guard_helper_test_unexpected_exc_guard,
    "provoke one guard exception type while exc_guard_helper is expecting another")
{
	if (process_is_translated()) {
		T_SKIP("VM guard exceptions not supported on Rosetta (rdar://142438840)");
	}

	pid_t child_pid;

	if ((child_pid = fork())) {
		/* parent */
		T_QUIET; T_ASSERT_POSIX_SUCCESS(child_pid, "fork");

		int status;
		pid_t waited_pid;

		waited_pid = waitpid(child_pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(waited_pid, "waitpid");
		T_QUIET; T_ASSERT_EQ(waited_pid, child_pid, "waitpid");

		T_ASSERT_TRUE(WIFSIGNALED(status), "child should have crashed");
		T_ASSERT_EQ(WTERMSIG(status), SIGKILL, "child should have crashed with SIGKILL");
	} else {
		/* child */
		kern_return_t kr;
		task_exc_guard_behavior_t behavior;
		exc_guard_helper_info_t exc_info;
		mach_port_t port;

		exc_guard_helper_init();

		/*
		 * set GUARD_TYPE_MACH_PORT to be enabled and fatal.
		 * This child process is expected to crash.
		 */
		kr = task_get_exc_guard_behavior(mach_task_self(), &behavior);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "get old behavior");
		behavior &= ~TASK_EXC_GUARD_MP_ALL;
		behavior |= TASK_EXC_GUARD_MP_DELIVER | TASK_EXC_GUARD_MP_FATAL;
		kr = task_set_exc_guard_behavior(mach_task_self(), behavior);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "set fatal mach port behavior");

		kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "new port");
		kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "make send");

		/* provoke GUARD_TYPE_MACH_PORT while listening for GUARD_TYPE_VIRT_MEMORY */

		if (block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
			kern_return_t kr;
			T_LOG("CHILD EXPECTED TO CRASH after this guard exception");
			kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, INT32_MAX);
			T_QUIET; T_ASSERT_MACH_ERROR(kr, KERN_INVALID_VALUE, "add too many send rights");
		})) {
			T_FAIL("Mach port guard exception unexpectedly caught by VM guard exception handler");
		}

		T_FAIL("expected Mach port guard exception to kill the process");
	}
}
