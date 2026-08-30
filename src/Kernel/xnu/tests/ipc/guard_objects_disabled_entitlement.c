/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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
#include <darwintest_utils.h>

#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/mach_vm.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <mach/task.h>
#include <sys/proc_info.h>
#include <sys/proc_info_private.h>

#include "../task_security_config.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.spawn"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_TAG_VM_PREFERRED);

T_DECL(test_guard_objects_entitlement_disabled,
    "entitlement should disable the guard-objects mitigation in task info",
    T_META_CHECK_LEAKS(false),
    T_META_BOOTARGS_SET("amfi_allow_any_signature=1"))
{
	struct task_security_config_info config;
	mach_msg_type_number_t count;
	kern_return_t kr;

	count = TASK_SECURITY_CONFIG_INFO_COUNT;
	kr = task_info(mach_task_self(), TASK_SECURITY_CONFIG_INFO, (task_info_t)&config, &count);
	T_ASSERT_MACH_SUCCESS(kr, "task_info(TASK_SECURITY_CONFIG_INFO)");
	T_ASSERT_EQ(count, 1, "security config should return 1 value");

	struct task_security_config *conf = (struct task_security_config*)&config;

	T_EXPECT_FALSE(conf->guard_objects, "guard-objects bit should not be set");
	T_EXPECT_GE_UINT(conf->hardened_process_version, 2, "hardened-process version should be set");
}

T_DECL(test_guard_objects_entitlements_proc_info_disabled,
    "entitlement should disable the guard-objects mitigation in proc info",
    T_META_CHECK_LEAKS(false),
    T_META_BOOTARGS_SET("amfi_allow_any_signature=1"))
{
	struct proc_bsdinfo bsd_info = {0};
	struct proc_bsdshortinfo bsd_shortinfo = {0};
	int ret = 0;

	ret = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info));
	T_ASSERT_EQ(ret, (int)sizeof(bsd_info), "proc_pidinfo PROC_PIDTBSDINFO should return the size of proc_bsdinfo structure");

	T_EXPECT_BITS_NOTSET(bsd_info.pbi_flags, PROC_FLAG_GUARD_OBJECTS_ENABLED, "bsd_info.pbi_flags should not have guard-objects flag set");

	ret = proc_pidinfo(getpid(), PROC_PIDT_SHORTBSDINFO, 0, &bsd_shortinfo, sizeof(bsd_shortinfo));
	T_ASSERT_EQ(ret, (int)sizeof(bsd_shortinfo), "proc_pidinfo PROC_PIDT_SHORTBSDINFO should return the size of proc_bsdshortinfo structure");

	T_EXPECT_BITS_NOTSET(bsd_shortinfo.pbsi_flags, PROC_FLAG_GUARD_OBJECTS_ENABLED, "bsd_shortinfo.pbsi_flags should not have guard-objects flag set");
}
