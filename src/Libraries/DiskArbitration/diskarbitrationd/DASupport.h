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

#ifndef __DISKARBITRATIOND_DASUPPORT__
#define __DISKARBITRATIOND_DASUPPORT__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"
#include "DAInternal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void ( *DAAuthorizeCallback )( DAReturn status, void * context );

extern DAReturn DAAuthorize( DASessionRef        session,
                             _DAAuthorizeOptions options,
                             DADiskRef           disk,
                             uid_t               userUID,
                             gid_t               userGID,
                             const char *        right );

extern void DAAuthorizeWithCallback( DASessionRef        session,
                                     _DAAuthorizeOptions options,
                                     DADiskRef           disk,
                                     uid_t               userUID,
                                     gid_t               userGID,
                                     DAAuthorizeCallback callback,
                                     void *              callbackContext,
                                     const char *        right );

extern const CFStringRef kDAFileSystemKey; /* ( DAFileSystem ) */

extern void DAFileSystemListRefresh( void );

extern const CFStringRef kDAMountMapMountAutomaticKey; /* ( CFBoolean ) */
extern const CFStringRef kDAMountMapMountOptionsKey;   /* ( CFString  ) */
extern const CFStringRef kDAMountMapMountPathKey;      /* ( CFURL     ) */
extern const CFStringRef kDAMountMapProbeIDKey;        /* ( CFUUID    ) */
extern const CFStringRef kDAMountMapProbeKindKey;      /* ( CFString  ) */

extern void DAMountMapListRefresh1( void );
extern void DAMountMapListRefresh2( void );

extern const CFStringRef kDAPreferenceMountDeferExternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountDeferInternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountDeferRemovableKey; /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustExternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustInternalKey;  /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceMountTrustRemovableKey; /* ( CFBoolean ) */
extern const CFStringRef kDAPreferenceAutoMountDisableKey;    /* ( CFBoolean ) */

extern void DAPreferenceListRefresh( void );

enum
{
///w:23678897:start
    _kDAUnitStateHasAPFS = 0x00000010,
///w:23678897:stop
    kDAUnitStateCommandActive        = 0x00000001,
    kDAUnitStateHasQuiesced          = 0x00000002,
    kDAUnitStateHasQuiescedNoTimeout = 0x00000004,
    kDAUnitStateStagedUnreadable     = 0x00010000
};

typedef UInt32 DAUnitState;

extern Boolean DAUnitGetState( DADiskRef disk, DAUnitState state );
extern Boolean DAUnitGetStateRecursively( DADiskRef disk, DAUnitState state );
extern void    DAUnitSetState( DADiskRef disk, DAUnitState state, Boolean value );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DASUPPORT__ */
