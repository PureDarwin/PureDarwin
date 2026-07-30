/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DAMOUNT__
#define __DISKARBITRATIOND_DAMOUNT__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum
{
    kDAMountPointActionLink,
    kDAMountPointActionMake,
    kDAMountPointActionMove,
    kDAMountPointActionNone
};

typedef UInt32 DAMountPointAction;

enum
{
    kDAMountPreferenceDefer,
    kDAMountPreferenceTrust,
    kDAMountPreferenceDisableAutoMount
};

typedef UInt32 DAMountPreference;

typedef void ( *DAMountCallback )( int status, CFURLRef mountpoint, void * context );

extern void DAMount( DADiskRef       disk,
                     CFURLRef        mountpoint,
                     DAMountCallback callback,
                     void *          callbackContext );

extern void DAMountWithArguments( DADiskRef       disk,
                                  CFURLRef        mountpoint,
                                  DAMountCallback callback,
                                  void *          callbackContext,
                                  ... );

extern Boolean DAMountContainsArgument( CFStringRef arguments, CFStringRef argument );

extern CFURLRef DAMountCreateMountPoint( DADiskRef disk );

extern CFURLRef DAMountCreateMountPointWithAction( DADiskRef disk, DAMountPointAction action );

extern Boolean DAMountGetPreference( DADiskRef disk, DAMountPreference preference );

extern void DAMountRemoveMountPoint( CFURLRef mountpoint );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAMOUNT__ */
