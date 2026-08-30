#include <darwintest.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <spawn_private.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/spawn_internal.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <sysexits.h>
#include <unistd.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

/* TEST_PATH needs to be something that exists, but is not the cwd */
#define TEST_PATH "/System/Library/Caches"

T_DECL(posix_spawn_file_actions_addchdir, "Check posix_spawn_file_actions_addchdir",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	posix_spawn_file_actions_t file_actions;
	int ret;

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	ret = posix_spawn_file_actions_addchdir(&file_actions, TEST_PATH);
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_addchdir");

	char * const    prog = "/bin/sh";
	char * const    argv_child[] = { prog,
		                         "-c",
		                         "test $(pwd) = \"" TEST_PATH "\"",
		                         NULL, };
	pid_t           child_pid;
	extern char   **environ;

	ret = posix_spawn(&child_pid, prog, &file_actions, NULL, argv_child, environ);
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn");

	T_LOG("parent: spawned child with pid %d\n", child_pid);

	ret = posix_spawn_file_actions_destroy(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_destroy");

	T_LOG("parent: waiting for child process\n");

	int status = 0;
	int waitpid_result = waitpid(child_pid, &status, 0);
	T_ASSERT_POSIX_SUCCESS(waitpid_result, "waitpid");
	T_ASSERT_EQ(waitpid_result, child_pid, "waitpid should return child we spawned");
	T_ASSERT_EQ(WIFEXITED(status), 1, "child should have exited normally");
	T_ASSERT_EQ(WEXITSTATUS(status), EX_OK, "child should have exited with success");
}

T_DECL(posix_spawn_file_actions_addchdir_errors, "Check posix_spawn_file_actions_addchdir errors",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	char longpath[PATH_MAX + 1];
	posix_spawn_file_actions_t file_actions;
	int ret;

	memset(longpath, 'a', PATH_MAX);
	longpath[PATH_MAX] = '\0';

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	ret = posix_spawn_file_actions_addchdir(NULL, "/");
	T_ASSERT_EQ(ret, EINVAL, "NULL *file_actions returns EINVAL");

	ret = posix_spawn_file_actions_addchdir(&file_actions, longpath);
	T_ASSERT_EQ(ret, ENAMETOOLONG, "Path longer than PATH_MAX returns ENAMETOOLONG");

	ret = posix_spawn_file_actions_destroy(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_destroy");
}

T_DECL(posix_spawn_file_actions_addfchdir, "Check posix_spawn_file_actions_addfchdir",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	posix_spawn_file_actions_t file_actions;
	int ret;
	int test_fd;

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	test_fd = open(TEST_PATH, O_RDONLY | O_CLOEXEC);
	T_ASSERT_POSIX_SUCCESS(test_fd, "open " TEST_PATH);

	ret = posix_spawn_file_actions_addfchdir(&file_actions, test_fd);
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_addfchdir");

	char * const    prog = "/bin/sh";
	char * const    argv_child[] = { prog,
		                         "-c",
		                         "test $(pwd) = \"" TEST_PATH "\"",
		                         NULL, };
	pid_t           child_pid;
	extern char   **environ;

	ret = posix_spawn(&child_pid, prog, &file_actions, NULL, argv_child, environ);
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn");

	T_LOG("parent: spawned child with pid %d\n", child_pid);

	ret = posix_spawn_file_actions_destroy(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_destroy");

	T_LOG("parent: waiting for child process\n");

	int status = 0;
	int waitpid_result = waitpid(child_pid, &status, 0);
	T_ASSERT_POSIX_SUCCESS(waitpid_result, "waitpid");
	T_ASSERT_EQ(waitpid_result, child_pid, "waitpid should return child we spawned");
	T_ASSERT_EQ(WIFEXITED(status), 1, "child should have exited normally");
	T_ASSERT_EQ(WEXITSTATUS(status), EX_OK, "child should have exited with success");

	ret = close(test_fd);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "close test fd");
}

T_DECL(posix_spawn_file_actions_addfchdir_errors, "Check posix_spawn_file_actions_addfchdir errors",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	posix_spawn_file_actions_t file_actions;
	int ret;

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	ret = posix_spawn_file_actions_addfchdir(NULL, 0);
	T_ASSERT_EQ(ret, EINVAL, "NULL *file_actions returns EINVAL");

	ret = posix_spawn_file_actions_addfchdir(&file_actions, -1);
	T_ASSERT_EQ(ret, EBADF, "-1 file descriptor returns EBADF");

	ret = posix_spawn_file_actions_destroy(&file_actions);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_destroy");
}
