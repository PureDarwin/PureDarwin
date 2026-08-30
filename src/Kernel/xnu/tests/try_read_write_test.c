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

/*
 * try_read_write_test.c
 *
 * Test the testing helper functions in try_read_write.h.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <darwintest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <ptrauth.h>

#include "try_read_write.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vm"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true)
	);

#define MAYBE_QUIET(quiet) \
	do {                                    \
	        if (quiet) {                    \
	                T_QUIET;                \
	        }                               \
	} while (0)

static void
test_try_read_byte_maybe_quietly(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	bool quiet,
	const char *message)
{
	bool expected_result = (expected_error == 0);
	bool actual_result;
	uint8_t actual_byte;
	kern_return_t actual_error;

	actual_result = try_read_byte(addr, &actual_byte, &actual_error);

	MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_result, actual_result, "%s: try_read_byte return value", message);
	MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_error, actual_error, "%s: try_read_byte error code", message);
	if (expected_error == 0 && actual_error == 0) {
		MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_byte, actual_byte, "%s: try_read_byte value read", message);
	}
}

static void
test_try_read_byte(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	const char *message)
{
	test_try_read_byte_maybe_quietly(addr, expected_byte, expected_error, false /* quiet */, message);
}

static void
test_try_read_byte_quietly(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	const char *message)
{
	test_try_read_byte_maybe_quietly(addr, expected_byte, expected_error, true /* quiet */, message);
}

static void
test_try_write_byte_maybe_quietly(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	bool quiet,
	const char *message)
{
	bool expected_result = (expected_error == 0);
	bool actual_result;
	uint8_t actual_byte;
	kern_return_t actual_error;

	actual_result = try_write_byte(addr, expected_byte, &actual_error);

	MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_result, actual_result, "%s: try_write_byte return value", message);
	MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_error, actual_error, "%s: try_write_byte error code", message);
	if (expected_error == 0 && actual_error == 0) {
		actual_byte = *(volatile uint8_t *)addr;
		MAYBE_QUIET(quiet); T_EXPECT_EQ(expected_byte, actual_byte, "%s: try_write_byte value written", message);
	}
}

static void
test_try_write_byte(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	const char *message)
{
	test_try_write_byte_maybe_quietly(addr, expected_byte, expected_error, false /* quiet */, message);
}

static void
test_try_write_byte_quietly(
	mach_vm_address_t addr,
	uint8_t expected_byte,
	kern_return_t expected_error,
	const char *message)
{
	test_try_write_byte_maybe_quietly(addr, expected_byte, expected_error, true /* quiet */, message);
}

static mach_vm_address_t
allocate_page_with_prot(vm_prot_t prot)
{
	mach_vm_address_t addr;
	kern_return_t kr;

	kr = mach_vm_allocate(mach_task_self(), &addr, PAGE_SIZE, VM_FLAGS_ANYWHERE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_allocate");
	kr = mach_vm_protect(mach_task_self(), addr, PAGE_SIZE, false /* set max */, prot);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_protect");
	return addr;
}

static void
deallocate_page(mach_vm_address_t addr)
{
	kern_return_t kr = mach_vm_deallocate(mach_task_self(), addr, PAGE_SIZE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_deallocate");
}

/*
 * Generate some r-x memory with a known value.
 */
static void __attribute__((naked))
instruction_byte_ff(void)
{
	asm(".quad 0xffffffff");
}

T_DECL(try_read_write_test,
    "test the test helper functions try_read_byte and try_write_byte")
{
	mach_vm_address_t addr;

	/* read and write an unmapped address */
	test_try_read_byte(0, 0, KERN_INVALID_ADDRESS, "read unmapped address");
	test_try_write_byte(0, 0, KERN_INVALID_ADDRESS, "write unmapped address");

	/* read and write --- */
	addr = allocate_page_with_prot(VM_PROT_NONE);
	test_try_read_byte(addr, 0, KERN_PROTECTION_FAILURE, "read prot ---");
	test_try_write_byte(addr, 1, KERN_PROTECTION_FAILURE, "write prot ---");
	deallocate_page(addr);

	/* read and write r-- */
	addr = allocate_page_with_prot(VM_PROT_READ);
	test_try_read_byte(addr, 0, KERN_SUCCESS, "read prot r--");
	test_try_write_byte(addr, 1, KERN_PROTECTION_FAILURE, "write prot r--");
	deallocate_page(addr);

	/* read and write -w- */
	addr = allocate_page_with_prot(VM_PROT_WRITE);
	test_try_read_byte(addr, 0, KERN_PROTECTION_FAILURE, "read prot -w-");
	test_try_write_byte(addr, 1, KERN_PROTECTION_FAILURE, "write prot -w-");
	deallocate_page(addr);

	/* read and write rw- */
	addr = allocate_page_with_prot(VM_PROT_READ | VM_PROT_WRITE);
	*(uint8_t *)addr = 1;
	test_try_read_byte(addr, 1, KERN_SUCCESS, "read prot rw-");
	test_try_write_byte(addr, 2, KERN_SUCCESS, "write prot rw-");
	test_try_read_byte(addr, 2, KERN_SUCCESS, "read prot rw- again");
	deallocate_page(addr);

	/* read and write r-x */
	addr = (mach_vm_address_t)ptrauth_strip(&instruction_byte_ff, ptrauth_key_function_pointer);
	test_try_read_byte(addr, 0xff, KERN_SUCCESS, "read prot r-x");
	test_try_write_byte(addr, 1, KERN_PROTECTION_FAILURE, "write prot r-x");
}


/* this test provokes THREAD_COUNT * REP_COUNT * PAGE_SIZE exceptions */
#define THREAD_COUNT 10
#define REP_COUNT 5

struct test_alloc {
	mach_vm_address_t addr;
	vm_prot_t prot;
	kern_return_t expected_read_error;
	kern_return_t expected_write_error;
};

static struct test_alloc
allocate_page_with_random_prot(void)
{
	struct test_alloc result;

	switch (random() % 4) {
	case 0:
		result.prot = VM_PROT_NONE;
		result.expected_read_error  = KERN_PROTECTION_FAILURE;
		result.expected_write_error = KERN_PROTECTION_FAILURE;
		break;
	case 1:
		result.prot = VM_PROT_READ;
		result.expected_read_error  = KERN_SUCCESS;
		result.expected_write_error = KERN_PROTECTION_FAILURE;
		break;
	case 2:
		result.prot = VM_PROT_WRITE;
		result.expected_read_error  = KERN_PROTECTION_FAILURE;
		result.expected_write_error = KERN_PROTECTION_FAILURE;
		break;
	case 3:
		result.prot = VM_PROT_READ | VM_PROT_WRITE;
		result.expected_read_error  = KERN_SUCCESS;
		result.expected_write_error = KERN_SUCCESS;
		break;
	}

	result.addr = allocate_page_with_prot(result.prot);
	return result;
}

static void *
multithreaded_test(void *arg)
{
	struct test_alloc alloc = *(struct test_alloc *)arg;

	/* Read and write a lot from our page. */
	for (int reps = 0; reps < REP_COUNT; reps++) {
		for (int offset = 0; offset < PAGE_SIZE; offset++) {
			test_try_read_byte_quietly(alloc.addr + offset, 0, alloc.expected_read_error, "thread read");
			test_try_write_byte_quietly(alloc.addr + offset, 0, alloc.expected_write_error, "thread write");
		}
	}

	return NULL;
}

T_DECL(try_read_write_test_multithreaded,
    "test try_read_byte and try_write_byte from multiple threads")
{
	verbose_exc_helper = false;

	pthread_t threads[THREAD_COUNT];
	struct test_alloc allocs[THREAD_COUNT];

	/* each thread gets a page with a random prot to read and write on */

	for (int i = 0; i < THREAD_COUNT; i++) {
		allocs[i] = allocate_page_with_random_prot();
	}

	T_LOG("running %d threads each %d times", THREAD_COUNT, REP_COUNT);

	for (int i = 0; i < THREAD_COUNT; i++) {
		pthread_create(&threads[i], NULL, multithreaded_test, &allocs[i]);
	}

	for (int i = 0; i < THREAD_COUNT; i++) {
		pthread_join(threads[i], NULL);
		deallocate_page(allocs[i].addr);
	}
}
