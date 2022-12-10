/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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
 *  CCCryptorReset_internal.h
 */

#ifndef CCCryptorReset_internal_h
#define CCCryptorReset_internal_h

#if defined(_MSC_VER) || defined(__ANDROID__)

#define ProgramLinkedOnOrAfter_macOS1013_iOS11() true

#else

#include <mach-o/dyld_priv.h>
#define ProgramLinkedOnOrAfter_macOS1013_iOS11() dyld_program_sdk_at_least(dyld_fall_2017_os_versions)

#endif

#endif /* CCCryptorReset_internal_h */
