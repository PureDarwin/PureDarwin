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

#ifndef ARM_MTE_UTILITIES_H
#define ARM_MTE_UTILITIES_H

#include <mach/vm_types.h>
#include <sys/spawn_internal.h>

#include <stdbool.h>
#include <stdint.h>

#if (TARGET_OS_OSX || TARGET_OS_IOS) && defined(__arm64__)
// TODO(PT): It'd be nice to have this as an allow list rather than the inverse,
// but I wasn't able to restrict based on TARGET_OS_[IPHONE|IOS] as this is sometimes set even for XR_OS.
// For now, to keep things moving, just restrict this from being set on platforms where
// we know it's not the case.
#if !(TARGET_OS_XR || TARGET_OS_TV || TARGET_OS_WATCH || TARGET_OS_BRIDGE)
	#define TARGET_SUPPORTS_MTE_EMULATION 1
#endif
#endif

#define MTE_TAG_SHIFT                56
#define MTE_TAG_MASK                 (0xFULL << MTE_TAG_SHIFT)
#define MTE_GRANULE_SIZE             16
#define VM_MIN_KERNEL_ADDRESS        (0ULL - (2ULL << 40))
#define KERNEL_BUFFER_COPY_THRESHOLD (32 * 1024)
#define VIRTUAL_COPY_SZ              ((32 * 1024) + 15)
#define NUM_MTE_TAGS                 16
#define VM_WIMG_MTE                  0x22 /* VM_MEM_MTE | VM_MEM_COHERENT */

// Used in arm_mte.c
#define SPAWN_HELPER_WITH_ENTITLEMENT "arm_mte_spawn_client_with_hardened_process_entitlement"
#define SPAWN_HELPER_WITHOUT_ENTITLEMENT "arm_mte_spawn_client_without_hardened_process_entitlement"

// Used in arm_mte_spawn_policies.c
#define HARDENED_PROCESS_HELPER         SPAWN_HELPER_WITH_ENTITLEMENT
#define NO_HARDENED_PROCESS_HELPER      SPAWN_HELPER_WITHOUT_ENTITLEMENT

#define HARDENED_PROCESS_TOP_LEVEL_ONLY_AND_IN_AMFI_MTE_OPT_OUT_HELPER         \
  "arm_mte_spawn_client_with_top_level_hardened_proc_entitlement_and_in_amfi_opt_out"

#define EXPECT_MTE         "YES"
#define DO_NOT_EXPECT_MTE  "NO"

static enum {
	MTE_ENABLEMENT_TEST_HARDENED_PROCESS = 0x41,
	MTE_ENABLEMENT_TEST_VANILLA_PROCESS,
	MTE_ENABLEMENT_TEST_OPTED_OUT_PROCESS,
	MTE_ENABLEMENT_TEST_DONE
} mte_enablement_test_op;

static char MTE_ENABLEMENT_TEST_HARDENED_PROCESS_STR[2] = {MTE_ENABLEMENT_TEST_HARDENED_PROCESS, '\x0'};
static char MTE_ENABLEMENT_TEST_VANILLA_PROCESS_STR[2] = {MTE_ENABLEMENT_TEST_VANILLA_PROCESS, '\x0'};
static char MTE_ENABLEMENT_TEST_OPTED_OUT_PROCESS_STR[2] = {MTE_ENABLEMENT_TEST_OPTED_OUT_PROCESS, '\x0'};
static char MTE_ENABLEMENT_TEST_DONE_STR[2] = {MTE_ENABLEMENT_TEST_DONE, '\x0'};

void kill_child(int child_pid);

bool wait_for_child(int pid);

bool validate_proc_pidinfo_mte_status(int child_pid,
    bool expect_mte_enabled);
bool validate_proc_pidinfo_mte_soft_mode_status(int child_pid,
    bool expect_mte_soft_mode_enabled);

bool fork_and_exec_new_process(char *new_argv[]);

static enum {
	MTE_SPAWN_USE_VANILLA,
	MTE_SPAWN_USE_LEGACY_API,
} mte_spawn_op;

bool posix_spawn_then_perform_action_from_process(char *new_argv[], uint8_t setup,
    uint16_t spawn_flags);

inline unsigned int
extract_mte_tag(void *ptr)
{
	return (unsigned int)((((uintptr_t)ptr) & MTE_TAG_MASK) >> MTE_TAG_SHIFT);
}

int64_t run_sysctl_test(const char *t, int64_t value);

void expect_signal_impl(int signal, char *signal_name, void (^fn)(void), const char *msg);
#define expect_signal(signal, fn, msg) (expect_signal_impl((signal), #signal, (fn), (msg)))
void expect_sigkill(void (^fn)(void), const char *msg);

void expect_normal_exit(void (^fn)(void), const char *msg);
void assert_normal_exit(void (^fn)(void), const char *msg);

/*
 * Uses vm_allocate to allocate `size` bytes of untagged memory and
 * returns the untagged pointer to the caller.
 *
 * If mask is non-NULL, writes out a mask which can be passed in to
 * __arm_mte_create_random_tag to exclude the zero tag.
 */
void *allocate_untagged_memory(mach_vm_size_t size);

/*
 * Uses allocate_tagged_memory to get a tagged memory range, and also applies
 * a tag value to each of the MTE granules within the new allocation.
 */
#define TAG_RANDOM_EXCLUDE(x) ((uintptr_t) ~x)
#define TAG_RANDOM TAG_RANDOM_EXCLUDE(0)
vm_address_t allocate_and_tag_range(mach_vm_size_t size, uintptr_t tag);
#endif

/* Posix spawn a process with any specified posix_spawn_secflag_options */
void
posix_spawn_with_flags_and_assert_successful_exit(
	char *const*args,
	posix_spawn_secflag_options flags,
	bool expect_mte,
	bool should_kill_child
	);

/*
 * Uses vm_allocate to allocate `size` bytes of tagged memory and
 * returns the untagged pointer to the caller.
 */
void *allocate_tagged_memory(mach_vm_size_t size, uint64_t *mask);

/* Utility to return the output of a output of a uint64_t sysctl */
uint64_t
sysctl_get_Q(const char *name);
