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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o openbyid_stress openbyid_stress.c -g -Weverything */
/* sign: codesign --force --sign - --timestamp=none --entitlements openbyid_stress.entitlements openbyid_stress */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fsgetpath_private.h>
#include <dispatch/dispatch.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define TEST_DURATION 10 /* seconds */

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char dir1[PATH_MAX], dir2[PATH_MAX];
static char file1[PATH_MAX], file2[PATH_MAX];

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	if (file1[0] != '\0') {
		unlink(file1);
	}
	if (file2[0] != '\0') {
		unlink(file2);
	}
	if (dir1[0] != '\0') {
		rmdir(dir1);
	}
	if (dir2[0] != '\0') {
		rmdir(dir2);
	}
	if (testdir) {
		rmdir(testdir);
	}
}

T_DECL(openbyid_stress,
    "Test that openbyid_np does not open the wrong file")
{
	int fd;
	struct stat buf_stat;
	struct statfs buf_statfs;
	__block int timeout = 0;
	__block int error = 0;
	int64_t interval = TEST_DURATION * NSEC_PER_SEC;
	dispatch_queue_t queue;
	dispatch_source_t timeout_source;

	dir1[0] = dir2[0] = '\0';
	file2[0] = file2[0] = '\0';

	T_ATEND(cleanup);

	T_SETUPBEGIN;

	T_ASSERT_NOTNULL((queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)), "Getting global queue");
	T_ASSERT_NOTNULL((timeout_source =  dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue)), "Creating dispatch source");

	dispatch_source_set_timer(timeout_source, dispatch_time(DISPATCH_TIME_NOW, interval), DISPATCH_TIME_FOREVER, 0);
	dispatch_source_set_event_handler(timeout_source, ^{
		timeout = 1;
		T_LOG("%d seconds timeout expired", TEST_DURATION);
	});

	snprintf(template, sizeof(template), "%s/openbyid_stress-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");

	snprintf(dir1, sizeof(dir1), "%s/%s", testdir, "dir1");
	snprintf(dir2, sizeof(dir2), "%s/%s", testdir, "dir2");

	T_ASSERT_POSIX_SUCCESS(mkdir(dir1, 0777), "Creating dir1");
	T_ASSERT_POSIX_SUCCESS(mkdir(dir2, 0777), "Creating dir2");

	snprintf(file1, sizeof(file1), "%s/%s", dir1, "file");
	snprintf(file2, sizeof(file2), "%s/%s", dir2, "file");

	T_ASSERT_POSIX_SUCCESS((fd = open(file1, O_CREAT | O_RDWR, 0777)), "Creating %s", file1);
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", file1);

	T_ASSERT_POSIX_SUCCESS((fd = open(file2, O_CREAT | O_RDWR, 0777)), "Creating %s", file2);
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", file2);

	T_ASSERT_POSIX_SUCCESS(stat(file1, &buf_stat), "Calling stat() on %s", file1);
	T_ASSERT_POSIX_SUCCESS(statfs(file1, &buf_statfs), "Calling statfs() on %s", file1);

	T_LOG("File successfully opened: fsid {%d, %d}, inode %llu", buf_statfs.f_fsid.val[0], buf_statfs.f_fsid.val[1], buf_stat.st_ino);

	T_SETUPEND;

	T_LOG("Running for %d seconds", TEST_DURATION);
	dispatch_resume(timeout_source);

	/* Replace between dir1 and dir2 */
	dispatch_async(queue, ^(void) {
		while (!timeout && !error) {
		        renamex_np(dir1, dir2, RENAME_SWAP);
		}
	});

	/* Query openbyid_np */
	while (!timeout && !error) {
		int fd2;
		struct stat buf_stat2;
		struct statfs buf_statfs2;

		if ((fd2 = openbyid_np(&buf_statfs.f_fsid, (fsobj_id_t *)&buf_stat.st_ino, 0)) < 0) {
			T_FAIL("openbyid_np() failed %d", errno);
			error = errno;
			break;
		}

		if ((error = fstatfs(fd2, &buf_statfs2)) < 0) {
			T_FAIL("fstatfs() failed");
			error = errno;
			close(fd2);
			break;
		}

		if ((error = fstat(fd2, &buf_stat2)) < 0) {
			T_FAIL("fstat() failed");
			error = errno;
			close(fd2);
			break;
		}

		if (buf_statfs.f_fsid.val[0] != buf_statfs2.f_fsid.val[0] ||
		    buf_statfs.f_fsid.val[1] != buf_statfs2.f_fsid.val[1] ||
		    buf_stat2.st_ino != buf_stat.st_ino) {
			T_FAIL("Wrong file opened! fsid {%d, %d}, inode %llu", buf_statfs2.f_fsid.val[0], buf_statfs2.f_fsid.val[1], buf_stat2.st_ino);
			error = EINVAL;
			close(fd2);
			break;
		}

		close(fd2);
	}

	T_ASSERT_POSIX_ZERO(error, "Test completed without error(s)");
}
