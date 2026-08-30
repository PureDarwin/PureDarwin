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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o open_symlink open_symlink.c -g -Weverything */

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

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char file[PATH_MAX], sym[PATH_MAX];

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
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

T_DECL(open_symlink,
    "Verify that O_SYMLINK is not being ignored while used by open() in addition to O_CREAT")
{
	int fd;
	char namebuf[MAXPATHLEN + 1];
	char namebuf2[MAXPATHLEN + 1];

	file[0] = sym[0] = '\0';

	T_ATEND(cleanup);
	T_SETUPBEGIN;

	/* Create test root dir */
	snprintf(template, sizeof(template), "%s/open_symlink-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");

	/* Setup file names */
	snprintf(file, sizeof(file), "%s/%s", testdir, "file");
	snprintf(sym, sizeof(sym), "%s/%s", testdir, "symlink");

	/* Create the test file */
	T_ASSERT_POSIX_SUCCESS((fd = open(file, O_CREAT | O_RDWR, 0777)), "Creating file %s", file);

	/* Create the symlink */
	T_ASSERT_POSIX_SUCCESS(symlink(file, sym), "Creating symlink %s -> %s", sym, file);

	/* Close the test file */
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", file);

	T_SETUPEND;

	/* Step 1 - Verify O_SYMLINK behaviour */
	T_ASSERT_POSIX_SUCCESS((fd = open(sym, O_SYMLINK, 0777)), "Opening %s using the O_SYMLINK flag", sym);
	T_ASSERT_POSIX_SUCCESS(fcntl(fd, F_GETPATH, namebuf), "Calling fcntl() to get the path");
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", sym);

	/* Step 2 - Verify O_SYMLINK | O_CREAT behaviour */
	T_ASSERT_POSIX_SUCCESS((fd = open(sym, O_SYMLINK | O_CREAT, 0777)), "Opening %s using the O_SYMLINK | O_CREAT flags", sym);
	T_ASSERT_POSIX_SUCCESS(fcntl(fd, F_GETPATH, namebuf2), "Calling fcntl() to get the path");
	T_ASSERT_POSIX_SUCCESS(close(fd), "Closing %s", sym);

	/* Compare names */
	T_ASSERT_EQ(strncmp(namebuf, namebuf2, strlen(namebuf)), 0, "Verifying %s was opened, got %s", namebuf, namebuf2);
}
