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
#include <darwintest_posix.h>
#include <sys/errno.h>
#include <stdint.h>
#include <sys/sysctl.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.trial"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("sysctl"),
	T_META_CHECK_LEAKS(false),
	T_META_RUN_CONCURRENTLY(true),
	T_META_TAG_VM_PREFERRED,
	T_META_ASROOT(false));


#if defined(ENTITLED)
T_DECL(kern_trial_sysctl_entitled,
    "test that kern.trial sysctls can be read-from/written-to if the proper "
    "entitlement is granted")
#else
T_DECL(kern_trial_sysctl_unentitled,
    "test that kern.trial sysctls cannot be read-from/written-to without "
    "the proper entitlement")
#endif
{
	int ret;
	int32_t val;
	size_t sz = sizeof(val);

	ret = sysctlbyname("kern.trial.test", &val, &sz, NULL, 0);
#if defined(ENTITLED)
	T_ASSERT_POSIX_SUCCESS(ret, "kern.trial.test can be read from");
#else
	T_EXPECT_POSIX_FAILURE(ret, EPERM, "kern.trial.test cannot be written to");
#endif

	val = 1;
	ret = sysctlbyname("kern.trial.test", NULL, 0, &val, sizeof(val));
#if !defined(ENTITLED)
	T_EXPECT_POSIX_FAILURE(ret, EPERM, "kern.trial.test cannot be written to");
#else
	T_EXPECT_POSIX_SUCCESS(ret, "kern.trial.test can be written to with a valid value");

	ret = sysctlbyname("kern.trial.test", &val, &sz, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.trial.test can be read from");
	T_EXPECT_EQ(val, 1, "kern.trial.test written value took effect");

	val = UINT32_MAX;
	ret = sysctlbyname("kern.trial.test", NULL, 0, &val, sizeof(val));
	T_EXPECT_POSIX_FAILURE(ret, EINVAL, "kern.trial.test cannot be written to with an invalid value");
#endif
}
