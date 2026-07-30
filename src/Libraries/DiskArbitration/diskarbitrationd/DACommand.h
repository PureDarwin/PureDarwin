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

#ifndef __DISKARBITRATIOND_DACOMMAND__
#define __DISKARBITRATIOND_DACOMMAND__

#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum
{
    kDACommandExecuteOptionDefault       = 0x00000000,
    kDACommandExecuteOptionCaptureOutput = 0x00000001
};

typedef UInt32 DACommandExecuteOptions;

typedef void ( *DACommandExecuteCallback )( int status, CFDataRef output, void * context );

extern CFRunLoopSourceRef DACommandCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order );

extern void DACommandExecute( CFURLRef                 executable,
                              DACommandExecuteOptions  options,
                              uid_t                    userUID,
                              gid_t                    userGID,
                              DACommandExecuteCallback callback,
                              void *                   callbackContext,
                              ... );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DACOMMAND__ */
