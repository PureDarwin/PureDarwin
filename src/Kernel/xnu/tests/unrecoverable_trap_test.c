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

#include <darwintest.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/exception_types.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/code_signing.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <os/crashlog_private.h>

#include "exc_helpers.h"
#include "test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("arm"),
	T_META_OWNER("p_tennen")
	);

static size_t
exception_handler_expect_not_called(mach_port_t task __unused, mach_port_t thread __unused,
    exception_type_t type __unused, mach_exception_data_t codes __unused)
{
	T_ASSERT_FAIL("kernel ran exception handler instead of terminating process");
	return 0;
}

static void
signal_handler_expect_not_called(int sig, siginfo_t *sip __unused, void *ucontext __unused)
{
	T_FAIL("kernel dispatched signal handler instead of terminating process");
}

T_DECL(uncatchable_fatal_trap_developer_mode_disabled,
    "Ensure a maybe-unrecoverable trap label is uncatchable with !developer_mode",
    T_META_REQUIRES_SYSCTL_EQ("security.mac.amfi.developer_mode_status", 0),
    T_META_ENABLED(TARGET_CPU_ARM64)
    )
{
	/* Given a child process that sets up some mechanisms to catch an exception/signal */
	/* And developer mode is disabled and we're not being debugged */
	pid_t pid = fork();
	T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "fork");

	if (pid == 0) {
		/*
		 * Try to catch the exception in two ways:
		 * - via setting up a Mach exception handler, and
		 * - via sigaction
		 */
		mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
		run_exception_handler(exc_port, (exc_handler_callback_t)exception_handler_expect_not_called);

		struct sigaction sa = {
			.sa_sigaction = signal_handler_expect_not_called,
			.sa_flags = SA_SIGINFO
		};
		sigfillset(&sa.sa_mask);

		T_ASSERT_POSIX_ZERO(sigaction(SIGILL, &sa, NULL), NULL);

		/* When the child issues a maybe-fatal trap label */
		/* 0xB000 is the start of the 'runtimes-owned traps' range in xnu */
		os_fatal_trap(0xB000);
		/* The brk above should have been treated as unrecoverable by the kernel */
		T_FAIL("child ran past unrecoverable brk");
	} else {
		int status;
		int err = waitpid(pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "waitpid");

		/* Then the child does not have an opportunity to run its exception handlers, and is immediately killed */
		T_EXPECT_TRUE(WIFSIGNALED(status), "child terminated due to signal");
		T_EXPECT_EQ(SIGKILL, WTERMSIG(status), "child terminated due to SIGKILL");
	}
}

T_DECL(uncatchable_fatal_trap_developer_mode_enabled,
    "Ensure an maybe-unrecoverable trap label is uncatchable with developer_mode",
    T_META_REQUIRES_SYSCTL_EQ("security.mac.amfi.developer_mode_status", 1),
    T_META_ENABLED(TARGET_CPU_ARM64)
    )
{
	/* Given a child process that sets up some mechanisms to catch an exception/signal */
	/* And developer mode is enabled, but we're not being debugged */
	pid_t pid = fork();
	T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "fork");

	if (pid == 0) {
		/*
		 * Try to catch the exception in two ways:
		 * - via setting up a Mach exception handler, and
		 * - via sigaction
		 */
		mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
		run_exception_handler(exc_port, (exc_handler_callback_t)exception_handler_expect_not_called);

		struct sigaction sa = {
			.sa_sigaction = signal_handler_expect_not_called,
			.sa_flags = SA_SIGINFO
		};
		sigfillset(&sa.sa_mask);

		T_ASSERT_POSIX_ZERO(sigaction(SIGILL, &sa, NULL), NULL);

		/* When the child issues a maybe-fatal trap label */
		/* 0xB000 is the start of the 'runtimes-owned traps' range in xnu */
		os_fatal_trap(0xB000);
		/* The brk above should have been treated as unrecoverable by the kernel */
		T_FAIL("child ran past brk");
	} else {
		int status;
		int err = waitpid(pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "waitpid");

		/* Then the child does not have an opportunity to run its exception handlers, and is immediately killed */
		T_EXPECT_TRUE(WIFSIGNALED(status), "child terminated due to signal");
		T_EXPECT_EQ(SIGKILL, WTERMSIG(status), "child terminated due to SIGKILL");
	}
}

static bool* shared_was_mach_exception_handler_called = NULL;
static bool* shared_was_posix_signal_handler_called = NULL;

static size_t
exception_handler_expect_called(mach_port_t task __unused, mach_port_t thread __unused,
    exception_type_t type __unused, mach_exception_data_t codes __unused)
{
	T_PASS("Our Mach exception handler ran");
	*shared_was_mach_exception_handler_called = true;
	exit(0);
	return 0;
}

static void
signal_handler_expect_called(int sig, siginfo_t *sip __unused, void *ucontext __unused)
{
	T_PASS("Our BSD signal handler ran");
	*shared_was_posix_signal_handler_called = true;
	exit(0);
}


T_DECL(uncatchable_fatal_trap_debugged,
    "Ensure an maybe-unrecoverable trap label is catchable under a debugger",
    T_META_REQUIRES_SYSCTL_EQ("security.mac.amfi.developer_mode_status", 1),
    /* It's not straightforward to ptrace on platforms other than macOS, so don't bother */
    // T_META_ENABLED(TARGET_CPU_ARM64 && TARGET_OS_OSX)
    T_META_ENABLED(false) /* rdar://153223014 */
    )
{
	/* Given a child process that sets up some mechanisms to catch an exception/signal */
	/* And developer mode is enabled, and the child is being debugged */
	int ret;

	const char* memory_path = "uncatchable_fatal_trap_debugged";
	shm_unlink(memory_path);
	int shm_fd = shm_open(memory_path, O_RDWR | O_CREAT);
	T_ASSERT_POSIX_SUCCESS(shm_fd, "Created shared memory");
	ret = ftruncate(shm_fd, sizeof(bool) * 2);
	T_ASSERT_POSIX_SUCCESS(ret, "ftruncate");

	shared_was_mach_exception_handler_called = (bool*)mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	shared_was_posix_signal_handler_called = (bool*)mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	bool* has_parent_connected = (bool*)mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	*has_parent_connected = false;

	pid_t pid = fork();
	T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "fork");

	if (pid == 0) {
		/* Allow the parent to attach */
		while (!*has_parent_connected) {
			sleep(1);
		}

		/*
		 * Try to catch the exception in two ways:
		 * - via setting up a Mach exception handler, and
		 * - via sigaction
		 */
		mach_port_t exc_port = create_exception_port(EXC_MASK_ALL);
		run_exception_handler(exc_port, (exc_handler_callback_t)exception_handler_expect_called);

		struct sigaction sa = {
			.sa_sigaction = signal_handler_expect_called,
			.sa_flags = SA_SIGINFO
		};
		sigfillset(&sa.sa_mask);

		T_ASSERT_POSIX_ZERO(sigaction(SIGILL, &sa, NULL), NULL);

		/* When the child issues a maybe-fatal trap label */
		/* 0xB000 is the start of the 'runtimes-owned traps' range in xnu */
		os_fatal_trap(0xB000);
		/* The brk above should have terminated this thread */
		T_FAIL("child ran past brk");
	} else {
		/* Attach to the child so it's marked as being debugged */
		ret = ptrace(PT_ATTACHEXC, pid, 0, 0);
		T_EXPECT_POSIX_SUCCESS(ret, "ptrace PT_ATTACHEXC");
		ret = ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);
		T_EXPECT_POSIX_SUCCESS(ret, "ptrace PT_CONTINUE");
		/* And let the child know that it can carry on */
		*has_parent_connected = true;

		int status;
		int err = waitpid(pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "waitpid");

		/*
		 * Then the child is given an opportunity to run its exception handlers,
		 * which we witness by its setting of a shared boolean and clean exit(0).
		 */
		T_EXPECT_TRUE(WIFEXITED(status), "child exited");
		T_EXPECT_TRUE(*shared_was_mach_exception_handler_called
		    || *shared_was_posix_signal_handler_called,
		    "Expected one of our handlers to be dispatched");

		T_ASSERT_POSIX_SUCCESS(close(shm_fd), "Closed shm fd");
		T_ASSERT_POSIX_SUCCESS(shm_unlink(memory_path), "Unlinked");
	}
}
