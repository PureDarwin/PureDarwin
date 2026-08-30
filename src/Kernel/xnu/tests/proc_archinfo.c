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
#include <libproc.h>

T_GLOBAL_META(
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("libsyscall"),
	T_META_OWNER("m_staveleytaylor"),
	T_META_RUN_CONCURRENTLY(true)
	);

T_DECL(proc_archinfo, "Check proc_archinfo is exposed in public headers")
{
	struct proc_archinfo pai = {0};
	pid_t pid = getpid();

	T_EXPECT_POSIX_SUCCESS(proc_pidinfo(pid, PROC_PIDARCHINFO, 0, &pai, sizeof(pai)), "proc_pidinfo(PROC_PIDARCHINFO)");

	/* checks from tests/proc_info.c */
#if defined(__arm__) || defined(__arm64__)
	bool arm = (pai.p_cputype & CPU_TYPE_ARM) == CPU_TYPE_ARM;
	bool arm64 = (pai.p_cputype & CPU_TYPE_ARM64) == CPU_TYPE_ARM64;
	if (!arm && !arm64) {
		T_EXPECT_EQ_INT(pai.p_cputype, CPU_TYPE_ARM, "PROC_PIDARCHINFO returned valid value for p_cputype");
	}
	T_EXPECT_EQ_INT((pai.p_cpusubtype & CPU_SUBTYPE_ARM_ALL), CPU_SUBTYPE_ARM_ALL,
	    "PROC_PIDARCHINFO returned valid value for p_cpusubtype");
#else
	bool x86 = (pai.p_cputype & CPU_TYPE_X86) == CPU_TYPE_X86;
	bool x86_64 = (pai.p_cputype & CPU_TYPE_X86_64) == CPU_TYPE_X86_64;
	if (!x86 && !x86_64) {
		T_EXPECT_EQ_INT(pai.p_cputype, CPU_TYPE_X86, "PROC_PIDARCHINFO returned valid value for p_cputype");
	}
#endif
}
