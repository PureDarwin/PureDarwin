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
 *
 */

#include <darwintest.h>
#include <darwintest_utils.h>

#include <stdlib.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_OWNER("jharmening"),
	T_META_CHECK_LEAKS(false),
	T_META_RUN_CONCURRENTLY(true));

#define NUM_IOVS 7

T_DECL(vm_vector_upl,
    "Test for vector UPLs",
    T_META_TAG_VM_PREFERRED)
{
	struct {
		uint64_t base;
		uint32_t len;
	} w_iovs[NUM_IOVS];
	int64_t expected_bytes;
	int w, w_idx;

	T_SETUPBEGIN;
	expected_bytes = 0;
	for (w = 0; w < NUM_IOVS; w++) {
		w_iovs[w].len = (uint32_t) ((w + 1) * (int)PAGE_SIZE);
		void *iov_base;
		T_QUIET; T_ASSERT_POSIX_SUCCESS(posix_memalign(&iov_base, PAGE_SIZE, w_iovs[w].len), "alloc(w_iov_base[%d])", w);
		memset(iov_base, 'a' + w, w_iovs[w].len);
		w_iovs[w].base = (uint64_t)iov_base;
		expected_bytes += w_iovs[w].len;
	}
	T_SETUPEND;

	struct {
		uint64_t iov;
		uint16_t iovcnt;
	} arg;

	arg.iov = (uint64_t) &w_iovs[0];
	arg.iovcnt = NUM_IOVS;

	int64_t addr = (int64_t)&arg;
	int64_t result = 0;
	size_t s = sizeof(result);
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("debug.test.vm_vector_upl", &result, &s, &addr, sizeof(addr)),
	    "sysctlbyname(debug.test.vm_vector_upl)");

	T_EXPECT_EQ_LLONG(result, expected_bytes, "sysctl output");

	w = 0;
	w_idx = 0;

	/* Validate that the kernel sysctl handler mapped and mutated the page contents as expected. */
	for (w = 0; w < NUM_IOVS; w++) {
		char *iov_base = (char*)w_iovs[w].base;
		for (w_idx = 0; w_idx < w_iovs[w].len; w_idx++) {
			T_QUIET; T_ASSERT_EQ(iov_base[w_idx], 'a' + w + 1,
			    "w_iovs[%d].iov_base[%d]='%c' == '%c'",
			    w, w_idx, (unsigned char)iov_base[w_idx], (unsigned char)('a' + w + 1));
		}
	}

	T_PASS("%s", __FUNCTION__);
}
