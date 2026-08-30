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
 * exc_guard_helper.h
 *
 * Helper functions for userspace tests to test for EXC_GUARD exceptions.
 *
 * To use these functions in your test you must set additional build options.
 * See target `exc_guard_helper_test` in tests/Makefile for an example.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <mach/task_info.h>

/*
 * Set verbose_exc_helper = true to log exception information with T_LOG().
 * The default is true.
 */
extern bool verbose_exc_helper;

typedef struct {
	/* The number of EXC_GUARD exceptions caught during the block. */
	unsigned catch_count;

	/*
	 * The remaining fields are only set for the first EXC_GUARD caught.
	 * See kern/exc_guard.h for definitions of these fields.
	 */
	unsigned guard_type;     /* e.g. GUARD_TYPE_VIRT_MEMORY */
	uint32_t guard_flavor;
	uint32_t guard_target;
	uint64_t guard_payload;
} exc_guard_helper_info_t;

/*
 * Initialize exc_guard_helper's exception handling.
 *
 * Calling this is optional. The other functions will perform
 * initialization if necessary. You may need to call this
 * function if that automatic initialization allocates
 * memory in address ranges that your test requires to
 * be unallocated.
 */
extern void
exc_guard_helper_init(void);

/*
 * Sets EXC_GUARD exceptions of the given type (e.g. GUARD_TYPE_VIRT_MEMORY)
 * to be enabled and non-fatal in this process.
 * Returns the previous guard exception behavior. Pass this value
 * to task_set_exc_guard_behavior() to restore the previous behavior.
 *
 * Fails with T_FAIL if the behavior could not be set; for example:
 * - guard exceptions cannot be configured in some processes
 * - some guard exception types cannot be set to non-fatal
 */
extern task_exc_guard_behavior_t
enable_exc_guard_of_type(unsigned int guard_type);

/*
 * Runs block() and returns true if it raised a non-fatal EXC_GUARD exception
 * of the requested type (e.g. GUARD_TYPE_VIRT_MEMORY).
 *
 * While block() runs, any EXC_GUARD exceptions of the requested
 * type are caught and recorded, then execution resumes.
 * Information about any caught exception(s) is returned in *out_exc_info.
 * If more than one EXC_GUARD exception of the requested type is raised then
 * details about all but the first are discarded, other than `catch_count`
 * the number of exceptions caught.
 *
 * Guard exceptions of this type must be enabled and non-fatal.
 * enable_exc_guard_of_type() can set this for your process.
 *
 * Note that block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY)
 * does not work on Rosetta. This function will T_FAIL if you try.
 * See block_raised_exc_guard_of_type_ignoring_translated() below
 * if you are willing to forgo the guard exception handler in
 * translated execution environments like Rosetta.
 *
 * Example:
 *      enable_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY);
 *      [...]
 *      exc_guard_helper_info_t exc_info;
 *      if (block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY, &exc_info, ^{
 *              mach_vm_deallocate(mach_task_self(), addr, size);
 *          })) {
 *              // EXC_GUARD raised during mach_vm_deallocate, details in exc_info
 *      } else {
 *              // mach_vm_deallocate did not raise EXC_GUARD
 *      }
 */
typedef void (^exc_guard_helper_block_t)(void);
extern bool
block_raised_exc_guard_of_type(
	unsigned int guard_type,
	exc_guard_helper_info_t * const out_exc_info,
	exc_guard_helper_block_t block);

/*
 * Like block_raised_exc_guard_of_type(), but quietly
 * runs the block with no guard exception handler if
 * the guard type is GUARD_TYPE_VIRT_MEMORY and we're
 * in a translated execution environment like Rosetta.
 */
extern bool
block_raised_exc_guard_of_type_ignoring_translated(
	unsigned int guard_type,
	exc_guard_helper_info_t * const out_exc_info,
	exc_guard_helper_block_t block);
