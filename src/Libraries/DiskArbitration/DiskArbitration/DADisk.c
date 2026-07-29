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

#include "DADisk.h"

#include "DAInternal.h"
#include "DAServer.h"
#include "DASession.h"

#include <paths.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

struct __DADisk
{
    CFRuntimeBase   _base;
    CFDictionaryRef _description;
    char *          _device;
    char *          _id;
    DASessionRef    _session;
};

typedef struct __DADisk __DADisk;

static CFStringRef __DADiskCopyDescription( CFTypeRef object );
static CFStringRef __DADiskCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options );
static void        __DADiskDeallocate( CFTypeRef object );
static Boolean     __DADiskEqual( CFTypeRef object1, CFTypeRef object2 );
static CFHashCode  __DADiskHash( CFTypeRef object );

static const CFRuntimeClass __DADiskClass =
{
    0,
    "DADisk",
    NULL,
    NULL,
    __DADiskDeallocate,
    __DADiskEqual,
    __DADiskHash,
    __DADiskCopyFormattingDescription,
    __DADiskCopyDescription
};

static CFTypeID __kDADiskTypeID = _kCFRuntimeNotATypeID;

__private_extern__ void _DAInitialize( void );

__private_extern__ mach_port_t _DASessionGetID( DASessionRef session );

extern CFHashCode CFHashBytes( UInt8 * bytes, CFIndex length );

static CFStringRef __DADiskCopyDescription( CFTypeRef object )
{
    DADiskRef disk = ( DADiskRef ) object;

    return CFStringCreateWithFormat( CFGetAllocator( object ), NULL, CFSTR( "<DADisk %p [%p]>{id = %s}" ), object, CFGetAllocator( object ), disk->_id );
}

static CFStringRef __DADiskCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options )
{
    DADiskRef disk = ( DADiskRef ) object;

    return CFStringCreateWithFormat( CFGetAllocator( object ), NULL, CFSTR( "%s" ), disk->_id );
}

static DADiskRef __DADiskCreate( CFAllocatorRef allocator, DASessionRef session, const char * id )
{
    __DADisk * disk;

    disk = ( void * ) _CFRuntimeCreateInstance( allocator, __kDADiskTypeID, sizeof( __DADisk ) - sizeof( CFRuntimeBase ), NULL );

    if ( disk )
    {
        CFRetain( session );

        disk->_description   = NULL;
        disk->_device        = NULL;
        disk->_id            = strdup( id );
        disk->_session       = session;
    }

    return disk;
}

static void __DADiskDeallocate( CFTypeRef object )
{
    DADiskRef disk = ( DADiskRef ) object;

    if ( disk->_description   )  CFRelease( disk->_description );
    if ( disk->_device        )  free( disk->_device );
    if ( disk->_id            )  free( disk->_id );
    if ( disk->_session       )  CFRelease( disk->_session );
}

static Boolean __DADiskEqual( CFTypeRef object1, CFTypeRef object2 )
{
    DADiskRef disk1 = ( DADiskRef ) object1;
    DADiskRef disk2 = ( DADiskRef ) object2;

    return ( strcmp( disk1->_id, disk2->_id ) == 0 );
}

static CFHashCode __DADiskHash( CFTypeRef object )
{
    DADiskRef disk = ( DADiskRef ) object;

    return CFHashBytes( ( void * ) disk->_id, MIN( strlen( disk->_id ), 16 ) );
}

__private_extern__ DADiskRef _DADiskCreate( CFAllocatorRef allocator, DASessionRef session, const char * id )
{
    DADiskRef disk = NULL;

    if ( session )
    {
        disk = __DADiskCreate( allocator, session, id );

        if ( disk )
        {
            if ( strncmp( id, _PATH_DEV, strlen( _PATH_DEV ) ) == 0 )
            {
                disk->_device = strdup( id + strlen( _PATH_DEV ) );
            }
        }
    }

    return disk;
}

DADiskRef _DADiskCreateFromSerialization( CFAllocatorRef allocator, DASessionRef session, CFDataRef serialization )
{
    DADiskRef disk = NULL;

    if ( serialization )
    {
        CFMutableDictionaryRef description;

        description = _DAUnserializeDiskDescription( CFGetAllocator( session ), serialization );

        if ( description )
        {
            CFDataRef data;

            data = CFDictionaryGetValue( description, _kDADiskIDKey );

            if ( data )
            {
                const char * id;

                id = ( void * ) CFDataGetBytePtr( data );

                if ( id )
                {
                    disk = _DADiskCreate( CFGetAllocator( session ), session, id );

                    if ( disk )
                    {
                        CFDictionaryRemoveValue( description, _kDADiskIDKey );

                        disk->_description = CFRetain( description );
                    }
                }
            }

            CFRelease( description );
        }
    }

    return disk;
}

__private_extern__ char * _DADiskGetID( DADiskRef disk )
{
    return disk->_id;
}

DASessionRef _DADiskGetSession( DADiskRef disk )
{
    return disk->_session;
}

__private_extern__ mach_port_t _DADiskGetSessionID( DADiskRef disk )
{
    return _DASessionGetID( disk->_session );
}

__private_extern__ void _DADiskInitialize( void )
{
    __kDADiskTypeID = _CFRuntimeRegisterClass( &__DADiskClass );
}

__private_extern__ void _DADiskSetDescription( DADiskRef disk, CFDictionaryRef description )
{
    if ( disk->_description )
    {
        CFRelease( disk->_description );

        disk->_description = NULL;
    }

    if ( description )
    {
        CFRetain( description );

        disk->_description = description;
    }    
}

CFDictionaryRef DADiskCopyDescription( DADiskRef disk )
{
    CFDictionaryRef description;

    description = NULL;

    if ( disk )
    {
        if ( disk->_description )
        {
            CFRetain( disk->_description );

            description = disk->_description;
        }
        else
        {
            vm_address_t           _description;
            mach_msg_type_number_t _descriptionSize;
            kern_return_t          status;

            status = _DAServerDiskCopyDescription( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &_description, &_descriptionSize );

            if ( status == KERN_SUCCESS )
            {
                description = _DAUnserializeDiskDescriptionWithBytes( CFGetAllocator( disk ), _description, _descriptionSize );

                CFDictionaryRemoveValue( ( void * ) description, _kDADiskIDKey );

                vm_deallocate( mach_task_self( ), _description, _descriptionSize );
            }
        }
    }

    return description;
}

io_service_t DADiskCopyIOMedia( DADiskRef disk )
{
    io_service_t media;

    media = IO_OBJECT_NULL;

    if ( disk )
    {
        if ( disk->_device )
        {
            media = IOServiceGetMatchingService( kIOMasterPortDefault, IOBSDNameMatching( kIOMasterPortDefault, 0, disk->_device ) );
        }
    }

    return media;
}

DADiskRef DADiskCopyWholeDisk( DADiskRef disk )
{
    if ( disk )
    {
        io_service_t media;

        media = DADiskCopyIOMedia( disk );

        while ( media )
        {
            io_service_t parent = IO_OBJECT_NULL;

            if ( IOObjectConformsTo( media, kIOMediaClass ) )
            {
                CFBooleanRef whole;

                whole = IORegistryEntryCreateCFProperty( media, CFSTR( kIOMediaWholeKey ), CFGetAllocator( disk ), 0 );

                if ( whole )
                {
                    if ( whole == kCFBooleanTrue )
                    {
                        disk = DADiskCreateFromIOMedia( CFGetAllocator( disk ), disk->_session, media );

                        IOObjectRelease( media );

                        CFRelease( whole );

                        return disk;
                    }

                    CFRelease( whole );
                }
            }

            IORegistryEntryGetParentEntry( media, kIOServicePlane, &parent );

            IOObjectRelease( media );

            media = parent;
        }
    }

    return NULL;
}

DADiskRef DADiskCreateFromBSDName( CFAllocatorRef allocator, DASessionRef session, const char * name )
{
    DADiskRef disk;

    disk = NULL;

    if ( name )
    {
        char id[MAXPATHLEN];

        if ( strncmp( name, _PATH_DEV, strlen( _PATH_DEV ) ) )
        {
            strlcpy( id, _PATH_DEV, sizeof( id ) );
            strlcat( id, name,      sizeof( id ) );
        }
        else
        {
            strlcpy( id, name, sizeof( id ) );
        }

        disk = _DADiskCreate( allocator, session, id );
    }

    return disk;
}

DADiskRef DADiskCreateFromIOMedia( CFAllocatorRef allocator, DASessionRef session, io_service_t media )
{
    DADiskRef disk;

    disk = NULL;

    if ( media )
    {
        CFStringRef string;

        string = IORegistryEntryCreateCFProperty( media, CFSTR( kIOBSDNameKey ), allocator, 0 );

        if ( string )
        {
            char name[MAXPATHLEN];

            CFStringGetCString( string, name, sizeof( name ), kCFStringEncodingUTF8 );

            disk = DADiskCreateFromBSDName( allocator, session, name );

            CFRelease( string );
        }
    }

    return disk;
}

DADiskRef DADiskCreateFromVolumePath( CFAllocatorRef allocator, DASessionRef session, CFURLRef path )
{
    DADiskRef disk = NULL;

    if ( path )
    {
        char * _path;

        _path = ___CFURLCopyFileSystemRepresentation( path );

        if ( _path )
        {
            struct statfs fs;
            int           status;

            status = ___statfs( _path, &fs, MNT_NOWAIT );

            if ( status )
            {
                char name[MAXPATHLEN];

                if ( realpath( _path, name ) )
                {
                    status = ___statfs( name, &fs, MNT_NOWAIT );
                }
            }

            if ( status == 0 )
            {
                char * id;

                id = _DAVolumeCopyID( &fs );

                if ( id )
                {
                    disk = _DADiskCreate( allocator, session, id );

                    free( id );
                }
            }

            free( _path );
        }
    }

    return disk;
}

const char * DADiskGetBSDName( DADiskRef disk )
{
    char * device;

    device = NULL;

    if ( disk )
    {
        device = disk->_device;
    }

    return device;
}

CFTypeID DADiskGetTypeID( void )
{
    _DAInitialize( );

    return __kDADiskTypeID;
}
