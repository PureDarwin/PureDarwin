/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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
#include <arm_acle.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "arm_mte_utilities.h"

static void
assert_posix_success(int ret, char *msg)
{
	if (ret == -1) {
		fprintf(stderr, "error in process being debugged: %s\n", msg);
		exit(1);
	}
}

/*
 * This program is used by the mte_debugger_untagged_copyio test in
 * arm_mte_unentitled.c. In this setup, the test is the "debugger" and this code
 * is the "program being debugged".
 */
int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: arm_mte_debugger_helper shm_name\n");
		fprintf(stderr, "this is intended to be called from test code\n");
		exit(1);
	}
	int ret;
	struct proc_bsdinfo bsd_info;
	ret = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info));
	if (ret != sizeof(bsd_info)) {
		fprintf(stderr, "PROC_PIDTBSDINFO");
		exit(1);
	}
	if (!(bsd_info.pbi_flags & PROC_FLAG_SEC_ENABLED)) {
		fprintf(stderr, "arm_mte_debugger_helper launched without MTE enabled");
		exit(1);
	}

	char *shm_name = argv[1];
	int fd = shm_open(shm_name, O_RDWR);
	assert_posix_success(fd, "shm_open");

	vm_address_t *shm = mmap(NULL, sizeof(vm_address_t), PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, 0);
	if ((void*) shm == MAP_FAILED) {
		fprintf(stderr, "mmap");
		exit(1);
	}

	/* the test will tell us how much memory to allocate via the shared memory */
	const vm_size_t ALLOC_SIZE = (mach_vm_size_t) *shm;

	/* allocate some tagged memory */
	vm_address_t addr = 0;
	kern_return_t kr = vm_allocate(mach_task_self(), &addr, ALLOC_SIZE,
	    VM_FLAGS_ANYWHERE | VM_FLAGS_MTE);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "vm_allocate");
		exit(1);
	}

	/* write some tags and data */
	uint64_t mask = __arm_mte_exclude_tag((void*) addr, 0);
	uint8_t *tagged_ptr = __arm_mte_create_random_tag((void*) addr, mask);
	for (vm_size_t i = 0; i < ALLOC_SIZE; i += MTE_GRANULE_SIZE) {
		__arm_mte_set_tag(tagged_ptr + i);
	}
	memset(tagged_ptr, 'A', ALLOC_SIZE);

	/* send the tagged memory's address back to the "debugger" so it can remap it */
	*shm = addr;
	msync((void*) shm, sizeof(vm_address_t), MS_SYNC | MS_INVALIDATE);

	ret = close(fd);
	assert_posix_success(ret, "close");

	/* when the parent process dies, the child will be given to launchd (pid=1) */
	while (getppid() != 1) {
		/* we need to keep the process alive so the memory doesn't go away */
		sleep(1);
	}
}
