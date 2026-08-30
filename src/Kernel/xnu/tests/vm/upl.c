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
 *
 */

#include <darwintest.h>
#include <darwintest_utils.h>

#include <os/thread_self_restrict.h>

#include <stdlib.h>
#include <sys/mman.h>

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include <System/machine/cpu_capabilities.h>

#include "exc_guard_helper.h"
#include "test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_OWNER("jharmening"),
	T_META_CHECK_LEAKS(false),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true));

typedef struct {
	uint64_t ptr;
	uint32_t size;
	char test_pattern;
	bool copy_expected;
	bool should_fail;
	bool upl_rw;
} upl_test_args;

T_DECL(vm_upl_ro_on_rw,
    "Generate RO UPL against RW memory region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'a',
		               .copy_expected = false, .should_fail = false, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x800;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x1000;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_ro_on_ro,
    "Generate RO UPL against RO memory region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	T_QUIET; T_ASSERT_POSIX_SUCCESS(mprotect(buf, buf_size, PROT_READ), "mprotect");

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'a',
		               .copy_expected = false, .should_fail = false, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x800;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x1000;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_rw_on_rw,
    "Generate RW UPL against RW memory region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'b',
		               .copy_expected = false, .should_fail = false, .upl_rw = true };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		T_QUIET; T_ASSERT_EQ(buf[i], (unsigned int)'b' + i,
		    "buf[%u]='%u' == '%u'",
		    i, buf[i], (unsigned int)'b' + i);
	}
	bzero(buf, buf_size);
	args.ptr = (uint64_t)buf + 0x800;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		if ((i < (0x800 / sizeof(*buf))) || (i >= ((0x800 + args.size) / sizeof(*buf)))) {
			T_QUIET; T_ASSERT_EQ(buf[i], 0,
			    "buf[%u]='%u' == 0", i, buf[i]);
		} else {
			T_QUIET; T_ASSERT_EQ(buf[i], (unsigned int)'b' + i - (unsigned int)(0x800 / sizeof(*buf)),
			    "buf[%u]='%u' == '%u'",
			    i, buf[i], (unsigned int)'b' + i - (unsigned int)(0x800 / sizeof(*buf)));
		}
	}

	bzero(buf, buf_size);
	args.ptr = (uint64_t)buf + 0x1000;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		if ((i < (0x1000 / sizeof(*buf))) || (i >= ((0x1000 + args.size) / sizeof(*buf)))) {
			T_QUIET; T_ASSERT_EQ(buf[i], 0,
			    "buf[%u]='%u' == 0", i, buf[i]);
		} else {
			T_QUIET; T_ASSERT_EQ(buf[i], (unsigned int)'b' + i - (unsigned int)(0x1000 / sizeof(*buf)),
			    "buf[%u]='%u' == '%u'",
			    i, buf[i], (unsigned int)'b' + i - (unsigned int)(0x1000 / sizeof(*buf)));
		}
	}

	munmap(buf, buf_size);
}

T_DECL(vm_upl_rw_on_ro,
    "Generate RW UPL against RO memory region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	T_QUIET; T_ASSERT_POSIX_SUCCESS(mprotect(buf, buf_size, PROT_READ), "mprotect");

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'b',
		               .copy_expected = false, .should_fail = true, .upl_rw = true };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x800;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x1000;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_ro_on_rx,
    "Generate RO UPL against RX memory region")
{
	bool copy_expected = true;
#if TARGET_OS_OSX
	/**
	 * For embedded targets, UPL creation against RX mappings should always produce a copy due to codesigning.
	 * For MacOS, a copy should only be produced if the SPTM is enabled, due to the SPTM's stricter requirements
	 * for DMA mappings of executable frame types.
	 */
	if (!is_sptm_enabled()) {
		copy_expected = false;
	}
#endif /* TARGET_OS_OSX */

	upl_test_args args = { .ptr = (uint64_t)__builtin_return_address(0), .size = PAGE_SIZE, .test_pattern = 'a',
		               .copy_expected = copy_expected, .should_fail = false, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr += 0x100;
	args.size -= 0x200;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");
}

T_DECL(vm_upl_rw_on_rx,
    "Generate RW UPL against RX memory region")
{
	upl_test_args args = { .ptr = (uint64_t)__builtin_return_address(0), .size = PAGE_SIZE, .test_pattern = 'a',
		               .copy_expected = true, .should_fail = true, .upl_rw = true };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr += 0x100;
	args.size -= 0x200;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");
}

T_DECL(vm_upl_ro_on_jit,
    "Generate RO UPL against JIT memory region")
{
	if (!is_map_jit_allowed()) {
		T_SKIP("MAP_JIT not allowed for this system configuration");
	}
	/**
	 * Direct RO UPLs against JIT pages should be allowed for non-SPTM targets.
	 * For SPTM targets, a copy is expected due to the SPTM's stricter requirements for DMA
	 * mappings of executable frame types.
	 */
	bool copy_expected = is_sptm_enabled();
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	if (os_thread_self_restrict_rwx_is_supported()) {
		os_thread_self_restrict_rwx_to_rw();
	}

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'b',
		               .copy_expected = copy_expected, .should_fail = false, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x800;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	args.ptr = (uint64_t)buf + 0x1000;
	args.size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_rw_on_jit,
    "Generate RW UPL against JIT memory region")
{
	if (process_is_translated()) {
		/* TODO: Remove this once rdar://142438840 is fixed. */
		T_SKIP("Guard exception handling does not work correctly with Rosetta (rdar://142438840), skipping...");
	}
	if (!is_map_jit_allowed()) {
		T_SKIP("MAP_JIT not allowed for this system configuration");
	}
	const size_t buf_size = 10 * PAGE_SIZE;
	/**
	 * Direct RW UPLs against JIT pages should be allowed for non-SPTM targets.
	 * For SPTM targets, UPL creation should fail due to the SPTM's stricter requirements for DMA
	 * mappings of executable frame types.
	 */
	bool should_fail = is_sptm_enabled();
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	upl_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .test_pattern = 'b',
		               .copy_expected = false, .should_fail = should_fail, .upl_rw = true };

	__block int64_t addr = (int64_t)&args;
	__block int64_t result = 0;
	__block size_t s = sizeof(result);

	/* Ensure that guard exceptions will not be fatal to the test process. */
	enable_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY);

	/**
	 * Iterate 3 times to guarantee buffer offsets that are neither 4K nor 16K aligned,
	 * and 4K but not necessarily 16K aligned.
	 */
	for (int i = 0; i < 2; i++) {
		exc_guard_helper_info_t exc_info;
		bool caught_exception =
		    block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
			T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
			"sysctlbyname(debug.test.vm_upl)");
		});
		if (args.should_fail) {
			T_ASSERT_TRUE(caught_exception, "Failing test should also throw guard exception");
			T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_SEC_UPL_WRITE_ON_EXEC_REGION,
			    "Failing test throws the expected guard exception flavor");
			T_ASSERT_EQ(exc_info.catch_count, 1, "Failing test should throw exactly one guard exception");
		} else {
			T_ASSERT_FALSE(caught_exception, "Passing test should not throw guard exception");
		}

		args.ptr += 0x800;
		args.size -= 0x1000;
	}

	munmap(buf, buf_size);
}

T_DECL(vm_upl_ro_on_commpage,
    "Generate RO UPL against comm page")
{
#if !TARGET_OS_OSX
	T_SKIP("Comm page only guaranteed to be within user address range on MacOS, skipping...");
#else
#ifndef __arm64__
	T_SKIP("Comm page only has UPL-incompatible mapping on arm64, skipping...");
#else
	upl_test_args args = { .ptr = (uint64_t)_COMM_PAGE_START_ADDRESS, .size = 0x1000, .test_pattern = 'b',
		               .copy_expected = false, .should_fail = true, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");
#endif /* !defined(__arm64__) */
#endif /* !TARGET_OS_OSX */
}

T_DECL(vm_upl_partial_cow,
    "Generate a UPL that requires CoW setup for part of an object")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	/*
	 * Mark a portion of the buffer RO, which will split off a separate vm_map_entry backed by the same
	 * vm_object.  This will produce an internal COPY_SYMMETRIC object with refcount > 1, which is the
	 * baseline requirement for partial CoW setup by vm_map_create_upl().
	 */
	T_QUIET; T_ASSERT_POSIX_SUCCESS(mprotect((char*)buf + (8 * PAGE_SIZE), 2 * PAGE_SIZE, PROT_READ), "mprotect");

	/*
	 * Request a non-page-aligned UPL against the RW region of the buffer, to ensure that partial CoW
	 * setup still ultimately uses a page-aligned buffer as required for vm_map_entry clipping.
	 */
	upl_test_args args = { .ptr = (uint64_t)buf + 0x800, .size = 2 * PAGE_SIZE, .test_pattern = 'b',
		               .copy_expected = false, .should_fail = false, .upl_rw = true };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl)");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		if ((i < (0x800 / sizeof(*buf))) || (i >= ((0x800 + args.size) / sizeof(*buf)))) {
			T_QUIET; T_ASSERT_EQ(buf[i], (unsigned int)'a' + i,
			    "buf[%u]='%u' == '%u'", i, buf[i], (unsigned int)'a' + i);
		} else {
			T_QUIET; T_ASSERT_EQ(buf[i], (unsigned int)'b' + i - (unsigned int)(0x800 / sizeof(*buf)),
			    "buf[%u]='%u' == '%u'",
			    i, buf[i], (unsigned int)'b' + i - (unsigned int)(0x800 / sizeof(*buf)));
		}
	}

	munmap(buf, buf_size);
}

typedef struct {
	uint64_t ptr;
	uint32_t size;
	bool upl_rw;
	bool should_fail;
	uint8_t fault_prot;
} upl_object_test_args;

T_DECL(vm_upl_rw_on_exec_object,
    "Attempt to create a writable UPL against an object containing executable pages")
{
	/**
	 * This test is meant to exercise functionality that is currently SPTM-specific.
	 * It also relies on the assumption that JIT regions are faulted in an all-or-nothing
	 * manner, so that the write faults generated by our buffer fill below will also
	 * produce executable mappings of the underlying JIT pages.  This happens to hold
	 * true on SPTM-enabled devices because all of them use xPRR, but may not hold true
	 * in general.
	 */
	if (!is_sptm_enabled()) {
		T_SKIP("Exec object test only supported on SPTM-enabled devices, skipping...");
	}
	if (process_is_translated()) {
		/* TODO: Remove this once rdar://142438840 is fixed. */
		T_SKIP("Guard exception handling does not work correctly with Rosetta (rdar://142438840), skipping...");
	}
	if (!is_map_jit_allowed()) {
		T_SKIP("MAP_JIT not allowed for this system configuration");
	}

	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	if (os_thread_self_restrict_rwx_is_supported()) {
		os_thread_self_restrict_rwx_to_rw();
	}

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	/* Ensure that guard exceptions will not be fatal to the test process. */
	enable_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY);

	upl_object_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .upl_rw = true, .should_fail = true, .fault_prot = VM_PROT_NONE };

	__block int64_t addr = (int64_t)&args;
	__block int64_t result = 0;
	__block size_t s = sizeof(result);
	exc_guard_helper_info_t exc_info;
	bool caught_exception =
	    block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
		T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_object", &result, &s, &addr, sizeof(addr)),
		"sysctlbyname(debug.test.vm_upl_object)");
	});
	if (args.should_fail) {
		T_ASSERT_TRUE(caught_exception, "Failing test should also throw guard exception");
		T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_SEC_IOPL_ON_EXEC_PAGE,
		    "Failing test throws the expected guard exception flavor");
		T_ASSERT_EQ(exc_info.catch_count, 1, "Failing test should throw exactly one guard exception");
	} else {
		T_ASSERT_FALSE(caught_exception, "Passing test should not throw guard exception");
	}

	munmap(buf, buf_size);
}

T_DECL(vm_upl_ro_with_exec_fault,
    "Attempt to exec-fault a region while a UPL is in-flight for that region")
{
	/**
	 * This test is meant to exercise functionality that is currently SPTM-specific.
	 */
	if (!is_sptm_enabled()) {
		T_SKIP("Exec-fault test only supported on SPTM-enabled devices, skipping...");
	}
	if (process_is_translated()) {
		/* TODO: Remove this once rdar://142438840 is fixed. */
		T_SKIP("Guard exception handling does not work correctly with Rosetta (rdar://142438840), skipping...");
	}
	if (!is_map_jit_allowed()) {
		T_SKIP("MAP_JIT not allowed for this system configuration");
	}

	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	if (os_thread_self_restrict_rwx_is_supported()) {
		os_thread_self_restrict_rwx_to_rw();
	}
	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}
	if (os_thread_self_restrict_rwx_is_supported()) {
		os_thread_self_restrict_rwx_to_rx();
	}

	/* Ensure that guard exceptions will not be fatal to the test process. */
	enable_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY);

	upl_object_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .upl_rw = false, .should_fail = false,
		                      .fault_prot = VM_PROT_EXECUTE | VM_PROT_READ };

	__block int64_t addr = (int64_t)&args;
	__block int64_t result = 0;
	__block size_t s = sizeof(result);
	exc_guard_helper_info_t exc_info;
	bool caught_exception =
	    block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
		T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_object", &result, &s, &addr, sizeof(addr)),
		"sysctlbyname(debug.test.vm_upl_object)");
	});
	T_ASSERT_TRUE(caught_exception, "Exec fault should throw guard exception");
	T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_SEC_EXEC_ON_IOPL_PAGE,
	    "Attempted exec fault throws the expected guard exception flavor");
	T_ASSERT_EQ(exc_info.catch_count, 1, "Attempted exec fault should throw exactly one guard exception");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_ro_with_write_fault_on_exec_file,
    "Attempt to write-fault a file-backed executable region while a UPL is in-flight for that region")
{
#if TARGET_OS_OSX
	/**
	 * Test the bug uncovered in rdar://158063220.  This requires an on-the-fly retype to an
	 * executable frame type in conjunction with a write fault on a file-backed page that is also
	 * being cleaned in place.  This implies a "legacy JIT" mapping, since modern MAP_JIT mappings
	 * aren't allowed for file-backed memory.  The existing vm_upl_object test hook can already
	 * simulate the concurrent retype and in-place clean, so this test is mostly a matter of
	 * setting up the file-backed memory correctly.
	 */
	/**
	 * This test is meant to exercise functionality that is currently SPTM-specific.
	 */
	if (!is_sptm_enabled()) {
		T_SKIP("Write-fault-on-exec test only supported on SPTM-enabled devices, skipping...");
	}

	if (process_is_translated()) {
		/* TODO: Remove this once rdar://142438840 is fixed. */
		T_SKIP("Guard exception handling does not work correctly with Rosetta (rdar://142438840), skipping...");
	}

	/* First, generate our backing file. */
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	ssize_t nbytes;
	int fd;
	char tmp_file_name[PATH_MAX] = "/tmp/vm_upl_data.XXXXXXXX";
	T_ASSERT_NOTNULL(mktemp(tmp_file_name), "create temporary file name");
	T_WITH_ERRNO; T_QUIET; T_ASSERT_GE(fd = open(tmp_file_name, O_CREAT | O_RDWR),
	    0, "create temp file");
	T_WITH_ERRNO; T_QUIET; T_ASSERT_EQ(nbytes = write(fd, buf, buf_size),
	    (ssize_t)buf_size, "write %zu bytes", buf_size);
	munmap(buf, buf_size);

	/* Map the backing file. */
	buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	/**
	 * Twiddle the mapping permissions between RX and RW.  This will mark the mapping as "user debug",
	 * which will induce a retype when the backing pages are faulted in.
	 */
	kern_return_t kr = mach_vm_protect(mach_task_self(), (mach_vm_address_t)buf, buf_size, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_protect(RX)");

	kr = mach_vm_protect(mach_task_self(), (mach_vm_address_t)buf, buf_size, FALSE, VM_PROT_READ | VM_PROT_WRITE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_vm_protect(RW)");

	/* Ensure that guard exceptions will not be fatal to the test process. */
	enable_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY);

	upl_object_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .upl_rw = false, .should_fail = false,
		                      .fault_prot = VM_PROT_WRITE | VM_PROT_READ };

	__block int64_t addr = (int64_t)&args;
	__block int64_t result = 0;
	__block size_t s = sizeof(result);
	exc_guard_helper_info_t exc_info;
	bool caught_exception =
	    block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
		T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_object", &result, &s, &addr, sizeof(addr)),
		"sysctlbyname(debug.test.vm_upl_object)");
	});
	if (!process_is_translated()) {
		T_ASSERT_TRUE(caught_exception, "Exec fault should throw guard exception");
		T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_SEC_EXEC_ON_IOPL_PAGE,
		    "Attempted exec fault throws the expected guard exception flavor");
		T_ASSERT_EQ(exc_info.catch_count, 1, "Attempted exec fault should throw exactly one guard exception");
	}

	munmap(buf, buf_size);
	close(fd);
#else
	T_SKIP("Write-fault on exec file requires non-MAP_JIT RWX support");
#endif /* TARGET_OS_OSX */
}

typedef struct {
	uint64_t ptr;
	uint64_t upl_base;
	uint32_t size;
	uint32_t upl_size;
	bool upl_rw;
} upl_submap_test_args;

T_DECL(vm_upl_ro_on_submap,
    "Generate RO UPL against a submap region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	upl_submap_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .upl_base = 0x180000000ULL,
		                      .upl_size = buf_size, .upl_rw = false };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap)");

	args.upl_base += 0x800;
	args.upl_size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap)");

	args.upl_base += 0x800;
	args.upl_size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap)");

	munmap(buf, buf_size);
}

T_DECL(vm_upl_rw_on_submap,
    "Generate RW UPL against a submap region")
{
	const size_t buf_size = 10 * PAGE_SIZE;
	unsigned int *buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	T_QUIET; T_ASSERT_NE_PTR(buf, MAP_FAILED, "map buffer");

	for (unsigned int i = 0; i < (buf_size / sizeof(*buf)); i++) {
		buf[i] = (unsigned int)'a' + i;
	}

	upl_submap_test_args args = { .ptr = (uint64_t)buf, .size = buf_size, .upl_base = 0x180000000ULL,
		                      .upl_size = buf_size, .upl_rw = true };

	int64_t addr = (int64_t)&args;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap)");

	args.upl_base += 0x800;
	args.upl_size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap");

	args.upl_base += 0x800;
	args.upl_size -= 0x1000;

	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_upl_submap", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_upl_submap)");

	munmap(buf, buf_size);
}
