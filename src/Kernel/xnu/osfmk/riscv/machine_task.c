/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <kern/task.h>
#include <kern/thread.h>
#include <riscv/misc_protos.h>
#include <riscv/machine_routines.h>

// riscv has no task wide debug state, task_debug stays empty and no flavor applies to a task

kern_return_t
machine_task_set_state(
	task_t task,
	int flavor,
	__unused thread_state_t state,
	__unused mach_msg_type_number_t state_count)
{
	switch (flavor) {
	case THREAD_STATE_NONE:         /* Using this flavor to clear task_debug */
	{
		if (task->task_debug != NULL) {
			task->task_debug = NULL;
			return KERN_SUCCESS;
		}
		return KERN_FAILURE;
	}
	default:
	{
		return KERN_INVALID_ARGUMENT;
	}
	}
	return KERN_FAILURE;
}

kern_return_t
machine_task_get_state(__unused task_t task,
    __unused int flavor,
    __unused thread_state_t state,
    __unused mach_msg_type_number_t *state_count)
{
	return KERN_INVALID_ARGUMENT;
}

void
machine_task_terminate(task_t task)
{
	if (task) {
		task->task_debug = NULL;
	}
}

kern_return_t
machine_thread_inherit_taskwide(
	__unused thread_t thread,
	__unused task_t parent_task)
{
	return KERN_SUCCESS;
}

void
machine_task_init(__unused task_t new_task,
    __unused task_t parent_task,
    __unused boolean_t memory_inherit)
{
}

// machine_task_process_signature: code signature dependent task state adjustments, may run more
// than once per task. On error, point error_msg to a static string (the caller will not free it).
kern_return_t
machine_task_process_signature(
	task_t task,
	uint32_t const __unused platform,
	uint32_t const __unused sdk,
	char const ** __unused error_msg)
{
	assert(error_msg != NULL);

	// the timebase is the platform timer, there is no 1 GHz mode to opt into
	task->uses_1ghz_timebase = false;

	return KERN_SUCCESS;
}

bool
ml_task_uses_1ghz_timebase(const task_t task)
{
	return task->uses_1ghz_timebase;
}
