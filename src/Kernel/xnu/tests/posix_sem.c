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
#include <semaphore.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/posix_sem.h>
#include <sys/code_signing.h>
#include <mach-o/dyld.h>


T_GLOBAL_META(
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("bsd"),
	T_META_OWNER("m_staveleytaylor"),
	T_META_RUN_CONCURRENTLY(true));

#define NUM_TEST_SEMAPHORES 50

static char open_test_prefix[PSEMNAMLEN + 1];
static char open_sem_invalid[PSEMNAMLEN + 1];
static char open_sem_a[PSEMNAMLEN + 1];
static char open_sem_b[PSEMNAMLEN + 1];

static void
cleanup_open()
{
	sem_unlink(open_sem_invalid);
	sem_unlink(open_sem_a);
	sem_unlink(open_sem_b);

	for (int i = 0; i < NUM_TEST_SEMAPHORES; i++) {
		char name_buf[PSEMNAMLEN];
		snprintf(name_buf, sizeof(name_buf), "%s/many%d", open_test_prefix, i);
		sem_unlink(name_buf);
	}
}

T_DECL(posix_sem_open, "POSIX sem_open",
    T_META_TAG_VM_PREFERRED)
{
	sem_t *sem;

	T_SETUPBEGIN;
	srand(time(NULL));
	snprintf(open_test_prefix, sizeof(open_test_prefix), "xnutest%d", rand() % 10000);
	snprintf(open_sem_invalid, sizeof(open_sem_invalid), "%s/invalid", open_test_prefix);
	snprintf(open_sem_a, sizeof(open_sem_a), "%s/a", open_test_prefix);
	snprintf(open_sem_b, sizeof(open_sem_b), "%s/b", open_test_prefix);

	T_ATEND(cleanup_open);
	T_SETUPEND;

	sem = sem_open(open_sem_invalid, 0);
	T_EXPECT_EQ_PTR(sem, SEM_FAILED, "sem_open without O_CREAT fails");
	T_EXPECT_EQ(errno, ENOENT, "sem_open without O_CREAT gives ENOENT");

	sem = sem_open(open_sem_a, O_CREAT, 0755, 0);
	T_WITH_ERRNO;
	T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open(O_CREAT) succeeds");

	sem = sem_open(open_sem_a, O_CREAT, 0755, 0);
	T_WITH_ERRNO;
	T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open(O_CREAT) on existing succeeds");

	sem = sem_open(open_sem_a, O_CREAT | O_EXCL, 0755, 0);
	T_EXPECT_EQ_PTR(sem, SEM_FAILED, "sem_open(O_CREAT | O_EXCL) on existing fails");
	T_EXPECT_EQ(errno, EEXIST, "sem_open(O_CREAT | O_EXCL) on existing gives EEXIST");

	sem = sem_open(open_sem_b, O_CREAT | O_EXCL, 0755, 0);
	T_WITH_ERRNO;
	T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open(O_CREAT | O_EXCL) on non-existing succeeds");

	for (int i = 0; i < NUM_TEST_SEMAPHORES; i++) {
		char name_buf[PSEMNAMLEN];
		snprintf(name_buf, sizeof(name_buf), "%s/many%d", open_test_prefix, i);

		int oflag = O_CREAT;
		if (rand() % 2 == 0) {
			oflag |= O_EXCL;
		}

		sem = sem_open(name_buf, oflag, 0755, 0);
		T_WITH_ERRNO;
		T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open name=%s oflag=%d succeeds", name_buf, oflag);
	}

	/* Fisher-Yates shuffle to randomize order in which we unlink semaphores */
	int unlink_order[NUM_TEST_SEMAPHORES] = { 0 };
	for (int i = 0; i < NUM_TEST_SEMAPHORES; i++) {
		unlink_order[i] = i;
	}
	for (int i = 0; i < NUM_TEST_SEMAPHORES; i++) {
		int next_index = rand() % (NUM_TEST_SEMAPHORES - i);

		int semaphore = unlink_order[i + next_index];
		unlink_order[i + next_index] = unlink_order[i];

		char name_buf[PSEMNAMLEN + 1];
		snprintf(name_buf, sizeof(name_buf), "%s/many%d", open_test_prefix, semaphore);

		T_WITH_ERRNO;
		T_EXPECT_POSIX_SUCCESS(sem_unlink(name_buf), "sem_unlink(%s)", name_buf);
	}
}

static char namespace_test_sem_name[PSEMNAMLEN + 1];

static int
find_helper(char* test_path, int team_id)
{
	char binpath[MAXPATHLEN];
	char* dirpath;
	uint32_t size = sizeof(binpath);
	int retval;

	retval = _NSGetExecutablePath(binpath, &size);
	assert(retval == 0);
	dirpath = dirname(binpath);

	snprintf(test_path, MAXPATHLEN, "%s/posix_sem_namespace_helper_team%d", dirpath, team_id);
	if (access(test_path, F_OK) == 0) {
		return 0;
	} else {
		return -1;
	}
}

static void
do_namespace_op(const char *namespace, const char *op)
{
	int ret, exit_status, signum;

	dt_pipe_data_handler_t stdout_handler = ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("%s: %s", (char *)context->user_context, data);
		return false;
	};
	dt_pipe_data_handler_t stderr_handler = ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("%s (stderr): %s", (char *)context->user_context, data);
		return false;
	};

	pid_t pid = dt_launch_tool_pipe((char *[]){ (char *)namespace, namespace_test_sem_name, (char *)op, NULL}, false, NULL, stdout_handler, stderr_handler, BUFFER_PATTERN_LINE, (void *)namespace);

	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(pid, "dt_launch_tool_pipe %s (%s) - %s", op, namespace, namespace_test_sem_name);

	ret = dt_waitpid(pid, &exit_status, &signum, 60 * 5);
	T_QUIET; T_ASSERT_EQ(ret, 1, "dt_waitpid (exit=%d,signum=%d)", exit_status, signum);
	T_QUIET; T_ASSERT_EQ(exit_status, 0, "dt_waitpid: exit_status");
	T_QUIET; T_ASSERT_EQ(signum, 0, "dt_waitpid: signum");
}

static void
cleanup_namespace()
{
	sem_unlink(namespace_test_sem_name);
}

/*
 * Unfortunately this test suffers from two issues that mean we must leave it disabled on BATS:
 *   1. rdar://75835929 means that XBS strips the team ID from our helper binaries after we've signed them.
 *   2. BATS infrastructure boots with amfi_get_out_of_my_way=1, which treats signatures as CS_PLATFORM_BINARY and causes the team ID to be ignored.
 */
T_DECL(posix_sem_open_team_id_namespace, "POSIX sem_open team ID namespace",
    T_META_BOOTARGS_SET("amfi_allow_any_signature=1"),
    T_META_ENABLED(FALSE),
    T_META_TAG_VM_PREFERRED)
{
	T_SETUPBEGIN;
	srand(time(NULL));
	snprintf(namespace_test_sem_name, sizeof(namespace_test_sem_name), "xnutest%d/ns", rand() % 10000);

	T_ATEND(cleanup_namespace);

	char team0_helper[MAXPATHLEN], team1_helper[MAXPATHLEN];
	find_helper(team0_helper, 0);
	find_helper(team1_helper, 1);
	printf("found helpers at '%s' and '%s'\n", team0_helper, team1_helper);

	/* Quite difficult to register cleanup handlers for this, so we'll perform cleanup now */
	T_LOG("Performing sem_unlink cleanup");
	do_namespace_op(team0_helper, "unlink_force");
	do_namespace_op(team1_helper, "unlink_force");

	T_SETUPEND;

	/* Check that semaphores created by 1st party applications can be discovered by 3rd party applications. */
	T_LOG("Check 3rd party sees 1st party");

	sem_t *sem = sem_open(namespace_test_sem_name, O_CREAT | O_EXCL, 0755, 0);
	T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open(O_CREAT | O_EXCL)");
	sem_close(sem);

	do_namespace_op(team0_helper, "check_access");
	T_ASSERT_POSIX_SUCCESS(sem_unlink(namespace_test_sem_name), "sem_unlink");
	do_namespace_op(team0_helper, "check_no_access");

#if TARGET_OS_OSX
	T_LOG("macOS only: check 1st party sees 3rd party");
	do_namespace_op(team0_helper, "open_excl");
	do_namespace_op(team0_helper, "check_access");

	sem = sem_open(namespace_test_sem_name, 0);
	T_EXPECT_NE_PTR(sem, SEM_FAILED, "sem_open on 3rd party semaphore");
	sem_close(sem);

	do_namespace_op(team0_helper, "unlink");

	T_LOG("macOS only: check 3rd party sees other 3rd party" );
	do_namespace_op(team0_helper, "check_no_access");
	do_namespace_op(team1_helper, "check_no_access");

	do_namespace_op(team0_helper, "open_excl");
	do_namespace_op(team0_helper, "check_access");
	do_namespace_op(team1_helper, "check_access");

	do_namespace_op(team1_helper, "unlink");
	do_namespace_op(team0_helper, "check_no_access");
	do_namespace_op(team1_helper, "check_no_access");
#else
	/* 1st party applications should not be able to look up semaphores created by 3rd party applications. */
	T_LOG("Check 1st party doesn't see 3rd party");

	do_namespace_op(team0_helper, "open_excl");
	do_namespace_op(team0_helper, "check_access");

	sem = sem_open(namespace_test_sem_name, 0);
	T_EXPECT_EQ_PTR(sem, SEM_FAILED, "sem_open on 3rd party semaphore");
	sem_close(sem);

	do_namespace_op(team0_helper, "unlink");

	/* 3rd party applications should not be able to interfere with eachother. */
	T_LOG("Check 3rd party doesn't see other 3rd party");

	do_namespace_op(team0_helper, "check_no_access");
	do_namespace_op(team1_helper, "check_no_access");

	do_namespace_op(team0_helper, "open_excl");
	do_namespace_op(team0_helper, "check_access");
	do_namespace_op(team1_helper, "check_no_access");

	do_namespace_op(team1_helper, "open_excl");
	do_namespace_op(team0_helper, "check_access");
	do_namespace_op(team1_helper, "check_access");

	do_namespace_op(team0_helper, "unlink");
	do_namespace_op(team0_helper, "check_no_access");
	do_namespace_op(team1_helper, "check_access");

	do_namespace_op(team1_helper, "unlink");
	do_namespace_op(team0_helper, "check_no_access");
	do_namespace_op(team1_helper, "check_no_access");
	#endif
}
