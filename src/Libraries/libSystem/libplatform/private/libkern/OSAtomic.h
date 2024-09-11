/*
 * Copyright (c) 2004-2016 Apple Inc. All rights reserved.
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

#ifndef _OSATOMIC_PRIVATE_H_
#define _OSATOMIC_PRIVATE_H_

#include <sys/cdefs.h>

#if __has_include(<libkern/OSAtomicDeprecated.h>) && \
		__has_include(<libkern/OSSpinLockDeprecated.h>) && \
		__has_include(<libkern/OSAtomicQueue.h>)


#include <libkern/OSAtomicDeprecated.h>
#include <libkern/OSSpinLockDeprecated.h>
#include <libkern/OSAtomicQueue.h>

#else

// #include_next <libkern/OSAtomic.h>

#endif

#endif // _OSATOMIC_PRIVATE_H_
