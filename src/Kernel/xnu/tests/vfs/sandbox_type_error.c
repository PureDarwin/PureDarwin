/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -lsandbox -o sandbox_type_error sandbox_type_error.c -g -Weverything */

#include <sandbox/libsandbox.h>
#include <TargetConditionals.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define RUN_TEST     TARGET_OS_OSX

static sandbox_params_t params = NULL;

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_ENABLED(RUN_TEST),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	if (params) {
		sandbox_free_params(params);
	}
}

static void
create_profile_string(char *buff, size_t size, char *path)
{
	snprintf(buff, size, "(version 1) \n\
                          (allow default) \n\
                          (deny file-read-metadata (path \"%s\")) \n",
	    path);
}

static void
test_path(char *deny_path, char *stat_path, int expected_err)
{
	struct stat sb;
	pid_t pid, res;
	char *sberror = NULL;
	char profile_string[1000];
	sandbox_profile_t profile = NULL;
	int status, error, ret;

	/* Fork */
	pid = fork();
	if (pid < -1) {
		T_FAIL("Failed to fork");
		return;
	}

	switch (pid) {
	case 0:
		/* Create sandbox variables */
		create_profile_string(profile_string, sizeof(profile_string), deny_path);
		if ((profile = sandbox_compile_string(profile_string, params, &sberror)) == NULL) {
			T_FAIL("Creating Sandbox profile object");
			exit(EINVAL);
		}

		error = sandbox_apply(profile);
		if (error) {
			T_FAIL("Applying Sandbox profile FAILED");
			sandbox_free_profile(profile);
			exit(EINVAL);
		}

		/* Query stat */
		error = stat(stat_path, &sb);

		/* Validate error */
		if ((!error && !expected_err) || (error == -1 && errno == expected_err)) {
			ret = 0;
		} else {
			ret = errno;
		}

		if (profile) {
			sandbox_free_profile(profile);
		}
		exit(ret);
	default:
		do {
			res = waitpid(pid, &status, WUNTRACED);
		} while (res == -1 && errno == EINTR);

		if (res != pid) {
			T_FAIL("(res != pid");
			break;
		}

		if (!WIFEXITED(status)) {
			T_FAIL("Stat of '%s' with deny path of '%s' FAILED", stat_path, deny_path);
			break;
		}

		if (WEXITSTATUS(status)) {
			T_FAIL("Stat of '%s' with deny path of '%s' should FAIL with '%s', got '%s'", stat_path, deny_path, strerror(expected_err), strerror(WEXITSTATUS(status)));
			break;
		}

		if (expected_err) {
			T_PASS("Stat of '%s' with deny path of '%s' should FAIL with '%s'", stat_path, deny_path, strerror(expected_err));
		} else {
			T_PASS("Stat of '%s' with deny path of '%s' should PASS", stat_path, deny_path);
		}
	}
}

T_DECL(sandbox_type_error,
    "Prevent the information disclosure on resource type File/Directory/Symlink")
{
#if (!RUN_TEST)
	T_SKIP("Not macOS");
#endif

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	T_ASSERT_POSIX_NOTNULL(params = sandbox_create_params(), "Creating Sandbox params object");

	T_SETUPEND;

	/* Verify handling of non-existent files */
	test_path("/.file", "/.nofollow/notexist/", ENOENT);

	/* Prevent the information disclosure on the resource type for file */
	test_path("/.file", "/.nofollow/.file/", EPERM);

	/* Prevent the information disclosure on the resource type for directory */
	test_path("/private", "/.nofollow/private/", EPERM);

	/* Prevent the information disclosure on the resource type for symlink */
	test_path("/tmp", "/.nofollow/tmp/", EPERM);

	/* Prevent the information disclosure on the resource type for symlink child */
	test_path("/tmp", "/.nofollow/tmp/notexist", EPERM);
}
