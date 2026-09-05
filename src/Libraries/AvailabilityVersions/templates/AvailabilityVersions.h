/*
 * Copyright (c) 2019 by Apple Inc.. All rights reserved.
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
 
#ifndef __AVAILABILITY_VERSIONS__
#define __AVAILABILITY_VERSIONS__

// @@AVAILABILITY_VERSION_DEFINES()@@


/*
 * Set up standard Mac OS X versions
 */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || defined(_DARWIN_C_SOURCE)

// @@ALIAS_VERSION_MACROS(macos, MAC_OS_X_VERSION_,__MAC_,maxVersion=0x000B0000)@@
// @@ALIAS_VERSION_MACROS(macos, MAC_OS_VERSION_,__MAC_,minVersion=0x000B0000)@@

#endif /* #if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || defined(_DARWIN_C_SOURCE) */

#endif /* __AVAILABILITY_VERSIONS__ */

