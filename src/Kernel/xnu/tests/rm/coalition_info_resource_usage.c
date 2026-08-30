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
#include <darwintest.h>
#include <darwintest_utils.h>
#include <mach-o/dyld.h>
#include <spawn_private.h>
#include <sys/coalition.h>
#include <sys/kauth.h>
#include <sys/param.h>
#include <sys/spawn_internal.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/unistd.h>

T_GLOBAL_META(T_META_NAMESPACE("xnu.rm"),
    T_META_RADAR_COMPONENT_NAME("xnu"),
    T_META_RADAR_COMPONENT_VERSION("rm"),
    T_META_OWNER("m_staveleytaylor"));

static uint64_t
create_coalition(int type)
{
	uint64_t id = 0;
	uint32_t flags = 0;
	uint64_t param[2];
	int ret;

	COALITION_CREATE_FLAGS_SET_TYPE(flags, type);
	ret = coalition_create(&id, flags);
	T_ASSERT_POSIX_SUCCESS(ret, "coalition_create");
	T_QUIET;
	T_ASSERT_GE(id, 0ULL, "coalition_create returned a valid id");

	T_LOG("coalition has id %lld\n", id);

	/* disable notifications for this coalition so launchd doesn't freak out */
	param[0] = id;
	param[1] = 0;
	ret = sysctlbyname("kern.coalition_notify", NULL, NULL, param, sizeof(param));
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "kern.coalition_notify");

	return id;
}

static pid_t
spawn_helper_in_coalition(char *helper_name, uint64_t coal_id)
{
	int ret;
	posix_spawnattr_t attr;
	extern char **environ;
	pid_t new_pid = 0;
	char path[PATH_MAX];
	uint32_t path_size = sizeof(path);

	T_QUIET;
	T_ASSERT_POSIX_ZERO(_NSGetExecutablePath(path, &path_size),
	    "_NSGetExecutablePath");
	char *args[] = {path, "-n", helper_name, NULL};

	ret = posix_spawnattr_init(&attr);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_init");

	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_setcoalition_np");
	ret = posix_spawnattr_setcoalition_np(&attr, coal_id,
	    COALITION_TYPE_RESOURCE,
	    COALITION_TASKROLE_LEADER);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_setcoalition_np");

	T_LOG("posix_spawn %s %s %s", args[0], args[1], args[2]);
	ret = posix_spawn(&new_pid, path, NULL, &attr, args, environ);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawn");

	ret = posix_spawnattr_destroy(&attr);
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_destroy");
	return new_pid;
}

T_HELPER_DECL(qos_expense, "qos_expense")
{
	mach_timebase_info_data_t tb_info;
	mach_timebase_info(&tb_info);

	T_LOG("starting busy work in child");

	uint64_t start_ns = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);

	/* Do 500ms of busy work to pad our QoS stats */
	while (true) {
		uint64_t now_ns = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
		uint64_t diff_ms = (now_ns - start_ns) / (1000ULL * 1000ULL);
		if (diff_ms > 500) {
			break;
		}
	}

	T_PASS("finished busy work in child");
}

static uint64_t
get_qos_sum(uint64_t coalition_id)
{
	struct coalition_resource_usage cru;
	int ret = coalition_info_resource_usage(coalition_id, &cru, sizeof(cru));
	T_ASSERT_POSIX_SUCCESS(ret, "coalition_info_resource_usage");

	uint64_t sum = 0;
	for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; i++) {
		sum += cru.cpu_time_eqos[i];
	}
	return sum;
}

static uint64_t coalition_id;

static void
terminate_and_reap_coalition(void)
{
	T_LOG("coalition_terminate"); coalition_terminate(coalition_id, 0);
	T_LOG("coalition_reap"); coalition_reap(coalition_id, 0);
}

T_DECL(coalition_info_resource_usage_qos_monotonic,
    "Make sure CPU time QoS values are accumulated from dead tasks",
    T_META_ASROOT(true),
    T_META_SYSCTL_INT("kern.unrestrict_coalitions=1"),
    T_META_TAG_VM_PREFERRED)
{
	T_SETUPBEGIN;
	coalition_id = create_coalition(COALITION_TYPE_RESOURCE);
	T_ATEND(terminate_and_reap_coalition);
	T_SETUPEND;

	T_ASSERT_EQ_ULLONG(get_qos_sum(coalition_id), 0ULL, "cpu_time_eqos == 0");

	pid_t child_pid = spawn_helper_in_coalition("qos_expense", coalition_id);

	T_LOG("waitpid(%d)\n", child_pid);
	int stat;
	int ret = waitpid(child_pid, &stat, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "waitpid");
	T_QUIET; T_ASSERT_TRUE(WIFEXITED(stat), "child exited.");
	T_QUIET; T_ASSERT_EQ(WEXITSTATUS(stat), 0, "child exited cleanly.");

	T_ASSERT_GT_ULLONG(get_qos_sum(coalition_id), 0ULL, "cpu_time_eqos > 0");
}
