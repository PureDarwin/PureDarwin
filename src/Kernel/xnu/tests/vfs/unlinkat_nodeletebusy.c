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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o unlinkat_nodeletebusy unlinkat_nodeletebusy.c -g -Weverything */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fsgetpath_private.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#ifndef AT_NODELETEBUSY
#define AT_NODELETEBUSY         0x4000
#endif

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char file[PATH_MAX];

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	if (file[0] != '\0') {
		unlink(file);
	}
	if (testdir) {
		rmdir(testdir);
	}
}

T_DECL(unlinkat_nodeletebusy,
    "Verify that O_SYMLINK is not being ignored while used by open() in addition to O_CREAT")
{
	int fd;

	file[0] = '\0';

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	/* Create test root dir */
	snprintf(template, sizeof(template), "%s/unlinkat_nodeletebusy-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");

	/* Setup file name */
	snprintf(file, sizeof(file), "%s/%s", testdir, "file");

	T_SETUPEND;

	/* Create the test file */
	T_ASSERT_POSIX_SUCCESS((fd = open(file, O_CREAT | O_RDWR, 0777)), "Creating test file");

	/* Unlinking when file is opened */
	T_ASSERT_POSIX_SUCCESS(unlinkat(AT_FDCWD, file, 0), "Unlinking when file is opened");

	/* Closing the test file */
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing file");

	/* Create the test file */
	T_ASSERT_POSIX_SUCCESS((fd = open(file, O_CREAT | O_RDWR, 0777)), "Creating test file");

	/* Unlinking when file is opened using the AT_NODELETEBUSY flag */
	T_ASSERT_POSIX_FAILURE(unlinkat(AT_FDCWD, file, AT_NODELETEBUSY), EBUSY, "Unlinking when file is opened using the AT_NODELETEBUSY flag -> Should fail with EBUSY");

	/* Closing the test file */
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing file");

	/* Unlinking when file is NOT opened using the AT_NODELETEBUSY flag */
	T_ASSERT_POSIX_SUCCESS(unlinkat(AT_FDCWD, file, AT_NODELETEBUSY), "Unlinking when file is NOT opened using the AT_NODELETEBUSY flag -> Should pass");
}
