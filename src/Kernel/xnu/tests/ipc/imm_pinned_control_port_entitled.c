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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <darwintest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>
#include <spawn.h>
#include <signal.h>
#include <TargetConditionals.h>

#define TASK_EXC_GUARD_MP_DELIVER 0x10

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_TAG_VM_PREFERRED);

static void
test_task_thread_port_values(void)
{
	T_LOG("Compare various task/thread control port values");
	kern_return_t kr;
	mach_port_t port, th_self;
	thread_array_t threadList;
	mach_msg_type_number_t threadCount = 0;
	boolean_t found_self = false;
	processor_set_name_array_t psets;
	processor_set_t        pset_priv;
	task_array_t taskList;
	mach_msg_type_number_t pcnt = 0, tcnt = 0;
	mach_port_t host = mach_host_self();

	/* Compare with task/thread_get_special_port() */
	kr = task_get_special_port(mach_task_self(), TASK_KERNEL_PORT, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "task_get_special_port() - TASK_KERNEL_PORT");
	T_EXPECT_EQ(port, mach_task_self(), "TASK_KERNEL_PORT should match mach_task_self()");
	mach_port_deallocate(mach_task_self(), port);

	kr = task_for_pid(mach_task_self(), getpid(), &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "task_for_pid()");
	T_EXPECT_EQ(port, mach_task_self(), "task_for_pid(self) should match mach_task_self()");
	mach_port_deallocate(mach_task_self(), port);

	th_self = mach_thread_self();
	kr = thread_get_special_port(th_self, THREAD_KERNEL_PORT, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "thread_get_special_port() - THREAD_KERNEL_PORT");
	T_EXPECT_EQ(port, th_self, "THREAD_KERNEL_PORT should match mach_thread_self()");
	mach_port_deallocate(mach_task_self(), port);

	/* Make sure task_threads() return immovable thread ports */
	kr = task_threads(mach_task_self(), &threadList, &threadCount);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "task_threads()");
	T_QUIET; T_ASSERT_GE(threadCount, 1, "should have at least 1 thread");

	for (size_t i = 0; i < threadCount; i++) {
		if (th_self == threadList[i]) { /* th_self is immovable */
			found_self = true;
			break;
		}
	}

	T_EXPECT_TRUE(found_self, "task_threads() should return thread self");

	for (size_t i = 0; i < threadCount; i++) {
		mach_port_deallocate(mach_task_self(), threadList[i]);
	}

	if (threadCount > 0) {
		mach_vm_deallocate(mach_task_self(),
		    (mach_vm_address_t)threadList,
		    threadCount * sizeof(mach_port_t));
	}

	kr = mach_port_deallocate(mach_task_self(), th_self);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_deallocate");

	/* Make sure processor_set_tasks() return immovable task self */
	kr = host_processor_sets(host, &psets, &pcnt);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "host_processor_sets");
	T_QUIET; T_ASSERT_GE(pcnt, 1, "should have at least 1 processor set");

	kr = host_processor_set_priv(host, psets[0], &pset_priv);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "host_processor_set_priv");
	for (size_t i = 0; i < pcnt; i++) {
		kr = mach_port_deallocate(mach_task_self(), psets[i]);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_deallocate");
	}
	kr = mach_port_deallocate(mach_task_self(), host);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_deallocate");
	kr = vm_deallocate(mach_task_self(), (vm_address_t)psets, (vm_size_t)pcnt * sizeof(mach_port_t));
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_deallocate");

	kr = processor_set_tasks_with_flavor(pset_priv, TASK_FLAVOR_CONTROL, &taskList, &tcnt);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "processor_set_tasks_with_flavor");
	T_QUIET; T_ASSERT_GE(tcnt, 1, "should have at least 1 task");
	kr = mach_port_deallocate(mach_task_self(), pset_priv);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_deallocate");

	found_self = false;
	for (size_t i = 0; i < tcnt; i++) {
		if (taskList[i] == mach_task_self()) {
			found_self = true;
			break;
		}
	}

	T_EXPECT_TRUE(found_self, "processor_set_tasks() should return immovable task self");

	for (size_t i = 0; i < tcnt; i++) {
		kr = mach_port_deallocate(mach_task_self(), taskList[i]);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_deallocate");
	}

	if (tcnt > 0) {
		kr = mach_vm_deallocate(mach_task_self(),
		    (mach_vm_address_t)taskList,
		    tcnt * sizeof(mach_port_t));
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_deallocate");
	}
}

T_DECL(task_thread_port_values,
    "Verify port names of mach_{task, thread}_self(), {TASK, THREAD}_KERNEL_PORT, \
	and ports returned from task_threads() and processor_set_tasks().",
    T_META_CHECK_LEAKS(false))
{
	uint32_t task_exc_guard = 0;
	size_t te_size = sizeof(&task_exc_guard);
	uint32_t opts = 0;
	size_t size = sizeof(&opts);

	T_SETUPBEGIN;

	T_LOG("Check if task_exc_guard exception has been enabled");
	int ret = sysctlbyname("kern.task_exc_guard_default", &task_exc_guard, &te_size, NULL, 0);
	T_ASSERT_EQ(ret, 0, "sysctl to check exc_guard config");

	if (!(task_exc_guard & TASK_EXC_GUARD_MP_DELIVER)) {
		T_SKIP("task_exc_guard exception is not enabled");
	}

	T_LOG("Check if immovable control port has been enabled");
	ret = sysctlbyname("kern.ipc_control_port_options", &opts, &size, NULL, 0);

	if (!ret && (opts & 0x8) != 0x8) {
		T_SKIP("hard immovable control port isn't enabled");
	}

	T_SETUPEND;

	test_task_thread_port_values();
}
