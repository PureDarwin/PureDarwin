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

#include "DABase.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAPrivate.h"

#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <CoreFoundation/CFRuntime.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOBDMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

struct __DADisk
{
    CFRuntimeBase          _base;
    CFAbsoluteTime         _busy;
    io_object_t            _busyNotification;
    CFURLRef               _bypath;
    DACallbackRef          _claim;
    CFTypeRef              _context;
    CFTypeRef              _contextRe;
    CFMutableDictionaryRef _description;
    CFURLRef               _device;
    char *                 _deviceLink[2];
    dev_t                  _deviceNode;
    char *                 _devicePath[2];
    SInt32                 _deviceUnit;
    DAFileSystemRef        _filesystem;
    char *                 _id;
    io_service_t           _media;
    mode_t                 _mode;
    DADiskOptions          _options;
    io_object_t            _propertyNotification;
    CFDataRef              _serialization;
    DADiskState            _state;
    gid_t                  _userGID;
    uid_t                  _userUID;
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

static DADiskRef __DADiskCreate( CFAllocatorRef allocator, const char * id )
{
    __DADisk * disk;

    disk = ( void * ) _CFRuntimeCreateInstance( allocator, __kDADiskTypeID, sizeof( __DADisk ) - sizeof( CFRuntimeBase ), NULL );

    if ( disk )
    {
        CFDataRef data;

        disk->_busy                 = 0;
        disk->_busyNotification     = IO_OBJECT_NULL;
        disk->_bypath               = NULL;
        disk->_claim                = NULL;
        disk->_context              = NULL;
        disk->_contextRe            = NULL;
        disk->_description          = CFDictionaryCreateMutable( allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
        disk->_device               = NULL;
        disk->_deviceLink[0]        = NULL;
        disk->_deviceLink[1]        = NULL;
        disk->_deviceNode           = 0;
        disk->_devicePath[0]        = NULL;
        disk->_devicePath[1]        = NULL;
        disk->_deviceUnit           = -1;
        disk->_filesystem           = NULL;
        disk->_id                   = strdup( id );
        disk->_media                = IO_OBJECT_NULL;
        disk->_mode                 = 0750;
        disk->_options              = 0;
        disk->_propertyNotification = IO_OBJECT_NULL;
        disk->_serialization        = NULL;
        disk->_state                = 0;
        disk->_userGID              = ___GID_WHEEL;
        disk->_userUID              = ___UID_ROOT;

        assert( disk->_description );
        assert( disk->_id          );

        data = CFDataCreate( allocator, ( void * ) id, strlen( id ) + 1 );

        if ( data )
        {
            CFDictionarySetValue( disk->_description, _kDADiskIDKey, data );

            CFRelease( data );
        }
    }

    return disk;
}

static void __DADiskDeallocate( CFTypeRef object )
{
    DADiskRef disk = ( DADiskRef ) object;

    if ( disk->_busyNotification     )  IOObjectRelease( disk->_busyNotification );
    if ( disk->_bypath               )  CFRelease( disk->_bypath );
    if ( disk->_claim                )  CFRelease( disk->_claim );
    if ( disk->_context              )  CFRelease( disk->_context );
    if ( disk->_contextRe            )  CFRelease( disk->_contextRe );
    if ( disk->_description          )  CFRelease( disk->_description );
    if ( disk->_device               )  CFRelease( disk->_device );
    if ( disk->_deviceLink[0]        )  free( disk->_deviceLink[0] );
    if ( disk->_deviceLink[1]        )  free( disk->_deviceLink[1] );
    if ( disk->_devicePath[0]        )  free( disk->_devicePath[0] );
    if ( disk->_devicePath[1]        )  free( disk->_devicePath[1] );
    if ( disk->_filesystem           )  CFRelease( disk->_filesystem );
    if ( disk->_id                   )  free( disk->_id );
    if ( disk->_media                )  IOObjectRelease( disk->_media );
    if ( disk->_propertyNotification )  IOObjectRelease( disk->_propertyNotification );
    if ( disk->_serialization        )  CFRelease( disk->_serialization );
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

static void __DADiskMatch( const void * key, const void * value, void * context )
{
    DADiskRef disk = *( ( void * * ) context );

    if ( disk )
    {
        if ( CFEqual( key, kDADiskDescriptionMediaMatchKey ) )
        {
            boolean_t match = FALSE;

            IOServiceMatchPropertyTable( disk->_media, value, &match );

            if ( match == FALSE )
            {
                *( ( void * * ) context ) = NULL;
            }
        }
        else
        {
            CFTypeRef compare;
        
            compare = CFDictionaryGetValue( disk->_description, key );

            if ( compare == NULL || CFEqual( value, compare ) == FALSE )
            {
                *( ( void * * ) context ) = NULL;
            }
        }
    }
}

CFComparisonResult DADiskCompareDescription( DADiskRef disk, CFStringRef description, CFTypeRef value )
{
    CFTypeRef object1 = CFDictionaryGetValue( disk->_description, description );
    CFTypeRef object2 = value;

    if ( object1 == object2 )  return kCFCompareEqualTo;
    if ( object1 == NULL    )  return kCFCompareLessThan;
    if ( object2 == NULL    )  return kCFCompareGreaterThan;

    return CFEqual( object1, object2 ) ? kCFCompareEqualTo : kCFCompareLessThan;
}

DADiskRef DADiskCreateFromIOMedia( CFAllocatorRef allocator, io_service_t media )
{
    io_service_t           bus        = IO_OBJECT_NULL;
    uint32_t               busy;
    io_service_t           device     = IO_OBJECT_NULL;
    DADiskRef              disk       = NULL;
    UInt32                 major;
    UInt32                 minor;
    io_name_t              name;
    CFMutableDictionaryRef properties = NULL;
    CFTypeRef              object;
    ___io_path_t           path;
    io_iterator_t          services;
    kern_return_t          status;
    CFDictionaryRef        sub;
    double                 time;

    /*
     * Obtain the media properties.
     */

    status = IORegistryEntryCreateCFProperties( media, &properties, allocator, 0 );
    if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

    /*
     * Create the disk object.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOBSDNameKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    status = CFStringGetCString( object, name, sizeof( name ), kCFStringEncodingUTF8 );
    if ( status == FALSE )  goto DADiskCreateFromIOMediaErr;

    strlcpy( path, _PATH_DEV, sizeof( path ) );
    strlcat( path, name,      sizeof( path ) );

    disk = __DADiskCreate( allocator, path );
    if ( disk == NULL )  goto DADiskCreateFromIOMediaErr;

    disk->_device = CFURLCreateFromFileSystemRepresentation( allocator, ( void * ) path, strlen( path ), FALSE );
    if ( disk->_device == NULL )  goto DADiskCreateFromIOMediaErr;

    disk->_devicePath[0] = strdup( path );

    strlcpy( path, _PATH_DEV, sizeof( path ) );
    strlcat( path, "r",       sizeof( path ) );
    strlcat( path, name,      sizeof( path ) );

    disk->_devicePath[1] = strdup( path );

    IOObjectRetain( media );

    disk->_media = media;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeNetworkKey, kCFBooleanFalse );

    /*
     * Create the disk description -- media block size.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaPreferredBlockSizeKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBlockSizeKey, object );

    /*
     * Create the disk description -- media BSD name.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOBSDNameKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDNameKey, object );

    /*
     * Create the disk description -- media BSD node.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOBSDMajorKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDMajorKey, object );

    CFNumberGetValue( object, kCFNumberSInt32Type, &major );

    object = CFDictionaryGetValue( properties, CFSTR( kIOBSDMinorKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDMinorKey, object );
    
    CFNumberGetValue( object, kCFNumberSInt32Type, &minor );

    disk->_deviceNode = makedev( major, minor );

    /*
     * Create the disk description -- media BSD unit.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOBSDUnitKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDUnitKey, object );

    CFNumberGetValue( object, kCFNumberSInt32Type, &disk->_deviceUnit );

    /*
     * Create the disk description -- media content.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaContentKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaContentKey, object );

    /*
     * Create the disk description -- media ejectable?
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaEjectableKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaEjectableKey, object );

    /*
     * Create the disk description -- media icon.
     */

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( kIOMediaIconKey ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaIconKey, object );
    CFRelease( object );

    /*
     * Create the disk description -- media kind.
     */

    if ( IOObjectConformsTo( media, kIOBDMediaClass ) )
    {
        object = CFSTR( kIOBDMediaClass );

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaKindKey, object );

        /*
         * Create the disk description -- media type.
         */

        object = CFDictionaryGetValue( properties, CFSTR( kIOBDMediaTypeKey ) );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaTypeKey, object );
    }
    else if ( IOObjectConformsTo( media, kIODVDMediaClass ) )
    {
        object = CFSTR( kIODVDMediaClass );

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaKindKey, object );

        /*
         * Create the disk description -- media type.
         */

        object = CFDictionaryGetValue( properties, CFSTR( kIODVDMediaTypeKey ) );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaTypeKey, object );
    }
    else if ( IOObjectConformsTo( media, kIOCDMediaClass ) )
    {
        object = CFSTR( kIOCDMediaClass );

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaKindKey, object );

        /*
         * Create the disk description -- media type.
         */

        object = CFDictionaryGetValue( properties, CFSTR( kIOCDMediaTypeKey ) );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaTypeKey, object );
    }
    else
    {
        object = CFSTR( kIOMediaClass );

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaKindKey, object );
    }

    /*
     * Create the disk description -- media leaf?
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaLeafKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaLeafKey, object );

    /*
     * Create the disk description -- media name.
     */

    status = IORegistryEntryGetName( media, name );
    if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

    object = CFStringCreateWithCString( allocator, name, kCFStringEncodingUTF8 );
    if ( object == NULL )  object = CFStringCreateWithCString( allocator, name, kCFStringEncodingMacRoman );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaNameKey, object );
    CFRelease( object );

    /*
     * Create the disk description -- media path.
     */

    status = ___IORegistryEntryGetPath( media, kIODeviceTreePlane, path );
    if ( status != KERN_SUCCESS )  status = ___IORegistryEntryGetPath( media, kIOServicePlane, path );
    if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

    object = CFStringCreateWithCString( allocator, path, kCFStringEncodingUTF8 );
    if ( object == NULL )  object = CFStringCreateWithCString( allocator, path, kCFStringEncodingMacRoman );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaPathKey, object );
    CFRelease( object );

    /*
     * Create the disk description -- media removable?
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaRemovableKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaRemovableKey, object );

    /*
     * Create the disk description -- media size.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaSizeKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaSizeKey, object );

    /*
     * Create the disk description -- media UUID.
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaUUIDKey ) );

    if ( object )
    {
        object = ___CFUUIDCreateFromString( allocator, object );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaUUIDKey, object );
        CFRelease( object );
    }

    /*
     * Create the disk description -- media whole?
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaWholeKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaWholeKey, object );

    /*
     * Create the disk description -- media writable?
     */

    object = CFDictionaryGetValue( properties, CFSTR( kIOMediaWritableKey ) );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaWritableKey, object );

    CFRelease( properties );
    properties = NULL;

    /*
     * Obtain the device object.
     */

    status = IORegistryEntryCreateIterator( media,
                                            kIOServicePlane,
                                            kIORegistryIterateParents | kIORegistryIterateRecursively,
                                            &services );

    while ( ( device = IOIteratorNext( services ) ) )
    {
        if ( IOObjectConformsTo( device, kIOBlockStorageDeviceClass ) )  break;

        IOObjectRelease( device );
    }

    IOObjectRelease( services );

    if ( device == IO_OBJECT_NULL )  goto DADiskCreateFromIOMediaErr;

    /*
     * Obtain the device properties.
     */

    status = IORegistryEntryCreateCFProperties( device, &properties, allocator, 0 );
    if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

    /*
     * Obtain the device protocol subproperties.
     */

    sub = CFDictionaryGetValue( properties, CFSTR( kIOPropertyProtocolCharacteristicsKey ) );

    if ( sub )
    {
        /*
         * Create the disk description -- device internal?
         */

        object = CFDictionaryGetValue( sub, CFSTR( kIOPropertyPhysicalInterconnectLocationKey ) );

        if ( object )
        {
            if ( CFStringCompare( object, CFSTR( kIOPropertyInternalKey ), 0 ) == 0 )
            {
                object = kCFBooleanTrue;

                CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceInternalKey, object );
            }
            else if ( CFStringCompare( object, CFSTR( kIOPropertyExternalKey ), 0 ) == 0 )
            {
                object = kCFBooleanFalse;

                CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceInternalKey, object );
            }
        }

        /*
         * Create the disk description -- device protocol.
         */

        object = CFDictionaryGetValue( sub, CFSTR( kIOPropertyPhysicalInterconnectTypeKey ) );

        if ( object )
        {
            CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceProtocolKey, object );
        }
    }

    /*
     * Obtain the device model subproperties.
     */

    sub = CFDictionaryGetValue( properties, CFSTR( kIOPropertyDeviceCharacteristicsKey ) );

    if ( sub )
    {
        /*
         * Create the disk description -- device model.
         */

        object = CFDictionaryGetValue( sub, CFSTR( kIOPropertyProductNameKey ) );

        if ( object )
        {
            CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceModelKey, object );
        }

        /*
         * Create the disk description -- device revision.
         */

        object = CFDictionaryGetValue( sub, CFSTR( kIOPropertyProductRevisionLevelKey ) );

        if ( object )
        {
            CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceRevisionKey, object );
        }

        /*
         * Create the disk description -- device vendor.
         */

        object = CFDictionaryGetValue( sub, CFSTR( kIOPropertyVendorNameKey ) );

        if ( object )
        {
            CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceVendorKey, object );
        }
    }

    /*
     * Create the disk description -- device path.
     */

    status = ___IORegistryEntryGetPath( device, kIOServicePlane, path );
    if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

    object = CFStringCreateWithCString( allocator, path, kCFStringEncodingUTF8 );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionDevicePathKey, object );
    CFRelease( object );

    /*
     * Create the disk description -- device unit.
     */

    object = IORegistryEntrySearchCFProperty( device,
                                              kIOServicePlane,
                                              CFSTR( "IOUnit" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceUnitKey, object );
        CFRelease( object );
    }

    /*
     * Create the disk description -- device GUID (IEEE EUI-64).
     */

    object = IORegistryEntrySearchCFProperty( device,
                                              kIOServicePlane,
                                              CFSTR( "GUID" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        UInt64 value;

        CFNumberGetValue( object, kCFNumberSInt64Type, &value );
        CFRelease( object );

        value = OSSwapHostToBigInt64( value );

        object = CFDataCreate( allocator, ( void * ) &value, sizeof( value ) );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceGUIDKey, object );
        CFRelease( object );
    }

    /*
     * Create the disk description -- device is TDM locked
     */

    object = IORegistryEntrySearchCFProperty( device,
                                              kIOServicePlane,
                                              CFSTR( "AppleTDMLocked" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        CFDictionarySetValue( disk->_description, kDADiskDescriptionDeviceTDMLockedKey, object );
        CFRelease( object );
    }

    CFRelease( properties );
    properties = NULL;

    /*
     * Obtain the bus object.
     */

    status = IORegistryEntryCreateIterator( device,
                                            kIOServicePlane,
                                            kIORegistryIterateParents | kIORegistryIterateRecursively,
                                            &services );

    while ( ( bus = IOIteratorNext( services ) ) )
    {
        if ( IORegistryEntryInPlane( bus, kIODeviceTreePlane ) )  break;

        IOObjectRelease( bus );
    }

    IOObjectRelease( services );

    if ( bus )
    {
        /*
         * Create the disk description -- bus name.
         */

        status = IORegistryEntryGetNameInPlane( bus, kIODeviceTreePlane, name );
        if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

        object = CFStringCreateWithCString( allocator, name, kCFStringEncodingUTF8 );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionBusNameKey, object );
        CFRelease( object );

        /*
         * Create the disk description -- bus path.
         */

        status = ___IORegistryEntryGetPath( bus, kIODeviceTreePlane, path );
        if ( status != KERN_SUCCESS )  goto DADiskCreateFromIOMediaErr;

        object = CFStringCreateWithCString( allocator, path, kCFStringEncodingUTF8 );
        if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

        CFDictionarySetValue( disk->_description, kDADiskDescriptionBusPathKey, object );
        CFRelease( object );

        IOObjectRelease( bus );
        bus = IO_OBJECT_NULL;
    }

    /*
     * Create the disk description -- appearance time.
     */

    time = CFAbsoluteTimeGetCurrent( );

    object = CFNumberCreate( allocator, kCFNumberDoubleType, &time );
    if ( object == NULL )  goto DADiskCreateFromIOMediaErr;

    CFDictionarySetValue( disk->_description, kDADiskDescriptionAppearanceTimeKey, object );
    CFRelease( object );

    /*
     * Create the disk state -- busy?
     */

    busy = 0;

    IOServiceGetBusyState( media, &busy );

    if ( busy )
    {
        disk->_busy = CFAbsoluteTimeGetCurrent( );
    }

    /*
     * Create the disk state -- mount automatic?
     */

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( "autodiskmount" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object == NULL )
    {
        disk->_state |= _kDADiskStateMountAutomatic;
    }
    else if ( object == kCFBooleanTrue )
    {
        disk->_state |= _kDADiskStateMountAutomatic | _kDADiskStateMountAutomaticNoDefer;
    }

    if ( object )  CFRelease( object );

    /*
     * Create the disk state -- owner.
     */

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( "owner-uid" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        if ( CFGetTypeID( object ) == CFNumberGetTypeID( ) )
        {
            struct passwd * user;
            int             value;

            CFNumberGetValue( object, kCFNumberIntType, &value );

            disk->_userUID = value;

            user = getpwuid( value );

            if ( user )
            {
                disk->_userGID = user->pw_gid;
            }
        }

        CFRelease( object );
    }

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( "owner-gid" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        if ( CFGetTypeID( object ) == CFNumberGetTypeID( ) )
        {
            int value;

            CFNumberGetValue( object, kCFNumberIntType, &value );

            disk->_userGID = value;
        }

        CFRelease( object );
    }

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( "owner-mode" ),
                                              allocator,
                                              kIORegistryIterateParents | kIORegistryIterateRecursively );

    if ( object )
    {
        if ( CFGetTypeID( object ) == CFNumberGetTypeID( ) )
        {
            int value;

            CFNumberGetValue( object, kCFNumberIntType, &value );

            disk->_mode = value;
        }

        CFRelease( object );
    }

    /*
     * Create the disk state -- media BSD link.
     */

    object = IORegistryEntrySearchCFProperty( media,
                                              kIOServicePlane,
                                              CFSTR( "dev-name" ),
                                              allocator,
                                              0 );

    if ( object )
    {
        if ( CFGetTypeID( object ) == CFStringGetTypeID( ) )
        {
            if ( CFStringGetCString( object, name, sizeof( name ), kCFStringEncodingUTF8 ) )
            {
                strlcpy( path, _PATH_DEV, sizeof( path ) );
                strlcat( path, name,      sizeof( path ) );

                disk->_deviceLink[0] = strdup( path );

                strlcpy( path, _PATH_DEV, sizeof( path ) );
                strlcat( path, "r",       sizeof( path ) );
                strlcat( path, name,      sizeof( path ) );

                disk->_deviceLink[1] = strdup( path );
            }
        }

        CFRelease( object );
    }

    IOObjectRelease( device );

    {
        CFBooleanRef encrypted = NULL;
        CFNumberRef  encryptionDetail = NULL;

        if ( 0 == _DADiskGetEncryptionStatus( allocator, disk, &encrypted, &encryptionDetail) )
        {
            CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaEncryptedKey, encrypted );
            CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaEncryptionDetailKey, encryptionDetail );
            CFRelease( encryptionDetail );
        }
    }

    return disk;

DADiskCreateFromIOMediaErr:

    if ( ___IORegistryEntryGetPath( media, kIOServicePlane, path ) == KERN_SUCCESS )
    {
        DALogError( "unable to create disk, id = %s.", disk ? DADiskGetID( disk ) : NULL );
    }

    if ( bus        )  IOObjectRelease( bus );
    if ( device     )  IOObjectRelease( device );
    if ( disk       )  CFRelease( disk );
    if ( properties )  CFRelease( properties );

    return NULL;
}

DADiskRef DADiskCreateFromVolumePath( CFAllocatorRef allocator, const struct statfs * fs )
{
    DADiskRef disk;

    disk = NULL;

    if ( fs )
    {
        CFURLRef path;

        path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) fs->f_mntonname, strlen( fs->f_mntonname ), TRUE );

        if ( path )
        {
            CFStringRef kind;

            kind = CFStringCreateWithCString( kCFAllocatorDefault, fs->f_fstypename, kCFStringEncodingUTF8 );

            if ( kind )
            {
                char * id;

                id = _DAVolumeCopyID( fs );

                if ( id )
                {
                    CFTypeRef object;

                    disk = __DADiskCreate( allocator, id );

                    if ( disk )
                    {
                        struct passwd * user;
                        const int32_t bsd_major = major( fs->f_fsid.val[0] );
                        const int32_t bsd_minor = minor( fs->f_fsid.val[0] );

                        disk->_bypath = CFRetain( path );

                        CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumePathKey, path );

                        CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeMountableKey, kCFBooleanTrue );

                        CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeKindKey, kind );

                        object = _DAFileSystemCopyName( NULL, path );

                        if ( object )
                        {
                            CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeNameKey, object );

                            CFRelease( object );
                        }

                        object = CFNumberCreate( NULL, kCFNumberSInt32Type, &bsd_major );
                        if ( object )
                        {
                            CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDMajorKey, object );
                            CFRelease( object );
                        }

                        object = CFNumberCreate( NULL, kCFNumberSInt32Type, &bsd_minor );
                        if ( object )
                        {
                            CFDictionarySetValue( disk->_description, kDADiskDescriptionMediaBSDMinorKey, object );
                            CFRelease( object );
                        }

                        if ( ( fs->f_flags & MNT_LOCAL ) )
                        {
                            CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeNetworkKey, kCFBooleanFalse );
                        }
                        else
                        {
                            CFDictionarySetValue( disk->_description, kDADiskDescriptionVolumeNetworkKey, kCFBooleanTrue );
                        }

                        disk->_state |= _kDADiskStateMountAutomatic;
                        disk->_state |= _kDADiskStateMountAutomaticNoDefer;

                        disk->_state |= kDADiskStateStagedProbe;
                        disk->_state |= kDADiskStateStagedPeek;
                        disk->_state |= kDADiskStateStagedMount;

                        disk->_userUID = fs->f_owner;

                        user = getpwuid( fs->f_owner );

                        if ( user )
                        {
                            disk->_userGID = user->pw_gid;
                        }
                    }

                    free( id );
                }

                CFRelease( kind );
            }

            CFRelease( path );
        }
    }

    return disk;
}

CFAbsoluteTime DADiskGetBusy( DADiskRef disk )
{
    return disk->_busy;
}

io_object_t DADiskGetBusyNotification( DADiskRef disk )
{
    return disk->_busyNotification;
}

CFURLRef DADiskGetBypath( DADiskRef disk )
{
    return disk->_bypath;
}

const char * DADiskGetBSDLink( DADiskRef disk, Boolean raw )
{
    return disk->_deviceLink[ raw ? 1 : 0 ];
}

dev_t DADiskGetBSDNode( DADiskRef disk )
{
    return disk->_deviceNode;
}

const char * DADiskGetBSDPath( DADiskRef disk, Boolean raw )
{
    return disk->_devicePath[ raw ? 1 : 0 ];
}

UInt32 DADiskGetBSDUnit( DADiskRef disk )
{
    return disk->_deviceUnit;
}

DACallbackRef DADiskGetClaim( DADiskRef disk )
{
    return disk->_claim;
}

CFTypeRef DADiskGetContext( DADiskRef disk )
{
    return disk->_context;
}

CFTypeRef DADiskGetContextRe( DADiskRef disk )
{
    return disk->_contextRe;
}

CFTypeRef DADiskGetDescription( DADiskRef disk, CFStringRef description )
{
    return CFDictionaryGetValue( disk->_description, description );
}

CFURLRef DADiskGetDevice( DADiskRef disk )
{
    return disk->_device;
}

DAFileSystemRef DADiskGetFileSystem( DADiskRef disk )
{
    return disk->_filesystem;
}

const char * DADiskGetID( DADiskRef disk )
{
    return disk->_id;
}

io_service_t DADiskGetIOMedia( DADiskRef disk )
{
    return disk->_media;
}

mode_t DADiskGetMode( DADiskRef disk )
{
    mode_t mode;

    mode = disk->_mode;

    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
    {
        mode &= 0555;
    }

    return mode;
}

Boolean DADiskGetOption( DADiskRef disk, DADiskOption option )
{
    return ( disk->_options & option ) ? TRUE : FALSE;
}

DADiskOptions DADiskGetOptions( DADiskRef disk )
{
    return disk->_options;
}

io_object_t DADiskGetPropertyNotification( DADiskRef disk )
{
    return disk->_propertyNotification;
}

CFDataRef DADiskGetSerialization( DADiskRef disk )
{
    if ( disk->_serialization == NULL )
    {
        disk->_serialization = _DASerializeDiskDescription( CFGetAllocator( disk ), disk->_description );
    }

    return disk->_serialization;
}

Boolean DADiskGetState( DADiskRef disk, DADiskState state )
{
    return ( disk->_state & state ) ? TRUE : FALSE;
}

CFTypeID DADiskGetTypeID( void )
{
    return __kDADiskTypeID;
}

gid_t DADiskGetUserGID( DADiskRef disk )
{
    return disk->_userGID;
}

uid_t DADiskGetUserUID( DADiskRef disk )
{
    return disk->_userUID;
}

void DADiskInitialize( void )
{
    __kDADiskTypeID = _CFRuntimeRegisterClass( &__DADiskClass );
}

Boolean DADiskMatch( DADiskRef disk, CFDictionaryRef match )
{
    CFDictionaryApplyFunction( match, __DADiskMatch, &disk );

    return disk ? TRUE : FALSE;
}

void DADiskSetBusy( DADiskRef disk, CFAbsoluteTime busy )
{
    disk->_busy = busy;
}

void DADiskSetBusyNotification( DADiskRef disk, io_object_t notification )
{
    if ( disk->_busyNotification )
    {
        IOObjectRelease( disk->_busyNotification );

        disk->_busyNotification = IO_OBJECT_NULL;
    }

    if ( notification )
    {
        IOObjectRetain( notification );

        disk->_busyNotification = notification;
    }
}

void DADiskSetBypath( DADiskRef disk, CFURLRef bypath )
{
    if ( disk->_bypath )
    {
        CFRelease( disk->_bypath );

        disk->_bypath = NULL;
    }

    if ( bypath )
    {
        CFRetain( bypath );

        disk->_bypath = bypath;
    }
}

void DADiskSetBSDLink( DADiskRef disk, Boolean raw, const char * link )
{
    if ( disk->_deviceLink[ raw ? 1 : 0 ] )
    {
        free( disk->_deviceLink[ raw ? 1 : 0 ] );

        disk->_deviceLink[ raw ? 1 : 0 ] = NULL;
    }

    if ( link )
    {
        disk->_deviceLink[ raw ? 1 : 0 ] = strdup( link );
    }
}

void DADiskSetClaim( DADiskRef disk, DACallbackRef claim )
{
    if ( disk->_claim )
    {
        CFRelease( disk->_claim );

        disk->_claim = NULL;
    }

    if ( claim )
    {
        CFRetain( claim );

        disk->_claim = claim;
    }
}

void DADiskSetContext( DADiskRef disk, CFTypeRef context )
{
    if ( disk->_context )
    {
        CFRelease( disk->_context );

        disk->_context = NULL;
    }

    if ( context )
    {
        CFRetain( context );

        disk->_context = context;
    }
}

void DADiskSetContextRe( DADiskRef disk, CFTypeRef context )
{
    if ( disk->_contextRe )
    {
        CFRelease( disk->_contextRe );

        disk->_contextRe = NULL;
    }

    if ( context )
    {
        CFRetain( context );

        disk->_contextRe = context;
    }
}

void DADiskSetDescription( DADiskRef disk, CFStringRef description, CFTypeRef value )
{
    if ( value )
    {
        CFDictionarySetValue( disk->_description, description, value );
    }
    else
    {
        CFDictionaryRemoveValue( disk->_description, description );
    }

    if ( disk->_serialization )
    {
        CFRelease( disk->_serialization );

        disk->_serialization = NULL;
    }
}

void DADiskSetFileSystem( DADiskRef disk, DAFileSystemRef filesystem )
{
    if ( disk->_filesystem )
    {
        CFRelease( disk->_filesystem );

        disk->_filesystem = NULL;
    }

    if ( filesystem )
    {
        CFRetain( filesystem );

        disk->_filesystem = filesystem;
    }
}

void DADiskSetOption( DADiskRef disk, DADiskOption option, Boolean value )
{
    DADiskSetOptions( disk, option, value );
}

void DADiskSetOptions( DADiskRef disk, DADiskOptions options, Boolean value )
{
    disk->_options &= ~options;
    disk->_options |= value ? options : 0;
}

void DADiskSetPropertyNotification( DADiskRef disk, io_object_t notification )
{
    if ( disk->_propertyNotification )
    {
        IOObjectRelease( disk->_propertyNotification );

        disk->_propertyNotification = IO_OBJECT_NULL;
    }

    if ( notification )
    {
        IOObjectRetain( notification );

        disk->_propertyNotification = notification;
    }
}

void DADiskSetState( DADiskRef disk, DADiskState state, Boolean value )
{
    disk->_state &= ~state;
    disk->_state |= value ? state : 0;
}
