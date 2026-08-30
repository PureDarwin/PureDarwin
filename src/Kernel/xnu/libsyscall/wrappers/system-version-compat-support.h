/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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
#ifndef __SYSTEM_VERSION_COMPAT_SUPPORT_H
#define __SYSTEM_VERSION_COMPAT_SUPPORT_H

#include <TargetConditionals.h>

#if TARGET_OS_OSX && !defined(__i386__)
#define SYSTEM_VERSION_COMPAT_ENABLED 1
#define SYSTEM_VERSION_COMPAT_HAS_MODE_MACOSX 1
#define SYSTEM_VERSION_COMPAT_HAS_MODE_IOS 1
#define SYSTEM_VERSION_COMPAT_NEEDS_SYSCTL 1
#define SYSTEM_VERSION_COMPAT_SHIM_OS_CRYPTEX 0
#endif


#if defined(RC_EXPERIMENTAL_SYSTEM_VERSION_COMPAT)
#ifndef SYSTEM_VERSION_COMPAT_ENABLED
#define SYSTEM_VERSION_COMPAT_ENABLED 1
#endif

/* Force enabling macOS mode */
#ifdef SYSTEM_VERSION_COMPAT_HAS_MODE_MACOSX
#undef SYSTEM_VERSION_COMPAT_HAS_MODE_MACOSX
#endif
#define SYSTEM_VERSION_COMPAT_HAS_MODE_MACOSX 1

#ifndef SYSTEM_VERSION_COMPAT_HAS_MODE_IOS
#define SYSTEM_VERSION_COMPAT_HAS_MODE_IOS 0
#endif

/* Force disabling sysctl submission */
#ifdef SYSTEM_VERSION_COMPAT_NEEDS_SYSCTL
#undef SYSTEM_VERSION_COMPAT_NEEDS_SYSCTL
#endif
#define SYSTEM_VERSION_COMPAT_NEEDS_SYSCTL 0

/* Force shimming path from OS cryptex */
#ifdef SYSTEM_VERSION_COMPAT_SHIM_OS_CRYPTEX
#undef SYSTEM_VERSION_COMPAT_SHIM_OS_CRYPTEX
#endif
#define SYSTEM_VERSION_COMPAT_SHIM_OS_CRYPTEX 1
#endif /* defined(RC_EXPERIMENTAL_SYSTEM_VERSION_COMPAT) */

#if SYSTEM_VERSION_COMPAT_ENABLED
typedef enum system_version_compat_mode {
	SYSTEM_VERSION_COMPAT_MODE_DISABLED = 0,
	SYSTEM_VERSION_COMPAT_MODE_MACOSX = 1,
	SYSTEM_VERSION_COMPAT_MODE_IOS = 2,
} system_version_compat_mode_t;
#endif /* SYSTEM_VERSION_COMPAT_ENABLED */

#endif /* __SYSTEM_VERSION_COMPAT_SUPPORT_H */
