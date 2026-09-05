/*
 * Copyright (c) 2025 by Apple Inc.. All rights reserved.
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

#ifndef __DYLD_VERSION_DEFINES__
#define __DYLD_VERSION_DEFINES__ 1


// @@DYLD_HEADER_VERSIONS()@@

// These exist to maintain source compat as we cleanup headers
// TODO: These should be marked for macro deprecation and eventually removed
#define DYLD_MACOSX_VERSION_11_00               DYLD_MACOSX_VERSION_11_0
#define DYLD_MACOSX_VERSION_12_00               DYLD_MACOSX_VERSION_12_0
#define dyld_platform_version_macOS_11_00       dyld_platform_version_macOS_11_0
#define dyld_platform_version_macOS_12_00       dyld_platform_version_macOS_12_0




#if 0
#pragma clang deprecated(DYLD_MACOSX_VERSION_11_00, "use DYLD_MACOSX_VERSION_11_0 instead")
#pragma clang deprecated(DYLD_MACOSX_VERSION_12_00, "use DYLD_MACOSX_VERSION_12_0 instead")
#pragma clang deprecated(dyld_platform_version_macOS_11_00, "use dyld_platform_version_macOS_11_0 instead")
#pragma clang deprecated(dyld_platform_version_macOS_12_00, "use dyld_platform_version_macOS_12_0 instead")
#endif
#endif // __DYLD_VERSION_DEFINES__

