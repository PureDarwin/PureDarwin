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

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <darwintest.h>
#include <kern/exc_guard.h>
#include <mach/task_info.h>

#include "exc_helpers.h"
#include "exc_guard_helper.h"
#include "test_utils.h"

/* Convenience macro for compile-time array size */
#define countof(array)                                                  \
	_Pragma("clang diagnostic push")                                \
	_Pragma("clang diagnostic error \"-Wsizeof-pointer-div\"")      \
	(sizeof(array)/sizeof((array)[0]))                              \
	_Pragma("clang diagnostic pop")

/*
 * Global data shared between the code running the block and the exception handler.
 * Ideally this would be thread-local data in the thread running the block,
 * but the exception handler runs on a different thread and can't see it.
 */
static pthread_mutex_t exc_guard_helper_mutex = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t exc_guard_helper_exc_port = MACH_PORT_NULL;

static pthread_mutex_t exc_guard_helper_request_mutex = PTHREAD_MUTEX_INITIALIZER;
static exc_guard_helper_info_t exc_guard_helper_reply;
static struct {
	mach_port_t thread;
	unsigned int guard_type;
} exc_guard_helper_request;

static const char *
name_for_guard_type(unsigned guard_type)
{
	static const char *names[] = {
		[GUARD_TYPE_NONE]        = "GUARD_TYPE_NONE",
		[GUARD_TYPE_MACH_PORT]   = "GUARD_TYPE_MACH_PORT",
		[GUARD_TYPE_FD]          = "GUARD_TYPE_FD",
		[GUARD_TYPE_USER]        = "GUARD_TYPE_USER",
		[GUARD_TYPE_VN]          = "GUARD_TYPE_VN",
		[GUARD_TYPE_VIRT_MEMORY] = "GUARD_TYPE_VIRT_MEMORY",
		[GUARD_TYPE_REJECTED_SC] = "GUARD_TYPE_REJECTED_SC",
	};
	const char *result = NULL;
	if (guard_type < countof(names)) {
		result = names[guard_type];
	}
	if (result == NULL) {
		result = "unknown";
	}
	return result;
}

static size_t
exc_guard_helper_exception_handler(
	__unused mach_port_t task,
	mach_port_t thread,
	exception_type_t exception,
	mach_exception_data_t codes,
	__unused uint64_t exception_pc)
{
	T_QUIET; T_ASSERT_EQ(exception, EXC_GUARD, "exception type");
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&exc_guard_helper_request_mutex), "lock");

	if (thread != exc_guard_helper_request.thread) {
		/* reject, nobody is waiting for exceptions */
		if (verbose_exc_helper) {
			T_LOG("exc_guard_helper caught an exception but nobody is waiting for it");
		}
		T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "unlock");
		return 0;
	}

	unsigned int exc_guard_type = EXC_GUARD_DECODE_GUARD_TYPE(codes[0]);
	uint32_t exc_guard_flavor = EXC_GUARD_DECODE_GUARD_FLAVOR(codes[0]);
	uint32_t exc_guard_target = EXC_GUARD_DECODE_GUARD_TARGET(codes[0]);
	uint64_t exc_guard_payload = codes[1];

	if (exc_guard_helper_request.guard_type == exc_guard_type) {
		/* okay, exception matches caller's requested guard type */
	} else {
		/* reject, exception's guard type is not of the requested type */
		if (verbose_exc_helper) {
			T_LOG("exc_guard_helper exception is not of the "
			    "desired guard type (expected %u, got %u)",
			    exc_guard_helper_request.guard_type, exc_guard_type);
		}
		T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "unlock");
		return 0;
	}

	if (++exc_guard_helper_reply.catch_count == 1) {
		/* save the details of the first caught exception */
		exc_guard_helper_reply.guard_type    = exc_guard_type;
		exc_guard_helper_reply.guard_flavor  = exc_guard_flavor;
		exc_guard_helper_reply.guard_target  = exc_guard_target;
		exc_guard_helper_reply.guard_payload = exc_guard_payload;
	}

	if (verbose_exc_helper) {
		T_LOG("exc_guard_helper caught EXC_GUARD type %u (%s), flavor %u, "
		    "target %u, payload 0x%llx (catch #%u in the block)",
		    exc_guard_type, name_for_guard_type(exc_guard_type),
		    exc_guard_flavor, exc_guard_target, exc_guard_payload,
		    exc_guard_helper_reply.catch_count);
	}

	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "unlock");
	return 0;
}

/*
 * Set up our exception handlers if they are not already configured.
 * exc_guard_helper_mutex must be held by the caller.
 */
static void
initialize_exception_handlers(void)
{
	if (exc_guard_helper_exc_port == MACH_PORT_NULL) {
		exc_guard_helper_exc_port = create_exception_port(EXC_MASK_GUARD);
		T_QUIET; T_ASSERT_NE(exc_guard_helper_exc_port, MACH_PORT_NULL, "exception port");
		repeat_exception_handler(exc_guard_helper_exc_port, exc_guard_helper_exception_handler);
		if (verbose_exc_helper) {
			T_LOG("exc_guard_helper exception handlers installed");
		}
	}
}

void
exc_guard_helper_init(void)
{
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&exc_guard_helper_mutex), "lock");
	initialize_exception_handlers();
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_mutex), "unlock");
}


/*
 * Return EXC_GUARD behavior flags that enable guard_type (non-fatal)
 * and leave all other behaviors in old_behavior unchanged.
 */
static task_exc_guard_behavior_t
configure_exc_guard_of_type(
	unsigned int guard_type,
	task_exc_guard_behavior_t old_behavior)
{
	/*
	 * Behavior flags for all known EXC_GUARD types.
	 * These flags are defined in mach/task_info.h.
	 * Some guard types cannot be configured and do not have these flags.
	 */
	static const struct {
		task_exc_guard_behavior_t set;
		task_exc_guard_behavior_t clear;
	} behavior_flags[] = {
		[GUARD_TYPE_VIRT_MEMORY] = {
			.clear = TASK_EXC_GUARD_VM_ALL,
			.set = TASK_EXC_GUARD_VM_DELIVER,
		},
		[GUARD_TYPE_MACH_PORT] = {
			.clear = TASK_EXC_GUARD_MP_ALL,
			.set = TASK_EXC_GUARD_MP_DELIVER,
		},
	};

	/* Reject guard types not present in behavior_flags[]. */
	if (guard_type >= countof(behavior_flags)) {
		goto unimplemented_guard_type;
	}
	if (behavior_flags[guard_type].set == 0 &&
	    behavior_flags[guard_type].clear == 0) {
		goto unimplemented_guard_type;
	}

	/* Set and clear behavior flags for the requested guard type(s). */
	task_exc_guard_behavior_t new_behavior = old_behavior;
	new_behavior &= ~behavior_flags[guard_type].clear;
	new_behavior |= behavior_flags[guard_type].set;
	return new_behavior;

unimplemented_guard_type:
	/*
	 * No behavior_flags[] entry for this EXC_GUARD guard type.
	 * If task_set_exc_guard_behavior() can configure your new
	 * guard type then add it to behavior_flags[] above.
	 */
	T_FAIL("guard type %u (%s) is unimplemented in exc_guard_helper",
	    guard_type, name_for_guard_type(guard_type));
	T_END;
}

task_exc_guard_behavior_t
enable_exc_guard_of_type(unsigned int guard_type)
{
	kern_return_t kr;
	task_exc_guard_behavior_t old_behavior, new_behavior;

	kr = task_get_exc_guard_behavior(mach_task_self(), &old_behavior);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "exc_guard_helper calling task_get_exc_guard_behavior");

	new_behavior = configure_exc_guard_of_type(guard_type, old_behavior);

	kr = task_set_exc_guard_behavior(mach_task_self(), new_behavior);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr,
	    "exc_guard_helper calling task_set_exc_guard_behavior to enable guard type %u %s",
	    guard_type, name_for_guard_type(guard_type));

	return old_behavior;
}

bool
block_raised_exc_guard_of_type(
	unsigned int guard_type,
	exc_guard_helper_info_t * const out_exc_info,
	exc_guard_helper_block_t block)
{
	if (process_is_translated() && guard_type == GUARD_TYPE_VIRT_MEMORY) {
		T_FAIL("block_raised_exc_guard_of_type(GUARD_TYPE_VIRT_MEMORY) "
		    "does not work on translation/Rosetta (rdar://142438840)");
	}

	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&exc_guard_helper_mutex), "lock");
	initialize_exception_handlers();

	/* lock the request and reply structs against the exception handler */
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&exc_guard_helper_request_mutex), "lock");

	/* prepare the global request and reply struct contents */
	memset(&exc_guard_helper_request, 0, sizeof(exc_guard_helper_request));
	memset(&exc_guard_helper_reply, 0, sizeof(exc_guard_helper_reply));
	exc_guard_helper_request.thread = mach_thread_self();
	exc_guard_helper_request.guard_type = guard_type;

	/* unlock the request and reply structs so the exception handler can use them */
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "unlock");

	/* run the caller's block */
	if (verbose_exc_helper) {
		T_LOG("exc_guard_helper calling a block");
	}
	block();
	if (verbose_exc_helper) {
		T_LOG("exc_guard_helper finished a block, %u exception%s caught",
		    exc_guard_helper_reply.catch_count,
		    exc_guard_helper_reply.catch_count == 1 ? "" : "s");
	}

	/* lock the request and reply structs again */
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "lock");

	/* read the reply from the exception handler */
	bool result = exc_guard_helper_reply.catch_count > 0;
	memcpy(out_exc_info, &exc_guard_helper_reply, sizeof(exc_guard_helper_reply));

	/* clear the request and reply before unlocking everything */
	memset(&exc_guard_helper_request, 0, sizeof(exc_guard_helper_request));
	memset(&exc_guard_helper_reply, 0, sizeof(exc_guard_helper_reply));
	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_request_mutex), "unlock");

	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&exc_guard_helper_mutex), "unlock");

	return result;
}

bool
block_raised_exc_guard_of_type_ignoring_translated(
	unsigned int guard_type,
	exc_guard_helper_info_t * const out_exc_info,
	exc_guard_helper_block_t block)
{
	if (process_is_translated() && guard_type == GUARD_TYPE_VIRT_MEMORY) {
		/* Rosetta can't recover from guard exceptions of GUARD_TYPE_VIRT_MEMORY */
		T_LOG("note: exc_guard_helper calling a block with no exception "
		    "handler due to translation/Rosetta (rdar://142438840)");
		block();
		memset(out_exc_info, 0, sizeof(*out_exc_info));
		return false;
	}

	return block_raised_exc_guard_of_type(guard_type, out_exc_info, block);
}
