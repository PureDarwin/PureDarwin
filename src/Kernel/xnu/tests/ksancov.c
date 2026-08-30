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

#include <darwintest.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <IOKit/IOKitLib.h>
#include "test_utils.h"

// Include the KSANCOV tool header
#include "../san/tools/ksancov.h"

T_GLOBAL_META(
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("kasan"),
	T_META_OWNER("a_fioraldi")
	);

T_DECL(
	kcov_ksancov_mode_trace,
	"Ensure that KSANCOV can trace program counters when the kernel is compiled with instrumentation (KSANCOV=1)",
	// Only relevant on KASAN kernels with KCOV available
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.available", 1),
	// Only relevant on KASAN kernels compiled with SanitizerCoverage instrumentation
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.sancov_compiled", 1)
	) {
	int fd = ksancov_open();
	T_ASSERT_POSIX_SUCCESS(fd, "ksancov_open");

	size_t max_entries = 64UL * 1024;
	int ret = ksancov_mode_trace(fd, max_entries);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_mode_trace");

	uintptr_t addr;
	size_t sz;
	ret = ksancov_map(fd, &addr, &sz);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_map");

	ksancov_trace_t *trace = (ksancov_trace_t *)addr;

	ret = ksancov_thread_self(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_thread_self");

	ksancov_reset(trace);
	ksancov_start(trace);

	// trace the getppid syscall
	getppid();

	ksancov_stop(trace);

	// ksancov_trace_* contain useful assertions
	size_t head = ksancov_trace_head(trace);
	T_EXPECT_GT(head, (size_t)0, "Expected to have at least 1 program counter in the coverage trace");
	if (head > 0) {
		ksancov_trace_entry(trace, 0);
	}

	ret = close(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "close");
}

T_DECL(
	kcov_ksancov_mode_trace_kext,
	"Ensure that KSANCOV can trace program counters of a kext bundle",
	// Only relevant on KASAN kernels with KCOV available
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.available", 1),
	// Can enable coverage collection of KEXTs only when on demand KSANCOV is enabled
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.od.support_enabled", 1),
	// Enable this test only on iPhone rdar://155524478
	T_META_ENABLED(TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_SIMULATOR)
	) {
	io_service_t io_surface = IO_OBJECT_NULL;
	io_surface = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOSurfaceRoot"));
	T_ASSERT_NE(io_surface, IO_OBJECT_NULL, "Expected to find the IOSurface service");

	int fd = ksancov_open();
	T_ASSERT_POSIX_SUCCESS(fd, "ksancov_open");

	size_t max_entries = 64UL * 1024;
	int ret = ksancov_mode_trace(fd, max_entries);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_mode_trace");

	uintptr_t addr;
	size_t sz;
	ret = ksancov_map(fd, &addr, &sz);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_map");

	ksancov_trace_t *trace = (ksancov_trace_t *)addr;

	ret = ksancov_thread_self(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_thread_self");

	ret = ksancov_on_demand_set_gate(fd, "com.apple.iokit.IOSurface", true);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_on_demand_set_gate");
	ksancov_reset(trace);
	ksancov_start(trace);

	// trace the IOSurface IOServiceOpen/Close
	io_connect_t connect = IO_OBJECT_NULL;
	IOReturn ioret;
	ioret = IOServiceOpen(io_surface, mach_task_self(), 0, &connect);
	T_EXPECT_EQ(ioret, kIOReturnSuccess, "Unable to open the IOSurface userclient");
	if (ioret == kIOReturnSuccess) {
		IOServiceClose(connect);
	}

	ksancov_stop(trace);
	ret = ksancov_on_demand_set_gate(fd, "com.apple.iokit.IOSurface", false);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_on_demand_set_gate");

	// ksancov_trace_* contain useful assertions
	size_t head = ksancov_trace_head(trace);
	T_EXPECT_GT(head, (size_t)0, "Expected to have at least 1 program counter in the coverage trace");
	if (head > 0) {
		ksancov_trace_entry(trace, 0);
	}

	ret = close(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "close");
}

T_DECL(
	kcov_ksancov_mode_trace_cmp_kext,
	"Ensure that KSANCOV can trace program counters and comparisons of a kext bundle",
	// Only relevant on KASAN kernels with KCOV available
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.available", 1),
	// Can enable coverage collection of KEXTs only when on demand KSANCOV is enabled
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.od.support_enabled", 1),
	// Enable this test only on iPhone rdar://155524478
	T_META_ENABLED(TARGET_OS_IOS && !TARGET_OS_MACCATALYST && !TARGET_OS_SIMULATOR)
	) {
	io_service_t io_surface = IO_OBJECT_NULL;
	io_surface = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOSurfaceRoot"));
	T_ASSERT_NE(io_surface, IO_OBJECT_NULL, "Expected to find the IOSurface service");

	int fd = ksancov_open();
	T_ASSERT_POSIX_SUCCESS(fd, "ksancov_open");

	size_t max_entries = 64UL * 1024;
	int ret = ksancov_mode_trace(fd, max_entries);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_mode_trace");

	uintptr_t addr;
	size_t sz;
	ret = ksancov_map(fd, &addr, &sz);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_map");

	ksancov_trace_t *trace = (ksancov_trace_t *)addr;

	ret = ksancov_cmps_mode_trace(fd, max_entries, true);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_cmps_mode_trace");

	ret = ksancov_cmps_map(fd, &addr, &sz);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_cmps_map");

	ksancov_trace_t *cmps_trace = (ksancov_trace_t *)addr;

	ret = ksancov_thread_self(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_thread_self");

	ret = ksancov_on_demand_set_gate(fd, "com.apple.iokit.IOSurface", true);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_on_demand_set_gate");
	ksancov_reset(trace);
	ksancov_reset(cmps_trace);
	ksancov_start(trace);
	ksancov_start(cmps_trace);

	// trace the IOSurface IOServiceOpen/Close
	io_connect_t connect = IO_OBJECT_NULL;
	IOReturn ioret;
	ioret = IOServiceOpen(io_surface, mach_task_self(), 0, &connect);
	T_EXPECT_EQ(ioret, kIOReturnSuccess, "Unable to open the IOSurface userclient");
	if (ioret == kIOReturnSuccess) {
		IOServiceClose(connect);
	}

	ksancov_stop(trace);
	ksancov_stop(cmps_trace);
	ret = ksancov_on_demand_set_gate(fd, "com.apple.iokit.IOSurface", false);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_on_demand_set_gate");

	// ksancov_trace_* contain useful assertions
	size_t head = ksancov_trace_head(trace);
	T_EXPECT_GT(head, (size_t)0, "Expected to have at least 1 program counter in the coverage trace");
	if (head > 0) {
		ksancov_trace_entry(trace, 0);
	}

	head = ksancov_trace_head(cmps_trace);
	T_EXPECT_GT(head, (size_t)0, "Expected to have at least 1 comparison in the comparisons trace");

	size_t i;
	for (i = 0; i < head;) {
		ksancov_cmps_trace_ent_t *entry = ksancov_cmps_trace_entry(cmps_trace, i);
		if (KCOV_CMP_IS_FUNC(entry->type)) {
			size_t space = ksancov_cmps_trace_func_space(entry->len1_func, entry->len2_func);
			i += space / sizeof(ksancov_cmps_trace_ent_t);
		} else {
			++i;
		}
	}
	T_EXPECT_EQ(i, head, "Cursor went past the end of the trace");

	ret = close(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "close");
}

T_DECL(
	kcov_ksancov_testcases,
	"Test the testcases buffer feature in KSANCOV",
	// Only relevant on KASAN kernels with KCOV available
	T_META_REQUIRES_SYSCTL_EQ("kern.kcov.available", 1)
	) {
	int fd = ksancov_open();
	if (fd < 0) {
		T_SKIP("Ksancov not available, skipping...");
		return;
	}

	size_t max_entries = 32;
	int ret = ksancov_mode_trace(fd, max_entries);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_mode_trace");

	size_t num_testcases = 2;
	ret = ksancov_testcases(fd, num_testcases);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_testcases");

	uintptr_t addr;
	size_t sz;
	ret = ksancov_testcases_map(fd, &addr, &sz);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_testcases_map");

	T_EXPECT_EQ(sz, sizeof(ksancov_serialized_testcases_t) + sizeof(ksancov_serialized_testcase_t) * num_testcases, "Testcases mapped size mismatch");
	ksancov_serialized_testcases_t *testcases = (ksancov_serialized_testcases_t *)addr;

	ret = ksancov_thread_self(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "ksancov_thread_self");

	T_ASSERT_EQ(sizeof(testcases->list[testcases->head].buffer), (size_t)KSANCOV_SERIALIZED_TESTCASE_BYTES, "Buffer size not KSANCOV_SERIALIZED_TESTCASE_BYTES");
	testcases->list[testcases->head].buffer[0] = 'f';
	testcases->list[testcases->head].buffer[1] = 'o';
	testcases->list[testcases->head].buffer[2] = 'o';
	testcases->list[testcases->head].buffer[3] = 0;
	testcases->list[testcases->head].size = 4;
	testcases->head = (testcases->head + 1) % num_testcases;

	ret = ksancov_testcases_log(fd);
	// The serial log can fail if serial=0x1 or debug=0x2 are not set in the boot args

	ret = close(fd);
	T_ASSERT_POSIX_SUCCESS(ret, "close");
}
