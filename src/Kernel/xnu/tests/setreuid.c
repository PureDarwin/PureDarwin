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
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <spawn_private.h>
#include <sys/aio.h>
#include <sys/spawn_internal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <signal.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.proc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("proc"),
	T_META_OWNER("s_musaev"),
	T_META_RUN_CONCURRENTLY(true));

#define UID (0xbadc0ffee)

static volatile int concurrent_setreuid_running = 0;

void
concurrent_setreuid_th1(void* arg __unused)
{
	while (concurrent_setreuid_running) {
		T_ASSERT_POSIX_SUCCESS(setreuid(1, 0), "setreuid");
		T_ASSERT_POSIX_SUCCESS(setreuid(UID, 0), "setreuid");
	}
}

void
concurrent_setreuid_th2(void* arg __unused)
{
	while (concurrent_setreuid_running) {
		T_ASSERT_POSIX_SUCCESS(setreuid(UID, 0), "setreuid");
	}
}

T_DECL(concurrent_setreuid,
    "Ensure concurrent calls to setreuid and setuid working correctly",
    T_META_ASROOT(true),
#if TARGET_OS_OSX
    T_META_ENABLED(true)
#else
    T_META_ENABLED(false)
#endif
    )
{
	pthread_t t1, t2;

	concurrent_setreuid_running = 1;
	pthread_create(&t1, NULL, (void*(*)(void*)) & concurrent_setreuid_th1, NULL);
	pthread_create(&t2, NULL, (void*(*)(void*)) & concurrent_setreuid_th2, NULL);

	sleep(1);
	concurrent_setreuid_running = 0;
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	unsigned long long uid = UID;
	size_t uid_size = sizeof(unsigned long long);
	int count = 0;
	size_t count_size = sizeof(unsigned long long);
	sysctlbyname("debug.test.proccnt_uid", &count, &count_size, &uid, uid_size);
	T_ASSERT_EQ(count, 1, "Expected to be exactly 1 proc (current proc) for given uid");
	T_PASS("INFO: passed test_concurrent_setreuid");
}
