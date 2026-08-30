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
#include <darwintest_perf.h>
#include <darwintest_utils.h>
#include <mach/error.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <stdbool.h>
#include <signal.h>
#include <spawn.h>
#include <spawn_private.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm.corpse"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_CHECK_LEAKS(false));

static pid_t
spawn_munch(size_t footprint)
{
	char **launch_tool_args;
	pid_t child_pid;
	int ret;

	char size_arg[64];

	T_LOG("Spawning munch with size %lu MiB", footprint >> 20);

	T_QUIET; T_ASSERT_POSIX_SUCCESS(
		snprintf(size_arg, sizeof(size_arg), "--lim-size=%lub", footprint),
		"snprintf()");

	launch_tool_args = (char *[]){
		"/usr/local/bin/munch",
		"--cfg-inprocess",
		"--fill-cr=2.5",
		"--type=malloc",
		size_arg,
		NULL
	};

	/* Spawn the child process. */
	ret = dt_launch_tool(&child_pid, launch_tool_args, false, NULL, NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "dt_launch_tool");
	T_QUIET; T_ASSERT_GT(child_pid, 0, "child pid");

	return child_pid;
}

static pid_t munch_pid = 0;

static void
perf_fork_corpse_teardown(void)
{
	int ret;
	bool exited;

	ret = kill(munch_pid, SIGINT);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kill()");

	exited = dt_waitpid(munch_pid, NULL, NULL, 30);
	T_QUIET; T_ASSERT_TRUE(exited, "dt_wait_pid()");
}

T_DECL(perf_fork_corpse,
    "Performance test for forking corpses",
    // T_META_ENABLED(!(TARGET_OS_WATCH || TARGET_OS_BRIDGE || TARGET_OS_TV)),
    T_META_ENABLED(false), /* rdar://148736982 */
    T_META_BOOTARGS_SET("amfi_unrestrict_task_for_pid=1"),
    T_META_TAG_PERF,
    T_META_TAG_VM_NOT_PREFERRED,
    T_META_RUN_CONCURRENTLY(false))
{
	size_t footprint = 512 << 20; // 512 MiB
	mach_port_t corpse_port;
	mach_port_t task_port;
	kern_return_t kr;

	pid_t pid = spawn_munch(footprint);

	T_ATEND(perf_fork_corpse_teardown);

	kr = task_for_pid(mach_task_self(), pid, &task_port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "task_for_pid()");
	T_QUIET; T_ASSERT_NE(task_port, MACH_PORT_NULL, "task_for_pid");

	dt_stat_time_t stat = dt_stat_time_create("duration");

	T_LOG("Collecting measurements...");
	while (!dt_stat_stable(stat)) {
		T_STAT_MEASURE(stat) {
			kr = task_generate_corpse(task_port, &corpse_port);
		}
		if (kr != KERN_SUCCESS) {
			T_SKIP("Unable to generate a corpse (%d | %s)", kr, mach_error_string(kr));
		}

		mach_port_deallocate(mach_task_self(), corpse_port);
	}
	dt_stat_finalize(stat);
}
