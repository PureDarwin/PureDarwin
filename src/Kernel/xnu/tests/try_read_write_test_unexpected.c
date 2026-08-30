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
 * try_read_write_test_unexpected.c
 *
 * Test the testing helper functions in try_read_write.h.
 * The exception handler used by try_read_byte/try_write_byte
 * should allow other exceptions to continue to a crash.
 */

#include <stdint.h>
#include <stdbool.h>
#include <darwintest.h>
#include <sys/wait.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "try_read_write.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vm"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true),

	/* these tests are expected to crash */
	T_META_IGNORECRASHES(".*try_read_write_test_unexpected.*")
	);

static void
install_exception_handler(void)
{
	kern_return_t kr;
	bool result;

	result = try_write_byte(0, 0, &kr);
	T_QUIET; T_ASSERT_EQ(result, false, "try_write_byte to NULL");
	T_QUIET; T_ASSERT_EQ(kr, KERN_INVALID_ADDRESS, "try_write_byte to NULL");
}

static void
test_crasher(void (^crashing_block)(void))
{
	pid_t child_pid;
	if ((child_pid = fork())) {
		/* parent */
		int status;
		int err = waitpid(child_pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "waitpid");
		T_EXPECT_TRUE(WIFSIGNALED(status), "parent: child process should crash");
	} else {
		/* child */
		T_LOG("-- Calling try_write_byte() to install exception handlers --");
		install_exception_handler();
		T_LOG("-- The next exception should crash --");
		crashing_block();
		T_FAIL("child: process should have crashed");
	}
}

static void __attribute__((noinline))
THIS_IS_EXPECTED_TO_CRASH_EXC_BAD_ACCESS(void)
{
	*(volatile int *)0 = 1;
}

static void __attribute__((noinline))
THIS_IS_EXPECTED_TO_CRASH_BUILTIN_TRAP(void)
{
	__builtin_trap();
}


T_DECL(try_read_write_unexpected_bad_access,
    "test an unrelated EXC_BAD_ACCESS exception "
    "with the try_read_write exception handler in place")
{
	test_crasher(^{
		/*
		 * Provoke EXC_BAD_ACCESS outside try_read_byte and try_write_byte.
		 * The try_read_write exception handler should catch and rethrow it.
		 */
		THIS_IS_EXPECTED_TO_CRASH_EXC_BAD_ACCESS();
	});
}


T_DECL(try_read_write_unexpected_trap,
    "test an unrelated non-EXC_BAD_ACCESS exception "
    "with the try_read_write exception handler in place")
{
	test_crasher(^{
		/*
		 * Provoke a non-EXC_BAD_ACCESS exception outside of try_read_byte and try_write_byte.
		 * The try_read_write exception handler should not catch it.
		 */
		THIS_IS_EXPECTED_TO_CRASH_BUILTIN_TRAP();
	});
}
