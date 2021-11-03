/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

/*
 * This file implements the following functions for the arm64 architecture.
 *
 *		void  OSAtomicFifoEnqueue( OSFifoQueueHead *__list,	void *__new,
 *			size_t __offset);
 *		void* OSAtomicFifoDequeue( OSFifoQueueHead *__list, size_t __offset);
 *
 */

#include <stdio.h>
#include <machine/cpu_capabilities.h>

#include "libkern/OSAtomic.h"
#include "../OSAtomicFifo.h"

typedef void (OSAtomicFifoEnqueue_t)(OSFifoQueueHead *, void *, size_t);
typedef void *(OSAtomicFifoDequeue_t)(OSFifoQueueHead *, size_t);

void OSAtomicFifoEnqueue(OSFifoQueueHead *__list, void *__new, size_t __offset)
{
	void *addr = commpage_pfz_base;
	addr += _COMM_PAGE_TEXT_ATOMIC_ENQUEUE;

	OSAtomicFifoEnqueue_t *OSAtomicFifoEnqueueInternal = SIGN_PFZ_FUNCTION_PTR(addr);

	return OSAtomicFifoEnqueueInternal(__list, __new, __offset);
}

void * OSAtomicFifoDequeue( OSFifoQueueHead *__list, size_t __offset)
{
	void *addr = commpage_pfz_base;
	addr += _COMM_PAGE_TEXT_ATOMIC_DEQUEUE;

	OSAtomicFifoDequeue_t *OSAtomicFifoDequeueInternal = SIGN_PFZ_FUNCTION_PTR(addr);

	return OSAtomicFifoDequeueInternal(__list, __offset);
}

#endif
