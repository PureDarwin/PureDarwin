/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <excserver.h>
#include <sys/mman.h>
#include <kern/exc_resource.h>
#include <TargetConditionals.h>
#include <mach/vm_page_size.h>

#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>

/* internal */
#include <spawn_private.h>
#include <sys/kern_memorystatus.h>

#define TEST_MEMLIMIT_MB 10
#define SEM_TIMEOUT dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC)
#define REARM_TIMES 5

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.memorystatus"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_OWNER("aaron_j_sonin"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_BOOTARGS_SET("memstat_no_task_limit_increase=1"));

/* Globals */
static dispatch_semaphore_t sync_sema;
static pid_t child_pid;

/* Exception  */
kern_return_t
catch_mach_exception_raise(mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count)
{
#pragma unused(exception_port, thread, task, code_count)
	if (exception != EXC_RESOURCE) {
		T_LOG("Received unknown exception %d\n", exception);
		return KERN_FAILURE;
	}

	mach_exception_data_type_t resource = EXC_RESOURCE_DECODE_RESOURCE_TYPE(code[0]);
	mach_exception_data_type_t flavor = EXC_RESOURCE_DECODE_FLAVOR(code[0]);

	if (resource != RESOURCE_TYPE_MEMORY) {
		T_LOG("Received EXC_RESOURCE, but not for memory");
		return KERN_FAILURE;
	}

	if (flavor != FLAVOR_HIGH_WATERMARK) {
		T_LOG("Received EXC_RESOURCE, but not high watermark");
		return KERN_FAILURE;
	}

	T_LOG("Received memory high watermark EXC_RESOURCE!\n");
	dispatch_semaphore_signal(sync_sema);
	return KERN_SUCCESS;
}

/* Unused, but necessary to link w/ excserver */
kern_return_t
catch_mach_exception_raise_state(mach_port_t exception_port,
    exception_type_t exception,
    const mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int * flavor,
    const thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t * new_state_count)
{
#pragma unused(exception_port, exception, code, code_count, flavor, old_state, old_state_count, new_state, new_state_count)
	T_FAIL("Unsupported catch_mach_exception_raise_state");
	return KERN_NOT_SUPPORTED;
}

/* Unused, but necessary to link w/ excserver */
kern_return_t
catch_mach_exception_raise_state_identity(mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    int * flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t * new_state_count)
{
#pragma unused(exception_port, thread, task, exception, code, code_count, flavor, old_state, old_state_count, new_state, new_state_count)
	T_FAIL("Unsupported catch_mach_exception_raise_state_identity");
	return KERN_NOT_SUPPORTED;
}

void
eat_memory(int num_pages)
{
	int ret;
	int i, j;
	unsigned char *buf;

	for (i = 0; i < REARM_TIMES; i++) {
		/* Allocate and touch all our pages */
		T_LOG("Allocating %d pages...", num_pages);
		buf = mmap(NULL, vm_page_size * num_pages, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(buf, "mmap");
		for (j = 0; j < num_pages; j++) {
			((volatile unsigned char *)buf)[j * vm_page_size] = 1;
		}

		/* Free them, hopefully putting us back under the limit */
		T_LOG("Freeing...");
		munmap((void*) ((size_t) buf + vm_page_size), vm_page_size * (num_pages - 1));

		/* Re-arm EXC_RESOURCE */
		ret = memorystatus_control(
			MEMORYSTATUS_CMD_REARM_MEMLIMIT,
			getpid(),
			MEMORYSTATUS_FLAGS_REARM_ACTIVE | MEMORYSTATUS_FLAGS_REARM_INACTIVE,
			NULL, 0);
		T_QUIET; T_ASSERT_POSIX_ZERO(ret, "memorystatus_control(MEMORYSTATUS_CMD_REARM_MEMLIMIT)");
	}

	exit(0);
}

/*
 * Background process that will allocate enough memory to push
 * itself over the threshold, hopefully triggering EXC_RESOURCE.
 */
T_HELPER_DECL(memory_enjoyer, "") {
	int ret;
	sig_t sig;
	dispatch_source_t dispatch;
	int num_pages = 0;

	if (argc == 1) {
		num_pages = atoi(argv[0]);
	}

	/* Use dispatch to wait for the signal from our parent to start eating memory */
	sig = signal(SIGUSR1, SIG_IGN);
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NE(sig, SIG_ERR, "signal(SIGUSR1)");
	dispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(dispatch, "dispatch_source_create");
	dispatch_source_set_event_handler(dispatch, ^{
		eat_memory(num_pages);
	});
	dispatch_activate(dispatch);

	/* Signal parent that we're ready */
	ret = kill(getppid(), SIGUSR1);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "signal parent");

	dispatch_main();
}

static void
kill_child(void)
{
	int ret = kill(child_pid, SIGKILL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kill");
}

static pid_t
launch_child(int num_pages)
{
	extern char **environ;
	int ret;
	pid_t pid;
	char testpath[PATH_MAX];
	posix_spawnattr_t spawn_attrs;

	uint32_t testpath_buf_size = PATH_MAX;
	char num_pages_str[32] = {0};
	char *argv[5] = {testpath, "-n", "memory_enjoyer", num_pages_str, NULL};

	T_LOG("Spawning child process...");

	ret = posix_spawnattr_init(&spawn_attrs);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawnattr_init");
	ret = posix_spawnattr_setjetsam_ext(&spawn_attrs, 0, JETSAM_PRIORITY_FOREGROUND, TEST_MEMLIMIT_MB, TEST_MEMLIMIT_MB);

	ret = snprintf(num_pages_str, sizeof(num_pages_str), "%d", num_pages);
	T_QUIET; T_ASSERT_LE((size_t) ret, sizeof(num_pages_str), "Don't allocate too many pages.");
	ret = _NSGetExecutablePath(testpath, &testpath_buf_size);
	T_QUIET; T_ASSERT_EQ(ret, 0, "_NSGetExecutablePath");
	ret = posix_spawn(&pid, argv[0], NULL, &spawn_attrs, argv, environ);
	T_QUIET; T_ASSERT_POSIX_ZERO(ret, "posix_spawn");

	T_ATEND(kill_child);

	return pid;
}

static void *
exc_handler_thread(void * arg)
{
#pragma unused(arg)
	kern_return_t kret;
	mach_port_t exception_port;

	/* Set up our exception port. */

	kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exception_port);
	if (kret != KERN_SUCCESS) {
		T_FAIL("mach_port_allocate: %s (%d)", mach_error_string(kret), kret);
	}

	kret = mach_port_insert_right(mach_task_self(), exception_port, exception_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kret != KERN_SUCCESS) {
		T_FAIL("mach_port_insert_right: %s (%d)", mach_error_string(kret), kret);
	}

	kret = task_set_exception_ports(mach_task_self(), EXC_MASK_RESOURCE, exception_port,
	    (exception_behavior_t)(EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES), 0);
	if (kret != KERN_SUCCESS) {
		T_FAIL("task_set_exception_ports: %s (%d)", mach_error_string(kret), kret);
	}

	dispatch_semaphore_signal(sync_sema);

	kret = mach_msg_server(mach_exc_server, MACH_MSG_SIZE_RELIABLE, exception_port, 0);
	if (kret != KERN_SUCCESS) {
		T_FAIL("mach_msg_server: %s (%d)", mach_error_string(kret), kret);
	}

	return NULL;
}

T_DECL(memorylimit_exception_tests, "EXC_RESOURCE re-arming",
    T_META_ASROOT(true),
    T_META_TAG_VM_PREFERRED,
    T_META_ENABLED(!TARGET_OS_OSX)
    )
{
	int num_pages;
	long dispatch_err;
	sig_t sig_ret;
	dispatch_source_t dispatch;
	pthread_t handle_thread;

	T_SETUPBEGIN;

	sync_sema = dispatch_semaphore_create(0);

	/* Start our exception handling thread */
	T_ASSERT_POSIX_ZERO(pthread_create(&handle_thread, NULL, exc_handler_thread, NULL), "pthread_create");
	dispatch_err = dispatch_semaphore_wait(sync_sema, SEM_TIMEOUT);
	T_QUIET; T_ASSERT_EQ(dispatch_err, 0L, "dispatch_semaphore_wait");

	/* Make sure we handle SIGUSR1 */
	sig_ret = signal(SIGUSR1, SIG_IGN);
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NE(sig_ret, SIG_ERR, "signal(SIGUSR1)");

	/*
	 * When we receive SIGUSR1 from our child (to indicate that it's ready), send it a
	 * SIGUSR1 back to indicate that we're ready (i.e. we've attached to the child).
	 * Then, wait for EXC_RESOURCE to happen REARM_TIMES times.
	 */
	dispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(dispatch, "dispatch_source_create");
	dispatch_source_set_event_handler(dispatch, ^{
		int ret;

		/* Attach to child */
		ret = ptrace(PT_ATTACHEXC, child_pid, NULL, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "ptrace");

		/* Tell child we're ready */
		kill(child_pid, SIGUSR1);

		/* Wait for EXC_RESOURCEs to be delivered */
		for (int i = 0; i < REARM_TIMES; i++) {
		        long dispatch_err = dispatch_semaphore_wait(sync_sema, SEM_TIMEOUT);
		        T_QUIET; T_ASSERT_EQ(dispatch_err, 0L, "Received EXC_RESOURCE");
		}
		T_END;
	});
	dispatch_activate(dispatch);

	/* Spawn child and attach to it  */
	num_pages = (TEST_MEMLIMIT_MB * (1 << 20)) / vm_page_size;
	child_pid = launch_child(num_pages);

	T_SETUPEND;

	dispatch_main();
}
