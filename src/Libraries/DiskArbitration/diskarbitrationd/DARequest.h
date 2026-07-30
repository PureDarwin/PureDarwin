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

#ifndef __DISKARBITRATIOND_DAREQUEST__
#define __DISKARBITRATIOND_DAREQUEST__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"
#include "DADissenter.h"
#include "DAInternal.h"
#include "DASession.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct __DARequest * DARequestRef;

enum
{
///w:start
    _kDARequestStateMountArgumentNoWrite = 0x08000000,
    _kDARequestStateStagedAuthorize      = 0x00200000,
///w:stop
    kDARequestStateStagedProbe   = 0x00010000,
    kDARequestStateStagedApprove = 0x00100000
};

typedef UInt32 DARequestState;

extern DARequestRef DARequestCreate( CFAllocatorRef allocator,
                                     _DARequestKind kind,
                                     DADiskRef      argument0,
                                     CFIndex        argument1,
                                     CFTypeRef      argument2,
                                     CFTypeRef      argument3,
                                     uid_t          userUID,
                                     gid_t          userGID,
                                     DACallbackRef  callback );

extern Boolean DARequestDispatch( DARequestRef request );

extern void           DARequestDispatchCallback( DARequestRef request, DAReturn status );
extern CFIndex        DARequestGetArgument1( DARequestRef request );
extern CFTypeRef      DARequestGetArgument2( DARequestRef request );
extern CFTypeRef      DARequestGetArgument3( DARequestRef request );
extern DACallbackRef  DARequestGetCallback( DARequestRef request );
extern DADiskRef      DARequestGetDisk( DARequestRef request );
extern DADissenterRef DARequestGetDissenter( DARequestRef request );
extern _DARequestKind DARequestGetKind( DARequestRef request );
extern CFArrayRef     DARequestGetLink( DARequestRef request );
extern Boolean        DARequestGetState( DARequestRef request, DARequestState state );
extern gid_t          DARequestGetUserGID( DARequestRef request );
extern uid_t          DARequestGetUserUID( DARequestRef request );
extern void           DARequestSetCallback( DARequestRef request, DACallbackRef callback );
extern void           DARequestSetDissenter( DARequestRef request, DADissenterRef dissenter );
extern void           DARequestSetLink( DARequestRef request, CFArrayRef link );
extern void           DARequestSetState( DARequestRef request, DARequestState state, Boolean value );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAREQUEST__ */
