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

#ifndef __DISKARBITRATIOND_DAINTERNAL__
#define __DISKARBITRATIOND_DAINTERNAL__

#include <mach/mach.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ___GID_WHEEL 0
#define ___UID_ROOT  0

#define ___GID_UNKNOWN 99
#define ___UID_UNKNOWN 99

#define _kDADaemonName "com.apple.DiskArbitration.diskarbitrationd"

enum
{
    _kDAAuthorizeOptionDefault                   = 0x00000000,
    _kDAAuthorizeOptionAuthenticateAdministrator = 0x00000001,
    _kDAAuthorizeOptionIsOwner                   = 0x00080000
};

typedef UInt32 _DAAuthorizeOptions;

enum
{
    _kDADiskAppearedCallback,
    _kDADiskClaimCallback,
    _kDADiskClaimReleaseCallback,
    _kDADiskDescriptionChangedCallback,
    _kDADiskDisappearedCallback,
    _kDADiskEjectCallback,
    _kDADiskEjectApprovalCallback,
    _kDADiskMountCallback,
    _kDADiskMountApprovalCallback,
    _kDADiskPeekCallback,
    _kDADiskProbeCallback,
    _kDADiskRefreshCallback,
    _kDADiskRenameCallback,
    _kDADiskUnmountCallback,
    _kDADiskUnmountApprovalCallback,
    _kDAIdleCallback,
    _kDADiskListCompleteCallback,
    _kDADiskLastKind = _kDADiskListCompleteCallback
};

typedef UInt32 _DACallbackKind;

enum
{
    _kDADiskClaim   = _kDADiskClaimCallback,
    _kDADiskEject   = _kDADiskEjectCallback,
    _kDADiskMount   = _kDADiskMountCallback,
    _kDADiskProbe   = _kDADiskProbeCallback,
    _kDADiskRefresh = _kDADiskRefreshCallback,
    _kDADiskRename  = _kDADiskRenameCallback,
    _kDADiskUnmount = _kDADiskUnmountCallback
};

typedef UInt32 _DARequestKind;

const char * _kDAAuthorizeRightAdopt;
const char * _kDAAuthorizeRightEncode;
const char * _kDAAuthorizeRightMount;
const char * _kDAAuthorizeRightRename;
const char * _kDAAuthorizeRightUnmount;

const CFStringRef _kDACallbackAddressKey;       /* ( CFNumber     ) */
const CFStringRef _kDACallbackArgument0Key;     /* ( CFType       ) */
const CFStringRef _kDACallbackArgument1Key;     /* ( CFType       ) */
const CFStringRef _kDACallbackContextKey;       /* ( CFNumber     ) */
const CFStringRef _kDACallbackDiskKey;          /* ( DADisk       ) */
const CFStringRef _kDACallbackKindKey;          /* ( CFNumber     ) */
const CFStringRef _kDACallbackMatchKey;         /* ( CFDictionary ) */
const CFStringRef _kDACallbackOrderKey;         /* ( CFNumber     ) */
const CFStringRef _kDACallbackSessionKey;       /* ( DASession    ) */
const CFStringRef _kDACallbackTimeKey;          /* ( CFDate       ) */
const CFStringRef _kDACallbackWatchKey;         /* ( CFArray      ) */

const CFStringRef _kDADiskIDKey;                /* ( CFData       ) */

const CFStringRef _kDADissenterProcessIDKey;    /* ( CFNumber     ) */
const CFStringRef _kDADissenterStatusKey;       /* ( CFNumber     ) */
const CFStringRef _kDADissenterStatusStringKey; /* ( CFString     ) */

const CFStringRef _kDARequestArgument1Key;      /* ( CFType       ) */
const CFStringRef _kDARequestArgument2Key;      /* ( CFType       ) */
const CFStringRef _kDARequestArgument3Key;      /* ( CFType       ) */
const CFStringRef _kDARequestCallbackKey;       /* ( DACallback   ) */
const CFStringRef _kDARequestDiskKey;           /* ( DADisk       ) */
const CFStringRef _kDARequestDissenterKey;      /* ( DADissenter  ) */
const CFStringRef _kDARequestKindKey;           /* ( CFNumber     ) */
const CFStringRef _kDARequestLinkKey;           /* ( CFArray      ) */
const CFStringRef _kDARequestStateKey;          /* ( CFNumber     ) */
const CFStringRef _kDARequestUserGIDKey;        /* ( CFNumber     ) */
const CFStringRef _kDARequestUserUIDKey;        /* ( CFNumber     ) */

__private_extern__ int          ___statfs( const char * path, struct statfs * buf, int flags );
__private_extern__ Boolean      ___CFArrayContainsValue( CFArrayRef array, const void * value );
__private_extern__ void         ___CFArrayRemoveValue( CFMutableArrayRef array, const void * value );
__private_extern__ vm_address_t ___CFDataCopyBytes( CFDataRef data, mach_msg_type_number_t * length );
__private_extern__ SInt64       ___CFDictionaryGetIntegerValue( CFDictionaryRef dictionary, const void * key );
__private_extern__ void         ___CFDictionarySetIntegerValue( CFMutableDictionaryRef dictionary, const void * key, SInt64 value );
__private_extern__ CFNumberRef  ___CFNumberCreateWithIntegerValue( CFAllocatorRef allocator, SInt64 value );
__private_extern__ SInt64       ___CFNumberGetIntegerValue( CFNumberRef number );
__private_extern__ char *       ___CFStringCopyCString( CFStringRef string );
__private_extern__ char *       ___CFURLCopyFileSystemRepresentation( CFURLRef url );

__private_extern__ const char * _DACallbackKindGetName( _DACallbackKind kind );
__private_extern__ const char * _DARequestKindGetName( _DARequestKind kind );

__private_extern__ CFDataRef              _DASerialize( CFAllocatorRef allocator, CFTypeRef object );
__private_extern__ CFDataRef              _DASerializeDiskDescription( CFAllocatorRef allocator, CFDictionaryRef description );
__private_extern__ CFTypeRef              _DAUnserialize( CFAllocatorRef allocator, CFDataRef data );
__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescription( CFAllocatorRef allocator, CFDataRef data );
__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescriptionWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length );
__private_extern__ CFTypeRef              _DAUnserializeWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length );

__private_extern__ char * _DAVolumeCopyID( const struct statfs * fs );
__private_extern__ char * _DAVolumeGetID( const struct statfs * fs );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAINTERNAL__ */
