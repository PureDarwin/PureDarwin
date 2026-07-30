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

#ifndef __DISKARBITRATIOND_DACALLBACK__
#define __DISKARBITRATIOND_DACALLBACK__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"
#include "DAInternal.h"
#include "DASession.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern DACallbackRef DACallbackCreate( CFAllocatorRef   allocator,
                                       DASessionRef     session,
                                       mach_vm_offset_t address,
                                       mach_vm_offset_t context,
                                       _DACallbackKind  kind,
                                       CFIndex          order,
                                       CFDictionaryRef  match,
                                       CFArrayRef       watch );

extern DACallbackRef    DACallbackCreateCopy( CFAllocatorRef allocator, DACallbackRef callback );
extern mach_vm_offset_t DACallbackGetAddress( DACallbackRef callback );
extern CFTypeRef        DACallbackGetArgument0( DACallbackRef callback );
extern CFTypeRef        DACallbackGetArgument1( DACallbackRef callback );
extern mach_vm_offset_t DACallbackGetContext( DACallbackRef callback );
extern DADiskRef        DACallbackGetDisk( DACallbackRef callback );
extern _DACallbackKind  DACallbackGetKind( DACallbackRef callback );
extern CFDictionaryRef  DACallbackGetMatch( DACallbackRef callback );
extern SInt32           DACallbackGetOrder( DACallbackRef callback );
extern DASessionRef     DACallbackGetSession( DACallbackRef callback );
extern CFAbsoluteTime   DACallbackGetTime( DACallbackRef callback );
extern CFArrayRef       DACallbackGetWatch( DACallbackRef callback );
extern void             DACallbackSetArgument0( DACallbackRef callback, CFTypeRef argument0 );
extern void             DACallbackSetArgument1( DACallbackRef callback, CFTypeRef argument1 );
extern void             DACallbackSetDisk( DACallbackRef callback, DADiskRef disk );
extern void             DACallbackSetMatch( DACallbackRef callback, CFDictionaryRef match );
extern void             DACallbackSetSession( DACallbackRef callback, DASessionRef session );
extern void             DACallbackSetTime( DACallbackRef callback, CFAbsoluteTime time );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DACALLBACK__ */
