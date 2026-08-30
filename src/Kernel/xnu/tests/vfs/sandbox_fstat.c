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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -lsandbox -o sandbox_fstat sandbox_fstat.c -g -Weverything */

#include <sys/paths.h>
#include <sys/xattr.h>
#include <sandbox/libsandbox.h>
#include <TargetConditionals.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define RUN_TEST     TARGET_OS_OSX

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char file[PATH_MAX], file_rsrcfork[PATH_MAX];
static sandbox_params_t params = NULL;
static sandbox_profile_t profile = NULL;

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
	if (profile) {
		sandbox_free_profile(profile);
	}
	if (params) {
		sandbox_free_params(params);
	}
	if (file[0] != '\0') {
		unlink(file);
	}
	if (testdir) {
		rmdir(testdir);
	}
}

static void
create_profile_string(char *buff, size_t size)
{
	snprintf(buff, size, "(version 1) \n\
                          (allow default) \n\
                          (import \"system.sb\") \n\
                          (deny file-read-metadata (path \"%s\")) \n",
	    file);
}
static void
do_test(int expected_error)
{
	int fd;
	struct stat sb;

	/* Test stat() */
	if (expected_error) {
		T_EXPECT_POSIX_FAILURE(stat(file, &sb), expected_error, "Calling stat() should FAIL with '%s'", strerror(expected_error));
	} else {
		T_EXPECT_POSIX_SUCCESS(stat(file, &sb), "Calling stat() for the file should PASS");
	}

	/* Test fstat() while the file is open with the O_CREAT | O_WRONLY flags  */
	T_EXPECT_POSIX_SUCCESS(fd = open(file, O_CREAT | O_WRONLY, 0666), "Opening with the O_CREAT | O_WRONLY flags");
	if (fd != -1) {
		if (expected_error) {
			T_EXPECT_POSIX_FAILURE(fstat(fd, &sb), expected_error, "Calling fstat() should FAIL with '%s'", strerror(expected_error));
		} else {
			T_EXPECT_POSIX_SUCCESS(fstat(fd, &sb), "Calling fstat() for the test file should PASS");
		}
		close(fd);
	}

	T_EXPECT_POSIX_SUCCESS(fd = open(file_rsrcfork, O_CREAT | O_WRONLY, 0666), "Opening rsrcfork with the O_CREAT | O_WRONLY flags");
	if (fd != -1) {
		T_EXPECT_POSIX_SUCCESS(fstat(fd, &sb), "Calling fstat() for the rsrcfork should PASS");
		close(fd);
	}
}

T_DECL(sandbox_fstat,
    "Prevent the information disclosure on files opened with O_WRONLY while sandbox profile denies 'file-read-metadata'")
{
#if (!RUN_TEST)
	T_SKIP("Not macOS");
#endif

	int fd;
	char *sberror = NULL;
	char profile_string[1000];
	char testdir_path[MAXPATHLEN];

	file[0] = '\0';

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	/* Create test root dir */
	snprintf(template, sizeof(template), "%s/sandbox_fstat-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");
	T_ASSERT_POSIX_SUCCESS((fd = open(testdir, O_SEARCH, 0777)), "Opening test root directory '%s'", testdir);
	T_ASSERT_POSIX_SUCCESS(fcntl(fd, F_GETPATH, testdir_path), "Calling fcntl() to get the path");
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", testdir_path);

	/* Setup file names */
	snprintf(file, sizeof(file), "%s/%s", testdir_path, "file");
	snprintf(file_rsrcfork, sizeof(file_rsrcfork), "%s/%s", file, _PATH_RSRCFORKSPEC);

	/* Create the test file */
	T_ASSERT_POSIX_SUCCESS((fd = open(file, O_CREAT | O_RDWR, 0777)), "Creating '%s'", file);
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing '%s'", file);

	/* Create sandbox variables */
	T_ASSERT_POSIX_NOTNULL(params = sandbox_create_params(), "Creating Sandbox params object");
	create_profile_string(profile_string, sizeof(profile_string));
	T_ASSERT_POSIX_NOTNULL(profile = sandbox_compile_string(profile_string, params, &sberror), "Creating Sandbox profile object");

	T_SETUPEND;

	/* Test stat()/fstat() */
	do_test(0);

	/* Apply sandbox profile */
	T_ASSERT_POSIX_SUCCESS(sandbox_apply(profile), "Applying Sandbox profile");

	/* Test stat()/fstat() */
	do_test(EPERM);
}
