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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include <darwintest.h>
#include <darwintest/utils.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED,
	T_META_OWNER("m_staveleytaylor"));

static char lstat_testdir[PATH_MAX];
static char access_testdir[PATH_MAX];

static void
cleanup_lstat()
{
	rmdir("c");
	unlink("b");
	unlink("a");
	rmdir(lstat_testdir);
}

static void
cleanup_access()
{
	unlink("test.f");
	unlink("test.lnk");
	unlink("test.d/test.df");
	rmdir("test.d");
	rmdir(access_testdir);
}

T_DECL(
	lstat_symlink_trailing_slash,
	"Check symlinks-to-symlinks are resolved correctly when trailing slashes are involved"
	) {
	struct stat st;

	T_ATEND(cleanup_lstat);
	T_SETUPBEGIN;

	// Create test root dir
	snprintf(lstat_testdir, sizeof(lstat_testdir), "%s/symlink_trailing_slash-lstat-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL(mkdtemp(lstat_testdir), "setup: create test root dir");

	// CD into test root dir
	T_ASSERT_POSIX_SUCCESS(chdir(lstat_testdir), "setup: cd testdir");

	// Setup a scenario with 'a -> b -> c' (where -> means 'is a symlink to').
	T_ASSERT_POSIX_SUCCESS(mkdir("c", 0755), "setup: mkdir c");
	T_ASSERT_POSIX_SUCCESS(symlink("c", "b"), "setup: ln c b");
	T_ASSERT_POSIX_SUCCESS(symlink("b", "a"), "setup: ln b a");

	T_SETUPEND;

	// stat

	T_ASSERT_POSIX_SUCCESS(stat("a", &st), "stat a succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "stat thinks a is directory");

	T_ASSERT_POSIX_SUCCESS(stat("b", &st), "stat b succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "stat thinks b is directory");

	T_ASSERT_POSIX_SUCCESS(stat("b/", &st), "stat b/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "stat thinks b/ is directory");

	T_ASSERT_POSIX_SUCCESS(stat("a/.", &st), "stat a/. succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "stat thinks a/. is directory");

	T_ASSERT_POSIX_SUCCESS(stat("a/", &st), "stat a/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "stat thinks a/ is directory");

	// lstat

	T_ASSERT_POSIX_SUCCESS(lstat("a", &st), "lstat a succeeds");
	T_ASSERT_TRUE(S_ISLNK(st.st_mode), "lstat thinks a is symlink");

	T_ASSERT_POSIX_SUCCESS(lstat("b", &st), "lstat b succeeds");
	T_ASSERT_TRUE(S_ISLNK(st.st_mode), "lstat thinks b is symlink");

	T_ASSERT_POSIX_SUCCESS(lstat("b/", &st), "lstat b/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks b/ is directory");

	T_ASSERT_POSIX_SUCCESS(lstat("a/.", &st), "lstat a/. succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks a/. is directory");

	// rdar://142559105 (lstat() of a name with trailing '/' is handled differently than other platforms)
	T_ASSERT_POSIX_SUCCESS(lstat("a/", &st), "lstat a/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks a/ is directory");

	// Now modify a such that it has a trailing slash in the link itself.
	T_ASSERT_POSIX_SUCCESS(unlink("a"), "unlink a");
	T_ASSERT_POSIX_SUCCESS(symlink("b/", "a"), "symlink a -> b/");

	T_ASSERT_POSIX_SUCCESS(lstat("a", &st), "lstat a succeeds");
	T_ASSERT_TRUE(S_ISLNK(st.st_mode), "lstat thinks a is symlink");

	T_ASSERT_POSIX_SUCCESS(lstat("a/", &st), "lstat a/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks a/ is directory");

	// Do the same for b.
	T_ASSERT_POSIX_SUCCESS(unlink("b"), "unlink b");
	T_ASSERT_POSIX_SUCCESS(symlink("c/", "b"), "symlink b -> c/");

	T_ASSERT_POSIX_SUCCESS(lstat("a", &st), "lstat a succeeds");
	T_ASSERT_TRUE(S_ISLNK(st.st_mode), "lstat thinks a is symlink");

	T_ASSERT_POSIX_SUCCESS(lstat("a/", &st), "lstat a/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks a/ is directory");

	T_ASSERT_POSIX_SUCCESS(lstat("b", &st), "lstat b succeeds");
	T_ASSERT_TRUE(S_ISLNK(st.st_mode), "lstat thinks b is symlink");

	T_ASSERT_POSIX_SUCCESS(lstat("b/", &st), "lstat b/ succeeds");
	T_ASSERT_TRUE(S_ISDIR(st.st_mode), "lstat thinks b/ is directory");
}

T_DECL(
	access_symlink_trailing_slash,
	"Check access returns ENOTDIR when symlink points to a file and trailing slash was used"
	) {
	T_ATEND(cleanup_access);
	T_SETUPBEGIN;

	// Create test root dir
	snprintf(access_testdir, sizeof(access_testdir), "%s/symlink_trailing_slash-access-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL(mkdtemp(access_testdir), "setup: create test root dir");
	printf("testdir is %s\n", access_testdir);

	// CD into test root dir
	T_ASSERT_POSIX_SUCCESS(chdir(access_testdir), "setup: cd testdir");

	T_ASSERT_POSIX_SUCCESS(creat("test.f", 0755), "setup: touch test.f");
	T_ASSERT_POSIX_SUCCESS(symlink("test.f", "test.lnk"), "setup: ln test.f test.lnk");
	T_ASSERT_POSIX_SUCCESS(mkdir("test.d", 0755), "setup: mkdir test.d");
	T_ASSERT_POSIX_SUCCESS(creat("test.d/test.df", 0755), "setup: touch test.d/test.df");

	T_SETUPEND;

	T_ASSERT_POSIX_SUCCESS(access("test.lnk", R_OK), "access test.lnk suceeds");

	T_ASSERT_EQ(access("test.lnk/", R_OK), -1, "access test.lnk/ returns -1");
	T_ASSERT_POSIX_ERROR(errno, ENOTDIR, "access sets errno to ENOTDIR");

	// Now modify test.lnk to contain a trailing slash in the link itself
	T_ASSERT_POSIX_SUCCESS(unlink("test.lnk"), "rm test.lnk");
	T_ASSERT_POSIX_SUCCESS(symlink("test.f/", "test.lnk"), "ln -s test.f/ test.lnk");

	T_ASSERT_EQ(access("test.lnk", R_OK), -1, "access test.lnk returns -1");
	T_ASSERT_POSIX_ERROR(errno, ENOTDIR, "access sets errno to ENOTDIR");

	T_ASSERT_EQ(access("test.lnk/", R_OK), -1, "access test.lnk/ returns -1");
	T_ASSERT_POSIX_ERROR(errno, ENOTDIR, "access sets errno to ENOTDIR");

	// Now introduce a directory so that we have:
	// test.lnk -> test.d/ which contains test.f
	// The trailing slash in test.lnk should not cause access("test.lnk/test.f") to fail
	T_ASSERT_POSIX_SUCCESS(unlink("test.lnk"), "rm test.lnk");
	T_ASSERT_POSIX_SUCCESS(symlink("test.d/", "test.lnk"), "ln -s test.d/ test.lnk");
	T_ASSERT_POSIX_SUCCESS(access("test.lnk/test.df", R_OK), "access test.lnk/test.df");
}
