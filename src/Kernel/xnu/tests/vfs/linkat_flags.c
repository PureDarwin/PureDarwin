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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o linkat_flags linkat_flags.c -g -Weverything */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syslimits.h>

#include <darwintest.h>
#include <darwintest/utils.h>

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char file[PATH_MAX], sym[PATH_MAX], symloop[PATH_MAX], dirloop[PATH_MAX];
static char lfile1[PATH_MAX], lfile2[PATH_MAX], lfile3[PATH_MAX];
static char lfile4[PATH_MAX], lfile5[PATH_MAX], lfile6[PATH_MAX];
static char lfile7[PATH_MAX], lfile8[PATH_MAX];

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	if (lfile8[0] != '\0') {
		unlink(lfile8);
	}
	if (lfile7[0] != '\0') {
		unlink(lfile7);
	}
	if (lfile6[0] != '\0') {
		unlink(lfile6);
	}
	if (lfile5[0] != '\0') {
		unlink(lfile5);
	}
	if (lfile4[0] != '\0') {
		unlink(lfile4);
	}
	if (lfile3[0] != '\0') {
		unlink(lfile3);
	}
	if (lfile2[0] != '\0') {
		unlink(lfile2);
	}
	if (lfile1[0] != '\0') {
		unlink(lfile1);
	}
	if (dirloop[0] != '\0') {
		unlink(dirloop);
	}
	if (sym[0] != '\0') {
		unlink(sym);
	}
	if (file[0] != '\0') {
		unlink(file);
	}
	if (testdir) {
		rmdir(testdir);
	}
}

static void
verify_stat(nlink_t file_nlink, nlink_t sym_nlink)
{
	int error;
	struct stat buf;

	/* Verify file's status */
	memset(&buf, 0, sizeof(buf));
	error = fstatat(AT_FDCWD, file, &buf, 0);
	if (error) {
		T_ASSERT_FAIL("Calling fstatat for the file failed with %s", strerror(errno));
	}
	T_ASSERT_EQ(file_nlink, buf.st_nlink, "Validating file's nlink count of %d", file_nlink);

	/* Verify symlink's status */
	memset(&buf, 0, sizeof(buf));
	error = fstatat(AT_FDCWD, sym, &buf, AT_SYMLINK_NOFOLLOW);
	if (error) {
		T_ASSERT_FAIL("Calling fstatat for the symlink failed with %s", strerror(errno));
	}
	T_ASSERT_EQ(sym_nlink, buf.st_nlink, "Validating symlink's nlink count of %d", sym_nlink);
}

T_DECL(linkat_flags,
    "Test linkat's AT_SYMLINK_FOLLOW and AT_SYMLINK_NOFOLLOW_ANY flags")
{
	int fd;
	char testdir_path[MAXPATHLEN + 1];

	file[0] = sym[0] = dirloop[0] = '\0';
	lfile1[0] = lfile2[0] = lfile3[0] = '\0';
	lfile4[0] = lfile5[0] = lfile6[0] = '\0';
	lfile7[0] = lfile8[0] = '\0';

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	/* Create test root dir */
	snprintf(template, sizeof(template), "%s/linkat_flags-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");

	/* Get testdir full path */
	T_ASSERT_POSIX_SUCCESS((fd = open(testdir, O_SEARCH, 0777)), "Opening the test root directory");
	T_ASSERT_POSIX_SUCCESS(fcntl(fd, F_GETPATH, testdir_path), "Calling fcntl() to get the path");
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", testdir);

	/* Setup file names */
	snprintf(file, sizeof(file), "%s/%s", testdir_path, "file");
	snprintf(sym, sizeof(sym), "%s/%s", testdir_path, "sym");
	snprintf(dirloop, sizeof(dirloop), "%s/%s", testdir_path, "dirloop");
	snprintf(symloop, sizeof(symloop), "%s/%s", dirloop, "sym");
	snprintf(lfile1, sizeof(lfile1), "%s/%s", testdir_path, "lfile1");
	snprintf(lfile2, sizeof(lfile2), "%s/%s", testdir_path, "lfile2");
	snprintf(lfile3, sizeof(lfile3), "%s/%s", testdir_path, "lfile3");
	snprintf(lfile4, sizeof(lfile4), "%s/%s", testdir_path, "lfile4");
	snprintf(lfile5, sizeof(lfile5), "%s/%s", testdir_path, "lfile5");
	snprintf(lfile6, sizeof(lfile6), "%s/%s", testdir_path, "lfile6");
	snprintf(lfile7, sizeof(lfile7), "%s/%s", testdir_path, "lfile7");
	snprintf(lfile8, sizeof(lfile8), "%s/%s", testdir_path, "lfile8");

	/* Create the test files */
	T_ASSERT_POSIX_SUCCESS((fd = open(file, O_CREAT | O_RDWR, 0777)), "Creating %s", file);
	T_ASSERT_POSIX_SUCCESS(symlink(file, sym), "Creating symbolic link %s ---> %s", sym, file);
	T_ASSERT_POSIX_SUCCESS(symlink(testdir_path, dirloop), "Creating symbolic link %s ---> %s", dirloop, testdir_path);

	/* Validating nlink count */
	verify_stat(1, 1);

	/* Close the open files */
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", file);

	T_SETUPEND;

	T_LOG("Testing linkat() using no flags");
	{
		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, file, AT_FDCWD, lfile1, 0), "Calling linkat() while name1 is a file");
		verify_stat(2, 1);

		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, sym, AT_FDCWD, lfile2, 0), "Calling linkat() while name1 is a symbolic link");
		verify_stat(2, 2);

		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, symloop, AT_FDCWD, lfile3, 0), "Calling linkat() while name1 is a symbolic link and it's path contains a symbolic");
		verify_stat(2, 3);
	}

	T_LOG("Testing linkat() using the AT_SYMLINK_FOLLOW flag");
	{
		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, file, AT_FDCWD, lfile4, AT_SYMLINK_FOLLOW), "Calling linkat() while name1 is a file");
		verify_stat(3, 3);

		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, sym, AT_FDCWD, lfile5, AT_SYMLINK_FOLLOW), "Calling linkat() while name1 is a symbolic link");
		verify_stat(4, 3);

		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, symloop, AT_FDCWD, lfile6, AT_SYMLINK_FOLLOW), "Calling linkat() while name1 is a symbolic link and it's path contains a symbolic");
		verify_stat(5, 3);
	}

	T_LOG("Testing linkat() using the AT_SYMLINK_NOFOLLOW_ANY flag");
	{
		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, file, AT_FDCWD, lfile7, AT_SYMLINK_NOFOLLOW_ANY), "Calling linkat() while name1 is a file %s", file);
		verify_stat(6, 3);

		T_ASSERT_POSIX_SUCCESS(linkat(AT_FDCWD, sym, AT_FDCWD, lfile8, AT_SYMLINK_NOFOLLOW_ANY), "Calling linkat() while name1 is a symbolic link");
		verify_stat(6, 4);

		T_ASSERT_POSIX_FAILURE(linkat(AT_FDCWD, symloop, AT_FDCWD, "invalid_path", AT_SYMLINK_NOFOLLOW_ANY), ELOOP, "Calling linkat() while name1 is a symbolic link and it's path contains a symbolic");
	}

	/* See resolve_beneath.c for the AT_RESOLVE_BENEATH flag tests */

	/* See open_unique.c for the AT_UNIQUE flag tests */
}
