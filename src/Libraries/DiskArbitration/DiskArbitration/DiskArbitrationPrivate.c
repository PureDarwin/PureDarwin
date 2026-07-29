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

#include "DiskArbitrationPrivate.h"

#include "DAInternal.h"
#include "DAServer.h"

#include <sys/stat.h>

#ifndef __LP64__

#include <paths.h>
#include <unistd.h>
#include <servers/bootstrap.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOBDMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>

///w:start
static kern_return_t          __gDiskArbStatus                      = KERN_SUCCESS;
static Boolean                __gDiskArbStatusLock                  = FALSE;
///w:stop

static int                    __gDiskArbAck                         = 0;
static CFMutableDictionaryRef __gDiskArbCallbackList                = NULL;
static CFMutableSetRef        __gDiskArbEjectList                   = NULL;
static int                    __gDiskArbHandlesUnrecognized         = 0;
static int                    __gDiskArbHandlesUnrecognizedPriority = 0;
static int                    __gDiskArbHandlesUnrecognizedTypes    = 0;
static int                    __gDiskArbNotificationComplete        = 0;
static CFMutableArrayRef      __gDiskArbRegisterList                = NULL;
static CFMutableSetRef        __gDiskArbReservationList             = NULL;
static DASessionRef           __gDiskArbSession                     = NULL;
static CFMutableSetRef        __gDiskArbUnmountList                 = NULL;

#endif /* !__LP64__ */

__private_extern__ DAReturn _DAAuthorize( DASessionRef session, _DAAuthorizeOptions options, DADiskRef disk, const char * right );

__private_extern__ char *      _DADiskGetID( DADiskRef disk );
__private_extern__ mach_port_t _DADiskGetSessionID( DADiskRef disk );

__private_extern__ mach_port_t _DASessionGetID( DASessionRef session );

__private_extern__ void _DARegisterCallback( DASessionRef    session,
                                             void *          callback,
                                             void *          context,
                                             _DACallbackKind kind,
                                             CFIndex         order,
                                             CFDictionaryRef match,
                                             CFArrayRef      watch );

#ifndef __LP64__

__private_extern__ void             _DASessionCallback( CFMachPortRef port, void * message, CFIndex messageSize, void * info );
__private_extern__ AuthorizationRef _DASessionGetAuthorization( DASessionRef session );
__private_extern__ mach_port_t      _DASessionGetClientPort( DASessionRef session );
__private_extern__ void             _DASessionScheduleWithRunLoop( DASessionRef session );

static unsigned __DiskArbCopyDiskDescriptionAppearanceTime( DADiskRef disk )
{
    double time = 0;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFNumberRef number;

            number = CFDictionaryGetValue( description, kDADiskDescriptionAppearanceTimeKey );

            if ( number )
            {
                CFNumberGetValue( number, kCFNumberDoubleType, &time );
            }

            CFRelease( description );
        }
    }

    return time;
}

static char * __DiskArbCopyDiskDescriptionDeviceTreePath( DADiskRef disk )
{
    char * path = NULL;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFStringRef string;

            string = CFDictionaryGetValue( description, kDADiskDescriptionMediaPathKey );

            if ( string )
            {
                char * buffer;

                buffer = ___CFStringCopyCString( string );

                if ( buffer )
                {
                    if ( strncmp( buffer, kIODeviceTreePlane ":", strlen( kIODeviceTreePlane ":" ) ) == 0 )
                    {
                        path = strdup( buffer + strlen( kIODeviceTreePlane ":" ) );
                    }

                    free( buffer );
                }
            }

            CFRelease( description );
        }
    }
    
    return path ? path : strdup( "" );
}

static unsigned __DiskArbCopyDiskDescriptionFlags( DADiskRef disk )
{
    unsigned flags = 0;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFTypeRef object;

            object = CFDictionaryGetValue( description, kDADiskDescriptionDeviceInternalKey );

            if ( object )
            {
                if ( object == kCFBooleanTrue )
                {
                    flags |= kDiskArbDiskAppearedInternal;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaEjectableKey );

            if ( object )
            {
                if ( object == kCFBooleanTrue )
                {
                    flags |= kDiskArbDiskAppearedEjectableMask;

                    object = CFDictionaryGetValue( description, kDADiskDescriptionMediaKindKey );

                    if ( object )
                    {
                        DADiskRef whole;

                        whole = DADiskCopyWholeDisk( disk );

                        if ( whole )
                        {
                            io_service_t media;

                            media = DADiskCopyIOMedia( whole );

                            if ( media )
                            {
                                if ( IOObjectConformsTo( media, kIOBDMediaClass ) )
                                {
                                    flags |= kDiskArbDiskAppearedBDROMMask;
                                }

                                if ( IOObjectConformsTo( media, kIOCDMediaClass ) )
                                {
                                    flags |= kDiskArbDiskAppearedCDROMMask;
                                }

                                if ( IOObjectConformsTo( media, kIODVDMediaClass ) )
                                {
                                    flags |= kDiskArbDiskAppearedDVDROMMask;
                                }

                                IOObjectRelease( media );
                            }

                            CFRelease( whole );
                        }
                    }
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaLeafKey );

            if ( object )
            {
                if ( object == kCFBooleanFalse )
                {
                    flags |= kDiskArbDiskAppearedNonLeafDiskMask;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaSizeKey );

            if ( object )
            {
                if ( ___CFNumberGetIntegerValue( object ) == 0 )
                {
                    flags |= kDiskArbDiskAppearedNoSizeMask;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaWholeKey );

            if ( object )
            {
                if ( object == kCFBooleanTrue )
                {
                    flags |= kDiskArbDiskAppearedWholeDiskMask;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionMediaWritableKey );

            if ( object )
            {
                if ( object == kCFBooleanFalse )
                {
                    flags |= kDiskArbDiskAppearedLockedMask;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionVolumeMountableKey );

            if ( object )
            {
                if ( object == kCFBooleanFalse )
                {
                    flags |= kDiskArbDiskAppearedUnrecognizableFormat;
                }
            }

            object = CFDictionaryGetValue( description, kDADiskDescriptionVolumeNetworkKey );

            if ( object )
            {
                if ( object == kCFBooleanTrue )
                {
                    flags |= kDiskArbDiskAppearedNetworkDiskMask;
                }
            }

            CFRelease( description );
        }
    }

    return flags;
}

static char * __DiskArbCopyDiskDescriptionMediaContent( DADiskRef disk )
{
    char * content = NULL;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFStringRef string;

            string = CFDictionaryGetValue( description, kDADiskDescriptionMediaContentKey );

            if ( string )
            {
                content = ___CFStringCopyCString( string );
            }

            CFRelease( description );
        }
    }

    return content ? content : strdup( "" );
}

static unsigned __DiskArbCopyDiskDescriptionSequenceNumber( DADiskRef disk )
{
    unsigned sequence = -1;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFURLRef url;

            url = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );

            if ( url )
            {
                CFNumberRef number;

                number = CFDictionaryGetValue( description, kDADiskDescriptionMediaBSDMinorKey );

                if ( number )
                {
                    CFNumberGetValue( number, kCFNumberIntType, &sequence );
                }
            }

            CFRelease( description );
        }
    }

    return sequence;
}

static char * __DiskArbCopyDiskDescriptionVolumeKind( DADiskRef disk )
{
    char * kind = NULL;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFStringRef string;

            string = CFDictionaryGetValue( description, kDADiskDescriptionVolumeKindKey );

            if ( string )
            {
                kind = ___CFStringCopyCString( string );
            }

            CFRelease( description );
        }
    }

    return kind ? kind : strdup( "" );
}

static char * __DiskArbCopyDiskDescriptionVolumeName( DADiskRef disk )
{
    char * name = NULL;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFStringRef string;

            string = CFDictionaryGetValue( description, kDADiskDescriptionVolumeNameKey );

            if ( string )
            {
                name = ___CFStringCopyCString( string );
            }

            CFRelease( description );
        }
    }

    return name ? name : strdup( "" );
}

static char * __DiskArbCopyDiskDescriptionVolumePath( DADiskRef disk )
{
    char * path = NULL;

    if ( disk )
    {
        CFDictionaryRef description;

        description = DADiskCopyDescription( disk );

        if ( description )
        {
            CFURLRef url;

            url = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );

            if ( url )
            {
                path = ___CFURLCopyFileSystemRepresentation( url );
            }

            CFRelease( description );
        }
    }

    return path ? path : strdup( "" );
}

static CFArrayRef __DiskArbGetCallbackHandler( int type )
{
    CFArrayRef callbacks = NULL;

    if ( __gDiskArbCallbackList )
    {
        CFNumberRef key;

        key = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &type );

        if ( key )
        {
            callbacks = CFDictionaryGetValue( __gDiskArbCallbackList, key );

            CFRelease( key );
        }
    }

    return callbacks;
}

static char * __DiskArbGetDiskID( DADiskRef _disk )
{
    char * disk;

    disk = _DADiskGetID( _disk );

    if ( strncmp( disk, _PATH_DEV, strlen( _PATH_DEV ) ) == 0 )
    {
        disk += strlen( _PATH_DEV );
    }

    return disk;
}

static struct statfs * __DiskArbGetFileSystemStatus( char * disk )
{
    struct statfs * mountList;
    int             mountListCount;
    int             mountListIndex;

    mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

    for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
    {
        if ( strncmp( disk, "disk", strlen( "disk" ) ) )
        {
            if ( strcmp( mountList[mountListIndex].f_mntfromname, disk ) == 0 )
            {
                break;
            }

            if ( strcmp( mountList[mountListIndex].f_mntonname, disk ) == 0 )
            {
                break;
            }
        }
        else
        {
            if ( strncmp( mountList[mountListIndex].f_mntfromname, _PATH_DEV, strlen( _PATH_DEV ) ) == 0 )
            {
                if ( strcmp( mountList[mountListIndex].f_mntfromname + strlen( _PATH_DEV ), disk ) == 0 )
                {
                    break;
                }
            }
        }
    }

    return ( mountListIndex < mountListCount ) ? ( mountList + mountListIndex ) : ( NULL );
}

static void __DiskArbCallback_CallFailedNotification( char * disk, int type, int status )
{
    CFArrayRef callbacks;

    callbacks = __DiskArbGetCallbackHandler( kDA_CALL_FAILED );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_CallFailedNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( disk, type, status );
            }
        }
    }
}

static void __DiskArbCallback_Device_Reservation_Status( char * disk, int status, int pid )
{
    CFArrayRef callbacks;

    callbacks = __DiskArbGetCallbackHandler( kDA_DEVICE_RESERVATION_STATUS );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_Device_Reservation_Status_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( disk, status, pid );
            }
        }
    }
}

static void __DiskArbCallback_DiskChangedNotification( char * disk, char * mountpoint, char * name, int context, int success )
{
    CFArrayRef callbacks;

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_CHANGED );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_DiskChangedNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( disk, mountpoint, name, context, success );
            }
        }
    }
}

static void __DiskArbCallback_EjectPostNotification( char * disk, int status, pid_t dissenter )
{
    CFArrayRef callbacks;

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_EJECT_POST_NOTIFY );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_EjectPostNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( disk, status, dissenter );
            }
        }
    }

    __gDiskArbNotificationComplete |= kDiskArbCompletedPostEject;
}

static void __DiskArbCallback_EjectPostNotificationApplier( const void * value, void * context )
{
    DADiskRef disk = ( DADiskRef ) value;

    __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( disk ), context ? EBUSY : 0, context ? -1 : 0 );
}

static void __DiskArbCallback_UnmountPostNotification( char * disk, int status, pid_t dissenter )
{
    CFArrayRef callbacks;

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_UNMOUNT_POST_NOTIFY );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_UnmountPostNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( disk, status, dissenter );
            }
        }
    }

    __gDiskArbNotificationComplete |= kDiskArbCompletedPostUnmount;
}

static void __DiskArbCallback_UnmountPostNotificationApplier( const void * value, void * context )
{
    DADiskRef disk = ( DADiskRef ) value;

    __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), context ? EBUSY : 0, context ? -1 : 0 );
}

static void __DiskArbDiskAppearedCallback( DADiskRef disk, void * context )
{
    CFArrayRef      callbacks   = NULL;
    char *          content     = NULL;
    char *          filesystem  = NULL;
    unsigned        flags       = 0;
    char *          mountpoint  = NULL;
    char *          name        = NULL;
    char *          path        = NULL;
    unsigned        sequence    = -1;
    double          time        = 0;

    content    = __DiskArbCopyDiskDescriptionMediaContent( disk );
    filesystem = __DiskArbCopyDiskDescriptionVolumeKind( disk );
    flags      = __DiskArbCopyDiskDescriptionFlags( disk );
    mountpoint = __DiskArbCopyDiskDescriptionVolumePath( disk );
    name       = __DiskArbCopyDiskDescriptionVolumeName( disk );
    path       = __DiskArbCopyDiskDescriptionDeviceTreePath( disk );
    sequence   = __DiskArbCopyDiskDescriptionSequenceNumber( disk );
    time       = __DiskArbCopyDiskDescriptionAppearanceTime( disk );

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_APPEARED );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_DiskAppeared2_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( __DiskArbGetDiskID( disk ), flags, mountpoint, content, path, sequence );
            }
        }
    }

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_APPEARED1 );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_DiskAppeared_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( __DiskArbGetDiskID( disk ), flags, mountpoint, content );
            }
        }
    }

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_APPEARED_COMPLETE );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_DiskAppearedComplete_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                ( callback )( __DiskArbGetDiskID( disk ), flags, mountpoint, content, path, sequence, time, filesystem, name );
            }
        }
    }

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_APPEARED_WITH_MT );

    if ( callbacks )
    {
        if ( strcmp( mountpoint, "" ) )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( callbacks );

            for ( index = 0; index < count; index++ )
            {
                DiskArbCallback_DiskAppearedWithMountpoint_t callback;

                callback = CFArrayGetValueAtIndex( callbacks, index );

                if ( callback )
                {
                    ( callback )( __DiskArbGetDiskID( disk ), flags, mountpoint );
                }
            }
        }
    }

    __gDiskArbNotificationComplete |= kDiskArbCompletedDiskAppeared;

    if ( content    )  free( content    );
    if ( filesystem )  free( filesystem );
    if ( mountpoint )  free( mountpoint );
    if ( name       )  free( name       );
    if ( path       )  free( path       );
}

static void __DiskArbDiskClaimCallback( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    if ( dissenter == NULL )
    {
        CFSetSetValue( __gDiskArbReservationList, disk );
    }

    if ( context == 0 )
    {
        if ( dissenter )
        {
            __DiskArbCallback_Device_Reservation_Status( __DiskArbGetDiskID( disk ), kDiskArbDeviceReservationRefused, -1 );
        }
        else
        {
            __DiskArbCallback_Device_Reservation_Status( __DiskArbGetDiskID( disk ), kDiskArbDeviceReservationObtained, getpid( ) );
        }
    }
}

static DADissenterRef __DiskArbDiskClaimReleaseCallback( DADiskRef disk, void * context )
{
    DADissenterRef dissenter = NULL;

    if ( context == 0 )
    {
        CFArrayRef callbacks;

        callbacks = __DiskArbGetCallbackHandler( kDA_WILL_CLIENT_RELEASE_DEVICE );

        if ( callbacks )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( callbacks );

            if ( count )
            {
                for ( index = 0; index < count; index++ )
                {
                    DiskArbCallback_Will_Client_Release_t callback;

                    callback = CFArrayGetValueAtIndex( callbacks, index );

                    if ( callback )
                    {
                        __gDiskArbAck = 0;

                        ( callback )( __DiskArbGetDiskID( disk ), -1 );

                        if ( dissenter == NULL )
                        {
                            if ( __gDiskArbAck == 0 )
                            {
                                dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );
                            }
                        }
                    }
                }
            }
            else
            {
                dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );
            }
        }
        else
        {
            dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );
        }
    }

    if ( dissenter == NULL )
    {
        CFSetRemoveValue( __gDiskArbReservationList, disk );
    }

    return dissenter;
}

static void __DiskArbDiskDescriptionChangedCallback( DADiskRef disk, CFArrayRef keys, void * context )
{
    CFDictionaryRef description;

    description = DADiskCopyDescription( disk );

    if ( description )
    {
        if ( ___CFArrayContainsValue( keys, kDADiskDescriptionVolumeNameKey ) )
        {
            if ( CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey ) )
            {
                char * name;
                char * path;

                name = __DiskArbCopyDiskDescriptionVolumeName( disk );
                path = __DiskArbCopyDiskDescriptionVolumePath( disk );

                __DiskArbCallback_DiskChangedNotification( __DiskArbGetDiskID( disk ), path, name, 0, kDiskArbRenameSuccessful );

                if ( name )  free( name );
                if ( path )  free( path );
            }
        }
        else if ( ___CFArrayContainsValue( keys, kDADiskDescriptionVolumePathKey ) )
        {
            if ( CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey ) )
            {
                __DiskArbDiskAppearedCallback( disk, NULL );
            }
            else
            {
                CFSetRemoveValue( __gDiskArbUnmountList, disk );

                __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
            }
        }

        CFRelease( description );
    }
}

static void __DiskArbDiskDisappearedCallback( DADiskRef disk, void * context )
{
    CFDictionaryRef description;

    CFSetRemoveValue( __gDiskArbUnmountList, disk );

    CFSetRemoveValue( __gDiskArbEjectList, disk );

    description = DADiskCopyDescription( disk );

    if ( description )
    {
        if ( CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey ) )
        {
            __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
        }

        CFRelease( description );
    }

    __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
}

static void __DiskArbDiskEjectCallback( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    if ( dissenter )
    {
        io_service_t media;
        DAReturn     status;

        status = DADissenterGetStatus( dissenter );

        media = DADiskCopyIOMedia( disk );

        if ( media )
        {
            io_iterator_t services = IO_OBJECT_NULL;

            IORegistryEntryCreateIterator( media, kIOServicePlane, kIORegistryIterateRecursively, &services );

            if ( services )
            {
                io_service_t service;

                while ( ( service = IOIteratorNext( services ) ) )
                {
                    if ( IOObjectConformsTo( service, kIOMediaClass ) )
                    {
                        DADiskRef child;

                        child = DADiskCreateFromIOMedia( kCFAllocatorDefault, __gDiskArbSession, service );

                        if ( child )
                        {
                            CFSetRemoveValue( __gDiskArbEjectList, child );

                            __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( child ), status ? EBUSY : 0, status ? -1 : 0 );

                            CFRelease( child );
                        }
                    }

                    IOObjectRelease( service );
                }

                IOObjectRelease( services );
            }

            IOObjectRelease( media );
        }

        CFSetRemoveValue( __gDiskArbEjectList, disk );

        __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( disk ), status ? EBUSY : 0, status ? -1 : 0 );

        if ( ( ( ( int ) context ) & kDiskArbUnmountAndEjectFlag ) )
        {
            __DiskArbCallback_CallFailedNotification( __DiskArbGetDiskID( disk ), kDiskArbUnmountAndEjectRequestFailed, status ? EBUSY : 0 );
        }
        else
        {
            __DiskArbCallback_CallFailedNotification( __DiskArbGetDiskID( disk ), kDiskArbEjectRequestFailed, status ? EBUSY : 0 );
        }
    }
    else
    {
        io_service_t media;

        media = DADiskCopyIOMedia( disk );

        if ( media )
        {
            io_iterator_t services = IO_OBJECT_NULL;

            IORegistryEntryCreateIterator( media, kIOServicePlane, kIORegistryIterateRecursively, &services );

            if ( services )
            {
                io_service_t service;

                while ( ( service = IOIteratorNext( services ) ) )
                {
                    if ( IOObjectConformsTo( service, kIOMediaClass ) )
                    {
                        DADiskRef child;

                        child = DADiskCreateFromIOMedia( kCFAllocatorDefault, __gDiskArbSession, service );

                        if ( child )
                        {
                            CFSetRemoveValue( __gDiskArbEjectList, child );

                            __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( child ), 0, 0 );

                            CFRelease( child );
                        }
                    }

                    IOObjectRelease( service );
                }

                IOObjectRelease( services );
            }

            IOObjectRelease( media );
        }

        CFSetRemoveValue( __gDiskArbEjectList, disk );

        __DiskArbCallback_EjectPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
    }
}

static DADissenterRef __DiskArbDiskEjectApprovalCallback( DADiskRef disk, void * context )
{
    CFArrayRef     callbacks;
    DADissenterRef dissenter = NULL;

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_EJECT_PRE_NOTIFY );

    if ( callbacks )
    {
        io_service_t media;

        media = DADiskCopyIOMedia( disk );

        if ( media )
        {
            io_iterator_t services = IO_OBJECT_NULL;

            IORegistryEntryCreateIterator( media, kIOServicePlane, kIORegistryIterateRecursively, &services );

            if ( services )
            {
                io_service_t service;

                IOObjectRetain( media );

                for ( service = media; service; service = IOIteratorNext( services ) )
                {
                    if ( IOObjectConformsTo( service, kIOMediaClass ) )
                    {
                        DADiskRef child;

                        child = DADiskCreateFromIOMedia( kCFAllocatorDefault, __gDiskArbSession, service );

                        if ( child )
                        {
                            CFIndex count;
                            CFIndex index;

                            count = CFArrayGetCount( callbacks );

                            for ( index = 0; index < count; index++ )
                            {
                                DiskArbCallback_EjectPreNotification_t callback;

                                callback = CFArrayGetValueAtIndex( callbacks, index );

                                if ( callback )
                                {
                                    __gDiskArbAck = 0;

                                    ( callback )( __DiskArbGetDiskID( child ), 0 );

                                    if ( dissenter == NULL )
                                    {
                                        switch ( __gDiskArbAck )
                                        {
                                            case 0:
                                            {
                                                break;
                                            }
                                            default:
                                            {
                                                dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );

                                                break;
                                            }
                                        }
                                    }
                                }
                            }

                            CFSetSetValue( __gDiskArbEjectList, child );

                            CFRelease( child );
                        }
                    }

                    IOObjectRelease( service );
                }

                IOObjectRelease( services );
            }

            IOObjectRelease( media );
        }
    }

    return dissenter;
}

static void __DiskArbDiskMountCallback( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    if ( dissenter )
    {
///w:start
        DAReturn status;

        status = DADissenterGetStatus( dissenter );

        if ( __gDiskArbStatusLock )
        {
             if ( status )
             {
                 __gDiskArbStatus = status;

                 return;
             }
        }
///w:stop
        __DiskArbDiskAppearedCallback( disk, NULL );
    }
    else
    {
        if ( context )
        {
            __DiskArbDiskAppearedCallback( disk, NULL );
        }
    }
}

static DADissenterRef __DiskArbDiskMountApprovalCallback( DADiskRef disk, void * context )
{
    CFArrayRef     callbacks  = NULL;
    char *         content    = NULL;
    DADissenterRef dissenter  = NULL;
    char *         filesystem = NULL;
    unsigned       flags      = 0;
    char *         name       = NULL;
    char *         path       = NULL;
    int            removable  = FALSE;
    int            whole      = FALSE;
    int            writable   = FALSE;

    content    = __DiskArbCopyDiskDescriptionMediaContent( disk );
    filesystem = __DiskArbCopyDiskDescriptionVolumeKind( disk );
    flags      = __DiskArbCopyDiskDescriptionFlags( disk );
    name       = __DiskArbCopyDiskDescriptionVolumeName( disk );
    path       = __DiskArbCopyDiskDescriptionDeviceTreePath( disk );
    removable  = ( flags & kDiskArbDiskAppearedEjectableMask ) ? TRUE : FALSE;
    whole      = ( flags & kDiskArbDiskAppearedWholeDiskMask ) ? TRUE : FALSE;
    writable   = ( flags & kDiskArbDiskAppearedLockedMask ) ? FALSE : TRUE; 

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_APPROVAL_NOTIFY );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_DiskApprovalNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                __gDiskArbAck = 0;

                ( callback )( __DiskArbGetDiskID( disk ), name, content, path, flags, writable, removable, whole, filesystem );

                if ( dissenter == NULL )
                {
///w:start
                    if ( ( __gDiskArbAck & kDiskArbEjectDevice ) )
                    {
                        if ( removable )
                        {
                            DiskArbEjectRequest_async_auto( __DiskArbGetDiskID( disk ), 0 );
                        }

                        __gDiskArbAck &= ~kDiskArbEjectDevice;

                        if ( __gDiskArbAck == 0 )
                        {
                            __gDiskArbAck = kDiskArbDisallowMounting;
                        }
                    }
///w:stop

                    switch ( __gDiskArbAck )
                    {
                        case 0:
                        {
                            break;
                        }
///w:start
                        case kDiskArbMountReadOnly | kDiskArbRequireAuthentication:
                        {
                            dissenter = DADissenterCreate( kCFAllocatorDefault, 0xF8DAFF03, NULL );

                            break;
                        }
                        case kDiskArbMountReadOnly:
                        {
                            dissenter = DADissenterCreate( kCFAllocatorDefault, 0xF8DAFF02, NULL );

                            break;
                        }
                        case kDiskArbRequireAuthentication:
                        {
                            dissenter = DADissenterCreate( kCFAllocatorDefault, 0xF8DAFF01, NULL );

                            break;
                        }
///w:stop
                        default:
                        {
                            dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );

                            break;
                        }
                    }
                }
            }
        }
    }

    if ( content    )  free( content    );
    if ( filesystem )  free( filesystem );
    if ( name       )  free( name       );
    if ( path       )  free( path       );

    return dissenter;
}

static void __DiskArbDiskPeekCallback( DADiskRef disk, void * context )
{
    CFDictionaryRef description = NULL;
    unsigned        flags       = 0;
    CFStringRef     kind        = NULL;
    int             removable   = FALSE;
    UInt64          size        = 0;
    int             type        = 0;
    int             whole       = FALSE;
    int             writable    = FALSE;

    description = DADiskCopyDescription( disk );

    if ( description )
    {
        flags      = __DiskArbCopyDiskDescriptionFlags( disk );
        kind       = CFDictionaryGetValue( description, kDADiskDescriptionMediaKindKey );
        removable  = ( flags & kDiskArbDiskAppearedEjectableMask ) ? TRUE : FALSE;
        size       = ___CFDictionaryGetIntegerValue( description, kDADiskDescriptionMediaSizeKey );
        whole      = ( flags & kDiskArbDiskAppearedWholeDiskMask ) ? TRUE : FALSE;
        writable   = ( flags & kDiskArbDiskAppearedLockedMask ) ? FALSE : TRUE;

        if ( CFEqual( kind, CFSTR( kIOBDMediaClass ) ) )
        {
            type = size ? kDiskArbHandlesUnrecognizedBDMedia : kDiskArbHandlesUninitializedBDMedia;
        }
        else if ( CFEqual( kind, CFSTR( kIOCDMediaClass ) ) )
        {
            type = size ? kDiskArbHandlesUnrecognizedCDMedia : kDiskArbHandlesUninitializedCDMedia;
        }
        else if ( CFEqual( kind, CFSTR( kIODVDMediaClass ) ) )
        {
            type = size ? kDiskArbHandlesUnrecognizedDVDMedia : kDiskArbHandlesUninitializedDVDMedia;
        }
        else if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaEjectableKey ) == kCFBooleanTrue )
        {
            type = size ? kDiskArbHandlesUnrecognizedOtherRemovableMedia : kDiskArbHandlesUninitializedOtherRemovableMedia;
        }
        else
        {
            type = size ? kDiskArbHandlesUnrecognizedFixedMedia : kDiskArbHandlesUninitializedFixedMedia;
        }

        CFRelease( description );
    }

    if ( context )
    {
        if ( __gDiskArbHandlesUnrecognized )
        {
            DADiskClaim( disk, kDADiskClaimOptionDefault, __DiskArbDiskClaimReleaseCallback, ( void * ) 1, __DiskArbDiskClaimCallback, ( void * ) 1 );
        }
    }
    else
    {
        if ( __gDiskArbHandlesUnrecognizedPriority > 0 )
        {
            if ( ( __gDiskArbHandlesUnrecognizedTypes & type ) )
            {
                CFArrayRef callbacks;

                callbacks = __DiskArbGetCallbackHandler( kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK );

                if ( callbacks )
                {
                    CFIndex count;
                    CFIndex index;

                    count = CFArrayGetCount( callbacks );

                    for ( index = 0; index < count; index++ )
                    {
                        DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t callback;

                        callback = CFArrayGetValueAtIndex( callbacks, index );

                        ( callback )( __DiskArbGetDiskID( disk ), type, "", "", writable, removable, whole );
                    }
                }
            }
        }
    }
}

static void __DiskArbDiskRenameCallback( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    if ( dissenter )
    {
        DAReturn status;

        status = DADissenterGetStatus( dissenter );
///w:start
        if ( __gDiskArbStatusLock )
        {
             if ( status )
             {
                 __gDiskArbStatus = status;

                 return;
             }
        }
///w:stop

        __DiskArbCallback_DiskChangedNotification( __DiskArbGetDiskID( disk ), "", "", 0, 0 );

        __DiskArbCallback_CallFailedNotification( __DiskArbGetDiskID( disk ), kDiskArbDiskChangeRequestFailed, status ? EBUSY : 0 );
    }
}

static void __DiskArbDiskUnmountCallback( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    if ( dissenter )
    {
        DAReturn status;

        status = DADissenterGetStatus( dissenter );
///w:start
        if ( __gDiskArbStatusLock )
        {
             if ( status )
             {
                 __gDiskArbStatus = status;

                 return;
             }
        }
///w:stop

        if ( ( ( ( int ) context ) & kDiskArbUnmountOneFlag ) == 0 )
        {
            io_service_t media;

            media = DADiskCopyIOMedia( disk );

            if ( media )
            {
                io_iterator_t services = IO_OBJECT_NULL;

                IORegistryEntryCreateIterator( media, kIOServicePlane, kIORegistryIterateRecursively, &services );

                if ( services )
                {
                    io_service_t service;

                    while ( ( service = IOIteratorNext( services ) ) )
                    {
                        if ( IOObjectConformsTo( service, kIOMediaClass ) )
                        {
                            DADiskRef child;

                            child = DADiskCreateFromIOMedia( kCFAllocatorDefault, __gDiskArbSession, service );

                            if ( child )
                            {
                                if ( __DiskArbGetFileSystemStatus( __DiskArbGetDiskID( child ) ) )
                                {
                                    CFSetRemoveValue( __gDiskArbUnmountList, child );

                                    __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( child ), status ? EBUSY : 0, status ? -1 : 0 );
                                }

                                CFRelease( child );
                            }
                        }

                        IOObjectRelease( service );
                    }

                    IOObjectRelease( services );
                }

                IOObjectRelease( media );
            }
        }

        if ( status == kDAReturnNotMounted )
        {
            CFSetRemoveValue( __gDiskArbUnmountList, disk );

            __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
        }
        else
        {
            CFSetRemoveValue( __gDiskArbUnmountList, disk );

            __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), status ? EBUSY : 0, status ? -1 : 0 );

            if ( ( ( ( int ) context ) & kDiskArbUnmountAndEjectFlag ) )
            {
                __DiskArbCallback_CallFailedNotification( __DiskArbGetDiskID( disk ), kDiskArbUnmountAndEjectRequestFailed, status ? EBUSY : 0 );
            }
            else
            {
                __DiskArbCallback_CallFailedNotification( __DiskArbGetDiskID( disk ), kDiskArbUnmountRequestFailed, status ? EBUSY : 0 );
            }
        }
    }
    else
    {
        if ( ( ( ( int ) context ) & kDiskArbUnmountOneFlag ) == 0 )
        {
            CFSetRemoveValue( __gDiskArbUnmountList, disk );

            __DiskArbCallback_UnmountPostNotification( __DiskArbGetDiskID( disk ), 0, 0 );
        }

        if ( ( ( ( int ) context ) & kDiskArbUnmountAndEjectFlag ) )
        {
            DADiskEject( disk, kDADiskEjectOptionDefault, __DiskArbDiskEjectCallback, ( void * ) kDiskArbUnmountAndEjectFlag );
        }
    }
}

static DADissenterRef __DiskArbDiskUnmountApprovalCallback( DADiskRef disk, void * context )
{
    CFArrayRef     callbacks;
    DADissenterRef dissenter = NULL;

    callbacks = __DiskArbGetCallbackHandler( kDA_DISK_UNMOUNT_PRE_NOTIFY );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_UnmountPreNotification_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                __gDiskArbAck = 0;

                ( callback )( __DiskArbGetDiskID( disk ), 0 );

                if ( dissenter == NULL )
                {
                    switch ( __gDiskArbAck )
                    {
                        case 0:
                        {
                            break;
                        }
                        default:
                        {
                            dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted, NULL );

                            break;
                        }
                    }
                }
            }
        }

        CFSetSetValue( __gDiskArbUnmountList, disk );
    }

    return dissenter;
}

static void __DiskArbIdleCallback( void * context )
{
    CFArrayRef callbacks;

    CFSetApplyFunction( __gDiskArbUnmountList, __DiskArbCallback_UnmountPostNotificationApplier, ( void * ) 1 );

    CFSetRemoveAllValues( __gDiskArbUnmountList );

    CFSetApplyFunction( __gDiskArbEjectList, __DiskArbCallback_EjectPostNotificationApplier, ( void * ) 1 );

    CFSetRemoveAllValues( __gDiskArbEjectList );

    callbacks = __DiskArbGetCallbackHandler( kDA_NOTIFICATIONS_COMPLETE );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DiskArbCallback_NotificationComplete_t callback;

            callback = CFArrayGetValueAtIndex( callbacks, index );

            if ( callback )
            {
                if ( ( __gDiskArbNotificationComplete & kDiskArbCompletedDiskAppeared ) )
                {
                    ( callback )( kDiskArbCompletedDiskAppeared );
                }

                if ( ( __gDiskArbNotificationComplete & kDiskArbCompletedPostUnmount ) )
                {
                    ( callback )( kDiskArbCompletedPostUnmount );
                }

                if ( ( __gDiskArbNotificationComplete & kDiskArbCompletedPostEject ) )
                {
                    ( callback )( kDiskArbCompletedPostEject );
                }
            }
        }
    }

    __gDiskArbNotificationComplete = 0;
}

void DiskArbAddCallbackHandler( int type, void * callback, int overwrite )
{
    if ( __gDiskArbCallbackList == NULL )
    {
        __gDiskArbCallbackList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

        assert( __gDiskArbCallbackList );

        __gDiskArbEjectList = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );

        assert( __gDiskArbEjectList );

        __gDiskArbRegisterList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        assert( __gDiskArbRegisterList );

        __gDiskArbReservationList = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );

        assert( __gDiskArbReservationList );

        __gDiskArbUnmountList = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );

        assert( __gDiskArbUnmountList );
    }

    if ( callback )
    {
        CFNumberRef key;

        key = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &type );

        if ( key )
        {
            CFMutableArrayRef callbacks;

            callbacks = ( void * ) CFDictionaryGetValue( __gDiskArbCallbackList, key );

            if ( callbacks )
            {
                CFRetain( callbacks );
            }
            else
            {
                callbacks = CFArrayCreateMutable( kCFAllocatorDefault, 0, NULL );

                CFArrayAppendValue( __gDiskArbRegisterList, key );
            }

            if ( callbacks )
            {
                if ( overwrite )
                {
                    CFArrayRemoveAllValues( callbacks );
                }            

                CFArrayAppendValue( callbacks, callback );

                CFDictionarySetValue( __gDiskArbCallbackList, key, callbacks );

                CFRelease( callbacks );
            }

            CFRelease( key );
        }
    }
}

kern_return_t DiskArbClientHandlesUninitializedDisks_auto( int yes )
{
    if ( __gDiskArbSession )
    {
        static Boolean registered = FALSE;

        __gDiskArbHandlesUnrecognized = yes;

        if ( yes )
        {
            if ( registered == FALSE )
            {
                DARegisterDiskPeekCallback( __gDiskArbSession, kDADiskDescriptionMatchVolumeUnrecognized, -16384, __DiskArbDiskPeekCallback, ( void * ) 1 );

                registered = TRUE;
            }
        }
    }

    return KERN_SUCCESS;
}

kern_return_t DiskArbClientHandlesUnrecognizedDisks( int types, int priority )
{
    if ( __gDiskArbSession )
    {
        static Boolean registered = FALSE;

        __gDiskArbHandlesUnrecognizedPriority = priority;
        __gDiskArbHandlesUnrecognizedTypes    = types;

        if ( priority > 0 )
        {
            if ( registered == FALSE )
            {
                DARegisterDiskPeekCallback( __gDiskArbSession, kDADiskDescriptionMatchVolumeUnrecognized, 16384 - priority, __DiskArbDiskPeekCallback, NULL );

                registered = TRUE;
            }
        }
    }

    return KERN_SUCCESS;
}

kern_return_t DiskArbClientRelinquishesReservation( char * disk, int pid, int status )
{
    __gDiskArbAck = status;

    return KERN_SUCCESS;
}

kern_return_t DiskArbClientWillHandleUnrecognizedDisk( char * disk, int yes )
{
    if ( yes )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            DADiskClaim( _disk, kDADiskClaimOptionDefault, __DiskArbDiskClaimReleaseCallback, NULL, __DiskArbDiskClaimCallback, ( void * ) 1 );

            CFRelease( _disk );
        }
    }

    return KERN_SUCCESS;
}

kern_return_t DiskArbDiskAppearedWithMountpointPing_auto( char * disk, unsigned reserved, char * mountpoint )
{
    return KERN_SUCCESS;
}

kern_return_t DiskArbDiskApprovedAck_auto( char * disk, int status )
{
    __gDiskArbAck = status;

    return KERN_SUCCESS;
}

kern_return_t DiskArbDiskDisappearedPing_auto( char * disk, unsigned reserved )
{
    return KERN_SUCCESS;
}

kern_return_t DiskArbEjectPreNotifyAck_async_auto( char * disk, int status )
{
    __gDiskArbAck = status;

    return KERN_SUCCESS;
}

kern_return_t DiskArbEjectRequest_async_auto( char * disk, unsigned flags )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            DADiskRef whole;

            whole = DADiskCopyWholeDisk( _disk );

            if ( whole )
            {
                DADiskEject( whole, kDADiskEjectOptionDefault, __DiskArbDiskEjectCallback, NULL );

                status = KERN_SUCCESS;

                CFRelease( whole );
            }

            CFRelease( _disk );
        }
    }

    return status;
}

int DiskArbGetVolumeEncoding_auto( char * disk )
{
    struct attr_text_encoding_t
    {
        size_t          size;
        text_encoding_t text_encoding;
    };

    struct attr_text_encoding_t attr;
    struct attrlist             attrlist;
    struct statfs *             fs;

    bzero( &attrlist, sizeof( attrlist ) );

    attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrlist.commonattr  = ATTR_CMN_SCRIPT;

    fs = __DiskArbGetFileSystemStatus( disk );

    return fs ? ( getattrlist( fs->f_mntonname, &attrlist, &attr, sizeof( attr ), 0 ) ? -1 : ( int ) attr.text_encoding ) : -1;
}

boolean_t DiskArbHandleMsg( mach_msg_header_t * message, mach_msg_header_t * reply )
{
    if ( reply )
    {
        reply->msgh_bits        = MACH_MSGH_BITS_ZERO;
        reply->msgh_id          = 0;
        reply->msgh_local_port  = MACH_PORT_NULL;
        reply->msgh_remote_port = MACH_PORT_NULL;
        reply->msgh_reserved    = 0;
        reply->msgh_size        = sizeof( mach_msg_header_t );
    }

    if ( message->msgh_id == 0 )
    {
        _DASessionCallback( NULL, message, message->msgh_size, __gDiskArbSession );

        return TRUE;
    }

    return FALSE;
}

kern_return_t DiskArbInit( void )
{
    if ( __gDiskArbSession == NULL )
    {
        __gDiskArbSession = DASessionCreate( kCFAllocatorDefault );

        if ( __gDiskArbSession )
        {
            _DASessionScheduleWithRunLoop( __gDiskArbSession );
        }
    }

    return __gDiskArbSession ? BOOTSTRAP_SUCCESS : BOOTSTRAP_UNKNOWN_SERVICE;
}

int DiskArbIsActive( void )
{
    return __gDiskArbSession ? 1 : 0;
}

kern_return_t DiskArbIsDeviceReservedForClient( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            status = DADiskIsClaimed( _disk ) ? kDiskArbDeviceIsReserved : kDiskArbDeviceIsNotReserved;

            if ( CFSetContainsValue( __gDiskArbReservationList, _disk ) )
            {
                __DiskArbCallback_Device_Reservation_Status( disk, status, getpid( ) );
            }
            else
            {
                __DiskArbCallback_Device_Reservation_Status( disk, status, -1 );
            }

            status = KERN_SUCCESS;

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbMsgLoop( void )
{
    return DiskArbMsgLoopWithTimeout( MACH_MSG_TIMEOUT_NONE );
}

kern_return_t DiskArbMsgLoopWithTimeout( mach_msg_timeout_t timeout )
{
    mach_msg_return_t status;

    status = KERN_FAILURE;

    if ( __gDiskArbSession )
    {
        mach_msg_empty_rcv_t message;

        status = mach_msg( ( void * ) &message,
                           MACH_RCV_MSG | ( timeout ? MACH_RCV_TIMEOUT : 0 ),
                           0,
                           sizeof( message ),
                           _DASessionGetClientPort( __gDiskArbSession ),
                           timeout,
                           MACH_PORT_NULL );

        if ( status == MACH_MSG_SUCCESS )
        {
            DiskArbHandleMsg( ( void * ) &message, NULL );
        }
    }

    return status;
}

void DiskArbNoOp( void )
{
    return;
}

kern_return_t DiskArbRefresh_auto( void )
{
    return KERN_SUCCESS;
}

void DiskArbRegisterCallback_CallFailedNotification( DiskArbCallback_CallFailedNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_CALL_FAILED, callback, 0 );
}

void DiskArbRegisterCallback_CallSucceededNotification( DiskArbCallback_CallSucceededNotification_t callback )
{
    return;
}

void DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbCallback_ClientDisconnectedNotification_t callback )
{
    return;
}

void DiskArbRegisterCallback_DiskAppeared( DiskArbCallback_DiskAppeared_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_APPEARED1, callback, 0 );
}

void DiskArbRegisterCallback_DiskAppeared2( DiskArbCallback_DiskAppeared2_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_APPEARED, callback, 0 );
}

void DiskArbRegisterCallback_DiskAppearedWithMountpoint( DiskArbCallback_DiskAppearedWithMountpoint_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_APPEARED_WITH_MT, callback, 0 );
}

void DiskArbRegisterCallback_DiskChangedNotification( DiskArbCallback_DiskChangedNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_CHANGED, callback, 0 );
}

void DiskArbRegisterCallback_DiskWillBeCheckedNotification( DiskArbCallback_DiskWillBeCheckedNotification_t callback )
{
    return;
}

void DiskArbRegisterCallback_EjectPostNotification( DiskArbCallback_EjectPostNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_EJECT_POST_NOTIFY, callback, 0 );
}

void DiskArbRegisterCallback_EjectPreNotification( DiskArbCallback_EjectPreNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_EJECT_PRE_NOTIFY, callback, 1 );
}

void DiskArbRegisterCallback_NotificationComplete( DiskArbCallback_NotificationComplete_t callback )
{
    DiskArbAddCallbackHandler( kDA_NOTIFICATIONS_COMPLETE, callback, 0 );
}

void DiskArbRegisterCallback_UnknownFileSystemNotification( DiskArbCallback_UnknownFileSystemNotification_t callback )
{
    return;
}

void DiskArbRegisterCallback_UnmountPostNotification( DiskArbCallback_UnmountPostNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_UNMOUNT_POST_NOTIFY, callback, 0 );
}

void DiskArbRegisterCallback_UnmountPreNotification( DiskArbCallback_UnmountPreNotification_t callback )
{
    DiskArbAddCallbackHandler( kDA_DISK_UNMOUNT_PRE_NOTIFY, callback, 1 );
}

kern_return_t DiskArbReleaseClientReservationForDevice( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            CFSetRemoveValue( __gDiskArbReservationList, _disk );

            DADiskUnclaim( _disk );

            status = KERN_SUCCESS;

            CFRelease( _disk );
        }
    }

    return status;
}

void DiskArbRemoveCallbackHandler( int type, void * callback )
{
    if ( __gDiskArbCallbackList )
    {
        CFNumberRef key;

        key = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &type );

        if ( key )
        {
            CFMutableArrayRef callbacks;

            callbacks = ( void * ) CFDictionaryGetValue( __gDiskArbCallbackList, key );

            if ( callbacks )
            {
                ___CFArrayRemoveValue( callbacks, callback );
            }

            CFRelease( key );
        }
    }
}

kern_return_t DiskArbRequestDiskChange_auto( char * disk, char * name, int flags )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = NULL;

        if ( strncmp( disk, "disk", strlen( "disk" ) ) )
        {
            struct statfs * fs;

            fs = __DiskArbGetFileSystemStatus( disk );

            if ( fs )
            {
                CFURLRef path;

                path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) fs->f_mntonname, strlen( fs->f_mntonname ), TRUE );

                if ( path )
                {
                    _disk = DADiskCreateFromVolumePath( kCFAllocatorDefault, __gDiskArbSession, path );

                    CFRelease( path );
                }
            }
        }
        else
        {
            _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );
        }
        
        if ( _disk )
        {
            CFStringRef string;

            string = CFStringCreateWithCString( kCFAllocatorDefault, name, kCFStringEncodingUTF8 );

            if ( string )
            {
///w:start
                __gDiskArbStatusLock = TRUE;
                __gDiskArbStatus = KERN_SUCCESS;
///w:stop
                DADiskRename( _disk, string, kDADiskRenameOptionDefault, __DiskArbDiskRenameCallback, ( void * ) flags );

                status = KERN_SUCCESS;
///w:start
                status = __gDiskArbStatus ? KERN_FAILURE : KERN_SUCCESS;
                __gDiskArbStatusLock = FALSE;
///w:stop

                CFRelease( string );
            }

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbRequestMount_auto( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            DADiskRef whole;

            whole = DADiskCopyWholeDisk( _disk );

            if ( whole )
            {
                if ( CFEqual( whole, _disk ) )
                {
///w:start
                    __gDiskArbStatusLock = TRUE;
                    __gDiskArbStatus = KERN_SUCCESS;
///w:stop
                    DADiskMount( _disk, NULL, kDADiskMountOptionWhole, __DiskArbDiskMountCallback, ( void * ) 1 );

                    status = KERN_SUCCESS;
///w:start
                    status = __gDiskArbStatus ? KERN_FAILURE : KERN_SUCCESS;
                    __gDiskArbStatusLock = FALSE;
///w:stop
                }

                CFRelease( whole );
            }

            if ( status )
            {
///w:start
                __gDiskArbStatusLock = TRUE;
                __gDiskArbStatus = KERN_SUCCESS;
///w:stop
                DADiskMount( _disk, NULL, kDADiskMountOptionDefault, __DiskArbDiskMountCallback, NULL );

                status = KERN_SUCCESS;
///w:start
                status = __gDiskArbStatus ? KERN_FAILURE : KERN_SUCCESS;
                __gDiskArbStatusLock = FALSE;
///w:stop
            }

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbRequestMountAndOwn_auto( char * disk )
{
    return DiskArbRequestMount_auto( disk );
}

kern_return_t DiskArbRetainClientReservationForDevice( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            DADiskClaim( _disk, kDADiskClaimOptionDefault, __DiskArbDiskClaimReleaseCallback, NULL, __DiskArbDiskClaimCallback, NULL );

            status = KERN_SUCCESS;

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbSetCurrentUser_auto( int user )
{
    return KERN_FAILURE;
}

kern_return_t DiskArbSetVolumeEncoding_auto( char * disk, int encoding )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            status = _DADiskSetEncoding( _disk, encoding );

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbStart( mach_port_t * port )
{
    kern_return_t status;

    status = DiskArbInit( );

    if ( status == KERN_SUCCESS )
    {
        DiskArbUpdateClientFlags( );

        *port = _DASessionGetClientPort( __gDiskArbSession );
    }

    return status;
}

kern_return_t DiskArbUnmountAndEjectRequest_async_auto( char * disk, unsigned flags )
{
    return DiskArbUnmountRequest_async_auto( disk, flags | kDiskArbUnmountAndEjectFlag );
}

kern_return_t DiskArbUnmountPreNotifyAck_async_auto( char * disk, int status )
{
    __gDiskArbAck = status;

    return KERN_SUCCESS;
}

kern_return_t DiskArbUnmountRequest_async_auto( char * disk, unsigned flags )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = NULL;

        if ( strncmp( disk, "disk", strlen( "disk" ) ) )
        {
            struct statfs * fs;

            fs = __DiskArbGetFileSystemStatus( disk );

            if ( fs )
            {
                CFURLRef path;

                path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) fs->f_mntonname, strlen( fs->f_mntonname ), TRUE );

                if ( path )
                {
                    flags |=  kDiskArbUnmountOneFlag;
                    flags &= ~kDiskArbUnmountAndEjectFlag;

                    if ( ( flags & kDiskArbNetworkUnmountFlag ) )
                    {
                        flags |= kDiskArbForceUnmountFlag;
                    }

                    _disk = DADiskCreateFromVolumePath( kCFAllocatorDefault, __gDiskArbSession, path );

                    CFRelease( path );
                }
            }
        }
        else
        {
            if ( ( flags & kDiskArbUnmountAndEjectFlag ) )
            {
                flags &= ~kDiskArbUnmountOneFlag;
            }

            _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );
        }

        if ( _disk )
        {
            DADiskUnmountOptions options = 0;

            if ( ( flags & kDiskArbForceUnmountFlag ) )
            {
                options |= kDADiskUnmountOptionForce;
            }

            if ( ( flags & kDiskArbUnmountOneFlag ) )
            {
///w:start
                __gDiskArbStatusLock = TRUE;
                __gDiskArbStatus = KERN_SUCCESS;
///w:stop
                DADiskUnmount( _disk, options, __DiskArbDiskUnmountCallback, ( void * ) kDiskArbUnmountOneFlag );

                status = KERN_SUCCESS;
///w:start
                status = __gDiskArbStatus ? KERN_FAILURE : KERN_SUCCESS;
                __gDiskArbStatusLock = FALSE;
///w:stop
            }
            else
            {
                DADiskRef whole;

                whole = DADiskCopyWholeDisk( _disk );

                if ( whole )
                {
                    options |= kDADiskUnmountOptionWhole;

///w:start
                    __gDiskArbStatusLock = TRUE;
                    __gDiskArbStatus = KERN_SUCCESS;
///w:stop
                    DADiskUnmount( whole, options, __DiskArbDiskUnmountCallback, ( void * ) ( flags & kDiskArbUnmountAndEjectFlag ) );

                    status = KERN_SUCCESS;
///w:start
                    status = __gDiskArbStatus ? KERN_FAILURE : KERN_SUCCESS;
                    __gDiskArbStatusLock = FALSE;
///w:stop

                    CFRelease( whole );
                }
            }

            CFRelease( _disk );
        }
    }

    return status;
}

void DiskArbUpdateClientFlags( void )
{
    if ( __gDiskArbSession )
    {
        if ( __gDiskArbRegisterList )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( __gDiskArbRegisterList );

            for ( index = 0; index < count; index++ )
            {
                CFNumberRef key;

                key = CFArrayGetValueAtIndex( __gDiskArbRegisterList, index );

                if ( key )
                {
                    int type;

                    CFNumberGetValue( key, kCFNumberIntType, &type );

                    switch ( type )
                    {
                        case kDA_DISK_APPEARED:
                        case kDA_DISK_APPEARED1:
                        case kDA_DISK_APPEARED_COMPLETE:
                        case kDA_DISK_APPEARED_WITH_MT:
                        case kDA_DISK_CHANGED:
                        case kDA_DISK_EJECT_POST_NOTIFY:
                        case kDA_DISK_UNMOUNT_POST_NOTIFY:
                        {
                            static Boolean registered = FALSE;

                            if ( registered == FALSE )
                            {
                                DARegisterDiskDescriptionChangedCallback( __gDiskArbSession, NULL, NULL, __DiskArbDiskDescriptionChangedCallback, NULL );

                                DARegisterDiskDisappearedCallback( __gDiskArbSession, NULL, __DiskArbDiskDisappearedCallback, NULL );
                                
                                DARegisterDiskAppearedCallback( __gDiskArbSession, NULL, __DiskArbDiskAppearedCallback, NULL );

                                registered = TRUE;
                            }

                            break;
                        }
                        case kDA_DISK_APPROVAL_NOTIFY:
                        {
                            DARegisterDiskMountApprovalCallback( ( void * ) __gDiskArbSession, NULL, __DiskArbDiskMountApprovalCallback, NULL );

                            break;
                        }
                        case kDA_DISK_EJECT_PRE_NOTIFY:
                        {
                            DARegisterDiskEjectApprovalCallback( ( void * ) __gDiskArbSession, NULL, __DiskArbDiskEjectApprovalCallback, NULL );

                            break;
                        }
                        case kDA_DISK_UNMOUNT_PRE_NOTIFY:
                        {
                            DARegisterDiskUnmountApprovalCallback( ( void * ) __gDiskArbSession, NULL, __DiskArbDiskUnmountApprovalCallback, NULL );

                            break;
                        }
                        case kDA_NOTIFICATIONS_COMPLETE:
                        {
                            DARegisterIdleCallback( __gDiskArbSession, __DiskArbIdleCallback, NULL );

                            break;
                        }
                    }
                }
            }

            CFArrayRemoveAllValues( __gDiskArbRegisterList );
        }
    }
}

kern_return_t DiskArbVSDBAdoptVolume_auto( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            status = _DADiskSetAdoption( _disk, TRUE );

            CFRelease( _disk );
        }
    }

    return status;
}

kern_return_t DiskArbVSDBDisownVolume_auto( char * disk )
{
    kern_return_t status;

    status = KERN_FAILURE;

    if ( disk )
    {
        DADiskRef _disk;

        _disk = DADiskCreateFromBSDName( kCFAllocatorDefault, __gDiskArbSession, disk );

        if ( _disk )
        {
            status = _DADiskSetAdoption( _disk, FALSE );

            CFRelease( _disk );
        }
    }

    return status;
}

int DiskArbVSDBGetVolumeStatus_auto( char * disk )
{
    struct statfs * fs;

    fs = __DiskArbGetFileSystemStatus( disk );

    return fs ? ( ( fs->f_flags & MNT_IGNORE_OWNERSHIP ) ? 2 : 1 ) : 0;
}

#endif /* !__LP64__ */

int _DAmkdir( const char * path, mode_t mode )
{
    int status;

    status = mkdir( path, mode );

    if ( status )
    {
        status = errno;

        if ( status == EACCES )
        {
            DASessionRef session;

            session = DASessionCreate( kCFAllocatorDefault );

            if ( session )
            {
                status = _DAServermkdir( _DASessionGetID( session ), path );

                if ( status )
                {
                    if ( unix_err( err_get_code( status ) ) == status )
                    {
                        status = err_get_code( status );
                    }
                    else
                    {
                        status = EACCES;
                    }
                }

                CFRelease( session );
            }
        }

        if ( status )
        {
            errno = status;

            status = -1;
        }
    }

    return status;
}

int _DArmdir( const char * path )
{
    int status;

    status = rmdir( path );

    if ( status )
    {
        status = errno;

        if ( status == EACCES )
        {
            DASessionRef session;

            session = DASessionCreate( kCFAllocatorDefault );

            if ( session )
            {
                status = _DAServerrmdir( _DASessionGetID( session ), path );

                if ( status )
                {
                    if ( unix_err( err_get_code( status ) ) == status )
                    {
                        status = err_get_code( status );
                    }
                    else
                    {
                        status = EACCES;
                    }
                }

                CFRelease( session );
            }
        }

        if ( status )
        {
            errno = status;

            status = -1;
        }
    }

    return status;
}

DAReturn _DADiskSetAdoption( DADiskRef disk, Boolean adoption )
{
    DAReturn status;

    status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionDefault, disk, _kDAAuthorizeRightAdopt );

    if ( status == kDAReturnSuccess )
    {
        status = _DAServerDiskSetAdoption( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), adoption );
    }

    return status;
}

DAReturn _DADiskSetEncoding( DADiskRef disk, UInt32 encoding )
{
    DAReturn status;

    status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionIsOwner, disk, _kDAAuthorizeRightEncode );

    if ( status == kDAReturnSuccess )
    {
        status = _DAServerDiskSetEncoding( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), encoding );
    }

    return status;
}

void DARegisterIdleCallback( DASessionRef session, DAIdleCallback callback, void * context )
{
    _DARegisterCallback( session, callback, context, _kDAIdleCallback, 0, NULL, NULL );
}

void DARegisterDiskListCompleteCallback( DASessionRef session, DADiskListCompleteCallback callback, void * context )
{
    _DARegisterCallback( session, callback, context, _kDADiskListCompleteCallback, 0, NULL, NULL );
}
