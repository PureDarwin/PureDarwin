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

#include "DAInternal.h"

#include <paths.h>
#include <DiskArbitration/DiskArbitration.h>

__private_extern__ const char * _kDAAuthorizeRightAdopt   = "adopt";
__private_extern__ const char * _kDAAuthorizeRightEncode  = "encode";
__private_extern__ const char * _kDAAuthorizeRightMount   = "mount";
__private_extern__ const char * _kDAAuthorizeRightRename  = "rename";
__private_extern__ const char * _kDAAuthorizeRightUnmount = "unmount";

__private_extern__ const CFStringRef _kDACallbackAddressKey       = CFSTR( "DACallbackAddress"   );
__private_extern__ const CFStringRef _kDACallbackArgument0Key     = CFSTR( "DACallbackArgument0" );
__private_extern__ const CFStringRef _kDACallbackArgument1Key     = CFSTR( "DACallbackArgument1" );
__private_extern__ const CFStringRef _kDACallbackContextKey       = CFSTR( "DACallbackContext"   );
__private_extern__ const CFStringRef _kDACallbackDiskKey          = CFSTR( "DACallbackDisk"      );
__private_extern__ const CFStringRef _kDACallbackKindKey          = CFSTR( "DACallbackKind"      );
__private_extern__ const CFStringRef _kDACallbackMatchKey         = CFSTR( "DACallbackMatch"     );
__private_extern__ const CFStringRef _kDACallbackOrderKey         = CFSTR( "DACallbackOrder"     );
__private_extern__ const CFStringRef _kDACallbackSessionKey       = CFSTR( "DACallbackSession"   );
__private_extern__ const CFStringRef _kDACallbackTimeKey          = CFSTR( "DACallbackTime"      );
__private_extern__ const CFStringRef _kDACallbackWatchKey         = CFSTR( "DACallbackWatch"     );

__private_extern__ const CFStringRef _kDADiskIDKey                = CFSTR( "DADiskID"            );

__private_extern__ const CFStringRef _kDADissenterProcessIDKey    = CFSTR( "DAProcessID"         );
__private_extern__ const CFStringRef _kDADissenterStatusKey       = CFSTR( "DAStatus"            );
__private_extern__ const CFStringRef _kDADissenterStatusStringKey = CFSTR( "DAStatusString"      );

__private_extern__ const CFStringRef _kDARequestArgument1Key      = CFSTR( "DARequestArgument1"  );
__private_extern__ const CFStringRef _kDARequestArgument2Key      = CFSTR( "DARequestArgument2"  );
__private_extern__ const CFStringRef _kDARequestArgument3Key      = CFSTR( "DARequestArgument3"  );
__private_extern__ const CFStringRef _kDARequestCallbackKey       = CFSTR( "DARequestCallback"   );
__private_extern__ const CFStringRef _kDARequestDiskKey           = CFSTR( "DARequestDisk"       );
__private_extern__ const CFStringRef _kDARequestDissenterKey      = CFSTR( "DARequestDissenter"  );
__private_extern__ const CFStringRef _kDARequestKindKey           = CFSTR( "DARequestKind"       );
__private_extern__ const CFStringRef _kDARequestLinkKey           = CFSTR( "DARequestLink"       );
__private_extern__ const CFStringRef _kDARequestStateKey          = CFSTR( "DARequestState"      );
__private_extern__ const CFStringRef _kDARequestUserGIDKey        = CFSTR( "DARequestUserGID"    );
__private_extern__ const CFStringRef _kDARequestUserUIDKey        = CFSTR( "DARequestUserUID"    );

const CFStringRef kDADiskDescriptionVolumeKindKey      = CFSTR( "DAVolumeKind"      );
const CFStringRef kDADiskDescriptionVolumeMountableKey = CFSTR( "DAVolumeMountable" );
const CFStringRef kDADiskDescriptionVolumeNameKey      = CFSTR( "DAVolumeName"      );
const CFStringRef kDADiskDescriptionVolumeNetworkKey   = CFSTR( "DAVolumeNetwork"   );
const CFStringRef kDADiskDescriptionVolumePathKey      = CFSTR( "DAVolumePath"      );
const CFStringRef kDADiskDescriptionVolumeTypeKey      = CFSTR( "DAVolumeType"      );
const CFStringRef kDADiskDescriptionVolumeUUIDKey      = CFSTR( "DAVolumeUUID"      );

const CFStringRef kDADiskDescriptionMediaBlockSizeKey  = CFSTR( "DAMediaBlockSize"  );
const CFStringRef kDADiskDescriptionMediaBSDMajorKey   = CFSTR( "DAMediaBSDMajor"   );
const CFStringRef kDADiskDescriptionMediaBSDMinorKey   = CFSTR( "DAMediaBSDMinor"   );
const CFStringRef kDADiskDescriptionMediaBSDNameKey    = CFSTR( "DAMediaBSDName"    );
const CFStringRef kDADiskDescriptionMediaBSDUnitKey    = CFSTR( "DAMediaBSDUnit"    );
const CFStringRef kDADiskDescriptionMediaContentKey    = CFSTR( "DAMediaContent"    );
const CFStringRef kDADiskDescriptionMediaEjectableKey  = CFSTR( "DAMediaEjectable"  );
const CFStringRef kDADiskDescriptionMediaIconKey       = CFSTR( "DAMediaIcon"       );
const CFStringRef kDADiskDescriptionMediaKindKey       = CFSTR( "DAMediaKind"       );
const CFStringRef kDADiskDescriptionMediaLeafKey       = CFSTR( "DAMediaLeaf"       );
const CFStringRef kDADiskDescriptionMediaNameKey       = CFSTR( "DAMediaName"       );
const CFStringRef kDADiskDescriptionMediaPathKey       = CFSTR( "DAMediaPath"       );
const CFStringRef kDADiskDescriptionMediaRemovableKey  = CFSTR( "DAMediaRemovable"  );
const CFStringRef kDADiskDescriptionMediaSizeKey       = CFSTR( "DAMediaSize"       );
const CFStringRef kDADiskDescriptionMediaTypeKey       = CFSTR( "DAMediaType"       );
const CFStringRef kDADiskDescriptionMediaUUIDKey       = CFSTR( "DAMediaUUID"       );
const CFStringRef kDADiskDescriptionMediaWholeKey      = CFSTR( "DAMediaWhole"      );
const CFStringRef kDADiskDescriptionMediaWritableKey   = CFSTR( "DAMediaWritable"   );
const CFStringRef kDADiskDescriptionMediaEncryptedKey  = CFSTR( "DAMediaEncrypted" );
const CFStringRef kDADiskDescriptionMediaEncryptionDetailKey = CFSTR( "DAMediaEncryptionDetail" );

const CFStringRef kDADiskDescriptionDeviceGUIDKey      = CFSTR( "DADeviceGUID"      );
const CFStringRef kDADiskDescriptionDeviceInternalKey  = CFSTR( "DADeviceInternal"  );
const CFStringRef kDADiskDescriptionDeviceModelKey     = CFSTR( "DADeviceModel"     );
const CFStringRef kDADiskDescriptionDevicePathKey      = CFSTR( "DADevicePath"      );
const CFStringRef kDADiskDescriptionDeviceProtocolKey  = CFSTR( "DADeviceProtocol"  );
const CFStringRef kDADiskDescriptionDeviceRevisionKey  = CFSTR( "DADeviceRevision"  );
const CFStringRef kDADiskDescriptionDeviceUnitKey      = CFSTR( "DADeviceUnit"      );
const CFStringRef kDADiskDescriptionDeviceVendorKey    = CFSTR( "DADeviceVendor"    );
const CFStringRef kDADiskDescriptionDeviceTDMLockedKey = CFSTR( "DADeviceTDMLocked" );

const CFStringRef kDADiskDescriptionBusNameKey         = CFSTR( "DABusName"         );
const CFStringRef kDADiskDescriptionBusPathKey         = CFSTR( "DABusPath"         );

const CFStringRef kDADiskDescriptionAppearanceTimeKey  = CFSTR( "DAAppearanceTime"  );

const CFStringRef kDADiskDescriptionMediaMatchKey      = CFSTR( "DAMediaMatch"      );

static const char * __kDAKindNameList[] =
{
    "disk appeared",
    "disk claim",
    "disk claim release",
    "disk description changed",
    "disk disappeared",
    "disk eject",
    "disk eject approval",
    "disk mount",
    "disk mount approval",
    "disk peek",
    "disk probe",
    "disk refresh",
    "disk rename",
    "disk unmount",
    "disk unmount approval",
    "idle",
    "disk list complete"
};

extern CFIndex __CFBinaryPlistWriteToStream( CFPropertyListRef plist, CFTypeRef stream );

__private_extern__ int ___statfs( const char * path, struct statfs * buf, int flags )
{
    struct statfs * mountList;
    int             mountListCount;
    int             mountListIndex;
    int             status;

    status = -1;

    mountListCount = getfsstat( NULL, 0, MNT_NOWAIT );

    if ( mountListCount > 0 )
    {
        mountList = malloc( mountListCount * sizeof( struct statfs ) );

        if ( mountList )
        {
            mountListCount = getfsstat( mountList, mountListCount * sizeof( struct statfs ), flags );

            if ( mountListCount > 0 )
            {
                for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
                {
                    if ( strcmp( mountList[mountListIndex].f_mntonname, path ) == 0 )
                    {
                        status = 0;

                        *buf = mountList[mountListIndex];

                        if ( mountList[mountListIndex].f_owner == geteuid( ) )
                        {
                            break;
                        }
                    }
                }
            }

            free( mountList );
        }
    }

    return status;
}

__private_extern__ Boolean ___CFArrayContainsValue( CFArrayRef array, const void * value )
{
    return CFArrayContainsValue( array, CFRangeMake( 0, CFArrayGetCount( array ) ), value );
}

__private_extern__ void ___CFArrayRemoveValue( CFMutableArrayRef array, const void * value )
{
    CFIndex index;

    index = CFArrayGetFirstIndexOfValue( array, CFRangeMake( 0, CFArrayGetCount( array ) ), value );

    if ( index != kCFNotFound )
    {
        CFArrayRemoveValueAtIndex( array, index );
    }
}

__private_extern__ vm_address_t ___CFDataCopyBytes( CFDataRef data, mach_msg_type_number_t * length )
{
    vm_address_t bytes = 0;

    *length = CFDataGetLength( data );

    vm_allocate( mach_task_self( ), &bytes, *length, TRUE );

    if ( bytes )
    {
        bcopy( CFDataGetBytePtr( data ), ( void * ) bytes, *length );
    }

    return bytes;
}

__private_extern__ SInt64 ___CFDictionaryGetIntegerValue( CFDictionaryRef dictionary, const void * key )
{
    CFNumberRef number;
    SInt64      value  = 0;

    number = CFDictionaryGetValue( dictionary, key );

    if ( number )
    {
        CFNumberGetValue( number, kCFNumberSInt64Type, &value );
    }

    return value;
}

__private_extern__ void ___CFDictionarySetIntegerValue( CFMutableDictionaryRef dictionary, const void * key, SInt64 value )
{
    CFNumberRef number;

    number = CFNumberCreate( CFGetAllocator( dictionary ), kCFNumberSInt64Type, &value );

    if ( number )
    {
        CFDictionarySetValue( dictionary, key, number );

        CFRelease( number );
    }
}

__private_extern__ CFNumberRef ___CFNumberCreateWithIntegerValue( CFAllocatorRef allocator, SInt64 value )
{
    CFNumberRef number;

    number = CFNumberCreate( allocator, kCFNumberSInt64Type, &value );

    return number;
}

__private_extern__ SInt64 ___CFNumberGetIntegerValue( CFNumberRef number )
{
    SInt64 value = 0;

    CFNumberGetValue( number, kCFNumberSInt64Type, &value );

    return value;
}

__private_extern__ char * ___CFStringCopyCString( CFStringRef string )
{
    /*
     * Creates a C string buffer from a CFString object.  The string encoding is presumed to be
     * UTF-8.  The result is a reference to a C string buffer or NULL if there was a problem in
     * creating the buffer.  The caller is responsible for releasing the buffer with free().
     */

    char * buffer = NULL;

    if ( string )
    {
        CFIndex length;
        CFRange range;

        range = CFRangeMake( 0, CFStringGetLength( string ) );

        if ( CFStringGetBytes( string, range, kCFStringEncodingUTF8, 0, FALSE, NULL, 0, &length ) )
        {
            buffer = malloc( length + 1 );

            if ( buffer )
            {
                CFStringGetBytes( string, range, kCFStringEncodingUTF8, 0, FALSE, ( void * ) buffer, length, NULL );

                buffer[length] = 0;
            }
        }
    }

    return buffer;
}

__private_extern__ char * ___CFURLCopyFileSystemRepresentation( CFURLRef url )
{
    /*
     * Creates a buffer with the file system's native string representation of URL's path.
     * The result is a reference to a C buffer or NULL if there was a problem in creating
     * the buffer.  The caller is responsible for releasing the buffer with free().
     */

    char * path = NULL;

    if ( url )
    {
        CFStringRef string;

        string = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );

        if ( string )
        {
            path = ___CFStringCopyCString( string );

            CFRelease( string );
        }
    }

    return path;
}

__private_extern__ const char * _DACallbackKindGetName( _DACallbackKind kind )
{
    const char * unknownKind = "Unknown Kind";
    if ( _kDADiskLastKind >= kind )
    {
    	return __kDAKindNameList[kind];
    }
    else
    {
        return unknownKind;
    }
}

__private_extern__ const char * _DARequestKindGetName( _DARequestKind kind )
{
    const char * unknownKind = "Unknown Kind";
    if ( _kDADiskLastKind >= kind )
    {
        return __kDAKindNameList[kind];
    }
    else
    {
        return unknownKind;
    }

}

__private_extern__ CFDataRef _DASerialize( CFAllocatorRef allocator, CFTypeRef object )
{
    CFDataRef data;

    data = CFDataCreateMutable( allocator, 0 );

    if ( data )
    {
        if ( __CFBinaryPlistWriteToStream( object, data ) == 0 )
        {
            CFRelease( data );

            data = NULL;
        }
    }

    return data;
}

__private_extern__ CFDataRef _DASerializeDiskDescription( CFAllocatorRef allocator, CFDictionaryRef description )
{
    CFDataRef data = NULL;

    if ( description )
    {
        CFMutableDictionaryRef copy;

        copy = CFDictionaryCreateMutableCopy( allocator, 0, description );

        if ( copy )
        {
            CFTypeRef object;

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaUUIDKey );

            if ( object )
            {
                object = CFUUIDCreateString( allocator, object );

                if ( object )
                {
                    CFDictionarySetValue( copy, kDADiskDescriptionMediaUUIDKey, object );

                    CFRelease( object );
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );

            if ( object )
            {
                object = CFURLCopyFileSystemPath( object, kCFURLPOSIXPathStyle );

                if ( object )
                {
                    CFDictionarySetValue( copy, kDADiskDescriptionVolumePathKey, object );

                    CFRelease( object );
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionVolumeUUIDKey );

            if ( object )
            {
                object = CFUUIDCreateString( allocator, object );

                if ( object )
                {
                    CFDictionarySetValue( copy, kDADiskDescriptionVolumeUUIDKey, object );

                    CFRelease( object );
                }
            }

            data = _DASerialize( allocator, copy );

            CFRelease( copy );
        }
    }

    return data;
}

__private_extern__ CFTypeRef _DAUnserialize( CFAllocatorRef allocator, CFDataRef data )
{
    return CFPropertyListCreateWithData( allocator, data, kCFPropertyListImmutable, NULL, NULL );
}

__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescription( CFAllocatorRef allocator, CFDataRef data )
{
    CFMutableDictionaryRef description;

    description = ( void * ) CFPropertyListCreateWithData( allocator, data, kCFPropertyListMutableContainers, NULL, NULL );

    if ( description )
    {
        CFTypeRef object;

        object = CFDictionaryGetValue( description, kDADiskDescriptionMediaUUIDKey );

        if ( object )
        {
            object = CFUUIDCreateFromString( allocator, object );

            if ( object )
            {
                CFDictionarySetValue( description, kDADiskDescriptionMediaUUIDKey, object );

                CFRelease( object );
            }
        }

        object = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );

        if ( object )
        {
            object = CFURLCreateWithFileSystemPath( allocator, object, kCFURLPOSIXPathStyle, TRUE );

            if ( object )
            {
                CFDictionarySetValue( description, kDADiskDescriptionVolumePathKey, object );

                CFRelease( object );
            }
        }

        object = CFDictionaryGetValue( description, kDADiskDescriptionVolumeUUIDKey );

        if ( object )
        {
            object = CFUUIDCreateFromString( allocator, object );

            if ( object )
            {
                CFDictionarySetValue( description, kDADiskDescriptionVolumeUUIDKey, object );

                CFRelease( object );
            }
        }
    }

    return description;
}

__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescriptionWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length )
{
    CFMutableDictionaryRef description = NULL;

    if ( bytes )
    {
        CFDataRef data;

        data = CFDataCreateWithBytesNoCopy( allocator, ( void * ) bytes, length, kCFAllocatorNull );

        if ( data )
        {
            description = _DAUnserializeDiskDescription( allocator, data );

            CFRelease( data );
        }
    }

    return description;
}

__private_extern__ CFTypeRef _DAUnserializeWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length )
{
    CFTypeRef object = NULL;

    if ( bytes )
    {
        CFDataRef data;

        data = CFDataCreateWithBytesNoCopy( allocator, ( void * ) bytes, length, kCFAllocatorNull );

        if ( data )
        {
            object = _DAUnserialize( allocator, data );

            CFRelease( data );
        }
    }

    return object;
}

__private_extern__ char * _DAVolumeCopyID( const struct statfs * fs )
{
    char * id;

    if ( strncmp( fs->f_mntfromname, _PATH_DEV, strlen( _PATH_DEV ) ) )
    {
        asprintf( &id, "%s?owner=%u", fs->f_mntonname, fs->f_owner );
    }
    else
    {
        asprintf( &id, "%s", fs->f_mntfromname );
    }

    return id;
}

__private_extern__ char * _DAVolumeGetID( const struct statfs * fs )
{
    static char id[ sizeof( fs->f_mntonname ) + strlen( "?owner=" ) + strlen( "4294967295" ) ];

    if ( strncmp( fs->f_mntfromname, _PATH_DEV, strlen( _PATH_DEV ) ) )
    {
        snprintf( id, sizeof( id ), "%s?owner=%u", fs->f_mntonname, fs->f_owner );
    }
    else
    {
        snprintf( id, sizeof( id ), "%s", fs->f_mntfromname );
    }

    return id;
}
