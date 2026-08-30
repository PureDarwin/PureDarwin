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

#include <sys/systm.h>

#include <net/pktsched/pktsched_ops.h>

pktsched_ops_list_t pktsched_ops_list;

void
pktsched_ops_register(pktsched_ops_t *new_ops)
{
	pktsched_ops_t *ops;

	ASSERT(new_ops->ps_deq != NULL);
	ASSERT(new_ops->ps_enq != NULL);
	ASSERT(new_ops->ps_deq_sc != NULL);
	ASSERT(new_ops->ps_setup != NULL);
	ASSERT(new_ops->ps_teardown != NULL);
	ASSERT(new_ops->ps_req != NULL);
	ASSERT(new_ops->ps_allow_dequeue != NULL);

	LIST_FOREACH(ops, &pktsched_ops_list, ps_ops_link) {
		VERIFY(ops->ps_id != new_ops->ps_id);
	}

	LIST_INSERT_HEAD(&pktsched_ops_list, new_ops, ps_ops_link);
}

pktsched_ops_t *
pktsched_ops_find(uint8_t ops_id)
{
	pktsched_ops_t *ops;

	LIST_FOREACH(ops, &pktsched_ops_list, ps_ops_link) {
		if (ops->ps_id == ops_id) {
			return ops;
		}
	}

	return NULL;
}
