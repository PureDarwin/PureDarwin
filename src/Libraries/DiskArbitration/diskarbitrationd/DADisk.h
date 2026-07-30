/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DADISK__
#define __DISKARBITRATIOND_DADISK__

#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOKitLib.h>

#include "DAFileSystem.h"
#include "DASession.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct __DADisk * DADiskRef;

typedef UInt32 DADiskOption;

enum
{
    _kDADiskStateMountAutomatic        = 0x00000010,
    _kDADiskStateMountAutomaticNoDefer = 0x00000020,

    kDADiskStateCommandActive       = 0x00000001,
    kDADiskStateRequireRepair       = 0x00000002,
    kDADiskStateRequireRepairQuotas = 0x00000004,
    kDADiskStateStagedProbe         = 0x00010000,
    kDADiskStateStagedPeek          = 0x00020000,
    kDADiskStateStagedMount         = 0x00040000,
    kDADiskStateStagedAppear        = 0x00080000,
    kDADiskStateStagedUnrepairable  = 0x00100000,
    kDADiskStateZombie              = 0x10000000
};

typedef UInt32 DADiskState;

extern CFComparisonResult DADiskCompareDescription( DADiskRef disk, CFStringRef description, CFTypeRef value );
extern DADiskRef          DADiskCreateFromIOMedia( CFAllocatorRef allocator, io_service_t media );
extern DADiskRef          DADiskCreateFromVolumePath( CFAllocatorRef allocator, const struct statfs * fs );
extern CFAbsoluteTime     DADiskGetBusy( DADiskRef disk );
extern io_object_t        DADiskGetBusyNotification( DADiskRef disk );
extern CFURLRef           DADiskGetBypath( DADiskRef disk );
extern const char *       DADiskGetBSDLink( DADiskRef disk, Boolean raw );
extern dev_t              DADiskGetBSDNode( DADiskRef disk );
extern const char *       DADiskGetBSDPath( DADiskRef disk, Boolean raw );
extern UInt32             DADiskGetBSDUnit( DADiskRef disk );
extern DACallbackRef      DADiskGetClaim( DADiskRef disk );
extern CFTypeRef          DADiskGetContext( DADiskRef disk );
extern CFTypeRef          DADiskGetContextRe( DADiskRef disk );
extern CFTypeRef          DADiskGetDescription( DADiskRef disk, CFStringRef description );
extern CFURLRef           DADiskGetDevice( DADiskRef disk );
extern DAFileSystemRef    DADiskGetFileSystem( DADiskRef disk );
extern const char *       DADiskGetID( DADiskRef disk );
extern io_service_t       DADiskGetIOMedia( DADiskRef disk );
extern mode_t             DADiskGetMode( DADiskRef disk );
extern Boolean            DADiskGetOption( DADiskRef disk, DADiskOption option );
extern DADiskOptions      DADiskGetOptions( DADiskRef disk );
extern io_object_t        DADiskGetPropertyNotification( DADiskRef disk );
extern CFDataRef          DADiskGetSerialization( DADiskRef disk );
extern Boolean            DADiskGetState( DADiskRef disk, DADiskState state );
extern CFTypeID           DADiskGetTypeID( void );
extern gid_t              DADiskGetUserGID( DADiskRef disk );
extern uid_t              DADiskGetUserUID( DADiskRef disk );
extern void               DADiskInitialize( void );
extern Boolean            DADiskMatch( DADiskRef disk, CFDictionaryRef match );
extern void               DADiskSetBusy( DADiskRef disk, CFAbsoluteTime busy );
extern void               DADiskSetBusyNotification( DADiskRef disk, io_object_t notification );
extern void               DADiskSetBypath( DADiskRef disk, CFURLRef bypath );
extern void               DADiskSetBSDLink( DADiskRef disk, Boolean raw, const char * link );
extern void               DADiskSetClaim( DADiskRef disk, DACallbackRef claim );
extern void               DADiskSetContext( DADiskRef disk, CFTypeRef context );
extern void               DADiskSetContextRe( DADiskRef disk, CFTypeRef context );
extern void               DADiskSetDescription( DADiskRef disk, CFStringRef description, CFTypeRef value );
extern void               DADiskSetFileSystem( DADiskRef disk, DAFileSystemRef filesystem );
extern void               DADiskSetOption( DADiskRef disk, DADiskOption option, Boolean value );
extern void               DADiskSetOptions( DADiskRef disk, DADiskOptions options, Boolean value );
extern void               DADiskSetPropertyNotification( DADiskRef disk, io_object_t notification );
extern void               DADiskSetState( DADiskRef disk, DADiskState state, Boolean value );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DADISK__ */
