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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -lsandbox -o sandbox_appledouble_write sandbox_appledouble_write.c -g -Weverything */

#include <sys/xattr.h>
#include <sandbox/libsandbox.h>
#include <TargetConditionals.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define RUN_TEST     TARGET_OS_OSX

#define FILE "file"
#define FILE_AD "._file"
#define TMP_FILE_AD "._tmpfile"

#define FILE2 "f"
#define FILE2_AD "._f"
#define TMP_FILE2_AD "._g"

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char file[PATH_MAX], file2[PATH_MAX];
static sandbox_params_t params = NULL;
static sandbox_profile_t profile = NULL;

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(true),
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
	if (file2[0] != '\0') {
		unlink(file2);
	}
	if (testdir) {
		unmount(testdir, MNT_FORCE);
		rmdir(testdir);
	}
}

static void
create_profile_string(char *buff, size_t size)
{
	snprintf(buff, size, "(version 1) \n\
                          (allow default) \n\
                          (import \"system.sb\") \n\
                          (deny file-write-xattr (path \"%s\")) \n\
                          (deny file-write-xattr (path \"%s\")) \n",
	    file, file2);
}

T_DECL(sandbox_appledouble_write,
    "Verify that the 'file-write-xattr' permission is enforced for apple-double files")
{
	int testdirfd, fd;
	char *sberror = NULL;
	char profile_string[1000];
	char testdir_path[MAXPATHLEN];
	char mount_tmpfs_cmd[1000];
	file[0] = file2[0] = '\0';

#if (!RUN_TEST)
	T_SKIP("Not macOS");
#endif

	if (geteuid() != 0) {
		T_SKIP("Test should run as root");
	}

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	/* Create test root dir */
	snprintf(template, sizeof(template), "%s/sandbox_appledouble_write-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");
	T_ASSERT_POSIX_SUCCESS((fd = open(testdir, O_SEARCH, 0777)), "Opening test root directory %s", testdir);
	T_ASSERT_POSIX_SUCCESS(fcntl(fd, F_GETPATH, testdir_path), "Calling fcntl() to get the path");
	close(fd);

	/* mount tmpfs */
	snprintf(mount_tmpfs_cmd, sizeof(mount_tmpfs_cmd), "/sbin/mount_tmpfs -s 50m %s", testdir_path);
	T_ASSERT_POSIX_SUCCESS(system(mount_tmpfs_cmd), "Mounting tmpfs mount -> Should PASS");

	T_EXPECT_POSIX_SUCCESS((testdirfd = open(testdir_path, O_SEARCH, 0777)), "Opening test root directory");

	/* Setup file names */
	snprintf(file, sizeof(file), "%s/%s", testdir_path, FILE);
	snprintf(file2, sizeof(file2), "%s/%s", testdir_path, FILE2);

	/* Create the test files */
	T_ASSERT_POSIX_SUCCESS((fd = openat(testdirfd, FILE, O_CREAT | O_RDWR, 0777)), "Creating %s", FILE);
	close(fd);

	T_ASSERT_POSIX_SUCCESS((fd = openat(testdirfd, FILE_AD, O_CREAT | O_RDWR, 0777)), "Creating %s", FILE_AD);
	close(fd);

	T_ASSERT_POSIX_SUCCESS((fd = openat(testdirfd, FILE2, O_CREAT | O_RDWR, 0777)), "Creating %s", FILE2);
	close(fd);

	T_ASSERT_POSIX_SUCCESS((fd = openat(testdirfd, FILE2_AD, O_CREAT | O_RDWR, 0777)), "Creating %s", FILE2_AD);
	close(fd);

	/* Create sandbox variables */
	T_ASSERT_POSIX_NOTNULL(params = sandbox_create_params(), "Creating Sandbox params object");
	create_profile_string(profile_string, sizeof(profile_string));
	T_ASSERT_POSIX_NOTNULL(profile = sandbox_compile_string(profile_string, params, &sberror), "Creating Sandbox profile object");

	T_SETUPEND;

	/* Validate SUCCESS for rename */
	T_EXPECT_POSIX_SUCCESS(renameat(testdirfd, FILE_AD, testdirfd, TMP_FILE_AD), "Verifying that rename() of '%s' -> '%s' succeeded", FILE_AD, TMP_FILE_AD);
	T_EXPECT_POSIX_SUCCESS(renameat(testdirfd, TMP_FILE_AD, testdirfd, FILE_AD), "Verifying that rename() of '%s' -> '%s' succeeded", TMP_FILE_AD, FILE_AD);

	T_EXPECT_POSIX_SUCCESS(renameat(testdirfd, FILE2_AD, testdirfd, TMP_FILE2_AD), "Verifying that rename() of '%s' -> '%s' succeeded", FILE2_AD, TMP_FILE2_AD);
	T_EXPECT_POSIX_SUCCESS(renameat(testdirfd, TMP_FILE2_AD, testdirfd, FILE2_AD), "Verifying that rename() of '%s' -> '%s' succeeded", TMP_FILE2_AD, FILE2_AD);

	/* Validate SUCCESS for unlink */
	T_EXPECT_POSIX_SUCCESS(unlinkat(testdirfd, FILE_AD, 0), "Verifying that unlink() of '%s' succeeded", FILE_AD);
	T_EXPECT_POSIX_SUCCESS(unlinkat(testdirfd, FILE2_AD, 0), "Verifying that unlink() of '%s' succeeded", FILE2_AD);

	/* Validate SUCCESS for open/create */
	T_EXPECT_POSIX_SUCCESS((fd = openat(testdirfd, FILE_AD, O_CREAT | O_WRONLY, 0777)), "Verifying that open() with O_WRONLY of '%s' succeeded ", FILE_AD);
	if (fd >= 0) {
		close(fd);
	}
	T_EXPECT_POSIX_SUCCESS((fd = openat(testdirfd, FILE2_AD, O_CREAT | O_TRUNC, 0777)), "Verifying that open() with O_TRUNC of '%s' succeeded", FILE2_AD);
	if (fd >= 0) {
		close(fd);
	}

	/* Apply sandbox profile */
	T_ASSERT_POSIX_SUCCESS(sandbox_apply(profile), "Applying Sandbox profile");

	/* Validate EPERM for rename */
	T_EXPECT_POSIX_FAILURE(renameat(testdirfd, FILE_AD, testdirfd, TMP_FILE_AD), EPERM, "Verifying that rename() of '%s' -> '%s' fails with EPERM", FILE_AD, TMP_FILE_AD);
	T_EXPECT_POSIX_FAILURE(renameat(testdirfd, FILE2_AD, testdirfd, TMP_FILE2_AD), EPERM, "Verifying that rename() of '%s' -> '%s' fails with EPERM", FILE2_AD, TMP_FILE2_AD);

	/* Validate EPERM for unlink */
	T_EXPECT_POSIX_FAILURE(unlinkat(testdirfd, FILE_AD, 0), EPERM, "Verifying that unlink() of '%s' fails with EPERM", FILE_AD);
	T_EXPECT_POSIX_FAILURE(unlinkat(testdirfd, FILE2_AD, 0), EPERM, "Verifying that unlink() of '%s' fails with EPERM", FILE2_AD);

	/* Validate EPERM for open */
	T_EXPECT_POSIX_FAILURE((fd = openat(testdirfd, FILE_AD, O_WRONLY, 0777)), EPERM, "Verifying that open() with O_WRONLY of '%s' fails with EPERM", FILE_AD);
	if (fd >= 0) {
		close(fd);
	}
	T_EXPECT_POSIX_FAILURE((fd = openat(testdirfd, FILE2_AD, O_TRUNC, 0777)), EPERM, "Verifying that open() with O_TRUNC of '%s' fails with EPERM", FILE2_AD);
	if (fd >= 0) {
		close(fd);
	}

	T_ASSERT_POSIX_SUCCESS(close(testdirfd), "Closing %s", testdir_path);
}
