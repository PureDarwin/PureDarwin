/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DASESSION__
#define __DISKARBITRATIOND_DASESSION__

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <Security/Authorization.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum
{
    kDASessionOptionNoTimeout = 0x01000000
};

typedef UInt32 DASessionOption;

typedef UInt32 DASessionOptions;

enum
{
    kDASessionStateIdle    = 0x00000001,
    kDASessionStateTimeout = 0x01000000,
    kDASessionStateZombie  = 0x10000000
};

typedef UInt32 DASessionState;

typedef struct __DACallback * DACallbackRef;

typedef struct __DASession * DASessionRef;

///w:start
extern const char * _DASessionGetName( DASessionRef session );
///w:stop
extern DASessionRef      DASessionCreate( CFAllocatorRef allocator, const char * _name, pid_t _pid );
extern AuthorizationRef  DASessionGetAuthorization( DASessionRef session );
extern CFMutableArrayRef DASessionGetCallbackQueue( DASessionRef session );
extern CFMutableArrayRef DASessionGetCallbackRegister( DASessionRef session );
extern mach_port_t       DASessionGetID( DASessionRef session );
extern Boolean           DASessionGetOption( DASessionRef session, DASessionOption option );
extern DASessionOptions  DASessionGetOptions( DASessionRef session );
extern mach_port_t       DASessionGetServerPort( DASessionRef session );
extern Boolean           DASessionGetState( DASessionRef session, DASessionState state );
extern CFTypeID          DASessionGetTypeID( void );
extern void              DASessionInitialize( void );
extern void              DASessionQueueCallback( DASessionRef session, DACallbackRef callback );
extern void              DASessionRegisterCallback( DASessionRef session, DACallbackRef callback );
extern void              DASessionScheduleWithRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode );
extern void              DASessionSetAuthorization( DASessionRef session, AuthorizationRef authorization );
extern void              DASessionSetClientPort( DASessionRef session, mach_port_t client );
extern void              DASessionSetOption( DASessionRef session, DASessionOption option, Boolean value );
extern void              DASessionSetOptions( DASessionRef session, DASessionOptions options, Boolean value );
extern void              DASessionSetState( DASessionRef session, DASessionState state, Boolean value );
extern void              DASessionUnregisterCallback( DASessionRef session, DACallbackRef callback );
extern void              DASessionUnscheduleFromRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DASESSION__ */
