/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _KERN_MEM_ACCT_H
#define _KERN_MEM_ACCT_H

#ifdef XNU_KERNEL_PRIVATE

#include <sys/mem_acct_private.h>

struct mem_acct;
/*
 * Add "size" to the memory accounting module of "type".
 */
__private_extern__ void _mem_acct_add(struct mem_acct *macct, int size);
__private_extern__ struct mem_acct *mem_acct_register(
	const char *__null_terminated name, uint64_t hardlimit, uint8_t percent);

/*
 * pre-softlimit means we are getting close to the softlimit (about 80% of it).
 * The subsystem should start taking preventive actions.
 */
#define MEMACCT_PRESOFTLIMIT 1
/*
 * We are at the softlimit. Take actions to reduce memory usage, but don't take
 * fully destructive actions yet.
 */
#define MEMACCT_SOFTLIMIT 2
/*
 * We are above the hardlimit. Prevent holding on to memory in this subsystem.
 */
#define MEMACCT_HARDLIMIT 3

extern int mem_acct_limited(const struct mem_acct *macct);

static inline void
mem_acct_add(struct mem_acct *macct, unsigned int size)
{
	_mem_acct_add(macct, size);
}

static inline void
mem_acct_sub(struct mem_acct *macct, unsigned int size)
{
	_mem_acct_add(macct, -size);
}


#endif /* XNU_KERNEL_PRIVATE */

#endif /*_KERN_MEM_ACCT_H */
