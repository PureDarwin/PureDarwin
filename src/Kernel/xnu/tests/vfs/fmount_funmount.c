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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o fmount_funmount fmount_funmount.c -g -Weverything */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define RUN_TEST     TARGET_OS_OSX

#define FSTYPE_APFS "apfs"
#define FSTYPE_DEVFS "devfs"

static char template[MAXPATHLEN];
static char *testdir = NULL;

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_ENABLED(RUN_TEST),
	T_META_CHECK_LEAKS(false));

static int
verify_fstypename(const char *name)
{
	int error;
	struct statfs statfs_buf;;

	error = statfs(testdir, &statfs_buf);
	if (error) {
		return errno;
	}

	if (strncmp(name, statfs_buf.f_fstypename, MFSNAMELEN)) {
		return EINVAL;
	}

	return 0;
}

static void
cleanup(void)
{
	if (testdir) {
		rmdir(testdir);
	}
}

T_DECL(fmount_funmount,
    "Test fmount() and funmount() system calls")
{
#if (!RUN_TEST)
	T_SKIP("Not macOS");
#endif

	int fd;

	T_ATEND(cleanup);

	T_SETUPBEGIN;

	snprintf(template, sizeof(template), "%s/fmount_funmount-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");
	T_ASSERT_POSIX_ZERO(verify_fstypename(FSTYPE_APFS), "Verifing fstype name equals %s", FSTYPE_APFS);

	T_SETUPEND;

	/* Mount phase */
	T_ASSERT_POSIX_SUCCESS((fd = open(testdir, O_DIRECTORY)), "Open test root dir: %s", testdir);
	T_ASSERT_POSIX_SUCCESS(fmount(FSTYPE_DEVFS, fd, MNT_RDONLY, NULL), "Mounting temporary %s mount using fmount(fd = %d)", FSTYPE_DEVFS, fd);
	T_ASSERT_POSIX_ZERO(verify_fstypename(FSTYPE_DEVFS), "Verifing fstype name equals %s", FSTYPE_DEVFS);
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing (fd = %d)", fd);

	/* Unmount phase */
	T_ASSERT_POSIX_SUCCESS((fd = open(testdir, O_DIRECTORY)), "Open test root dir: %s", testdir);
	T_ASSERT_POSIX_SUCCESS(funmount(fd, MNT_FORCE), "Unmounting %s using funmount(fd = %d)", testdir, fd);
	T_ASSERT_POSIX_ZERO(verify_fstypename(FSTYPE_APFS), "Verifing fstype name equals %s", FSTYPE_APFS);
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing (fd = %d)", fd);
}
