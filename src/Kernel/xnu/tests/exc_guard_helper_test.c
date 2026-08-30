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
 * exc_guard_helper_test.c
 *
 * Test the testing helper functions in exc_guard_helper.h.
 */

#include "exc_guard_helper.h"

#include <darwintest.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/task_info.h>
#include "test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vm"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true)
	);

/* Convenience macro for compile-time array size */
#define countof(array)                                                  \
	_Pragma("clang diagnostic push")                                \
	_Pragma("clang diagnostic error \"-Wsizeof-pointer-div\"")      \
	(sizeof(array)/sizeof((array)[0]))                              \
	_Pragma("clang diagnostic pop")

/*
 * Return true if [query_start, query_start + query_size) is unallocated memory.
 */
static bool
is_hole(mach_vm_address_t query_start, mach_vm_size_t query_size)
{
	mach_vm_address_t entry_start = query_start;
	mach_vm_size_t entry_size;
	vm_region_submap_info_data_64_t info;
	uint32_t depth = 0;
	mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
	kern_return_t kr = mach_vm_region_recurse(mach_task_self(),
	    &entry_start, &entry_size, &depth,
	    (vm_region_recurse_info_t)&info, &count);

	if (kr == KERN_INVALID_ADDRESS) {
		/*
		 * query_start is unmapped, and so is everything after it,
		 * therefore the query range is a hole
		 */
		return true;
	}
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_region");

	/* this code does not handle submaps */
	T_QUIET; T_ASSERT_EQ(depth, 0, "submaps unimplemented");

	/*
	 * entry_start is mapped memory, and either
	 * (1) entry_start's mapping contains query_start, OR
	 * (2) query_start is unmapped and entry_start is the next mapped memory
	 */

	if (entry_start >= query_start + query_size) {
		/*
		 * entry_start's mapping does not contain query_start,
		 * and entry_start's mapping begins after the query range,
		 * therefore the query range is a hole
		 */
		return true;
	} else {
		return false;
	}
}

/* Call enable_exc_guard_of_type(), and test its behavior. */
static void
enable_exc_guard_of_type_and_verify(unsigned int guard_type)
{
	struct {
		const char *name;
		task_exc_guard_behavior_t all_mask;
		task_exc_guard_behavior_t deliver_mask;
		task_exc_guard_behavior_t fatal_mask;
	} guards[] = {
		[GUARD_TYPE_VIRT_MEMORY] = {
			.name = "VM",
			.all_mask = TASK_EXC_GUARD_VM_ALL,
			.deliver_mask = TASK_EXC_GUARD_VM_DELIVER,
			.fatal_mask = TASK_EXC_GUARD_VM_FATAL
		},
		[GUARD_TYPE_MACH_PORT] = {
			.name = "Mach port",
			.all_mask = TASK_EXC_GUARD_MP_ALL,
			.deliver_mask = TASK_EXC_GUARD_MP_DELIVER,
			.fatal_mask = TASK_EXC_GUARD_MP_FATAL
		}
	};

	kern_return_t kr;
	task_exc_guard_behavior_t disabling_behavior, old_behavior, new_behavior;

	T_QUIET; T_ASSERT_TRUE(guard_type < countof(guards) && guards[guard_type].name != NULL,
	    "guard type in enable_exc_guard_of_type_and_verify");

	/* disable guard exceptions of this type, then verify that enable_exc_guard_of_type enables them */

	kr = task_get_exc_guard_behavior(mach_task_self(), &disabling_behavior);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "get old behavior");
	disabling_behavior &= ~guards[guard_type].all_mask;
	kr = task_set_exc_guard_behavior(mach_task_self(), disabling_behavior);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "set empty behavior");

	old_behavior = enable_exc_guard_of_type(guard_type);
	T_QUIET; T_ASSERT_EQ(old_behavior, disabling_behavior, "enable_exc_guard_of_type return value");
	T_QUIET; T_ASSERT_FALSE(old_behavior & guards[guard_type].deliver_mask,
	    "%s guard exceptions must not be enabled", guards[guard_type].name);

	kr = task_get_exc_guard_behavior(mach_task_self(), &new_behavior);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "get new behavior");
	T_ASSERT_TRUE(new_behavior & guards[guard_type].deliver_mask,
	    "enable_exc_guard_of_type enabled %s guard exceptions", guards[guard_type].name);
	T_ASSERT_FALSE(new_behavior & guards[guard_type].fatal_mask,
	    "enable_exc_guard_of_type set %s guard exceptions to non-fatal", guards[guard_type].name);
}


T_DECL(exc_guard_helper_test_vm,
    "test the test helper function block_raised_exc_guard_of_type with VM guard exceptions")
{
	if (process_is_translated()) {
		T_SKIP("VM guard exceptions not supported on Rosetta (rdar://142438840)");
	}

	kern_return_t kr;
	exc_guard_helper_info_t exc_info;

	exc_guard_helper_init();
	enable_exc_guard_of_type_and_verify(GUARD_TYPE_VIRT_MEMORY);

	/*
	 * Test guard exceptions by deallocating unallocated VM space.
	 * Problem: Rosetta asynchronously allocates memory in the process
	 * to store translated instructions. These allocations can land
	 * inside our unallocated space, disrupting our test and crashing
	 * after we call vm_deallocate() on space that we thought was empty.
	 * Solution:
	 * - use VM_FLAGS_RANDOM_ADDR in the hope of moving our allocation
	 *   away from VM's ordinary next allocation space
	 * - try to verify that the unallocated space is empty before
	 *   calling vm_deallocate, and retry several times if it is not empty
	 */

#define LAST_RETRY 10
	for (int retry_count = 0; retry_count <= LAST_RETRY; retry_count++) {
		/* allocate three pages */
		mach_vm_address_t allocated = 0;
		kr = mach_vm_allocate(mach_task_self(), &allocated, PAGE_SIZE * 3,
		    VM_FLAGS_ANYWHERE | VM_FLAGS_RANDOM_ADDR);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "allocate space");

		/* deallocate the page in the middle; no EXC_GUARD from successful deallocation */
		if (block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
			kern_return_t kr;
			kr = mach_vm_deallocate(mach_task_self(), allocated + PAGE_SIZE, PAGE_SIZE);
			T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "create hole");
		})) {
			T_FAIL("unexpected guard exception");
		} else {
			T_ASSERT_EQ(exc_info.catch_count, 0, "block_raised_exc_guard_of_type(VM) with no exceptions");
		}

		/* try to deallocate the hole, twice, and detect the guard exceptions */
		__block bool retry = false;
		bool caught_exception = block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
			kern_return_t kr;

			/* deallocate page-hole-page; EXC_GUARD expected from deallocating a hole */
			if (!is_hole(allocated + PAGE_SIZE, PAGE_SIZE)) {
			        retry = true;  /* somebody allocated inside our unallocated space; retry */
			        return;
			}
			kr = mach_vm_deallocate(mach_task_self(), allocated, PAGE_SIZE * 3);
			T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_deallocate a hole");

			/* deallocate again, now all holes; EXC_GUARD expected from deallocating a hole */
			if (!is_hole(allocated, PAGE_SIZE * 3)) {
			        retry = true;  /* somebody allocated inside our unallocated space; retry */
			        return;
			}
			kr = mach_vm_deallocate(mach_task_self(), allocated, PAGE_SIZE * 3);
			T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "vm_deallocate a hole again");

			if (!is_hole(allocated, PAGE_SIZE * 3)) {
			        retry = true;  /* somebody allocated inside our unallocated space; retry */
			        return;
			}
		});

		if (retry) {
			if (retry_count < LAST_RETRY) {
				T_LOG("unallocated space was found to be allocated, retrying");
			} else {
				T_FAIL("intended unallocated space was repeatedly found to be allocated, giving up");
			}
		} else if (caught_exception) {
			/* caught an exception as expected: verify what we caught */
			T_ASSERT_EQ(exc_info.catch_count, 2, "block_raised_exc_guard_of_type(VM) with 2 exceptions");
			T_ASSERT_EQ(exc_info.guard_type, GUARD_TYPE_VIRT_MEMORY, "caught exception's type");
			T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_DEALLOC_GAP, "caught exception's flavor");
			T_ASSERT_EQ(exc_info.guard_payload, allocated + PAGE_SIZE, "caught exception's payload");
			break;  /* done retrying */
		} else {
			/* where's the beef? */
			T_FAIL("no VM guard exception caught");
			break;  /* done retrying */
		}
	}
}


T_DECL(exc_guard_helper_test_mach_port,
    "test the test helper function block_raised_exc_guard_of_type with Mach port guard exceptions")
{
	kern_return_t kr;
	exc_guard_helper_info_t exc_info;
	mach_port_t port;

	exc_guard_helper_init();
	enable_exc_guard_of_type_and_verify(GUARD_TYPE_MACH_PORT);

	/*
	 * Test guard exceptions by overflowing the send right count for a port.
	 */

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "new port");
	kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "make send");

	/* add and remove one send right, should succeed */
	if (block_raised_exc_guard_of_type(GUARD_TYPE_MACH_PORT, &exc_info, ^{
		kern_return_t kr;
		kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, +1);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "add one send right");
		kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "remove one send right");
	})) {
		T_FAIL("unexpected guard exception");
	} else {
		T_ASSERT_EQ(exc_info.catch_count, 0, "block_raised_exc_guard_of_type(MACH_PORT) with no exceptions");
	}

	/* try to overflow the port's send right count, twice, and catch the exceptions */
	bool caught_exception = block_raised_exc_guard_of_type(GUARD_TYPE_MACH_PORT, &exc_info, ^{
		kern_return_t kr;
		unsigned expected_error;
		if (process_is_translated()) {
		        expected_error = 0x1000013;  /* KERN_UREFS_OVERFLOW plus another bit? */
		} else {
		        expected_error = KERN_INVALID_VALUE;
		}
		kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, INT32_MAX);
		T_QUIET; T_ASSERT_MACH_ERROR(kr, expected_error, "add too many send rights");
		kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, INT32_MAX);
		T_QUIET; T_ASSERT_MACH_ERROR(kr, expected_error, "add too many send rights, again");
	});
	if (caught_exception) {
		/* caught an exception as expected: verify what we caught */
		T_ASSERT_EQ(exc_info.catch_count, 2, "block_raised_exc_guard_of_type(MACH_PORT) with 2 exceptions");
		T_ASSERT_EQ(exc_info.guard_type, GUARD_TYPE_MACH_PORT, "caught exception's type");
		T_ASSERT_EQ(exc_info.guard_flavor, kGUARD_EXC_INVALID_VALUE, "caught exception's flavor");
		T_ASSERT_EQ(exc_info.guard_target, port, "caught exception's target");
	} else {
		/* where's the beef? */
		T_FAIL("no Mach port guard exception caught");
	}
}
