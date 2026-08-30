/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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

/* compile: xcrun -sdk macosx.internal clang -arch arm64e -arch x86_64 -ldarwintest -o chroot_unmount_stress chroot_unmount_stress.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <pthread.h>
#include <dispatch/dispatch.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define TEST_DURATION 10 /* seconds */

struct tmpfs_mount_args {
	uint64_t max_pages;
	ino64_t max_nodes;
	uint64_t options;
};

static char template[MAXPATHLEN];
static char *testdir = NULL;
static char chroot_path[MAXPATHLEN];
static int underfd = -1;
static struct stat targetsb;
static volatile int timeout = 0;
static volatile int should_stop = 0;
static volatile int error = 0;

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(true),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	should_stop = 1;

	if (underfd != -1) {
		close(underfd);
		underfd = -1;
	}

	if (chroot_path[0] != '\0') {
		(void)unmount(chroot_path, MNT_FORCE);
		(void)rmdir(chroot_path);
	}

	if (testdir) {
		rmdir(testdir);
	}
}

static void *
lookup_thread(void *arg)
{
	while (!should_stop && !timeout && !error) {
		struct stat sb;
		/* Try to stat ".." from within the chroot to trigger namei lookups */
		if (fstatat(underfd, "..", &sb, 0) == 0) {
			/* Check if we can access the parent directory outside chroot */
			/* This should normally fail or return the chroot root */
			if (sb.st_dev == targetsb.st_dev && sb.st_ino == targetsb.st_ino) {
				T_LOG("Detected chroot escape attempt");
				break;
			}
		}
	}
	return NULL;
}

static int
setup_chroot(void)
{
	/* Create the chroot directory */
	if (mkdir(chroot_path, 0755) != 0) {
		T_LOG("mkdir failed: %s", strerror(errno));
		return -1;
	}

	/* Get a file descriptor to the underlying directory before mounting */
	underfd = open(chroot_path, O_RDONLY);
	if (underfd == -1) {
		T_LOG("open failed: %s", strerror(errno));
		return -1;
	}

	/* Mount tmpfs over the directory */
	struct tmpfs_mount_args args = {
		.max_pages = 100,
		.max_nodes = 100,
		.options = 0,
	};

	if (mount("tmpfs", chroot_path, 0, &args) != 0) {
		T_LOG("mount failed: %s", strerror(errno));
		close(underfd);
		underfd = -1;
		return -1;
	}

	/* Change to the mounted directory and chroot */
	if (chdir(chroot_path) != 0) {
		T_LOG("chdir failed: %s", strerror(errno));
		close(underfd);
		underfd = -1;
		return -1;
	}

	if (chroot(chroot_path) != 0) {
		T_LOG("chroot failed: %s", strerror(errno));
		close(underfd);
		underfd = -1;
		return -1;
	}

	return 0;
}

static void
test_unmount_remount_cycle(void)
{
	struct tmpfs_mount_args args = {
		.max_pages = 100,
		.max_nodes = 100,
		.options = 0,
	};

	while (!should_stop && !timeout && !error) {
		/* Unmount the chroot */
		if (unmount("/", MNT_FORCE) != 0) {
			T_LOG("unmount failed: %s", strerror(errno));
			break;
		}

		/* Change to the underlying directory */
		if (fchdir(underfd) != 0) {
			T_LOG("fchdir failed: %s", strerror(errno));
			break;
		}

		/* Remount tmpfs */
		if (mount("tmpfs", ".", 0, &args) != 0) {
			T_LOG("remount failed: %s", strerror(errno));
			break;
		}

		/* Re-enter chroot */
		if (chroot(".") != 0) {
			T_LOG("chroot failed: %s", strerror(errno));
			break;
		}

		if (chdir("/") != 0) {
			T_LOG("chdir to / failed: %s", strerror(errno));
			break;
		}

		usleep(10000); /* 10ms delay between cycles */
	}
}

T_DECL(chroot_unmount_stress,
    "Test vnode reference counting race during chroot unmount")
{
	int64_t interval = TEST_DURATION * NSEC_PER_SEC;
	dispatch_queue_t queue;
	dispatch_source_t timeout_source;
	pthread_t threads[4];

	chroot_path[0] = '\0';

	T_ATEND(cleanup);

	T_SETUPBEGIN;

	T_ASSERT_NOTNULL((queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)), "Getting global queue");
	T_ASSERT_NOTNULL((timeout_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue)), "Creating dispatch source");

	dispatch_source_set_timer(timeout_source, dispatch_time(DISPATCH_TIME_NOW, interval), DISPATCH_TIME_FOREVER, 0);
	dispatch_source_set_event_handler(timeout_source, ^{
		timeout = 1;
		T_LOG("%d seconds timeout expired", TEST_DURATION);
	});

	snprintf(template, sizeof(template), "%s/chroot_unmount_stress-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");

	snprintf(chroot_path, sizeof(chroot_path), "%s/test_chroot", testdir);

	/* Get a reference to a directory outside the chroot for comparison */
	if (stat(testdir, &targetsb) != 0) {
		T_SKIP("Could not stat the testdir: %s", strerror(errno));
	}

	/* Set up the chroot environment */
	if (setup_chroot() != 0) {
		T_SKIP("Could not set up chroot environment");
	}

	T_LOG("Chroot setup complete, starting stress test");

	T_SETUPEND;

	T_LOG("Running for %d seconds", TEST_DURATION);
	dispatch_resume(timeout_source);

	/* Start multiple threads doing ".." lookups */
	for (int i = 0; i < 4; i++) {
		if (pthread_create(&threads[i], NULL, lookup_thread, NULL) != 0) {
			T_FAIL("Failed to create lookup thread %d", i);
			error = errno;
		}
	}

	/* Run the unmount/remount cycle that triggers the race in a separate thread */
	dispatch_async(queue, ^(void) {
		test_unmount_remount_cycle();
	});

	/* Wait for timeout or error */
	while (!timeout && !error) {
		usleep(100000); /* 100ms */
	}

	/* Signal threads to stop and wait for them */
	should_stop = 1;
	for (int i = 0; i < 4; i++) {
		pthread_join(threads[i], NULL);
	}

	T_ASSERT_POSIX_ZERO(error, "Test completed without error(s)");
	T_PASS("Stress test completed - race condition not reproduced or system remained stable");
}
