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

#ifndef __DISKARBITRATIOND_DAFILESYSTEM__
#define __DISKARBITRATIOND_DAFILESYSTEM__

#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct __DAFileSystem * DAFileSystemRef;

extern const CFStringRef kDAFileSystemMountArgumentForce;
extern const CFStringRef kDAFileSystemMountArgumentNoDevice;
extern const CFStringRef kDAFileSystemMountArgumentNoExecute;
extern const CFStringRef kDAFileSystemMountArgumentNoOwnership;
extern const CFStringRef kDAFileSystemMountArgumentNoSetUserID;
extern const CFStringRef kDAFileSystemMountArgumentNoWrite;
extern const CFStringRef kDAFileSystemMountArgumentUnion;
extern const CFStringRef kDAFileSystemMountArgumentUpdate;

extern const CFStringRef kDAFileSystemUnmountArgumentForce;

typedef void ( *DAFileSystemCallback )( int status, void * context );

typedef void ( *DAFileSystemProbeCallback )( int status, CFBooleanRef clean, CFStringRef name, CFStringRef type, CFUUIDRef uuid, void * context );

extern CFStringRef _DAFileSystemCopyName( DAFileSystemRef filesystem, CFURLRef mountpoint );

extern CFUUIDRef _DAFileSystemCreateUUIDFromString( CFAllocatorRef allocator, CFStringRef string );

extern DAFileSystemRef DAFileSystemCreate( CFAllocatorRef allocator, CFURLRef path );

extern CFRunLoopSourceRef DAFileSystemCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order );

extern CFStringRef DAFileSystemGetKind( DAFileSystemRef filesystem );

extern CFDictionaryRef DAFileSystemGetProbeList( DAFileSystemRef filesystem );

extern CFTypeID DAFileSystemGetTypeID( void );

extern void DAFileSystemInitialize( void );

extern void DAFileSystemMount( DAFileSystemRef      filesystem,
                               CFURLRef             device,
                               CFURLRef             mountpoint,
                               uid_t                userUID,
                               gid_t                userGID,
                               DAFileSystemCallback callback,
                               void *               callbackContext );

extern void DAFileSystemMountWithArguments( DAFileSystemRef      filesystem,
                                            CFURLRef             device,
                                            CFURLRef             mountpoint,
                                            uid_t                userUID,
                                            gid_t                userGID,
                                            DAFileSystemCallback callback,
                                            void *               callbackContext,
                                            ... );

extern void DAFileSystemProbe( DAFileSystemRef           filesystem,
                               CFURLRef                  device,
                               DAFileSystemProbeCallback callback,
                               void *                    callbackContext );

extern void DAFileSystemRename( DAFileSystemRef      filesystem,
                                CFURLRef             mountpoint,
                                CFStringRef          name,
                                DAFileSystemCallback callback,
                                void *               callbackContext );

extern void DAFileSystemRepair( DAFileSystemRef      filesystem,
                                CFURLRef             device,
                                DAFileSystemCallback callback,
                                void *               callbackContext );

extern void DAFileSystemRepairQuotas( DAFileSystemRef      filesystem,
                                      CFURLRef             mountpoint,
                                      DAFileSystemCallback callback,
                                      void *               callbackContext );

extern void DAFileSystemUnmount( DAFileSystemRef      filesystem,
                                 CFURLRef             mountpoint,
                                 DAFileSystemCallback callback,
                                 void *               callbackContext );

extern void DAFileSystemUnmountWithArguments( DAFileSystemRef      filesystem,
                                              CFURLRef             mountpoint,
                                              DAFileSystemCallback callback,
                                              void *               callbackContext,
                                              ... );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAFILESYSTEM__ */
